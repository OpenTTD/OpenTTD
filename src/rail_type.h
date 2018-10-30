/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file rail_type.h The different types of rail */

#ifndef RAIL_TYPE_H
#define RAIL_TYPE_H

#include "core/enum_type.hpp"

typedef uint32 RailTypeLabel;

static const RailTypeLabel RAILTYPE_RAIL_LABEL     = 'RAIL';
static const RailTypeLabel RAILTYPE_ELECTRIC_LABEL = 'ELRL';
static const RailTypeLabel RAILTYPE_MONO_LABEL     = 'MONO';
static const RailTypeLabel RAILTYPE_MAGLEV_LABEL   = 'MGLV';

/**
 * Enumeration for all possible railtypes.
 *
 * This enumeration defines all 4 possible railtypes.
 */
enum RailType {
	RAILTYPE_BEGIN    = 0,          ///< Used for iterations
	RAILTYPE_RAIL     = 0,          ///< Standard non-electric rails
	RAILTYPE_ELECTRIC = 1,          ///< Electric rails
	RAILTYPE_MONO     = 2,          ///< Monorail
	RAILTYPE_MAGLEV   = 3,          ///< Maglev
	RAILTYPE_END      = 64,         ///< Used for iterations
	INVALID_RAILTYPE  = 0xFF,       ///< Flag for invalid railtype

	DEF_RAILTYPE_FIRST = RAILTYPE_END, ///< Default railtype: first available
	DEF_RAILTYPE_LAST,                 ///< Default railtype: last available
	DEF_RAILTYPE_MOST_USED,            ///< Default railtype: most used
};

/** Allow incrementing of Track variables */
DECLARE_POSTFIX_INCREMENT(RailType)
/** Define basic enum properties */
template <> struct EnumPropsT<RailType> : MakeEnumPropsT<RailType, byte, RAILTYPE_BEGIN, RAILTYPE_END, INVALID_RAILTYPE, 6> {};
typedef TinyEnumT<RailType> RailTypeByte;

/**
 * The different railtypes we support, but then a bitmask of them.
 * @note Must be treated as a uint64 type, narrowing it causes bit membership tests to give wrong results, as in bug #6951.
 */
enum RailTypes : uint64 {
	RAILTYPES_NONE     = 0,                      ///< No rail types
	RAILTYPES_RAIL     = 1 << RAILTYPE_RAIL,     ///< Non-electrified rails
	RAILTYPES_ELECTRIC = 1 << RAILTYPE_ELECTRIC, ///< Electrified rails
	RAILTYPES_MONO     = 1 << RAILTYPE_MONO,     ///< Monorail!
	RAILTYPES_MAGLEV   = 1 << RAILTYPE_MAGLEV,   ///< Ever fast maglev
	INVALID_RAILTYPES  = UINT64_MAX,             ///< Invalid railtypes
};
DECLARE_ENUM_AS_BIT_SET(RailTypes)

#endif /* RAIL_TYPE_H */
