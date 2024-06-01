/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file gfx_layout.cpp Handling of laying out text. */

#include "stdafx.h"
#include "core/math_func.hpp"
#include "gfx_layout.h"
#include "string_func.h"
#include "strings_func.h"
#include "debug.h"

#include "table/control_codes.h"

#include "gfx_layout_fallback.h"

#if defined(WITH_ICU_I18N) && defined(WITH_HARFBUZZ)
#include "gfx_layout_icu.h"
#endif /* WITH_ICU_I18N && WITH_HARFBUZZ */

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

/**
 * Helper for getting a ParagraphLayouter of the given type.
 *
 * @note In case no ParagraphLayouter could be constructed, line.layout will be nullptr.
 * @param line The cache item to store our layouter in.
 * @param str The string to create a layouter for.
 * @param state The state of the font and color.
 * @tparam T The type of layouter we want.
 */
template <typename T>
static inline void GetLayouter(Layouter::LineCacheItem &line, std::string_view str, FontState &state)
{
	if (line.buffer != nullptr) free(line.buffer);

	typename T::CharType *buff_begin = MallocT<typename T::CharType>(str.size() + 1);
	const typename T::CharType *buffer_last = buff_begin + str.size() + 1;
	typename T::CharType *buff = buff_begin;
	FontMap &fontMapping = line.runs;
	Font *f = Layouter::GetFont(state.fontsize, state.cur_colour);

	line.buffer = buff_begin;
	fontMapping.clear();

	auto cur = str.begin();

	/*
	 * Go through the whole string while adding Font instances to the font map
	 * whenever the font changes, and convert the wide characters into a format
	 * usable by ParagraphLayout.
	 */
	for (; buff < buffer_last && cur != str.end();) {
		char32_t c = Utf8Consume(cur);
		if (c == '\0' || c == '\n') {
			/* Caller should already have filtered out these characters. */
			NOT_REACHED();
		} else if (c >= SCC_BLUE && c <= SCC_BLACK) {
			state.SetColour((TextColour)(c - SCC_BLUE));
		} else if (c == SCC_PUSH_COLOUR) {
			state.PushColour();
		} else if (c == SCC_POP_COLOUR) {
			state.PopColour();
		} else if (c >= SCC_FIRST_FONT && c <= SCC_LAST_FONT) {
			state.SetFontSize((FontSize)(c - SCC_FIRST_FONT));
		} else {
			/* Filter out non printable characters */
			if (!IsPrintable(c)) continue;
			/* Filter out text direction characters that shouldn't be drawn, and
			 * will not be handled in the fallback case because they are mostly
			 * needed for RTL languages which need more proper shaping support. */
			if (!T::SUPPORTS_RTL && IsTextDirectionChar(c)) continue;
			buff += T::AppendToBuffer(buff, buffer_last, c);
			continue;
		}

		if (fontMapping.count(buff - buff_begin) == 0) {
			fontMapping[buff - buff_begin] = f;
		}
		f = Layouter::GetFont(state.fontsize, state.cur_colour);
	}

	/* Better safe than sorry. */
	*buff = '\0';

	if (fontMapping.count(buff - buff_begin) == 0) {
		fontMapping[buff - buff_begin] = f;
	}
	line.layout = T::GetParagraphLayout(buff_begin, buff, fontMapping);
	line.state_after = state;
}

/**
 * Create a new layouter.
 * @param str      The string to create the layout for.
 * @param maxw     The maximum width.
 * @param fontsize The size of font to use.
 */
