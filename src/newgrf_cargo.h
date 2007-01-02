/* $Id$ */

#ifndef NEWGRF_CARGO_H
#define NEWGRF_CARGO_H

enum {
	CC_NOAVAILABLE  = 0,
	CC_PASSENGERS   = 1 << 0,
	CC_MAIL         = 1 << 1,
	CC_EXPRESS      = 1 << 2,
	CC_ARMOURED     = 1 << 3,
	CC_BULK         = 1 << 4,
	CC_PIECE_GOODS  = 1 << 5,
	CC_LIQUID       = 1 << 6,
	CC_REFRIGERATED = 1 << 7,
};

enum GlobalCargo {
	GC_PASSENGERS   =   0,
	GC_COAL         =   1,
	GC_MAIL         =   2,
	GC_OIL          =   3,
	GC_LIVESTOCK    =   4,
	GC_GOODS        =   5,
	GC_GRAIN        =   6, // GC_WHEAT / GC_MAIZE
	GC_WOOD         =   7,
	GC_IRON_ORE     =   8,
	GC_STEEL        =   9,
	GC_VALUABLES    =  10, // GC_GOLD / GC_DIAMONDS
	GC_PAPER        =  11,
	GC_FOOD         =  12,
	GC_FRUIT        =  13,
	GC_COPPER_ORE   =  14,
	GC_WATER        =  15,
	GC_RUBBER       =  16,
	GC_SUGAR        =  17,
	GC_TOYS         =  18,
	GC_BATTERIES    =  19,
	GC_CANDY        =  20,
	GC_TOFFEE       =  21,
	GC_COLA         =  22,
	GC_COTTON_CANDY =  23,
	GC_BUBBLES      =  24,
	GC_PLASTIC      =  25,
	GC_FIZZY_DRINKS =  26,
	GC_PAPER_TEMP   =  27,
	GC_UNDEFINED    =  28, // undefined; unused slot in arctic climate
	GC_DEFAULT      =  29,
	GC_PURCHASE     =  30,
	GC_DEFAULT_NA   =  31, // New stations only
	GC_INVALID      = 255,
	NUM_GLOBAL_CID  =  32
};

VARDEF const CargoID _global_cargo_id[NUM_LANDSCAPE][NUM_CARGO];
VARDEF const uint32 _landscape_global_cargo_mask[NUM_LANDSCAPE];
VARDEF const CargoID _local_cargo_id_ctype[NUM_GLOBAL_CID];
VARDEF const uint32 cargo_classes[16];

#endif /* NEWGRF_CARGO_H */
