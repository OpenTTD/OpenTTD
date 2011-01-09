/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tilearea.cpp Handling of tile areas. */

#include "stdafx.h"

#include "map_func.h"
#include "tilearea_type.h"

/**
 * Construct this tile area based on two points.
 * @param start the start of the area
 * @param end   the end of the area
 */
TileArea::TileArea(TileIndex start, TileIndex end)
{
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
void TileArea::Add(TileIndex to_add)
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

	sx = min(ax, sx);
	sy = min(ay, sy);
	ex = max(ax, ex);
	ey = max(ay, ey);

	this->tile = TileXY(sx, sy);
	this->w    = ex - sx + 1;
	this->h    = ey - sy + 1;
}

/**
 * Does this tile area intersect with another?
 * @param ta the other tile area to check against.
 * @return true if they intersect.
 */
bool TileArea::Intersects(const TileArea &ta) const
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
 * Clamp the tile area to map borders.
 */
void TileArea::ClampToMap()
{
	assert(this->tile < MapSize());
	this->w = min(this->w, MapSizeX() - TileX(this->tile));
	this->h = min(this->h, MapSizeY() - TileY(this->tile));
}

DiagonalTileIterator::DiagonalTileIterator(TileIndex corner1, TileIndex corner2) : TileIterator(corner2), base_x(TileX(corner2)), base_y(TileY(corner2)), a_cur(0), b_cur(0)
{
	assert(corner1 < MapSize());
	assert(corner2 < MapSize());

	int dist_x = TileX(corner1) - TileX(corner2);
	int dist_y = TileY(corner1) - TileY(corner2);
	this->a_max = dist_x + dist_y;
	this->b_max = dist_y - dist_x;

	/* Unfortunately we can't find a new base and make all a and b positive because
	 * the new base might be a "flattened" corner where there actually is no single
	 * tile. If we try anyway the result is either inaccurate ("one off" half of the
	 * time) or the code gets much more complex;
	 *
	 * We also need to increment here to have equality as marker for the end of a row or
	 * column. Like that it's shorter than having another if/else in operator++
	 */
	if (this->a_max > 0) {
		this->a_max++;
	} else {
		this->a_max--;
	}

	if (this->b_max > 0) {
		this->b_max++;
	} else {
		this->b_max--;
	}
}

TileIterator &DiagonalTileIterator::operator++()
{
	assert(this->tile != INVALID_TILE);

	bool new_line = false;
	do {
		/* Iterate using the rotated coordinates. */
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

		/* And convert the coordinates back once we've gone to the next tile. */
		uint x = this->base_x + (this->a_cur - this->b_cur) / 2;
		uint y = this->base_y + (this->b_cur + this->a_cur) / 2;
		/* Prevent wrapping around the map's borders. */
		this->tile = x >= MapSizeX() || y >= MapSizeY() ? INVALID_TILE : TileXY(x, y);
	} while (this->tile > MapSize() && this->b_max != this->b_cur);

	if (this->b_max == this->b_cur) this->tile = INVALID_TILE;
	return *this;
}
