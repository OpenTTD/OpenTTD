/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "newgrf_cargo.h"

/** TRANSLATE FROM LOCAL CARGO TO GLOBAL CARGO ID'S.
 * This maps the per-landscape cargo ID's to globally unique cargo ID's usable ie. in
 * the custom GRF  files. It is basically just a transcribed table from TTDPatch's newgrf.txt.
 */
const CargoID _global_cargo_id[NUM_LANDSCAPE][NUM_CARGO] = {
	/* LT_NORMAL */ {GC_PASSENGERS, GC_COAL,   GC_MAIL, GC_OIL,  GC_LIVESTOCK, GC_GOODS, GC_GRAIN,  GC_WOOD, GC_IRON_ORE,     GC_STEEL,   GC_VALUABLES, GC_PAPER_TEMP},
	/* LT_HILLY */  {GC_PASSENGERS, GC_COAL,   GC_MAIL, GC_OIL,  GC_LIVESTOCK, GC_GOODS, GC_GRAIN,  GC_WOOD, GC_INVALID,      GC_PAPER,   GC_VALUABLES, GC_FOOD },
	/* LT_DESERT */ {GC_PASSENGERS, GC_RUBBER, GC_MAIL, GC_OIL,  GC_FRUIT,     GC_GOODS, GC_GRAIN,  GC_WOOD, GC_COPPER_ORE,   GC_WATER,   GC_VALUABLES, GC_FOOD },
	/* LT_CANDY */  {GC_PASSENGERS, GC_SUGAR,  GC_MAIL, GC_TOYS, GC_BATTERIES, GC_CANDY, GC_TOFFEE, GC_COLA, GC_COTTON_CANDY, GC_BUBBLES, GC_PLASTIC,   GC_FIZZY_DRINKS },
	/**
	 * - GC_INVALID (255) means that  cargo is not available for that climate
	 * - GC_PAPER_TEMP (27) is paper in  temperate climate in TTDPatch
	 * Following can  be renumbered:
	 * - GC_DEFAULT (29) is the defa ult cargo for the purpose of spritesets
	 * - GC_PURCHASE (30) is the purchase list image (the equivalent of 0xff) for the purpose of spritesets
	 */
};

/** BEGIN --- TRANSLATE FROM GLOBAL CARGO TO LOCAL CARGO ID'S **/
/** Map global cargo ID's to local-cargo ID's */
const CargoID _local_cargo_id_ctype[NUM_GLOBAL_CID] = {
	CT_PASSENGERS, CT_COAL,    CT_MAIL,         CT_OIL,       CT_LIVESTOCK, CT_GOODS,  CT_GRAIN,      CT_WOOD,         /*  0- 7 */
	CT_IRON_ORE,   CT_STEEL,   CT_VALUABLES,    CT_PAPER,     CT_FOOD,      CT_FRUIT,  CT_COPPER_ORE, CT_WATER,        /*  8-15 */
	CT_RUBBER,     CT_SUGAR,   CT_TOYS,         CT_BATTERIES, CT_CANDY,     CT_TOFFEE, CT_COLA,       CT_COTTON_CANDY, /* 16-23 */
	CT_BUBBLES,    CT_PLASTIC, CT_FIZZY_DRINKS, CT_PAPER      /* unsup. */, CT_HILLY_UNUSED,                           /* 24-28 */
	CT_INVALID,    CT_INVALID                                                                                          /* 29-30 */
};

/** Bitmasked value where the global cargo ID is available in landscape
 * 0: LT_NORMAL, 1: LT_HILLY, 2: LT_DESERT, 3: LT_CANDY */
#define MC(cargo) (1 << cargo)
const uint32 _landscape_global_cargo_mask[NUM_LANDSCAPE] =
{ /* LT_NORMAL: temperate */
	MC(GC_PASSENGERS) | MC(GC_COAL) | MC(GC_MAIL)  | MC(GC_OIL)   | MC(GC_LIVESTOCK) | MC(GC_GOODS) | MC(GC_GRAIN)     | MC(GC_WOOD) | MC(GC_IRON_ORE)     | MC(GC_STEEL)      | MC(GC_VALUABLES),
	/* LT_HILLY: arctic */
	MC(GC_PASSENGERS) | MC(GC_COAL) | MC(GC_MAIL)  | MC(GC_OIL)   | MC(GC_LIVESTOCK) | MC(GC_GOODS) | MC(GC_GRAIN)     | MC(GC_WOOD) | MC(GC_VALUABLES)    | MC(GC_PAPER)      | MC(GC_FOOD),
	/* LT_DESERT: rainforest/desert */
	MC(GC_PASSENGERS) | MC(GC_MAIL) | MC(GC_OIL)   | MC(GC_GOODS) | MC(GC_GRAIN)     | MC(GC_WOOD)  | MC(GC_VALUABLES) | MC(GC_FOOD) | MC(GC_FRUIT)        | MC(GC_COPPER_ORE) | MC(GC_WATER)   | MC(GC_RUBBER),
	/* LT_CANDY: toyland */
	MC(GC_PASSENGERS) | MC(GC_MAIL) | MC(GC_SUGAR) | MC(GC_TOYS)  | MC(GC_BATTERIES) | MC(GC_CANDY) | MC(GC_TOFFEE)    | MC(GC_COLA) | MC(GC_COTTON_CANDY) | MC(GC_BUBBLES)    | MC(GC_PLASTIC) | MC(GC_FIZZY_DRINKS)
};
/** END   --- TRANSLATE FROM GLOBAL CARGO TO LOCAL CARGO ID'S **/

/**
 * Bitmask of classes for cargo types.
 */
const uint32 cargo_classes[16] = {
	/* Passengers */ MC(GC_PASSENGERS),
	/* Mail       */ MC(GC_MAIL),
	/* Express    */ MC(GC_GOODS)     | MC(GC_FOOD)  | MC(GC_CANDY),
	/* Armoured   */ MC(GC_VALUABLES),
	/* Bulk       */ MC(GC_COAL)      | MC(GC_GRAIN) | MC(GC_IRON_ORE) | MC(GC_COPPER_ORE) | MC(GC_FRUIT)   | MC(GC_SUGAR)     | MC(GC_TOFFEE)  | MC(GC_COTTON_CANDY),
	/* Piece      */ MC(GC_LIVESTOCK) | MC(GC_WOOD)  | MC(GC_STEEL)    | MC(GC_PAPER)      | MC(GC_TOYS)    | MC(GC_BATTERIES) | MC(GC_BUBBLES) | MC(GC_FIZZY_DRINKS),
	/* Liquids    */ MC(GC_OIL)       | MC(GC_WATER) | MC(GC_RUBBER)   | MC(GC_COLA)       | MC(GC_PLASTIC),
	/* Chilled    */ MC(GC_FOOD)      | MC(GC_FRUIT),
	/* Undefined  */ 0, 0, 0, 0, 0, 0, 0, 0
};
#undef MC

/**
 *there are 32 slots available per climate with newcargo.*/
#define MAXSLOTS 32
