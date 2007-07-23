/* $Id$ */

/** @file slope.h Definitions of a slope.
 * This file defines the enumeration and helper functions for handling
 * the slope info of a tile.
 */

#ifndef SLOPE_H
#define SLOPE_H

/**
 * Enumeration for the slope-type.
 *
 * This enumeration use the chars N,E,S,W corresponding the
 * direction north, east, south and west. The top corner of a tile
 * is the north-part of the tile. The whole slope is encoded with
 * 5 bits, 4 bits for each corner and 1 bit for a steep-flag.
 */
enum Slope {
	SLOPE_FLAT     = 0x00,                                  ///< a flat tile
	SLOPE_W        = 0x01,                                  ///< the west corner of the tile is raised
	SLOPE_S        = 0x02,                                  ///< the south corner of the tile is raised
	SLOPE_E        = 0x04,                                  ///< the east corner of the tile is raised
	SLOPE_N        = 0x08,                                  ///< the north corner of the tile is raised
	SLOPE_STEEP    = 0x10,                                  ///< indicates the slope is steep
	SLOPE_NW       = SLOPE_N | SLOPE_W,                     ///< north and west corner are raised
	SLOPE_SW       = SLOPE_S | SLOPE_W,                     ///< south and west corner are raised
	SLOPE_SE       = SLOPE_S | SLOPE_E,                     ///< south and east corner are raised
	SLOPE_NE       = SLOPE_N | SLOPE_E,                     ///< north and east corner are raised
	SLOPE_EW       = SLOPE_E | SLOPE_W,                     ///< east and west corner are raised
	SLOPE_NS       = SLOPE_N | SLOPE_S,                     ///< north and south corner are raised
	SLOPE_ELEVATED = SLOPE_N | SLOPE_E | SLOPE_S | SLOPE_W, ///< all corner are raised, similar to SLOPE_FLAT
	SLOPE_NWS      = SLOPE_N | SLOPE_W | SLOPE_S,           ///< north, west and south corner are raised
	SLOPE_WSE      = SLOPE_W | SLOPE_S | SLOPE_E,           ///< west, south and east corner are raised
	SLOPE_SEN      = SLOPE_S | SLOPE_E | SLOPE_N,           ///< south, east and north corner are raised
	SLOPE_ENW      = SLOPE_E | SLOPE_N | SLOPE_W,           ///< east, north and west corner are raised
	SLOPE_STEEP_W  = SLOPE_STEEP | SLOPE_NWS,               ///< a steep slope falling to east (from west)
	SLOPE_STEEP_S  = SLOPE_STEEP | SLOPE_WSE,               ///< a steep slope falling to north (from south)
	SLOPE_STEEP_E  = SLOPE_STEEP | SLOPE_SEN,               ///< a steep slope falling to west (from east)
	SLOPE_STEEP_N  = SLOPE_STEEP | SLOPE_ENW                ///< a steep slope falling to south (from north)
};

/**
 * Checks if a slope is steep.
 *
 * @param s The given #Slope.
 * @return True if the slope is steep, else false.
 */
static inline bool IsSteepSlope(Slope s)
{
	return (s & SLOPE_STEEP) != 0;
}

/**
 * Return the complement of a slope.
 *
 * This method returns the complement of a slope. The complement of a
 * slope is a slope with raised corner which aren't raised in the given
 * slope.
 *
 * @pre The slope must not be steep.
 * @param s The #Slope to get the complement.
 * @return a complement Slope of the given slope.
 */
static inline Slope ComplementSlope(Slope s)
{
	assert(!IsSteepSlope(s));
	return (Slope)(0xF ^ s);
}

#endif /* SLOPE_H */
