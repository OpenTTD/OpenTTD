/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file string_uniscribe.cpp Functions related to laying out text on Win32. */

#include "../../stdafx.h"
#include "../../debug.h"
#include "string_uniscribe.h"
#include "../../language.h"
#include "../../strings_func.h"
#include "../../string_func.h"
#include "../../table/control_codes.h"
#include "../../zoom_func.h"
#include "win32.h"

#include <windows.h>
#include <usp10.h>

#include "../../safeguards.h"

#ifdef _MSC_VER
#	pragma comment(lib, "usp10")
#endif


/** Uniscribe cache for internal font information, cleared when OTTD changes fonts. */
static SCRIPT_CACHE _script_cache[FS_END];

/**
 * Contains all information about a run of characters. A run are consecutive
 * characters that share a single font and language.
 */
struct UniscribeRun {
	int pos;
	int len;
	Font *font;

	std::vector<GlyphID> ft_glyphs;

	SCRIPT_ANALYSIS sa;
	std::vector<WORD> char_to_glyph;

	std::vector<SCRIPT_VISATTR> vis_attribs;
	std::vector<WORD> glyphs;
	std::vector<int> advances;
	std::vector<GOFFSET> offsets;
	int total_advance;

	UniscribeRun(int pos, int len, Font *font, SCRIPT_ANALYSIS &sa) : pos(pos), len(len), font(font), sa(sa) {}
};

/** Break a string into language formatting ranges. */
static std::vector<SCRIPT_ITEM> UniscribeItemizeString(UniscribeParagraphLayoutFactory::CharType *buff, int32_t length);
/** Generate and place glyphs for a run of characters. */
static bool UniscribeShapeRun(const UniscribeParagraphLayoutFactory::CharType *buff, UniscribeRun &range);

/**
 * Wrapper for doing layouts with Uniscribe.
 */
class UniscribeParagraphLayout : public ParagraphLayouter {
private:
	const UniscribeParagraphLayoutFactory::CharType *text_buffer;

	std::vector<UniscribeRun> ranges; ///< All runs of the text.
	std::vector<UniscribeRun>::iterator cur_range; ///< The next run to be output.
	int cur_range_offset = 0; ///< Offset from the start of the current run from where to output.

public:
	/** Visual run contains data about the bit of text with the same font. */
	class UniscribeVisualRun : public ParagraphLayouter::VisualRun {
	private:
		std::vector<GlyphID> glyphs;
		std::vector<float> positions;
		std::vector<WORD> char_to_glyph;

		int start_pos;
		int total_advance;
		int num_glyphs;
		Font *font;

		mutable int *glyph_to_char = nullptr;

	public:
		UniscribeVisualRun(const UniscribeRun &range, int x);
		UniscribeVisualRun(UniscribeVisualRun &&other) noexcept;
		~UniscribeVisualRun() override
		{
			free(this->glyph_to_char);
		}

		const GlyphID *GetGlyphs() const override { return &this->glyphs[0]; }
		const float *GetPositions() const override { return &this->positions[0]; }
		const int *GetGlyphToCharMap() const override;

		const Font *GetFont() const override { return this->font;  }
		int GetLeading() const override { return this->font->fc->GetHeight(); }
		int GetGlyphCount() const override { return this->num_glyphs; }
		int GetAdvance() const { return this->total_advance; }
	};

	/** A single line worth of VisualRuns. */
	class UniscribeLine : public std::vector<UniscribeVisualRun>, public ParagraphLayouter::Line {
	public:
		int GetLeading() const override;
		int GetWidth() const override;
		int CountRuns() const override { return (uint)this->size();  }
		const VisualRun &GetVisualRun(int run) const override { return this->at(run);  }

		int GetInternalCharLength(char32_t c) const override
		{
			/* Uniscribe uses UTF-16 internally which means we need to account for surrogate pairs. */
			return c >= 0x010000U ? 2 : 1;
		}
	};

	UniscribeParagraphLayout(std::vector<UniscribeRun> &ranges, const UniscribeParagraphLayoutFactory::CharType *buffer) : text_buffer(buffer), ranges(ranges)
	{
		this->Reflow();
	}

