/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file gfx_layout.cpp Handling of laying out text. */

#include "stdafx.h"
#include "gfx_layout.h"
#include "string_func.h"
#include "strings_func.h"
#include "debug.h"

#include "table/control_codes.h"

#ifdef WITH_ICU_LAYOUT
#include <unicode/ustring.h>
#endif /* WITH_ICU_LAYOUT */

#ifdef WITH_UNISCRIBE
#include "os/windows/string_uniscribe.h"
#endif /* WITH_UNISCRIBE */

#ifdef WITH_COCOA
#include "os/macosx/string_osx.h"
#endif

#include "safeguards.h"


/** Cache of ParagraphLayout lines. */
Layouter::LineCache *Layouter::linecache;

/** Cache of Font instances. */
Layouter::FontColourMap Layouter::fonts[FS_END];


/**
 * Construct a new font.
 * @param size   The font size to use for this font.
 * @param colour The colour to draw this font in.
 */
Font::Font(FontSize size, TextColour colour) :
		fc(FontCache::Get(size)), colour(colour)
{
	assert(size < FS_END);
}

#ifdef WITH_ICU_LAYOUT
/* Implementation details of LEFontInstance */

le_int32 Font::getUnitsPerEM() const
{
	return this->fc->GetUnitsPerEM();
}

le_int32 Font::getAscent() const
{
	return this->fc->GetAscender();
}

le_int32 Font::getDescent() const
{
	return -this->fc->GetDescender();
}

le_int32 Font::getLeading() const
{
	return this->fc->GetHeight();
}

float Font::getXPixelsPerEm() const
{
	return (float)this->fc->GetHeight();
}

float Font::getYPixelsPerEm() const
{
	return (float)this->fc->GetHeight();
}

float Font::getScaleFactorX() const
{
	return 1.0f;
}

float Font::getScaleFactorY() const
{
	return 1.0f;
}

const void *Font::getFontTable(LETag tableTag) const
{
	size_t length;
	return this->getFontTable(tableTag, length);
}

const void *Font::getFontTable(LETag tableTag, size_t &length) const
{
	return this->fc->GetFontTable(tableTag, length);
}

LEGlyphID Font::mapCharToGlyph(LEUnicode32 ch) const
{
	if (IsTextDirectionChar(ch)) return 0;
	return this->fc->MapCharToGlyph(ch);
}

void Font::getGlyphAdvance(LEGlyphID glyph, LEPoint &advance) const
{
	advance.fX = glyph == 0xFFFF ? 0 : this->fc->GetGlyphWidth(glyph);
	advance.fY = 0;
}

le_bool Font::getGlyphPoint(LEGlyphID glyph, le_int32 pointNumber, LEPoint &point) const
{
	return FALSE;
}

/**
 * Wrapper for doing layouts with ICU.
 */
class ICUParagraphLayout : public AutoDeleteSmallVector<ParagraphLayouter::Line *, 4>, public ParagraphLayouter {
	icu::ParagraphLayout *p; ///< The actual ICU paragraph layout.
public:
	/** Visual run contains data about the bit of text with the same font. */
	class ICUVisualRun : public ParagraphLayouter::VisualRun {
		const icu::ParagraphLayout::VisualRun *vr; ///< The actual ICU vr.

	public:
		ICUVisualRun(const icu::ParagraphLayout::VisualRun *vr) : vr(vr) { }

		const Font *GetFont() const          { return (const Font*)vr->getFont(); }
		int GetGlyphCount() const            { return vr->getGlyphCount(); }
		const GlyphID *GetGlyphs() const     { return vr->getGlyphs(); }
		const float *GetPositions() const    { return vr->getPositions(); }
		int GetLeading() const               { return vr->getLeading(); }
		const int *GetGlyphToCharMap() const { return vr->getGlyphToCharMap(); }
	};

	/** A single line worth of VisualRuns. */
	class ICULine : public AutoDeleteSmallVector<ICUVisualRun *, 4>, public ParagraphLayouter::Line {
		icu::ParagraphLayout::Line *l; ///< The actual ICU line.

