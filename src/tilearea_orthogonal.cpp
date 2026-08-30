/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tilearea_orthogonal.cpp Handling of orthogonal tile areas. */

#include "stdafx.h"

#include "tilearea_orthogonal.h"

#include "safeguards.h"

/**
 * Construct this tile area based on two points.
 * @param start the start of the area.
 * @param end the end of the area.
 */
OrthogonalTileArea::OrthogonalTileArea(TileIndex start, TileIndex end)
{
	assert(start < Map::Size());
	assert(end < Map::Size());

	uint sx = TileX(start);
	uint sy = TileY(start);
	uint ex = TileX(end);
	uint ey = TileY(end);

	if (sx > ex) std::swap(sx, ex);
	if (sy > ey) std::swap(sy, ey);

	this->tile = TileXY(sx, sy);
	this->w = ex - sx + 1;
	this->h = ey - sy + 1;
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
	this->w = ex - sx + 1;
	this->h = ey - sy + 1;
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

	uint left1 = TileX(this->tile);
	uint top1 = TileY(this->tile);
	uint right1 = left1 + this->w - 1;
	uint bottom1 = top1 + this->h - 1;

	uint left2 = TileX(ta.tile);
	uint top2 = TileY(ta.tile);
	uint right2 = left2 + ta.w - 1;
	uint bottom2 = top2 + ta.h - 1;

	return !(left2 > right1 || right2 < left1 || top2 > bottom1 || bottom2 < top1);
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

	uint left = TileX(this->tile);
	uint top = TileY(this->tile);
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
	int ex = std::min<int>(x + this->w + rad, Map::SizeX());
	int ey = std::min<int>(y + this->h + rad, Map::SizeY());

	this->tile = TileXY(sx, sy);
	this->w = ex - sx;
	this->h = ey - sy;
	return *this;
}

/**
 * Clamp the tile area to map borders.
 */
void OrthogonalTileArea::ClampToMap()
{
	assert(this->tile < Map::Size());
	this->w = std::min<int>(this->w, Map::SizeX() - TileX(this->tile));
	this->h = std::min<int>(this->h, Map::SizeY() - TileY(this->tile));
}