	~UniscribeParagraphLayout() override {}

	void Reflow() override
	{
		this->cur_range = this->ranges.begin();
		this->cur_range_offset = 0;
	}

	std::unique_ptr<const Line> NextLine(int max_width) override;
};

void UniscribeResetScriptCache(FontSize size)
{
	if (_script_cache[size] != nullptr) {
		ScriptFreeCache(&_script_cache[size]);
		_script_cache[size] = nullptr;
	}
}

/** Load the matching native Windows font. */
static HFONT HFontFromFont(Font *font)
{
	if (font->fc->GetOSHandle() != nullptr) return CreateFontIndirect(reinterpret_cast<PLOGFONT>(const_cast<void *>(font->fc->GetOSHandle())));

	LOGFONT logfont;
	ZeroMemory(&logfont, sizeof(LOGFONT));
	logfont.lfHeight = font->fc->GetHeight();
	logfont.lfWeight = FW_NORMAL;
	logfont.lfCharSet = DEFAULT_CHARSET;
	convert_to_fs(font->fc->GetFontName(), logfont.lfFaceName, lengthof(logfont.lfFaceName));

	return CreateFontIndirect(&logfont);
}

/** Determine the glyph positions for a run. */
static bool UniscribeShapeRun(const UniscribeParagraphLayoutFactory::CharType *buff, UniscribeRun &range)
{
	/* Initial size guess for the number of glyphs recommended by Uniscribe. */
	range.glyphs.resize(range.len * 3 / 2 + 16);
	range.vis_attribs.resize(range.glyphs.size());

	/* The char-to-glyph array is the same size as the input. */
	range.char_to_glyph.resize(range.len);

	HDC temp_dc = nullptr;
	HFONT old_font = nullptr;
	HFONT cur_font = nullptr;

	while (true) {
		/* Shape the text run by determining the glyphs needed for display. */
		int glyphs_used = 0;
		HRESULT hr = ScriptShape(temp_dc, &_script_cache[range.font->fc->GetSize()], buff + range.pos, range.len, (int)range.glyphs.size(), &range.sa, &range.glyphs[0], &range.char_to_glyph[0], &range.vis_attribs[0], &glyphs_used);

		if (SUCCEEDED(hr)) {
			range.glyphs.resize(glyphs_used);
			range.vis_attribs.resize(glyphs_used);

			/* Calculate the glyph positions. */
			ABC abc;
			range.advances.resize(range.glyphs.size());
			range.offsets.resize(range.glyphs.size());
			hr = ScriptPlace(temp_dc, &_script_cache[range.font->fc->GetSize()], &range.glyphs[0], (int)range.glyphs.size(), &range.vis_attribs[0], &range.sa, &range.advances[0], &range.offsets[0], &abc);
			if (SUCCEEDED(hr)) {
				/* We map our special sprite chars to values that don't fit into a WORD. Copy the glyphs
				 * into a new vector and query the real glyph to use for these special chars. */
				range.ft_glyphs.resize(range.glyphs.size());
				for (size_t g_id = 0; g_id < range.glyphs.size(); g_id++) {
					range.ft_glyphs[g_id] = range.glyphs[g_id];
				}
				for (int i = 0; i < range.len; i++) {
					if (buff[range.pos + i] >= SCC_SPRITE_START && buff[range.pos + i] <= SCC_SPRITE_END) {
						auto pos = range.char_to_glyph[i];
						range.ft_glyphs[pos] = range.font->fc->MapCharToGlyph(buff[range.pos + i]);
						range.offsets[pos].dv = (range.font->fc->GetHeight() - ScaleSpriteTrad(FontCache::GetDefaultFontHeight(range.font->fc->GetSize()))) / 2; // Align sprite font to centre
						range.advances[pos] = range.font->fc->GetGlyphWidth(range.ft_glyphs[pos]);
					}
				}

				range.total_advance = 0;
				for (size_t i = 0; i < range.advances.size(); i++) {
#ifdef WITH_FREETYPE
					/* FreeType and GDI/Uniscribe seems to occasionally disagree over the width of a glyph. */
					if (range.advances[i] > 0 && range.ft_glyphs[i] != 0xFFFF) range.advances[i] = range.font->fc->GetGlyphWidth(range.ft_glyphs[i]);
#endif
					range.total_advance += range.advances[i];
				}
				break;
			}
		}

		if (hr == E_OUTOFMEMORY) {
			/* The glyph buffer needs to be larger. Just double it every time. */
			range.glyphs.resize(range.glyphs.size() * 2);
			range.vis_attribs.resize(range.vis_attribs.size() * 2);
		} else if (hr == E_PENDING) {
			/* Glyph data is not in cache, load native font. */
			cur_font = HFontFromFont(range.font);
			if (cur_font == nullptr) return false; // Sorry, no dice.

			temp_dc = CreateCompatibleDC(nullptr);
			SetMapMode(temp_dc, MM_TEXT);
			old_font = (HFONT)SelectObject(temp_dc, cur_font);
		} else if (hr == USP_E_SCRIPT_NOT_IN_FONT && range.sa.eScript != SCRIPT_UNDEFINED) {
			/* Try again with the generic shaping engine. */
			range.sa.eScript = SCRIPT_UNDEFINED;
		} else {
			/* Some unknown other error. */
			if (temp_dc != nullptr) {
				SelectObject(temp_dc, old_font);
				DeleteObject(cur_font);
				ReleaseDC(nullptr, temp_dc);
			}
			return false;
		}
	}

	if (temp_dc != nullptr) {
		SelectObject(temp_dc, old_font);
		DeleteObject(cur_font);
		ReleaseDC(nullptr, temp_dc);
	}

	return true;
}

