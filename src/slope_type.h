/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/**
 * @file slope_type.h Definitions of a slope.
 * This file defines the enumeration and helper functions for handling the slope info of a tile.
 */

#ifndef SLOPE_TYPE_H
#define SLOPE_TYPE_H

#include "core/enum_type.hpp"
#include "core/convertible_through_base.hpp"

/**
 * Enumeration of tile corners
 */
enum class Corner : uint8_t {
	W, ///< West tile corner.
	S, ///< South tile corner.
	E, ///< East tile corner.
	N, ///< North tile corner.
	End, ///< End marker.
	Steep = Corner::End, ///< Indicates the slope is steep.
	HalfTile, ///< One halftile is leveled (non continuous slope).
	Invalid = 0xFF, ///< Invalid marker.
};

/**
 * Array with \c Corner as index.
 * @tparam T the type contained within the array.
 */
template <typename T>
using CornerIndexArray = EnumIndexArray<T, Corner, Corner::End>;

/**
 * Bit set for the slope-type.
 *
 * This bit set use the chars N,E,S,W corresponding the
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
 * @{
 */
using Slope = EnumBitSet<Corner, uint8_t>;

static constexpr Slope SLOPE_FLAT{}; ///< A flat tile.

static constexpr Slope SLOPE_NW{Corner::N, Corner::W}; ///< North and west corners are raised.
static constexpr Slope SLOPE_SW{Corner::S, Corner::W}; ///< South and west corners are raised.
static constexpr Slope SLOPE_SE{Corner::S, Corner::E}; ///< South and east corners are raised.
static constexpr Slope SLOPE_NE{Corner::N, Corner::E}; ///< North and east corners are raised.
static constexpr Slope SLOPE_EW{Corner::E, Corner::W}; ///< East and west corners are raised.
static constexpr Slope SLOPE_NS{Corner::N, Corner::S}; ///< North and south corners are raised.
static constexpr Slope SLOPE_ELEVATED{Corner::N, Corner::E, Corner::S, Corner::W}; ///< Bit mask containing all 'simple' slopes.
static constexpr Slope SLOPE_NWS{Corner::N, Corner::W, Corner::S}; ///< North, west and south corners are raised.
static constexpr Slope SLOPE_WSE{Corner::W, Corner::S, Corner::E}; ///< West, south and east corners are raised.
static constexpr Slope SLOPE_SEN{Corner::S, Corner::E, Corner::N}; ///< South, east and north corners are raised.
static constexpr Slope SLOPE_ENW{Corner::E, Corner::N, Corner::W}; ///< East, north and west corners are raised.
static constexpr Slope SLOPE_STEEP_W = Slope(SLOPE_NWS).Set(Corner::Steep); ///< A steep slope falling to east (from west).
static constexpr Slope SLOPE_STEEP_S = Slope(SLOPE_WSE).Set(Corner::Steep); ///< A steep slope falling to north (from south).
static constexpr Slope SLOPE_STEEP_E = Slope(SLOPE_SEN).Set(Corner::Steep); ///< A steep slope falling to west (from east).
static constexpr Slope SLOPE_STEEP_N = Slope(SLOPE_ENW).Set(Corner::Steep); ///< A steep slope falling to south (from north).

static constexpr Slope SLOPE_HALFTILE_MASK{0xE0}; ///< Three bits used for halftile slopes.
static constexpr Slope SLOPE_HALFTILE_W = Slope(to_underlying(Corner::W) << 6).Set(Corner::HalfTile); ///< The west halftile is leveled (non continuous slope).
static constexpr Slope SLOPE_HALFTILE_S = Slope(to_underlying(Corner::S) << 6).Set(Corner::HalfTile); ///< The south halftile is leveled (non continuous slope).
static constexpr Slope SLOPE_HALFTILE_E = Slope(to_underlying(Corner::E) << 6).Set(Corner::HalfTile); ///< The east halftile is leveled (non continuous slope).
static constexpr Slope SLOPE_HALFTILE_N = Slope(to_underlying(Corner::N) << 6).Set(Corner::HalfTile); ///< The north halftile is leveled (non continuous slope).
/** @} */

/** The total number of possible slope types. */
static constexpr uint8_t NUM_SLOPES = 19;

/**
 * Array with \c Slope as index.
 * @note Remove halftile form the slope before accessing an element.
 * @note Elevated steep slope (e.g. value 31) is an invalid slope because it does not define which corner is steep.
 * @tparam T the type contained within the array.
 */
template <typename T>
using SlopeIndexArray = TypedIndexContainer<std::array<T, Slope{SLOPE_ELEVATED}.Set(Corner::Steep).base()>, Slope>;

/**
 * Array with non steep \c Slope as index.
 * @note Remove halftile form the slope before accessing an element.
 * @note SLOPE_ELEVATED is just SLOPE_FLAT with height increased by one, therefore it is not a valid index.
 * @tparam T the type contained within the array.
 */
template <typename T>
using NonSteepSlopeIndexArray = TypedIndexContainer<std::array<T, SLOPE_ELEVATED.base()>, Slope>;

/**
 * Helper for creating a bitset of slopes.
 * @param x The slope to convert into a bitset.
 */
#define M(x) (1U << (x.base()))
/** Constant bitset with safe slopes for building a level crossing. */
static const uint32_t VALID_LEVEL_CROSSING_SLOPES = M(SLOPE_SEN) | M(SLOPE_ENW) | M(SLOPE_NWS) | M(SLOPE_NS) | M(SLOPE_WSE) | M(SLOPE_EW) | M(SLOPE_FLAT);
#undef M


/**
 * Enumeration for Foundations.
 */
enum class Foundation : uint8_t {
	None, ///< The tile has no foundation, the slope remains unchanged.
	Leveled, ///< The tile is leveled up to a flat slope.
	InclinedX, ///< The tile has an along X-axis inclined foundation.
	InclinedY, ///< The tile has an along Y-axis inclined foundation.
	SteepLower, ///< The tile has a steep slope. The lowest corner is raised by a foundation to allow building railroad on the lower halftile.

	/* Halftile foundations */
	SteepBoth, ///< The tile has a steep slope. The lowest corner is raised by a foundation and the upper halftile is leveled.
	HalfTileW, ///< Level west halftile non-continuously.
	HalfTileS, ///< Level south halftile non-continuously.
	HalfTileE, ///< Level east halftile non-continuously.
	HalfTileN, ///< Level north halftile non-continuously.
	HalfTileEnd, ///< End marker for halftile foundations.

	/* Special anti-zig-zag foundations for single horizontal/vertical track */
	RailW = Foundation::HalfTileEnd, ///< Foundation for TRACK_BIT_LEFT, but not a leveled foundation.
	RailS, ///< Foundation for TRACK_BIT_LOWER, but not a leveled foundation.
	RailE, ///< Foundation for TRACK_BIT_RIGHT, but not a leveled foundation.
	RailN, ///< Foundation for TRACK_BIT_UPPER, but not a leveled foundation.
	End, ///< End marker.

	Invalid = 0xFF, ///< Used inside "rail_cmd.cpp" to indicate invalid slope/track combination.
};

#endif /* SLOPE_TYPE_H */
