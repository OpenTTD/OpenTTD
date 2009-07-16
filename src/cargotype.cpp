/* $Id$ */

/** @file cargotype.cpp Implementation of cargos. */

#include "stdafx.h"
#include "newgrf_cargo.h"
#include "cargotype.h"
#include "core/bitmath_func.hpp"

#include "table/sprites.h"
#include "table/strings.h"
#include "table/cargo_const.h"

CargoSpec CargoSpec::array[NUM_CARGO];

/* Bitmask of cargo types available */
uint32 _cargo_mask;


void SetupCargoForClimate(LandscapeID l)
{
	assert(l < lengthof(_default_climate_cargo));

	/* Reset and disable all cargo types */
	memset(CargoSpec::array, 0, sizeof(CargoSpec::array));
	for (CargoID i = 0; i < lengthof(CargoSpec::array); i++) CargoSpec::Get(i)->bitnum = INVALID_CARGO;

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


CargoID GetCargoIDByLabel(CargoLabel cl)
{
	const CargoSpec *cs;
	FOR_ALL_CARGOSPECS(cs) {
		if (cs->label == cl) return cs->Index();
	}

	/* No matching label was found, so it is invalid */
	return CT_INVALID;
}


/** Find the CargoID of a 'bitnum' value.
 * @param bitnum 'bitnum' to find.
 * @return First CargoID with the given bitnum, or CT_INVALID if not found.
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

