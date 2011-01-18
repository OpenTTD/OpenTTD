/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file road_func.h Functions related to roads. */

#ifndef ROAD_FUNC_H
#define ROAD_FUNC_H

#include "core/bitmath_func.hpp"
#include "road_type.h"
#include "direction_type.h"
#include "company_type.h"
#include "tile_type.h"

/**
 * Iterate through each set RoadType in a RoadTypes value.
 * For more informations see FOR_EACH_SET_BIT_EX.
 *
 * @param var Loop index variable that stores fallowing set road type. Must be of type RoadType.
 * @param road_types The value to iterate through (any expression).
 *
 * @see FOR_EACH_SET_BIT_EX
 */
#define FOR_EACH_SET_ROADTYPE(var, road_types) FOR_EACH_SET_BIT_EX(RoadType, var, RoadTypes, road_types)

/**
 * Whether the given roadtype is valid.
 * @param rt the roadtype to check for validness
 * @return true if and only if valid
 */
static inline bool IsValidRoadType(RoadType rt)
{
	return rt == ROADTYPE_ROAD || rt == ROADTYPE_TRAM;
}

/**
 * Maps a RoadType to the corresponding RoadTypes value
 *
 * @param rt the roadtype to get the roadtypes from
 * @return the roadtypes with the given roadtype
 */
static inline RoadTypes RoadTypeToRoadTypes(RoadType rt)
{
	return (RoadTypes)(1 << rt);
}

/**
 * Returns the RoadTypes which are not present in the given RoadTypes
 *
 * This function returns the complement of a given RoadTypes.
 *
 * @param r The given RoadTypes
 * @return The complement of the given RoadTypes
 * @note The unused value ROADTYPES_HWAY will be used, too.
 */
static inline RoadTypes ComplementRoadTypes(RoadTypes r)
{
	return (RoadTypes)(ROADTYPES_ALL ^ r);
}


/**
 * Calculate the complement of a RoadBits value
 *
 * Simply flips all bits in the RoadBits value to get the complement
 * of the RoadBits.
 *
 * @param r The given RoadBits value
 * @return the complement
 */
static inline RoadBits ComplementRoadBits(RoadBits r)
{
	return (RoadBits)(ROAD_ALL ^ r);
}

/**
 * Calculate the mirrored RoadBits
 *
 * Simply move the bits to their new position.
 *
 * @param r The given RoadBits value
 * @return the mirrored
 */
static inline RoadBits MirrorRoadBits(RoadBits r)
{
	return (RoadBits)(GB(r, 0, 2) << 2 | GB(r, 2, 2));
}

/**
 * Calculate rotated RoadBits
 *
 * Move the Roadbits clockwise til they are in their final position.
 *
 * @param r The given RoadBits value
 * @param rot The given Rotation angle
 * @return the rotated
 */
static inline RoadBits RotateRoadBits(RoadBits r, DiagDirDiff rot)
{
	for (; rot > (DiagDirDiff)0; rot--) {
		r = (RoadBits)(GB(r, 0, 1) << 3 | GB(r, 1, 3));
	}
	return r;
}

/**
 * Check if we've got a straight road
 *
 * @param r The given RoadBits
 * @return true if we've got a straight road
 */
static inline bool IsStraightRoad(RoadBits r)
{
	return (r == ROAD_X || r == ROAD_Y);
}

/**
 * Create the road-part which belongs to the given DiagDirection
 *
 * This function returns a RoadBits value which belongs to
 * the given DiagDirection.
 *
 * @param d The DiagDirection
 * @return The result RoadBits which the selected road-part set
 */
static inline RoadBits DiagDirToRoadBits(DiagDirection d)
{
	return (RoadBits)(ROAD_NW << (3 ^ d));
}

/**
 * Create the road-part which belongs to the given Axis
 *
 * This function returns a RoadBits value which belongs to
 * the given Axis.
 *
 * @param a The Axis
 * @return The result RoadBits which the selected road-part set
 */
static inline RoadBits AxisToRoadBits(Axis a)
{
	return a == AXIS_X ? ROAD_X : ROAD_Y;
}

bool HasRoadTypesAvail(const CompanyID company, const RoadTypes rts);
bool ValParamRoadType(const RoadType rt);
RoadTypes GetCompanyRoadtypes(const CompanyID company);

void UpdateLevelCrossing(TileIndex tile, bool sound = true);

#endif /* ROAD_FUNC_H */
