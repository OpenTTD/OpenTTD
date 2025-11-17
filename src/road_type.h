/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file road_type.h Enums and other types related to roads. */

#ifndef ROAD_TYPE_H
#define ROAD_TYPE_H

#include "core/enum_type.hpp"
#include "core/flatset_type.hpp"
#include "transport_mapping.hpp"

typedef uint32_t RoadTypeLabel;

static const RoadTypeLabel ROADTYPE_LABEL_ROAD = 'ROAD';
static const RoadTypeLabel ROADTYPE_LABEL_TRAM = 'ELRL';

/**
 * The different roadtypes we support
 */
enum RoadType : uint16_t {
	ROADTYPE_BEGIN   = 0,    ///< Used for iterations
	ROADTYPE_ROAD    = 0,    ///< Basic road type
	ROADTYPE_TRAM    = 1,    ///< Trams
	INVALID_ROADTYPE = UINT16_MAX, ///< flag for invalid roadtype
};
DECLARE_INCREMENT_DECREMENT_OPERATORS(RoadType)

using RoadTypes = FlatBitSet<RoadType>;

/**
 * The different types of road type.
 */
enum RoadTramType : bool {
	RTT_ROAD, ///< Road road type.
	RTT_TRAM, ///< Tram road type.
};

enum RoadTramTypes : uint8_t {
	RTTB_ROAD = 1 << RTT_ROAD, ///< Road road type bit.
	RTTB_TRAM = 1 << RTT_TRAM, ///< Tram road type bit.
};
DECLARE_ENUM_AS_BIT_SET(RoadTramTypes)

static const RoadTramType _roadtramtypes[] = { RTT_ROAD, RTT_TRAM };

/**
 * Enumeration for the road parts on a tile.
 *
 * This enumeration defines the possible road parts which
 * can be build on a tile.
 */
enum RoadBits : uint8_t {
	ROAD_NONE = 0U,                  ///< No road-part is build
	ROAD_NW   = 1U,                  ///< North-west part
	ROAD_SW   = 2U,                  ///< South-west part
	ROAD_SE   = 4U,                  ///< South-east part
	ROAD_NE   = 8U,                  ///< North-east part
	ROAD_X    = ROAD_SW | ROAD_NE,   ///< Full road along the x-axis (south-west + north-east)
	ROAD_Y    = ROAD_NW | ROAD_SE,   ///< Full road along the y-axis (north-west + south-east)

	ROAD_N    = ROAD_NE | ROAD_NW,   ///< Road at the two northern edges
	ROAD_E    = ROAD_NE | ROAD_SE,   ///< Road at the two eastern edges
	ROAD_S    = ROAD_SE | ROAD_SW,   ///< Road at the two southern edges
	ROAD_W    = ROAD_NW | ROAD_SW,   ///< Road at the two western edges

	ROAD_ALL  = ROAD_X  | ROAD_Y,    ///< Full 4-way crossing

	ROAD_END  = ROAD_ALL + 1,        ///< Out-of-range roadbits, used for iterations
};
DECLARE_ENUM_AS_BIT_SET(RoadBits)

/** Which directions are disallowed ? */
enum DisallowedRoadDirections : uint8_t {
	DRD_NONE,       ///< None of the directions are disallowed
	DRD_SOUTHBOUND, ///< All southbound traffic is disallowed
	DRD_NORTHBOUND, ///< All northbound traffic is disallowed
	DRD_BOTH,       ///< All directions are disallowed
	DRD_END,        ///< Sentinel
};
DECLARE_ENUM_AS_BIT_SET(DisallowedRoadDirections)

/** Mapped road type. */
using RoadTypeMapping = TransportMapping<RoadType, INVALID_ROADTYPE, 63, struct RoadTypeMappingTag>;
extern RoadTypeMapping _roadtype_mapping;

/** Mapped tram type. */
using TramTypeMapping = TransportMapping<RoadType, INVALID_ROADTYPE, 63, struct TramTypeMappingTag>;
extern TramTypeMapping _tramtype_mapping;

/** Alias for mapped road type. */
using MapRoadType = RoadTypeMapping::MapType;
/** Alias for mapped tram type. */
using MapTramType = TramTypeMapping::MapType;

#endif /* ROAD_TYPE_H */
