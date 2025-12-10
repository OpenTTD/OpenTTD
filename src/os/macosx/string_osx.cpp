/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file string_osx.cpp Functions related to localized text support on OSX. */

#include "../../stdafx.h"
#include "string_osx.h"
#include "../../gfx_func.h"
#include "../../string_func.h"
#include "../../strings_func.h"
#include "../../core/utf8.hpp"
#include "../../table/control_codes.h"
#include "../../fontcache.h"
#include "../../zoom_func.h"
#include "macos.h"

#include <CoreFoundation/CoreFoundation.h>
#include "../../safeguards.h"


/* CTRunDelegateCreate is supported since MacOS X 10.5, but was only included in the SDKs starting with the 10.9 SDK. */
#ifndef HAVE_OSX_109_SDK
extern "C" {
	typedef const struct __CTRunDelegate * CTRunDelegateRef;

	typedef void (*CTRunDelegateDeallocateCallback) (void *refCon);
	typedef CGFloat (*CTRunDelegateGetAscentCallback) (void *refCon);
	typedef CGFloat (*CTRunDelegateGetDescentCallback) (void *refCon);
	typedef CGFloat (*CTRunDelegateGetWidthCallback) (void *refCon);
	typedef struct {
		CFIndex                         version;
		CTRunDelegateDeallocateCallback dealloc;
		CTRunDelegateGetAscentCallback  getAscent;
		CTRunDelegateGetDescentCallback getDescent;
		CTRunDelegateGetWidthCallback   getWidth;
	} CTRunDelegateCallbacks;

	enum : int32_t {
		kCTRunDelegateVersion1 = 1,
		kCTRunDelegateCurrentVersion = kCTRunDelegateVersion1
	};

	extern const CFStringRef kCTRunDelegateAttributeName AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;

	CTRunDelegateRef CTRunDelegateCreate(const CTRunDelegateCallbacks *callbacks, void *refCon) AVAILABLE_MAC_OS_X_VERSION_10_5_AND_LATER;
}
#endif /* HAVE_OSX_109_SDK */

/** Cached current locale. */
static CFAutoRelease<CFLocaleRef> _osx_locale;
/** CoreText cache for font information, cleared when OTTD changes fonts. */
static std::unordered_map<FontIndex, CFAutoRelease<CTFontRef>> _font_cache;


/**
 * Wrapper for doing layouts with CoreText.
 */
class CoreTextParagraphLayout : public ParagraphLayouter {
private:
	ptrdiff_t length;
	const FontMap &font_map;

	CFAutoRelease<CTTypesetterRef> typesetter;

	CFIndex cur_offset = 0; ///< Offset from the start of the current run from where to output.

public:
	/** Visual run contains data about the bit of text with the same font. */
	class CoreTextVisualRun : public ParagraphLayouter::VisualRun {
	private:
		std::vector<GlyphID> glyphs;
		std::vector<Position> positions;
		std::vector<int> glyph_to_char;

		int total_advance = 0;
		Font font;

	public:
		CoreTextVisualRun(CTRunRef run, const Font &font);
		CoreTextVisualRun(CoreTextVisualRun &&other) = default;

		std::span<const GlyphID> GetGlyphs() const override { return this->glyphs; }
		std::span<const Position> GetPositions() const override { return this->positions; }
		std::span<const int> GetGlyphToCharMap() const override { return this->glyph_to_char; }

		const Font &GetFont() const override { return this->font;  }
		int GetLeading() const override { return GetCharacterHeight(this->font.GetFontCache().GetSize()); }
		int GetGlyphCount() const override { return (int)this->glyphs.size(); }
		int GetAdvance() const { return this->total_advance; }
	};

	/** A single line worth of VisualRuns. */
	class CoreTextLine : public std::vector<CoreTextVisualRun>, public ParagraphLayouter::Line {
	public:
		CoreTextLine(CFAutoRelease<CTLineRef> line, const FontMap &font_mapping)
		{
			CFArrayRef runs = CTLineGetGlyphRuns(line.get());
			for (CFIndex i = 0; i < CFArrayGetCount(runs); i++) {
				CTRunRef run = (CTRunRef)CFArrayGetValueAtIndex(runs, i);

				/* Extract font information for this run. */
				CFRange chars = CTRunGetStringRange(run);
				const auto &map = std::ranges::upper_bound(font_mapping, chars.location, std::less{}, &std::pair<int, Font>::first);

				this->emplace_back(run, map->second);
			}
		}

		int GetLeading() const override;
		int GetWidth() const override;
		int CountRuns() const override { return this->size(); }
		const VisualRun &GetVisualRun(int run) const override { return this->at(run);  }

		int GetInternalCharLength(char32_t c) const override
		{
			/* CoreText uses UTF-16 internally which means we need to account for surrogate pairs. */
			return c >= 0x010000U ? 2 : 1;
		}
	};