	public:
		ICULine(icu::ParagraphLayout::Line *l) : l(l)
		{
			for (int i = 0; i < l->countRuns(); i++) {
				*this->Append() = new ICUVisualRun(l->getVisualRun(i));
			}
		}
		~ICULine() { delete l; }

		int GetLeading() const { return l->getLeading(); }
		int GetWidth() const   { return l->getWidth(); }
		int CountRuns() const  { return l->countRuns(); }
		const ParagraphLayouter::VisualRun *GetVisualRun(int run) const { return *this->Get(run); }

		int GetInternalCharLength(WChar c) const
		{
			/* ICU uses UTF-16 internally which means we need to account for surrogate pairs. */
			return Utf8CharLen(c) < 4 ? 1 : 2;
		}
	};

	ICUParagraphLayout(icu::ParagraphLayout *p) : p(p) { }
	~ICUParagraphLayout() { delete p; }
	void Reflow() { p->reflow(); }

	ParagraphLayouter::Line *NextLine(int max_width)
	{
		icu::ParagraphLayout::Line *l = p->nextLine(max_width);
		return l == NULL ? NULL : new ICULine(l);
	}
};

/**
 * Helper class to construct a new #ICUParagraphLayout.
 */
class ICUParagraphLayoutFactory {
public:
	/** Helper for GetLayouter, to get the right type. */
	typedef UChar CharType;
	/** Helper for GetLayouter, to get whether the layouter supports RTL. */
	static const bool SUPPORTS_RTL = true;

	static ParagraphLayouter *GetParagraphLayout(UChar *buff, UChar *buff_end, FontMap &fontMapping)
	{
		int32 length = buff_end - buff;

		if (length == 0) {
			/* ICU's ParagraphLayout cannot handle empty strings, so fake one. */
			buff[0] = ' ';
			length = 1;
			fontMapping.End()[-1].first++;
		}

		/* Fill ICU's FontRuns with the right data. */
		icu::FontRuns runs(fontMapping.Length());
		for (FontMap::iterator iter = fontMapping.Begin(); iter != fontMapping.End(); iter++) {
			runs.add(iter->second, iter->first);
		}

		LEErrorCode status = LE_NO_ERROR;
		/* ParagraphLayout does not copy "buff", so it must stay valid.
		 * "runs" is copied according to the ICU source, but the documentation does not specify anything, so this might break somewhen. */
		icu::ParagraphLayout *p = new icu::ParagraphLayout(buff, length, &runs, NULL, NULL, NULL, _current_text_dir == TD_RTL ? UBIDI_DEFAULT_RTL : UBIDI_DEFAULT_LTR, false, status);
		if (status != LE_NO_ERROR) {
			delete p;
			return NULL;
		}

		return new ICUParagraphLayout(p);
	}

	static size_t AppendToBuffer(UChar *buff, const UChar *buffer_last, WChar c)
	{
		/* Transform from UTF-32 to internal ICU format of UTF-16. */
		int32 length = 0;
		UErrorCode err = U_ZERO_ERROR;
		u_strFromUTF32(buff, buffer_last - buff, &length, (UChar32*)&c, 1, &err);
		return length;
	}
};
#endif /* WITH_ICU_LAYOUT */

/*** Paragraph layout ***/
/**
 * Class handling the splitting of a paragraph of text into lines and
 * visual runs.
 *
 * One constructs this class with the text that needs to be split into
 * lines. Then nextLine is called with the maximum width until NULL is
 * returned. Each nextLine call creates VisualRuns which contain the
 * length of text that are to be drawn with the same font. In other
 * words, the result of this class is a list of sub strings with their
 * font. The sub strings are then already fully laid out, and only
 * need actual drawing.
 *
 * The positions in a visual run are sequential pairs of X,Y of the
 * begin of each of the glyphs plus an extra pair to mark the end.
 *
 * @note This variant does not handle left-to-right properly. This
 *       is supported in the one ParagraphLayout coming from ICU.
 */
