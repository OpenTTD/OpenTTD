/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cargo_const.h Table of all default cargo types */

/** Construction macro for a #CargoSpec structure. */
#define MK(bt, label, c, e, f, g, h, fr, te, ks1, ks2, ks3, ks4, ks5, l, m) \
		{bt, label, c, c, e, f, {g, h}, fr, te, 0, 0, ks1, ks2, ks3, ks4, ks5, l, m, NULL, NULL, 0}
/** Cargo types available by default. */
static const CargoSpec _default_cargo[] = {
	MK(  0, 'PASS', 152,  1, 3185,  0,  24, false, TE_PASSENGERS,
		STR_CARGO_PLURAL_PASSENGERS,     STR_CARGO_SINGULAR_PASSENGER,      STR_PASSENGERS, STR_QUANTITY_PASSENGERS,   STR_ABBREV_PASSENGERS,
		SPR_CARGO_PASSENGER,     CC_PASSENGERS  ),

	MK(  1, 'COAL',   6, 16, 5916,  7, 255, true,  TE_NONE,
		STR_CARGO_PLURAL_COAL,           STR_CARGO_SINGULAR_COAL,           STR_TONS,       STR_QUANTITY_COAL,         STR_ABBREV_COAL,
		SPR_CARGO_COAL,          CC_BULK        ),

	MK(  2, 'MAIL',  15,  4, 4550, 20,  90, false, TE_MAIL,
		STR_CARGO_PLURAL_MAIL,           STR_CARGO_SINGULAR_MAIL,           STR_BAGS,       STR_QUANTITY_MAIL,         STR_ABBREV_MAIL,
		SPR_CARGO_MAIL,          CC_MAIL        ),

	/* Oil in temperate and arctic */
	MK(  3, 'OIL_', 174, 16, 4437, 25, 255, true,  TE_NONE,
		STR_CARGO_PLURAL_OIL,            STR_CARGO_SINGULAR_OIL,            STR_LITERS,     STR_QUANTITY_OIL,          STR_ABBREV_OIL,
		SPR_CARGO_OIL,           CC_LIQUID      ),

	/* Oil in subtropic */
	MK(  3, 'OIL_', 174, 16, 4892, 25, 255, true,  TE_NONE,
		STR_CARGO_PLURAL_OIL,            STR_CARGO_SINGULAR_OIL,            STR_LITERS,     STR_QUANTITY_OIL,          STR_ABBREV_OIL,
		SPR_CARGO_OIL,           CC_LIQUID      ),

	MK(  4, 'LVST', 208,  3, 4322,  4,  18, true,  TE_NONE,
		STR_CARGO_PLURAL_LIVESTOCK,      STR_CARGO_SINGULAR_LIVESTOCK,      STR_ITEMS,      STR_QUANTITY_LIVESTOCK,    STR_ABBREV_LIVESTOCK,
		SPR_CARGO_LIVESTOCK,     CC_PIECE_GOODS ),

	MK(  5, 'GOOD', 194,  8, 6144,  5,  28, true,  TE_GOODS,
		STR_CARGO_PLURAL_GOODS,          STR_CARGO_SINGULAR_GOODS,          STR_CRATES,     STR_QUANTITY_GOODS,        STR_ABBREV_GOODS,
		SPR_CARGO_GOODS,         CC_EXPRESS     ),

	MK(  6, 'GRAI', 191, 16, 4778,  4,  40, true,  TE_NONE,
		STR_CARGO_PLURAL_GRAIN,          STR_CARGO_SINGULAR_GRAIN,          STR_TONS,       STR_QUANTITY_GRAIN,        STR_ABBREV_GRAIN,
		SPR_CARGO_GRAIN,         CC_BULK        ),

	MK(  6, 'WHEA', 191, 16, 4778,  4,  40, true,  TE_NONE,
		STR_CARGO_PLURAL_WHEAT,          STR_CARGO_SINGULAR_WHEAT,          STR_TONS,       STR_QUANTITY_WHEAT,        STR_ABBREV_WHEAT,
		SPR_CARGO_GRAIN,         CC_BULK        ),

	MK(  6, 'MAIZ', 191, 16, 4322,  4,  40, true,  TE_NONE,
		STR_CARGO_PLURAL_MAIZE,          STR_CARGO_SINGULAR_MAIZE,          STR_TONS,       STR_QUANTITY_MAIZE,        STR_ABBREV_MAIZE,
		SPR_CARGO_GRAIN,         CC_BULK        ),

	/* Wood in temperate and arctic */
	MK(  7, 'WOOD',  84, 16, 5005, 15, 255, true,  TE_NONE,
		STR_CARGO_PLURAL_WOOD,           STR_CARGO_SINGULAR_WOOD,           STR_TONS,       STR_QUANTITY_WOOD,         STR_ABBREV_WOOD,
		SPR_CARGO_WOOD,          CC_PIECE_GOODS ),

	/* Wood in subtropic */
	MK(  7, 'WOOD',  84, 16, 7964, 15, 255, true,  TE_NONE,
		STR_CARGO_PLURAL_WOOD,           STR_CARGO_SINGULAR_WOOD,           STR_TONS,       STR_QUANTITY_WOOD,         STR_ABBREV_WOOD,
		SPR_CARGO_WOOD,          CC_PIECE_GOODS ),

	MK(  8, 'IORE', 184, 16, 5120,  9, 255, true,  TE_NONE,
		STR_CARGO_PLURAL_IRON_ORE,       STR_CARGO_SINGULAR_IRON_ORE,       STR_TONS,       STR_QUANTITY_IRON_ORE,     STR_ABBREV_IRON_ORE,
		SPR_CARGO_IRON_ORE,      CC_BULK        ),

	MK(  9, 'STEL',  10, 16, 5688,  7, 255, true,  TE_NONE,
		STR_CARGO_PLURAL_STEEL,          STR_CARGO_SINGULAR_STEEL,          STR_TONS,       STR_QUANTITY_STEEL,        STR_ABBREV_STEEL,
		SPR_CARGO_STEEL,         CC_PIECE_GOODS ),

	MK( 10, 'VALU', 202,  2, 7509,  1,  32, true,  TE_NONE,
		STR_CARGO_PLURAL_VALUABLES,      STR_CARGO_SINGULAR_VALUABLES,      STR_BAGS,       STR_QUANTITY_VALUABLES,    STR_ABBREV_VALUABLES,
		SPR_CARGO_VALUES_GOLD,   CC_ARMOURED    ),

	MK( 10, 'GOLD', 202,  8, 5802, 10,  40, true,  TE_NONE,
		STR_CARGO_PLURAL_GOLD,           STR_CARGO_SINGULAR_GOLD,           STR_BAGS,       STR_QUANTITY_GOLD,         STR_ABBREV_GOLD,
		SPR_CARGO_VALUES_GOLD,   CC_ARMOURED    ),

	MK( 10, 'DIAM', 202,  2, 5802, 10, 255, true,  TE_NONE,
		STR_CARGO_PLURAL_DIAMONDS,       STR_CARGO_SINGULAR_DIAMOND,        STR_BAGS,       STR_QUANTITY_DIAMONDS,     STR_ABBREV_DIAMONDS,
		SPR_CARGO_DIAMONDS,      CC_ARMOURED    ),

	MK( 11, 'PAPR',  10, 16, 5461,  7,  60, true,  TE_NONE,
		STR_CARGO_PLURAL_PAPER,          STR_CARGO_SINGULAR_PAPER,          STR_TONS,       STR_QUANTITY_PAPER,        STR_ABBREV_PAPER,
		SPR_CARGO_PAPER,         CC_PIECE_GOODS ),

	MK( 12, 'FOOD',  48, 16, 5688,  0,  30, true,  TE_FOOD,
		STR_CARGO_PLURAL_FOOD,           STR_CARGO_SINGULAR_FOOD,           STR_TONS,       STR_QUANTITY_FOOD,         STR_ABBREV_FOOD,
		SPR_CARGO_FOOD,          CC_EXPRESS     | CC_REFRIGERATED),

	MK( 13, 'FRUT', 208, 16, 4209,  0,  15, true,  TE_NONE,
		STR_CARGO_PLURAL_FRUIT,          STR_CARGO_SINGULAR_FRUIT,          STR_TONS,       STR_QUANTITY_FRUIT,        STR_ABBREV_FRUIT,
		SPR_CARGO_FRUIT,         CC_BULK        | CC_REFRIGERATED),

	MK( 14, 'CORE', 184, 16, 4892, 12, 255, true,  TE_NONE,
		STR_CARGO_PLURAL_COPPER_ORE,     STR_CARGO_SINGULAR_COPPER_ORE,     STR_TONS,       STR_QUANTITY_COPPER_ORE,   STR_ABBREV_COPPER_ORE,
		SPR_CARGO_COPPER_ORE,    CC_BULK        ),

	MK( 15, 'WATR',  10, 16, 4664, 20,  80, true,  TE_WATER,
		STR_CARGO_PLURAL_WATER,          STR_CARGO_SINGULAR_WATER,          STR_LITERS,     STR_QUANTITY_WATER,        STR_ABBREV_WATER,
		SPR_CARGO_WATERCOLA,     CC_LIQUID      ),

	MK( 16, 'RUBR',   6, 16, 4437,  2,  20, true,  TE_NONE,
		STR_CARGO_PLURAL_RUBBER,         STR_CARGO_SINGULAR_RUBBER,         STR_LITERS,     STR_QUANTITY_RUBBER,       STR_ABBREV_RUBBER,
		SPR_CARGO_RUBBER,        CC_LIQUID      ),

	MK( 17, 'SUGR',   6, 16, 4437, 20, 255, true,  TE_NONE,
		STR_CARGO_PLURAL_SUGAR,          STR_CARGO_SINGULAR_SUGAR,          STR_TONS,       STR_QUANTITY_SUGAR,        STR_ABBREV_SUGAR,
		SPR_CARGO_SUGAR,         CC_BULK        ),

	MK( 18, 'TOYS', 174,  2, 5574, 25, 255, true,  TE_NONE,
		STR_CARGO_PLURAL_TOYS,           STR_CARGO_SINGULAR_TOY,            STR_ITEMS,      STR_QUANTITY_TOYS,         STR_ABBREV_TOYS,
		SPR_CARGO_TOYS,          CC_PIECE_GOODS ),

	MK( 19, 'BATT', 208,  4, 4322,  2,  30, true,  TE_NONE,
		STR_CARGO_PLURAL_BATTERIES,      STR_CARGO_SINGULAR_BATTERY,        STR_ITEMS,      STR_QUANTITY_BATTERIES,    STR_ABBREV_BATTERIES,
		SPR_CARGO_BATTERIES,     CC_PIECE_GOODS ),

	MK( 20, 'SWET', 194,  5, 6144,  8,  40, true,  TE_GOODS,
		STR_CARGO_PLURAL_CANDY,          STR_CARGO_SINGULAR_CANDY,          STR_BAGS,       STR_QUANTITY_SWEETS,       STR_ABBREV_SWEETS,
		SPR_CARGO_CANDY,         CC_EXPRESS     ),

	MK( 21, 'TOFF', 191, 16, 4778, 14,  60, true,  TE_NONE,
		STR_CARGO_PLURAL_TOFFEE,         STR_CARGO_SINGULAR_TOFFEE,         STR_TONS,       STR_QUANTITY_TOFFEE,       STR_ABBREV_TOFFEE,
		SPR_CARGO_TOFFEE,        CC_BULK        ),

	MK( 22, 'COLA',  84, 16, 4892,  5,  75, true,  TE_NONE,
		STR_CARGO_PLURAL_COLA,           STR_CARGO_SINGULAR_COLA,           STR_LITERS,     STR_QUANTITY_COLA,         STR_ABBREV_COLA,
		SPR_CARGO_WATERCOLA,     CC_LIQUID      ),

	MK( 23, 'CTCD', 184, 16, 5005, 10,  25, true,  TE_NONE,
		STR_CARGO_PLURAL_COTTON_CANDY,   STR_CARGO_SINGULAR_COTTON_CANDY,   STR_TONS,       STR_QUANTITY_CANDYFLOSS,   STR_ABBREV_CANDYFLOSS,
		SPR_CARGO_COTTONCANDY,   CC_BULK        ),

	MK( 24, 'BUBL',  10,  1, 5077, 20,  80, true,  TE_NONE,
		STR_CARGO_PLURAL_BUBBLES,        STR_CARGO_SINGULAR_BUBBLE,         STR_ITEMS,      STR_QUANTITY_BUBBLES,      STR_ABBREV_BUBBLES,
		SPR_CARGO_BUBBLES,       CC_PIECE_GOODS ),

	MK( 25, 'PLST', 202, 16, 4664, 30, 255, true,  TE_NONE,
		STR_CARGO_PLURAL_PLASTIC,        STR_CARGO_SINGULAR_PLASTIC,        STR_LITERS,     STR_QUANTITY_PLASTIC,      STR_ABBREV_PLASTIC,
		SPR_CARGO_PLASTIC,       CC_LIQUID      ),

	MK( 26, 'FZDR',  48,  2, 6250, 30,  50, true,  TE_FOOD,
		STR_CARGO_PLURAL_FIZZY_DRINKS,   STR_CARGO_SINGULAR_FIZZY_DRINK,    STR_ITEMS,      STR_QUANTITY_FIZZY_DRINKS, STR_ABBREV_FIZZY_DRINKS,
		SPR_CARGO_FIZZYDRINK,    CC_PIECE_GOODS ),

	/* Void slot in temperate */
	MK( 0xFF,    0,   1,  0, 5688,  0,  30, true,  TE_NONE,
		STR_CARGO_PLURAL_NOTHING,        STR_CARGO_SINGULAR_NOTHING,        STR_TONS,       STR_QUANTITY_NOTHING,      STR_ABBREV_NOTHING,
		SPR_ASCII_SPACE,         CC_NOAVAILABLE ),

	/* Void slot in arctic */
	MK( 0xFF,    0, 184,  0, 5120,  9, 255, true,  TE_NONE,
		STR_CARGO_PLURAL_NOTHING,        STR_CARGO_SINGULAR_NOTHING,        STR_TONS,       STR_QUANTITY_NOTHING,      STR_ABBREV_NOTHING,
		SPR_ASCII_SPACE,         CC_NOAVAILABLE ),

};


/** Table of cargo types available in each climate, by default */
static const CargoLabel _default_climate_cargo[NUM_LANDSCAPE][12] = {
	{ 'PASS', 'COAL', 'MAIL', 'OIL_', 'LVST', 'GOOD', 'GRAI', 'WOOD', 'IORE', 'STEL', 'VALU',     33, },
	{ 'PASS', 'COAL', 'MAIL', 'OIL_', 'LVST', 'GOOD', 'WHEA', 'WOOD',     34, 'PAPR', 'GOLD', 'FOOD', },
	{ 'PASS', 'RUBR', 'MAIL',      4, 'FRUT', 'GOOD', 'MAIZ',     11, 'CORE', 'WATR', 'DIAM', 'FOOD', },
	{ 'PASS', 'SUGR', 'MAIL', 'TOYS', 'BATT', 'SWET', 'TOFF', 'COLA', 'CTCD', 'BUBL', 'PLST', 'FZDR', },
};

