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

#include "table/control_codes.h"

#ifdef WITH_ICU
#include <unicode/ustring.h>
#endif /* WITH_ICU */

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

#ifdef WITH_ICU
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
	return this->fc->GetFontTable(tableTag);
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

size_t Layouter::AppendToBuffer(UChar *buff, const UChar *buffer_last, WChar c)
{
	/* Transform from UTF-32 to internal ICU format of UTF-16. */
	int32 length = 0;
	UErrorCode err = U_ZERO_ERROR;
	u_strFromUTF32(buff, buffer_last - buff, &length, (UChar32*)&c, 1, &err);
	return length;
}

ParagraphLayout *Layouter::GetParagraphLayout(UChar *buff, UChar *buff_end, FontMap &fontMapping)
{
	int32 length = buff_end - buff;

	if (length == 0) {
		/* ICU's ParagraphLayout cannot handle empty strings, so fake one. */
		buff[0] = ' ';
		length = 1;
		fontMapping.End()[-1].first++;
	}

	/* Fill ICU's FontRuns with the right data. */
	FontRuns runs(fontMapping.Length());
	for (FontMap::iterator iter = fontMapping.Begin(); iter != fontMapping.End(); iter++) {
		runs.add(iter->second, iter->first);
	}

	LEErrorCode status = LE_NO_ERROR;
	return new ParagraphLayout(buff, length, &runs, NULL, NULL, NULL, _current_text_dir == TD_RTL ? UBIDI_DEFAULT_RTL : UBIDI_DEFAULT_LTR, false, status);
}

#else /* WITH_ICU */

/*** Paragraph layout ***/

/**
 * Create the visual run.
 * @param font       The font to use for this run.
 * @param chars      The characters to use for this run.
 * @param char_count The number of characters in this run.
 * @param x          The initial x position for this run.
 */
ParagraphLayout::VisualRun::VisualRun(Font *font, const WChar *chars, int char_count, int x) :
		font(font), glyph_count(char_count)
{
	this->glyphs = MallocT<GlyphID>(this->glyph_count);

	/* Positions contains the location of the begin of each of the glyphs, and the end of the last one. */
	this->positions = MallocT<float>(this->glyph_count * 2 + 2);
	this->positions[0] = x;
	this->positions[1] = 0;

	for (int i = 0; i < this->glyph_count; i++) {
		this->glyphs[i] = font->fc->MapCharToGlyph(chars[i]);
		this->positions[2 * i + 2] = this->positions[2 * i] + font->fc->GetGlyphWidth(this->glyphs[i]);
		this->positions[2 * i + 3] = 0;
	}
}

/** Free all data. */
ParagraphLayout::VisualRun::~VisualRun()
{
	free(this->positions);
	free(this->glyphs);
}

/**
 * Get the font associated with this run.
 * @return The font.
 */
Font *ParagraphLayout::VisualRun::getFont() const
{
	return this->font;
}

/**
 * Get the number of glyhps in this run.
 * @return The number of glyphs.
 */
int ParagraphLayout::VisualRun::getGlyphCount() const
{
	return this->glyph_count;
}

/**
 * Get the glyhps of this run.
 * @return The glyphs.
 */
const GlyphID *ParagraphLayout::VisualRun::getGlyphs() const
{
	return this->glyphs;
}

/**
 * Get the positions of this run.
 * @return The positions.
 */
float *ParagraphLayout::VisualRun::getPositions() const
{
	return this->positions;
}

/**
 * Get the height of this font.
 * @return The height of the font.
 */
int ParagraphLayout::VisualRun::getLeading() const
{
	return this->getFont()->fc->GetHeight();
}

/**
 * Get the height of the line.
 * @return The maximum height of the line.
 */
int ParagraphLayout::Line::getLeading() const
{
	int leading = 0;
	for (const VisualRun * const *run = this->Begin(); run != this->End(); run++) {
		leading = max(leading, (*run)->getLeading());
	}

	return leading;
}

/**
 * Get the width of this line.
 * @return The width of the line.
 */
int ParagraphLayout::Line::getWidth() const
{
	if (this->Length() == 0) return 0;

	/*
	 * The last X position of a run contains is the end of that run.
	 * Since there is no left-to-right support, taking this value of
	 * the last run gives us the end of the line and thus the width.
	 */
	const VisualRun *run = this->getVisualRun(this->countRuns() - 1);
	return run->getPositions()[run->getGlyphCount() * 2];
}

/**
 * Get the number of runs in this line.
 * @return The number of runs.
 */
int ParagraphLayout::Line::countRuns() const
{
	return this->Length();
}

/**
 * Get a specific visual run.
 * @return The visual run.
 */
ParagraphLayout::VisualRun *ParagraphLayout::Line::getVisualRun(int run) const
{
	return *this->Get(run);
}

/**
 * Create a new paragraph layouter.
 * @param buffer The characters of the paragraph.
 * @param length The length of the paragraph.
 * @param runs   The font mapping of this paragraph.
 */
ParagraphLayout::ParagraphLayout(WChar *buffer, int length, FontMap &runs) : buffer_begin(buffer), buffer(buffer), runs(runs)
{
	assert(runs.End()[-1].first == length);
}

/**
 * Construct a new line with a maximum width.
 * @param max_width The maximum width of the string.
 * @return A Line, or NULL when at the end of the paragraph.
 */
