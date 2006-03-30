/* $Id$ */

#include "industry.h"
#include "macros.h"
#include "tile.h"


static inline uint GetIndustryIndex(TileIndex t)
{
	assert(IsTileType(t, MP_INDUSTRY));
	return _m[t].m2;
}

static inline Industry* GetIndustryByTile(TileIndex t)
{
	return GetIndustry(GetIndustryIndex(t));
}


static inline bool IsIndustryCompleted(TileIndex t)
{
	assert(IsTileType(t, MP_INDUSTRY));
	return HASBIT(_m[t].m1, 7);
}


static inline uint GetIndustryGfx(TileIndex t)
{
	assert(IsTileType(t, MP_INDUSTRY));
	return _m[t].m5;
}

static inline void SetIndustryGfx(TileIndex t, uint gfx)
{
	assert(IsTileType(t, MP_INDUSTRY));
	_m[t].m5 = gfx;
}


static inline void MakeIndustry(TileIndex t, uint index, uint gfx)
{
	SetTileType(t, MP_INDUSTRY);
	_m[t].m1 = 0;
	_m[t].m2 = index;
	_m[t].m3 = 0;
	_m[t].m4 = 0;
	_m[t].m5 = gfx;
}
