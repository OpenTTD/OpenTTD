/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cargo_const.h Table of all default cargo types */

/** Construction macros for the #CargoSpec StringID entries. */
#define MK_STR_CARGO_PLURAL(label_plural) STR_CARGO_PLURAL_ ## label_plural
#define MK_STR_CARGO_SINGULAR(label_singular) STR_CARGO_SINGULAR_ ## label_singular
#define MK_STR_QUANTITY(label_plural) STR_QUANTITY_ ## label_plural
#define MK_STR_ABBREV(label_plural) STR_ABBREV_ ## label_plural
/** Construction macros for the #CargoSpec SpriteID entry. */
#define MK_SPRITE(label_plural) SPR_CARGO_ ## label_plural

/**
 * Construction macro for a #CargoSpec structure.
 * The order of arguments matches the order in which they are defined in #CargoSpec.
 * Some macros are used to automatically expand to the correct StringID consts, this
 * means that adding/changing a cargo spec requires updating of the following strings:
 * - STR_CARGO_PLURAL_<str_plural>
 * - STR_CARGO_SINGULAR_<str_singular>
 * - STR_QUANTITY_<str_plural>
 * - STR_ABBREV_<str_plural>
 * And the following sprite:
 * - SPR_CARGO_<str_plural>
 *
 * @param bt           Cargo bit number, is #INVALID_CARGO_BITNUM for a non-used spec.
 * @param label        Unique label of the cargo type.
 * @param colour       CargoSpec->legend_colour and CargoSpec->rating_colour.
 * @param weight       Weight of a single unit of this cargo type in 1/16 ton (62.5 kg).
 * @param mult         Capacity multiplier for vehicles. (8 fractional bits).
 * @param ip           CargoSpec->initial_payment.
 * @param td1          CargoSpec->transit_periods[0].
 * @param td2          CargoSpec->transit_periods[1].
 * @param freight      Cargo type is considered to be freight (affects train freight multiplier).
 * @param tae          The effect that delivering this cargo type has on towns.
 * @param str_plural   The name suffix used to populate CargoSpec->name, CargoSpec->quantifier,
 *                     CargoSpec->abbrev and CargoSpec->sprite. See above for more detailed information.
 * @param str_singular The name suffix used to populate CargoSpec->name_single. See above for more information.
 * @param str_volume   Name of a single unit of cargo of this type.
 * @param classes      Classes of this cargo type. @see CargoClass
 */
#define MK(bt, label, colour, weight, mult, ip, td1, td2, freight, tae, str_plural, str_singular, str_volume, classes) \
		{label, bt, colour, colour, weight, mult, classes, ip, {td1, td2}, freight, tae, INVALID_TPE, 0, \
		MK_STR_CARGO_PLURAL(str_plural), MK_STR_CARGO_SINGULAR(str_singular), str_volume, MK_STR_QUANTITY(str_plural), MK_STR_ABBREV(str_plural), \
		MK_SPRITE(str_plural), nullptr, nullptr, 0}

