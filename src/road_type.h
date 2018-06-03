/* $Id$ */

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
#include "core/math_func.hpp"

typedef uint32 RoadTypeLabel;

/**
 * The different roadtypes we support
 *
 * @note currently only ROADTYPE_ROAD and ROADTYPE_TRAM are supported.
 */
enum RoadType {
	ROADTYPE_BEGIN = 0,      ///< Used for iterations
	ROADTYPE_ROAD = 0,       ///< Basic road type
	ROADTYPE_TRAM = 1,       ///< Trams
	ROADTYPE_END,            ///< Used for iterations
	INVALID_ROADTYPE = 0xFF, ///< flag for invalid roadtype
};
DECLARE_POSTFIX_INCREMENT(RoadType)
template <> struct EnumPropsT<RoadType> : MakeEnumPropsT<RoadType, byte, ROADTYPE_BEGIN, ROADTYPE_END, INVALID_ROADTYPE, 2> {};
typedef TinyEnumT<RoadType> RoadTypeByte;

enum RoadSubType {
	ROADSUBTYPE_BEGIN    = 0,   ///< Used for iterations
	ROADSUBTYPE_NORMAL   = 0,   ///< Plain road/tram
	ROADSUBTYPE_ELECTRIC = 1,   ///< Electrified road/tram
	ROADSUBTYPE_END      = 15,  ///< Used for iterations
	INVALID_ROADSUBTYPE  = 0xF, ///< flag for invalid roadsubtype
};
DECLARE_POSTFIX_INCREMENT(RoadSubType);

/**
 * The different roadtypes we support, but then a bitmask of them.
 */
enum RoadTypes {
	ROADTYPES_NONE     = 0,                                ///< No roadtypes
	ROADTYPES_ROAD     = 1 << ROADTYPE_ROAD,               ///< Road
	ROADTYPES_TRAM     = 1 << ROADTYPE_TRAM,               ///< Trams
	ROADTYPES_ALL      = ROADTYPES_ROAD | ROADTYPES_TRAM,  ///< Road + trams
	ROADTYPES_END,                                         ///< Used for iterations?
	INVALID_ROADTYPES  = 0xFF,                             ///< Invalid roadtypes
};
DECLARE_ENUM_AS_BIT_SET(RoadTypes)
template <> struct EnumPropsT<RoadTypes> : MakeEnumPropsT<RoadTypes, byte, ROADTYPES_NONE, ROADTYPES_END, INVALID_ROADTYPES, 2> {};
typedef SimpleTinyEnumT<RoadTypes, byte> RoadTypesByte;

/**
 * The different roadsubtypes which exist, but then a bitmask of them.
 */
enum RoadSubTypes {
	ROADSUBTYPES_NONE     = 0,                          ///< No roadsubtypes
	ROADSUBTYPES_NORMAL   = 1 << ROADSUBTYPE_NORMAL,   ///< Plain road/tram
	ROADSUBTYPES_ELECTRIC = 1 << ROADSUBTYPE_ELECTRIC, ///< Electrified road/tram
};
DECLARE_ENUM_AS_BIT_SET(RoadSubTypes)

/**
 * Identifier for road and tram types.
 */
struct RoadTypeIdentifier {
	RoadType basetype;
	RoadSubType subtype;

	bool operator==(const RoadTypeIdentifier &other) const
	{
		return this->basetype == other.basetype && this->subtype == other.subtype;
	}

	bool operator!=(const RoadTypeIdentifier &other) const
	{
		return this->basetype != other.basetype || this->subtype != other.subtype;
	}

	uint8 Pack() const;
	bool UnpackIfValid(uint32 data);
	static RoadTypeIdentifier Unpack(uint32 data);

	bool IsValid() const
	{
		return (this->basetype == ROADTYPE_ROAD || this->basetype == ROADTYPE_TRAM) && IsInsideMM(this->subtype, ROADSUBTYPE_BEGIN, ROADSUBTYPE_END);
	}

	bool IsRoad() const
	{
		return (this->basetype == ROADTYPE_ROAD) && IsInsideMM(this->subtype, ROADSUBTYPE_BEGIN, ROADSUBTYPE_END);
	}

	bool IsTram() const
	{
		return (this->basetype == ROADTYPE_TRAM) && IsInsideMM(this->subtype, ROADSUBTYPE_BEGIN, ROADSUBTYPE_END);
	}

	RoadTypeIdentifier(RoadType basetype, RoadSubType subtype) : basetype(basetype), subtype(subtype) {}
	RoadTypeIdentifier() : basetype(INVALID_ROADTYPE), subtype(INVALID_ROADSUBTYPE) {}
};

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

	ROAD_N    = ROAD_NE | ROAD_NW,   ///< Road at the two northern edges
	ROAD_E    = ROAD_NE | ROAD_SE,   ///< Road at the two eastern edges
	ROAD_S    = ROAD_SE | ROAD_SW,   ///< Road at the two southern edges
	ROAD_W    = ROAD_NW | ROAD_SW,   ///< Road at the two western edges

	ROAD_ALL  = ROAD_X  | ROAD_Y,    ///< Full 4-way crossing

	ROAD_END  = ROAD_ALL + 1,        ///< Out-of-range roadbits, used for iterations
};
DECLARE_ENUM_AS_BIT_SET(RoadBits)
template <> struct EnumPropsT<RoadBits> : MakeEnumPropsT<RoadBits, byte, ROAD_NONE, ROAD_END, ROAD_NONE, 4> {};

#endif /* ROAD_TYPE_H */