class FallbackParagraphLayout : public ParagraphLayouter {
public:
	/** Visual run contains data about the bit of text with the same font. */
	class FallbackVisualRun : public ParagraphLayouter::VisualRun {
		Font *font;       ///< The font used to layout these.
		GlyphID *glyphs;  ///< The glyphs we're drawing.
		float *positions; ///< The positions of the glyphs.
		int *glyph_to_char; ///< The char index of the glyphs.
		int glyph_count;  ///< The number of glyphs.

	public:
		FallbackVisualRun(Font *font, const WChar *chars, int glyph_count, int x);
		~FallbackVisualRun();
		const Font *GetFont() const;
		int GetGlyphCount() const;
		const GlyphID *GetGlyphs() const;
		const float *GetPositions() const;
		int GetLeading() const;
		const int *GetGlyphToCharMap() const;
	};

	/** A single line worth of VisualRuns. */
	class FallbackLine : public AutoDeleteSmallVector<FallbackVisualRun *, 4>, public ParagraphLayouter::Line {
	public:
		int GetLeading() const;
		int GetWidth() const;
		int CountRuns() const;
		const ParagraphLayouter::VisualRun *GetVisualRun(int run) const;

		int GetInternalCharLength(WChar c) const { return 1; }
	};

	const WChar *buffer_begin; ///< Begin of the buffer.
	const WChar *buffer;       ///< The current location in the buffer.
	FontMap &runs;             ///< The fonts we have to use for this paragraph.

	FallbackParagraphLayout(WChar *buffer, int length, FontMap &runs);
	void Reflow();
	const ParagraphLayouter::Line *NextLine(int max_width);
};

/**
 * Helper class to construct a new #FallbackParagraphLayout.
 */
class FallbackParagraphLayoutFactory {
public:
	/** Helper for GetLayouter, to get the right type. */
	typedef WChar CharType;
	/** Helper for GetLayouter, to get whether the layouter supports RTL. */
	static const bool SUPPORTS_RTL = false;

	/**
	 * Get the actual ParagraphLayout for the given buffer.
	 * @param buff The begin of the buffer.
	 * @param buff_end The location after the last element in the buffer.
	 * @param fontMapping THe mapping of the fonts.
	 * @return The ParagraphLayout instance.
	 */
	static ParagraphLayouter *GetParagraphLayout(WChar *buff, WChar *buff_end, FontMap &fontMapping)
	{
		return new FallbackParagraphLayout(buff, buff_end - buff, fontMapping);
	}

	/**
	 * Append a wide character to the internal buffer.
	 * @param buff        The buffer to append to.
	 * @param buffer_last The end of the buffer.
	 * @param c           The character to add.
	 * @return The number of buffer spaces that were used.
	 */
	static size_t AppendToBuffer(WChar *buff, const WChar *buffer_last, WChar c)
	{
		*buff = c;
		return 1;
	}
};

/**
 * Create the visual run.
 * @param font       The font to use for this run.
 * @param chars      The characters to use for this run.
 * @param char_count The number of characters in this run.
 * @param x          The initial x position for this run.
 */
FallbackParagraphLayout::FallbackVisualRun::FallbackVisualRun(Font *font, const WChar *chars, int char_count, int x) :
		font(font), glyph_count(char_count)
{
	this->glyphs = MallocT<GlyphID>(this->glyph_count);
	this->glyph_to_char = MallocT<int>(this->glyph_count);

	/* Positions contains the location of the begin of each of the glyphs, and the end of the last one. */
	this->positions = MallocT<float>(this->glyph_count * 2 + 2);
	this->positions[0] = x;
	this->positions[1] = 0;

	for (int i = 0; i < this->glyph_count; i++) {
		this->glyphs[i] = font->fc->MapCharToGlyph(chars[i]);
		this->positions[2 * i + 2] = this->positions[2 * i] + font->fc->GetGlyphWidth(this->glyphs[i]);
		this->positions[2 * i + 3] = 0;
		this->glyph_to_char[i] = i;
	}
}

/** Free all data. */
FallbackParagraphLayout::FallbackVisualRun::~FallbackVisualRun()
{
	free(this->positions);
	free(this->glyph_to_char);
	free(this->glyphs);
}