/** Cargo types available by default. */
static const CargoSpec _default_cargo[] = {
	MK(   0, CT_PASSENGERS,   152,  1, 0x400, 3185,  0,  24, false, TAE_PASSENGERS,   PASSENGERS,    PASSENGER, STR_PASSENGERS, CC_PASSENGERS),
	MK(   1, CT_COAL,           6, 16, 0x100, 5916,  7, 255,  true,       TAE_NONE,         COAL,         COAL,       STR_TONS, CC_BULK),
	MK(   2, CT_MAIL,          15,  4, 0x200, 4550, 20,  90, false,       TAE_MAIL,         MAIL,         MAIL,       STR_BAGS, CC_MAIL),
	/* Oil in temperate and arctic */
	MK(   3, CT_OIL,          174, 16, 0x100, 4437, 25, 255,  true,       TAE_NONE,          OIL,          OIL,     STR_LITERS, CC_LIQUID),
	/* Oil in subtropic */
	MK(   3, CT_OIL,          174, 16, 0x100, 4892, 25, 255,  true,       TAE_NONE,          OIL,          OIL,     STR_LITERS, CC_LIQUID),
	MK(   4, CT_LIVESTOCK,    208,  3, 0x100, 4322,  4,  18,  true,       TAE_NONE,    LIVESTOCK,    LIVESTOCK,      STR_ITEMS, CC_PIECE_GOODS),
	MK(   5, CT_GOODS,        194,  8, 0x200, 6144,  5,  28,  true,      TAE_GOODS,        GOODS,        GOODS,     STR_CRATES, CC_EXPRESS),
	MK(   6, CT_GRAIN,        191, 16, 0x100, 4778,  4,  40,  true,       TAE_NONE,        GRAIN,        GRAIN,       STR_TONS, CC_BULK),
	MK(   6, CT_WHEAT,        191, 16, 0x100, 4778,  4,  40,  true,       TAE_NONE,        WHEAT,        WHEAT,       STR_TONS, CC_BULK),
	MK(   6, CT_MAIZE,        191, 16, 0x100, 4322,  4,  40,  true,       TAE_NONE,        MAIZE,        MAIZE,       STR_TONS, CC_BULK),
	/* Wood in temperate and arctic */
	MK(   7, CT_WOOD,          84, 16, 0x100, 5005, 15, 255,  true,       TAE_NONE,         WOOD,         WOOD,       STR_TONS, CC_PIECE_GOODS),
	/* Wood in subtropic */
	MK(   7, CT_WOOD,          84, 16, 0x100, 7964, 15, 255,  true,       TAE_NONE,         WOOD,         WOOD,       STR_TONS, CC_PIECE_GOODS),
	MK(   8, CT_IRON_ORE,     184, 16, 0x100, 5120,  9, 255,  true,       TAE_NONE,     IRON_ORE,     IRON_ORE,       STR_TONS, CC_BULK),
	MK(   9, CT_STEEL,         10, 16, 0x100, 5688,  7, 255,  true,       TAE_NONE,        STEEL,        STEEL,       STR_TONS, CC_PIECE_GOODS),
	MK(  10, CT_VALUABLES,    202,  2, 0x100, 7509,  1,  32,  true,       TAE_NONE,    VALUABLES,    VALUABLES,       STR_BAGS, CC_ARMOURED),
	MK(  10, CT_GOLD,         202,  8, 0x100, 5802, 10,  40,  true,       TAE_NONE,         GOLD,         GOLD,       STR_BAGS, CC_ARMOURED),
	MK(  10, CT_DIAMONDS,     202,  2, 0x100, 5802, 10, 255,  true,       TAE_NONE,     DIAMONDS,      DIAMOND,       STR_BAGS, CC_ARMOURED),
	MK(  11, CT_PAPER,         10, 16, 0x100, 5461,  7,  60,  true,       TAE_NONE,        PAPER,        PAPER,       STR_TONS, CC_PIECE_GOODS),
	MK(  12, CT_FOOD,          48, 16, 0x100, 5688,  0,  30,  true,       TAE_FOOD,         FOOD,         FOOD,       STR_TONS, CC_EXPRESS | CC_REFRIGERATED),
	MK(  13, CT_FRUIT,        208, 16, 0x100, 4209,  0,  15,  true,       TAE_NONE,        FRUIT,        FRUIT,       STR_TONS, CC_BULK | CC_REFRIGERATED),
	MK(  14, CT_COPPER_ORE,   184, 16, 0x100, 4892, 12, 255,  true,       TAE_NONE,   COPPER_ORE,   COPPER_ORE,       STR_TONS, CC_BULK),
	MK(  15, CT_WATER,         10, 16, 0x100, 4664, 20,  80,  true,      TAE_WATER,        WATER,        WATER,     STR_LITERS, CC_LIQUID),
	MK(  16, CT_RUBBER,         6, 16, 0x100, 4437,  2,  20,  true,       TAE_NONE,       RUBBER,       RUBBER,     STR_LITERS, CC_LIQUID),
	MK(  17, CT_SUGAR,          6, 16, 0x100, 4437, 20, 255,  true,       TAE_NONE,        SUGAR,        SUGAR,       STR_TONS, CC_BULK),
	MK(  18, CT_TOYS,         174,  2, 0x100, 5574, 25, 255,  true,       TAE_NONE,         TOYS,          TOY,      STR_ITEMS, CC_PIECE_GOODS),
	MK(  19, CT_BATTERIES,    208,  4, 0x100, 4322,  2,  30,  true,       TAE_NONE,    BATTERIES,      BATTERY,      STR_ITEMS, CC_PIECE_GOODS),
	MK(  20, CT_CANDY,        194,  5, 0x200, 6144,  8,  40,  true,      TAE_GOODS,       SWEETS,       SWEETS,       STR_BAGS, CC_EXPRESS),
	MK(  21, CT_TOFFEE,       191, 16, 0x100, 4778, 14,  60,  true,       TAE_NONE,       TOFFEE,       TOFFEE,       STR_TONS, CC_BULK),
	MK(  22, CT_COLA,          84, 16, 0x100, 4892,  5,  75,  true,       TAE_NONE,         COLA,         COLA,     STR_LITERS, CC_LIQUID),
	MK(  23, CT_COTTON_CANDY, 184, 16, 0x100, 5005, 10,  25,  true,       TAE_NONE,   CANDYFLOSS,   CANDYFLOSS,       STR_TONS, CC_BULK),
	MK(  24, CT_BUBBLES,       10,  1, 0x100, 5077, 20,  80,  true,       TAE_NONE,      BUBBLES,       BUBBLE,      STR_ITEMS, CC_PIECE_GOODS),
	MK(  25, CT_PLASTIC,      202, 16, 0x100, 4664, 30, 255,  true,       TAE_NONE,      PLASTIC,      PLASTIC,     STR_LITERS, CC_LIQUID),
	MK(  26, CT_FIZZY_DRINKS,  48,  2, 0x100, 6250, 30,  50,  true,       TAE_FOOD, FIZZY_DRINKS,  FIZZY_DRINK,      STR_ITEMS, CC_PIECE_GOODS),

	/* Void slot in temperate */
	MK(0xFF, CT_INVALID,        1,  0, 0x100, 5688,  0,  30,  true,       TAE_NONE,      NOTHING,      NOTHING,       STR_TONS, CC_NOAVAILABLE),
	/* Void slot in arctic */
	MK(0xFF, CT_INVALID,      184,  0, 0x100, 5120,  9, 255,  true,       TAE_NONE,      NOTHING,      NOTHING,       STR_TONS, CC_NOAVAILABLE),
};


