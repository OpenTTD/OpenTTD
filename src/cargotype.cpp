/* $Id$ */

/** @file cargotype.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "newgrf_cargo.h"
#include "cargotype.h"
#include "core/bitmath_func.hpp"

#include "table/cargo_const.h"

CargoSpec _cargo[NUM_CARGO];

static const byte INVALID_CARGO = 0xFF;

/* Bitmask of cargo types available */
uint32 _cargo_mask;


void SetupCargoForClimate(LandscapeID l)
{
	assert(l < lengthof(_default_climate_cargo));

	/* Reset and disable all cargo types */
	memset(_cargo, 0, sizeof(_cargo));
	for (CargoID i = 0; i < lengthof(_cargo); i++) _cargo[i].bitnum = INVALID_CARGO;

	_cargo_mask = 0;

	for (CargoID i = 0; i < lengthof(_default_climate_cargo[l]); i++) {
		CargoLabel cl = _default_climate_cargo[l][i];

		/* Bzzt: check if cl is just an index into the cargo table */
		if (cl < lengthof(_default_cargo)) {
			/* Copy the indexed cargo */
			_cargo[i] = _default_cargo[cl];
			if (_cargo[i].bitnum != INVALID_CARGO) SetBit(_cargo_mask, i);
			continue;
		}

		/* Loop through each of the default cargo types to see if
		 * the label matches */
		for (uint j = 0; j < lengthof(_default_cargo); j++) {
			if (_default_cargo[j].label == cl) {
				_cargo[i] = _default_cargo[j];

				/* Populate the available cargo mask */
				SetBit(_cargo_mask, i);
				break;
			}
		}
	}
}


const CargoSpec *GetCargo(CargoID c)
{
	assert(c < lengthof(_cargo));
	return &_cargo[c];
}


bool CargoSpec::IsValid() const
{
	return bitnum != INVALID_CARGO;
}


CargoID GetCargoIDByLabel(CargoLabel cl)
{
	for (CargoID c = 0; c < lengthof(_cargo); c++) {
		if (_cargo[c].bitnum == INVALID_CARGO) continue;
		if (_cargo[c].label == cl) return c;
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

	for (CargoID c = 0; c < lengthof(_cargo); c++) {
		if (_cargo[c].bitnum == bitnum) return c;
	}

	/* No matching label was found, so it is invalid */
	return CT_INVALID;
}

