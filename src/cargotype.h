/* $Id$ */

#ifndef CARGOTYPE_H
#define CARGOTYPE_H


typedef uint32 CargoLabel;


typedef struct CargoSpec {
	uint8 bitnum;
	CargoLabel label;
	uint32 grfid;
	uint8 legend_colour;
	uint8 rating_colour;
	uint8 weight;
	uint16 initial_payment;
	uint8 transit_days[2];

	bool is_freight;
	uint8 substitutetowngrowth;
	uint16 multipliertowngrowth;
	uint8 callbackflags;

	StringID name;
	StringID name_plural;
	StringID units_volume;
	StringID quantifier;
	StringID abbrev;

	SpriteID sprite;

	uint16 classes;
} CargoSpec;


extern uint32 _cargo_mask;


/* Set up the default cargo types for the given landscape type */
void SetupCargoForClimate(LandscapeID l);
/* Retrieve cargo details for the given cargo ID */
const CargoSpec *GetCargo(CargoID c);
/* Get the cargo ID of a cargo bitnum */
CargoID GetCargoIDByBitnum(byte bitnum);


#endif /* CARGOTYPE_H */
