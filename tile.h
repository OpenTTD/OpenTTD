/* $Id$ */

#ifndef TILE_H
#define TILE_H

#include "macros.h"
#include "map.h"

typedef enum TileTypes {
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
	MP_UNMOVABLE,
} TileType;

/* Direction as commonly used in v->direction, 8 way. */
typedef enum Directions {
	DIR_N   = 0,
	DIR_NE  = 1,      /* Northeast, upper right on your monitor */
	DIR_E   = 2,
	DIR_SE  = 3,
	DIR_S   = 4,
	DIR_SW  = 5,
	DIR_W   = 6,
	DIR_NW  = 7,
	DIR_END,
	INVALID_DIR = 0xFF,
} Direction;

/* Direction commonly used as the direction of entering and leaving tiles, 4-way */
typedef enum DiagonalDirections {
	DIAGDIR_NE  = 0,      /* Northeast, upper right on your monitor */
	DIAGDIR_SE  = 1,
	DIAGDIR_SW  = 2,
	DIAGDIR_NW  = 3,
	DIAGDIR_END,
	INVALID_DIAGDIR = 0xFF,
} DiagDirection;

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
	return GB(_m[tile].type_height, 0, 4);
}

static inline bool IsSteepTileh(uint tileh)
{
	return (tileh & 0x10);
}

static inline void SetTileHeight(TileIndex tile, uint height)
{
	assert(tile < MapSize());
	assert(height < 16);
	SB(_m[tile].type_height, 0, 4, height);
}

static inline uint TilePixelHeight(TileIndex tile)
{
	return TileHeight(tile) * 8;
}

static inline TileType GetTileType(TileIndex tile)
{
	assert(tile < MapSize());
	return GB(_m[tile].type_height, 4, 4);
}

static inline void SetTileType(TileIndex tile, TileType type)
{
	assert(tile < MapSize());
	SB(_m[tile].type_height, 4, 4, type);
}

static inline bool IsTileType(TileIndex tile, TileType type)
{
	return GetTileType(tile) == type;
}

static inline bool IsTunnelTile(TileIndex tile)
{
	return IsTileType(tile, MP_TUNNELBRIDGE) && GB(_m[tile].m5, 4, 4) == 0;
}

static inline Owner GetTileOwner(TileIndex tile)
{
	assert(tile < MapSize());
	assert(!IsTileType(tile, MP_HOUSE));
	assert(!IsTileType(tile, MP_VOID));
	assert(!IsTileType(tile, MP_INDUSTRY));

	return _m[tile].m1;
}

static inline void SetTileOwner(TileIndex tile, Owner owner)
{
	assert(tile < MapSize());
	assert(!IsTileType(tile, MP_HOUSE));
	assert(!IsTileType(tile, MP_VOID));
	assert(!IsTileType(tile, MP_INDUSTRY));

	_m[tile].m1 = owner;
}

static inline bool IsTileOwner(TileIndex tile, Owner owner)
{
	return GetTileOwner(tile) == owner;
}

#endif /* TILE_H */
