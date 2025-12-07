/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file gfx_layout.cpp Handling of laying out text. */

#include "stdafx.h"
#include "core/math_func.hpp"
#include "gfx_layout.h"
#include "gfx_func.h"
#include "string_func.h"
#include "strings_func.h"
#include "core/utf8.hpp"
#include "debug.h"
#include "timer/timer.h"
#include "timer/timer_window.h"
#include "viewport_func.h"
#include "window_func.h"

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
std::unique_ptr<Layouter::LineCache> Layouter::linecache;

class RuntimeMissingGlyphSearcher : public MissingGlyphSearcher {
	std::array<std::set<char32_t>, FS_END> glyphs{};
public:
	RuntimeMissingGlyphSearcher() : MissingGlyphSearcher(FONTSIZES_ALL) {}

	FontLoadReason GetLoadReason() override { return FontLoadReason::MissingFallback; }

	inline void Insert(FontSize fs, char32_t c)
	{
		this->glyphs[fs].insert(c);
		this->search_timeout.Reset();
	}

	std::set<char32_t> GetRequiredGlyphs(FontSizes fontsizes) override
	{
		std::set<char32_t> r;
		for (FontSize fs : fontsizes) {
			r.merge(this->glyphs[fs]);
		}
		return r;
	}

	TimeoutTimer<TimerWindow> search_timeout{std::chrono::milliseconds(250), [this]()
	{
		FontSizes changed_fontsizes{};
		for (FontSize fs = FS_BEGIN; fs != FS_END; ++fs) {
			auto &missing = this->glyphs[fs];
			if (missing.empty()) continue;

			if (FontProviderManager::FindFallbackFont({}, fs, this)) changed_fontsizes.Set(fs);
			missing.clear();
		}

		if (!changed_fontsizes.Any()) return;

		FontCache::LoadFontCaches(changed_fontsizes);
		LoadStringWidthTable(changed_fontsizes);
		UpdateAllVirtCoords();
		ReInitAllWindows(true);
	}};
};

static RuntimeMissingGlyphSearcher _missing_glyphs;

/**
 * Helper for getting a ParagraphLayouter of the given type.
 *
 * @note In case no ParagraphLayouter could be constructed, line.layout will be nullptr.
 * @param line The cache item to store our layouter in.
 * @param str The string to create a layouter for.
 * @param state The state of the font and colour.
 * @tparam T The type of layouter we want.
 */
template <typename T>
static inline void GetLayouter(Layouter::LineCacheItem &line, std::string_view str, FontState &state)
{
	typename T::CharType *buff_begin = new typename T::CharType[str.size() + 1];
	/* Move ownership of buff_begin into the Buffer/unique_ptr. */
	line.buffer = Layouter::LineCacheItem::Buffer(buff_begin, [](void *p) { delete[] reinterpret_cast<T::CharType *>(p); });

	const typename T::CharType *buffer_last = buff_begin + str.size() + 1;
	typename T::CharType *buff = buff_begin;
	FontMap &font_mapping = line.runs;
	Font f{state.font_index, state.cur_colour};

	font_mapping.clear();

	/*
	 * Go through the whole string while adding Font instances to the font map
	 * whenever the font changes, and convert the wide characters into a format
	 * usable by ParagraphLayout.
	 */
	Utf8View view(str);
	for (auto it = view.begin(); it != view.end(); /* nothing */) {
		auto cur = it;
		uint32_t c = *it++;
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

			if (IsTextDirectionChar(c)) {
				/* Filter out text direction characters that shouldn't be drawn, and
				 * will not be handled in the fallback case because they are mostly
				 * needed for RTL languages which need more proper shaping support. */
				if constexpr (!T::SUPPORTS_RTL) continue;

				buff += T::AppendToBuffer(buff, buffer_last, c);
				if (buff >= buffer_last) break;
				continue;
			}

			FontIndex font_index = FontCache::GetFontIndexForCharacter(state.fontsize, c);

			if (font_index == INVALID_FONT_INDEX) {
				_missing_glyphs.Insert(state.fontsize, c);
				font_index = FontCache::GetDefaultFontIndex(state.fontsize);
			}

			if (state.font_index == font_index) {
				buff += T::AppendToBuffer(buff, buffer_last, c);
				if (buff >= buffer_last) break;
				continue;
			}

			/* This character goes in the next run so don't advance. */
			state.font_index = font_index;

			it = cur;
		}

		if (buff - buff_begin > 0 && (font_mapping.empty() || font_mapping.back().first != buff - buff_begin)) {
			font_mapping.emplace_back(buff - buff_begin, f);
		}
		f = {state.font_index, state.cur_colour};
	}

	/* Better safe than sorry. */
	*buff = '\0';

	if (font_mapping.empty() || font_mapping.back().first != buff - buff_begin) {
		font_mapping.emplace_back(buff - buff_begin, f);
	}

	if constexpr (!std::is_same_v<T, FallbackParagraphLayoutFactory>) {
		/* Don't layout if all runs use a built-in font and we're not using the fallback layouter. */
		if (std::ranges::all_of(font_mapping, [](const auto &i) { return i.second.GetFontCache().IsBuiltInFont(); })) {
			return;
		}
	}

	line.layout = T::GetParagraphLayout(buff_begin, buff, font_mapping);
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
	FontState state(TC_INVALID, fontsize, FontCache::GetDefaultFontIndex(fontsize));

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
					state = std::move(old_state);
				}
			}
