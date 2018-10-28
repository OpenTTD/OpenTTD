/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file string_osx.cpp Functions related to localized text support on OSX. */

#include "../../stdafx.h"
#include "string_osx.h"
#include "../../string_func.h"
#include "../../strings_func.h"
#include "../../table/control_codes.h"
#include "../../fontcache.h"
#include "macos.h"

#include <CoreFoundation/CoreFoundation.h>


#if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5)
/** Cached current locale. */
static CFLocaleRef _osx_locale = NULL;
/** CoreText cache for font information, cleared when OTTD changes fonts. */
static CTFontRef _font_cache[FS_END];


/**
 * Wrapper for doing layouts with CoreText.
 */
class CoreTextParagraphLayout : public ParagraphLayouter {
private:
	const CoreTextParagraphLayoutFactory::CharType *text_buffer;
	ptrdiff_t length;
	const FontMap& font_map;

	CTTypesetterRef typesetter;

	CFIndex cur_offset = 0; ///< Offset from the start of the current run from where to output.

public:
	/** Visual run contains data about the bit of text with the same font. */
	class CoreTextVisualRun : public ParagraphLayouter::VisualRun {
	private:
		std::vector<GlyphID> glyphs;
		std::vector<float> positions;
		std::vector<int> glyph_to_char;

		int total_advance = 0;
		Font *font;

	public:
		CoreTextVisualRun(CTRunRef run, Font *font, const CoreTextParagraphLayoutFactory::CharType *buff);

		virtual const GlyphID *GetGlyphs() const { return &this->glyphs[0]; }
		virtual const float *GetPositions() const { return &this->positions[0]; }
		virtual const int *GetGlyphToCharMap() const { return &this->glyph_to_char[0]; }

		virtual const Font *GetFont() const { return this->font;  }
		virtual int GetLeading() const { return this->font->fc->GetHeight(); }
		virtual int GetGlyphCount() const { return (int)this->glyphs.size(); }
		int GetAdvance() const { return this->total_advance; }
	};

	/** A single line worth of VisualRuns. */
	class CoreTextLine : public AutoDeleteSmallVector<CoreTextVisualRun *, 4>, public ParagraphLayouter::Line {
	public:
		CoreTextLine(CTLineRef line, const FontMap &fontMapping, const CoreTextParagraphLayoutFactory::CharType *buff)
		{
			CFArrayRef runs = CTLineGetGlyphRuns(line);
			for (CFIndex i = 0; i < CFArrayGetCount(runs); i++) {
				CTRunRef run = (CTRunRef)CFArrayGetValueAtIndex(runs, i);

				/* Extract font information for this run. */
				CFRange chars = CTRunGetStringRange(run);
				FontMap::const_iterator map = fontMapping.Begin();
				while (map < fontMapping.End() - 1 && map->first <= chars.location) map++;

				*this->Append() = new CoreTextVisualRun(run, map->second, buff);
			}
			CFRelease(line);
		}

		virtual int GetLeading() const;
		virtual int GetWidth() const;
		virtual int CountRuns() const { return this->Length();  }
		virtual const VisualRun *GetVisualRun(int run) const { return *this->Get(run);  }

		int GetInternalCharLength(WChar c) const
		{
			/* CoreText uses UTF-16 internally which means we need to account for surrogate pairs. */
			return c >= 0x010000U ? 2 : 1;
		}
	};

	CoreTextParagraphLayout(CTTypesetterRef typesetter, const CoreTextParagraphLayoutFactory::CharType *buffer, ptrdiff_t len, const FontMap &fontMapping) : text_buffer(buffer), length(len), font_map(fontMapping), typesetter(typesetter)
	{
		this->Reflow();
	}

	virtual ~CoreTextParagraphLayout()
	{
		CFRelease(this->typesetter);
	}

	virtual void Reflow()
	{
		this->cur_offset = 0;
	}

	virtual const Line *NextLine(int max_width);
};


/** Get the width of an encoded sprite font character.  */
static CGFloat SpriteFontGetWidth(void *ref_con)
{
	FontSize fs = (FontSize)((size_t)ref_con >> 24);
	WChar c = (WChar)((size_t)ref_con & 0xFFFFFF);

	return GetGlyphWidth(fs, c);
}

static CTRunDelegateCallbacks _sprite_font_callback = {
	kCTRunDelegateCurrentVersion, NULL, NULL, NULL,
	&SpriteFontGetWidth
};

