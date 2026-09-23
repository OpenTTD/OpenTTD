/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tilearea_diagonal.cpp Handling of diagonal tile areas. */

#include "stdafx.h"

#include "tilearea_diagonal.h"

#include "safeguards.h"

/**
 * Create a diagonal tile area from two corners.
 * @param start First corner of the area.
 * @param end Second corner of the area.
 */
DiagonalTileArea::DiagonalTileArea(TileIndex start, TileIndex end) : tile(start)
{
	assert(start < Map::Size());
	assert(end < Map::Size());

	/* Unfortunately we can't find a new base and make all a and b positive because
	 * the new base might be a "flattened" corner where there actually is no single
	 * tile. If we try anyway the result is either inaccurate ("one off" half of the
	 * time) or the code gets much more complex;
	 *
	 * We also need to increment/decrement a and b here to have one-past-end semantics
	 * for a and b, just the way the orthogonal tile area does it for w and h. */

	this->a = TileY(end) + TileX(end) - TileY(start) - TileX(start);
	this->b = TileY(end) - TileX(end) - TileY(start) + TileX(start);
	if (this->a > 0) {
		this->a++;
	} else {
		this->a--;
	}

	if (this->b > 0) {
		this->b++;
	} else {
		this->b--;
	}
}

/**
 * Does this tile area contain a tile?
 * @param tile Tile to test for.
 * @return True if the tile is inside the area.
 */
bool DiagonalTileArea::Contains(TileIndex tile) const
{
	int a = TileY(tile) + TileX(tile);
	int b = TileY(tile) - TileX(tile);

	int start_a = TileY(this->tile) + TileX(this->tile);
	int start_b = TileY(this->tile) - TileX(this->tile);

	int end_a = start_a + this->a;
	int end_b = start_b + this->b;

	/* Swap if necessary, preserving the "one past end" semantics. */
	if (start_a > end_a) {
		int tmp = start_a;
		start_a = end_a + 1;
		end_a = tmp + 1;
	}
	if (start_b > end_b) {
		int tmp = start_b;
		start_b = end_b + 1;
		end_b = tmp + 1;
	}

	return a >= start_a && a < end_a && b >= start_b && b < end_b;
}

/**
 * Increment the iterator and set it to the next position.
 * @return This iterator after incrementing.
 */
DiagonalTileArea::iterator &DiagonalTileArea::iterator::operator++()
{
	assert(this->tile != INVALID_TILE);

	/* Determine the next tile, while clipping at map borders */
	bool new_line = false;
	do {
		/* Iterate using the rotated coordinates. */
		if (this->a_max == 1 || this->a_max == -1) {
			/* Special case: Every second column has zero length, skip them completely */
			this->a_cur = 0;
			if (this->b_max > 0) {
				this->b_cur = std::min(this->b_cur + 2, this->b_max);
			} else {
				this->b_cur = std::max(this->b_cur - 2, this->b_max);
			}
		} else {
			/* Every column has at least one tile to process */
			if (this->a_max > 0) {
				this->a_cur += 2;
				new_line = this->a_cur >= this->a_max;
			} else {
				this->a_cur -= 2;
				new_line = this->a_cur <= this->a_max;
			}
			if (new_line) {
				/* offset of initial a_cur: one tile in the same direction as a_max
				 * every second line.
				 */
				this->a_cur = abs(this->a_cur) % 2 ? 0 : (this->a_max > 0 ? 1 : -1);

				if (this->b_max > 0) {
					++this->b_cur;
				} else {
					--this->b_cur;
				}
			}
		}

		/* And convert the coordinates back once we've gone to the next tile. */
		uint x = this->base_x + (this->a_cur - this->b_cur) / 2;
		uint y = this->base_y + (this->b_cur + this->a_cur) / 2;
		/* Prevent wrapping around the map's borders. */
		this->tile = x >= Map::SizeX() || y >= Map::SizeY() ? INVALID_TILE : TileXY(x, y);
	} while (this->tile > Map::Size() && this->b_max != this->b_cur);

	if (this->b_max == this->b_cur) this->tile = INVALID_TILE;
	return *this;
}
