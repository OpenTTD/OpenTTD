#ifndef TILE_H
#define TILE_H

#include "map.h"

static inline uint TileHeight(TileIndex tile)
{
	assert(tile < MapSize());
	return _map_type_and_height[tile] & 0xf;
}

static inline void SetTileHeight(TileIndex tile, uint height)
{
	assert(tile < MapSize());
	assert(height < 16);
	_map_type_and_height[tile] &= ~0x0F;
	_map_type_and_height[tile] |= height;
}

static inline uint TilePixelHeight(TileIndex tile)
{
	return TileHeight(tile) * 8;
}

static inline int TileType(TileIndex tile)
{
	assert(tile < MapSize());
	return _map_type_and_height[tile] >> 4;
}

static inline void SetTileType(TileIndex tile, uint type)
{
	assert(tile < MapSize());
	_map_type_and_height[tile] &= ~0xF0;
	_map_type_and_height[tile] |= type << 4;
}

static inline bool IsTileType(TileIndex tile, int type)
{
	return TileType(tile) == type;
}

#endif
