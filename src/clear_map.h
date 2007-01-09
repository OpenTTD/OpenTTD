/* $Id$ */

#ifndef CLEAR_MAP_H
#define CLEAR_MAP_H

#include "macros.h"
#include "tile.h"
#include "bridge_map.h"

/* ground type, m5 bits 2...4
 * valid densities (bits 0...1) in comments after the enum
 */
typedef enum ClearGround {
	CLEAR_GRASS  = 0, // 0-3
	CLEAR_ROUGH  = 1, // 3
	CLEAR_ROCKS  = 2, // 3
	CLEAR_FIELDS = 3, // 3
	CLEAR_SNOW   = 4, // 0-3
	CLEAR_DESERT = 5  // 1,3
} ClearGround;


static inline ClearGround GetClearGround(TileIndex t)
{
	assert(IsTileType(t, MP_CLEAR));
	return GB(_m[t].m5, 2, 3);
}

static inline bool IsClearGround(TileIndex t, ClearGround ct)
{
	return GetClearGround(t) == ct;
}


static inline uint GetClearDensity(TileIndex t)
{
	assert(IsTileType(t, MP_CLEAR));
	return GB(_m[t].m5, 0, 2);
}

static inline void AddClearDensity(TileIndex t, int d)
{
	assert(IsTileType(t, MP_CLEAR)); // XXX incomplete
	_m[t].m5 += d;
}


static inline uint GetClearCounter(TileIndex t)
{
	assert(IsTileType(t, MP_CLEAR));
	return GB(_m[t].m5, 5, 3);
}

static inline void AddClearCounter(TileIndex t, int c)
{
	assert(IsTileType(t, MP_CLEAR)); // XXX incomplete
	_m[t].m5 += c << 5;
}

static inline void SetClearCounter(TileIndex t, uint c)
{
	assert(IsTileType(t, MP_CLEAR)); // XXX incomplete
	SB(_m[t].m5, 5, 3, c);
}


/* Sets type and density in one go, also sets the counter to 0 */
static inline void SetClearGroundDensity(TileIndex t, ClearGround type, uint density)
{
	assert(IsTileType(t, MP_CLEAR)); // XXX incomplete
	_m[t].m5 = 0 << 5 | type << 2 | density;
}


static inline uint GetFieldType(TileIndex t)
{
	assert(GetClearGround(t) == CLEAR_FIELDS);
	return GB(_m[t].m3, 0, 4);
}

static inline void SetFieldType(TileIndex t, uint f)
{
	assert(GetClearGround(t) == CLEAR_FIELDS); // XXX incomplete
	SB(_m[t].m3, 0, 4, f);
}

static inline uint16 GetIndustryIndexOfField(TileIndex t)
{
	assert(GetClearGround(t) == CLEAR_FIELDS);
	return _m[t].m2;
}

static inline void SetIndustryIndexOfField(TileIndex t, uint16 i)
{
	assert(GetClearGround(t) == CLEAR_FIELDS);
	_m[t].m2 = i;
}

/* Is used by tree tiles, too */
static inline uint GetFenceSE(TileIndex t)
{
	assert(IsTileType(t, MP_CLEAR) || IsTileType(t, MP_TREES));
	return GB(_m[t].m4, 2, 3);
}

static inline void SetFenceSE(TileIndex t, uint h)
{
	assert(IsTileType(t, MP_CLEAR) || IsTileType(t, MP_TREES)); // XXX incomplete
	SB(_m[t].m4, 2, 3, h);
}

static inline uint GetFenceSW(TileIndex t)
{
	assert(IsTileType(t, MP_CLEAR) || IsTileType(t, MP_TREES));
	return GB(_m[t].m4, 5, 3);
}

static inline void SetFenceSW(TileIndex t, uint h)
{
	assert(IsTileType(t, MP_CLEAR) || IsTileType(t, MP_TREES)); // XXX incomplete
	SB(_m[t].m4, 5, 3, h);
}


static inline void MakeClear(TileIndex t, ClearGround g, uint density)
{
	/* If this is a non-bridgeable tile, clear the bridge bits while the rest
	 * of the tile information is still here. */
	if (!MayHaveBridgeAbove(t)) SB(_m[t].extra, 6, 2, 0);

	SetTileType(t, MP_CLEAR);
	SetTileOwner(t, OWNER_NONE);
	_m[t].m2 = 0;
	_m[t].m3 = 0;
	_m[t].m4 = 0 << 5 | 0 << 2;
	SetClearGroundDensity(t, g, density);
	SB(_m[t].extra, 2, 4, 0);
}


static inline void MakeField(TileIndex t, uint field_type, uint16 industry)
{
	SetTileType(t, MP_CLEAR);
	SetTileOwner(t, OWNER_NONE);
	_m[t].m2 = industry;
	_m[t].m3 = field_type;
	_m[t].m4 = 0 << 5 | 0 << 2;
	SetClearGroundDensity(t, CLEAR_FIELDS, 3);
}

#endif /* CLEAR_MAP_H */
