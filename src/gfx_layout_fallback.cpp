/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file gfx_layout_fallback.cpp Handling of laying out text as fallback. */

#include "stdafx.h"

#include "gfx_layout_fallback.h"
#include "string_func.h"
#include "zoom_func.h"

#include "table/control_codes.h"

#include "safeguards.h"

/*** Paragraph layout ***/
/**
 * Class handling the splitting of a paragraph of text into lines and
 * visual runs.
 *
 * One constructs this class with the text that needs to be split into
 * lines. Then nextLine is called with the maximum width until nullptr is
 * returned. Each nextLine call creates VisualRuns which contain the
 * length of text that are to be drawn with the same font. In other
 * words, the result of this class is a list of sub strings with their
 * font. The sub strings are then already fully laid out, and only
 * need actual drawing.
 *
 * The positions in a visual run are sequential pairs of X,Y of the
 * begin of each of the glyphs plus an extra pair to mark the end.
 *
 * @note This variant does not handle right-to-left properly.
 */
class FallbackParagraphLayout : public ParagraphLayouter {
public:
	/** Visual run contains data about the bit of text with the same font. */
	class FallbackVisualRun : public ParagraphLayouter::VisualRun {
		std::vector<GlyphID> glyphs; ///< The glyphs we're drawing.
		std::vector<float> positions; ///< The positions of the glyphs.
		std::vector<int> glyph_to_char; ///< The char index of the glyphs.

		Font *font;       ///< The font used to layout these.

	public:
		FallbackVisualRun(Font *font, const char32_t *chars, int glyph_count, int char_offset, int x);
		const Font *GetFont() const override { return this->font; }
		int GetGlyphCount() const override { return static_cast<int>(this->glyphs.size()); }
		const GlyphID *GetGlyphs() const override { return this->glyphs.data(); }
		const float *GetPositions() const override { return this->positions.data(); }
		int GetLeading() const override { return this->GetFont()->fc->GetHeight(); }
		const int *GetGlyphToCharMap() const override { return this->glyph_to_char.data(); }
	};

	/** A single line worth of VisualRuns. */
	class FallbackLine : public std::vector<FallbackVisualRun>, public ParagraphLayouter::Line {
	public:
		int GetLeading() const override;
		int GetWidth() const override;
		int CountRuns() const override;
		const ParagraphLayouter::VisualRun &GetVisualRun(int run) const override;

		int GetInternalCharLength(char32_t) const override { return 1; }
	};

	const char32_t *buffer_begin; ///< Begin of the buffer.
	const char32_t *buffer;       ///< The current location in the buffer.
	FontMap &runs;             ///< The fonts we have to use for this paragraph.

	FallbackParagraphLayout(char32_t *buffer, int length, FontMap &runs);
	void Reflow() override;
	std::unique_ptr<const Line> NextLine(int max_width) override;
};

/**
 * Get the actual ParagraphLayout for the given buffer.
 * @param buff The begin of the buffer.
 * @param buff_end The location after the last element in the buffer.
 * @param fontMapping THe mapping of the fonts.
 * @return The ParagraphLayout instance.
 */
/* static */ ParagraphLayouter *FallbackParagraphLayoutFactory::GetParagraphLayout(char32_t *buff, char32_t *buff_end, FontMap &fontMapping)
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
/* static */ size_t FallbackParagraphLayoutFactory::AppendToBuffer(char32_t *buff, [[maybe_unused]] const char32_t *buffer_last, char32_t c)
{
	assert(buff < buffer_last);
	*buff = c;
	return 1;
}

/**
 * Create the visual run.
 * @param font        The font to use for this run.
 * @param chars       The characters to use for this run.
 * @param char_count  The number of characters in this run.
 * @param char_offset This run's offset from the start of the layout input string.
 * @param x           The initial x position for this run.
 */
