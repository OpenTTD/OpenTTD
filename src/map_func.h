/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file map_func.h Functions related to maps. */

#ifndef MAP_FUNC_H
#define MAP_FUNC_H

#include "core/math_func.hpp"
#include "tile_type.h"
#include "map_type.h"
#include "direction_func.h"

extern uint _map_tile_mask;

/**
 * 'Wraps' the given tile to it is within the map. It does
 * this by masking the 'high' bits of.
 * @param x the tile to 'wrap'
 */

#define TILE_MASK(x) ((x) & _map_tile_mask)

/**
 * Pointer to the tile-array.
 *
 * This variable points to the tile-array which contains the tiles of
 * the map.
 */
extern Tile *_m;

/**
 * Pointer to the extended tile-array.
 *
 * This variable points to the extended tile-array which contains the tiles
 * of the map.
 */
extern TileExtended *_me;

void AllocateMap(uint size_x, uint size_y);

/**
 * Logarithm of the map size along the X side.
 * @note try to avoid using this one
 * @return 2^"return value" == MapSizeX()
 */
static inline uint MapLogX()
{
	extern uint _map_log_x;
	return _map_log_x;
}

/**
 * Logarithm of the map size along the y side.
 * @note try to avoid using this one
 * @return 2^"return value" == MapSizeY()
 */
static inline uint MapLogY()
{
	extern uint _map_log_y;
	return _map_log_y;
}

/**
 * Get the size of the map along the X
 * @return the number of tiles along the X of the map
 */
static inline uint MapSizeX()
{
	extern uint _map_size_x;
	return _map_size_x;
}

/**
 * Get the size of the map along the Y
 * @return the number of tiles along the Y of the map
 */
static inline uint MapSizeY()
{
	extern uint _map_size_y;
	return _map_size_y;
}

/**
 * Get the size of the map
 * @return the number of tiles of the map
 */
static inline uint MapSize()
{
	extern uint _map_size;
	return _map_size;
}

/**
 * Gets the maximum X coordinate within the map, including MP_VOID
 * @return the maximum X coordinate
 */
static inline uint MapMaxX()
{
	return MapSizeX() - 1;
}

/**
 * Gets the maximum Y coordinate within the map, including MP_VOID
 * @return the maximum Y coordinate
 */
static inline uint MapMaxY()
{
	return MapSizeY() - 1;
}

/**
 * Scales the given value by the map size, where the given value is
 * for a 256 by 256 map.
 * @param n the value to scale
 * @return the scaled size
 */
static inline uint ScaleByMapSize(uint n)
{
	/* Subtract 12 from shift in order to prevent integer overflow
	 * for large values of n. It's safe since the min mapsize is 64x64. */
	return CeilDiv(n << (MapLogX() + MapLogY() - 12), 1 << 4);
}


/**
 * Scales the given value by the maps circumference, where the given
 * value is for a 256 by 256 map
 * @param n the value to scale
 * @return the scaled size
 */
static inline uint ScaleByMapSize1D(uint n)
{
	/* Normal circumference for the X+Y is 256+256 = 1<<9
	 * Note, not actually taking the full circumference into account,
	 * just half of it. */
	return CeilDiv((n << MapLogX()) + (n << MapLogY()), 1 << 9);
}

/**
 * An offset value between to tiles.
 *
 * This value is used for the difference between
 * two tiles. It can be added to a tileindex to get
 * the resulting tileindex of the start tile applied
 * with this saved difference.
 *
 * @see TileDiffXY(int, int)
 */
typedef int32 TileIndexDiff;

/**
 * Returns the TileIndex of a coordinate.
 *
 * @param x The x coordinate of the tile
 * @param y The y coordinate of the tile
 * @return The TileIndex calculated by the coordinate
 */
static inline TileIndex TileXY(uint x, uint y)
{
	return (y << MapLogX()) + x;
}

/**
 * Calculates an offset for the given coordinate(-offset).
 *
 * This function calculate an offset value which can be added to an
 * #TileIndex. The coordinates can be negative.
 *
 * @param x The offset in x direction
 * @param y The offset in y direction
 * @return The resulting offset value of the given coordinate
 * @see ToTileIndexDiff(TileIndexDiffC)
 */
