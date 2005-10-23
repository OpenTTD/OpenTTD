/* $Id$ */

#include "stdafx.h"
#include "tile.h"

void SetMapExtraBits(TileIndex tile, byte bits)
{
	assert(tile < MapSize());
	SB(_m[tile].extra, 0, 2, bits & 3);
}

uint GetMapExtraBits(TileIndex tile)
{
	assert(tile < MapSize());
	return GB(_m[tile].extra, 0, 2);
}


uint GetTileSlope(TileIndex tile, uint *h)
{
	uint a;
	uint b;
	uint c;
	uint d;
	uint min;
	uint r;

	assert(tile < MapSize());

	if (TileX(tile) == MapMaxX() || TileY(tile) == MapMaxY()) {
		if (h != NULL) *h = 0;
		return 0;
	}

	min = a = TileHeight(tile);
	b = TileHeight(tile + TileDiffXY(1, 0));
	if (min >= b) min = b;
	c = TileHeight(tile + TileDiffXY(0, 1));
	if (min >= c) min = c;
	d = TileHeight(tile + TileDiffXY(1, 1));
	if (min >= d) min = d;

	r = 0;
	if ((a -= min) != 0) r += (--a << 4) + 8;
	if ((c -= min) != 0) r += (--c << 4) + 4;
	if ((d -= min) != 0) r += (--d << 4) + 2;
	if ((b -= min) != 0) r += (--b << 4) + 1;

	if (h != NULL)
		*h = min * 8;

	return r;
}

uint GetTileZ(TileIndex tile)
{
	uint h;
	GetTileSlope(tile, &h);
	return h;
}
