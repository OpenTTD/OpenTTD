/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cargotype.cpp Implementation of cargoes. */

#include "stdafx.h"
#include "cargotype.h"
#include "newgrf_cargo.h"
#include "string_func.h"
#include "strings_func.h"
#include "core/sort_func.hpp"

#include "table/sprites.h"
#include "table/strings.h"
#include "table/cargo_const.h"

#include "safeguards.h"

CargoSpec CargoSpec::array[NUM_CARGO];

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
 * Set up the default cargo types for the given landscape type.
 * @param l Landscape
 */
void SetupCargoForClimate(LandscapeID l)
{
	assert(l < lengthof(_default_climate_cargo));

	/* Reset and disable all cargo types */
	memset(CargoSpec::array, 0, sizeof(CargoSpec::array));
	for (CargoID i = 0; i < lengthof(CargoSpec::array); i++) {
		CargoSpec::Get(i)->bitnum = INVALID_CARGO;

		/* Set defaults for newer properties, which old GRFs do not know */
		CargoSpec::Get(i)->multiplier = 0x100;
	}

	_cargo_mask = 0;

	for (CargoID i = 0; i < lengthof(_default_climate_cargo[l]); i++) {
		CargoLabel cl = _default_climate_cargo[l][i];

		/* Bzzt: check if cl is just an index into the cargo table */
		if (cl < lengthof(_default_cargo)) {
			/* Copy the indexed cargo */
			CargoSpec *cargo = CargoSpec::Get(i);
			*cargo = _default_cargo[cl];
			if (cargo->bitnum != INVALID_CARGO) SetBit(_cargo_mask, i);
			continue;
		}

		/* Loop through each of the default cargo types to see if
		 * the label matches */
		for (uint j = 0; j < lengthof(_default_cargo); j++) {
			if (_default_cargo[j].label == cl) {
				*CargoSpec::Get(i) = _default_cargo[j];

				/* Populate the available cargo mask */
				SetBit(_cargo_mask, i);
				break;
			}
		}
	}
}

/**
 * Get the cargo ID by cargo label.
 * @param cl Cargo type to get.
 * @return ID number if the cargo exists, else #CT_INVALID
 */
CargoID GetCargoIDByLabel(CargoLabel cl)
{
	const CargoSpec *cs;
	FOR_ALL_CARGOSPECS(cs) {
		if (cs->label == cl) return cs->Index();
	}

	/* No matching label was found, so it is invalid */
	return CT_INVALID;
}


/**
 * Find the CargoID of a 'bitnum' value.
 * @param bitnum 'bitnum' to find.
 * @return First CargoID with the given bitnum, or #CT_INVALID if not found or if the provided \a bitnum is invalid.
 */
CargoID GetCargoIDByBitnum(uint8 bitnum)
{
	if (bitnum == INVALID_CARGO) return CT_INVALID;

	const CargoSpec *cs;
	FOR_ALL_CARGOSPECS(cs) {
		if (cs->bitnum == bitnum) return cs->Index();
	}

	/* No matching label was found, so it is invalid */
	return CT_INVALID;
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

const CargoSpec *_sorted_cargo_specs[NUM_CARGO]; ///< Cargo specifications sorted alphabetically by name.
uint8 _sorted_cargo_specs_size;                  ///< Number of cargo specifications stored at the _sorted_cargo_specs array (including special cargoes).
uint8 _sorted_standard_cargo_specs_size;         ///< Number of standard cargo specifications stored at the _sorted_cargo_specs array.


/** Sort cargo specifications by their name. */
static int CDECL CargoSpecNameSorter(const CargoSpec * const *a, const CargoSpec * const *b)
{
	static char a_name[64];
	static char b_name[64];

	GetString(a_name, (*a)->name, lastof(a_name));
	GetString(b_name, (*b)->name, lastof(b_name));

	int res = strnatcmp(a_name, b_name); // Sort by name (natural sorting).

	/* If the names are equal, sort by cargo bitnum. */
	return (res != 0) ? res : ((*a)->bitnum - (*b)->bitnum);
}

/** Sort cargo specifications by their cargo class. */
static int CDECL CargoSpecClassSorter(const CargoSpec * const *a, const CargoSpec * const *b)
{
	int res = ((*b)->classes & CC_PASSENGERS) - ((*a)->classes & CC_PASSENGERS);
	if (res == 0) {
		res = ((*b)->classes & CC_MAIL) - ((*a)->classes & CC_MAIL);
		if (res == 0) {
			res = ((*a)->classes & CC_SPECIAL) - ((*b)->classes & CC_SPECIAL);
			if (res == 0) {
				return CargoSpecNameSorter(a, b);
			}
		}
	}

	return res;
}

/** Initialize the list of sorted cargo specifications. */
void InitializeSortedCargoSpecs()
{
	_sorted_cargo_specs_size = 0;
	const CargoSpec *cargo;
	/* Add each cargo spec to the list. */
	FOR_ALL_CARGOSPECS(cargo) {
		_sorted_cargo_specs[_sorted_cargo_specs_size] = cargo;
		_sorted_cargo_specs_size++;
	}

	/* Sort cargo specifications by cargo class and name. */
	QSortT(_sorted_cargo_specs, _sorted_cargo_specs_size, &CargoSpecClassSorter);

	_standard_cargo_mask = 0;

	_sorted_standard_cargo_specs_size = 0;
	FOR_ALL_SORTED_CARGOSPECS(cargo) {
		if (cargo->classes & CC_SPECIAL) break;
		_sorted_standard_cargo_specs_size++;
		SetBit(_standard_cargo_mask, cargo->Index());
	}
}

