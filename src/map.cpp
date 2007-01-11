/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "macros.h"
#include "map.h"
#include "direction.h"
#include "helpers.hpp"

#if defined(_MSC_VER) && _MSC_VER >= 1400 /* VStudio 2005 is stupid! */
/* Why the hell is that not in all MSVC headers?? */
extern "C" _CRTIMP void __cdecl _assert(void *, void *, unsigned);
#endif

uint _map_log_x;
uint _map_size_x;
uint _map_size_y;
uint _map_tile_mask;
uint _map_size;

Tile* _m = NULL;


void AllocateMap(uint size_x, uint size_y)
{
	// Make sure that the map size is within the limits and that
	// the x axis size is a power of 2.
	if (size_x < 64 || size_x > 2048 ||
			size_y < 64 || size_y > 2048 ||
			(size_x&(size_x-1)) != 0 ||
			(size_y&(size_y-1)) != 0)
		error("Invalid map size");

	DEBUG(map, 1, "Allocating map of size %dx%d", size_x, size_y);

	_map_log_x = FindFirstBit(size_x);
	_map_size_x = size_x;
	_map_size_y = size_y;
	_map_size = size_x * size_y;
	_map_tile_mask = _map_size - 1;

	free(_m);
	_m = CallocT<Tile>(_map_size);

	// XXX TODO handle memory shortage more gracefully
	if (_m == NULL) error("Failed to allocate memory for the map");
}


#ifdef _DEBUG
TileIndex TileAdd(TileIndex tile, TileIndexDiff add,
	const char *exp, const char *file, int line)
{
	int dx;
	int dy;
	uint x;
	uint y;

	dx = add & MapMaxX();
	if (dx >= (int)MapSizeX() / 2) dx -= MapSizeX();
	dy = (add - dx) / (int)MapSizeX();

	x = TileX(tile) + dx;
	y = TileY(tile) + dy;

	if (x >= MapSizeX() || y >= MapSizeY()) {
		char buf[512];

		snprintf(buf, lengthof(buf), "TILE_ADD(%s) when adding 0x%.4X and 0x%.4X failed",
			exp, tile, add);
#if !defined(_MSC_VER)
		fprintf(stderr, "%s:%d %s\n", file, line, buf);
#else
		_assert(buf, (char*)file, line);
#endif
	}

	assert(TileXY(x,y) == TILE_MASK(tile + add));

	return TileXY(x,y);
}
#endif


uint ScaleByMapSize(uint n)
{
	// First shift by 12 to prevent integer overflow for large values of n.
	// >>12 is safe since the min mapsize is 64x64
	// Add (1<<4)-1 to round upwards.
	return (n * (MapSize() >> 12) + (1<<4) - 1) >> 4;
}


// Scale relative to the circumference of the map
uint ScaleByMapSize1D(uint n)
{
	// Normal circumference for the X+Y is 256+256 = 1<<9
	// Note, not actually taking the full circumference into account,
	// just half of it.
	// (1<<9) - 1 is there to scale upwards.
	return (n * (MapSizeX() + MapSizeY()) + (1<<9) - 1) >> 9;
}


// This function checks if we add addx/addy to tile, if we
//  do wrap around the edges. For example, tile = (10,2) and
//  addx = +3 and addy = -4. This function will now return
//  INVALID_TILE, because the y is wrapped. This is needed in
//  for example, farmland. When the tile is not wrapped,
//  the result will be tile + TileDiffXY(addx, addy)
uint TileAddWrap(TileIndex tile, int addx, int addy)
{
	uint x = TileX(tile) + addx;
	uint y = TileY(tile) + addy;

	// Are we about to wrap?
	if (x < MapMaxX() && y < MapMaxY())
		return tile + TileDiffXY(addx, addy);

	return INVALID_TILE;
}