/**
 * Get the font associated with this run.
 * @return The font.
 */
const Font *FallbackParagraphLayout::FallbackVisualRun::GetFont() const
{
	return this->font;
}

/**
 * Get the number of glyphs in this run.
 * @return The number of glyphs.
 */
int FallbackParagraphLayout::FallbackVisualRun::GetGlyphCount() const
{
	return this->glyph_count;
}

/**
 * Get the glyphs of this run.
 * @return The glyphs.
 */
const GlyphID *FallbackParagraphLayout::FallbackVisualRun::GetGlyphs() const
{
	return this->glyphs;
}

/**
 * Get the positions of this run.
 * @return The positions.
 */
const float *FallbackParagraphLayout::FallbackVisualRun::GetPositions() const
{
	return this->positions;
}

/**
 * Get the glyph-to-character map for this visual run.
 * @return The glyph-to-character map.
 */
const int *FallbackParagraphLayout::FallbackVisualRun::GetGlyphToCharMap() const
{
	return this->glyph_to_char;
}

/**
 * Get the height of this font.
 * @return The height of the font.
 */
int FallbackParagraphLayout::FallbackVisualRun::GetLeading() const
{
	return this->GetFont()->fc->GetHeight();
}

/**
 * Get the height of the line.
 * @return The maximum height of the line.
 */
int FallbackParagraphLayout::FallbackLine::GetLeading() const
{
	int leading = 0;
	for (const FallbackVisualRun * const *run = this->Begin(); run != this->End(); run++) {
		leading = max(leading, (*run)->GetLeading());
	}

	return leading;
}

/**
 * Get the width of this line.
 * @return The width of the line.
 */
int FallbackParagraphLayout::FallbackLine::GetWidth() const
{
	if (this->Length() == 0) return 0;

	/*
	 * The last X position of a run contains is the end of that run.
	 * Since there is no left-to-right support, taking this value of
	 * the last run gives us the end of the line and thus the width.
	 */
	const ParagraphLayouter::VisualRun *run = this->GetVisualRun(this->CountRuns() - 1);
	return (int)run->GetPositions()[run->GetGlyphCount() * 2];
}

/**
 * Get the number of runs in this line.
 * @return The number of runs.
 */
int FallbackParagraphLayout::FallbackLine::CountRuns() const
{
	return this->Length();
}

/**
 * Get a specific visual run.
 * @return The visual run.
 */
const ParagraphLayouter::VisualRun *FallbackParagraphLayout::FallbackLine::GetVisualRun(int run) const
{
	return *this->Get(run);
}

/**
 * Create a new paragraph layouter.
 * @param buffer The characters of the paragraph.
 * @param length The length of the paragraph.
 * @param runs   The font mapping of this paragraph.
 */
FallbackParagraphLayout::FallbackParagraphLayout(WChar *buffer, int length, FontMap &runs) : buffer_begin(buffer), buffer(buffer), runs(runs)
{
	assert(runs.End()[-1].first == length);
}

/**
 * Reset the position to the start of the paragraph.
 */
void FallbackParagraphLayout::Reflow()
{
	this->buffer = this->buffer_begin;
}

/**
 * Construct a new line with a maximum width.
 * @param max_width The maximum width of the string.
 * @return A Line, or NULL when at the end of the paragraph.
 */
