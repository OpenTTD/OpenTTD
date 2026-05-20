/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file road_type.h Enums and other types related to roads. */

#ifndef ROAD_TYPE_H
#define ROAD_TYPE_H

#include "core/enum_type.hpp"

typedef uint32_t RoadTypeLabel;

static const RoadTypeLabel ROADTYPE_LABEL_ROAD = 'ROAD';
static const RoadTypeLabel ROADTYPE_LABEL_TRAM = 'ELRL';

/**
 * The different roadtypes we support
 */
enum RoadType : uint8_t {
	ROADTYPE_BEGIN   = 0,    ///< Used for iterations
	ROADTYPE_ROAD    = 0,    ///< Basic road type
	ROADTYPE_TRAM    = 1,    ///< Trams
	ROADTYPE_END     = 63,   ///< Used for iterations
	INVALID_ROADTYPE = 63,   ///< flag for invalid roadtype
};
DECLARE_INCREMENT_DECREMENT_OPERATORS(RoadType)

using RoadTypes = EnumBitSet<RoadType, uint64_t>;

/**
 * The different types of road type.
 */
enum class RoadTramType : uint8_t {
	Road, ///< Road type.
	Tram, ///< Tram type.
	End, ///< End marker.
	Invalid = 0xFF, ///< Invalid marker.
};

/** Bitset of \c RoadTramType elements. */
using RoadTramTypes = EnumBitSet<RoadTramType, uint8_t>;

/** All possible RoadTramTypes. */
static constexpr RoadTramTypes ROADTRAMTYPES_ALL{RoadTramType::Road, RoadTramType::Tram};

/**
 * Enumeration for the road parts on a tile.
 *
 * This enumeration defines the possible road parts which
 * can be build on a tile.
 */
enum class RoadBit : uint8_t {
	NW = 0, ///< North-west part
	SW = 1, ///< South-west part
	SE = 2, ///< South-east part
	NE = 3, ///< North-east part
};

/** Bitset of \c RoadBit elements. */
using RoadBits = EnumBitSet<RoadBit, uint8_t>;

static constexpr RoadBits ROAD_X{RoadBit::SW, RoadBit::NE}; ///< Full road along the x-axis (south-west + north-east)
static constexpr RoadBits ROAD_Y{RoadBit::NW, RoadBit::SE}; ///< Full road along the y-axis (north-west + south-east)

static constexpr RoadBits ROAD_N{RoadBit::NE, RoadBit::NW}; ///< Road at the two northern edges
static constexpr RoadBits ROAD_E{RoadBit::NE, RoadBit::SE}; ///< Road at the two eastern edges
static constexpr RoadBits ROAD_S{RoadBit::SE, RoadBit::SW}; ///< Road at the two southern edges
static constexpr RoadBits ROAD_W{RoadBit::NW, RoadBit::SW}; ///< Road at the two western edges

static constexpr RoadBits ROAD_ALL{RoadBit::NW, RoadBit::SW, RoadBit::SE, RoadBit::NE}; ///< Full 4-way crossing

/** Which directions are disallowed ? */
enum class DisallowedRoadDirection : uint8_t {
	Southbound, ///< All southbound traffic is disallowed.
	Northbound, ///< All northbound traffic is disallowed.
	End, ///< End marker.
};

/** Bitset of \c DisallowedRoadDirection elements. */
using DisallowedRoadDirections = EnumBitSet<DisallowedRoadDirection, uint8_t>;

#endif /* ROAD_TYPE_H */
