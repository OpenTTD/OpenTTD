/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tilearea.cpp Handling of tile areas. */

#include "stdafx.h"

#include "tilearea_type.h"

#include "safeguards.h"

/**
 * Construct this tile area based on two points.
 * @param start the start of the area
 * @param end   the end of the area
 */
OrthogonalTileArea::OrthogonalTileArea(TileIndex start, TileIndex end)
{
	assert(start < MapSize());
	assert(end < MapSize());

	uint sx = TileX(start);
	uint sy = TileY(start);
	uint ex = TileX(end);
	uint ey = TileY(end);

	if (sx > ex) Swap(sx, ex);
	if (sy > ey) Swap(sy, ey);

	this->tile = TileXY(sx, sy);
	this->w    = ex - sx + 1;
	this->h    = ey - sy + 1;
}

/**
 * Add a single tile to a tile area; enlarge if needed.
 * @param to_add The tile to add
 */
void OrthogonalTileArea::Add(TileIndex to_add)
{
	if (this->tile == INVALID_TILE) {
		this->tile = to_add;
		this->w = 1;
		this->h = 1;
		return;
	}

	uint sx = TileX(this->tile);
	uint sy = TileY(this->tile);
	uint ex = sx + this->w - 1;
	uint ey = sy + this->h - 1;

	uint ax = TileX(to_add);
	uint ay = TileY(to_add);

	sx = std::min(ax, sx);
	sy = std::min(ay, sy);
	ex = std::max(ax, ex);
	ey = std::max(ay, ey);

	this->tile = TileXY(sx, sy);
	this->w    = ex - sx + 1;
	this->h    = ey - sy + 1;
}

/**
 * Does this tile area intersect with another?
 * @param ta the other tile area to check against.
 * @return true if they intersect.
 */
bool OrthogonalTileArea::Intersects(const OrthogonalTileArea &ta) const
{
	if (ta.w == 0 || this->w == 0) return false;

	assert(ta.w != 0 && ta.h != 0 && this->w != 0 && this->h != 0);

	uint left1   = TileX(this->tile);
	uint top1    = TileY(this->tile);
	uint right1  = left1 + this->w - 1;
	uint bottom1 = top1  + this->h - 1;

	uint left2   = TileX(ta.tile);
	uint top2    = TileY(ta.tile);
	uint right2  = left2 + ta.w - 1;
	uint bottom2 = top2  + ta.h - 1;

	return !(
			left2   > right1  ||
			right2  < left1   ||
			top2    > bottom1 ||
			bottom2 < top1
		);
}

/**
 * Does this tile area contain a tile?
 * @param tile Tile to test for.
 * @return True if the tile is inside the area.
 */
bool OrthogonalTileArea::Contains(TileIndex tile) const
{
	if (this->w == 0) return false;

	assert(this->w != 0 && this->h != 0);

	uint left   = TileX(this->tile);
	uint top    = TileY(this->tile);
	uint tile_x = TileX(tile);
	uint tile_y = TileY(tile);

	return IsInsideBS(tile_x, left, this->w) && IsInsideBS(tile_y, top, this->h);
}

/**
 * Expand a tile area by rad tiles in each direction, keeping within map bounds.
 * @param rad Number of tiles to expand
 * @return The OrthogonalTileArea.
 */
OrthogonalTileArea &OrthogonalTileArea::Expand(int rad)
{
	int x = TileX(this->tile);
	int y = TileY(this->tile);

	int sx = std::max<int>(x - rad, 0);
	int sy = std::max<int>(y - rad, 0);
	int ex = std::min<int>(x + this->w + rad, MapSizeX());
	int ey = std::min<int>(y + this->h + rad, MapSizeY());

	this->tile = TileXY(sx, sy);
	this->w    = ex - sx;
	this->h    = ey - sy;
	return *this;
}

/**
 * Clamp the tile area to map borders.
 */
void OrthogonalTileArea::ClampToMap()
{
	assert(this->tile < MapSize());
	this->w = std::min<int>(this->w, MapSizeX() - TileX(this->tile));
	this->h = std::min<int>(this->h, MapSizeY() - TileY(this->tile));
}

/**
 * Create a diagonal tile area from two corners.
 * @param start First corner of the area.
 * @param end Second corner of the area.
 */
DiagonalTileArea::DiagonalTileArea(TileIndex start, TileIndex end) : tile(start)
{
	assert(start < MapSize());
	assert(end < MapSize());

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

	return (a >= start_a && a < end_a && b >= start_b && b < end_b);
}

/**
 * Move ourselves to the next tile in the rectangle on the map.
 */
TileIterator &DiagonalTileIterator::operator++()
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
		this->tile = x >= MapSizeX() || y >= MapSizeY() ? INVALID_TILE : TileXY(x, y);
	} while (this->tile > MapSize() && this->b_max != this->b_cur);

	if (this->b_max == this->b_cur) this->tile = INVALID_TILE;
	return *this;
}
