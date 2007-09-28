/* $Id$ */

/** @file industry_map.h Accessors for industries */

#ifndef INDUSTRY_MAP_H
#define INDUSTRY_MAP_H

#include "industry.h"
#include "macros.h"
#include "tile.h"



/**
 * The following enums are indices used to know what to draw for this industry tile.
 * They all are pointing toward array _industry_draw_tile_data, in table/industry_land.h
 * How to calculate the correct position ? GFXid << 2 | IndustryStage (0 to 3)
 */
enum {
	GFX_COAL_MINE_TOWER_NOT_ANIMATED   =   0,
	GFX_COAL_MINE_TOWER_ANIMATED       =   1,
	GFX_POWERPLANT_CHIMNEY             =   8,
	GFX_POWERPLANT_SPARKS              =  10,
	GFX_OILRIG_1                       =  24,
	GFX_OILRIG_2                       =  25,
	GFX_OILRIG_3                       =  26,
	GFX_OILRIG_4                       =  27,
	GFX_OILRIG_5                       =  28,
	GFX_OILWELL_NOT_ANIMATED           =  29,
	GFX_OILWELL_ANIMATED_1             =  30,
	GFX_OILWELL_ANIMATED_2             =  31,
	GFX_OILWELL_ANIMATED_3             =  32,
	GFX_COPPER_MINE_TOWER_NOT_ANIMATED =  47,
	GFX_COPPER_MINE_TOWER_ANIMATED     =  48,
	GFX_COPPER_MINE_CHIMNEY            =  49,
	GFX_GOLD_MINE_TOWER_NOT_ANIMATED   =  79,
	GFX_GOLD_MINE_TOWER_ANIMATED       =  88,
	GFX_TOY_FACTORY                    = 143,
	GFX_PLASTIC_FOUNTAIN_ANIMATED_1    = 148,
	GFX_PLASTIC_FOUNTAIN_ANIMATED_2    = 149,
	GFX_PLASTIC_FOUNTAIN_ANIMATED_3    = 150,
	GFX_PLASTIC_FOUNTAIN_ANIMATED_4    = 151,
	GFX_PLASTIC_FOUNTAIN_ANIMATED_5    = 152,
	GFX_PLASTIC_FOUNTAIN_ANIMATED_6    = 153,
	GFX_PLASTIC_FOUNTAIN_ANIMATED_7    = 154,
	GFX_PLASTIC_FOUNTAIN_ANIMATED_8    = 155,
	GFX_BUBBLE_GENERATOR               = 161,
	GFX_BUBBLE_CATCHER                 = 162,
	GFX_TOFFEE_QUARY                   = 165,
	GFX_SUGAR_MINE_SIEVE               = 174,
	GFX_WATERTILE_SPECIALCHECK         = 255,  ///< not really a tile, but rather a very special check
};

/**
 * Get the industry ID of the given tile
 * @param t the tile to get the industry ID from
 * @pre IsTileType(t, MP_INDUSTRY)
 * @return the industry ID
 */
static inline IndustryID GetIndustryIndex(TileIndex t)
{
	assert(IsTileType(t, MP_INDUSTRY));
	return _m[t].m2;
}

/**
 * Get the industry of the given tile
 * @param t the tile to get the industry from
 * @pre IsTileType(t, MP_INDUSTRY)
 * @return the industry
 */
static inline Industry *GetIndustryByTile(TileIndex t)
{
	return GetIndustry(GetIndustryIndex(t));
}

/**
 * Is this industry tile fully built?
 * @param t the tile to analyze
 * @pre IsTileType(t, MP_INDUSTRY)
 * @return true if and only if the industry tile is fully built
 */
static inline bool IsIndustryCompleted(TileIndex t)
{
	assert(IsTileType(t, MP_INDUSTRY));
	return HASBIT(_m[t].m1, 7);
}

IndustryType GetIndustryType(TileIndex tile);

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
	return IsIndustryCompleted(tile) ? (byte)INDUSTRY_COMPLETED : GB(_m[tile].m1, 0, 2);
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

static inline IndustryGfx GetCleanIndustryGfx(TileIndex t)
{
	assert(IsTileType(t, MP_INDUSTRY));
	return _m[t].m5 | (GB(_m[t].m6, 2, 1) << 8);
}

/**
 * Get the industry graphics ID for the given industry tile
 * @param t the tile to get the gfx for
 * @pre IsTileType(t, MP_INDUSTRY)
 * @return the gfx ID
 */
