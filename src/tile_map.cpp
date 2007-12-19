/* $Id$ */

/** @file tile.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "tile_map.h"

Slope GetTileSlope(TileIndex tile, uint *h)
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
		return SLOPE_FLAT;
	}

	min = a = TileHeight(tile);
	b = TileHeight(tile + TileDiffXY(1, 0));
	if (min > b) min = b;
	c = TileHeight(tile + TileDiffXY(0, 1));
	if (min > c) min = c;
	d = TileHeight(tile + TileDiffXY(1, 1));
	if (min > d) min = d;

	r = SLOPE_FLAT;
	if ((a -= min) != 0) r += (--a << 4) + SLOPE_N;
	if ((c -= min) != 0) r += (--c << 4) + SLOPE_E;
	if ((d -= min) != 0) r += (--d << 4) + SLOPE_S;
	if ((b -= min) != 0) r += (--b << 4) + SLOPE_W;

	if (h != NULL) *h = min * TILE_HEIGHT;

	return (Slope)r;
}

uint GetTileZ(TileIndex tile)
{
	if (TileX(tile) == MapMaxX() || TileY(tile) == MapMaxY()) return 0;

	uint h = TileHeight(tile);
	h = min(h, TileHeight(tile + TileDiffXY(1, 0)));
	h = min(h, TileHeight(tile + TileDiffXY(0, 1)));
	h = min(h, TileHeight(tile + TileDiffXY(1, 1)));

	return h * TILE_HEIGHT;
}


uint GetTileMaxZ(TileIndex t)
{
	if (TileX(t) == MapMaxX() || TileY(t) == MapMaxY()) return 0;

	uint h = TileHeight(t);
	h = max(h, TileHeight(t + TileDiffXY(1, 0)));
	h = max(h, TileHeight(t + TileDiffXY(0, 1)));
	h = max(h, TileHeight(t + TileDiffXY(1, 1)));

	return h * TILE_HEIGHT;
}