static inline TileIndexDiff TileDiffXY(int x, int y)
{
	/* Multiplication gives much better optimization on MSVC than shifting.
	 * 0 << shift isn't optimized to 0 properly.
	 * Typically x and y are constants, and then this doesn't result
	 * in any actual multiplication in the assembly code.. */
	return (y * MapSizeX()) + x;
}

/**
 * Get a tile from the virtual XY-coordinate.
 * @param x The virtual x coordinate of the tile.
 * @param y The virtual y coordinate of the tile.
 * @return The TileIndex calculated by the coordinate.
 */
static inline TileIndex TileVirtXY(uint x, uint y)
{
	return (y >> 4 << MapLogX()) + (x >> 4);
}


/**
 * Get the X component of a tile
 * @param tile the tile to get the X component of
 * @return the X component
 */
static inline uint TileX(TileIndex tile)
{
	return tile & MapMaxX();
}

/**
 * Get the Y component of a tile
 * @param tile the tile to get the Y component of
 * @return the Y component
 */
static inline uint TileY(TileIndex tile)
{
	return tile >> MapLogX();
}

/**
 * Return the offset between to tiles from a TileIndexDiffC struct.
 *
 * This function works like #TileDiffXY(int, int) and returns the
 * difference between two tiles.
 *
 * @param tidc The coordinate of the offset as TileIndexDiffC
 * @return The difference between two tiles.
 * @see TileDiffXY(int, int)
 */
static inline TileIndexDiff ToTileIndexDiff(TileIndexDiffC tidc)
{
	return (tidc.y << MapLogX()) + tidc.x;
}


#ifndef _DEBUG
	/**
	 * Adds to tiles together.
	 *
	 * @param x One tile
	 * @param y Another tile to add
	 * @return The resulting tile(index)
	 */
	#define TILE_ADD(x, y) ((x) + (y))
#else
	extern TileIndex TileAdd(TileIndex tile, TileIndexDiff add,
		const char *exp, const char *file, int line);
	#define TILE_ADD(x, y) (TileAdd((x), (y), #x " + " #y, __FILE__, __LINE__))
#endif

/**
 * Adds a given offset to a tile.
 *
 * @param tile The tile to add an offset on it
 * @param x The x offset to add to the tile
 * @param y The y offset to add to the tile
 */
#define TILE_ADDXY(tile, x, y) TILE_ADD(tile, TileDiffXY(x, y))

TileIndex TileAddWrap(TileIndex tile, int addx, int addy);

/**
 * Returns the TileIndexDiffC offset from a DiagDirection.
 *
 * @param dir The given direction
 * @return The offset as TileIndexDiffC value
 */
static inline TileIndexDiffC TileIndexDiffCByDiagDir(DiagDirection dir)
{
	extern const TileIndexDiffC _tileoffs_by_diagdir[DIAGDIR_END];

	assert(IsValidDiagDirection(dir));
	return _tileoffs_by_diagdir[dir];
}

/**
 * Returns the TileIndexDiffC offset from a Direction.
 *
 * @param dir The given direction
 * @return The offset as TileIndexDiffC value
 */
static inline TileIndexDiffC TileIndexDiffCByDir(Direction dir)
{
	extern const TileIndexDiffC _tileoffs_by_dir[DIR_END];

	assert(IsValidDirection(dir));
	return _tileoffs_by_dir[dir];
}

/**
 * Add a TileIndexDiffC to a TileIndex and returns the new one.
 *
 * Returns tile + the diff given in diff. If the result tile would end up
 * outside of the map, INVALID_TILE is returned instead.
 *
 * @param tile The base tile to add the offset on
 * @param diff The offset to add on the tile
 * @return The resulting TileIndex
 */
static inline TileIndex AddTileIndexDiffCWrap(TileIndex tile, TileIndexDiffC diff)
{
	int x = TileX(tile) + diff.x;
	int y = TileY(tile) + diff.y;
	/* Negative value will become big positive value after cast */
	if ((uint)x >= MapSizeX() || (uint)y >= MapSizeY()) return INVALID_TILE;
	return TileXY(x, y);
}

/**
 * Returns the diff between two tiles
 *
 * @param tile_a from tile
 * @param tile_b to tile
 * @return the difference between tila_a and tile_b
 */
static inline TileIndexDiffC TileIndexToTileIndexDiffC(TileIndex tile_a, TileIndex tile_b)
{
	TileIndexDiffC difference;

	difference.x = TileX(tile_a) - TileX(tile_b);
	difference.y = TileY(tile_a) - TileY(tile_b);

	return difference;
}

