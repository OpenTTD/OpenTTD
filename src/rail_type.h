/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file rail_type.h The different types of rail */

#ifndef RAIL_TYPE_H
#define RAIL_TYPE_H

#include "core/enum_type.hpp"

typedef uint32_t RailTypeLabel;

static const RailTypeLabel RAILTYPE_LABEL_RAIL     = 'RAIL';
static const RailTypeLabel RAILTYPE_LABEL_ELECTRIC = 'ELRL';
static const RailTypeLabel RAILTYPE_LABEL_MONO     = 'MONO';
static const RailTypeLabel RAILTYPE_LABEL_MAGLEV   = 'MGLV';

/**
 * Enumeration for all possible railtypes.
 */
enum RailType : uint8_t {
	RAILTYPE_BEGIN    = 0,          ///< Used for iterations
	RAILTYPE_RAIL     = 0,          ///< Standard non-electric rails
	RAILTYPE_ELECTRIC = 1,          ///< Electric rails
	RAILTYPE_MONO     = 2,          ///< Monorail
	RAILTYPE_MAGLEV   = 3,          ///< Maglev
	RAILTYPE_END      = 64,         ///< Used for iterations
	INVALID_RAILTYPE  = 0xFF,       ///< Flag for invalid railtype
};

/** Allow incrementing of Track variables */
DECLARE_INCREMENT_DECREMENT_OPERATORS(RailType)

using RailTypes = EnumBitSet<RailType, uint64_t>;

static constexpr RailTypes INVALID_RAILTYPES{UINT64_MAX};

#endif /* RAIL_TYPE_H */
