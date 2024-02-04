/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cargotype.cpp Implementation of cargoes. */

#include "stdafx.h"
#include "cargotype.h"
#include "core/geometry_func.hpp"
#include "newgrf_cargo.h"
#include "string_func.h"
#include "strings_func.h"
#include "settings_type.h"

#include "table/sprites.h"
#include "table/strings.h"
#include "table/cargo_const.h"

#include "safeguards.h"

CargoSpec CargoSpec::array[NUM_CARGO];
std::array<std::vector<const CargoSpec *>, NUM_TPE> CargoSpec::town_production_cargoes{};

/**
 * Bitmask of cargo types available. This includes phony cargoes like regearing cargoes.
 * Initialized during a call to #SetupCargoForClimate.
 */
CargoTypes _cargo_mask;

/**
 * Bitmask of real cargo types available. Phony cargoes like regearing cargoes are excluded.
 */
CargoTypes _standard_cargo_mask;

/**
 * List of default cargo labels, used when setting up cargo types for default vehicles.
 * This is done by label so that a cargo label can be redefined in a different slot.
 */
static std::vector<CargoLabel> _default_cargo_labels;

/**
 * Set up the default cargo types for the given landscape type.
 * @param l Landscape
 */
void SetupCargoForClimate(LandscapeID l)
{
	assert(l < lengthof(_default_climate_cargo));

	_cargo_mask = 0;
	_default_cargo_labels.clear();

	/* Copy from default cargo by label or index. */
	auto insert = std::begin(CargoSpec::array);
	for (const auto &cl : _default_climate_cargo[l]) {

		/* Check if value is an index into the cargo table */
		if (std::holds_alternative<int>(cl)) {
			/* Copy the default cargo by index. */
			*insert = _default_cargo[std::get<int>(cl)];
		} else {
			/* Search for label in default cargo types and copy if found. */
			CargoLabel label = std::get<CargoLabel>(cl);
			auto found = std::find_if(std::begin(_default_cargo), std::end(_default_cargo), [&label](const CargoSpec &cs) { return cs.label == label; });
			if (found != std::end(_default_cargo)) {
				*insert = *found;
			} else {
				/* Index or label is invalid, this should not happen. */
				NOT_REACHED();
			}
		}

		if (insert->IsValid()) {
			SetBit(_cargo_mask, insert->Index());
			_default_cargo_labels.push_back(insert->label);
		}
		++insert;
	}

	/* Reset and disable remaining cargo types. */
	std::fill(insert, std::end(CargoSpec::array), CargoSpec{});

	BuildCargoLabelMap();
}

/**
 * Build cargo label map.
 * This is called multiple times during NewGRF initialization as cargos are defined, so that TranslateRefitMask() and
 * GetCargoTranslation(), also used during initialization, get the correct information.
 */
void BuildCargoLabelMap()
{
	CargoSpec::label_map.clear();
	for (const CargoSpec &cs : CargoSpec::array) {
		/* During initialization, CargoSpec can be marked valid before the label has been set. */
		if (!cs.IsValid() || cs.label == CargoLabel{0}) continue;
		/* Label already exists, don't addd again. */
		if (CargoSpec::label_map.count(cs.label) != 0) continue;

		CargoSpec::label_map.insert(std::make_pair(cs.label, cs.Index()));
	}
}

/**
 * Test if a cargo is a default cargo type.
 * @param cid Cargo ID.
 * @returns true iff the cargo type is a default cargo type.
 */
bool IsDefaultCargo(CargoID cid)
{
	auto cs = CargoSpec::Get(cid);
	if (!cs->IsValid()) return false;

	CargoLabel label = cs->label;
	return std::any_of(std::begin(_default_cargo_labels), std::end(_default_cargo_labels), [&label](const CargoLabel &cl) { return cl == label; });
}

/**
 * Get dimensions of largest cargo icon.
 * @return Dimensions of largest cargo icon.
 */
Dimension GetLargestCargoIconSize()
{
	Dimension size = {0, 0};
	for (const CargoSpec *cs : _sorted_cargo_specs) {
		size = maxdim(size, GetSpriteSize(cs->GetCargoIcon()));
	}
	return size;
}

