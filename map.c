#include "stdafx.h"
#include "ttd.h"
#include "map.h"

#define TILE_X_BITS 8
#define TILE_Y_BITS 8

uint _map_log_x = TILE_X_BITS;
uint _map_log_y = TILE_Y_BITS;

#define MAP_SIZE ((1 << TILE_X_BITS) * (1 << TILE_Y_BITS))

byte   _map_type_and_height [MAP_SIZE];
byte   _map5                [MAP_SIZE];
byte   _map3_lo             [MAP_SIZE];
byte   _map3_hi             [MAP_SIZE];
byte   _map_owner           [MAP_SIZE];
uint16 _map2                [MAP_SIZE];
byte   _map_extra_bits      [MAP_SIZE / 4];


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


const TileIndexDiffC _tileoffs_by_dir[] = {
	{-1,  0},
	{ 0,  1},
	{ 1,  0},
	{ 0, -1}
};