/* static */ ParagraphLayouter *CoreTextParagraphLayoutFactory::GetParagraphLayout(CharType *buff, CharType *buff_end, FontMap &fontMapping)
{
	if (!MacOSVersionIsAtLeast(10, 5, 0)) return NULL;

	/* Can't layout an empty string. */
	ptrdiff_t length = buff_end - buff;
	if (length == 0) return NULL;

	/* Can't layout our in-built sprite fonts. */
	for (FontMap::const_iterator i = fontMapping.Begin(); i != fontMapping.End(); i++) {
		if (i->second->fc->IsBuiltInFont()) return NULL;
	}

	/* Make attributed string with embedded font information. */
	CFMutableAttributedStringRef str = CFAttributedStringCreateMutable(kCFAllocatorDefault, 0);
	CFAttributedStringBeginEditing(str);

	CFStringRef base = CFStringCreateWithCharactersNoCopy(kCFAllocatorDefault, buff, length, kCFAllocatorNull);
	CFAttributedStringReplaceString(str, CFRangeMake(0, 0), base);
	CFRelease(base);

	/* Apply font and colour ranges to our string. This is important to make sure
	 * that we get proper glyph boundaries on style changes. */
	int last = 0;
	for (FontMap::const_iterator i = fontMapping.Begin(); i != fontMapping.End(); i++) {
		if (i->first - last == 0) continue;

		if (_font_cache[i->second->fc->GetSize()] == NULL) {
			/* Cache font information. */
			CFStringRef font_name = CFStringCreateWithCString(kCFAllocatorDefault, i->second->fc->GetFontName(), kCFStringEncodingUTF8);
			_font_cache[i->second->fc->GetSize()] = CTFontCreateWithName(font_name, i->second->fc->GetFontSize(), NULL);
			CFRelease(font_name);
		}
		CFAttributedStringSetAttribute(str, CFRangeMake(last, i->first - last), kCTFontAttributeName, _font_cache[i->second->fc->GetSize()]);

		CGColorRef color = CGColorCreateGenericGray((uint8)i->second->colour / 255.0f, 1.0f); // We don't care about the real colours, just that they are different.
		CFAttributedStringSetAttribute(str, CFRangeMake(last, i->first - last), kCTForegroundColorAttributeName, color);
		CGColorRelease(color);

		/* Install a size callback for our special sprite glyphs. */
		for (ssize_t c = last; c < i->first; c++) {
			if (buff[c] >= SCC_SPRITE_START && buff[c] <= SCC_SPRITE_END) {
				CTRunDelegateRef del = CTRunDelegateCreate(&_sprite_font_callback, (void *)(size_t)(buff[c] | (i->second->fc->GetSize() << 24)));
				CFAttributedStringSetAttribute(str, CFRangeMake(c, 1), kCTRunDelegateAttributeName, del);
				CFRelease(del);
			}
		}

		last = i->first;
	}
	CFAttributedStringEndEditing(str);

	/* Create and return typesetter for the string. */
	CTTypesetterRef typesetter = CTTypesetterCreateWithAttributedString(str);
	CFRelease(str);

	return typesetter != NULL ? new CoreTextParagraphLayout(typesetter, buff, length, fontMapping) : NULL;
}

/* virtual */ const CoreTextParagraphLayout::Line *CoreTextParagraphLayout::NextLine(int max_width)
{
	if (this->cur_offset >= this->length) return NULL;

	/* Get line break position, trying word breaking first and breaking somewhere if that doesn't work. */
	CFIndex len = CTTypesetterSuggestLineBreak(this->typesetter, this->cur_offset, max_width);
	if (len <= 0) len = CTTypesetterSuggestClusterBreak(this->typesetter, this->cur_offset, max_width);

	/* Create line. */
	CTLineRef line = CTTypesetterCreateLine(this->typesetter, CFRangeMake(this->cur_offset, len));
	this->cur_offset += len;

	return line != NULL ? new CoreTextLine(line, this->font_map, this->text_buffer) : NULL;
}

