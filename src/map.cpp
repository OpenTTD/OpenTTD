/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file map.cpp Base functions related to the map and distances on them. */

#include "stdafx.h"
#include "debug.h"
#include "water_map.h"
#include "error_func.h"
#include "string_func.h"
#include "pathfinder/water_regions.h"

#include "safeguards.h"

/* static */ uint Map::log_x;     ///< 2^_map_log_x == _map_size_x
/* static */ uint Map::log_y;     ///< 2^_map_log_y == _map_size_y
/* static */ uint Map::size_x;    ///< Size of the map along the X
/* static */ uint Map::size_y;    ///< Size of the map along the Y
/* static */ uint Map::size;      ///< The number of tiles on the map
/* static */ uint Map::tile_mask; ///< _map_size - 1 (to mask the mapsize)

/* static */ uint Map::initial_land_count; ///< Initial number of land tiles on the map.

/* static */ std::unique_ptr<Tile::TileBase[]> Tile::base_tiles; ///< Base tiles of the map
/* static */ std::unique_ptr<Tile::TileExtended[]> Tile::extended_tiles; ///< Extended tiles of the map


/**
 * (Re)allocates a map with the given dimension
 * @param size_x the width of the map along the NE/SW edge
 * @param size_y the 'height' of the map along the SE/NW edge
 */
/* static */ void Map::Allocate(uint size_x, uint size_y)
{
	/* Make sure that the map size is within the limits and that
	 * size of both axes is a power of 2. */
	if (!IsInsideMM(size_x, MIN_MAP_SIZE, MAX_MAP_SIZE + 1) ||
			!IsInsideMM(size_y, MIN_MAP_SIZE, MAX_MAP_SIZE + 1) ||
			(size_x & (size_x - 1)) != 0 ||
			(size_y & (size_y - 1)) != 0) {
		FatalError("Invalid map size");
	}

	Debug(map, 1, "Allocating map of size {}x{}", size_x, size_y);

	Map::log_x = FindFirstBit(size_x);
	Map::log_y = FindFirstBit(size_y);
	Map::size_x = size_x;
	Map::size_y = size_y;
	Map::size = size_x * size_y;
	Map::tile_mask = Map::size - 1;

	Tile::base_tiles = std::make_unique<Tile::TileBase[]>(Map::size);
	Tile::extended_tiles = std::make_unique<Tile::TileExtended[]>(Map::size);

	AllocateWaterRegions();
}

/* static */ void Map::CountLandTiles()
{
	/* Count number of tiles that are land. */
	Map::initial_land_count = 0;
	for (const auto tile : Map::Iterate()) {
		Map::initial_land_count += IsWaterTile(tile) ? 0 : 1;
	}

	/* Compensate for default values being set for (or users are most familiar with) at least
	 * very low sea level. Dividing by 12 adds roughly 8%. */
	Map::initial_land_count += Map::initial_land_count / 12;
	Map::initial_land_count = std::min(Map::initial_land_count, Map::size);
}


#ifdef _DEBUG
TileIndex TileAdd(TileIndex tile, TileIndexDiff offset)
{
	int dx = offset & Map::MaxX();
	if (dx >= (int)Map::SizeX() / 2) dx -= Map::SizeX();
	int dy = (offset - dx) / (int)Map::SizeX();

	uint32_t x = TileX(tile) + dx;
	uint32_t y = TileY(tile) + dy;

	assert(x < Map::SizeX());
	assert(y < Map::SizeY());
	assert(TileXY(x, y) == Map::WrapToMap(tile + offset));

	return TileXY(x, y);
}
#endif

/**
 * This function checks if we add addx/addy to tile, if we
 * do wrap around the edges. For example, tile = (10,2) and
 * addx = +3 and addy = -4. This function will now return
 * INVALID_TILE, because the y is wrapped. This is needed in
 * for example, farmland. When the tile is not wrapped,
 * the result will be tile + TileDiffXY(addx, addy)
 *
 * @param tile the 'starting' point of the adding
 * @param addx the amount of tiles in the X direction to add
 * @param addy the amount of tiles in the Y direction to add
 * @return translated tile, or INVALID_TILE when it would've wrapped.
 */
TileIndex TileAddWrap(TileIndex tile, int addx, int addy)
{
	uint x = TileX(tile) + addx;
	uint y = TileY(tile) + addy;

	/* Disallow void tiles at the north border. */
	if ((x == 0 || y == 0) && _settings_game.construction.freeform_edges) return INVALID_TILE;

	/* Are we about to wrap? */
	if (x >= Map::MaxX() || y >= Map::MaxY()) return INVALID_TILE;

	return TileXY(x, y);
}

/** 'Lookup table' for tile offsets given an Axis */
extern const TileIndexDiffC _tileoffs_by_axis[] = {
	{ 1,  0}, ///< AXIS_X
	{ 0,  1}, ///< AXIS_Y
};

/** 'Lookup table' for tile offsets given a DiagDirection */
extern const TileIndexDiffC _tileoffs_by_diagdir[] = {
	{-1,  0}, ///< DIAGDIR_NE
	{ 0,  1}, ///< DIAGDIR_SE
	{ 1,  0}, ///< DIAGDIR_SW
	{ 0, -1}  ///< DIAGDIR_NW
};

/** 'Lookup table' for tile offsets given a Direction */
extern const TileIndexDiffC _tileoffs_by_dir[] = {
	{-1, -1}, ///< DIR_N
	{-1,  0}, ///< DIR_NE
	{-1,  1}, ///< DIR_E
	{ 0,  1}, ///< DIR_SE
	{ 1,  1}, ///< DIR_S
	{ 1,  0}, ///< DIR_SW
	{ 1, -1}, ///< DIR_W
	{ 0, -1}  ///< DIR_NW
};

