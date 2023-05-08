/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file slope_type.h Definitions of a slope.
 * This file defines the enumeration and helper functions for handling
 * the slope info of a tile.
 */

#ifndef SLOPE_TYPE_H
#define SLOPE_TYPE_H

#include "core/enum_type.hpp"

/**
 * Enumeration of tile corners
 */
enum Corner {
	CORNER_W = 0,
	CORNER_S = 1,
	CORNER_E = 2,
	CORNER_N = 3,
	CORNER_END,
	CORNER_INVALID = 0xFF
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
enum Slope : byte {
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
	SLOPE_ELEVATED = SLOPE_N | SLOPE_E | SLOPE_S | SLOPE_W, ///< bit mask containing all 'simple' slopes
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
DECLARE_ENUM_AS_BIT_SET(Slope)

/**
 * Helper for creating a bitset of slopes.
 * @param x The slope to convert into a bitset.
 */
#define M(x) (1U << (x))
/** Constant bitset with safe slopes for building a level crossing. */
static const uint32_t VALID_LEVEL_CROSSING_SLOPES = M(SLOPE_SEN) | M(SLOPE_ENW) | M(SLOPE_NWS) | M(SLOPE_NS) | M(SLOPE_WSE) | M(SLOPE_EW) | M(SLOPE_FLAT);
#undef M


/**
 * Enumeration for Foundations.
 */
enum Foundation {
	FOUNDATION_NONE,             ///< The tile has no foundation, the slope remains unchanged.
	FOUNDATION_LEVELED,          ///< The tile is leveled up to a flat slope.
	FOUNDATION_INCLINED_X,       ///< The tile has an along X-axis inclined foundation.
	FOUNDATION_INCLINED_Y,       ///< The tile has an along Y-axis inclined foundation.
	FOUNDATION_STEEP_LOWER,      ///< The tile has a steep slope. The lowest corner is raised by a foundation to allow building railroad on the lower halftile.

	/* Halftile foundations */
	FOUNDATION_STEEP_BOTH,       ///< The tile has a steep slope. The lowest corner is raised by a foundation and the upper halftile is leveled.
	FOUNDATION_HALFTILE_W,       ///< Level west halftile non-continuously.
	FOUNDATION_HALFTILE_S,       ///< Level south halftile non-continuously.
	FOUNDATION_HALFTILE_E,       ///< Level east halftile non-continuously.
	FOUNDATION_HALFTILE_N,       ///< Level north halftile non-continuously.

	/* Special anti-zig-zag foundations for single horizontal/vertical track */
	FOUNDATION_RAIL_W,           ///< Foundation for TRACK_BIT_LEFT, but not a leveled foundation.
	FOUNDATION_RAIL_S,           ///< Foundation for TRACK_BIT_LOWER, but not a leveled foundation.
	FOUNDATION_RAIL_E,           ///< Foundation for TRACK_BIT_RIGHT, but not a leveled foundation.
	FOUNDATION_RAIL_N,           ///< Foundation for TRACK_BIT_UPPER, but not a leveled foundation.

	FOUNDATION_INVALID = 0xFF,   ///< Used inside "rail_cmd.cpp" to indicate invalid slope/track combination.
};

#endif /* SLOPE_TYPE_H */
