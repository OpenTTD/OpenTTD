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
	uint8 callback_mask;

	StringID name_plural;
	StringID name;
	StringID units_volume;
	StringID quantifier;
	StringID abbrev;

	SpriteID sprite;

	uint16 classes;
	const struct SpriteGroup *group;

	bool IsValid() const;
};


extern uint32 _cargo_mask;
extern CargoSpec _cargo[NUM_CARGO];


/* Set up the default cargo types for the given landscape type */
void SetupCargoForClimate(LandscapeID l);
/* Retrieve cargo details for the given cargo ID */
const CargoSpec *GetCargo(CargoID c);
/* Get the cargo ID with the cargo label */
CargoID GetCargoIDByLabel(CargoLabel cl);
CargoID GetCargoIDByBitnum(uint8 bitnum);

static inline bool IsCargoInClass(CargoID c, uint16 cc)
{
	return (GetCargo(c)->classes & cc) != 0;
}


#endif /* CARGOTYPE_H */
