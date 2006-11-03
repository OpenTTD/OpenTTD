/* $Id$ */

#include "sprites.h"

typedef struct CargoTypesValues {
	StringID names[NUM_CARGO];
	StringID units_volume[NUM_CARGO];
	byte weights[NUM_CARGO];
	SpriteID sprites[NUM_CARGO];

	uint16 initial_cargo_payment[NUM_CARGO];
	byte transit_days_table_1[NUM_CARGO];
	byte transit_days_table_2[NUM_CARGO];
} CargoTypesValues;


static const CargoTypesValues _cargo_types_base_values[4] = {
	{
		/* normal names */
		{
			STR_000F_PASSENGERS,
			STR_0010_COAL,
			STR_0011_MAIL,
			STR_0012_OIL,
			STR_0013_LIVESTOCK,
			STR_0014_GOODS,
			STR_0015_GRAIN,
			STR_0016_WOOD,
			STR_0017_IRON_ORE,
			STR_0018_STEEL,
			STR_0019_VALUABLES,
			STR_000E,
		},

		{ /* normal units of volume */
			STR_PASSENGERS,
			STR_TONS,
			STR_BAGS,
			STR_LITERS,
			STR_ITEMS,
			STR_CRATES,
			STR_TONS,
			STR_TONS,
			STR_TONS,
			STR_TONS,
			STR_BAGS,
			STR_RES_OTHER
		},

		/* normal weights */
		{
			1, 16, 4, 16, 3, 8, 16, 16, 16, 16, 2, 0,
		},

		/* normal sprites */
		{
			SPR_CARGO_PASSENGER,      SPR_CARGO_COAL,  SPR_CARGO_MAIL, SPR_CARGO_OIL,      SPR_CARGO_LIVESTOCK,
			SPR_CARGO_GOODS,          SPR_CARGO_GRAIN, SPR_CARGO_WOOD, SPR_CARGO_IRON_ORE, SPR_CARGO_STEEL,
			SPR_CARGO_VALUES_GOLD,    SPR_ASCII_SPACE
		},

		/* normal initial cargo payment */
		{
			3185, 5916, 4550, 4437, 4322, 6144, 4778, 5005, 5120, 5688, 7509, 5688
		},

		/* normal transit days table 1 */
		{
			0, 7, 20, 25, 4, 5, 4, 15, 9, 7, 1, 0,
		},

		/* normal transit days table 2 */
		{
			24, 255, 90, 255, 18, 28, 40, 255, 255, 255, 32, 30,
		},
	},

	{
		/* hilly names */
		{
			STR_000F_PASSENGERS,
			STR_0010_COAL,
			STR_0011_MAIL,
			STR_0012_OIL,
			STR_0013_LIVESTOCK,
			STR_0014_GOODS,
			STR_0022_WHEAT,
			STR_0016_WOOD,
			STR_000E,
			STR_001F_PAPER,
			STR_0020_GOLD,
			STR_001E_FOOD,
		},

		{ /* hilly units of volume */
			STR_PASSENGERS,
			STR_TONS,
			STR_BAGS,
			STR_LITERS,
			STR_ITEMS,
			STR_CRATES,
			STR_TONS,
			STR_TONS,
			STR_RES_OTHER,
			STR_TONS,
			STR_BAGS,
			STR_TONS
		},

		/* hilly weights */
		{
			1, 16, 4, 16, 3, 8, 16, 16, 0, 16, 8, 16
		},

		/* hilly sprites */
		{
			SPR_CARGO_PASSENGER,   SPR_CARGO_COAL,  SPR_CARGO_MAIL, SPR_CARGO_OIL,   SPR_CARGO_LIVESTOCK,
			SPR_CARGO_GOODS,       SPR_CARGO_GRAIN, SPR_CARGO_WOOD, SPR_ASCII_SPACE, SPR_CARGO_PAPER,
			SPR_CARGO_VALUES_GOLD, SPR_CARGO_FOOD
		},

		/* hilly initial cargo payment */
		{
			3185, 5916, 4550, 4437, 4322, 6144, 4778, 5005, 5120, 5461, 5802, 5688
		},

		/* hilly transit days table 1 */
		{
			0, 7, 20, 25, 4, 5, 4, 15, 9, 7, 10, 0,
		},

		/* hilly transit days table 2 */
		{
			24, 255, 90, 255, 18, 28, 40, 255, 255, 60, 40, 30
		},
	},

	{
		/* desert names */
		{
			STR_000F_PASSENGERS,
			STR_0023_RUBBER,
			STR_0011_MAIL,
			STR_0012_OIL,
			STR_001C_FRUIT,
			STR_0014_GOODS,
			STR_001B_MAIZE,
			STR_0016_WOOD,
			STR_001A_COPPER_ORE,
			STR_0021_WATER,
			STR_001D_DIAMONDS,
			STR_001E_FOOD
		},

		{ /* desert units of volume */
			STR_PASSENGERS,
			STR_LITERS,
			STR_BAGS,
			STR_LITERS,
			STR_TONS,
			STR_CRATES,
			STR_TONS,
			STR_TONS,
			STR_TONS,
			STR_LITERS,
			STR_BAGS,
			STR_TONS
		},

		/* desert weights */
		{
			1, 16, 4, 16, 16, 8, 16, 16, 16, 16, 2, 16,
		},

		/* desert sprites */
		{
			SPR_CARGO_PASSENGER, SPR_CARGO_RUBBER, SPR_CARGO_MAIL, SPR_CARGO_OIL,        SPR_CARGO_FRUIT,
			SPR_CARGO_GOODS,     SPR_CARGO_GRAIN,  SPR_CARGO_WOOD, SPR_CARGO_COPPER_ORE, SPR_CARGO_WATERCOLA,
			SPR_CARGO_DIAMONDS,  SPR_CARGO_FOOD
		},

		/* desert initial cargo payment */
		{
			3185, 4437, 4550, 4892, 4209, 6144, 4322, 7964, 4892, 4664, 5802, 5688
		},

		/* desert transit days table 1 */
		{
			0, 2, 20, 25, 0, 5, 4, 15, 12, 20, 10, 0
		},

		/* desert transit days table 2 */
		{
			24, 20, 90, 255, 15, 28, 40, 255, 255, 80, 255, 30
		},
	},

	{
		/* candy names */
		{
			STR_000F_PASSENGERS,
			STR_0024_SUGAR,
			STR_0011_MAIL,
			STR_0025_TOYS,
			STR_002B_BATTERIES,
			STR_0026_CANDY,
			STR_002A_TOFFEE,
			STR_0027_COLA,
			STR_0028_COTTON_CANDY,
			STR_0029_BUBBLES,
			STR_002C_PLASTIC,
			STR_002D_FIZZY_DRINKS,
		},

		{ /* candy unitrs of volume */
			STR_PASSENGERS,
			STR_TONS,
			STR_BAGS,
			STR_NOTHING,
			STR_NOTHING,
			STR_TONS,
			STR_TONS,
			STR_LITERS,
			STR_TONS,
			STR_NOTHING,
			STR_LITERS,
			STR_NOTHING
		},

		/* candy weights */
		{
			1, 16, 4, 2, 4, 5, 16, 16, 16, 1, 16, 2
		},

		/* candy sprites */
		{
			SPR_CARGO_PASSENGER, SPR_CARGO_SUGAR,  SPR_CARGO_MAIL,      SPR_CARGO_TOYS,        SPR_CARGO_BATTERIES,
			SPR_CARGO_CANDY,     SPR_CARGO_TOFFEE, SPR_CARGO_WATERCOLA, SPR_CARGO_COTTONCANDY, SPR_CARGO_BUBBLES,
			SPR_CARGO_PLASTIC,   SPR_CARGO_FIZZYDRINK
		},

		/* candy initial cargo payment */
		{
			3185, 4437, 4550, 5574, 4322, 6144, 4778, 4892, 5005, 5077, 4664, 6250
		},

		/* candy transit days table 1 */
		{
			0, 20, 20, 25, 2, 8, 14, 5, 10, 20, 30, 30,
		},

		/* candy transit days table 2 */
		{
			24, 255, 90, 255, 30, 40, 60, 75, 25, 80, 255, 50
		},
	}
};
