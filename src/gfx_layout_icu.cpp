/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file gfx_layout_icu.cpp Handling of laying out with ICU / Harfbuzz. */

#include "stdafx.h"
#include "gfx_layout_icu.h"

#include "debug.h"
#include "strings_func.h"
#include "language.h"
#include "table/control_codes.h"
#include "zoom_func.h"

#include "3rdparty/icu/scriptrun.h"

#include <unicode/ubidi.h>
#include <unicode/brkiter.h>

#include <hb.h>
#include <hb-ft.h>

#include "safeguards.h"

/** harfbuzz doesn't use floats, so we need a value to scale position with to get sub-pixel precision. */
constexpr float FONT_SCALE = 64.0;

/**
 * Helper class to store the information of all the runs of a paragraph in.
 *
 * During itemization, more and more information is filled in.
 */
class ICURun {
public:
	int start; ///< Start of the run in the buffer.
	int length; ///< Length of the run in the buffer.
	UBiDiLevel level; ///< Embedding level of the run.
	UScriptCode script; ///< Script of the run.
	Font *font; ///< Font of the run.

	std::vector<GlyphID> glyphs; ///< The glyphs of the run. Valid after Shape() is called.
	std::vector<int> advance; ///< The advance (width) of the glyphs. Valid after Shape() is called.
	std::vector<int> glyph_to_char; ///< The mapping from glyphs to characters. Valid after Shape() is called.
	std::vector<float> positions; ///< The positions of the glyphs. Valid after Shape() is called.
	int total_advance = 0; ///< The total advance of the run. Valid after Shape() is called.

	ICURun(int start, int length, UBiDiLevel level, UScriptCode script = USCRIPT_UNKNOWN, Font *font = nullptr) : start(start), length(length), level(level), script(script), font(font) {}

	void Shape(UChar *buff, size_t length);
};

/**
 * Wrapper for doing layouts with ICU.
 */
class ICUParagraphLayout : public ParagraphLayouter {
public:
	/** Visual run contains data about the bit of text with the same font. */
	class ICUVisualRun : public ParagraphLayouter::VisualRun {
	private:
		std::vector<GlyphID> glyphs;
		std::vector<float> positions;
		std::vector<int> glyph_to_char;

		int total_advance;
		const Font *font;

	public:
		ICUVisualRun(const ICURun &run, int x);

		const GlyphID *GetGlyphs() const override { return this->glyphs.data(); }
		const float *GetPositions() const override { return this->positions.data(); }
		const int *GetGlyphToCharMap() const override { return this->glyph_to_char.data(); }

		const Font *GetFont() const override { return this->font; }
		int GetLeading() const override { return this->font->fc->GetHeight(); }
		int GetGlyphCount() const override { return this->glyphs.size(); }
		int GetAdvance() const { return this->total_advance; }
	};

	/** A single line worth of VisualRuns. */
	class ICULine : public std::vector<ICUVisualRun>, public ParagraphLayouter::Line {
	public:
		int GetLeading() const override;
		int GetWidth() const override;
		int CountRuns() const override { return (uint)this->size();  }
		const VisualRun &GetVisualRun(int run) const override { return this->at(run); }

		int GetInternalCharLength(char32_t c) const override
		{
			/* ICU uses UTF-16 internally which means we need to account for surrogate pairs. */
			return c >= 0x010000U ? 2 : 1;
		}
	};

private:
	std::vector<ICURun> runs;
	UChar *buff;
	size_t buff_length;
	std::vector<ICURun>::iterator current_run;
	int partial_offset;

public:
	ICUParagraphLayout(std::vector<ICURun> &runs, UChar *buff, size_t buff_length) : runs(runs), buff(buff), buff_length(buff_length)
	{
		this->Reflow();
	}

	~ICUParagraphLayout() override { }

	void Reflow() override
	{
		this->current_run = this->runs.begin();
		this->partial_offset = 0;
	}

	std::unique_ptr<const Line> NextLine(int max_width) override;
};

/**
 * Constructor for a new ICUVisualRun.
 *
 * It bases all information on the ICURun, which should already be shaped.
 *
 * @param run The ICURun to base the visual run on.
 * @param x The offset of the run on the line.
 */
ICUParagraphLayout::ICUVisualRun::ICUVisualRun(const ICURun &run, int x) :
	glyphs(run.glyphs), glyph_to_char(run.glyph_to_char), total_advance(run.total_advance), font(run.font)
{
	/* If there are no positions, the ICURun was not Shaped; that should never happen. */
	assert(!run.positions.empty());
	this->positions.reserve(run.positions.size());

	/* "positions" is an array of x/y. So we need to alternate. */
	bool is_x = true;
	for (auto &position : run.positions) {
		if (is_x) {
			this->positions.push_back(position + x);
		} else {
			this->positions.push_back(position);
		}

		is_x = !is_x;
	}
}