static std::vector<SCRIPT_ITEM> UniscribeItemizeString(UniscribeParagraphLayoutFactory::CharType *buff, int32_t length)
{
	/* Itemize text. */
	SCRIPT_CONTROL control;
	ZeroMemory(&control, sizeof(SCRIPT_CONTROL));
	control.uDefaultLanguage = _current_language->winlangid;

	SCRIPT_STATE state;
	ZeroMemory(&state, sizeof(SCRIPT_STATE));
	state.uBidiLevel = _current_text_dir == TD_RTL ? 1 : 0;

	std::vector<SCRIPT_ITEM> items(16);
	while (true) {
		/* We subtract one from max_items to work around a buffer overflow on some older versions of Windows. */
		int generated = 0;
		HRESULT hr = ScriptItemize(buff, length, (int)items.size() - 1, &control, &state, &items[0], &generated);

		if (SUCCEEDED(hr)) {
			/* Resize the item buffer. Note that Uniscribe will always add an additional end sentinel item. */
			items.resize(generated + 1);
			break;
		}
		/* Some kind of error except item buffer too small. */
		if (hr != E_OUTOFMEMORY) return std::vector<SCRIPT_ITEM>();

		items.resize(items.size() * 2);
	}

	return items;
}

/* static */ ParagraphLayouter *UniscribeParagraphLayoutFactory::GetParagraphLayout(CharType *buff, CharType *buff_end, FontMap &fontMapping)
{
	int32_t length = buff_end - buff;
	/* Can't layout an empty string. */
	if (length == 0) return nullptr;

	/* Can't layout our in-built sprite fonts. */
	for (auto const &pair : fontMapping) {
		if (pair.second->fc->IsBuiltInFont()) return nullptr;
	}

	/* Itemize text. */
	std::vector<SCRIPT_ITEM> items = UniscribeItemizeString(buff, length);
	if (items.empty()) return nullptr;

	/* Build ranges from the items and the font map. A range is a run of text
	 * that is part of a single item and formatted using a single font style. */
	std::vector<UniscribeRun> ranges;

	int cur_pos = 0;
	std::vector<SCRIPT_ITEM>::iterator cur_item = items.begin();
	for (auto const &i : fontMapping) {
		while (cur_pos < i.first && cur_item != items.end() - 1) {
			/* Add a range that spans the intersection of the remaining item and font run. */
			int stop_pos = std::min(i.first, (cur_item + 1)->iCharPos);
			assert(stop_pos - cur_pos > 0);
			ranges.push_back(UniscribeRun(cur_pos, stop_pos - cur_pos, i.second, cur_item->a));

			/* Shape the range. */
			if (!UniscribeShapeRun(buff, ranges.back())) {
				return nullptr;
			}

			/* If we are at the end of the current item, advance to the next item. */
			if (stop_pos == (cur_item + 1)->iCharPos) cur_item++;
			cur_pos = stop_pos;
		}
	}

	return new UniscribeParagraphLayout(ranges, buff);
}

