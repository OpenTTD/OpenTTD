/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

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
static constexpr inline bool IsValidCorner(Corner corner)
{
	return IsInsideMM(corner, 0, CORNER_END);
}


/**
 * Checks if a slope is steep.
 *
 * @param s The given #Slope.
 * @return True if the slope is steep, else false.
 */
static constexpr inline bool IsSteepSlope(Slope s)
{
	return (s & SLOPE_STEEP) != 0;
}

/**
 * Checks for non-continuous slope on halftile foundations.
 *
 * @param s The given #Slope.
 * @return True if the slope is non-continuous, else false.
 */
static constexpr inline bool IsHalftileSlope(Slope s)
{
	return (s & SLOPE_HALFTILE) != 0;
}

/**
 * Removes a halftile slope from a slope
 *
 * Non-halftile slopes remain unmodified.
 *
 * @param s A #Slope.
 * @return The slope s without its halftile slope.
 */
static constexpr inline Slope RemoveHalftileSlope(Slope s)
{
	return s & ~SLOPE_HALFTILE_MASK;
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
	return s ^ SLOPE_ELEVATED;
}

/**
 * Tests if a specific slope has exactly one corner raised.
 *
 * @param s The #Slope
 * @return true iff exactly one corner is raised
 */
static inline bool IsSlopeWithOneCornerRaised(Slope s)
{
	return (s == SLOPE_W) || (s == SLOPE_S) || (s == SLOPE_E) || (s == SLOPE_N);
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
 * Tests if a slope has a highest corner (i.e. one corner raised or a steep slope).
 *
 * Note: A halftile slope is ignored.
 *
 * @param s The #Slope.
 * @return  true iff the slope has a highest corner.
 */
static inline bool HasSlopeHighestCorner(Slope s)
{
	s = RemoveHalftileSlope(s);
	return IsSteepSlope(s) || IsSlopeWithOneCornerRaised(s);
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
	switch (RemoveHalftileSlope(s)) {
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
static constexpr inline Corner GetHalftileSlopeCorner(Slope s)
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
static constexpr inline int GetSlopeMaxZ(Slope s)
{
	if (s == SLOPE_FLAT) return 0;
	if (IsSteepSlope(s)) return 2;
	return 1;
}

/**
 * Returns the height of the highest corner of a slope relative to TileZ (= minimal height)
 *
 * @param s The #Slope.
 * @return Relative height of highest corner.
 */
static constexpr inline int GetSlopeMaxPixelZ(Slope s)
{
	return GetSlopeMaxZ(s) * TILE_HEIGHT;
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
 * Tests if a specific slope has exactly three corners raised.
 *
 * @param s The #Slope
 * @return true iff exactly three corners are raised
 */
static inline bool IsSlopeWithThreeCornersRaised(Slope s)
{
	return !IsHalftileSlope(s) && !IsSteepSlope(s) && IsSlopeWithOneCornerRaised(ComplementSlope(s));
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
 * Returns a specific steep slope
 *
 * @param corner A #Corner.
 * @return The steep #Slope with "corner" as highest corner.
 */
static inline Slope SteepSlope(Corner corner)
{
	return SLOPE_STEEP | SlopeWithThreeCornersRaised(OppositeCorner(corner));
}

/**
 * Tests if a specific slope is an inclined slope.
 *
 * @param s The #Slope
 * @return true iff the slope is inclined.
 */
static inline bool IsInclinedSlope(Slope s)
{
	return (s == SLOPE_NW) || (s == SLOPE_SW) || (s == SLOPE_SE) || (s == SLOPE_NE);
}

/**
 * Returns the direction of an inclined slope.
 *
 * @param s A #Slope
 * @return The direction the slope goes up in. Or INVALID_DIAGDIR if the slope is not an inclined slope.
 */
static inline DiagDirection GetInclinedSlopeDirection(Slope s)
{
	switch (s) {
		case SLOPE_NE: return DIAGDIR_NE;
		case SLOPE_SE: return DIAGDIR_SE;
		case SLOPE_SW: return DIAGDIR_SW;
		case SLOPE_NW: return DIAGDIR_NW;
		default: return INVALID_DIAGDIR;
	}
}

/**
 * Returns the slope that is inclined in a specific direction.
 *
 * @param dir A #DiagDirection
 * @return The #Slope that goes up in direction dir.
 */
static inline Slope InclinedSlope(DiagDirection dir)
{
	switch (dir) {
		case DIAGDIR_NE: return SLOPE_NE;
		case DIAGDIR_SE: return SLOPE_SE;
		case DIAGDIR_SW: return SLOPE_SW;
		case DIAGDIR_NW: return SLOPE_NW;
		default: NOT_REACHED();
	}
}

/**
 * Adds a halftile slope to a slope.
 *
 * @param s #Slope without a halftile slope.
 * @param corner The #Corner of the halftile.
 * @return The #Slope s with the halftile slope added.
 */
static constexpr inline Slope HalftileSlope(Slope s, Corner corner)
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
 * @param s  The current #Slope.
 * @return   The needed #Foundation.
 */
static inline Foundation FlatteningFoundation(Slope s)
{
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

/**
 * Returns the #Sprite offset for a given #Slope.
 *
 * @param s The #Slope to get the offset for.
 * @return The sprite offset for this #Slope.
 */
static inline uint SlopeToSpriteOffset(Slope s)
{
	extern const byte _slope_to_sprite_offset[32];
	return _slope_to_sprite_offset[s];
}

#endif /* SLOPE_FUNC_H */
