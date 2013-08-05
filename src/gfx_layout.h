/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file gfx_layout.h Functions related to laying out the texts. */

#ifndef GFX_LAYOUT_H
#define GFX_LAYOUT_H

#include "fontcache.h"
#include "gfx_func.h"
#include "core/smallmap_type.hpp"

#include <map>
#include <string>

#ifdef WITH_ICU
#include "layout/ParagraphLayout.h"
#define ICU_FONTINSTANCE : public LEFontInstance
#else /* WITH_ICU */
#define ICU_FONTINSTANCE
#endif /* WITH_ICU */

/**
 * Text drawing parameters, which can change while drawing a line, but are kept between multiple parts
 * of the same text, e.g. on line breaks.
 */
struct FontState {
	FontSize fontsize;       ///< Current font size.
	TextColour cur_colour;   ///< Current text colour.
	TextColour prev_colour;  ///< Text colour from before the last colour switch.

	FontState() : fontsize(FS_END), cur_colour(TC_INVALID), prev_colour(TC_INVALID) {}
	FontState(TextColour colour, FontSize fontsize) : fontsize(fontsize), cur_colour(colour), prev_colour(colour) {}

	/**
	 * Switch to new colour \a c.
	 * @param c New colour to use.
	 */
	inline void SetColour(TextColour c)
	{
		assert(c >= TC_BLUE && c <= TC_BLACK);
		this->prev_colour = this->cur_colour;
		this->cur_colour = c;
	}

	/** Switch to previous colour. */
	inline void SetPreviousColour()
	{
		Swap(this->cur_colour, this->prev_colour);
	}

	/**
	 * Switch to using a new font \a f.
	 * @param f New font to use.
	 */
	inline void SetFontSize(FontSize f)
	{
		this->fontsize = f;
	}
};

/**
 * Container with information about a font.
 */
class Font ICU_FONTINSTANCE {
public:
	FontCache *fc;     ///< The font we are using.
	TextColour colour; ///< The colour this font has to be.

	Font(FontSize size, TextColour colour);

#ifdef WITH_ICU
	/* Implementation details of LEFontInstance */

	le_int32 getUnitsPerEM() const;
	le_int32 getAscent() const;
	le_int32 getDescent() const;
	le_int32 getLeading() const;
	float getXPixelsPerEm() const;
	float getYPixelsPerEm() const;
	float getScaleFactorX() const;
	float getScaleFactorY() const;
	const void *getFontTable(LETag tableTag) const;
	const void *getFontTable(LETag tableTag, size_t &length) const;
	LEGlyphID mapCharToGlyph(LEUnicode32 ch) const;
	void getGlyphAdvance(LEGlyphID glyph, LEPoint &advance) const;
	le_bool getGlyphPoint(LEGlyphID glyph, le_int32 pointNumber, LEPoint &point) const;
#endif /* WITH_ICU */
};

/** Mapping from index to font. */
typedef SmallMap<int, Font *> FontMap;

#ifndef WITH_ICU
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
 * @note Does not conform to function naming style as it provides a
 *       fallback for the ICU class.
 */
class ParagraphLayout {
public:
	/** Visual run contains data about the bit of text with the same font. */
	class VisualRun {
		Font *font;       ///< The font used to layout these.
		GlyphID *glyphs;  ///< The glyphs we're drawing.
		float *positions; ///< The positions of the glyphs.
		int glyph_count;  ///< The number of glyphs.

	public:
		VisualRun(Font *font, const WChar *chars, int glyph_count, int x);
		~VisualRun();
		Font *getFont() const;
		int getGlyphCount() const;
		const GlyphID *getGlyphs() const;
		float *getPositions() const;
		int getLeading() const;
	};

	/** A single line worth of VisualRuns. */
	class Line : public AutoDeleteSmallVector<VisualRun *, 4> {
	public:
		int getLeading() const;
		int getWidth() const;
		int countRuns() const;
		VisualRun *getVisualRun(int run) const;
	};

	const WChar *buffer_begin; ///< Begin of the buffer.
	const WChar *buffer;       ///< The current location in the buffer.
	FontMap &runs;             ///< The fonts we have to use for this paragraph.

	ParagraphLayout(WChar *buffer, int length, FontMap &runs);
	void reflow();
	Line *nextLine(int max_width);
};
#endif /* !WITH_ICU */

/**
 * The layouter performs all the layout work.
 *
 * It also accounts for the memory allocations and frees.
 */
class Layouter : public AutoDeleteSmallVector<ParagraphLayout::Line *, 4> {
#ifdef WITH_ICU
	typedef UChar CharType; ///< The type of character used within the layouter.
#else /* WITH_ICU */
	typedef WChar CharType; ///< The type of character used within the layouter.
#endif /* WITH_ICU */

	const char *string; ///< Pointer to the original string.

	size_t AppendToBuffer(CharType *buff, const CharType *buffer_last, WChar c);
	ParagraphLayout *GetParagraphLayout(CharType *buff, CharType *buff_end, FontMap &fontMapping);

	/** Key into the linecache */
	struct LineCacheKey {
		FontState state_before;  ///< Font state at the beginning of the line.
		std::string str;         ///< Source string of the line (including colour and font size codes).

		/** Comparison operator for std::map */
		bool operator<(const LineCacheKey &other) const
		{
			if (this->state_before.fontsize != other.state_before.fontsize) return this->state_before.fontsize < other.state_before.fontsize;
			if (this->state_before.cur_colour != other.state_before.cur_colour) return this->state_before.cur_colour < other.state_before.cur_colour;
			if (this->state_before.prev_colour != other.state_before.prev_colour) return this->state_before.prev_colour < other.state_before.prev_colour;
			return this->str < other.str;
		}
	};
	/** Item in the linecache */
	struct LineCacheItem {
		/* Stuff that cannot be freed until the ParagraphLayout is freed */
		CharType buffer[DRAW_STRING_BUFFER]; ///< Accessed by both ICU's and our ParagraphLayout::nextLine.
		FontMap runs;                        ///< Accessed by our ParagraphLayout::nextLine.

		FontState state_after;   ///< Font state after the line.
		ParagraphLayout *layout; ///< Layout of the line.

		LineCacheItem() : layout(NULL) {}
		~LineCacheItem() { delete layout; }
	};
	typedef std::map<LineCacheKey, LineCacheItem> LineCache;
	static LineCache *linecache;

	static LineCacheItem &GetCachedParagraphLayout(const char *str, size_t len, const FontState &state);

	typedef SmallMap<TextColour, Font *> FontColourMap;
	static FontColourMap fonts[FS_END];
	static Font *GetFont(FontSize size, TextColour colour);

public:
	Layouter(const char *str, int maxw = INT32_MAX, TextColour colour = TC_FROMSTRING, FontSize fontsize = FS_NORMAL);
	Dimension GetBounds();
	Point GetCharPosition(const char *ch) const;

	static void ResetFontCache(FontSize size);
	static void ResetLineCache();
	static void ReduceLineCache();
};

#endif /* GFX_LAYOUT_H */