/* Functions to calculate distances */
uint DistanceManhattan(TileIndex, TileIndex); ///< also known as L1-Norm. Is the shortest distance one could go over diagonal tracks (or roads)
uint DistanceSquare(TileIndex, TileIndex); ///< euclidian- or L2-Norm squared
uint DistanceMax(TileIndex, TileIndex); ///< also known as L-Infinity-Norm
uint DistanceMaxPlusManhattan(TileIndex, TileIndex); ///< Max + Manhattan
uint DistanceFromEdge(TileIndex); ///< shortest distance from any edge of the map
uint DistanceFromEdgeDir(TileIndex, DiagDirection); ///< distance from the map edge in given direction

/**
 * Convert a DiagDirection to a TileIndexDiff
 *
 * @param dir The DiagDirection
 * @return The resulting TileIndexDiff
 * @see TileIndexDiffCByDiagDir
 */
static inline TileIndexDiff TileOffsByDiagDir(DiagDirection dir)
{
	extern const TileIndexDiffC _tileoffs_by_diagdir[DIAGDIR_END];

	assert(IsValidDiagDirection(dir));
	return ToTileIndexDiff(_tileoffs_by_diagdir[dir]);
}

/**
 * Convert a Direction to a TileIndexDiff.
 *
 * @param dir The direction to convert from
 * @return The resulting TileIndexDiff
 */
static inline TileIndexDiff TileOffsByDir(Direction dir)
{
	extern const TileIndexDiffC _tileoffs_by_dir[DIR_END];

	assert(IsValidDirection(dir));
	return ToTileIndexDiff(_tileoffs_by_dir[dir]);
}

/**
 * Adds a Direction to a tile.
 *
 * @param tile The current tile
 * @param dir The direction in which we want to step
 * @return the moved tile
 */
static inline TileIndex TileAddByDir(TileIndex tile, Direction dir)
{
	return TILE_ADD(tile, TileOffsByDir(dir));
}

/**
 * Adds a DiagDir to a tile.
 *
 * @param tile The current tile
 * @param dir The direction in which we want to step
 * @return the moved tile
 */
static inline TileIndex TileAddByDiagDir(TileIndex tile, DiagDirection dir)
{
	return TILE_ADD(tile, TileOffsByDiagDir(dir));
}

/**
 * Determines the DiagDirection to get from one tile to another.
 * The tiles do not necessarily have to be adjacent.
 * @param tile_from Origin tile
 * @param tile_to Destination tile
 * @return DiagDirection from tile_from towards tile_to, or INVALID_DIAGDIR if the tiles are not on an axis
 */
static inline DiagDirection DiagdirBetweenTiles(TileIndex tile_from, TileIndex tile_to)
{
	int dx = (int)TileX(tile_to) - (int)TileX(tile_from);
	int dy = (int)TileY(tile_to) - (int)TileY(tile_from);
	if (dx == 0) {
		if (dy == 0) return INVALID_DIAGDIR;
		return (dy < 0 ? DIAGDIR_NW : DIAGDIR_SE);
	} else {
		if (dy != 0) return INVALID_DIAGDIR;
		return (dx < 0 ? DIAGDIR_NE : DIAGDIR_SW);
	}
}

/**
 * A callback function type for searching tiles.
 *
 * @param tile The tile to test
 * @param user_data additional data for the callback function to use
 * @return A boolean value, depend on the definition of the function.
 */
typedef bool TestTileOnSearchProc(TileIndex tile, void *user_data);

bool CircularTileSearch(TileIndex *tile, uint size, TestTileOnSearchProc proc, void *user_data);
bool CircularTileSearch(TileIndex *tile, uint radius, uint w, uint h, TestTileOnSearchProc proc, void *user_data);

/**
 * Get a random tile out of a given seed.
 * @param r the random 'seed'
 * @return a valid tile
 */
static inline TileIndex RandomTileSeed(uint32 r)
{
	return TILE_MASK(r);
}

/**
 * Get a valid random tile.
 * @note a define so 'random' gets inserted in the place where it is actually
 *       called, thus making the random traces more explicit.
 * @return a valid tile
 */
#define RandomTile() RandomTileSeed(Random())

uint GetClosestWaterDistance(TileIndex tile, bool water);

#endif /* MAP_FUNC_H */