FallbackParagraphLayout::FallbackVisualRun::FallbackVisualRun(Font *font, const char32_t *chars, int char_count, int char_offset, int x) :
		font(font)
{
	const bool isbuiltin = font->fc->IsBuiltInFont();

	this->glyphs.reserve(char_count);
	this->glyph_to_char.reserve(char_count);

	/* Positions contains the location of the begin of each of the glyphs, and the end of the last one. */
	this->positions.resize(char_count * 2 + 2);
	this->positions[0] = x;

	for (int i = 0; i < char_count; i++) {
		const GlyphID &glyph_id = this->glyphs.emplace_back(font->fc->MapCharToGlyph(chars[i]));
		if (isbuiltin) {
			this->positions[2 * i + 1] = font->fc->GetAscender(); // Apply sprite font's ascender.
		} else if (chars[i] >= SCC_SPRITE_START && chars[i] <= SCC_SPRITE_END) {
			this->positions[2 * i + 1] = (font->fc->GetHeight() - ScaleSpriteTrad(FontCache::GetDefaultFontHeight(font->fc->GetSize()))) / 2; // Align sprite font to centre
		} else {
			this->positions[2 * i + 1] = 0;                       // No ascender adjustment.
		}
		this->positions[2 * i + 2] = this->positions[2 * i] + font->fc->GetGlyphWidth(glyph_id);
		this->glyph_to_char.push_back(char_offset + i);
	}
}

/**
 * Get the height of the line.
 * @return The maximum height of the line.
 */
int FallbackParagraphLayout::FallbackLine::GetLeading() const
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
int FallbackParagraphLayout::FallbackLine::GetWidth() const
{
	if (this->empty()) return 0;

	/*
	 * The last X position of a run contains is the end of that run.
	 * Since there is no left-to-right support, taking this value of
	 * the last run gives us the end of the line and thus the width.
	 */
	const auto &run = this->GetVisualRun(this->CountRuns() - 1);
	return (int)run.GetPositions()[run.GetGlyphCount() * 2];
}

/**
 * Get the number of runs in this line.
 * @return The number of runs.
 */
int FallbackParagraphLayout::FallbackLine::CountRuns() const
{
	return (uint)this->size();
}

/**
 * Get a specific visual run.
 * @return The visual run.
 */
const ParagraphLayouter::VisualRun &FallbackParagraphLayout::FallbackLine::GetVisualRun(int run) const
{
	return this->at(run);
}

/**
 * Create a new paragraph layouter.
 * @param buffer The characters of the paragraph.
 * @param length The length of the paragraph.
 * @param runs   The font mapping of this paragraph.
 */
FallbackParagraphLayout::FallbackParagraphLayout(char32_t *buffer, [[maybe_unused]] int length, FontMap &runs) : buffer_begin(buffer), buffer(buffer), runs(runs)
{
	assert(runs.rbegin()->first == length);
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
 * @return A Line, or nullptr when at the end of the paragraph.
 */
std::unique_ptr<const ParagraphLayouter::Line> FallbackParagraphLayout::NextLine(int max_width)
{
	/* Simple idea:
	 *  - split a line at a newline character, or at a space where we can break a line.
	 *  - split for a visual run whenever a new line happens, or the font changes.
	 */
	if (this->buffer == nullptr) return nullptr;

	std::unique_ptr<FallbackLine> l(new FallbackLine());

	if (*this->buffer == '\0') {
		/* Only a newline. */
		this->buffer = nullptr;
		l->emplace_back(this->runs.begin()->second, this->buffer, 0, 0, 0);
		return l;
	}

	int offset = this->buffer - this->buffer_begin;
	FontMap::iterator iter = this->runs.begin();
	while (iter->first <= offset) {
		++iter;
		assert(iter != this->runs.end());
	}

	const FontCache *fc = iter->second->fc;
	const char32_t *next_run = this->buffer_begin + iter->first;

	const char32_t *begin = this->buffer;
	const char32_t *last_space = nullptr;
	const char32_t *last_char;
	int width = 0;
	for (;;) {
		char32_t c = *this->buffer;
		last_char = this->buffer;

		if (c == '\0') {
			this->buffer = nullptr;
			break;
		}

		if (this->buffer == next_run) {
			int w = l->GetWidth();
			l->emplace_back(iter->second, begin, this->buffer - begin, begin - this->buffer_begin, w);
			++iter;
			assert(iter != this->runs.end());

			next_run = this->buffer_begin + iter->first;
			begin = this->buffer;
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
					this->buffer = nullptr;
					return l;
				}

				if (last_space == nullptr) {
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

	if (l->empty() || last_char - begin > 0) {
		int w = l->GetWidth();
		l->emplace_back(iter->second, begin, last_char - begin, begin - this->buffer_begin, w);
	}
	return l;
}
