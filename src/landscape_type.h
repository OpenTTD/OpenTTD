/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file landscape_type.h Types related to the landscape. */

#ifndef LANDSCAPE_TYPE_H
#define LANDSCAPE_TYPE_H

#include "core/enum_type.hpp"
typedef uint8_t LandscapeID; ///< Landscape type. @see LandscapeType

/** Landscape types */
enum LandscapeType {
	LT_TEMPERATE  = 0,
	LT_ARCTIC     = 1,
	LT_TROPIC     = 2,
	LT_TOYLAND    = 3,

	NUM_LANDSCAPE = 4,
};

/**
 * For storing the water borders which shall be retained.
 */
enum class Borders : uint8_t {
	None = 0,
	NorthEast = 1U << 0, ///< Border on North East.
	SouthEast = 1U << 1, ///< Border on South East.
	SouthWest = 1U << 2, ///< Border on South West.
	NorthWest = 1U << 3, ///< Border on North West.
	Random = 1U << 4, ///< Randomise borders.
	All = NorthEast | SouthEast | SouthWest | NorthWest, ///< Border on all sides.
};
DECLARE_ENUM_AS_BIT_SET(Borders)

#endif /* LANDSCAPE_TYPE_H */
