/* $Id$ */

/** @file clear_map.h Map accessors for 'clear' tiles */

#ifndef CLEAR_MAP_H
#define CLEAR_MAP_H

#include "bridge_map.h"

/**
 * Ground types. Valid densities in comments after the enum.
 */
enum ClearGround {
	CLEAR_GRASS  = 0, ///< 0-3
	CLEAR_ROUGH  = 1, ///< 3
	CLEAR_ROCKS  = 2, ///< 3
	CLEAR_FIELDS = 3, ///< 3
	CLEAR_SNOW   = 4, ///< 0-3
	CLEAR_DESERT = 5  ///< 1,3
};


/**
 * Get the type of clear tile.
 * @param t the tile to get the clear ground type of
 * @pre IsTileType(t, MP_CLEAR)
 * @return the ground type
 */
static inline ClearGround GetClearGround(TileIndex t)
{
	assert(IsTileType(t, MP_CLEAR));
	return (ClearGround)GB(_m[t].m5, 2, 3);
}

/**
 * Set the type of clear tile.
 * @param t  the tile to set the clear ground type of
 * @param ct the ground type
 * @pre IsTileType(t, MP_CLEAR)
 */
static inline bool IsClearGround(TileIndex t, ClearGround ct)
{
	return GetClearGround(t) == ct;
}


/**
 * Get the density of a non-field clear tile.
 * @param t the tile to get the density of
 * @pre IsTileType(t, MP_CLEAR)
 * @return the density
 */
static inline uint GetClearDensity(TileIndex t)
{
	assert(IsTileType(t, MP_CLEAR));
	return GB(_m[t].m5, 0, 2);
}

/**
 * Increment the density of a non-field clear tile.
 * @param t the tile to increment the density of
 * @param d the amount to increment the density with
 * @pre IsTileType(t, MP_CLEAR)
 */
static inline void AddClearDensity(TileIndex t, int d)
{
	assert(IsTileType(t, MP_CLEAR)); // XXX incomplete
	_m[t].m5 += d;
}


/**
 * Get the counter used to advance to the next clear density/field type.
 * @param t the tile to get the counter of
 * @pre IsTileType(t, MP_CLEAR)
 * @return the value of the counter
 */
static inline uint GetClearCounter(TileIndex t)
{
	assert(IsTileType(t, MP_CLEAR));
	return GB(_m[t].m5, 5, 3);
}

/**
 * Increments the counter used to advance to the next clear density/field type.
 * @param t the tile to increment the counter of
 * @param c the amount to increment the counter with
 * @pre IsTileType(t, MP_CLEAR)
 */
static inline void AddClearCounter(TileIndex t, int c)
{
	assert(IsTileType(t, MP_CLEAR)); // XXX incomplete
	_m[t].m5 += c << 5;
}

/**
 * Sets the counter used to advance to the next clear density/field type.
 * @param t the tile to set the counter of
 * @param c the amount to set the counter to
 * @pre IsTileType(t, MP_CLEAR)
 */
static inline void SetClearCounter(TileIndex t, uint c)
{
	assert(IsTileType(t, MP_CLEAR)); // XXX incomplete
	SB(_m[t].m5, 5, 3, c);
}


/**
 * Sets ground type and density in one go, also sets the counter to 0
 * @param t       the tile to set the ground type and density for
 * @param type    the new ground type of the tile
 * @param density the density of the ground tile
 * @pre IsTileType(t, MP_CLEAR)
 */
static inline void SetClearGroundDensity(TileIndex t, ClearGround type, uint density)
{
	assert(IsTileType(t, MP_CLEAR)); // XXX incomplete
	_m[t].m5 = 0 << 5 | type << 2 | density;
}


/**
 * Get the field type (production stage) of the field
 * @param t the field to get the type of
 * @pre GetClearGround(t) == CLEAR_FIELDS
 * @return the field type
 */
static inline uint GetFieldType(TileIndex t)
{
	assert(GetClearGround(t) == CLEAR_FIELDS);
	return GB(_m[t].m3, 0, 4);
}

/**
 * Set the field type (production stage) of the field
 * @param t the field to get the type of
 * @param f the field type
 * @pre GetClearGround(t) == CLEAR_FIELDS
 */