/* virtual */ std::unique_ptr<const ParagraphLayouter::Line> UniscribeParagraphLayout::NextLine(int max_width)
{
	std::vector<UniscribeRun>::iterator start_run = this->cur_range;
	std::vector<UniscribeRun>::iterator last_run = this->cur_range;

	if (start_run == this->ranges.end()) return nullptr;

	/* Add remaining width of the first run if it is a broken run. */
	int cur_width = 0;
	if (this->cur_range_offset != 0) {
		std::vector<int> dx(start_run->len);
		ScriptGetLogicalWidths(&start_run->sa, start_run->len, (int)start_run->glyphs.size(), &start_run->advances[0], &start_run->char_to_glyph[0], &start_run->vis_attribs[0], &dx[0]);

		for (std::vector<int>::const_iterator c = dx.begin() + this->cur_range_offset; c != dx.end(); c++) {
			cur_width += *c;
		}
		++last_run;
	}

	/* Gather runs until the line is full. */
	while (last_run != this->ranges.end() && cur_width <= max_width) {
		cur_width += last_run->total_advance;
		++last_run;
	}

	/* If the text does not fit into the available width, find a suitable breaking point. */
	int remaining_offset = (last_run - 1)->len + 1;
	int whitespace_count = 0;
	if (cur_width > max_width) {
		std::vector<SCRIPT_LOGATTR> log_attribs;

		/* Get word break information. */
		int width_avail = max_width;
		int num_chars = this->cur_range_offset;
		int start_offs = this->cur_range_offset;
		int last_cluster = this->cur_range_offset + 1;
		for (std::vector<UniscribeRun>::iterator r = start_run; r != last_run; r++) {
			log_attribs.resize(r->pos - start_run->pos + r->len);
			if (FAILED(ScriptBreak(this->text_buffer + r->pos + start_offs, r->len - start_offs, &r->sa, &log_attribs[r->pos - start_run->pos + start_offs]))) return nullptr;

			std::vector<int> dx(r->len);
			ScriptGetLogicalWidths(&r->sa, r->len, (int)r->glyphs.size(), &r->advances[0], &r->char_to_glyph[0], &r->vis_attribs[0], &dx[0]);

			/* Count absolute max character count on the line. */
			for (int c = start_offs; c < r->len && width_avail > 0; c++, num_chars++) {
				if (c > start_offs && log_attribs[num_chars].fCharStop) last_cluster = num_chars;
				width_avail -= dx[c];
			}

			start_offs = 0;
		}

		/* Walk backwards to find the last suitable breaking point. */
		while (--num_chars > this->cur_range_offset && !log_attribs[num_chars].fSoftBreak && !log_attribs[num_chars].fWhiteSpace) {}

		if (num_chars == this->cur_range_offset) {
			/* Didn't find any suitable word break point, just break on the last cluster boundary. */
			num_chars = last_cluster;
		}

		/* Eat any whitespace characters before the breaking point. */
		while (num_chars - 1 > this->cur_range_offset && log_attribs[num_chars - 1].fWhiteSpace) num_chars--;
		/* Count whitespace after the breaking point. */
		while (num_chars + whitespace_count < (int)log_attribs.size() && log_attribs[num_chars + whitespace_count].fWhiteSpace) whitespace_count++;

		/* Get last run that corresponds to the number of characters to show. */
		for (std::vector<UniscribeRun>::iterator run = start_run; run != last_run; run++) {
			num_chars -= run->len;

			if (num_chars <= 0) {
				remaining_offset = num_chars + run->len + 1;
				last_run = run + 1;
				assert(remaining_offset - 1 > 0);
				break;
			}
		}
	}

	/* Build display order from the runs. */
	std::vector<BYTE> bidi_level;
	for (std::vector<UniscribeRun>::iterator r = start_run; r != last_run; r++) {
		bidi_level.push_back(r->sa.s.uBidiLevel);
	}
	std::vector<INT> vis_to_log(bidi_level.size());
	if (FAILED(ScriptLayout((int)bidi_level.size(), &bidi_level[0], &vis_to_log[0], nullptr))) return nullptr;

	/* Create line. */
	std::unique_ptr<UniscribeLine> line(new UniscribeLine());

	int cur_pos = 0;
	for (std::vector<INT>::iterator l = vis_to_log.begin(); l != vis_to_log.end(); l++) {
		std::vector<UniscribeRun>::iterator i_run = start_run + *l;
		UniscribeRun run = *i_run;

		/* Partial run after line break (either start or end)? Reshape run to get the first/last glyphs right. */
		if (i_run == last_run - 1 && remaining_offset < (last_run - 1)->len) {
			run.len = remaining_offset - 1;

			if (!UniscribeShapeRun(this->text_buffer, run)) return nullptr;
		}
		if (i_run == start_run && this->cur_range_offset > 0) {
			assert(run.len - this->cur_range_offset > 0);
			run.pos += this->cur_range_offset;
			run.len -= this->cur_range_offset;

			if (!UniscribeShapeRun(this->text_buffer, run)) return nullptr;
		}

		line->emplace_back(run, cur_pos);
		cur_pos += run.total_advance;
	}

	if (remaining_offset + whitespace_count - 1 < (last_run - 1)->len) {
		/* We didn't use up all of the last run, store remainder for the next line. */
		this->cur_range_offset = remaining_offset + whitespace_count - 1;
		this->cur_range = last_run - 1;
		assert(this->cur_range->len > this->cur_range_offset);
	} else {
		this->cur_range_offset = 0;
		this->cur_range = last_run;
	}

	return line;
}

