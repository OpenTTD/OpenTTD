#include "stdafx.h"
#include "ttd.h"
#include "functions.h"
#include "map.h"

uint _map_log_x;
uint _map_log_y;

byte   *_map_type_and_height = NULL;
byte   *_map_owner           = NULL;
uint16 *_map2                = NULL;
byte   *_map3_lo             = NULL;
byte   *_map3_hi             = NULL;
byte   *_map5                = NULL;
byte   *_map_extra_bits      = NULL;


void InitMap(uint log_x, uint log_y)
{
	uint map_size;

	assert(log_x <= 15 && log_y <= 15);

	_map_log_x = log_x;
	_map_log_y = log_y;

	map_size = MapSize();

	_map_type_and_height =
		realloc(_map_type_and_height, map_size * sizeof(_map_type_and_height[0]));
	_map_owner = realloc(_map_owner, map_size * sizeof(_map_owner[0]));
	_map2      = realloc(_map2,      map_size * sizeof(_map2[0]));
	_map3_lo   = realloc(_map3_lo,   map_size * sizeof(_map3_lo[0]));
	_map3_hi   = realloc(_map3_hi,   map_size * sizeof(_map3_hi[0]));
	_map5      = realloc(_map5,      map_size * sizeof(_map5[0]));
	_map_extra_bits =
		realloc(_map_extra_bits, map_size * sizeof(_map_extra_bits[0] / 4));

	// XXX TODO handle memory shortage more gracefully
	if (_map_type_and_height == NULL ||
			_map_owner           == NULL ||
			_map2                == NULL ||
			_map3_lo             == NULL ||
			_map3_hi             == NULL ||
			_map5                == NULL ||
			_map_extra_bits      == NULL)
		error("Failed to allocate memory for the map");
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

		sprintf(buf, "TILE_ADD(%s) when adding 0x%.4X and 0x%.4X failed",
			exp, tile, add);
#if !defined(_MSC_VER)
		fprintf(stderr, "%s:%d %s\n", file, line, buf);
#else
		_assert(buf, (char*)file, line);
#endif
	}

	assert(TILE_XY(x,y) == TILE_MASK(tile + add));

	return TILE_XY(x,y);
}
#endif


uint ScaleByMapSize(uint n)
{
	int shift = (int)MapLogX() - 8 + (int)MapLogY() - 8;

	if (shift < 0)
		return (n + (1 << -shift) - 1) >> -shift;
	else
		return n << shift;
}


uint ScaleByMapSize1D(uint n)
{
	int shift = ((int)MapLogX() - 8 + (int)MapLogY() - 8) / 2;

	if (shift < 0)
		return (n + (1 << -shift) - 1) >> -shift;
	else
		return n << shift;
}


const TileIndexDiffC _tileoffs_by_dir[] = {
	{-1,  0},
	{ 0,  1},
	{ 1,  0},
	{ 0, -1}
};