static inline IndustryGfx GetIndustryGfx(TileIndex t)
{
	assert(IsTileType(t, MP_INDUSTRY));
	return GetTranslatedIndustryTileID(GetCleanIndustryGfx(t));
}

/**
 * Set the industry graphics ID for the given industry tile
 * @param t   the tile to set the gfx for
 * @pre IsTileType(t, MP_INDUSTRY)
 * @param gfx the graphics ID
 */
static inline void SetIndustryGfx(TileIndex t, IndustryGfx gfx)
{
	assert(IsTileType(t, MP_INDUSTRY));
	_m[t].m5 = GB(gfx, 0, 8);
	SB(_m[t].m6, 2, 1, GB(gfx, 8, 1));
}

/**
 * Make the given tile an industry tile
 * @param t     the tile to make an industry tile
 * @param index the industry this tile belongs to
 * @param gfx   the graphics to use for the tile
 */
static inline void MakeIndustry(TileIndex t, IndustryID index, IndustryGfx gfx)
{
	SetTileType(t, MP_INDUSTRY);
	_m[t].m1 = 0;
	_m[t].m2 = index;
	_m[t].m3 = 0;
	_m[t].m4 = 0;
	SetIndustryGfx(t, gfx);
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
 * @pre IsTileType(tile, MP_INDUSTRY)
 */
static inline void ResetIndustryConstructionStage(TileIndex tile)
{
	assert(IsTileType(tile, MP_INDUSTRY));
	_m[tile].m1 = 0;
}

/**
 * Get the animation loop number
 * @param tile the tile to get the animation loop number of
 * @pre IsTileType(tile, MP_INDUSTRY)
 */
static inline byte GetIndustryAnimationLoop(TileIndex tile)
{
	assert(IsTileType(tile, MP_INDUSTRY));
	return _m[tile].m4;
}

/**
 * Set the animation loop number
 * @param tile the tile to set the animation loop number of
 * @param count the new animation frame number
 * @pre IsTileType(tile, MP_INDUSTRY)
 */
static inline void SetIndustryAnimationLoop(TileIndex tile, byte count)
{
	assert(IsTileType(tile, MP_INDUSTRY));
	_m[tile].m4 = count;
}

/**
 * Get the animation state
 * @param tile the tile to get the animation state of
 * @pre IsTileType(tile, MP_INDUSTRY)
 */
static inline byte GetIndustryAnimationState(TileIndex tile)
{
	assert(IsTileType(tile, MP_INDUSTRY));
	return _m[tile].m3;
}

/**
 * Set the animation state
 * @param tile the tile to set the animation state of
 * @param state the new animation state
 * @pre IsTileType(tile, MP_INDUSTRY)
 */
static inline void SetIndustryAnimationState(TileIndex tile, byte state)
{
	assert(IsTileType(tile, MP_INDUSTRY));
	_m[tile].m3 = state;
}

/**
 * Get the random bits for this tile.
 * Used for grf callbacks
 * @param tile TileIndex of the tile to query
 * @pre IsTileType(tile, MP_INDUSTRY)
 * @return requested bits
 */
static inline byte GetIndustryRandomBits(TileIndex tile)
{
	assert(IsTileType(tile, MP_INDUSTRY));
	return _me[tile].m7;
}

/**
 * Set the random bits for this tile.
 * Used for grf callbacks
 * @param tile TileIndex of the tile to query
 * @param bits the random bits
 * @pre IsTileType(tile, MP_INDUSTRY)
 */
static inline byte GetIndustryRandomBits(TileIndex tile, byte bits)
{
	assert(IsTileType(tile, MP_INDUSTRY));
	_me[tile].m7 = bits;
}

/**
 * Get the activated triggers bits for this industry tile
 * Used for grf callbacks
 * @param tile TileIndex of the tile to query
 * @pre IsTileType(tile, MP_INDUSTRY)
 * @return requested triggers
 */
static inline byte GetIndustryTriggers(TileIndex tile)
{
	assert(IsTileType(tile, MP_INDUSTRY));
	return GB(_m[tile].m6, 3, 3);
}


/**
 * Set the activated triggers bits for this industry tile
 * Used for grf callbacks
 * @param tile TileIndex of the tile to query
 * @param triggers the triggers to set
 * @pre IsTileType(tile, MP_INDUSTRY)
 */
static inline void SetIndustryTriggers(TileIndex tile, byte triggers)
{
	assert(IsTileType(tile, MP_INDUSTRY));
	SB(_m[tile].m6, 3, 3, triggers);
}

#endif /* INDUSTRY_MAP_H */
