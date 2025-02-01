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

/** Landscape types */
enum class LandscapeType : uint8_t {
	Temperate = 0,
	Arctic    = 1,
	Tropic    = 2,
	Toyland   = 3,
};
using LandscapeTypes = EnumBitSet<LandscapeType, uint8_t>;

static constexpr uint NUM_LANDSCAPE = 4;

/**
 * For storing the water borders which shall be retained.
 */
enum class BorderFlag : uint8_t {
	NorthEast, ///< Border on North East.
	SouthEast, ///< Border on South East.
	SouthWest, ///< Border on South West.
	NorthWest, ///< Border on North West.
	Random, ///< Randomise borders.
};
using BorderFlags = EnumBitSet<BorderFlag, uint8_t>;

static constexpr BorderFlags BORDERFLAGS_ALL = BorderFlags{BorderFlag::NorthEast, BorderFlag::SouthEast, BorderFlag::SouthWest, BorderFlag::NorthWest}; ///< Border on all sides.

#endif /* LANDSCAPE_TYPE_H */
