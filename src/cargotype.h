/* $Id$ */

/** @file cargotype.h */

#ifndef CARGOTYPE_H
#define CARGOTYPE_H


typedef uint32 CargoLabel;

enum TownEffect {
	TE_NONE,
	TE_PASSENGERS,
	TE_MAIL,
	TE_GOODS,
	TE_WATER,
	TE_FOOD,
};


struct CargoSpec {
	uint8 bitnum;
	CargoLabel label;
	uint32 grfid;
	uint8 legend_colour;
	uint8 rating_colour;
	uint8 weight;
	uint16 initial_payment;
	uint8 transit_days[2];

	bool is_freight;
	TownEffect town_effect; ///< The effect this cargo type has on towns
	uint16 multipliertowngrowth;
	uint8 callbackflags;

	StringID name;
	StringID name_plural;
	StringID units_volume;
	StringID quantifier;
	StringID abbrev;

	SpriteID sprite;

	uint16 classes;

	bool IsValid() const;
};


extern uint32 _cargo_mask;


/* Set up the default cargo types for the given landscape type */
void SetupCargoForClimate(LandscapeID l);
/* Retrieve cargo details for the given cargo ID */
const CargoSpec *GetCargo(CargoID c);
/* Get the cargo ID with the cargo label */
CargoID GetCargoIDByLabel(CargoLabel cl);

static inline bool IsCargoInClass(CargoID c, uint16 cc)
{
	return GetCargo(c)->classes & cc;
}


#endif /* CARGOTYPE_H */
