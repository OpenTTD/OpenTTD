/* $Id$ */

#include "stdafx.h"
#include "tile.h"

/** Converts the heights of 4 corners into a tileh, and returns the minimum height of the tile
  * @param n,w,e,s the four corners
  * @param h uint pointer to write the height to
  * @return the tileh
*/
Slope GetTileh(uint n, uint w, uint e, uint s, uint *h)
{
	uint min = n;
	Slope r;

	if (min >= w) min = w;
	if (min >= e) min = e;
	if (min >= s) min = s;

	r = SLOPE_FLAT;
	if ((n -= min) != 0) r += (--n << 4) + SLOPE_N;
	if ((e -= min) != 0) r += (--e << 4) + SLOPE_E;
	if ((s -= min) != 0) r += (--s << 4) + SLOPE_S;
	if ((w -= min) != 0) r += (--w << 4) + SLOPE_W;

	if (h != NULL) *h = min * TILE_HEIGHT;

	return r;
}

Slope GetTileSlope(TileIndex tile, uint *h)
{
	uint a;
	uint b;
	uint c;
	uint d;

	assert(tile < MapSize());

	if (TileX(tile) == MapMaxX() || TileY(tile) == MapMaxY()) {
		if (h != NULL) *h = 0;
		return 0;
	}

	a = TileHeight(tile);
	b = TileHeight(tile + TileDiffXY(1, 0));
	c = TileHeight(tile + TileDiffXY(0, 1));
	d = TileHeight(tile + TileDiffXY(1, 1));

	return GetTileh(a, b, c, d, h);
}

uint GetTileZ(TileIndex tile)
{
	uint h;
	GetTileSlope(tile, &h);
	return h;
}
