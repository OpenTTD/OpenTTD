#ifndef TILE_H
#define TILE_H

#include "macros.h"
#include "map.h"

typedef enum TileType {
	MP_CLEAR,
	MP_RAILWAY,
	MP_STREET,
	MP_HOUSE,
	MP_TREES,
	MP_STATION,
	MP_WATER,
	MP_VOID, // invisible tiles at the SW and SE border
	MP_INDUSTRY,
	MP_TUNNELBRIDGE,
	MP_UNMOVABLE
} TileType;

void SetMapExtraBits(TileIndex tile, byte flags);
uint GetMapExtraBits(TileIndex tile);

uint GetTileSlope(TileIndex tile, uint *h);
uint GetTileZ(TileIndex tile);

static inline bool CorrectZ(uint tileh)
{
	/* tile height must be corrected if the north corner is not raised, but
	 * any other corner is. These are the cases 1 till 7 */
	return IS_INT_INSIDE(tileh, 1, 8);
}

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

static inline TileType GetTileType(TileIndex tile)
{
	assert(tile < MapSize());
	return _map_type_and_height[tile] >> 4;
}

static inline void SetTileType(TileIndex tile, TileType type)
{
	assert(tile < MapSize());
	_map_type_and_height[tile] &= ~0xF0;
	_map_type_and_height[tile] |= type << 4;
}

static inline bool IsTileType(TileIndex tile, TileType type)
{
	return GetTileType(tile) == type;
}

static inline Owner GetTileOwner(TileIndex tile)
{
	assert(tile < MapSize());
	return _map_owner[tile];
}

static inline bool IsTileOwner(TileIndex tile, Owner owner)
{
	return GetTileOwner(tile) == owner;
}

#endif
