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

typedef byte RailTypeMask;

/** Allow incrementing of Track variables */
DECLARE_POSTFIX_INCREMENT(RailType);
/** Define basic enum properties */
template <> struct EnumPropsT<RailType> : MakeEnumPropsT<RailType, byte, RAILTYPE_BEGIN, RAILTYPE_END, INVALID_RAILTYPE> {};
typedef TinyEnumT<RailType> RailTypeByte;

#endif /* RAIL_TYPE_H */