ParagraphLayout::Line *ParagraphLayout::nextLine(int max_width)
{
	/* Simple idea:
	 *  - split a line at a newline character, or at a space where we can break a line.
	 *  - split for a visual run whenever a new line happens, or the font changes.
	 */
	if (this->buffer == NULL) return NULL;

	Line *l = new Line();

	if (*this->buffer == '\0') {
		/* Only a newline. */
		this->buffer = NULL;
		*l->Append() = new VisualRun(this->runs.Begin()->second, this->buffer, 0, 0);
		return l;
	}

	const WChar *begin = this->buffer;
	WChar *last_space = NULL;
	const WChar *last_char = begin;
	int width = 0;

	int offset = this->buffer - this->buffer_begin;
	FontMap::iterator iter = this->runs.Begin();
	while (iter->first <= offset) {
		iter++;
		assert(iter != this->runs.End());
	}

	const FontCache *fc = iter->second->fc;
	const WChar *next_run = this->buffer_begin + iter->first + 1;

	for (;;) {
		WChar c = *this->buffer++;

		if (c == '\0') {
			this->buffer = NULL;
			break;
		}

		if (this->buffer == next_run) {
			*l->Append() = new VisualRun(iter->second, begin, this->buffer - begin, l->getWidth());
			iter++;
			assert(iter != this->runs.End());

			next_run = this->buffer_begin + iter->first + 1;
			begin = this->buffer;
		}

		if (IsWhitespace(c)) last_space = this->buffer;

		last_char = this->buffer;

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
					this->buffer--;
					last_char = this->buffer;
				} else {
					/* A space is found; perfect place to terminate */
					this->buffer = last_space;
					last_char = last_space - 1;
				}
				break;
			}
		}
	}

	if (l->Length() == 0 || last_char - begin != 0) {
		*l->Append() = new VisualRun(iter->second, begin, last_char - begin, l->getWidth());
	}
	return l;
}

/**
 * Appand a wide character to the internal buffer.
 * @param buff        The buffer to append to.
 * @param buffer_last The end of the buffer.
 * @param c           The character to add.
 * @return The number of buffer spaces that were used.
 */
size_t Layouter::AppendToBuffer(WChar *buff, const WChar *buffer_last, WChar c)
{
	*buff = c;
	return 1;
}

/**
 * Get the actual ParagraphLayout for the given buffer.
 * @param buff The begin of the buffer.
 * @param buff_end The location after the last element in the buffer.
 * @param fontMapping THe mapping of the fonts.
 * @return The ParagraphLayout instance.
 */
ParagraphLayout *Layouter::GetParagraphLayout(WChar *buff, WChar *buff_end, FontMap &fontMapping)
{
	return new ParagraphLayout(buff, buff_end - buff, fontMapping);
}
#endif /* !WITH_ICU */

/**
 * Create a new layouter.
 * @param str      The string to create the layout for.
 * @param maxw     The maximum width.
 * @param colour   The colour of the font.
 * @param fontsize The size of font to use.
 */
Layouter::Layouter(const char *str, int maxw, TextColour colour, FontSize fontsize)
{
	const CharType *buffer_last = lastof(this->buffer);
	CharType *buff = this->buffer;

	TextColour cur_colour = colour, prev_colour = colour;
	WChar c;

	do {
		Font *f = new Font(fontsize, cur_colour);
		CharType *buff_begin = buff;
		FontMap fontMapping;

		/*
		 * Go through the whole string while adding Font instances to the font map
		 * whenever the font changes, and convert the wide characters into a format
		 * usable by ParagraphLayout.
		 */
		for (; buff < buffer_last;) {
			c = Utf8Consume(const_cast<const char **>(&str));
			if (c == '\0' || c == '\n') {
				break;
			} else if (c >= SCC_BLUE && c <= SCC_BLACK) {
				prev_colour = cur_colour;
				cur_colour = (TextColour)(c - SCC_BLUE);
			} else if (c == SCC_PREVIOUS_COLOUR) { // Revert to the previous colour.
				Swap(prev_colour, cur_colour);
			} else if (c == SCC_TINYFONT) {
				fontsize = FS_SMALL;
			} else if (c == SCC_BIGFONT) {
				fontsize = FS_LARGE;
			} else {
				buff += AppendToBuffer(buff, buffer_last, c);
				continue;
			}

			if (!fontMapping.Contains(buff - buff_begin)) {
				fontMapping.Insert(buff - buff_begin, f);
				*this->fonts.Append() = f;
			} else {
				delete f;
			}
			f = new Font(fontsize, cur_colour);
		}

		/* Better safe than sorry. */
		*buff = '\0';

		if (!fontMapping.Contains(buff - buff_begin)) {
			fontMapping.Insert(buff - buff_begin, f);
			*this->fonts.Append() = f;
		}
		ParagraphLayout *p = GetParagraphLayout(buff_begin, buff, fontMapping);

		/* Copy all lines into a local cache so we can reuse them later on more easily. */
		ParagraphLayout::Line *l;
		while ((l = p->nextLine(maxw)) != NULL) {
			*this->Append() = l;
		}

		delete p;

	} while (c != '\0' && buff < buffer_last);
}

/** Free everything we allocated. */
Layouter::~Layouter()
{
	for (Font **iter = this->fonts.Begin(); iter != this->fonts.End(); iter++) {
		delete *iter;
	}
}

/**
 * Get the boundaries of this paragraph.
 * @return The boundaries.
 */
Dimension Layouter::GetBounds()
{
	Dimension d = { 0, 0 };
	for (ParagraphLayout::Line **l = this->Begin(); l != this->End(); l++) {
		d.width = max<uint>(d.width, (*l)->getWidth());
		d.height += (*l)->getLeading();
	}
	return d;
}
