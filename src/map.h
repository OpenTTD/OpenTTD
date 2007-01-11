/* $Id$ */

#ifndef MAP_H
#define MAP_H

#include "stdafx.h"

// Putting externs inside inline functions seems to confuse the aliasing
// checking on MSVC6. Never use those variables directly.
extern uint _map_log_x;
extern uint _map_size_x;
extern uint _map_size_y;
extern uint _map_tile_mask;
extern uint _map_size;

#define TILE_MASK(x) ((x) & _map_tile_mask)
#define TILE_ASSERT(x) assert(TILE_MASK(x) == (x));

typedef struct Tile {
	byte type_height;
	byte m1;
	uint16 m2;
	byte m3;
	byte m4;
	byte m5;
	byte m6;
} Tile;

extern Tile* _m;

void AllocateMap(uint size_x, uint size_y);

// binary logarithm of the map size, try to avoid using this one
static inline uint MapLogX(void)  { return _map_log_x; }
/* The size of the map */
static inline uint MapSizeX(void) { return _map_size_x; }
static inline uint MapSizeY(void) { return _map_size_y; }
/* The maximum coordinates */
static inline uint MapMaxX(void) { return _map_size_x - 1; }
static inline uint MapMaxY(void) { return _map_size_y - 1; }
/* The number of tiles in the map */
static inline uint MapSize(void) { return _map_size; }

// Scale a number relative to the map size
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
	// Multiplication gives much better optimization on MSVC than shifting.
	// 0 << shift isn't optimized to 0 properly.
	// Typically x and y are constants, and then this doesn't result
	// in any actual multiplication in the assembly code..
	return (y * MapSizeX()) + x;
}

static inline TileIndex TileVirtXY(uint x, uint y)
{
	return (y >> 4 << MapLogX()) + (x >> 4);
}


enum {
	INVALID_TILE = (TileIndex)-1
};

enum {
	TILE_SIZE   = 16,   /* Tiles are 16x16 "units" in size */
	TILE_PIXELS = 32,   /* a tile is 32x32 pixels */
	TILE_HEIGHT =  8,   /* The standard height-difference between tiles on two levels is 8 (z-diff 8) */
};


static inline uint TileX(TileIndex tile)
{
	return tile & MapMaxX();
}

static inline uint TileY(TileIndex tile)
{
	return tile >> MapLogX();
}


typedef struct TileIndexDiffC {
	int16 x;
	int16 y;
} TileIndexDiffC;

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

static inline TileIndexDiffC TileIndexDiffCByDiagDir(uint dir) {
	extern const TileIndexDiffC _tileoffs_by_diagdir[4];

	assert(dir < lengthof(_tileoffs_by_diagdir));
	return _tileoffs_by_diagdir[dir];
}

/* Returns tile + the diff given in diff. If the result tile would end up
 * outside of the map, INVALID_TILE is returned instead.
 */
static inline TileIndex AddTileIndexDiffCWrap(TileIndex tile, TileIndexDiffC diff) {
	int x = TileX(tile) + diff.x;
	int y = TileY(tile) + diff.y;
	if (x < 0 || y < 0 || x > (int)MapMaxX() || y > (int)MapMaxY())
		return INVALID_TILE;
	else
		return TileXY(x, y);
}

// Functions to calculate distances
uint DistanceManhattan(TileIndex, TileIndex); // also known as L1-Norm. Is the shortest distance one could go over diagonal tracks (or roads)
uint DistanceSquare(TileIndex, TileIndex); // euclidian- or L2-Norm squared
uint DistanceMax(TileIndex, TileIndex); // also known as L-Infinity-Norm
uint DistanceMaxPlusManhattan(TileIndex, TileIndex); // Max + Manhattan
uint DistanceFromEdge(TileIndex); // shortest distance from any edge of the map


#define BEGIN_TILE_LOOP(var,w,h,tile)                      \
	{                                                        \
		int h_cur = h;                                         \
		uint var = tile;                                       \
		do {                                                   \
			int w_cur = w;                                       \
			do {

#define END_TILE_LOOP(var,w,h,tile)                        \
			} while (++var, --w_cur != 0);                       \
		} while (var += TileDiffXY(0, 1) - (w), --h_cur != 0); \
	}

static inline TileIndexDiff TileOffsByDiagDir(uint dir)
{
	extern const TileIndexDiffC _tileoffs_by_diagdir[4];

	assert(dir < lengthof(_tileoffs_by_diagdir));
	return ToTileIndexDiff(_tileoffs_by_diagdir[dir]);
}

static inline TileIndexDiff TileOffsByDir(uint dir)
{
	extern const TileIndexDiffC _tileoffs_by_dir[8];

	assert(dir < lengthof(_tileoffs_by_dir));
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