/**
 * Shape a single run.
 *
 * @param buff The buffer of which a partial (depending on start/length of the run) will be shaped.
 * @param length The length of the buffer.
 */
void ICURun::Shape(UChar *buff, size_t buff_length)
{
	auto hbfont = hb_ft_font_create_referenced(*(static_cast<const FT_Face *>(font->fc->GetOSHandle())));
	hb_font_set_scale(hbfont, this->font->fc->GetFontSize() * FONT_SCALE, this->font->fc->GetFontSize() * FONT_SCALE);

	/* ICU buffer is in UTF-16. */
	auto hbbuf = hb_buffer_create();
	hb_buffer_add_utf16(hbbuf, reinterpret_cast<uint16_t *>(buff), buff_length, this->start, this->length);

	/* Set all the properties of this segment. */
	hb_buffer_set_direction(hbbuf, (this->level & 1) == 1 ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
	hb_buffer_set_script(hbbuf, hb_script_from_string(uscript_getShortName(this->script), -1));
	hb_buffer_set_language(hbbuf, hb_language_from_string(_current_language->isocode, -1));
	hb_buffer_set_cluster_level(hbbuf, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES);

	/* Shape the segment. */
	hb_shape(hbfont, hbbuf, nullptr, 0);

	unsigned int glyph_count;
	auto glyph_info = hb_buffer_get_glyph_infos(hbbuf, &glyph_count);
	auto glyph_pos = hb_buffer_get_glyph_positions(hbbuf, &glyph_count);

	/* Make sure any former run is lost. */
	this->glyphs.clear();
	this->glyph_to_char.clear();
	this->positions.clear();
	this->advance.clear();

	/* Reserve space, as we already know the size. */
	this->glyphs.reserve(glyph_count);
	this->glyph_to_char.reserve(glyph_count);
	this->positions.reserve(glyph_count * 2 + 2);
	this->advance.reserve(glyph_count);

	/* Prepare the glyphs/position. ICUVisualRun will give the position an offset if needed. */
	hb_position_t advance = 0;
	for (unsigned int i = 0; i < glyph_count; i++) {
		int x_advance;

		if (buff[glyph_info[i].cluster] >= SCC_SPRITE_START && buff[glyph_info[i].cluster] <= SCC_SPRITE_END) {
			auto glyph = this->font->fc->MapCharToGlyph(buff[glyph_info[i].cluster]);

			this->glyphs.push_back(glyph);
			this->positions.push_back(advance);
			this->positions.push_back((this->font->fc->GetHeight() - ScaleSpriteTrad(FontCache::GetDefaultFontHeight(this->font->fc->GetSize()))) / 2); // Align sprite font to centre
			x_advance = this->font->fc->GetGlyphWidth(glyph);
		} else {
			this->glyphs.push_back(glyph_info[i].codepoint);
			this->positions.push_back(glyph_pos[i].x_offset / FONT_SCALE + advance);
			this->positions.push_back(glyph_pos[i].y_offset / FONT_SCALE);
			x_advance = glyph_pos[i].x_advance / FONT_SCALE;
		}

		this->glyph_to_char.push_back(glyph_info[i].cluster);
		this->advance.push_back(x_advance);
		advance += x_advance;
	}

	/* Position has one more element to close off the array. */
	this->positions.push_back(advance);
	this->positions.push_back(0);

	/* Track the total advancement we made. */
	this->total_advance = advance;

	hb_buffer_destroy(hbbuf);
	hb_font_destroy(hbfont);
}

/**
 * Get the height of the line.
 * @return The maximum height of the line.
 */
int ICUParagraphLayout::ICULine::GetLeading() const
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
int ICUParagraphLayout::ICULine::GetWidth() const
{
	int length = 0;
	for (const auto &run : *this) {
		length += run.GetAdvance();
	}

	return length;
}

/**
 * Itemize the string into runs per embedding level.
 *
 * Later on, based on the levels, we can deduce the order of a subset of runs.
 *
 * @param buff The string to itemize.
 * @param length The length of the string.
 * @return The runs.
 */
std::vector<ICURun> ItemizeBidi(UChar *buff, size_t length)
{
	auto ubidi = ubidi_open();

	auto parLevel = _current_text_dir == TD_RTL ? UBIDI_RTL : UBIDI_LTR;

	UErrorCode err = U_ZERO_ERROR;
	ubidi_setPara(ubidi, buff, length, parLevel, nullptr, &err);
	if (U_FAILURE(err)) {
		Debug(fontcache, 0, "Failed to set paragraph: %s", u_errorName(err));
		ubidi_close(ubidi);
		return std::vector<ICURun>();
	}

	int32_t count = ubidi_countRuns(ubidi, &err);
	if (U_FAILURE(err)) {
		Debug(fontcache, 0, "Failed to count runs: %s", u_errorName(err));
		ubidi_close(ubidi);
		return std::vector<ICURun>();
	}

	std::vector<ICURun> runs;
	runs.reserve(count);

	/* Find the breakpoints for the logical runs. So we get runs that say "from START to END". */
	int32_t logical_pos = 0;
	while (static_cast<size_t>(logical_pos) < length) {
		auto start_pos = logical_pos;

		/* Fetch the embedding level, so we can order bidi correctly later on. */
		UBiDiLevel level;
		ubidi_getLogicalRun(ubidi, start_pos, &logical_pos, &level);

		runs.emplace_back(ICURun(start_pos, logical_pos - start_pos, level));
	}

	assert(static_cast<size_t>(count) == runs.size());

	ubidi_close(ubidi);
	return runs;
}

/**
 * Itemize the string into runs per script, based on the previous created runs.
 *
 * Basically, this always returns the same or more runs than given.
 *
 * @param buff The string to itemize.
 * @param length The length of the string.
 * @param runs_current The current runs.
 * @return The runs.
 */
std::vector<ICURun> ItemizeScript(UChar *buff, size_t length, std::vector<ICURun> &runs_current)
{
	std::vector<ICURun> runs;
	icu::ScriptRun script_itemizer(buff, length);

	int cur_pos = 0;
	auto cur_run = runs_current.begin();
	while (true) {
		while (cur_pos < script_itemizer.getScriptEnd() && cur_run != runs_current.end()) {
			int stop_pos = std::min(script_itemizer.getScriptEnd(), cur_run->start + cur_run->length);
			assert(stop_pos - cur_pos > 0);

			runs.push_back(ICURun(cur_pos, stop_pos - cur_pos, cur_run->level, script_itemizer.getScriptCode()));

			if (stop_pos == cur_run->start + cur_run->length) cur_run++;
			cur_pos = stop_pos;
		}

		if (!script_itemizer.next()) break;
	}

	return runs;
}

/**
 * Itemize the string into runs per style, based on the previous created runs.
 *
 * Basically, this always returns the same or more runs than given.
 *
 * @param runs_current The current runs.
 * @param font_mapping The font mapping.
 * @return The runs.
 */
std::vector<ICURun> ItemizeStyle(std::vector<ICURun> &runs_current, FontMap &font_mapping)
{
	std::vector<ICURun> runs;

	int cur_pos = 0;
	auto cur_run = runs_current.begin();
	for (auto const &font_map : font_mapping) {
		while (cur_pos < font_map.first && cur_run != runs_current.end()) {
			int stop_pos = std::min(font_map.first, cur_run->start + cur_run->length);
			assert(stop_pos - cur_pos > 0);

			runs.push_back(ICURun(cur_pos, stop_pos - cur_pos, cur_run->level, cur_run->script, font_map.second));

			if (stop_pos == cur_run->start + cur_run->length) cur_run++;
			cur_pos = stop_pos;
		}
	}

	return runs;
}

/* static */ ParagraphLayouter *ICUParagraphLayoutFactory::GetParagraphLayout(UChar *buff, UChar *buff_end, FontMap &font_mapping)
{
	size_t length = buff_end - buff;
	/* Can't layout an empty string. */
	if (length == 0) return nullptr;

	/* Can't layout our in-built sprite fonts. */
	for (auto const &pair : font_mapping) {
		if (pair.second->fc->IsBuiltInFont()) return nullptr;
	}

	auto runs = ItemizeBidi(buff, length);
	runs = ItemizeScript(buff, length, runs);
	runs = ItemizeStyle(runs, font_mapping);

	if (runs.empty()) return nullptr;

	for (auto &run : runs) {
		run.Shape(buff, length);
	}

	return new ICUParagraphLayout(runs, buff, length);
}

std::unique_ptr<const ICUParagraphLayout::Line> ICUParagraphLayout::NextLine(int max_width)
{
	std::vector<ICURun>::iterator start_run = this->current_run;
	std::vector<ICURun>::iterator last_run = this->current_run;

	if (start_run == this->runs.end()) return nullptr;

	int cur_width = 0;

	/* Add remaining width of the first run if it is a broken run. */
	if (this->partial_offset > 0) {
		if ((start_run->level & 1) == 0) {
			for (size_t i = this->partial_offset; i < start_run->advance.size(); i++) {
				cur_width += start_run->advance[i];
			}
		} else {
			for (int i = 0; i < this->partial_offset; i++) {
				cur_width += start_run->advance[i];
			}
		}
		last_run++;
	}

	/* Gather runs until the line is full. */
	while (last_run != this->runs.end() && cur_width < max_width) {
		cur_width += last_run->total_advance;
		last_run++;
	}

	/* If the text does not fit into the available width, find a suitable breaking point. */
	int new_partial_length = 0;
	if (cur_width > max_width) {
		auto locale = icu::Locale(_current_language->isocode);

		/* Create a break-iterator to find a good place to break lines. */
		UErrorCode err = U_ZERO_ERROR;
		auto break_iterator = icu::BreakIterator::createLineInstance(locale, err);
		break_iterator->setText(icu::UnicodeString(this->buff, this->buff_length));

		auto overflow_run = last_run - 1;

		/* Find the last glyph that fits. */
		size_t index;
		if ((overflow_run->level & 1) == 0) {
			/* LTR */
			for (index = overflow_run->glyphs.size(); index > 0; index--) {
				cur_width -= overflow_run->advance[index - 1];
				if (cur_width <= max_width) break;
			}
			index--;
		} else {
			/* RTL */
			for (index = 0; index < overflow_run->glyphs.size(); index++) {
				cur_width -= overflow_run->advance[index];
				if (cur_width <= max_width) break;
			}
		}

		/* Find the character that matches; this is the start of the cluster. */
		auto char_pos = overflow_run->glyph_to_char[index];

		/* See if there is a good breakpoint inside this run. */
		int32_t break_pos = break_iterator->preceding(char_pos + 1);
		if (break_pos != icu::BreakIterator::DONE && break_pos > overflow_run->start + this->partial_offset) {
			/* There is a line-break inside this run that is suitable. */
			new_partial_length = break_pos - overflow_run->start - this->partial_offset;
		} else if (overflow_run != start_run) {
			/* There is no suitable line-break in this run, but it is also not
			 * the only run on this line. So we remove the run. */
			last_run--;
		} else {
			/* There is no suitable line-break and this is the only run on the
			 * line. So we break at the cluster. This is not pretty, but the
			 * best we can do. */
			new_partial_length = char_pos - overflow_run->start - this->partial_offset;
		}
	}

	/* Reorder the runs on this line for display. */
	std::vector<UBiDiLevel> bidi_level;
	for (auto run = start_run; run != last_run; run++) {
		bidi_level.push_back(run->level);
	}
	std::vector<int32_t> vis_to_log(bidi_level.size());
	ubidi_reorderVisual(bidi_level.data(), bidi_level.size(), vis_to_log.data());

	/* Create line. */
	std::unique_ptr<ICULine> line(new ICULine());

	int cur_pos = 0;
	for (auto &i : vis_to_log) {
		auto i_run = start_run + i;
		/* Copy the ICURun here, so we can modify it in case of a partial. */
		ICURun run = *i_run;

		if (i_run == last_run - 1 && new_partial_length > 0) {
			if (i_run == start_run && this->partial_offset > 0) {
				assert(run.length > this->partial_offset);
				run.start += this->partial_offset;
				run.length -= this->partial_offset;
			}

			assert(run.length > new_partial_length);
			run.length = new_partial_length;

			run.Shape(this->buff, this->buff_length);
		} else if (i_run == start_run && this->partial_offset > 0) {
			assert(run.length > this->partial_offset);

			run.start += this->partial_offset;
			run.length -= this->partial_offset;

			run.Shape(this->buff, this->buff_length);
		}

		auto total_advance = run.total_advance;
		line->emplace_back(std::move(run), cur_pos);
		cur_pos += total_advance;
	}

	if (new_partial_length > 0) {
		this->current_run = last_run - 1;
		this->partial_offset += new_partial_length;
	} else {
		this->current_run = last_run;
		this->partial_offset = 0;
	}

	return line;
}

/* static */ size_t ICUParagraphLayoutFactory::AppendToBuffer(UChar *buff, const UChar *buffer_last, char32_t c)
{
	assert(buff < buffer_last);
	/* Transform from UTF-32 to internal ICU format of UTF-16. */
	int32_t length = 0;
	UErrorCode err = U_ZERO_ERROR;
	u_strFromUTF32(buff, buffer_last - buff, &length, (UChar32*)&c, 1, &err);
	return length;
}
