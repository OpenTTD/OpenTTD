/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file gfx_layout.h Functions related to laying out the texts. */

#ifndef GFX_LAYOUT_H
#define GFX_LAYOUT_H

#include "misc/lrucache.hpp"
#include "fontcache.h"

#include <string_view>

/**
 * Text drawing parameters, which can change while drawing a line, but are kept between multiple parts
 * of the same text, e.g. on line breaks.
 */
struct FontState {
	FontSize fontsize; ///< Current font size.
	FontIndex font_index; ///< Current font index.
	TextColour cur_colour; ///< Current text colour.
	std::vector<TextColour> colour_stack; ///< Stack of colours to assist with colour switching.

	FontState() : fontsize(FS_END), font_index(INVALID_FONT_INDEX), cur_colour(TC_INVALID) {}
	FontState(TextColour colour, FontSize fontsize, FontIndex font_index) : fontsize(fontsize), font_index(font_index), cur_colour(colour) {}

	auto operator<=>(const FontState &) const = default;

	/**
	 * Switch to new colour \a c.
	 * @param c New colour to use.
	 */
	inline void SetColour(TextColour c)
	{
		assert(((c & TC_COLOUR_MASK) >= TC_BLUE && (c & TC_COLOUR_MASK) <= TC_BLACK) || (c & TC_COLOUR_MASK) == TC_INVALID);
		assert((c & (TC_COLOUR_MASK | TC_FLAGS_MASK)) == c);
		if ((this->cur_colour & TC_FORCED) == 0) this->cur_colour = c;
	}

	/**
	 * Switch to and pop the last saved colour on the stack.
	 */
	inline void PopColour()
	{
		if (colour_stack.empty()) return;
		SetColour(colour_stack.back());
		colour_stack.pop_back();
	}

	/**
	 * Push the current colour on to the stack.
	 */
	inline void PushColour()
	{
		colour_stack.push_back(this->cur_colour);
	}

	/**
	 * Switch to using a new font \a f.
	 * @param f New font to use.
	 */
	inline void SetFontSize(FontSize f)
	{
		this->fontsize = f;
		this->font_index = FontCache::GetDefaultFontIndex(this->fontsize);
	}
};

template <typename T> struct std::hash<std::vector<T>> {
	size_t operator()(const std::vector<T> &vec) const
	{
		/* This is not an optimal hash algorithm but in most cases this is empty and therefore the same anyway. */
		return std::transform_reduce(std::begin(vec), std::end(vec),
			std::hash<size_t>{}(std::size(vec)),
			[](const size_t &a, const size_t &b) -> size_t { return a ^ b; },
			[](const T &x) -> size_t { return std::hash<T>{}(x); });
	}
};

template <> struct std::hash<FontState> {
	std::size_t operator()(const FontState &state) const noexcept
	{
		size_t h1 = std::hash<FontSize>{}(state.fontsize);
		size_t h2 = std::hash<FontIndex>{}(state.font_index);
		size_t h3 = std::hash<TextColour>{}(state.cur_colour);
		size_t h4 = std::hash<std::vector<TextColour>>{}(state.colour_stack);
		return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
	}
};

/**
 * Container with information about a font.
 */
class Font {
public:
	FontIndex font_index = INVALID_FONT_INDEX; ///< The font we are using.
	TextColour colour = TC_INVALID; ///< The colour this font has to be.

	inline FontCache &GetFontCache() const { return *FontCache::Get(this->font_index); }
};

/** Mapping from index to font. The pointer is owned by FontColourMap. */
using FontMap = std::vector<std::pair<int, Font>>;

/**
 * Interface to glue fallback and normal layouter into one.
 */
class ParagraphLayouter {
public:
	virtual ~ParagraphLayouter() = default;

	/** Position of a glyph within a VisualRun. */
	class Position {
	public:
		int16_t left; ///< Left-most position of glyph.
		int16_t right; ///< Right-most position of glyph.
		int16_t top; ///< Top-most position of glyph.

		constexpr inline Position(int16_t left, int16_t right, int16_t top) : left(left), right(right), top(top) { }

		/** Conversion from a single point to a Position. */
		constexpr inline Position(const Point &pt) : left(pt.x), right(pt.x), top(pt.y) { }
	};

