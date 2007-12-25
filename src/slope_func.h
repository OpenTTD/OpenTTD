/* $Id$ */

/** @file slope_func.h Functions related to slopes. */

#ifndef SLOPE_FUNC_H
#define SLOPE_FUNC_H

#include "core/math_func.hpp"
#include "slope_type.h"
#include "direction_type.h"
#include "tile_type.h"

/**
 * Rangecheck for Corner enumeration.
 *
 * @param corner A #Corner.
 * @return true iff corner is in a valid range.
 */
static inline bool IsValidCorner(Corner corner)
{
	return IsInsideMM(corner, 0, CORNER_END);
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
 * Tests if a foundation is a non-continuous foundation, i.e. halftile-foundation or FOUNDATION_STEEP_BOTH.
 *
 * @param f  The #Foundation.
 * @return   true iff f is a non-continuous foundation
 */
static inline bool IsNonContinuousFoundation(Foundation f)
{
	return IsInsideMM(f, FOUNDATION_STEEP_BOTH, FOUNDATION_HALFTILE_N + 1);
}

/**
 * Returns the halftile corner of a halftile-foundation
 *
 * @pre f != FOUNDATION_STEEP_BOTH
 *
 * @param f  The #Foundation.
 * @return   The #Corner with track.
 */
static inline Corner GetHalftileFoundationCorner(Foundation f)
{
	assert(IsInsideMM(f, FOUNDATION_HALFTILE_W, FOUNDATION_HALFTILE_N + 1));
	return (Corner)(f - FOUNDATION_HALFTILE_W);
}

/**
 * Tests if a foundation is a special rail foundation for single horizontal/vertical track.
 *
 * @param f  The #Foundation.
 * @return   true iff f is a special rail foundation for single horizontal/vertical track.
 */
static inline bool IsSpecialRailFoundation(Foundation f)
{
	return IsInsideMM(f, FOUNDATION_RAIL_W, FOUNDATION_RAIL_N + 1);
}

/**
 * Returns the track corner of a special rail foundation
 *
 * @param f  The #Foundation.
 * @return   The #Corner with track.
 */
static inline Corner GetRailFoundationCorner(Foundation f)
{
	assert(IsSpecialRailFoundation(f));
	return (Corner)(f - FOUNDATION_RAIL_W);
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

/**
 * Returns the halftile foundation for single horizontal/vertical track.
 *
 * @param corner The #Corner with the track.
 * @return       The wanted #Foundation.
 */
static inline Foundation HalftileFoundation(Corner corner)
{
	assert(IsValidCorner(corner));
	return (Foundation)(FOUNDATION_HALFTILE_W + corner);
}

/**
 * Returns the special rail foundation for single horizontal/vertical track.
 *
 * @param corner The #Corner with the track.
 * @return       The wanted #Foundation.
 */
static inline Foundation SpecialRailFoundation(Corner corner)
{
	assert(IsValidCorner(corner));
	return (Foundation)(FOUNDATION_RAIL_W + corner);
}

#endif /* SLOPE_FUNC_H */
