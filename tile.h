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
	/* Allow only MP_VOID to be set to border tiles. This code is put here since
	 * it seems there is a bug that violates this somewhere. (Formely know as
	 * the "old ship pf" bug, which presented a case in which this broke). It
	 * can be removed as soon as  the bug is squashed. */
	assert((TileX(tile) < MapMaxX() && TileY(tile) < MapMaxY()) || type == MP_VOID);
	SB(_m[tile].type_height, 4, 4, type);
}

static inline bool IsTileType(TileIndex tile, TileType type)
{
	return GetTileType(tile) == type;
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