/**
 * Get the height of the line.
 * @return The maximum height of the line.
 */
int UniscribeParagraphLayout::UniscribeLine::GetLeading() const
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
int UniscribeParagraphLayout::UniscribeLine::GetWidth() const
{
	int length = 0;
	for (const auto &run : *this) {
		length += run.GetAdvance();
	}

	return length;
}

UniscribeParagraphLayout::UniscribeVisualRun::UniscribeVisualRun(const UniscribeRun &range, int x) : glyphs(range.ft_glyphs), char_to_glyph(range.char_to_glyph), start_pos(range.pos), total_advance(range.total_advance), font(range.font)
{
	this->num_glyphs = (int)glyphs.size();
	this->positions.resize(this->num_glyphs * 2 + 2);

	int advance = 0;
	for (int i = 0; i < this->num_glyphs; i++) {
		this->positions[i * 2 + 0] = range.offsets[i].du + advance + x;
		this->positions[i * 2 + 1] = range.offsets[i].dv;

		advance += range.advances[i];
	}
	this->positions[this->num_glyphs * 2] = advance + x;
}

UniscribeParagraphLayout::UniscribeVisualRun::UniscribeVisualRun(UniscribeVisualRun&& other) noexcept
								: glyphs(std::move(other.glyphs)), positions(std::move(other.positions)), char_to_glyph(std::move(other.char_to_glyph)),
								  start_pos(other.start_pos), total_advance(other.total_advance), num_glyphs(other.num_glyphs), font(other.font)
{
	this->glyph_to_char = other.glyph_to_char;
	other.glyph_to_char = nullptr;
}

const int *UniscribeParagraphLayout::UniscribeVisualRun::GetGlyphToCharMap() const
{
	if (this->glyph_to_char == nullptr) {
		this->glyph_to_char = CallocT<int>(this->GetGlyphCount());

		/* The char to glyph array contains the first glyph index of the cluster that is associated
		 * with each character. It is possible for a cluster to be formed of several chars. */
		for (int c = 0; c < (int)this->char_to_glyph.size(); c++) {
			/* If multiple chars map to one glyph, only refer back to the first character. */
			if (this->glyph_to_char[this->char_to_glyph[c]] == 0) this->glyph_to_char[this->char_to_glyph[c]] = c + this->start_pos;
		}

		/* We only marked the first glyph of each cluster in the loop above. Fill the gaps. */
		int last_char = this->glyph_to_char[0];
		for (int g = 0; g < this->GetGlyphCount(); g++) {
			if (this->glyph_to_char[g] != 0) last_char = this->glyph_to_char[g];
			this->glyph_to_char[g] = last_char;
		}
	}

	return this->glyph_to_char;
}


