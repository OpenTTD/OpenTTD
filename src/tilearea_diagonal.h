/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tilearea_diagonal.h Type for storing and iterating a diagonal tile area. */

#ifndef TILEAREA_DIAGONAL_H
#define TILEAREA_DIAGONAL_H

#include "map_func.h"

/** Represents a diagonal tile area. */
class DiagonalTileArea {
public:
	TileIndex tile; ///< Base tile of the area
	int16_t a; ///< Extent in diagonal "x" direction (may be negative to signify the area stretches to the left)
	int16_t b; ///< Extent in diagonal "y" direction (may be negative to signify the area stretches upwards)

	/**
	 * Construct this tile area with some set values.
	 * @param tile The base tile.
	 * @param a The "x" extent.
	 * @param b The "y" extent.
	 */
	DiagonalTileArea(TileIndex tile = INVALID_TILE, int16_t a = 0, int16_t b = 0) : tile(tile), a(a), b(b) {}

	DiagonalTileArea(TileIndex start, TileIndex end);

	/**
	 * Clears the TileArea by making the tile invalid and setting a and b to 0.
	 */
	void Clear()
	{
		this->tile = INVALID_TILE;
		this->a = 0;
		this->b = 0;
	}

	bool Contains(TileIndex tile) const;

	/** Iterator to iterate over a diagonal area of the map. */
	class iterator {
	private:
		TileIndex tile; ///< The current tile.
		uint base_x; ///< The base tile x coordinate from where the iterating happens.
		uint base_y; ///< The base tile y coordinate from where the iterating happens.
		int a_cur; ///< The current (rotated) x coordinate of the iteration.
		int b_cur; ///< The current (rotated) y coordinate of the iteration.
		int a_max; ///< The (rotated) x coordinate of the end of the iteration.
		int b_max; ///< The (rotated) y coordinate of the end of the iteration.

	public:
		using value_type = TileIndex; ///< value_type iterator trait
		using difference_type = std::ptrdiff_t; ///< difference_type iterator trait
		using iterator_category = std::forward_iterator_tag; ///< iterator_category iterator trait
		using pointer = void; ///< pointer iterator trait
		using reference = void; ///< reference iterator trait

		/**
		 * Construct the iterator.
		 * @param ta Area, i.e. begin point and (diagonal) width/height of to-be-iterated area.
		 */
		explicit iterator(const DiagonalTileArea &ta) : tile(ta.tile), base_x(TileX(ta.tile)), base_y(TileY(ta.tile)), a_cur(0), b_cur(0), a_max(ta.a), b_max(ta.b) {}

		/**
		 * Compare with other iterator.
		 * @return \c true iff the other iterator matches this value.
		 */
		bool operator==(const iterator &) const = default;

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
		TileIndex operator*() const
		{
			return this->tile;
		}

		iterator &operator++();
	};

	/**
	 * Returns an iterator to the start of the tile area.
	 * @return The DiagonalTileIterator.
	 */
	inline iterator begin() const
	{
		return iterator{*this};
	}

	/**
	 * Returns an iterator to the end of the tile area.
	 * @return The iterator.
	 */
	inline std::default_sentinel_t end() const
	{
		return std::default_sentinel_t();
	}
};

#endif /* TILEAREA_DIAGONAL_H */