	CoreTextParagraphLayout(CFAutoRelease<CTTypesetterRef> typesetter, ptrdiff_t len, const FontMap &font_mapping) : length(len), font_map(font_mapping), typesetter(std::move(typesetter))
	{
		this->Reflow();
	}

	void Reflow() override
	{
		this->cur_offset = 0;
	}

	std::unique_ptr<const Line> NextLine(int max_width) override;
};


/** Get the width of an encoded sprite font character.  */
static CGFloat CustomFontGetWidth(void *ref_con)
{
	FontIndex fi = static_cast<FontIndex>(reinterpret_cast<uintptr_t>(ref_con) >> 24);
	char32_t c = static_cast<char32_t>(reinterpret_cast<uintptr_t>(ref_con) & 0xFFFFFF);

	return FontCache::Get(fi)->GetGlyphWidth(c);
}

static const CTRunDelegateCallbacks _sprite_font_callback = {
	kCTRunDelegateCurrentVersion, nullptr, nullptr, nullptr,
	&CustomFontGetWidth
};

/* static */ std::unique_ptr<ParagraphLayouter> CoreTextParagraphLayoutFactory::GetParagraphLayout(CharType *buff, CharType *buff_end, FontMap &font_mapping)
{
	if (!MacOSVersionIsAtLeast(10, 5, 0)) return nullptr;

	/* Can't layout an empty string. */
	ptrdiff_t length = buff_end - buff;
	if (length == 0) return nullptr;

	/* Make attributed string with embedded font information. */
	CFAutoRelease<CFMutableAttributedStringRef> str(CFAttributedStringCreateMutable(kCFAllocatorDefault, 0));
	CFAttributedStringBeginEditing(str.get());

	CFAutoRelease<CFStringRef> base(CFStringCreateWithCharactersNoCopy(kCFAllocatorDefault, buff, length, kCFAllocatorNull));
	CFAttributedStringReplaceString(str.get(), CFRangeMake(0, 0), base.get());

	const UniChar replacement_char = 0xFFFC;
	CFAutoRelease<CFStringRef> replacement_str(CFStringCreateWithCharacters(kCFAllocatorDefault, &replacement_char, 1));

	/* Apply font and colour ranges to our string. This is important to make sure
	 * that we get proper glyph boundaries on style changes. */
	int last = 0;
	for (const auto &[position, font] : font_mapping) {
		if (position - last == 0) continue;

		FontCache &fc = font.GetFontCache();
		CTFontRef font_handle = static_cast<CTFontRef>(fc.GetOSHandle());
		if (font_handle == nullptr) {
			if (!_font_cache[fc.GetIndex()]) {
				/* Cache font information. */
				CFAutoRelease<CFStringRef> font_name(CFStringCreateWithCString(kCFAllocatorDefault, fc.GetFontName().c_str(), kCFStringEncodingUTF8));
				_font_cache[fc.GetIndex()].reset(CTFontCreateWithName(font_name.get(), fc.GetFontSize(), nullptr));
			}
			font_handle = _font_cache[fc.GetIndex()].get();
		}
		CFAttributedStringSetAttribute(str.get(), CFRangeMake(last, position - last), kCTFontAttributeName, font_handle);

		CGColorRef colour = CGColorCreateGenericGray((uint8_t)font.colour / 255.0f, 1.0f); // We don't care about the real colours, just that they are different.
		CFAttributedStringSetAttribute(str.get(), CFRangeMake(last, position - last), kCTForegroundColorAttributeName, colour);
		CGColorRelease(colour);

		/* Install a size callback for our custom fonts. */
		if (fc.IsBuiltInFont()) {
			for (ssize_t c = last; c < position; c++) {
				CFAutoRelease<CTRunDelegateRef> del(CTRunDelegateCreate(&_sprite_font_callback, reinterpret_cast<void *>(static_cast<size_t>(buff[c] | (fc.GetIndex() << 24)))));
				/* According to the official documentation, if a run delegate is used, the char should always be 0xFFFC. */
				CFAttributedStringReplaceString(str.get(), CFRangeMake(c, 1), replacement_str.get());
				CFAttributedStringSetAttribute(str.get(), CFRangeMake(c, 1), kCTRunDelegateAttributeName, del.get());
			}
		}

		last = position;
	}
	CFAttributedStringEndEditing(str.get());

	/* Create and return typesetter for the string. */
	CFAutoRelease<CTTypesetterRef> typesetter(CTTypesetterCreateWithAttributedString(str.get()));

	return typesetter ? std::make_unique<CoreTextParagraphLayout>(std::move(typesetter), length, font_mapping) : nullptr;
}

