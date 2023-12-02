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
#include "core/strong_typedef_type.hpp"

/** Globally unique label of a cargo type. */
using CargoLabel = StrongType::Typedef<uint32_t, struct CargoLabelTag, StrongType::Compare>;

/**
 * Cargo slots to indicate a cargo type within a game.
 */
using CargoID = byte;

/**
 * Available types of cargo
 * Labels may be re-used between different climates.
 */

/* Temperate */
static constexpr CargoLabel CT_PASSENGERS   = CargoLabel{'PASS'};
static constexpr CargoLabel CT_COAL         = CargoLabel{'COAL'};
static constexpr CargoLabel CT_MAIL         = CargoLabel{'MAIL'};
static constexpr CargoLabel CT_OIL          = CargoLabel{'OIL_'};
static constexpr CargoLabel CT_LIVESTOCK    = CargoLabel{'LVST'};
static constexpr CargoLabel CT_GOODS        = CargoLabel{'GOOD'};
static constexpr CargoLabel CT_GRAIN        = CargoLabel{'GRAI'};
static constexpr CargoLabel CT_WOOD         = CargoLabel{'WOOD'};
static constexpr CargoLabel CT_IRON_ORE     = CargoLabel{'IORE'};
static constexpr CargoLabel CT_STEEL        = CargoLabel{'STEL'};
static constexpr CargoLabel CT_VALUABLES    = CargoLabel{'VALU'};

/* Arctic */
static constexpr CargoLabel CT_WHEAT        = CargoLabel{'WHEA'};
static constexpr CargoLabel CT_PAPER        = CargoLabel{'PAPR'};
static constexpr CargoLabel CT_GOLD         = CargoLabel{'GOLD'};
static constexpr CargoLabel CT_FOOD         = CargoLabel{'FOOD'};

/* Tropic */
static constexpr CargoLabel CT_RUBBER       = CargoLabel{'RUBR'};
static constexpr CargoLabel CT_FRUIT        = CargoLabel{'FRUI'};
static constexpr CargoLabel CT_MAIZE        = CargoLabel{'MAIZ'};
static constexpr CargoLabel CT_COPPER_ORE   = CargoLabel{'CORE'};
static constexpr CargoLabel CT_WATER        = CargoLabel{'WATR'};
static constexpr CargoLabel CT_DIAMONDS     = CargoLabel{'DIAM'};

/* Toyland */
static constexpr CargoLabel CT_SUGAR        = CargoLabel{'SUGR'};
static constexpr CargoLabel CT_TOYS         = CargoLabel{'TOYS'};
static constexpr CargoLabel CT_BATTERIES    = CargoLabel{'BATT'};
static constexpr CargoLabel CT_CANDY        = CargoLabel{'SWET'};
static constexpr CargoLabel CT_TOFFEE       = CargoLabel{'TOFF'};
static constexpr CargoLabel CT_COLA         = CargoLabel{'COLA'};
static constexpr CargoLabel CT_COTTON_CANDY = CargoLabel{'CTCD'};
static constexpr CargoLabel CT_BUBBLES      = CargoLabel{'BUBL'};
static constexpr CargoLabel CT_PLASTIC      = CargoLabel{'PLST'};
static constexpr CargoLabel CT_FIZZY_DRINKS = CargoLabel{'FZDR'};

/** Dummy label for engines that carry no cargo; they actually carry 0 passengers. */
static constexpr CargoLabel CT_NONE         = CT_PASSENGERS;

static constexpr CargoLabel CT_INVALID      = CargoLabel{UINT32_MAX}; ///< Invalid cargo type.

static const CargoID NUM_ORIGINAL_CARGO = 12; ///< Original number of cargo types.
static const CargoID NUM_CARGO = 64; ///< Maximum number of cargo types in a game.

/* CARGO_AUTO_REFIT and CARGO_NO_REFIT are stored in save-games for refit-orders, so should not be changed. */
static const CargoID CARGO_AUTO_REFIT = 0xFD; ///< Automatically choose cargo type when doing auto refitting.
static const CargoID CARGO_NO_REFIT = 0xFE; ///< Do not refit cargo of a vehicle (used in vehicle orders and auto-replace/auto-renew).

static const CargoID INVALID_CARGO = UINT8_MAX;

/**
 * Special cargo filter criteria.
 * These are used by user interface code only and must not be assigned to any entity. Not all values are valid for every UI filter.
 */
namespace CargoFilterCriteria {
	static constexpr CargoID CF_ANY     = NUM_CARGO;     ///< Show all items independent of carried cargo (i.e. no filtering)
	static constexpr CargoID CF_NONE    = NUM_CARGO + 1; ///< Show only items which do not carry cargo (e.g. train engines)
	static constexpr CargoID CF_ENGINES = NUM_CARGO + 2; ///< Show only engines (for rail vehicles only)
	static constexpr CargoID CF_FREIGHT = NUM_CARGO + 3; ///< Show only vehicles which carry any freight (non-passenger) cargo

	static constexpr CargoID CF_NO_RATING   = NUM_CARGO + 4; ///< Show items with no rating (station list)
	static constexpr CargoID CF_SELECT_ALL  = NUM_CARGO + 5; ///< Select all items (station list)
	static constexpr CargoID CF_EXPAND_LIST = NUM_CARGO + 6; ///< Expand list to show all items (station list)
};

/** Test whether cargo type is not CT_INVALID */
inline bool IsValidCargoType(CargoLabel t) { return t != CT_INVALID; }
/** Test whether cargo type is not INVALID_CARGO */
inline bool IsValidCargoID(CargoID t) { return t != INVALID_CARGO; }

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
