/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tilearea_spiral.h Type for iterating a sprial tile area. */

#ifndef TILEAREA_SPIRAL_H
#define TILEAREA_SPIRAL_H

#include "direction_type.h"
#include "map_func.h"
#include "tile_type.h"

/**
 * Helper class for SpiralTileSequence.
 */
class SpiralTileIterator {
public:
	using value_type = TileIndex; ///< value_type iterator trait
	using difference_type = std::ptrdiff_t; ///< difference_type iterator trait
	using iterator_category = std::forward_iterator_tag; ///< iterator_category iterator trait
	using pointer = void; ///< pointer iterator trait
	using reference = void; ///< reference iterator trait

	SpiralTileIterator(TileIndex center, uint diameter);
	SpiralTileIterator(TileIndex start_north, uint radius, uint w, uint h);

	/**
	 * Compare with other iterator.
	 * @param rhs The other iterator to compare with.
	 * @return \c true iff the other iterator matches this value.
	 */
	bool operator==(const SpiralTileIterator &rhs) const
	{
		return this->x == rhs.x && this->y == rhs.y;
	}

	/**
	 * Compare with end sentinel.
	 * @return \c true iff this iterator is at the end.
	 */
	bool operator==(const std::default_sentinel_t &) const
	{
		return this->IsEnd();
	}

	/**
	 * Get the current tile.
	 * @return The current tile.
	 */
	TileIndex operator*() const
	{
		return TileXY(this->x, this->y);
	}

	/**
	 * Increment the iterator and set it to the next position.
	 * @return This iterator after incrementing.
	 */
	SpiralTileIterator &operator++()
	{
		this->Increment();
		this->SkipOutsideMap();
		return *this;
	}

private:
	/* set by constructor, const afterwards */
	const uint max_radius; ///< The maximum radius of the spiral.
	DiagDirectionIndexArray<uint> extent; ///< Size of hole in the centre of the spiral.

	/* mutable iterator state */
	uint cur_radius; ///< The current radius.
	DiagDirection dir; ///< The current spiral direction.
	uint position; ///< The current position along an edge.
	uint x; ///< The current tile X-coordinate.
	uint y; ///< The current tile Y-coordinate.

	void SkipOutsideMap();
	void InitPosition();
	void Increment();

	/**
	 * Test whether the iterator reached the end.
	 * @return \c true iff the end of the iteration is reached.
	 */
	bool IsEnd() const
	{
		return this->cur_radius == this->max_radius && this->dir != DiagDirection::Invalid;
	}
};

/**
 * Generate TileIndices around a center tile or tile area, with increasing distance.
 */
class SpiralTileSequence {
public:
	using iterator = SpiralTileIterator; ///< The tile iterator type for this tile area type.

	/**
	 * Generate TileIndices for a square area around a center tile.
	 *
	 * The size of the square is given by the length of the edge.
	 * If the size is even, the south extent will be larger than the north extent.
	 *
	 * Example for diameter=4, [ ] is the "center":
	 *        1
	 *      1   1
	 *    1  [0]  1
	 *  1   0   0   1
	 *    1   0   1
	 *      1   1
	 *        1
	 * The sequence starts with the "0" tiles, and continues with the shells around it.
	 *
	 * @param center Center of the square area.
	 * @param diameter Edge length of the square.
	 * @pre diameter > 0
	 * @note This constructor uses a "diameter", unlike the other constructor using a "radius".
	 */
	SpiralTileSequence(TileIndex center, uint diameter) : start(center, diameter) {}

	/**
	 * Generate TileIndices for a rectangular area with an optional rectangular hole in the center.
	 * The TileIndices will be sorted by increasing distance from the center (hole).
	 *
	 * Example for radius=2, w=2, h=1, [ ] is "start_north":
	 *            1
	 *          1   1
	 *        1  [0]  1
	 *      1   0   0   1
	 *    1   0   H   0   1
	 *  1   0   H   0   1
	 *    1   0   0   1
	 *      1   0   1
	 *        1   1
	 *          1
	 * The sequence starts with the "0" tiles, and continues with the shells around it.
	 *
	 * @param start_north Tile directly north from the center hole.
	 * @param radius Radial distance between outer rectangle and center hole.
	 * @param w Width of the inner rectangular hole.
	 * @param h Height of the inner rectangular hole.
	 * @pre radius > 0
	 * @note This constructor uses a "radius", unlike the other constructor using a "diameter".
	 */
	SpiralTileSequence(TileIndex start_north, uint radius, uint w, uint h) : start(start_north, radius, w, h) {}

	/**
	 * Returns an iterator to the start of the tile area.
	 * @return The SpiralTileIterator.
	 */
	SpiralTileIterator begin() const
	{
		return start;
	}

	/**
	 * Returns an iterator to the end of the tile area.
	 * @return The iterator.
	 */
	std::default_sentinel_t end() const
	{
		return std::default_sentinel_t();
	}

private:
	SpiralTileIterator start; ///< The starting iterator for thi spiral sequence.
};

#endif /* TILEAREA_SPIRAL_H */
