/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file map.cpp Base functions related to the map and distances on them. */

#include "stdafx.h"
#include "debug.h"
#include "core/alloc_func.hpp"
#include "water_map.h"
#include "error_func.h"
#include "string_func.h"

#include "safeguards.h"

#if defined(_MSC_VER)
/* Why the hell is that not in all MSVC headers?? */
extern "C" _CRTIMP void __cdecl _assert(void *, void *, unsigned);
#endif

/* static */ uint Map::log_x;     ///< 2^_map_log_x == _map_size_x
/* static */ uint Map::log_y;     ///< 2^_map_log_y == _map_size_y
/* static */ uint Map::size_x;    ///< Size of the map along the X
/* static */ uint Map::size_y;    ///< Size of the map along the Y
/* static */ uint Map::size;      ///< The number of tiles on the map
/* static */ uint Map::tile_mask; ///< _map_size - 1 (to mask the mapsize)

/* static */ Tile::TileBase *Tile::base_tiles = nullptr;         ///< Base tiles of the map
/* static */ Tile::TileExtended *Tile::extended_tiles = nullptr; ///< Extended tiles of the map


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

	free(Tile::base_tiles);
	free(Tile::extended_tiles);

	Tile::base_tiles = CallocT<Tile::TileBase>(Map::size);
	Tile::extended_tiles = CallocT<Tile::TileExtended>(Map::size);
}


#ifdef _DEBUG
TileIndex TileAdd(TileIndex tile, TileIndexDiff add,
	const char *exp, const char *file, int line)
{
	int dx;
	int dy;
	uint x;
	uint y;

	dx = add & Map::MaxX();
	if (dx >= (int)Map::SizeX() / 2) dx -= Map::SizeX();
	dy = (add - dx) / (int)Map::SizeX();

	x = TileX(tile) + dx;
	y = TileY(tile) + dy;

	if (x >= Map::SizeX() || y >= Map::SizeY()) {
		std::string message = fmt::format("TILE_ADD({}) when adding 0x{:04X} and 0x{:04X} failed",
			exp, tile, add);
#if !defined(_MSC_VER)
		fmt::print(stderr, "{}:{} {}\n", file, line, message);
#else
		_assert(message.data(), (char*)file, line);
#endif
	}

	assert(TileXY(x, y) == Map::WrapToMap(tile + add));

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
 * Also known as euclidian- or L2-Norm squared.
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
 * Function performing a search around a center tile and going outward, thus in circle.
 * Although it really is a square search...
 * Every tile will be tested by means of the callback function proc,
 * which will determine if yes or no the given tile meets criteria of search.
 * @param tile to start the search from. Upon completion, it will return the tile matching the search
 * @param size: number of tiles per side of the desired search area
 * @param proc: callback testing function pointer.
 * @param user_data to be passed to the callback function. Depends on the implementation
 * @return result of the search
 * @pre proc != nullptr
 * @pre size > 0
 */
bool CircularTileSearch(TileIndex *tile, uint size, TestTileOnSearchProc proc, void *user_data)
{
	assert(proc != nullptr);
	assert(size > 0);

	if (size % 2 == 1) {
		/* If the length of the side is uneven, the center has to be checked
		 * separately, as the pattern of uneven sides requires to go around the center */
		if (proc(*tile, user_data)) return true;

		/* If tile test is not successful, get one tile up,
		 * ready for a test in first circle around center tile */
		*tile = TileAddByDir(*tile, DIR_N);
		return CircularTileSearch(tile, size / 2, 1, 1, proc, user_data);
	} else {
		return CircularTileSearch(tile, size / 2, 0, 0, proc, user_data);
	}
}

/**
 * Generalized circular search allowing for rectangles and a hole.
 * Function performing a search around a center rectangle and going outward.
 * The center rectangle is left out from the search. To do a rectangular search
 * without a hole, set either h or w to zero.
 * Every tile will be tested by means of the callback function proc,
 * which will determine if yes or no the given tile meets criteria of search.
 * @param tile to start the search from. Upon completion, it will return the tile matching the search.
 *  This tile should be directly north of the hole (if any).
 * @param radius How many tiles to search outwards. Note: This is a radius and thus different
 *                from the size parameter of the other CircularTileSearch function, which is a diameter.
 * @param w the width of the inner rectangle
 * @param h the height of the inner rectangle
 * @param proc callback testing function pointer.
 * @param user_data to be passed to the callback function. Depends on the implementation
 * @return result of the search
 * @pre proc != nullptr
 * @pre radius > 0
 */
bool CircularTileSearch(TileIndex *tile, uint radius, uint w, uint h, TestTileOnSearchProc proc, void *user_data)
{
	assert(proc != nullptr);
	assert(radius > 0);

	uint x = TileX(*tile) + w + 1;
	uint y = TileY(*tile);

	const uint extent[DIAGDIR_END] = { w, h, w, h };

	for (uint n = 0; n < radius; n++) {
		for (DiagDirection dir = DIAGDIR_BEGIN; dir < DIAGDIR_END; dir++) {
			/* Is the tile within the map? */
			for (uint j = extent[dir] + n * 2 + 1; j != 0; j--) {
				if (x < Map::SizeX() && y < Map::SizeY()) {
					TileIndex t = TileXY(x, y);
					/* Is the callback successful? */
					if (proc(t, user_data)) {
						/* Stop the search */
						*tile = t;
						return true;
					}
				}

				/* Step to the next 'neighbour' in the circular line */
				x += _tileoffs_by_diagdir[dir].x;
				y += _tileoffs_by_diagdir[dir].y;
			}
		}
		/* Jump to next circle to test */
		x += _tileoffs_by_dir[DIR_W].x;
		y += _tileoffs_by_dir[DIR_W].y;
	}

	*tile = INVALID_TILE;
	return false;
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
		for (TileIndex t = 0; t < Map::Size(); t++) {
			if (!IsTileType(t, MP_VOID) && !IsTileType(t, MP_WATER)) return 0x1FF;
		}
	}

	return max_dist;
}