/* virtual */ void UniscribeStringIterator::SetString(const char *s)
{
	const char *string_base = s;

	this->utf16_to_utf8.clear();
	this->str_info.clear();
	this->cur_pos = 0;

	/* Uniscribe operates on UTF-16, thus we have to convert the input string.
	 * To be able to return proper offsets, we have to create a mapping at the same time. */
	std::vector<wchar_t> utf16_str;     ///< UTF-16 copy of the string.
	while (*s != '\0') {
		size_t idx = s - string_base;

		char32_t c = Utf8Consume(&s);
		if (c < 0x10000) {
			utf16_str.push_back((wchar_t)c);
		} else {
			/* Make a surrogate pair. */
			utf16_str.push_back((wchar_t)(0xD800 + ((c - 0x10000) >> 10)));
			utf16_str.push_back((wchar_t)(0xDC00 + ((c - 0x10000) & 0x3FF)));
			this->utf16_to_utf8.push_back(idx);
		}
		this->utf16_to_utf8.push_back(idx);
	}
	this->utf16_to_utf8.push_back(s - string_base);

	/* Query Uniscribe for word and cluster break information. */
	this->str_info.resize(utf16_to_utf8.size());

	if (!utf16_str.empty()) {
		/* Itemize string into language runs. */
		std::vector<SCRIPT_ITEM> runs = UniscribeItemizeString(&utf16_str[0], (int32_t)utf16_str.size());

		for (std::vector<SCRIPT_ITEM>::const_iterator run = runs.begin(); !runs.empty() && run != runs.end() - 1; run++) {
			/* Get information on valid word and character break.s */
			int len = (run + 1)->iCharPos - run->iCharPos;
			std::vector<SCRIPT_LOGATTR> attr(len);
			ScriptBreak(&utf16_str[run->iCharPos], len, &run->a, &attr[0]);

			/* Extract the information we're interested in. */
			for (size_t c = 0; c < attr.size(); c++) {
				/* First character of a run is always a valid word break. */
				this->str_info[c + run->iCharPos].word_stop = attr[c].fWordStop || c == 0;
				this->str_info[c + run->iCharPos].char_stop = attr[c].fCharStop;
			}
		}
	}

	/* End-of-string is always a valid stopping point. */
	this->str_info.back().char_stop = true;
	this->str_info.back().word_stop = true;
}

/* virtual */ size_t UniscribeStringIterator::SetCurPosition(size_t pos)
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

/* virtual */ size_t UniscribeStringIterator::Next(IterType what)
{
	assert(this->cur_pos <= this->utf16_to_utf8.size());
	assert(what == StringIterator::ITER_CHARACTER || what == StringIterator::ITER_WORD);

	if (this->cur_pos == this->utf16_to_utf8.size()) return END;

	do {
		this->cur_pos++;
	} while (this->cur_pos < this->utf16_to_utf8.size() && (what  == ITER_WORD ? !this->str_info[this->cur_pos].word_stop : !this->str_info[this->cur_pos].char_stop));

	return this->cur_pos == this->utf16_to_utf8.size() ? END : this->utf16_to_utf8[this->cur_pos];
}

/*virtual */ size_t UniscribeStringIterator::Prev(IterType what)
{
	assert(this->cur_pos <= this->utf16_to_utf8.size());
	assert(what == StringIterator::ITER_CHARACTER || what == StringIterator::ITER_WORD);

	if (this->cur_pos == 0) return END;

	do {
		this->cur_pos--;
	} while (this->cur_pos > 0 && (what == ITER_WORD ? !this->str_info[this->cur_pos].word_stop : !this->str_info[this->cur_pos].char_stop));

	return this->utf16_to_utf8[this->cur_pos];
}
