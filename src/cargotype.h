/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file cargotype.h Types/functions related to cargoes. */

#ifndef CARGOTYPE_H
#define CARGOTYPE_H

#include "economy_type.h"
#include "cargo_type.h"
#include "gfx_type.h"
#include "newgrf_callbacks.h"
#include "strings_type.h"
#include "landscape_type.h"

/** Town growth effect when delivering cargo. */
enum class TownAcceptanceEffect : uint8_t {
	Begin = 0, ///< Used for iteration.
	None = TownAcceptanceEffect::Begin, ///< Cargo has no effect.
	Passengers, ///< Cargo behaves passenger-like.
	Mail, ///< Cargo behaves mail-like.
	Goods, ///< Cargo behaves goods/candy-like.
	Water, ///< Cargo behaves water-like.
	Food, ///< Cargo behaves food/fizzy-drinks-like.
	End, ///< End of town effects.
};

DECLARE_INCREMENT_DECREMENT_OPERATORS(TownAcceptanceEffect)

/** Town effect when producing cargo. */
enum class TownProductionEffect : uint8_t {
	None, ///< Town will not produce this cargo type.
	Passengers, ///< Cargo behaves passenger-like for production.
	Mail, ///< Cargo behaves mail-like for production.
	End, ///< End marker.

	/**
	 * Invalid town production effect. Used as a sentinel to indicate if a NewGRF has explicitly set an effect.
	 * This does not 'exist' after cargo types are finalised.
	 */
	Invalid,
};

/** Cargo classes. */
enum class CargoClass : uint8_t {
	Passengers   =  0, ///< Passengers
	Mail         =  1, ///< Mail
	Express      =  2, ///< Express cargo (Goods, Food, Candy, but also possible for passengers)
	Armoured     =  3, ///< Armoured cargo (Valuables, Gold, Diamonds)
	Bulk         =  4, ///< Bulk cargo (Coal, Grain etc., Ores, Fruit)
	PieceGoods   =  5, ///< Piece goods (Livestock, Wood, Steel, Paper)
	Liquid       =  6, ///< Liquids (Oil, Water, Rubber)
	Refrigerated =  7, ///< Refrigerated cargo (Food, Fruit)
	Hazardous    =  8, ///< Hazardous cargo (Nuclear Fuel, Explosives, etc.)
	Covered      =  9, ///< Covered/Sheltered Freight (Transportation in Box Vans, Silo Wagons, etc.)
	Oversized    = 10, ///< Oversized (stake/flatbed wagon)
	Powderized   = 11, ///< Powderized, moist protected (powder/silo wagon)
	NotPourable  = 12, ///< Not Pourable (open wagon, but not hopper wagon)
	Potable      = 13, ///< Potable / food / clean.
	NonPotable   = 14, ///< Non-potable / non-food / dirty.
	Special      = 15, ///< Special bit used for livery refit tricks instead of normal cargoes.
};

/** Bitset of \c CargoClass elements. */
using CargoClasses = EnumBitSet<CargoClass, uint16_t>;

static const uint8_t INVALID_CARGO_BITNUM = 0xFF; ///< Constant representing invalid cargo bit number.

static const uint TOWN_PRODUCTION_DIVISOR = 256; ///< By what factor divide town production.

/** Specification of a cargo type. */
struct CargoSpec {
	CargoLabel label;                ///< Unique label of the cargo type.
	uint8_t bitnum = INVALID_CARGO_BITNUM; ///< Cargo bit number, is #INVALID_CARGO_BITNUM for a non-used spec.
	PixelColour legend_colour; ///< Colour in legends and graphs.
	PixelColour rating_colour; ///< Colour used for box representing the amount of cargo waiting at the station in the station rating view.
	uint8_t weight;                    ///< Weight of a single unit of this cargo type in 1/16 ton (62.5 kg).
	uint16_t multiplier = 0x100; ///< Capacity multiplier for vehicles. (8 fractional bits)
	CargoClasses classes; ///< Classes of this cargo type. @see CargoClass
	int32_t initial_payment;           ///< Initial payment rate before inflation is applied.
	uint8_t transit_periods[2]; ///< Values used to calculate timefactor for cargo payment alghoritm.

