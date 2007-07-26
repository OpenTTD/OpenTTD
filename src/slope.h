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

/**
 * Returns the highest corner of a slope (one corner raised or a steep slope).
 *
 * @pre      The slope must be a slope with one corner raised or a steep slope.
 * @param s  The #Slope.
 * @return   Number of the highest corner. (0 west, 1 south, 2 east, 3 north)
 */
static inline byte GetHighestSlopeCorner(Slope s)
{
	switch (s) {
		case SLOPE_W:
		case SLOPE_STEEP_W: return 0;
		case SLOPE_S:
		case SLOPE_STEEP_S: return 1;
		case SLOPE_E:
		case SLOPE_STEEP_E: return 2;
		case SLOPE_N:
		case SLOPE_STEEP_N: return 3;
		default: NOT_REACHED();
	}
}


/**
 * Enumeration for Foundations.
 */
enum Foundation {
	FOUNDATION_NONE,             ///< The tile has no foundation, the slope remains unchanged.
	FOUNDATION_LEVELED,          ///< The tile is leveled up to a flat slope.
	FOUNDATION_INCLINED_X,       ///< The tile has an along X-axis inclined foundation.
	FOUNDATION_INCLINED_Y,       ///< The tile has an along Y-axis inclined foundation.
	FOUNDATION_STEEP_LOWER,      ///< The tile has a steep slope. The lowerst corner is raised by a foundation to allow building railroad on the lower halftile.
	FOUNDATION_STEEP_HIGHER,     ///< The tile has a steep slope. Three corners are raised by a foundation to allow building railroad on the higher halftile.
};

/**
 * Tests for FOUNDATION_NONE.
 *
 * @param f  Maybe a #Foundation.
 * @return   true iff f is a foundation.
 */
static inline bool IsFoundation(Foundation f)
{
	return f != FOUNDATION_NONE;
}

/**
 * Tests if the foundation is a leveled foundation.
 *
 * @param f  The #Foundation.
 * @return   true iff f is a leveled foundation.
 */
static inline bool IsLeveledFoundation(Foundation f)
{
	return f == FOUNDATION_LEVELED;
}

/**
 * Tests if the foundation is an inclined foundation.
 *
 * @param f  The #Foundation.
 * @return   true iff f is an inclined foundation.
 */
static inline bool IsInclinedFoundation(Foundation f)
{
	return (f == FOUNDATION_INCLINED_X) || (f == FOUNDATION_INCLINED_Y);
}

/**
 * Returns the foundation needed to flatten a slope.
 * The returned foundation is either FOUNDATION_NONE if the tile was already flat, or FOUNDATION_LEVELED.
 *
 * @pre      The slope must not be steep.
 * @param s  The current #Slope.
 * @return   The needed #Foundation.
 */
static inline Foundation FlatteningFoundation(Slope s)
{
	assert(!IsSteepSlope(s));
	return (s == SLOPE_FLAT ? FOUNDATION_NONE : FOUNDATION_LEVELED);
}

/**
 * Returns the along a specific axis inclined foundation.
 *
 * @param axis  The #Axis.
 * @return      The needed #Foundation.
 */
static inline Foundation InclinedFoundation(Axis axis)
{
	return (axis == AXIS_X ? FOUNDATION_INCLINED_X : FOUNDATION_INCLINED_Y);
}

#endif /* SLOPE_H */