const ParagraphLayouter::Line *FallbackParagraphLayout::NextLine(int max_width)
{
	/* Simple idea:
	 *  - split a line at a newline character, or at a space where we can break a line.
	 *  - split for a visual run whenever a new line happens, or the font changes.
	 */
	if (this->buffer == NULL) return NULL;

	FallbackLine *l = new FallbackLine();

	if (*this->buffer == '\0') {
		/* Only a newline. */
		this->buffer = NULL;
		*l->Append() = new FallbackVisualRun(this->runs.Begin()->second, this->buffer, 0, 0);
		return l;
	}

	int offset = this->buffer - this->buffer_begin;
	FontMap::iterator iter = this->runs.Begin();
	while (iter->first <= offset) {
		iter++;
		assert(iter != this->runs.End());
	}

	const FontCache *fc = iter->second->fc;
	const WChar *next_run = this->buffer_begin + iter->first;

	const WChar *begin = this->buffer;
	const WChar *last_space = NULL;
	const WChar *last_char;
	int width = 0;
	for (;;) {
		WChar c = *this->buffer;
		last_char = this->buffer;

		if (c == '\0') {
			this->buffer = NULL;
			break;
		}

		if (this->buffer == next_run) {
			int w = l->GetWidth();
			*l->Append() = new FallbackVisualRun(iter->second, begin, this->buffer - begin, w);
			iter++;
			assert(iter != this->runs.End());

			next_run = this->buffer_begin + iter->first;
			begin = this->buffer;

			last_space = NULL;
		}

		if (IsWhitespace(c)) last_space = this->buffer;

		if (IsPrintable(c) && !IsTextDirectionChar(c)) {
			int char_width = GetCharacterWidth(fc->GetSize(), c);
			width += char_width;
			if (width > max_width) {
				/* The string is longer than maximum width so we need to decide
				 * what to do with it. */
				if (width == char_width) {
					/* The character is wider than allowed width; don't know
					 * what to do with this case... bail out! */
					this->buffer = NULL;
					return l;
				}

				if (last_space == NULL) {
					/* No space has been found. Just terminate at our current
					 * location. This usually happens for languages that do not
					 * require spaces in strings, like Chinese, Japanese and
					 * Korean. For other languages terminating mid-word might
					 * not be the best, but terminating the whole string instead
					 * of continuing the word at the next line is worse. */
					last_char = this->buffer;
				} else {
					/* A space is found; perfect place to terminate */
					this->buffer = last_space + 1;
					last_char = last_space;
				}
				break;
			}
		}

		this->buffer++;
	}

	if (l->Length() == 0 || last_char - begin != 0) {
		int w = l->GetWidth();
		*l->Append() = new FallbackVisualRun(iter->second, begin, last_char - begin, w);
	}
	return l;
}

/**
 * Helper for getting a ParagraphLayouter of the given type.
 *
 * @note In case no ParagraphLayouter could be constructed, line.layout will be NULL.
 * @param line The cache item to store our layouter in.
 * @param str The string to create a layouter for.
 * @param state The state of the font and color.
 * @tparam T The type of layouter we want.
 */
template <typename T>
static inline void GetLayouter(Layouter::LineCacheItem &line, const char *&str, FontState &state)
{
	if (line.buffer != NULL) free(line.buffer);

	typename T::CharType *buff_begin = MallocT<typename T::CharType>(DRAW_STRING_BUFFER);
	const typename T::CharType *buffer_last = buff_begin + DRAW_STRING_BUFFER;
	typename T::CharType *buff = buff_begin;
	FontMap &fontMapping = line.runs;
	Font *f = Layouter::GetFont(state.fontsize, state.cur_colour);

	line.buffer = buff_begin;
	fontMapping.Clear();

	/*
	 * Go through the whole string while adding Font instances to the font map
	 * whenever the font changes, and convert the wide characters into a format
	 * usable by ParagraphLayout.
	 */
	for (; buff < buffer_last;) {
		WChar c = Utf8Consume(const_cast<const char **>(&str));
		if (c == '\0' || c == '\n') {
			break;
		} else if (c >= SCC_BLUE && c <= SCC_BLACK) {
			state.SetColour((TextColour)(c - SCC_BLUE));
		} else if (c == SCC_PUSH_COLOUR) {
			state.PushColour();
		} else if (c == SCC_POP_COLOUR) {
			state.PopColour();
		} else if (c >= SCC_FIRST_FONT && c <= SCC_LAST_FONT) {
			state.SetFontSize((FontSize)(c - SCC_FIRST_FONT));
		} else {
			/* Filter out text direction characters that shouldn't be drawn, and
			 * will not be handled in the fallback non ICU case because they are
			 * mostly needed for RTL languages which need more ICU support. */
			if (!T::SUPPORTS_RTL && IsTextDirectionChar(c)) continue;
			buff += T::AppendToBuffer(buff, buffer_last, c);
			continue;
		}

		if (!fontMapping.Contains(buff - buff_begin)) {
			fontMapping.Insert(buff - buff_begin, f);
		}
		f = Layouter::GetFont(state.fontsize, state.cur_colour);
	}

	/* Better safe than sorry. */
	*buff = '\0';

	if (!fontMapping.Contains(buff - buff_begin)) {
		fontMapping.Insert(buff - buff_begin, f);
	}
	line.layout = T::GetParagraphLayout(buff_begin, buff, fontMapping);
	line.state_after = state;
}

