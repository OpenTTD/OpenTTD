/* $Id$ */

/** @file cargotype.h Types/functions related to cargos. */

#ifndef CARGOTYPE_H
#define CARGOTYPE_H

#include "cargo_type.h"
#include "gfx_type.h"
#include "strings_type.h"
#include "landscape_type.h"

typedef uint32 CargoLabel;

enum TownEffect {
	TE_NONE,
	TE_PASSENGERS,
	TE_MAIL,
	TE_GOODS,
	TE_WATER,
	TE_FOOD,
};


static const byte INVALID_CARGO = 0xFF;

struct CargoSpec {
	uint8 bitnum;
	CargoLabel label;
	uint8 legend_colour;
	uint8 rating_colour;
	uint8 weight;
	uint16 initial_payment;
	uint8 transit_days[2];

	bool is_freight;
	TownEffect town_effect; ///< The effect this cargo type has on towns
	uint16 multipliertowngrowth;
	uint8 callback_mask;

	StringID name;
	StringID name_single;
	StringID units_volume;
	StringID quantifier;
	StringID abbrev;

	SpriteID sprite;

	uint16 classes;
	const struct GRFFile *grffile;   ///< NewGRF where 'group' belongs to
	const struct SpriteGroup *group;

	bool IsValid() const
	{
		return this->bitnum != INVALID_CARGO;
	}

	/**
	 * Retrieve cargo details for the given cargo ID
	 * @param c ID of cargo
	 * @pre c is a valid cargo ID
	 */
	static CargoSpec *Get(CargoID c)
	{
		assert(c < lengthof(CargoSpec::cargo));
		return &CargoSpec::cargo[c];
	}

private:
	static CargoSpec cargo[NUM_CARGO];

	friend void SetupCargoForClimate(LandscapeID l);
	friend CargoID GetCargoIDByLabel(CargoLabel cl);
	friend CargoID GetCargoIDByBitnum(uint8 bitnum);
};

extern uint32 _cargo_mask;

/* Set up the default cargo types for the given landscape type */
void SetupCargoForClimate(LandscapeID l);
/* Get the cargo icon for a given cargo ID */
SpriteID GetCargoSprite(CargoID i);
/* Get the cargo ID with the cargo label */
CargoID GetCargoIDByLabel(CargoLabel cl);
CargoID GetCargoIDByBitnum(uint8 bitnum);

static inline bool IsCargoInClass(CargoID c, uint16 cc)
{
	return (CargoSpec::Get(c)->classes & cc) != 0;
}

#endif /* CARGOTYPE_H */
