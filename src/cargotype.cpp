/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "macros.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "newgrf_cargo.h"
#include "cargotype.h"

#include "table/cargo_const.h"

static CargoSpec _cargo[NUM_CARGO];

static const byte INVALID_CARGO = 0xFF;


void SetupCargoForClimate(LandscapeID l)
{
	assert(l < lengthof(_default_climate_cargo));

	/* Reset and disable all cargo types */
	memset(_cargo, 0, sizeof(_cargo));
	for (CargoID i = 0; i < lengthof(_cargo); i++) _cargo[i].bitnum = INVALID_CARGO;

	for (CargoID i = 0; i < lengthof(_default_climate_cargo[l]); i++) {
		CargoLabel cl = _default_climate_cargo[l][i];

		/* Loop through each of the default cargo types to see if
		 * the label matches */
		for (uint j = 0; j < lengthof(_default_cargo); j++) {
			if (_default_cargo[j].label == cl) {
				_cargo[i] = _default_cargo[j];
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

