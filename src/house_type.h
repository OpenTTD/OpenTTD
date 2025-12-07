/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file house_type.h declaration of basic house types and enums */

#ifndef HOUSE_TYPE_H
#define HOUSE_TYPE_H

#include "core/enum_type.hpp"

typedef uint16_t HouseID; ///< OpenTTD ID of house types.
typedef uint16_t HouseClassID; ///< Classes of houses.

struct HouseSpec;

/** Randomisation triggers for houses */
enum class HouseRandomTrigger : uint8_t {
	/* The tile of the house has been triggered during the tileloop. */
	TileLoop,
	/*
	 * The top tile of a (multitile) building has been triggered during and all
	 * the tileloop other tiles of the same building get the same random value.
	 */
	TileLoopNorth,
};
using HouseRandomTriggers = EnumBitSet<HouseRandomTrigger, uint8_t>;

#endif /* HOUSE_TYPE_H */