CoreTextParagraphLayout::CoreTextVisualRun::CoreTextVisualRun(CTRunRef run, Font *font, const CoreTextParagraphLayoutFactory::CharType *buff) : font(font)
{
	this->glyphs.resize(CTRunGetGlyphCount(run));

	/* Query map of glyphs to source string index. */
	CFIndex map[this->glyphs.size()];
	CTRunGetStringIndices(run, CFRangeMake(0, 0), map);

	this->glyph_to_char.resize(this->glyphs.size());
	for (size_t i = 0; i < this->glyph_to_char.size(); i++) this->glyph_to_char[i] = (int)map[i];

	CGPoint pts[this->glyphs.size()];
	CTRunGetPositions(run, CFRangeMake(0, 0), pts);
	this->positions.resize(this->glyphs.size() * 2 + 2);

	/* Convert glyph array to our data type. At the same time, substitute
	 * the proper glyphs for our private sprite glyphs. */
	CGGlyph gl[this->glyphs.size()];
	CTRunGetGlyphs(run, CFRangeMake(0, 0), gl);
	for (size_t i = 0; i < this->glyphs.size(); i++) {
		if (buff[this->glyph_to_char[i]] >= SCC_SPRITE_START && buff[this->glyph_to_char[i]] <= SCC_SPRITE_END) {
			this->glyphs[i] = font->fc->MapCharToGlyph(buff[this->glyph_to_char[i]]);
			this->positions[i * 2 + 0] = pts[i].x;
			this->positions[i * 2 + 1] = font->fc->GetAscender() - font->fc->GetGlyph(this->glyphs[i])->height - 1; // Align sprite glyphs to font baseline.
		} else {
			this->glyphs[i] = gl[i];
			this->positions[i * 2 + 0] = pts[i].x;
			this->positions[i * 2 + 1] = pts[i].y;
		}
	}
	this->total_advance = (int)CTRunGetTypographicBounds(run, CFRangeMake(0, 0), NULL, NULL, NULL);
	this->positions[this->glyphs.size() * 2] = this->positions[0] + this->total_advance;
}

/**
 * Get the height of the line.
 * @return The maximum height of the line.
 */
int CoreTextParagraphLayout::CoreTextLine::GetLeading() const
{
	int leading = 0;
	for (const CoreTextVisualRun * const *run = this->Begin(); run != this->End(); run++) {
		leading = max(leading, (*run)->GetLeading());
	}

	return leading;
}

/**
 * Get the width of this line.
 * @return The width of the line.
 */
int CoreTextParagraphLayout::CoreTextLine::GetWidth() const
{
	if (this->Length() == 0) return 0;

	int total_width = 0;
	for (const CoreTextVisualRun * const *run = this->Begin(); run != this->End(); run++) {
		total_width += (*run)->GetAdvance();
	}

	return total_width;
}


/** Delete CoreText font reference for a specific font size. */
void MacOSResetScriptCache(FontSize size)
{
	if (_font_cache[size] != NULL) {
		CFRelease(_font_cache[size]);
		_font_cache[size] = NULL;
	}
}

/** Store current language locale as a CoreFounation locale. */
void MacOSSetCurrentLocaleName(const char *iso_code)
{
	if (!MacOSVersionIsAtLeast(10, 5, 0)) return;

	if (_osx_locale != NULL) CFRelease(_osx_locale);

	CFStringRef iso = CFStringCreateWithCString(kCFAllocatorNull, iso_code, kCFStringEncodingUTF8);
	_osx_locale = CFLocaleCreate(kCFAllocatorDefault, iso);
	CFRelease(iso);
}

/**
 * Compares two strings using case insensitive natural sort.
 *
 * @param s1 First string to compare.
 * @param s2 Second string to compare.
 * @return 1 if s1 < s2, 2 if s1 == s2, 3 if s1 > s2, or 0 if not supported by the OS.
 */
int MacOSStringCompare(const char *s1, const char *s2)
{
	static bool supported = MacOSVersionIsAtLeast(10, 5, 0);
	if (!supported) return 0;

	CFStringCompareFlags flags = kCFCompareCaseInsensitive | kCFCompareNumerically | kCFCompareLocalized | kCFCompareWidthInsensitive | kCFCompareForcedOrdering;

	CFStringRef cf1 = CFStringCreateWithCString(kCFAllocatorDefault, s1, kCFStringEncodingUTF8);
	CFStringRef cf2 = CFStringCreateWithCString(kCFAllocatorDefault, s2, kCFStringEncodingUTF8);

	CFComparisonResult res = CFStringCompareWithOptionsAndLocale(cf1, cf2, CFRangeMake(0, CFStringGetLength(cf1)), flags, _osx_locale);

	CFRelease(cf1);
	CFRelease(cf2);

	return (int)res + 2;
}


