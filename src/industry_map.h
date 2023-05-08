/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file industry_map.h Accessors for industries */

#ifndef INDUSTRY_MAP_H
#define INDUSTRY_MAP_H

#include "industrytype.h"
#include "water_map.h"


/**
 * The following enums are indices used to know what to draw for this industry tile.
 * They all are pointing toward array _industry_draw_tile_data, in table/industry_land.h
 * How to calculate the correct position ? GFXid << 2 | IndustryStage (0 to 3)
 */
enum IndustryGraphics {
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
static inline IndustryID GetIndustryIndex(Tile t)
{
	assert(IsTileType(t, MP_INDUSTRY));
	return t.m2();
}

/**
 * Is this industry tile fully built?
 * @param t the tile to analyze
 * @pre IsTileType(t, MP_INDUSTRY)
 * @return true if and only if the industry tile is fully built
 */
static inline bool IsIndustryCompleted(Tile t)
{
	assert(IsTileType(t, MP_INDUSTRY));
	return HasBit(t.m1(), 7);
}

IndustryType GetIndustryType(Tile tile);

/**
 * Set if the industry that owns the tile as under construction or not
 * @param tile the tile to query
 * @pre IsTileType(tile, MP_INDUSTRY)
 */
static inline void SetIndustryCompleted(Tile tile)
{
	assert(IsTileType(tile, MP_INDUSTRY));
	SB(tile.m1(), 7, 1, 1);
}

/**
 * Returns the industry construction stage of the specified tile
 * @param tile the tile to query
 * @pre IsTileType(tile, MP_INDUSTRY)
 * @return the construction stage
 */
static inline byte GetIndustryConstructionStage(Tile tile)
{
	assert(IsTileType(tile, MP_INDUSTRY));
	return IsIndustryCompleted(tile) ? (byte)INDUSTRY_COMPLETED : GB(tile.m1(), 0, 2);
}

/**
 * Sets the industry construction stage of the specified tile
 * @param tile the tile to query
 * @param value the new construction stage
 * @pre IsTileType(tile, MP_INDUSTRY)
 */
static inline void SetIndustryConstructionStage(Tile tile, byte value)
{
	assert(IsTileType(tile, MP_INDUSTRY));
	SB(tile.m1(), 0, 2, value);
}

/**
 * Get the industry graphics ID for the given industry tile as
 * stored in the without translation.
 * @param t the tile to get the gfx for
 * @pre IsTileType(t, MP_INDUSTRY)
 * @return the gfx ID
 */
static inline IndustryGfx GetCleanIndustryGfx(Tile t)
{
	assert(IsTileType(t, MP_INDUSTRY));
	return t.m5() | (GB(t.m6(), 2, 1) << 8);
}

/**
 * Get the industry graphics ID for the given industry tile
 * @param t the tile to get the gfx for
 * @pre IsTileType(t, MP_INDUSTRY)
 * @return the gfx ID
 */
static inline IndustryGfx GetIndustryGfx(Tile t)
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
static inline void SetIndustryGfx(Tile t, IndustryGfx gfx)
{
	assert(IsTileType(t, MP_INDUSTRY));
	t.m5() = GB(gfx, 0, 8);
	SB(t.m6(), 2, 1, GB(gfx, 8, 1));
}

/**
 * Returns this industry tile's construction counter value
 * @param tile the tile to query
 * @pre IsTileType(tile, MP_INDUSTRY)
 * @return the construction counter
 */
static inline byte GetIndustryConstructionCounter(Tile tile)
{
	assert(IsTileType(tile, MP_INDUSTRY));
	return GB(tile.m1(), 2, 2);
}

/**
 * Sets this industry tile's construction counter value
 * @param tile the tile to query
 * @param value the new value for the construction counter
 * @pre IsTileType(tile, MP_INDUSTRY)
 */
static inline void SetIndustryConstructionCounter(Tile tile, byte value)
{
	assert(IsTileType(tile, MP_INDUSTRY));
	SB(tile.m1(), 2, 2, value);
}

/**
 * Reset the construction stage counter of the industry,
 * as well as the completion bit.
 * In fact, it is the same as restarting construction frmo ground up
 * @param tile the tile to query
 * @pre IsTileType(tile, MP_INDUSTRY)
 */
static inline void ResetIndustryConstructionStage(Tile tile)
{
	assert(IsTileType(tile, MP_INDUSTRY));
	SB(tile.m1(), 0, 4, 0);
	SB(tile.m1(), 7, 1, 0);
}

/**
 * Get the animation loop number
 * @param tile the tile to get the animation loop number of
 * @pre IsTileType(tile, MP_INDUSTRY)
 */
static inline byte GetIndustryAnimationLoop(Tile tile)
{
	assert(IsTileType(tile, MP_INDUSTRY));
	return tile.m4();
}

/**
 * Set the animation loop number
 * @param tile the tile to set the animation loop number of
 * @param count the new animation frame number
 * @pre IsTileType(tile, MP_INDUSTRY)
 */
static inline void SetIndustryAnimationLoop(Tile tile, byte count)
{
	assert(IsTileType(tile, MP_INDUSTRY));
	tile.m4() = count;
}

/**
 * Get the random bits for this tile.
 * Used for grf callbacks
 * @param tile the tile to query
 * @pre IsTileType(tile, MP_INDUSTRY)
 * @return requested bits
 */
static inline byte GetIndustryRandomBits(Tile tile)
{
	assert(IsTileType(tile, MP_INDUSTRY));
	return tile.m3();
}

/**
 * Set the random bits for this tile.
 * Used for grf callbacks
 * @param tile the tile to query
 * @param bits the random bits
 * @pre IsTileType(tile, MP_INDUSTRY)
 */
static inline void SetIndustryRandomBits(Tile tile, byte bits)
{
	assert(IsTileType(tile, MP_INDUSTRY));
	tile.m3() = bits;
}

/**
 * Get the activated triggers bits for this industry tile
 * Used for grf callbacks
 * @param tile the tile to query
 * @pre IsTileType(tile, MP_INDUSTRY)
 * @return requested triggers
 */
static inline byte GetIndustryTriggers(Tile tile)
{
	assert(IsTileType(tile, MP_INDUSTRY));
	return GB(tile.m6(), 3, 3);
}


/**
 * Set the activated triggers bits for this industry tile
 * Used for grf callbacks
 * @param tile the tile to query
 * @param triggers the triggers to set
 * @pre IsTileType(tile, MP_INDUSTRY)
 */
static inline void SetIndustryTriggers(Tile tile, byte triggers)
{
	assert(IsTileType(tile, MP_INDUSTRY));
	SB(tile.m6(), 3, 3, triggers);
}

/**
 * Make the given tile an industry tile
 * @param t      the tile to make an industry tile
 * @param index  the industry this tile belongs to
 * @param gfx    the graphics to use for the tile
 * @param random the random value
 * @param wc     the water class for this industry; only useful when build on water
 */
static inline void MakeIndustry(Tile t, IndustryID index, IndustryGfx gfx, uint8_t random, WaterClass wc)
{
	SetTileType(t, MP_INDUSTRY);
	t.m1() = 0;
	t.m2() = index;
	SetIndustryRandomBits(t, random); // m3
	t.m4() = 0;
	SetIndustryGfx(t, gfx); // m5, part of m6
	SetIndustryTriggers(t, 0); // rest of m6
	SetWaterClass(t, wc);
	t.m7() = 0;
}

#endif /* INDUSTRY_MAP_H */