/**
 * Gets the Manhattan distance between the two given tiles.
 * The Manhattan distance is the sum of the delta of both the
 * X and Y component.
 * Also known as L1-Norm
 * @param t0 the start tile
 * @param t1 the end tile
 * @return the distance
 */
uint DistanceManhattan(TileIndex t0, TileIndex t1)
{
	const uint dx = Delta(TileX(t0), TileX(t1));
	const uint dy = Delta(TileY(t0), TileY(t1));
	return dx + dy;
}


/**
 * Gets the 'Square' distance between the two given tiles.
 * The 'Square' distance is the square of the shortest (straight line)
 * distance between the two tiles.
 * Also known as Euclidean- or L2-Norm squared.
 * @param t0 the start tile
 * @param t1 the end tile
 * @return the distance
 */
uint DistanceSquare(TileIndex t0, TileIndex t1)
{
	const int dx = TileX(t0) - TileX(t1);
	const int dy = TileY(t0) - TileY(t1);
	return dx * dx + dy * dy;
}


/**
 * Gets the biggest distance component (x or y) between the two given tiles.
 * Also known as L-Infinity-Norm.
 * @param t0 the start tile
 * @param t1 the end tile
 * @return the distance
 */
uint DistanceMax(TileIndex t0, TileIndex t1)
{
	const uint dx = Delta(TileX(t0), TileX(t1));
	const uint dy = Delta(TileY(t0), TileY(t1));
	return std::max(dx, dy);
}


/**
 * Gets the biggest distance component (x or y) between the two given tiles
 * plus the Manhattan distance, i.e. two times the biggest distance component
 * and once the smallest component.
 * @param t0 the start tile
 * @param t1 the end tile
 * @return the distance
 */
uint DistanceMaxPlusManhattan(TileIndex t0, TileIndex t1)
{
	const uint dx = Delta(TileX(t0), TileX(t1));
	const uint dy = Delta(TileY(t0), TileY(t1));
	return dx > dy ? 2 * dx + dy : 2 * dy + dx;
}

/**
 * Param the minimum distance to an edge
 * @param tile the tile to get the distance from
 * @return the distance from the edge in tiles
 */
uint DistanceFromEdge(TileIndex tile)
{
	const uint xl = TileX(tile);
	const uint yl = TileY(tile);
	const uint xh = Map::SizeX() - 1 - xl;
	const uint yh = Map::SizeY() - 1 - yl;
	const uint minl = std::min(xl, yl);
	const uint minh = std::min(xh, yh);
	return std::min(minl, minh);
}

/**
 * Gets the distance to the edge of the map in given direction.
 * @param tile the tile to get the distance from
 * @param dir the direction of interest
 * @return the distance from the edge in tiles
 */
uint DistanceFromEdgeDir(TileIndex tile, DiagDirection dir)
{
	switch (dir) {
		case DIAGDIR_NE: return             TileX(tile) - (_settings_game.construction.freeform_edges ? 1 : 0);
		case DIAGDIR_NW: return             TileY(tile) - (_settings_game.construction.freeform_edges ? 1 : 0);
		case DIAGDIR_SW: return Map::MaxX() - TileX(tile) - 1;
		case DIAGDIR_SE: return Map::MaxY() - TileY(tile) - 1;
		default: NOT_REACHED();
	}
}

/**
 * Finds the distance for the closest tile with water/land given a tile
 * @param tile  the tile to find the distance too
 * @param water whether to find water or land
 * @return distance to nearest water (max 0x7F) / land (max 0x1FF; 0x200 if there is no land)
 */
uint GetClosestWaterDistance(TileIndex tile, bool water)
{
	if (HasTileWaterGround(tile) == water) return 0;

	uint max_dist = water ? 0x7F : 0x200;

	int x = TileX(tile);
	int y = TileY(tile);

	uint max_x = Map::MaxX();
	uint max_y = Map::MaxY();
	uint min_xy = _settings_game.construction.freeform_edges ? 1 : 0;

	/* go in a 'spiral' with increasing manhattan distance in each iteration */
	for (uint dist = 1; dist < max_dist; dist++) {
		/* next 'diameter' */
		y--;

		/* going counter-clockwise around this square */
		for (DiagDirection dir = DIAGDIR_BEGIN; dir < DIAGDIR_END; dir++) {
			static const int8_t ddx[DIAGDIR_END] = { -1,  1,  1, -1};
			static const int8_t ddy[DIAGDIR_END] = {  1,  1, -1, -1};

			int dx = ddx[dir];
			int dy = ddy[dir];

			/* each side of this square has length 'dist' */
			for (uint a = 0; a < dist; a++) {
				/* MP_VOID tiles are not checked (interval is [min; max) for IsInsideMM())*/
				if (IsInsideMM(x, min_xy, max_x) && IsInsideMM(y, min_xy, max_y)) {
					TileIndex t = TileXY(x, y);
					if (HasTileWaterGround(t) == water) return dist;
				}
				x += dx;
				y += dy;
			}
		}
	}

	if (!water) {
		/* no land found - is this a water-only map? */
		for (const auto t : Map::Iterate()) {
			if (!IsTileType(t, MP_VOID) && !IsTileType(t, MP_WATER)) return 0x1FF;
		}
	}

	return max_dist;
}
