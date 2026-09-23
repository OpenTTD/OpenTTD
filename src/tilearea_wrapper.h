/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tilearea_wrapper.h Tile area and iterator wrappers to allow mixing different tile area types. */

#ifndef TILEAREA_WRAPPER_H
#define TILEAREA_WRAPPER_H

#include "tile_type.h"

/**
 * A type is considered a TileIterator when it has a dereference operator that returns as TileIndex.
 * @tparam T The type under consideration.
 */
template <typename T>
concept TileIterator = requires(const T iter) {
	{ *iter } -> std::same_as<TileIndex>;
};

/**
 * Tile iterator wrapper.
 * @tparam Titer Tile iterator types this wrapper supports.
 */
template <TileIterator... Titer>
class TileIteratorWrapper {
public:
	using value_type = TileIndex; ///< value_type iterator trait
	using difference_type = std::ptrdiff_t; ///< difference_type iterator trait
	using iterator_category = std::forward_iterator_tag; ///< iterator_category iterator trait
	using pointer = void; ///< pointer iterator trait
	using reference = void; ///< reference iterator trait

	/**
	 * Construct the iterator wrapper.
	 * @param iter The underlying iterator.
	 */
	TileIteratorWrapper(const TileIterator auto &iter) : iter(iter) {}

	/**
	 * Construct the iterator wrapper.
	 * @param iter The underlying iterator.
	 */
	TileIteratorWrapper(TileIterator auto &&iter) : iter(std::move(iter)) {}

	/**
	 * Compare with other iterator.
	 * @param other The other iterator to compare with.
	 * @return \c true iff the other iterator matches this value.
	 */
	bool operator==(const TileIteratorWrapper &other) const
	{
		return this->iter == other.iter;
	}

	/**
	 * Compare with end sentinel.
	 * @return \c true iff this iterator is at the end.
	 */
	bool operator==(const std::default_sentinel_t &) const
	{
		return **this == INVALID_TILE;
	}

	/**
	 * Get the current tile.
	 * @return The current tile, or INVALID_TILE when complete.
	 */
	inline TileIndex operator*() const
	{
		return std::visit(dereference_visitor{}, this->iter);
	}

	/**
	 * Increment the iterator and set it to the next position.
	 * @return This iterator after incrementing.
	 */
	inline TileIteratorWrapper &operator++()
	{
		std::visit(increment_visitor{}, this->iter);
		return *this;
	}

private:
	std::variant<Titer...> iter; ///< The underlying iterator.

	/** Visitor to call the underlying tile iterator's dereference operator. */
	struct dereference_visitor {
		/**
		 * Call the underlying iterator's dereference operator.
		 * @param iter The underlying iterator.
		 * @return The tile of the underlying iterator.
		 */
		TileIndex operator()(const auto &iter)
		{
			return *iter;
		}
	};

	/** Visitor to call the underlying tile iterator's increment operator. */
	struct increment_visitor {
		/**
		 * Call the underlying iterator's increment operator.
		 * @param iter The underlying iterator.
		 */
		void operator()(auto &iter)
		{
			++iter;
		}
	};
};

/**
 * Tile area wrapper.
 * @tparam Titer Tile iterator types this wrapper supports.
 * @tparam Tarea Tile area types this wrapper supports.
 */
template <TileIterator Titer, typename... Tarea>
class TileAreaWrapper {
public:
	using iterator = Titer; ///< The iterator type.

	/**
	 * Construct a new tile area wrapper.
	 * @param area The underlying tile area.
	 */
	template <typename T>
	TileAreaWrapper(const T &area) : area(area)
	{
	}

	/**
	 * Construct a new tile area wrapper.
	 * @param area The underlying tile area.
	 */
	template <typename T>
	TileAreaWrapper(T &&area) : area(std::move(area))
	{
	}

	/**
	 * Returns an iterator to the start of the tile area.
	 * @return The TileAreaWrapper::iterator.
	 */
	inline iterator begin() const
	{
		return std::visit(begin_visitor{}, this->area);
	}

	/**
	 * Returns an iterator to the end of the tile area.
	 * @return The iterator.
	 */
	inline std::default_sentinel_t end() const
	{
		return std::default_sentinel_t();
	}

	/**
	 * Test if a tile is part of the tile area.
	 * @param tile Tile to check.
	 * @return \c true iff the tile is in this area.
	 */
	inline bool Contains(TileIndex tile) const
	{
		return std::visit(contains_visitor{tile}, this->area);
	}

private:
	std::variant<Tarea...> area; ///< The underlying tile area.

	/** Visitor to call the underlying tile area's begin method. */
	struct begin_visitor {
		/**
		 * Get the iterator for the underlying tile area.
		 * @param area The underlying tile area.
		 * @return Iterator for the underlying tile area.
		 */
		iterator operator()(const auto &area)
		{
			return iterator{area.begin()};
		};
	};

	/** Visitor to call the underlying tile area's contains method. */
	struct contains_visitor {
		TileIndex tile; ///< The tile to test.

		/**
		 * Test if the underlying tile area contains a tile.
		 * @param area The underlying tile area.
		 * @return \c true iff the tile area contains the tile.
		 */
		bool operator()(const auto &area)
		{
			return area.Contains(this->tile);
		}
	};
};

#endif /* TILEAREA_WRAPPER_H */
