/* $Id$ */

/** @file newgrf_cargo.h */

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

static const CargoID CT_DEFAULT      = NUM_CARGO + 0;
static const CargoID CT_PURCHASE     = NUM_CARGO + 1;
static const CargoID CT_DEFAULT_NA   = NUM_CARGO + 2;

typedef struct CargoSpec;

SpriteID GetCustomCargoSprite(const CargoSpec *cs);
uint16 GetCargoCallback(uint16 callback, uint32 param1, uint32 param2, const CargoSpec *cs);

#endif /* NEWGRF_CARGO_H */