	bool is_freight;                 ///< Cargo type is considered to be freight (affects train freight multiplier).
	TownAcceptanceEffect town_acceptance_effect; ///< The effect that delivering this cargo type has on towns. Also affects destination of subsidies.
	TownProductionEffect town_production_effect = TownProductionEffect::Invalid; ///< The effect on town cargo production.
	uint16_t town_production_multiplier = TOWN_PRODUCTION_DIVISOR; ///< Town production multiplier, if commanded by TownProductionEffect.
	CargoCallbackMasks callback_mask;             ///< Bitmask of cargo callbacks that have to be called

	StringID name;                   ///< Name of this type of cargo.
	StringID name_single;            ///< Name of a single entity of this type of cargo.
	StringID units_volume;           ///< Name of a single unit of cargo of this type.
	StringID quantifier;             ///< Text for multiple units of cargo of this type.
	StringID abbrev;                 ///< Two letter abbreviation for this cargo type.

	SpriteID sprite;                 ///< Icon to display this cargo type, may be \c 0xFFF (which means to resolve an action123 chain).

	const struct GRFFile *grffile;   ///< NewGRF where #group belongs to.
	const struct SpriteGroup *group; ///< Group of sprites used for drawing the cargo.

	Money current_payment; ///< Payment rate scaled by inflation.

