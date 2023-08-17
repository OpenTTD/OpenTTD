/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cargo_type.h Types related to cargoes... */

#ifndef CARGO_TYPE_H
#define CARGO_TYPE_H

#include "core/enum_type.hpp"

/**
 * Cargo slots to indicate a cargo type within a game.
 * Numbers are re-used between different climates.
 * @see CargoTypes
 */
typedef byte CargoID;

/** Available types of cargo */
enum CargoType {
	/* Temperate */
	CT_PASSENGERS   =  0,
	CT_COAL         =  1,
	CT_MAIL         =  2,
	CT_OIL          =  3,
	CT_LIVESTOCK    =  4,
	CT_GOODS        =  5,
	CT_GRAIN        =  6,
	CT_WOOD         =  7,
	CT_IRON_ORE     =  8,
	CT_STEEL        =  9,
	CT_VALUABLES    = 10,

	/* Arctic */
	CT_WHEAT        =  6,
	CT_HILLY_UNUSED =  8,
	CT_PAPER        =  9,
	CT_GOLD         = 10,
	CT_FOOD         = 11,

	/* Tropic */
	CT_RUBBER       =  1,
	CT_FRUIT        =  4,
	CT_MAIZE        =  6,
	CT_COPPER_ORE   =  8,
	CT_WATER        =  9,
	CT_DIAMONDS     = 10,

	/* Toyland */
	CT_SUGAR        =  1,
	CT_TOYS         =  3,
	CT_BATTERIES    =  4,
	CT_CANDY        =  5,
	CT_TOFFEE       =  6,
	CT_COLA         =  7,
	CT_COTTON_CANDY =  8,
	CT_BUBBLES      =  9,
	CT_PLASTIC      = 10,
	CT_FIZZY_DRINKS = 11,

	NUM_ORIGINAL_CARGO = 12,
	NUM_CARGO       = 64,   ///< Maximal number of cargo types in a game.

	CT_AUTO_REFIT   = 0xFD, ///< Automatically choose cargo type when doing auto refitting.
	CT_NO_REFIT     = 0xFE, ///< Do not refit cargo of a vehicle (used in vehicle orders and auto-replace/auto-new).
	CT_INVALID      = 0xFF, ///< Invalid cargo type.
};

/** Test whether cargo type is not CT_INVALID */
inline bool IsValidCargoType(CargoType t) { return t != CT_INVALID; }
/** Test whether cargo type is not CT_INVALID */
inline bool IsValidCargoID(CargoID t) { return t != CT_INVALID; }

typedef uint64_t CargoTypes;

static const CargoTypes ALL_CARGOTYPES = (CargoTypes)UINT64_MAX;

/** Class for storing amounts of cargo */
struct CargoArray : std::array<uint, NUM_CARGO> {
	/**
	 * Get the sum of all cargo amounts.
	 * @return The sum.
	 */
	template <typename T>
	inline const T GetSum() const
	{
		return std::reduce(this->begin(), this->end(), T{});
	}

	/**
	 * Get the amount of cargos that have an amount.
	 * @return The amount.
	 */
	inline uint GetCount() const
	{
		return std::count_if(this->begin(), this->end(), [](uint amount) { return amount != 0; });
	}
};


/** Types of cargo source and destination */
enum class SourceType : byte {
	Industry,     ///< Source/destination is an industry
	Town,         ///< Source/destination is a town
	Headquarters, ///< Source/destination are company headquarters
};

typedef uint16_t SourceID; ///< Contains either industry ID, town ID or company ID (or INVALID_SOURCE)
static const SourceID INVALID_SOURCE = 0xFFFF; ///< Invalid/unknown index of source

#endif /* CARGO_TYPE_H */