/**
 * Find the CargoID of a 'bitnum' value.
 * @param bitnum 'bitnum' to find.
 * @return First CargoID with the given bitnum, or #INVALID_CARGO if not found or if the provided \a bitnum is invalid.
 */
CargoID GetCargoIDByBitnum(uint8_t bitnum)
{
	if (bitnum == INVALID_CARGO_BITNUM) return INVALID_CARGO;

	for (const CargoSpec *cs : CargoSpec::Iterate()) {
		if (cs->bitnum == bitnum) return cs->Index();
	}

	/* No matching label was found, so it is invalid */
	return INVALID_CARGO;
}

/**
 * Get sprite for showing cargo of this type.
 * @return Sprite number to use.
 */
SpriteID CargoSpec::GetCargoIcon() const
{
	SpriteID sprite = this->sprite;
	if (sprite == 0xFFFF) {
		/* A value of 0xFFFF indicates we should draw a custom icon */
		sprite = GetCustomCargoSprite(this);
	}

	if (sprite == 0) sprite = SPR_CARGO_GOODS;

	return sprite;
}

std::array<uint8_t, NUM_CARGO> _sorted_cargo_types; ///< Sort order of cargoes by cargo ID.
std::vector<const CargoSpec *> _sorted_cargo_specs;   ///< Cargo specifications sorted alphabetically by name.
std::span<const CargoSpec *> _sorted_standard_cargo_specs; ///< Standard cargo specifications sorted alphabetically by name.

/** Sort cargo specifications by their name. */
static bool CargoSpecNameSorter(const CargoSpec * const &a, const CargoSpec * const &b)
{
	std::string a_name = GetString(a->name);
	std::string b_name = GetString(b->name);

	int res = StrNaturalCompare(a_name, b_name); // Sort by name (natural sorting).

	/* If the names are equal, sort by cargo bitnum. */
	return (res != 0) ? res < 0 : (a->bitnum < b->bitnum);
}

/** Sort cargo specifications by their cargo class. */
static bool CargoSpecClassSorter(const CargoSpec * const &a, const CargoSpec * const &b)
{
	int res = (b->classes & CC_PASSENGERS) - (a->classes & CC_PASSENGERS);
	if (res == 0) {
		res = (b->classes & CC_MAIL) - (a->classes & CC_MAIL);
		if (res == 0) {
			res = (a->classes & CC_SPECIAL) - (b->classes & CC_SPECIAL);
			if (res == 0) {
				return CargoSpecNameSorter(a, b);
			}
		}
	}

	return res < 0;
}

/** Initialize the list of sorted cargo specifications. */
void InitializeSortedCargoSpecs()
{
	for (auto &tpc : CargoSpec::town_production_cargoes) tpc.clear();
	_sorted_cargo_specs.clear();
	/* Add each cargo spec to the list, and determine the largest cargo icon size. */
	for (const CargoSpec *cargo : CargoSpec::Iterate()) {
		_sorted_cargo_specs.push_back(cargo);
	}

	/* Sort cargo specifications by cargo class and name. */
	std::sort(_sorted_cargo_specs.begin(), _sorted_cargo_specs.end(), &CargoSpecClassSorter);

	/* Populate */
	for (auto it = std::begin(_sorted_cargo_specs); it != std::end(_sorted_cargo_specs); ++it) {
		_sorted_cargo_types[(*it)->Index()] = static_cast<uint8_t>(it - std::begin(_sorted_cargo_specs));
	}

	/* Count the number of standard cargos and fill the mask. */
	_standard_cargo_mask = 0;
	uint8_t nb_standard_cargo = 0;
	for (const auto &cargo : _sorted_cargo_specs) {
		assert(cargo->town_production_effect != INVALID_TPE);
		CargoSpec::town_production_cargoes[cargo->town_production_effect].push_back(cargo);
		if (cargo->classes & CC_SPECIAL) break;
		nb_standard_cargo++;
		SetBit(_standard_cargo_mask, cargo->Index());
	}

	/* _sorted_standard_cargo_specs is a subset of _sorted_cargo_specs. */
	_sorted_standard_cargo_specs = { _sorted_cargo_specs.data(), nb_standard_cargo };
}

uint64_t CargoSpec::WeightOfNUnitsInTrain(uint32_t n) const
{
	if (this->is_freight) n *= _settings_game.vehicle.freight_trains;
	return this->WeightOfNUnits(n);
}
