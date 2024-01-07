/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cargotype.h Types/functions related to cargoes. */

#ifndef CARGOTYPE_H
#define CARGOTYPE_H

#include "economy_type.h"
#include "cargo_type.h"
#include "gfx_type.h"
#include "strings_type.h"
#include "landscape_type.h"
#include "core/bitmath_func.hpp"

/** Globally unique label of a cargo type. */
typedef uint32_t CargoLabel;

/** Town growth effect when delivering cargo. */
enum TownAcceptanceEffect : byte {
	TAE_BEGIN = 0,
	TAE_NONE = TAE_BEGIN, ///< Cargo has no effect.
	TAE_PASSENGERS, ///< Cargo behaves passenger-like.
	TAE_MAIL, ///< Cargo behaves mail-like.
	TAE_GOODS, ///< Cargo behaves goods/candy-like.
	TAE_WATER, ///< Cargo behaves water-like.
	TAE_FOOD, ///< Cargo behaves food/fizzy-drinks-like.
	TAE_END, ///< End of town effects.
	NUM_TAE = TAE_END, ///< Amount of town effects.
};

/** Town effect when producing cargo. */
enum TownProductionEffect : byte {
	TPE_NONE, ///< Town will not produce this cargo type.
	TPE_PASSENGERS, ///< Cargo behaves passenger-like for production.
	TPE_MAIL, ///< Cargo behaves mail-like for production.
	NUM_TPE,

	/**
	 * Invalid town production effect. Used as a sentinel to indicate if a NewGRF has explicitly set an effect.
	 * This does not 'exist' after cargo types are finalised.
	 */
	INVALID_TPE,
};

/** Cargo classes. */
enum CargoClass {
	CC_NOAVAILABLE  = 0,       ///< No cargo class has been specified
	CC_PASSENGERS   = 1 <<  0, ///< Passengers
	CC_MAIL         = 1 <<  1, ///< Mail
	CC_EXPRESS      = 1 <<  2, ///< Express cargo (Goods, Food, Candy, but also possible for passengers)
	CC_ARMOURED     = 1 <<  3, ///< Armoured cargo (Valuables, Gold, Diamonds)
	CC_BULK         = 1 <<  4, ///< Bulk cargo (Coal, Grain etc., Ores, Fruit)
	CC_PIECE_GOODS  = 1 <<  5, ///< Piece goods (Livestock, Wood, Steel, Paper)
	CC_LIQUID       = 1 <<  6, ///< Liquids (Oil, Water, Rubber)
	CC_REFRIGERATED = 1 <<  7, ///< Refrigerated cargo (Food, Fruit)
	CC_HAZARDOUS    = 1 <<  8, ///< Hazardous cargo (Nuclear Fuel, Explosives, etc.)
	CC_COVERED      = 1 <<  9, ///< Covered/Sheltered Freight (Transportation in Box Vans, Silo Wagons, etc.)
	CC_SPECIAL      = 1 << 15, ///< Special bit used for livery refit tricks instead of normal cargoes.
};

static const byte INVALID_CARGO_BITNUM = 0xFF; ///< Constant representing invalid cargo

/** Specification of a cargo type. */
struct CargoSpec {
	CargoLabel label;                ///< Unique label of the cargo type.
	uint8_t bitnum{INVALID_CARGO_BITNUM}; ///< Cargo bit number, is #INVALID_CARGO_BITNUM for a non-used spec.
	uint8_t legend_colour;
	uint8_t rating_colour;
	uint8_t weight;                    ///< Weight of a single unit of this cargo type in 1/16 ton (62.5 kg).
	uint16_t multiplier{0x100}; ///< Capacity multiplier for vehicles. (8 fractional bits)
	uint16_t classes;                  ///< Classes of this cargo type. @see CargoClass
	int32_t initial_payment;           ///< Initial payment rate before inflation is applied.
	uint8_t transit_periods[2];

	bool is_freight;                 ///< Cargo type is considered to be freight (affects train freight multiplier).
	TownAcceptanceEffect town_acceptance_effect; ///< The effect that delivering this cargo type has on towns. Also affects destination of subsidies.
	TownProductionEffect town_production_effect{INVALID_TPE}; ///< The effect on town cargo production.
	uint8_t callback_mask;             ///< Bitmask of cargo callbacks that have to be called

	StringID name;                   ///< Name of this type of cargo.
	StringID name_single;            ///< Name of a single entity of this type of cargo.
	StringID units_volume;           ///< Name of a single unit of cargo of this type.
	StringID quantifier;             ///< Text for multiple units of cargo of this type.
	StringID abbrev;                 ///< Two letter abbreviation for this cargo type.

	SpriteID sprite;                 ///< Icon to display this cargo type, may be \c 0xFFF (which means to resolve an action123 chain).