#endif

#ifdef WITH_UNISCRIBE
			if (line.layout == nullptr) {
				GetLayouter<UniscribeParagraphLayoutFactory>(line, str_line, state);
				if (line.layout == nullptr) {
					state = std::move(old_state);
				}
			}
#endif

#ifdef WITH_COCOA
			if (line.layout == nullptr) {
				GetLayouter<CoreTextParagraphLayoutFactory>(line, str_line, state);
				if (line.layout == nullptr) {
					state = std::move(old_state);
				}
			}
#endif

			if (line.layout == nullptr) {
				GetLayouter<FallbackParagraphLayoutFactory>(line, str_line, state);
			}
		}

		if (line.cached_width != maxw) {
			/* First run or width has changed, so we need to go through the layouter. Lines are moved into a cache to
			 * be reused if the width is not changed. */
			line.cached_layout.clear();
			line.cached_width = maxw;
			for (;;) {
				auto l = line.layout->NextLine(maxw);
				if (l == nullptr) break;
				line.cached_layout.push_back(std::move(l));
			}
		}

		/* Retrieve layout from the cache. */
		for (const auto &l : line.cached_layout) {
			this->push_back(l.get());
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
	/* All other characters defined in Unicode standard are assumed to be non-consumed. */
	return false;
}

/**
 * Get the position of a character in the layout.
 * @param ch Character to get the position of. Must be an iterator of the string passed to the constructor.
 * @return Upper left corner of the character relative to the start of the string.
 * @note Will only work right for single-line strings.
 */
ParagraphLayouter::Position Layouter::GetCharPosition(std::string_view::const_iterator ch) const
{
	const auto &line = this->front();

	/* Pointer to the end-of-string marker? Return total line width. */
	if (ch == this->string.end()) {
		Point p = {_current_text_dir == TD_LTR ? line->GetWidth() : 0, 0};
		return p;
	}

	/* Initial position, returned if character not found. */
	const ParagraphLayouter::Position initial_position = Point{_current_text_dir == TD_LTR ? 0 : line->GetWidth(), 0};

	/* Find the code point index which corresponds to the char
	 * pointer into our UTF-8 source string. */
	size_t index = 0;
	{
		Utf8View view(this->string);
		const size_t offset = ch - this->string.begin();
		const auto pos = view.GetIterAtByte(offset);

		/* We couldn't find the code point index. */
		if (pos.GetByteOffset() != offset) return initial_position;

		for (auto it = view.begin(); it < pos; ++it) {
			char32_t c = *it;
			if (!IsConsumedFormattingCode(c)) index += line->GetInternalCharLength(c);
		}
	}

	const ParagraphLayouter::Position *position = &initial_position;

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

			int begin_x = positions[i].left;
			int end_x   = positions[i].right + 1;

			if (IsInsideMM(x, begin_x, end_x)) {
				/* Found our glyph, now convert to UTF-8 string index. */
				size_t index = charmap[i];

				size_t cur_idx = 0;
				Utf8View view(this->string);
				for (auto it = view.begin(), end = view.end(); it != end; ++it) {
					if (cur_idx == index) return it.GetByteOffset();

					char32_t c = *it;
					if (!IsConsumedFormattingCode(c)) cur_idx += line->GetInternalCharLength(c);
				}
			}
		}
	}

	return -1;
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
 */
void Layouter::ResetFontCache([[maybe_unused]] FontSize size)
{
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
		linecache = std::make_unique<LineCache>(4096);
	}

	if (auto match = linecache->GetIfValid(LineCacheQuery{state, str});
		match != nullptr) {
		return *match;
	}

	/* Create missing entry */
	LineCacheKey key;
	key.state_before = state;
	key.str.assign(str);
	linecache->Insert(key, {});
	return *linecache->GetIfValid(key);
}

/**
 * Clear line cache.
 */
void Layouter::ResetLineCache()
{
	if (linecache != nullptr) linecache->Clear();
}

/**
 * Get the leading corner of a character in a single-line string relative
 * to the start of the string.
 * @param str String containing the character.
 * @param pos Index to the character in the string.
 * @param start_fontsize Font size to start the text with.
 * @return Upper left corner of the glyph associated with the character.
 */
ParagraphLayouter::Position GetCharPosInString(std::string_view str, size_t pos, FontSize start_fontsize)
{
	assert(pos <= str.size());
	auto it_ch = str.begin() + pos;

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
