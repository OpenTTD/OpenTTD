/* $Id$ */

/** @file map.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "map.h"
#include "helpers.hpp"
#include "direction_func.h"
#include "core/bitmath_func.hpp"

#if defined(_MSC_VER) && _MSC_VER >= 1400 /* VStudio 2005 is stupid! */
/* Why the hell is that not in all MSVC headers?? */
extern "C" _CRTIMP void __cdecl _assert(void *, void *, unsigned);
#endif

uint _map_log_x;     ///< 2^_map_log_x == _map_size_x
uint _map_size_x;    ///< Size of the map along the X
uint _map_size_y;    ///< Size of the map along the Y
uint _map_size;      ///< The number of tiles on the map
uint _map_tile_mask; ///< _map_size - 1 (to mask the mapsize)

Tile *_m = NULL;          ///< Tiles of the map
TileExtended *_me = NULL; ///< Extended Tiles of the map


/*!
 * (Re)allocates a map with the given dimension
 * @param size_x the width of the map along the NE/SW edge
 * @param size_y the 'height' of the map along the SE/NW edge
 */
void AllocateMap(uint size_x, uint size_y)
{
	/* Make sure that the map size is within the limits and that
	 * the x axis size is a power of 2. */
	if (size_x < 64 || size_x > 2048 ||
			size_y < 64 || size_y > 2048 ||
			(size_x & (size_x - 1)) != 0 ||
			(size_y & (size_y - 1)) != 0)
		error("Invalid map size");

	DEBUG(map, 1, "Allocating map of size %dx%d", size_x, size_y);

	_map_log_x = FindFirstBit(size_x);
	_map_size_x = size_x;
	_map_size_y = size_y;
	_map_size = size_x * size_y;
	_map_tile_mask = _map_size - 1;

	free(_m);
	free(_me);

	/* XXX @todo handle memory shortage more gracefully
	 * CallocT does the out-of-memory check
	 * Maybe some attemps could be made to try with smaller maps down to 64x64
	 * Maybe check for available memory before doing the calls, after all, we know how big
	 * the map is */
	_m = CallocT<Tile>(_map_size);
	_me = CallocT<TileExtended>(_map_size);
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
#if !defined(_MSC_VER) || defined(WINCE)
		fprintf(stderr, "%s:%d %s\n", file, line, buf);
#else
		_assert(buf, (char*)file, line);
#endif
	}

	assert(TileXY(x, y) == TILE_MASK(tile + add));

	return TileXY(x, y);
}
#endif

/*!
 * Scales the given value by the map size, where the given value is
 * for a 256 by 256 map.
 * @param n the value to scale
 * @return the scaled size
 */
uint ScaleByMapSize(uint n)
{
	/* First shift by 12 to prevent integer overflow for large values of n.
	 * >>12 is safe since the min mapsize is 64x64
	 * Add (1<<4)-1 to round upwards. */
	return (n * (MapSize() >> 12) + (1 << 4) - 1) >> 4;
}


/*!
 * Scales the given value by the maps circumference, where the given
 * value is for a 256 by 256 map
 * @param n the value to scale
 * @return the scaled size
 */
uint ScaleByMapSize1D(uint n)
{
	/* Normal circumference for the X+Y is 256+256 = 1<<9
	 * Note, not actually taking the full circumference into account,
	 * just half of it.
	 * (1<<9) - 1 is there to scale upwards. */
	return (n * (MapSizeX() + MapSizeY()) + (1 << 9) - 1) >> 9;
}


/*!
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
uint TileAddWrap(TileIndex tile, int addx, int addy)
{
	uint x = TileX(tile) + addx;
	uint y = TileY(tile) + addy;

	/* Are we about to wrap? */
	if (x < MapMaxX() && y < MapMaxY())
		return tile + TileDiffXY(addx, addy);

	return INVALID_TILE;
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

/*!
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


/*!
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


/*!
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
	return max(dx, dy);
}


/*!
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

/*!
 * Param the minimum distance to an edge
 * @param tile the tile to get the distance from
 * @return the distance from the edge in tiles
 */
uint DistanceFromEdge(TileIndex tile)
{
	const uint xl = TileX(tile);
	const uint yl = TileY(tile);
	const uint xh = MapSizeX() - 1 - xl;
	const uint yh = MapSizeY() - 1 - yl;
	const uint minl = min(xl, yl);
	const uint minh = min(xh, yh);
	return min(minl, minh);
}

/*!
 * Function performing a search around a center tile and going outward, thus in circle.
 * Although it really is a square search...
 * Every tile will be tested by means of the callback function proc,
 * which will determine if yes or no the given tile meets criteria of search.
 * @param tile to start the search from
 * @param size: number of tiles per side of the desired search area
 * @param proc: callback testing function pointer.
 * @param data to be passed to the callback function. Depends on the implementation
 * @return result of the search
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

		/* If tile test is not successful, get one tile down and left,
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
						proc(TileXY(x, y), data)) {     ///< Is the callback successful?
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