/* virtual */ void OSXStringIterator::SetString(const char *s)
{
	const char *string_base = s;

	this->utf16_to_utf8.clear();
	this->str_info.clear();
	this->cur_pos = 0;

	/* CoreText operates on UTF-16, thus we have to convert the input string.
	 * To be able to return proper offsets, we have to create a mapping at the same time. */
	std::vector<UniChar> utf16_str;     ///< UTF-16 copy of the string.
	while (*s != '\0') {
		size_t idx = s - string_base;

		WChar c = Utf8Consume(&s);
		if (c < 0x10000) {
			utf16_str.push_back((UniChar)c);
		} else {
			/* Make a surrogate pair. */
			utf16_str.push_back((UniChar)(0xD800 + ((c - 0x10000) >> 10)));
			utf16_str.push_back((UniChar)(0xDC00 + ((c - 0x10000) & 0x3FF)));
			this->utf16_to_utf8.push_back(idx);
		}
		this->utf16_to_utf8.push_back(idx);
	}
	this->utf16_to_utf8.push_back(s - string_base);

	/* Query CoreText for word and cluster break information. */
	this->str_info.resize(utf16_to_utf8.size());

	if (utf16_str.size() > 0) {
		CFStringRef str = CFStringCreateWithCharactersNoCopy(kCFAllocatorDefault, &utf16_str[0], utf16_str.size(), kCFAllocatorNull);

		/* Get cluster breaks. */
		for (CFIndex i = 0; i < CFStringGetLength(str); ) {
			CFRange r = CFStringGetRangeOfComposedCharactersAtIndex(str, i);
			this->str_info[r.location].char_stop = true;

			i += r.length;
		}

		/* Get word breaks. */
		CFStringTokenizerRef tokenizer = CFStringTokenizerCreate(kCFAllocatorDefault, str, CFRangeMake(0, CFStringGetLength(str)), kCFStringTokenizerUnitWordBoundary, _osx_locale);

		CFStringTokenizerTokenType tokenType = kCFStringTokenizerTokenNone;
		while ((tokenType = CFStringTokenizerAdvanceToNextToken(tokenizer)) != kCFStringTokenizerTokenNone) {
			/* Skip tokens that are white-space or punctuation tokens. */
			if ((tokenType & kCFStringTokenizerTokenHasNonLettersMask) != kCFStringTokenizerTokenHasNonLettersMask) {
				CFRange r = CFStringTokenizerGetCurrentTokenRange(tokenizer);
				this->str_info[r.location].word_stop = true;
			}
		}

		CFRelease(tokenizer);
		CFRelease(str);
	}

	/* End-of-string is always a valid stopping point. */
	this->str_info.back().char_stop = true;
	this->str_info.back().word_stop = true;
}

/* virtual */ size_t OSXStringIterator::SetCurPosition(size_t pos)
{
	/* Convert incoming position to an UTF-16 string index. */
	size_t utf16_pos = 0;
	for (size_t i = 0; i < this->utf16_to_utf8.size(); i++) {
		if (this->utf16_to_utf8[i] == pos) {
			utf16_pos = i;
			break;
		}
	}

	/* Sanitize in case we get a position inside a grapheme cluster. */
	while (utf16_pos > 0 && !this->str_info[utf16_pos].char_stop) utf16_pos--;
	this->cur_pos = utf16_pos;

	return this->utf16_to_utf8[this->cur_pos];
}

/* virtual */ size_t OSXStringIterator::Next(IterType what)
{
	assert(this->cur_pos <= this->utf16_to_utf8.size());
	assert(what == StringIterator::ITER_CHARACTER || what == StringIterator::ITER_WORD);

	if (this->cur_pos == this->utf16_to_utf8.size()) return END;

	do {
		this->cur_pos++;
	} while (this->cur_pos < this->utf16_to_utf8.size() && (what  == ITER_WORD ? !this->str_info[this->cur_pos].word_stop : !this->str_info[this->cur_pos].char_stop));

	return this->cur_pos == this->utf16_to_utf8.size() ? END : this->utf16_to_utf8[this->cur_pos];
}

/* virtual */ size_t OSXStringIterator::Prev(IterType what)
{
	assert(this->cur_pos <= this->utf16_to_utf8.size());
	assert(what == StringIterator::ITER_CHARACTER || what == StringIterator::ITER_WORD);

	if (this->cur_pos == 0) return END;

	do {
		this->cur_pos--;
	} while (this->cur_pos > 0 && (what == ITER_WORD ? !this->str_info[this->cur_pos].word_stop : !this->str_info[this->cur_pos].char_stop));

	return this->utf16_to_utf8[this->cur_pos];
}

/* static */ StringIterator *OSXStringIterator::Create()
{
	if (!MacOSVersionIsAtLeast(10, 5, 0)) return NULL;

	return new OSXStringIterator();
}

#else
void MacOSResetScriptCache(FontSize size) {}
void MacOSSetCurrentLocaleName(const char *iso_code) {}

int MacOSStringCompare(const char *s1, const char *s2)
{
	return 0;
}

/* static */ StringIterator *OSXStringIterator::Create()
{
	return NULL;
}

/* static */ ParagraphLayouter *CoreTextParagraphLayoutFactory::GetParagraphLayout(CharType *buff, CharType *buff_end, FontMap &fontMapping)
{
	return NULL;
}
#endif /* (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5) */