/**
 * Create a new layouter.
 * @param str      The string to create the layout for.
 * @param maxw     The maximum width.
 * @param colour   The colour of the font.
 * @param fontsize The size of font to use.
 */
Layouter::Layouter(const char *str, int maxw, TextColour colour, FontSize fontsize) : string(str)
{
	FontState state(colour, fontsize);
	WChar c = 0;

	do {
		/* Scan string for end of line */
		const char *lineend = str;
		for (;;) {
			size_t len = Utf8Decode(&c, lineend);
			if (c == '\0' || c == '\n') break;
			lineend += len;
		}

		LineCacheItem& line = GetCachedParagraphLayout(str, lineend - str, state);
		if (line.layout != NULL) {
			/* Line is in cache */
			str = lineend + 1;
			state = line.state_after;
			line.layout->Reflow();
		} else {
			/* Line is new, layout it */
			FontState old_state = state;
#if defined(WITH_ICU_LAYOUT) || defined(WITH_UNISCRIBE) || defined(WITH_COCOA)
			const char *old_str = str;
#endif

#ifdef WITH_ICU_LAYOUT
			GetLayouter<ICUParagraphLayoutFactory>(line, str, state);
			if (line.layout == NULL) {
				static bool warned = false;
				if (!warned) {
					DEBUG(misc, 0, "ICU layouter bailed on the font. Falling back to the fallback layouter");
					warned = true;
				}

				state = old_state;
				str = old_str;
			}
#endif

#ifdef WITH_UNISCRIBE
			if (line.layout == NULL) {
				GetLayouter<UniscribeParagraphLayoutFactory>(line, str, state);
				if (line.layout == NULL) {
					state = old_state;
					str = old_str;
				}
			}
#endif

#ifdef WITH_COCOA
			if (line.layout == NULL) {
				GetLayouter<CoreTextParagraphLayoutFactory>(line, str, state);
				if (line.layout == NULL) {
					state = old_state;
					str = old_str;
				}
			}
#endif

			if (line.layout == NULL) {
				GetLayouter<FallbackParagraphLayoutFactory>(line, str, state);
			}
		}

		/* Copy all lines into a local cache so we can reuse them later on more easily. */
		const ParagraphLayouter::Line *l;
		while ((l = line.layout->NextLine(maxw)) != NULL) {
			*this->Append() = l;
		}

	} while (c != '\0');
}

/**
 * Get the boundaries of this paragraph.
 * @return The boundaries.
 */
Dimension Layouter::GetBounds()
{
	Dimension d = { 0, 0 };
	for (const ParagraphLayouter::Line **l = this->Begin(); l != this->End(); l++) {
		d.width = max<uint>(d.width, (*l)->GetWidth());
		d.height += (*l)->GetLeading();
	}
	return d;
}

/**
 * Get the position of a character in the layout.
 * @param ch Character to get the position of.
 * @return Upper left corner of the character relative to the start of the string.
 * @note Will only work right for single-line strings.
 */
