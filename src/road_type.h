/* $Id$ */

/** @file road.h Enums and other types related to roads. */

#ifndef ROAD_TYPE_H
#define ROAD_TYPE_H

#include "core/enum_type.hpp"

/**
 * The different roadtypes we support
 *
 * @note currently only ROADTYPE_ROAD and ROADTYPE_TRAM are supported.
 */
enum RoadType {
	ROADTYPE_ROAD = 0,      ///< Basic road type
	ROADTYPE_TRAM = 1,      ///< Trams
	ROADTYPE_HWAY = 2,      ///< Only a placeholder. Not sure what we are going to do with this road type.
	ROADTYPE_END,           ///< Used for iterations
	INVALID_ROADTYPE = 0xFF ///< flag for invalid roadtype
};
DECLARE_POSTFIX_INCREMENT(RoadType);

/**
 * The different roadtypes we support, but then a bitmask of them
 * @note currently only roadtypes with ROADTYPE_ROAD and ROADTYPE_TRAM are supported.
 */
enum RoadTypes {
	ROADTYPES_NONE     = 0,                                                 ///< No roadtypes
	ROADTYPES_ROAD     = 1 << ROADTYPE_ROAD,                                ///< Road
	ROADTYPES_TRAM     = 1 << ROADTYPE_TRAM,                                ///< Trams
	ROADTYPES_HWAY     = 1 << ROADTYPE_HWAY,                                ///< Highway (or whatever substitute)
	ROADTYPES_ROADTRAM = ROADTYPES_ROAD | ROADTYPES_TRAM,                   ///< Road + trams
	ROADTYPES_ROADHWAY = ROADTYPES_ROAD | ROADTYPES_HWAY,                   ///< Road + highway (or whatever substitute)
	ROADTYPES_TRAMHWAY = ROADTYPES_TRAM | ROADTYPES_HWAY,                   ///< Trams + highway (or whatever substitute)
	ROADTYPES_ALL      = ROADTYPES_ROAD | ROADTYPES_TRAM | ROADTYPES_HWAY,  ///< Road + trams + highway (or whatever substitute)
	ROADTYPES_END,                                                          ///< Used for iterations?
	INVALID_ROADTYPES  = 0xFF                                               ///< Invalid roadtypes
};
DECLARE_ENUM_AS_BIT_SET(RoadTypes);
template <> struct EnumPropsT<RoadTypes> : MakeEnumPropsT<RoadTypes, byte, ROADTYPES_NONE, ROADTYPES_END, INVALID_ROADTYPES> {};
typedef TinyEnumT<RoadTypes> RoadTypesByte;


/**
 * Enumeration for the road parts on a tile.
 *
 * This enumeration defines the possible road parts which
 * can be build on a tile.
 */
enum RoadBits {
	ROAD_NONE = 0U,                  ///< No road-part is build
	ROAD_NW   = 1U,                  ///< North-west part
	ROAD_SW   = 2U,                  ///< South-west part
	ROAD_SE   = 4U,                  ///< South-east part
	ROAD_NE   = 8U,                  ///< North-east part
	ROAD_X    = ROAD_SW | ROAD_NE,   ///< Full road along the x-axis (south-west + north-east)
	ROAD_Y    = ROAD_NW | ROAD_SE,   ///< Full road along the y-axis (north-west + south-east)
	ROAD_ALL  = ROAD_X  | ROAD_Y     ///< Full 4-way crossing
};
DECLARE_ENUM_AS_BIT_SET(RoadBits);

#endif /* ROAD_TYPE_H */
