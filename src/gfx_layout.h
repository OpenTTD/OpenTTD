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

/**
 * Container with information about a font.
 */
class Font {
public:
	FontCache *fc;     ///< The font we are using.
	TextColour colour; ///< The colour this font has to be.

	Font(FontSize size, TextColour colour);
};

/** Mapping from index to font. */
typedef SmallMap<int, Font *> FontMap;

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
	WChar *buffer;             ///< The current location in the buffer.
	FontMap &runs;             ///< The fonts we have to use for this paragraph.

	ParagraphLayout(WChar *buffer, int length, FontMap &runs);
	Line *nextLine(int max_width);
};

/**
 * The layouter performs all the layout work.
 *
 * It also accounts for the memory allocations and frees.
 */
class Layouter : public AutoDeleteSmallVector<ParagraphLayout::Line *, 4> {
	typedef WChar CharType; ///< The type of character used within the layouter.

	size_t AppendToBuffer(CharType *buff, const CharType *buffer_last, WChar c);
	ParagraphLayout *GetParagraphLayout(CharType *buff);

	CharType buffer[DRAW_STRING_BUFFER]; ///< Buffer for the text that is going to be drawn.
	FontMap fonts;                       ///< The fonts needed for drawing.

public:
	Layouter(const char *str, int maxw = INT32_MAX, TextColour colour = TC_FROMSTRING, FontSize fontsize = FS_NORMAL);
	~Layouter();
	Dimension GetBounds();
};

#endif /* GFX_LAYOUT_H */