Layouter::Layouter(std::string_view str, int maxw, FontSize fontsize) : string(str)
{
	FontState state(TC_INVALID, fontsize);

	while (true) {
		auto line_length = str.find_first_of('\n');
		auto str_line = str.substr(0, line_length);

		LineCacheItem &line = GetCachedParagraphLayout(str_line, state);
		if (line.layout != nullptr) {
			state = line.state_after;
			line.layout->Reflow();
		} else {
			/* Line is new, layout it */
			FontState old_state = state;

#if defined(WITH_ICU_I18N) && defined(WITH_HARFBUZZ)
			if (line.layout == nullptr) {
				GetLayouter<ICUParagraphLayoutFactory>(line, str_line, state);
				if (line.layout == nullptr) {
					state = old_state;
				}
			}
#endif

#ifdef WITH_UNISCRIBE
			if (line.layout == nullptr) {
				GetLayouter<UniscribeParagraphLayoutFactory>(line, str_line, state);
				if (line.layout == nullptr) {
					state = old_state;
				}
			}
#endif

#ifdef WITH_COCOA
			if (line.layout == nullptr) {
				GetLayouter<CoreTextParagraphLayoutFactory>(line, str_line, state);
				if (line.layout == nullptr) {
					state = old_state;
				}
			}
#endif

			if (line.layout == nullptr) {
				GetLayouter<FallbackParagraphLayoutFactory>(line, str_line, state);
			}
		}

		/* Move all lines into a local cache so we can reuse them later on more easily. */
		for (;;) {
			auto l = line.layout->NextLine(maxw);
			if (l == nullptr) break;
			this->push_back(std::move(l));
		}

		/* Break out if this was the last line. */
		if (line_length == std::string_view::npos) {
			break;
		}

		/* Go to the next line. */
		str.remove_prefix(line_length + 1);
	}
}

/**
 * Get the boundaries of this paragraph.
 * @return The boundaries.
 */
Dimension Layouter::GetBounds()
{
	Dimension d = { 0, 0 };
	for (const auto &l : *this) {
		d.width = std::max<uint>(d.width, l->GetWidth());
		d.height += l->GetLeading();
	}
	return d;
}

/**
 * Test whether a character is a non-printable formatting code
 */
static bool IsConsumedFormattingCode(char32_t ch)
{
	if (ch >= SCC_BLUE && ch <= SCC_BLACK) return true;
	if (ch == SCC_PUSH_COLOUR) return true;
	if (ch == SCC_POP_COLOUR) return true;
	if (ch >= SCC_FIRST_FONT && ch <= SCC_LAST_FONT) return true;
	// All other characters defined in Unicode standard are assumed to be non-consumed.
	return false;
}

/**
 * Get the position of a character in the layout.
 * @param ch Character to get the position of. Must be an iterator of the string passed to the constructor.
 * @return Upper left corner of the character relative to the start of the string.
 * @note Will only work right for single-line strings.
 */
Point Layouter::GetCharPosition(std::string_view::const_iterator ch) const
{
	const auto &line = this->front();

	/* Pointer to the end-of-string marker? Return total line width. */
	if (ch == this->string.end()) {
		Point p = {_current_text_dir == TD_LTR ? line->GetWidth() : 0, 0};
		return p;
	}

	/* Find the code point index which corresponds to the char
	 * pointer into our UTF-8 source string. */
	size_t index = 0;
	auto str = this->string.begin();
	while (str < ch) {
		char32_t c = Utf8Consume(str);
		if (!IsConsumedFormattingCode(c)) index += line->GetInternalCharLength(c);
	}

	/* Initial position, returned if character not found. */
	const Point initial_position = {_current_text_dir == TD_LTR ? 0 : line->GetWidth(), 0};
	const Point *position = &initial_position;

	/* We couldn't find the code point index. */
	if (str != ch) return *position;

	/* Valid character. */

	/* Scan all runs until we've found our code point index. */
	size_t best_index = SIZE_MAX;
	for (int run_index = 0; run_index < line->CountRuns(); run_index++) {
		const ParagraphLayouter::VisualRun &run = line->GetVisualRun(run_index);
		const auto &positions = run.GetPositions();
		const auto &charmap = run.GetGlyphToCharMap();

		auto itp = positions.begin();
		for (auto it = charmap.begin(); it != charmap.end(); ++it, ++itp) {
			const size_t cur_index = static_cast<size_t>(*it);
			/* Found exact character match? */
			if (cur_index == index) return *itp;

			/* If the character we are looking for has been combined with other characters to form a ligature then
			 * we may not be able to find an exact match. We don't actually know if our character is part of a
			 * ligature. In this case we will aim to select the first character of the ligature instead, so the best
			 * index is the index nearest to but lower than the desired index. */
			if (cur_index < index && (best_index < cur_index || best_index == SIZE_MAX)) {
				best_index = cur_index;
				position = &*itp;
			}
		}
	}

	/* At the end of the run but still didn't find our character so probably a trailing ligature, use the last found position. */
	return *position;
}

/**
 * Get the character that is at a pixel position in the first line of the layouted text.
 * @param x Position in the string.
 * @param line_index Which line of the layout to search
 * @return String offset of the position (bytes) or -1 if no character is at the position.
 */
