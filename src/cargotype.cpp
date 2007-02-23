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

/* Quick mapping from cargo type 'bitnums' to real cargo IDs */
static CargoID _cargo_bitnum_map[32];

/* Bitmask of cargo type 'bitnums' availabe */
uint32 _cargo_mask;


void SetupCargoForClimate(LandscapeID l)
{
	assert(l < lengthof(_default_climate_cargo));

	/* Reset and disable all cargo types */
	memset(_cargo, 0, sizeof(_cargo));
	for (CargoID i = 0; i < lengthof(_cargo); i++) _cargo[i].bitnum = INVALID_CARGO;

	memset(_cargo_bitnum_map, CT_INVALID, sizeof(_cargo_bitnum_map));
	_cargo_mask = 0;

	for (CargoID i = 0; i < lengthof(_default_climate_cargo[l]); i++) {
		CargoLabel cl = _default_climate_cargo[l][i];

		/* Loop through each of the default cargo types to see if
		 * the label matches */
		for (uint j = 0; j < lengthof(_default_cargo); j++) {
			if (_default_cargo[j].label == cl) {
				_cargo[i] = _default_cargo[j];

				/* Populate the bitnum map and masks */
				byte bitnum = _cargo[i].bitnum;
				if (bitnum < lengthof(_cargo_bitnum_map)) {
					_cargo_bitnum_map[bitnum] = i;
					SETBIT(_cargo_mask, bitnum);
				}
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


CargoID GetCargoIDByBitnum(byte bitnum)
{
	assert(bitnum < lengthof(_cargo_bitnum_map));
	assert(_cargo_bitnum_map[bitnum] != CT_INVALID);
	return _cargo_bitnum_map[bitnum];
}


bool CargoSpec::IsValid() const
{
	return bitnum != INVALID_CARGO;
}
