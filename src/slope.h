/* $Id$ */

/** @file slope.h Definitions of a slope.
 * This file defines the enumeration and helper functions for handling
 * the slope info of a tile.
 */

#ifndef SLOPE_H
#define SLOPE_H

/**
 * Enumeration of tile corners
 */
enum Corner {
	CORNER_W = 0,
	CORNER_S = 1,
	CORNER_E = 2,
	CORNER_N = 3,
	CORNER_END
};

/**
 * Enumeration for the slope-type.
 *
 * This enumeration use the chars N,E,S,W corresponding the
 * direction north, east, south and west. The top corner of a tile
 * is the north-part of the tile. The whole slope is encoded with
 * 5 bits, 4 bits for each corner and 1 bit for a steep-flag.
 *
 * For halftile slopes an extra 3 bits are used to represent this
 * properly; 1 bit for a halftile-flag and 2 bits to encode which
 * extra side (corner) is leveled when the slope of the first 5
 * bits is applied. This means that there can only be one leveled
 * slope for steep slopes, which is logical because two leveled
 * slopes would mean that it is not a steep slope as halftile
 * slopes only span one height level.
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
	SLOPE_STEEP_N  = SLOPE_STEEP | SLOPE_ENW,               ///< a steep slope falling to south (from north)

	SLOPE_HALFTILE = 0x20,                                  ///< one halftile is leveled (non continuous slope)
	SLOPE_HALFTILE_MASK = 0xE0,                             ///< three bits used for halftile slopes
	SLOPE_HALFTILE_W = SLOPE_HALFTILE | (CORNER_W << 6),    ///< the west halftile is leveled (non continuous slope)
	SLOPE_HALFTILE_S = SLOPE_HALFTILE | (CORNER_S << 6),    ///< the south halftile is leveled (non continuous slope)
	SLOPE_HALFTILE_E = SLOPE_HALFTILE | (CORNER_E << 6),    ///< the east halftile is leveled (non continuous slope)
	SLOPE_HALFTILE_N = SLOPE_HALFTILE | (CORNER_N << 6),    ///< the north halftile is leveled (non continuous slope)
};

/**
 * Rangecheck for Corner enumeration.
 *
 * @param corner A #Corner.
 * @return true iff corner is in a valid range.
 */
static inline bool IsValidCorner(Corner corner)
{
	return IS_INT_INSIDE(corner, 0, CORNER_END);
}

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
 * Checks for non-continuous slope on halftile foundations.
 *
 * @param s The given #Slope.
 * @return True if the slope is non-continuous, else false.
 */
static inline bool IsHalftileSlope(Slope s)
{
	return (s & SLOPE_HALFTILE) != 0;
}

/**
 * Return the complement of a slope.
 *
 * This method returns the complement of a slope. The complement of a
 * slope is a slope with raised corner which aren't raised in the given
 * slope.
 *
 * @pre The slope must neither be steep nor a halftile slope.
 * @param s The #Slope to get the complement.
 * @return a complement Slope of the given slope.
 */
static inline Slope ComplementSlope(Slope s)
{
	assert(!IsSteepSlope(s) && !IsHalftileSlope(s));
	return (Slope)(0xF ^ s);
}

/**
 * Tests if a slope has a highest corner (i.e. one corner raised or a steep slope).
 *
 * Note: A halftile slope is ignored.
 *
 * @param s The #Slope.
 * @return  true iff the slope has a highest corner.
 */
static inline bool HasSlopeHighestCorner(Slope s)
{
	s = (Slope)(s & ~SLOPE_HALFTILE_MASK);
	return IsSteepSlope(s) || (s == SLOPE_W) || (s == SLOPE_S) || (s == SLOPE_E) || (s == SLOPE_N);
}

/**
 * Returns the highest corner of a slope (one corner raised or a steep slope).
 *
 * @pre      The slope must be a slope with one corner raised or a steep slope. A halftile slope is ignored.
 * @param s  The #Slope.
 * @return   Highest corner.
 */
static inline Corner GetHighestSlopeCorner(Slope s)
{
	switch (s & ~SLOPE_HALFTILE_MASK) {
		case SLOPE_W:
		case SLOPE_STEEP_W: return CORNER_W;
		case SLOPE_S:
		case SLOPE_STEEP_S: return CORNER_S;
		case SLOPE_E:
		case SLOPE_STEEP_E: return CORNER_E;
		case SLOPE_N:
		case SLOPE_STEEP_N: return CORNER_N;
		default: NOT_REACHED();
	}
}

/**
 * Returns the leveled halftile of a halftile slope.
 *
 * @pre     The slope must be a halftile slope.
 * @param s The #Slope.
 * @return  The corner of the leveled halftile.
 */
static inline Corner GetHalftileSlopeCorner(Slope s)
{
	assert(IsHalftileSlope(s));
	return (Corner)((s >> 6) & 3);
}

/**
 * Returns the height of the highest corner of a slope relative to TileZ (= minimal height)
 *
 * @param s The #Slope.
 * @return Relative height of highest corner.
 */
static inline uint GetSlopeMaxZ(Slope s)
{
	if (s == SLOPE_FLAT) return 0;
	if (IsSteepSlope(s)) return 2 * TILE_HEIGHT;
	return TILE_HEIGHT;
}

/**
 * Returns the opposite corner.
 *
 * @param corner A #Corner.
 * @return The opposite corner to "corner".
 */
static inline Corner OppositeCorner(Corner corner)
{
	return (Corner)(corner ^ 2);
}

/**
 * Returns the slope with a specific corner raised.
 *
 * @param corner The #Corner.
 * @return The #Slope with corner "corner" raised.
 */
static inline Slope SlopeWithOneCornerRaised(Corner corner)
{
	assert(IsValidCorner(corner));
	return (Slope)(1 << corner);
}

/**
 * Returns the slope with all except one corner raised.
 *
 * @param corner The #Corner.
 * @return The #Slope with all corners but "corner" raised.
 */
static inline Slope SlopeWithThreeCornersRaised(Corner corner)
{
	return ComplementSlope(SlopeWithOneCornerRaised(corner));
}

/**
 * Adds a halftile slope to a slope.
 *
 * @param s #Slope without a halftile slope.
 * @param corner The #Corner of the halftile.
 * @return The #Slope s with the halftile slope added.
 */
static inline Slope HalftileSlope(Slope s, Corner corner)
{
	assert(IsValidCorner(corner));
	return (Slope)(s | SLOPE_HALFTILE | (corner << 6));
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

	FOUNDATION_INVALID = 0xFF    ///< Used inside "rail_cmd.cpp" to indicate invalid slope/track combination.
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