	/** Visual run contains data about the bit of text with the same font. */
	class VisualRun {
	public:
		virtual ~VisualRun() = default;
		virtual const Font &GetFont() const = 0;
		virtual int GetGlyphCount() const = 0;
		virtual std::span<const GlyphID> GetGlyphs() const = 0;
		virtual std::span<const Position> GetPositions() const = 0;
		virtual int GetLeading() const = 0;
		virtual std::span<const int> GetGlyphToCharMap() const = 0;
	};

	/** A single line worth of VisualRuns. */
	class Line {
	public:
		virtual ~Line() = default;
		virtual int GetLeading() const = 0;
		virtual int GetWidth() const = 0;
		virtual int CountRuns() const = 0;
		virtual const VisualRun &GetVisualRun(int run) const = 0;
		virtual int GetInternalCharLength(char32_t c) const = 0;
	};

	virtual void Reflow() = 0;
	virtual std::unique_ptr<const Line> NextLine(int max_width) = 0;
};

/**
 * The layouter performs all the layout work.
 *
 * It also accounts for the memory allocations and frees.
 */
class Layouter : public std::vector<const ParagraphLayouter::Line *> {
	std::string_view string; ///< Pointer to the original string.

	/** Key into the linecache */
	struct LineCacheKey {
		FontState state_before;  ///< Font state at the beginning of the line.
		std::string str;         ///< Source string of the line (including colour and font size codes).
	};

	struct LineCacheQuery {
		const FontState &state_before; ///< Font state at the beginning of the line.
		std::string_view str;    ///< Source string of the line (including colour and font size codes).
	};

	friend struct std::hash<Layouter::LineCacheQuery>;
	struct LineCacheHash;

	struct LineCacheEqualTo {
		using is_transparent = void;

		template <typename Tlhs, typename Trhs>
		bool operator()(const Tlhs &lhs, const Trhs &rhs) const
		{
			return lhs.state_before == rhs.state_before && lhs.str == rhs.str;
		}
	};

public:
	/** Item in the linecache */
	struct LineCacheItem {
		/* Due to the type of data in the buffer differing depending on the Layouter, we need to pass our own deleter routine. */
		using Buffer = std::unique_ptr<void, void(*)(void *)>;
		/* Stuff that cannot be freed until the ParagraphLayout is freed */
		Buffer buffer{nullptr, [](void *){}}; ///< Accessed by our ParagraphLayout::nextLine.
		FontMap runs;              ///< Accessed by our ParagraphLayout::nextLine.

		FontState state_after;     ///< Font state after the line.
		std::unique_ptr<ParagraphLayouter> layout = nullptr; ///< Layout of the line.

		std::vector<std::unique_ptr<const ParagraphLayouter::Line>> cached_layout{}; ///< Cached results of line layouting.
		int cached_width = 0; ///< Width used for the cached layout.
	};
private:
	using LineCache = LRUCache<LineCacheKey, LineCacheItem, LineCacheHash, LineCacheEqualTo>;
	static std::unique_ptr<LineCache> linecache;

	static LineCacheItem &GetCachedParagraphLayout(std::string_view str, const FontState &state);

public:
	Layouter(std::string_view str, int maxw = INT32_MAX, FontSize fontsize = FS_NORMAL);
	Dimension GetBounds();
	ParagraphLayouter::Position GetCharPosition(std::string_view::const_iterator ch) const;
	ptrdiff_t GetCharAtPosition(int x, size_t line_index) const;

	static void Initialize();
	static void ResetFontCache(FontSize fs);
	static void ResetLineCache();
};

ParagraphLayouter::Position GetCharPosInString(std::string_view str, size_t pos, FontSize start_fontsize = FS_NORMAL);
ptrdiff_t GetCharAtPosition(std::string_view str, int x, FontSize start_fontsize = FS_NORMAL);

template <> struct std::hash<Layouter::LineCacheQuery> {
	std::size_t operator()(const Layouter::LineCacheQuery &state) const noexcept
	{
		size_t h1 = std::hash<std::string_view>{}(state.str);
		size_t h2 = std::hash<FontState>{}(state.state_before);
		return h1 ^ (h2 << 1);
	}
};

struct Layouter::LineCacheHash {
	using is_transparent = void;

	std::size_t operator()(const Layouter::LineCacheKey &query) const { return std::hash<Layouter::LineCacheQuery>{}(LineCacheQuery{query.state_before, query.str}); }
	std::size_t operator()(const Layouter::LineCacheQuery &query) const { return std::hash<Layouter::LineCacheQuery>{}(query); }
};

#endif /* GFX_LAYOUT_H */