	const struct GRFFile *grffile;   ///< NewGRF where #group belongs to.
	const struct SpriteGroup *group;

	Money current_payment;

	/**
	 * Determines index of this cargospec
	 * @return index (in the CargoSpec::array array)
	 */
	inline CargoID Index() const
	{
		return this - CargoSpec::array;
	}

	/**
	 * Tests for validity of this cargospec
	 * @return is this cargospec valid?
	 * @note assert(cs->IsValid()) can be triggered when GRF config is modified
	 */
	inline bool IsValid() const
	{
		return this->bitnum != INVALID_CARGO_BITNUM;
	}

	/**
	 * Total number of cargospecs, both valid and invalid
	 * @return length of CargoSpec::array
	 */
	static inline size_t GetArraySize()
	{
		return lengthof(CargoSpec::array);
	}

	/**
	 * Retrieve cargo details for the given cargo ID
	 * @param index ID of cargo
	 * @pre index is a valid cargo ID
	 */
	static inline CargoSpec *Get(size_t index)
	{
		assert(index < lengthof(CargoSpec::array));
		return &CargoSpec::array[index];
	}

	SpriteID GetCargoIcon() const;

	inline uint64_t WeightOfNUnits(uint32_t n) const
	{
		return n * this->weight / 16u;
	}

	uint64_t WeightOfNUnitsInTrain(uint32_t n) const;

	/**
	 * Iterator to iterate all valid CargoSpec
	 */
	struct Iterator {
		typedef CargoSpec value_type;
		typedef CargoSpec *pointer;
		typedef CargoSpec &reference;
		typedef size_t difference_type;
		typedef std::forward_iterator_tag iterator_category;

		explicit Iterator(size_t index) : index(index)
		{
			this->ValidateIndex();
		};

		bool operator==(const Iterator &other) const { return this->index == other.index; }
		bool operator!=(const Iterator &other) const { return !(*this == other); }
		CargoSpec * operator*() const { return CargoSpec::Get(this->index); }
		Iterator & operator++() { this->index++; this->ValidateIndex(); return *this; }

	private:
		size_t index;
		void ValidateIndex() { while (this->index < CargoSpec::GetArraySize() && !(CargoSpec::Get(this->index)->IsValid())) this->index++; }
	};

	/*
	 * Iterable ensemble of all valid CargoSpec
	 */
	struct IterateWrapper {
		size_t from;
		IterateWrapper(size_t from = 0) : from(from) {}
		Iterator begin() { return Iterator(this->from); }
		Iterator end() { return Iterator(CargoSpec::GetArraySize()); }
		bool empty() { return this->begin() == this->end(); }
	};

	/**
	 * Returns an iterable ensemble of all valid CargoSpec
	 * @param from index of the first CargoSpec to consider
	 * @return an iterable ensemble of all valid CargoSpec
	 */
	static IterateWrapper Iterate(size_t from = 0) { return IterateWrapper(from); }

	/** List of cargo specs for each Town Product Effect. */
	static std::array<std::vector<const CargoSpec *>, NUM_TPE> town_production_cargoes;

private:
	static CargoSpec array[NUM_CARGO]; ///< Array holding all CargoSpecs

	friend void SetupCargoForClimate(LandscapeID l);
	friend void FinaliseCargoArray();
};

extern CargoTypes _cargo_mask;
extern CargoTypes _standard_cargo_mask;

void SetupCargoForClimate(LandscapeID l);
CargoID GetCargoIDByLabel(CargoLabel cl);
CargoID GetCargoIDByBitnum(uint8_t bitnum);
CargoID GetDefaultCargoID(LandscapeID l, CargoType ct);
Dimension GetLargestCargoIconSize();

void InitializeSortedCargoSpecs();
extern std::array<uint8_t, NUM_CARGO> _sorted_cargo_types;
extern std::vector<const CargoSpec *> _sorted_cargo_specs;
extern std::span<const CargoSpec *> _sorted_standard_cargo_specs;

/**
 * Does cargo \a c have cargo class \a cc?
 * @param c  Cargo type.
 * @param cc Cargo class.
 * @return The type fits in the class.
 */
inline bool IsCargoInClass(CargoID c, CargoClass cc)
{
	return (CargoSpec::Get(c)->classes & cc) != 0;
}

using SetCargoBitIterator = SetBitIterator<CargoID, CargoTypes>;

/** Comparator to sort CargoID by according to desired order. */
struct CargoIDComparator {
	bool operator() (const CargoID &lhs, const CargoID &rhs) const { return _sorted_cargo_types[lhs] < _sorted_cargo_types[rhs]; }
};

#endif /* CARGOTYPE_H */