	/**
	 * Determines index of this cargospec
	 * @return index (in the CargoSpec::array array)
	 */
	inline CargoType Index() const
	{
		return static_cast<CargoType>(this - CargoSpec::array);
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
	 * Retrieve cargo details for the given cargo type.
	 * @param index ID of cargo.
	 * @pre index is a valid cargo type.
	 * @return The cargo specification.
	 */
	static inline CargoSpec *Get(size_t index)
	{
		assert(index < lengthof(CargoSpec::array));
		return &CargoSpec::array[index];
	}

	SpriteID GetCargoIcon() const;

	/**
	 * Calculate the weight of \a n units of cargo.
	 * @param n For how many units of cargo calculate the weight.
	 * @return The weight of \a n units of cargo.
	 */
	inline uint64_t WeightOfNUnits(uint32_t n) const
	{
		return n * this->weight / 16u;
	}

	uint64_t WeightOfNUnitsInTrain(uint32_t n) const;

	/**
	 * Iterator to iterate all valid CargoSpec
	 */
	struct Iterator {
		using value_type = CargoSpec; ///< The type pointed by iterator.
		using pointer = CargoSpec *; ///< The pointer type to #value_type.
		using reference = CargoSpec &; ///< The reference type to #value_type.
		using difference_type = size_t; ///< Type used to store difference between two iterators.
		using iterator_category = std::forward_iterator_tag; ///< The category of the iterator.

		/**
		 * Constructs new iterator from an index.
		 * @param index Index of the CargoSpec to iterate from.
		 */
		explicit Iterator(size_t index) : index(index)
		{
			this->ValidateIndex();
		};

		/**
		 * Compare with other iterator.
		 * @param other The other iterator to compare with.
		 * @return \c true iff the other iterator matches this value e.i. they are equal.
		 */
		bool operator==(const Iterator &other) const { return this->index == other.index; }

		/**
		 * Dereference operator. Gets the pointed CargoSpec.
		 * @return A pointer to the CargoSpec this iterator points to.
		 */
		CargoSpec * operator*() const { return CargoSpec::Get(this->index); }

		/**
		 * Increment the iterator and set it to the next position.
		 * @return This iterator after incrementing.
		 */
		Iterator & operator++() { this->index++; this->ValidateIndex(); return *this; }

	private:
		size_t index; ///< Index of the CargoSpec that this iterator points to.

		/** Validate #index so it points to a valid cargo spec. */
		void ValidateIndex() { while (this->index < CargoSpec::GetArraySize() && !(CargoSpec::Get(this->index)->IsValid())) this->index++; }
	};

	/** Iterable ensemble of all valid CargoSpec. */
	struct IterateWrapper {
		size_t from; ///< Index from which the iteration started.

		/**
		 * Constructs new iterator wrapper from an stating index.
		 * @param from Index of the first CargoSpec to consider.
		 */
		IterateWrapper(size_t from = 0) : from(from) {}

		/**
		 * Get the begin iterator for this range of valid CargoSpec.
		 * @return Begin iterator.
		 */
		Iterator begin() { return Iterator(this->from); }

		/**
		 * Get the end iterator for all valid CargoSpec.
		 * @return End iterator.
		 */
		Iterator end() { return Iterator(CargoSpec::GetArraySize()); }

		/**
		 * Check whether this range of valid CargoSpec is empty.
		 * @return \c true iff the range is empty.
		 */
		bool empty() { return this->begin() == this->end(); }
	};

	/**
	 * Returns an iterable ensemble of all valid CargoSpec
	 * @param from index of the first CargoSpec to consider
	 * @return an iterable ensemble of all valid CargoSpec
	 */
	static IterateWrapper Iterate(size_t from = 0) { return IterateWrapper(from); }

	/** List of cargo specs for each Town Product Effect. */
	static inline EnumIndexArray<std::vector<const CargoSpec *>, TownProductionEffect, TownProductionEffect::End> town_production_cargoes{};

private:
	static CargoSpec array[NUM_CARGO]; ///< Array holding all CargoSpecs
	static inline std::map<CargoLabel, CargoType> label_map{}; ///< Translation map from CargoLabel to Cargo type.

	friend void SetupCargoForClimate(LandscapeType l);
	friend void BuildCargoLabelMap();
	friend inline CargoType GetCargoTypeByLabel(CargoLabel label);
	friend void FinaliseCargoArray();
};

extern CargoTypes _cargo_mask;
extern CargoTypes _standard_cargo_mask;

void SetupCargoForClimate(LandscapeType l);
bool IsDefaultCargo(CargoType cargo_type);
void BuildCargoLabelMap();

std::optional<std::string> BuildCargoAcceptanceString(const CargoArray &acceptance, StringID label);

/**
 * Get cargo assigned to a label.
 * @param label The label to deduce cargo for.
 * @return Cargo that corresponds to given label.
 */
inline CargoType GetCargoTypeByLabel(CargoLabel label)
{
	auto found = CargoSpec::label_map.find(label);
	if (found != std::end(CargoSpec::label_map)) return found->second;
	return INVALID_CARGO;
}

Dimension GetLargestCargoIconSize();

void InitializeSortedCargoSpecs();
extern std::array<uint8_t, NUM_CARGO> _sorted_cargo_types;
extern std::vector<const CargoSpec *> _sorted_cargo_specs;
extern std::span<const CargoSpec *> _sorted_standard_cargo_specs;

/**
 * Does a given cargo have any cargo class from \a cc.
 * @param cargo The cargo type to test.
 * @param cc The cargo classes to check for.
 * @return \c true iff the cargo type fits in the classes.
 */
inline bool IsCargoInClass(CargoType cargo, CargoClasses cc)
{
	return CargoSpec::Get(cargo)->classes.Any(cc);
}

/** Comparator to sort CargoType by according to desired order. */
struct CargoTypeComparator {
	/**
	 * Call operator that performs the comparison.
	 * @param lhs Cargo to compare that is in previous spot in the sorted container.
	 * @param rhs Cargo to compare that is in later spot in the sorted container.
	 * @return \c true iff the \a lhs and \a rhs should be swapped in order to sort the container.
	 */
	bool operator() (const CargoType &lhs, const CargoType &rhs) const { return _sorted_cargo_types[lhs] < _sorted_cargo_types[rhs]; }
};

#endif /* CARGOTYPE_H */
