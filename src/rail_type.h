/* $Id$ */

/** @file rail_type.h The different types of rail */

#ifndef RAIL_TYPE_H
#define RAIL_TYPE_H

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
	RAILTYPE_END,                   ///< Used for iterations
	INVALID_RAILTYPE  = 0xFF        ///< Flag for invalid railtype
};

/** Allow incrementing of Track variables */
DECLARE_POSTFIX_INCREMENT(RailType);
/** Define basic enum properties */
template <> struct EnumPropsT<RailType> : MakeEnumPropsT<RailType, byte, RAILTYPE_BEGIN, RAILTYPE_END, INVALID_RAILTYPE> {};
typedef TinyEnumT<RailType> RailTypeByte;

/**
 * The different roadtypes we support, but then a bitmask of them
 */
enum RailTypes {
	RAILTYPES_NONE     = 0,                      ///< No rail types
	RAILTYPES_RAIL     = 1 << RAILTYPE_RAIL,     ///< Non-electrified rails
	RAILTYPES_ELECTRIC = 1 << RAILTYPE_ELECTRIC, ///< Electrified rails
	RAILTYPES_MONO     = 1 << RAILTYPE_MONO,     ///< Monorail!
	RAILTYPES_MAGLEV   = 1 << RAILTYPE_MAGLEV,   ///< Ever fast maglev
	RAILTYPES_ALL      = RAILTYPES_RAIL | RAILTYPES_ELECTRIC | RAILTYPES_MONO | RAILTYPES_MAGLEV, ///< All of them
	RAILTYPES_END,                               ///< Used for iterations?
	INVALID_RAILTYPES  = 0xFF                    ///< Invalid railtypes
};
DECLARE_ENUM_AS_BIT_SET(RailTypes);
template <> struct EnumPropsT<RailTypes> : MakeEnumPropsT<RailTypes, byte, RAILTYPES_NONE, RAILTYPES_END, INVALID_RAILTYPES> {};
typedef TinyEnumT<RailTypes> RailTypesByte;

#endif /* RAIL_TYPE_H */