ptrdiff_t Layouter::GetCharAtPosition(int x, size_t line_index) const
{
	if (line_index >= this->size()) return -1;

	const auto &line = this->at(line_index);

	for (int run_index = 0; run_index < line->CountRuns(); run_index++) {
		const ParagraphLayouter::VisualRun &run = line->GetVisualRun(run_index);
		const auto &glyphs = run.GetGlyphs();
		const auto &positions = run.GetPositions();
		const auto &charmap = run.GetGlyphToCharMap();

		for (int i = 0; i < run.GetGlyphCount(); i++) {
			/* Not a valid glyph (empty). */
			if (glyphs[i] == 0xFFFF) continue;

			int begin_x = positions[i].x;
			int end_x   = positions[i + 1].x;

			if (IsInsideMM(x, begin_x, end_x)) {
				/* Found our glyph, now convert to UTF-8 string index. */
				size_t index = charmap[i];

				size_t cur_idx = 0;
				for (auto str = this->string.begin(); str != this->string.end();) {
					if (cur_idx == index) return str - this->string.begin();

					char32_t c = Utf8Consume(str);
					if (!IsConsumedFormattingCode(c)) cur_idx += line->GetInternalCharLength(c);
				}
			}
		}
	}

	return -1;
}

/**
 * Get a static font instance.
 */
Font *Layouter::GetFont(FontSize size, TextColour colour)
{
	FontColourMap::iterator it = fonts[size].find(colour);
	if (it != fonts[size].end()) return it->second.get();

	fonts[size][colour] = std::make_unique<Font>(size, colour);
	return fonts[size][colour].get();
}

/**
 * Perform initialization of layout engine.
 */
void Layouter::Initialize()
{
#if defined(WITH_ICU_I18N) && defined(WITH_HARFBUZZ)
	ICUParagraphLayoutFactory::InitializeLayouter();
#endif /* WITH_ICU_I18N && WITH_HARFBUZZ */
}

/**
 * Reset cached font information.
 * @param size Font size to reset.
 */
void Layouter::ResetFontCache(FontSize size)
{
	fonts[size].clear();

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
 * @param state State of the font at the beginning of the line.
 * @return Reference to cache item.
 */
Layouter::LineCacheItem &Layouter::GetCachedParagraphLayout(std::string_view str, const FontState &state)
{
	if (linecache == nullptr) {
		/* Create linecache on first access to avoid trouble with initialisation order of static variables. */
		linecache = new LineCache();
	}

	if (auto match = linecache->find(LineCacheQuery{state, str});
		match != linecache->end()) {
		return match->second;
	}

	/* Create missing entry */
	LineCacheKey key;
	key.state_before = state;
	key.str.assign(str);
	return (*linecache)[std::move(key)];
}

/**
 * Clear line cache.
 */
void Layouter::ResetLineCache()
{
	if (linecache != nullptr) linecache->clear();
}

/**
 * Reduce the size of linecache if necessary to prevent infinite growth.
 */
void Layouter::ReduceLineCache()
{
	if (linecache != nullptr) {
		/* TODO LRU cache would be fancy, but not exactly necessary */
		if (linecache->size() > 4096) ResetLineCache();
	}
}

/**
 * Get the leading corner of a character in a single-line string relative
 * to the start of the string.
 * @param str String containing the character.
 * @param ch Pointer to the character in the string.
 * @param start_fontsize Font size to start the text with.
 * @return Upper left corner of the glyph associated with the character.
 */
Point GetCharPosInString(std::string_view str, const char *ch, FontSize start_fontsize)
{
	/* Ensure "ch" is inside "str" or at the exact end. */
	assert(ch >= str.data() && (ch - str.data()) <= static_cast<ptrdiff_t>(str.size()));
	auto it_ch = str.begin() + (ch - str.data());

	Layouter layout(str, INT32_MAX, start_fontsize);
	return layout.GetCharPosition(it_ch);
}

/**
 * Get the character from a string that is drawn at a specific position.
 * @param str String to test.
 * @param x Position relative to the start of the string.
 * @param start_fontsize Font size to start the text with.
 * @return Index of the character position or -1 if there is no character at the position.
 */
ptrdiff_t GetCharAtPosition(std::string_view str, int x, FontSize start_fontsize)
{
	if (x < 0) return -1;

	Layouter layout(str, INT32_MAX, start_fontsize);
	return layout.GetCharAtPosition(x, 0);
}