/* virtual */ std::unique_ptr<const ParagraphLayouter::Line> CoreTextParagraphLayout::NextLine(int max_width)
{
	if (this->cur_offset >= this->length) return nullptr;

	/* Get line break position, trying word breaking first and breaking somewhere if that doesn't work. */
	CFIndex len = CTTypesetterSuggestLineBreak(this->typesetter.get(), this->cur_offset, max_width);
	if (len <= 0) len = CTTypesetterSuggestClusterBreak(this->typesetter.get(), this->cur_offset, max_width);

	/* Create line. */
	CFAutoRelease<CTLineRef> line(CTTypesetterCreateLine(this->typesetter.get(), CFRangeMake(this->cur_offset, len)));
	this->cur_offset += len;

	if (!line) return nullptr;
	return std::make_unique<CoreTextLine>(std::move(line), this->font_map);
}

CoreTextParagraphLayout::CoreTextVisualRun::CoreTextVisualRun(CTRunRef run, const Font &font) : font(font)
{
	this->glyphs.resize(CTRunGetGlyphCount(run));

	/* Query map of glyphs to source string index. */
	auto map = std::make_unique<CFIndex[]>(this->glyphs.size());
	CTRunGetStringIndices(run, CFRangeMake(0, 0), map.get());

	this->glyph_to_char.resize(this->glyphs.size());
	for (size_t i = 0; i < this->glyph_to_char.size(); i++) this->glyph_to_char[i] = (int)map[i];

	auto pts = std::make_unique<CGPoint[]>(this->glyphs.size());
	CTRunGetPositions(run, CFRangeMake(0, 0), pts.get());
	auto advs = std::make_unique<CGSize[]>(this->glyphs.size());
	CTRunGetAdvances(run, CFRangeMake(0, 0), advs.get());
	this->positions.reserve(this->glyphs.size());

	int y_offset = this->font.GetFontCache().GetGlyphYOffset();

	/* Convert glyph array to our data type. At the same time, substitute
	 * the proper glyphs for our private sprite glyphs. */
	auto gl = std::make_unique<CGGlyph[]>(this->glyphs.size());
	CTRunGetGlyphs(run, CFRangeMake(0, 0), gl.get());
	for (size_t i = 0; i < this->glyphs.size(); i++) {
		this->glyphs[i] = gl[i];
		this->positions.emplace_back(pts[i].x, pts[i].x + advs[i].width - 1, pts[i].y + y_offset);
	}
	this->total_advance = (int)std::ceil(CTRunGetTypographicBounds(run, CFRangeMake(0, 0), nullptr, nullptr, nullptr));
}

/**
 * Get the height of the line.
 * @return The maximum height of the line.
 */
int CoreTextParagraphLayout::CoreTextLine::GetLeading() const
{
	int leading = 0;
	for (const auto &run : *this) {
		leading = std::max(leading, run.GetLeading());
	}

	return leading;
}

/**
 * Get the width of this line.
 * @return The width of the line.
 */
int CoreTextParagraphLayout::CoreTextLine::GetWidth() const
{
	if (this->empty()) return 0;

	int total_width = 0;
	for (const auto &run : *this) {
		total_width += run.GetAdvance();
	}

	return total_width;
}


/** Delete CoreText font reference for a specific font size. */
void MacOSResetScriptCache(FontSize size)
{
	_font_cache[size].reset();
}

/** Register an external font file with the CoreText system. */
void MacOSRegisterExternalFont(std::string_view file_path)
{
	if (!MacOSVersionIsAtLeast(10, 6, 0)) return;

	CFAutoRelease<CFStringRef> path(CFStringCreateWithBytes(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(file_path.data()), file_path.size(), kCFStringEncodingUTF8, false));
	CFAutoRelease<CFURLRef> url(CFURLCreateWithFileSystemPath(kCFAllocatorDefault, path.get(), kCFURLPOSIXPathStyle, false));

	CTFontManagerRegisterFontsForURL(url.get(), kCTFontManagerScopeProcess, nullptr);
}

/** Store current language locale as a CoreFoundation locale. */
void MacOSSetCurrentLocaleName(std::string_view iso_code)
{
	if (!MacOSVersionIsAtLeast(10, 5, 0)) return;

	CFAutoRelease<CFStringRef> iso(CFStringCreateWithBytes(kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(iso_code.data()), iso_code.size(), kCFStringEncodingUTF8, false));
	_osx_locale.reset(CFLocaleCreate(kCFAllocatorDefault, iso.get()));
}

/**
 * Compares two strings using case insensitive natural sort.
 *
 * @param s1 First string to compare.
 * @param s2 Second string to compare.
 * @return 1 if s1 < s2, 2 if s1 == s2, 3 if s1 > s2, or 0 if not supported by the OS.
 */