/** Table of cargo types available in each climate, by default */
static const std::variant<CargoLabel, int> _default_climate_cargo[NUM_LANDSCAPE][NUM_ORIGINAL_CARGO] = {
	{ CT_PASSENGERS, CT_COAL,   CT_MAIL, CT_OIL,  CT_LIVESTOCK, CT_GOODS, CT_GRAIN,  CT_WOOD, CT_IRON_ORE,     CT_STEEL,   CT_VALUABLES, 33, },
	{ CT_PASSENGERS, CT_COAL,   CT_MAIL, CT_OIL,  CT_LIVESTOCK, CT_GOODS, CT_WHEAT,  CT_WOOD, 34,              CT_PAPER,   CT_GOLD,      CT_FOOD, },
	{ CT_PASSENGERS, CT_RUBBER, CT_MAIL, 4,       CT_FRUIT,     CT_GOODS, CT_MAIZE,  11,      CT_COPPER_ORE,   CT_WATER,   CT_DIAMONDS,  CT_FOOD, },
	{ CT_PASSENGERS, CT_SUGAR,  CT_MAIL, CT_TOYS, CT_BATTERIES, CT_CANDY, CT_TOFFEE, CT_COLA, CT_COTTON_CANDY, CT_BUBBLES, CT_PLASTIC,   CT_FIZZY_DRINKS, },
};