Point Layouter::GetCharPosition(const char *ch) const
{
	/* Find the code point index which corresponds to the char
	 * pointer into our UTF-8 source string. */
	size_t index = 0;
	const char *str = this->string;
	while (str < ch) {
		WChar c;
		size_t len = Utf8Decode(&c, str);
		if (c == '\0' || c == '\n') break;
		str += len;
		index += (*this->Begin())->GetInternalCharLength(c);
	}

	if (str == ch) {
		/* Valid character. */
		const ParagraphLayouter::Line *line = *this->Begin();

		/* Pointer to the end-of-string/line marker? Return total line width. */
		if (*ch == '\0' || *ch == '\n') {
			Point p = { line->GetWidth(), 0 };
			return p;
		}

		/* Scan all runs until we've found our code point index. */
		for (int run_index = 0; run_index < line->CountRuns(); run_index++) {
			const ParagraphLayouter::VisualRun *run = line->GetVisualRun(run_index);

			for (int i = 0; i < run->GetGlyphCount(); i++) {
				/* Matching glyph? Return position. */
				if ((size_t)run->GetGlyphToCharMap()[i] == index) {
					Point p = { (int)run->GetPositions()[i * 2], (int)run->GetPositions()[i * 2 + 1] };
					return p;
				}
			}
		}
	}

	Point p = { 0, 0 };
	return p;
}

/**
 * Get the character that is at a position.
 * @param x Position in the string.
 * @return Pointer to the character at the position or NULL if no character is at the position.
 */
const char *Layouter::GetCharAtPosition(int x) const
{
	const ParagraphLayouter::Line *line = *this->Begin();

	for (int run_index = 0; run_index < line->CountRuns(); run_index++) {
		const ParagraphLayouter::VisualRun *run = line->GetVisualRun(run_index);

		for (int i = 0; i < run->GetGlyphCount(); i++) {
			/* Not a valid glyph (empty). */
			if (run->GetGlyphs()[i] == 0xFFFF) continue;

			int begin_x = (int)run->GetPositions()[i * 2];
			int end_x   = (int)run->GetPositions()[i * 2 + 2];

			if (IsInsideMM(x, begin_x, end_x)) {
				/* Found our glyph, now convert to UTF-8 string index. */
				size_t index = run->GetGlyphToCharMap()[i];

				size_t cur_idx = 0;
				for (const char *str = this->string; *str != '\0'; ) {
					if (cur_idx == index) return str;

					WChar c = Utf8Consume(&str);
					cur_idx += line->GetInternalCharLength(c);
				}
			}
		}
	}

	return NULL;
}

/**
 * Get a static font instance.
 */
Font *Layouter::GetFont(FontSize size, TextColour colour)
{
	FontColourMap::iterator it = fonts[size].Find(colour);
	if (it != fonts[size].End()) return it->second;

	Font *f = new Font(size, colour);
	*fonts[size].Append() = FontColourMap::Pair(colour, f);
	return f;
}

/**
 * Reset cached font information.
 * @param size Font size to reset.
 */
void Layouter::ResetFontCache(FontSize size)
{
	for (FontColourMap::iterator it = fonts[size].Begin(); it != fonts[size].End(); ++it) {
		delete it->second;
	}
	fonts[size].Clear();

	/* We must reset the linecache since it references the just freed fonts */
	ResetLineCache();

#if defined(WITH_UNISCRIBE)
	UniscribeResetScriptCache(size);
#endif
#if defined(WITH_COCOA)
	MacOSResetScriptCache(size);
#endif
}

/**
 * Get reference to cache item.
 * If the item does not exist yet, it is default constructed.
 * @param str Source string of the line (including colour and font size codes).
 * @param len Length of \a str in bytes (no termination).
 * @param state State of the font at the beginning of the line.
 * @return Reference to cache item.
 */
Layouter::LineCacheItem &Layouter::GetCachedParagraphLayout(const char *str, size_t len, const FontState &state)
{
	if (linecache == NULL) {
		/* Create linecache on first access to avoid trouble with initialisation order of static variables. */
		linecache = new LineCache();
	}

	LineCacheKey key;
	key.state_before = state;
	key.str.assign(str, len);
	return (*linecache)[key];
}

/**
 * Clear line cache.
 */
void Layouter::ResetLineCache()
{
	if (linecache != NULL) linecache->clear();
}

/**
 * Reduce the size of linecache if necessary to prevent infinite growth.
 */
void Layouter::ReduceLineCache()
{
	if (linecache != NULL) {
		/* TODO LRU cache would be fancy, but not exactly necessary */
		if (linecache->size() > 4096) ResetLineCache();
	}
}