int MacOSStringCompare(std::string_view s1, std::string_view s2)
{
	static const bool supported = MacOSVersionIsAtLeast(10, 5, 0);
	if (!supported) return 0;

	CFStringCompareFlags flags = kCFCompareCaseInsensitive | kCFCompareNumerically | kCFCompareLocalized | kCFCompareWidthInsensitive | kCFCompareForcedOrdering;

	CFAutoRelease<CFStringRef> cf1(CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8 *)s1.data(), s1.size(), kCFStringEncodingUTF8, false));
	CFAutoRelease<CFStringRef> cf2(CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8 *)s2.data(), s2.size(), kCFStringEncodingUTF8, false));

	/* If any CFString could not be created (e.g., due to UTF8 invalid chars), return OS unsupported functionality */
	if (cf1 == nullptr || cf2 == nullptr) return 0;

	return (int)CFStringCompareWithOptionsAndLocale(cf1.get(), cf2.get(), CFRangeMake(0, CFStringGetLength(cf1.get())), flags, _osx_locale.get()) + 2;
}

/**
 * Search if a string is contained in another string using the current locale.
 *
 * @param str String to search in.
 * @param value String to search for.
 * @param case_insensitive Search case-insensitive.
 * @return 1 if value was found, 0 if it was not found, or -1 if not supported by the OS.
 */
int MacOSStringContains(std::string_view str, std::string_view value, bool case_insensitive)
{
	static const bool supported = MacOSVersionIsAtLeast(10, 5, 0);
	if (!supported) return -1;

	CFStringCompareFlags flags = kCFCompareLocalized | kCFCompareWidthInsensitive;
	if (case_insensitive) flags |= kCFCompareCaseInsensitive;

	CFAutoRelease<CFStringRef> cf_str(CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8 *)str.data(), str.size(), kCFStringEncodingUTF8, false));
	CFAutoRelease<CFStringRef> cf_value(CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8 *)value.data(), value.size(), kCFStringEncodingUTF8, false));

	/* If any CFString could not be created (e.g., due to UTF8 invalid chars), return OS unsupported functionality */
	if (cf_str == nullptr || cf_value == nullptr) return -1;

	return CFStringFindWithOptionsAndLocale(cf_str.get(), cf_value.get(), CFRangeMake(0, CFStringGetLength(cf_str.get())), flags, _osx_locale.get(), nullptr) ? 1 : 0;
}


/* virtual */ void OSXStringIterator::SetString(std::string_view s)
{
	this->utf16_to_utf8.clear();
	this->str_info.clear();
	this->cur_pos = 0;

	/* CoreText operates on UTF-16, thus we have to convert the input string.
	 * To be able to return proper offsets, we have to create a mapping at the same time. */
	std::vector<UniChar> utf16_str;     ///< UTF-16 copy of the string.
	Utf8View view(s);
	for (auto it = view.begin(), end = view.end(); it != end; ++it) {
		size_t idx = it.GetByteOffset();
		char32_t c = *it;
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
	this->utf16_to_utf8.push_back(s.size());

	/* Query CoreText for word and cluster break information. */
	this->str_info.resize(utf16_to_utf8.size());

	if (!utf16_str.empty()) {
		CFAutoRelease<CFStringRef> str(CFStringCreateWithCharactersNoCopy(kCFAllocatorDefault, &utf16_str[0], utf16_str.size(), kCFAllocatorNull));

		/* Get cluster breaks. */
		for (CFIndex i = 0; i < CFStringGetLength(str.get()); ) {
			CFRange r = CFStringGetRangeOfComposedCharactersAtIndex(str.get(), i);
			this->str_info[r.location].char_stop = true;

			i += r.length;
		}

		/* Get word breaks. */
		CFAutoRelease<CFStringTokenizerRef> tokenizer(CFStringTokenizerCreate(kCFAllocatorDefault, str.get(), CFRangeMake(0, CFStringGetLength(str.get())), kCFStringTokenizerUnitWordBoundary, _osx_locale.get()));

		CFStringTokenizerTokenType tokenType = kCFStringTokenizerTokenNone;
		while ((tokenType = CFStringTokenizerAdvanceToNextToken(tokenizer.get())) != kCFStringTokenizerTokenNone) {
			/* Skip tokens that are white-space or punctuation tokens. */
			if ((tokenType & kCFStringTokenizerTokenHasNonLettersMask) != kCFStringTokenizerTokenHasNonLettersMask) {
				CFRange r = CFStringTokenizerGetCurrentTokenRange(tokenizer.get());
				this->str_info[r.location].word_stop = true;
			}
		}
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

/* static */ std::unique_ptr<StringIterator> OSXStringIterator::Create()
{
	if (!MacOSVersionIsAtLeast(10, 5, 0)) return nullptr;

	return std::make_unique<OSXStringIterator>();
}