static inline void SetFieldType(TileIndex t, uint f)
{
	assert(GetClearGround(t) == CLEAR_FIELDS); // XXX incomplete
	SB(_m[t].m3, 0, 4, f);
}

/**
 * Get the industry (farm) that made the field
 * @param t the field to get creating industry of
 * @pre GetClearGround(t) == CLEAR_FIELDS
 * @return the industry that made the field
 */
static inline IndustryID GetIndustryIndexOfField(TileIndex t)
{
	assert(GetClearGround(t) == CLEAR_FIELDS);
	return(IndustryID) _m[t].m2;
}

/**
 * Set the industry (farm) that made the field
 * @param t the field to get creating industry of
 * @param i the industry that made the field
 * @pre GetClearGround(t) == CLEAR_FIELDS
 */
static inline void SetIndustryIndexOfField(TileIndex t, IndustryID i)
{
	assert(GetClearGround(t) == CLEAR_FIELDS);
	_m[t].m2 = i;
}


/**
 * Is there a fence at the south eastern border?
 * @param t the tile to check for fences
 * @pre IsTileType(t, MP_CLEAR) || IsTileType(t, MP_TREES)
 * @return 0 if there is no fence, otherwise the fence type
 */
static inline uint GetFenceSE(TileIndex t)
{
	assert(IsTileType(t, MP_CLEAR) || IsTileType(t, MP_TREES));
	return GB(_m[t].m4, 2, 3);
}

/**
 * Sets the type of fence (and whether there is one) for the south
 * eastern border.
 * @param t the tile to check for fences
 * @param h 0 if there is no fence, otherwise the fence type
 * @pre IsTileType(t, MP_CLEAR) || IsTileType(t, MP_TREES)
 */
static inline void SetFenceSE(TileIndex t, uint h)
{
	assert(IsTileType(t, MP_CLEAR) || IsTileType(t, MP_TREES)); // XXX incomplete
	SB(_m[t].m4, 2, 3, h);
}

/**
 * Is there a fence at the south western border?
 * @param t the tile to check for fences
 * @pre IsTileType(t, MP_CLEAR) || IsTileType(t, MP_TREES)
 * @return 0 if there is no fence, otherwise the fence type
 */
static inline uint GetFenceSW(TileIndex t)
{
	assert(IsTileType(t, MP_CLEAR) || IsTileType(t, MP_TREES));
	return GB(_m[t].m4, 5, 3);
}

/**
 * Sets the type of fence (and whether there is one) for the south
 * western border.
 * @param t the tile to check for fences
 * @param h 0 if there is no fence, otherwise the fence type
 * @pre IsTileType(t, MP_CLEAR) || IsTileType(t, MP_TREES)
 */
static inline void SetFenceSW(TileIndex t, uint h)
{
	assert(IsTileType(t, MP_CLEAR) || IsTileType(t, MP_TREES)); // XXX incomplete
	SB(_m[t].m4, 5, 3, h);
}


/**
 * Make a clear tile.
 * @param t       the tile to make a clear tile
 * @param g       the type of ground
 * @param density the density of the grass/snow/desert etc
 */
static inline void MakeClear(TileIndex t, ClearGround g, uint density)
{
	/* If this is a non-bridgeable tile, clear the bridge bits while the rest
	 * of the tile information is still here. */
	if (!MayHaveBridgeAbove(t)) SB(_m[t].m6, 6, 2, 0);

	SetTileType(t, MP_CLEAR);
	SetTileOwner(t, OWNER_NONE);
	_m[t].m2 = 0;
	_m[t].m3 = 0;
	_m[t].m4 = 0 << 5 | 0 << 2;
	SetClearGroundDensity(t, g, density);
	SB(_m[t].m6, 2, 4, 0); // Clear the rest of m6, bits 2 to 5
}


/**
 * Make a (farm) field tile.
 * @param t          the tile to make a farm field
 * @param field_type the 'growth' level of the field
 * @param industry   the industry this tile belongs to
 */
static inline void MakeField(TileIndex t, uint field_type, IndustryID industry)
{
	SetTileType(t, MP_CLEAR);
	SetTileOwner(t, OWNER_NONE);
	_m[t].m2 = industry;
	_m[t].m3 = field_type;
	_m[t].m4 = 0 << 5 | 0 << 2;
	SetClearGroundDensity(t, CLEAR_FIELDS, 3);
}

#endif /* CLEAR_MAP_H */