extern const TileIndexDiffC _tileoffs_by_diagdir[] = {
	{-1,  0}, // DIAGDIR_NE
	{ 0,  1}, // DIAGDIR_SE
	{ 1,  0}, // DIAGDIR_SW
	{ 0, -1}  // DIAGDIR_NW
};

extern const TileIndexDiffC _tileoffs_by_dir[] = {
	{-1, -1}, // DIR_N
	{-1,  0}, // DIR_NE
	{-1,  1}, // DIR_E
	{ 0,  1}, // DIR_SE
	{ 1,  1}, // DIR_S
	{ 1,  0}, // DIR_SW
	{ 1, -1}, // DIR_W
	{ 0, -1}  // DIR_NW
};

uint DistanceManhattan(TileIndex t0, TileIndex t1)
{
	const uint dx = delta(TileX(t0), TileX(t1));
	const uint dy = delta(TileY(t0), TileY(t1));
	return dx + dy;
}


uint DistanceSquare(TileIndex t0, TileIndex t1)
{
	const int dx = TileX(t0) - TileX(t1);
	const int dy = TileY(t0) - TileY(t1);
	return dx * dx + dy * dy;
}


uint DistanceMax(TileIndex t0, TileIndex t1)
{
	const uint dx = delta(TileX(t0), TileX(t1));
	const uint dy = delta(TileY(t0), TileY(t1));
	return dx > dy ? dx : dy;
}


uint DistanceMaxPlusManhattan(TileIndex t0, TileIndex t1)
{
	const uint dx = delta(TileX(t0), TileX(t1));
	const uint dy = delta(TileY(t0), TileY(t1));
	return dx > dy ? 2 * dx + dy : 2 * dy + dx;
}

uint DistanceFromEdge(TileIndex tile)
{
	const uint xl = TileX(tile);
	const uint yl = TileY(tile);
	const uint xh = MapSizeX() - 1 - xl;
	const uint yh = MapSizeY() - 1 - yl;
	const uint minl = xl < yl ? xl : yl;
	const uint minh = xh < yh ? xh : yh;
	return minl < minh ? minl : minh;
}

/**
 * Function performing a search around a center tile and going outward, thus in circle.
 * Although it really is a square search...
 * Every tile will be tested by means of the callback function proc,
 * which will determine if yes or no the given tile meets criteria of search.
 * @param tile to start the search from
 * @param size: number of tiles per side of the desired search area
 * @param proc: callback testing function pointer.
 * @param data to be passed to the callback function. Depends on the implementation
 * @result of the search
 * @pre proc != NULL
 * @pre size > 0
 */
bool CircularTileSearch(TileIndex tile, uint size, TestTileOnSearchProc proc, uint32 data)
{
	uint n, x, y;
	DiagDirection dir;

	assert(proc != NULL);
	assert(size > 0);

	x = TileX(tile);
	y = TileY(tile);

	if (size % 2 == 1) {
		/* If the length of the side is uneven, the center has to be checked
		 * separately, as the pattern of uneven sides requires to go around the center */
		n = 2;
		if (proc(TileXY(x, y), data)) return true;

		/* If tile test is not successfull, get one tile down and left,
		 * ready for a test in first circle around center tile */
		x += _tileoffs_by_dir[DIR_W].x;
		y += _tileoffs_by_dir[DIR_W].y;
	} else {
		n = 1;
		/* To use _tileoffs_by_diagdir's order, we must relocate to
		 * another tile, as we now first go 'up', 'right', 'down', 'left'
		 * instead of 'right', 'down', 'left', 'up', which the calling
		 * function assume. */
		x++;
	}

	for (; n < size; n += 2) {
		for (dir = DIAGDIR_NE; dir < DIAGDIR_END; dir++) {
			uint j;
			for (j = n; j != 0; j--) {
				if (x <= MapMaxX() && y <= MapMaxY() && ///< Is the tile within the map?
						proc(TileXY(x, y), data)) {     ///< Is the callback successfulll?
					return true;                        ///< then stop the search
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
	return false;
}
