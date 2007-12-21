/* $Id$ */

/** @file newgrf_cargo.h */

#ifndef NEWGRF_CARGO_H
#define NEWGRF_CARGO_H

#include "newgrf_callbacks.h"
#include "cargo_type.h"

enum {
	CC_NOAVAILABLE  = 0,       ///< No cargo class has been specified
	CC_PASSENGERS   = 1 <<  0, ///< Passengers
	CC_MAIL         = 1 <<  1, ///< Mail
	CC_EXPRESS      = 1 <<  2, ///< Express cargo (Goods, Food, Candy, but also possible for passengers)
	CC_ARMOURED     = 1 <<  3, ///< Armoured cargo (Valuables, Gold, Diamonds)
	CC_BULK         = 1 <<  4, ///< Bulk cargo (Coal, Grain etc., Ores, Fruit)
	CC_PIECE_GOODS  = 1 <<  5, ///< Piece goods (Livestock, Wood, Steel, Paper)
	CC_LIQUID       = 1 <<  6, ///< Liquids (Oil, Water, Rubber)
	CC_REFRIGERATED = 1 <<  7, ///< Refrigerated cargo (Food, Fruit)
	CC_HAZARDOUS    = 1 <<  8, ///< Hazardous cargo (Nucleair Fuel, Explosives, etc.)
	CC_COVERED      = 1 <<  9, ///< Covered/Sheltered Freight (Transporation in Box Vans, Silo Wagons, etc.)
	CC_SPECIAL      = 1 << 15  ///< Special bit used for livery refit tricks instead of normal cargoes.
};

static const CargoID CT_DEFAULT      = NUM_CARGO + 0;
static const CargoID CT_PURCHASE     = NUM_CARGO + 1;
static const CargoID CT_DEFAULT_NA   = NUM_CARGO + 2;

/* Forward declarations of structs used */
struct CargoSpec;
struct GRFFile;

SpriteID GetCustomCargoSprite(const CargoSpec *cs);
uint16 GetCargoCallback(CallbackID callback, uint32 param1, uint32 param2, const CargoSpec *cs);
CargoID GetCargoTranslation(uint8 cargo, const GRFFile *grffile, bool usebit = false);
uint8 GetReverseCargoTranslation(CargoID cargo, const GRFFile *grffile);

#endif /* NEWGRF_CARGO_H */
