/* $Id$ */

/** @file industry_map.h Accessors for industries */

#ifndef INDUSTRY_MAP_H
#define INDUSTRY_MAP_H

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

/**
 * Set if the industry that owns the tile as under construction or not
 * @param tile the tile to query
 * @param isCompleted whether it is completed or not
 * @pre IsTileType(tile, MP_INDUSTRY)
 */
static inline void SetIndustryCompleted(TileIndex tile, bool isCompleted)
{
	assert(IsTileType(tile, MP_INDUSTRY));
	SB(_m[tile].m1, 7, 1, isCompleted ? 1 :0);
}

/**
 * Returns the industry construction stage of the specified tile
 * @param tile the tile to query
 * @pre IsTileType(tile, MP_INDUSTRY)
 * @return the construction stage
 */
static inline byte GetIndustryConstructionStage(TileIndex tile)
{
	assert(IsTileType(tile, MP_INDUSTRY));
	return GB(_m[tile].m1, 0, 2);
}

/**
 * Sets the industry construction stage of the specified tile
 * @param tile the tile to query
 * @param value the new construction stage
 * @pre IsTileType(tile, MP_INDUSTRY)
 */
static inline void SetIndustryConstructionStage(TileIndex tile, byte value)
{
	assert(IsTileType(tile, MP_INDUSTRY));
	SB(_m[tile].m1, 0, 2, value);
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

/**
 * Returns this indutry tile's construction counter value
 * @param tile the tile to query
 * @pre IsTileType(tile, MP_INDUSTRY)
 * @return the construction counter
 */
static inline byte GetIndustryConstructionCounter(TileIndex tile)
{
	assert(IsTileType(tile, MP_INDUSTRY));
	return GB(_m[tile].m1, 2, 2);
}

/**
 * Sets this indutry tile's construction counter value
 * @param tile the tile to query
 * @param value the new value for the construction counter
 * @pre IsTileType(tile, MP_INDUSTRY)
 */
static inline void SetIndustryConstructionCounter(TileIndex tile, byte value)
{
	assert(IsTileType(tile, MP_INDUSTRY));
	SB(_m[tile].m1, 2, 2, value);
}

/**
 * Reset the construction stage counter of the industry,
 * as well as the completion bit.
 * In fact, it is the same as restarting construction frmo ground up
 * @param tile the tile to query
 * @param generating_world whether generating a world or not
 * @pre IsTileType(tile, MP_INDUSTRY)
 */
static inline void ResetIndustryConstructionStage(TileIndex tile)
{
	assert(IsTileType(tile, MP_INDUSTRY));
	_m[tile].m1 = 0;
}

#endif /* INDUSTRY_MAP_H */
