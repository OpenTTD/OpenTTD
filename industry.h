#ifndef INDUSTRY_H
#define INDUSTRY_H

struct Industry {
	TileIndex xy;
	byte width; /* swapped order of w/h with town */
	byte height;
	Town *town;
	byte produced_cargo[2];
	uint16 cargo_waiting[2];
	byte production_rate[2];
	byte accepts_cargo[3];
	byte prod_level;
	uint16 last_mo_production[2];
	uint16 last_mo_transported[2];
	byte pct_transported[2];
	uint16 total_production[2];
	uint16 total_transported[2];
	uint16 counter;

	byte type;
	byte owner;
	byte color_map;
	byte last_prod_year;
	byte was_cargo_delivered;

	uint16 index;
};

VARDEF int _total_industries; // For the AI: the amount of industries active

VARDEF Industry _industries[90];
VARDEF uint _industries_size;

VARDEF uint16 *_industry_sort;

static inline Industry *GetIndustry(uint index)
{
	assert(index < _industries_size);
	return &_industries[index];
}

#define FOR_ALL_INDUSTRIES(i) for(i = _industries; i != &_industries[_industries_size]; i++)

VARDEF bool _industry_sort_dirty;
void DeleteIndustry(Industry *is);

enum {
	IT_COAL_MINE = 0,
	IT_POWER_STATION = 1,
	IT_SAWMILL = 2,
	IT_FOREST = 3,
	IT_OIL_REFINERY = 4,
	IT_OIL_RIG = 5,
	IT_FACTORY = 6,
	IT_PRINTING_WORKS = 7,
	IT_STEEL_MILL = 8,
	IT_FARM = 9,
	IT_COPPER_MINE = 10,
	IT_OIL_WELL = 11,
	IT_BANK = 12,
	IT_FOOD_PROCESS = 13,
	IT_PAPER_MILL = 14,
	IT_GOLD_MINE = 15,
	IT_BANK_2 = 16,
	IT_DIAMOND_MINE = 17,
	IT_IRON_MINE = 18,
	IT_FRUIT_PLANTATION = 19,
	IT_RUBBER_PLANTATION = 20,
	IT_WATER_SUPPLY = 21,
	IT_WATER_TOWER = 22,
	IT_FACTORY_2 = 23,
	IT_FARM_2 = 24,
	IT_LUMBER_MILL = 25,
	IT_COTTON_CANDY = 26,
	IT_CANDY_FACTORY = 27,
	IT_BATTERY_FARM = 28,
	IT_COLA_WELLS = 29,
	IT_TOY_SHOP = 30,
	IT_TOY_FACTORY = 31,
	IT_PLASTIC_FOUNTAINS = 32,
	IT_FIZZY_DRINK_FACTORY = 33,
	IT_BUBBLE_GENERATOR = 34,
	IT_TOFFEE_QUARRY = 35,
	IT_SUGAR_MINE = 36,
};

#endif
