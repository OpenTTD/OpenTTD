/* $Id$ */

/** @file map.h */

#ifndef MAP_H
#define MAP_H

#include "stdafx.h"
#include "direction.h"

extern uint _map_tile_mask;

/**
 * 'Wraps' the given tile to it is within the map. It does
 * this by masking the 'high' bits of.
 * @param x the tile to 'wrap'
 */

#define TILE_MASK(x) ((x) & _map_tile_mask)
/**
 * Asserts when the tile is outside of the map.
 * @param x the tile to check
 */
#define TILE_ASSERT(x) assert(TILE_MASK(x) == (x));

/**
 * Data that is stored per tile. Also used TileExtended for this.
 * Look at docs/landscape.html for the exact meaning of the members.
 */
struct Tile {
	byte type_height; ///< The type (bits 4..7) and height of the northern corner
	byte m1;   ///< Primarily used for ownership information
	uint16 m2; ///< Primarily used for indices to towns, industries and stations
	byte m3;   ///< General purpose
	byte m4;   ///< General purpose
	byte m5;   ///< General purpose
	byte m6;   ///< Primarily used for bridges and rainforest/desert
};

/**
 * Data that is stored per tile. Also used Tile for this.
 * Look at docs/landscape.html for the exact meaning of the members.
 */
struct TileExtended {
	byte m7; ///< Primarily used for newgrf support
};

extern Tile *_m;
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
 * Gets the maximum X coordinate within the map, including MP_VOID
 * @return the maximum X coordinate
 */
static inline uint MapMaxY()
{
	return MapSizeY() - 1;
}

/* Scale a number relative to the map size */
uint ScaleByMapSize(uint); // Scale relative to the number of tiles
uint ScaleByMapSize1D(uint); // Scale relative to the circumference of the map

typedef uint32 TileIndex;
typedef int32 TileIndexDiff;

static inline TileIndex TileXY(uint x, uint y)
{
	return (y * MapSizeX()) + x;
}

static inline TileIndexDiff TileDiffXY(int x, int y)
{
	/* Multiplication gives much better optimization on MSVC than shifting.
	 * 0 << shift isn't optimized to 0 properly.
	 * Typically x and y are constants, and then this doesn't result
	 * in any actual multiplication in the assembly code.. */
	return (y * MapSizeX()) + x;
}

static inline TileIndex TileVirtXY(uint x, uint y)
{
	return (y >> 4 << MapLogX()) + (x >> 4);
}


enum {
	INVALID_TILE = (TileIndex)-1 ///< The very nice invalid tile marker
};

enum {
	TILE_SIZE   = 16,   ///< Tiles are 16x16 "units" in size
	TILE_PIXELS = 32,   ///< a tile is 32x32 pixels
	TILE_HEIGHT =  8,   ///< The standard height-difference between tiles on two levels is 8 (z-diff 8)
};


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


struct TileIndexDiffC {
	int16 x;
	int16 y;
};

static inline TileIndexDiff ToTileIndexDiff(TileIndexDiffC tidc)
{
	return (tidc.y << MapLogX()) + tidc.x;
}


#ifndef _DEBUG
	#define TILE_ADD(x,y) ((x) + (y))
#else
	extern TileIndex TileAdd(TileIndex tile, TileIndexDiff add,
		const char *exp, const char *file, int line);
	#define TILE_ADD(x, y) (TileAdd((x), (y), #x " + " #y, __FILE__, __LINE__))
#endif

#define TILE_ADDXY(tile, x, y) TILE_ADD(tile, TileDiffXY(x, y))

uint TileAddWrap(TileIndex tile, int addx, int addy);

static inline TileIndexDiffC TileIndexDiffCByDiagDir(DiagDirection dir)
{
	extern const TileIndexDiffC _tileoffs_by_diagdir[DIAGDIR_END];

	assert(IsValidDiagDirection(dir));
	return _tileoffs_by_diagdir[dir];
}

/* Returns tile + the diff given in diff. If the result tile would end up
 * outside of the map, INVALID_TILE is returned instead.
 */
static inline TileIndex AddTileIndexDiffCWrap(TileIndex tile, TileIndexDiffC diff)
{
	int x = TileX(tile) + diff.x;
	int y = TileY(tile) + diff.y;
	if (x < 0 || y < 0 || x > (int)MapMaxX() || y > (int)MapMaxY())
		return INVALID_TILE;
	else
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


#define BEGIN_TILE_LOOP(var, w, h, tile)                      \
	{                                                        \
		int h_cur = h;                                         \
		uint var = tile;                                       \
		do {                                                   \
			int w_cur = w;                                       \
			do {

#define END_TILE_LOOP(var, w, h, tile)                        \
			} while (++var, --w_cur != 0);                       \
		} while (var += TileDiffXY(0, 1) - (w), --h_cur != 0); \
	}

static inline TileIndexDiff TileOffsByDiagDir(DiagDirection dir)
{
	extern const TileIndexDiffC _tileoffs_by_diagdir[DIAGDIR_END];

	assert(IsValidDiagDirection(dir));
	return ToTileIndexDiff(_tileoffs_by_diagdir[dir]);
}

static inline TileIndexDiff TileOffsByDir(Direction dir)
{
	extern const TileIndexDiffC _tileoffs_by_dir[DIR_END];

	assert(IsValidDirection(dir));
	return ToTileIndexDiff(_tileoffs_by_dir[dir]);
}

typedef bool TestTileOnSearchProc(TileIndex tile, uint32 data);
bool CircularTileSearch(TileIndex tile, uint size, TestTileOnSearchProc proc, uint32 data);

/* Approximation of the length of a straight track, relative to a diagonal
 * track (ie the size of a tile side). #defined instead of const so it can
 * stay integer. (no runtime float operations) Is this needed?
 * Watch out! There are _no_ brackets around here, to prevent intermediate
 * rounding! Be careful when using this!
 * This value should be sqrt(2)/2 ~ 0.7071 */
#define STRAIGHT_TRACK_LENGTH 7071/10000

#endif /* MAP_H */
