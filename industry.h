/* $Id$ */

#ifndef INDUSTRY_H
#define INDUSTRY_H

#include "oldpool.h"

typedef byte IndustryGfx;
typedef uint8 IndustryType;

enum {
	INVALID_INDUSTRY = 0xFFFF,
};

typedef enum IndustryLifeTypes {
	INDUSTRYLIFE_NOT_CLOSABLE,     ///< Industry can never close
	INDUSTRYLIFE_PRODUCTION,       ///< Industry can close and change of production
	INDUSTRYLIFE_CLOSABLE,         ///< Industry can only close (no production change)
} IndustryLifeType;

struct Industry {
	TileIndex xy;
	byte width; /* swapped order of w/h with town */
	byte height;
	const Town* town;
	CargoID produced_cargo[2];
	uint16 cargo_waiting[2];
	byte production_rate[2];
	CargoID accepts_cargo[3];
	byte prod_level;
	uint16 last_mo_production[2];
	uint16 last_mo_transported[2];
	byte pct_transported[2];
	uint16 total_production[2];
	uint16 total_transported[2];
	uint16 counter;

	byte type;
	byte owner;
	byte random_color;
	Year last_prod_year;
	byte was_cargo_delivered;

	IndustryID index;
};

typedef struct IndustryTileTable {
	TileIndexDiffC ti;
	IndustryGfx gfx;
} IndustryTileTable;

typedef struct IndustrySpec {
	/** Tables with the 'layout' of different composition of GFXes */
	const IndustryTileTable *const *table;
	/** Number of elements in the table */
	byte num_table;
	/** Base cost multiplier*/
	byte cost_multiplier;
	/** Industries this industry cannot be close to */
	IndustryType conflicting[3];
	/** index to a procedure to check for conflicting circumstances */
	byte check_proc;

	CargoID produced_cargo[2];
	byte production_rate[2];
	/** The minimum amount of cargo transported to the stations; if the
	 * waiting cargo is less than this number, no cargo is moved to it*/
	byte minimal_cargo;
	CargoID accepts_cargo[3];

	IndustryLifeType life_type;  ///< This is also known as Industry production flag, in newgrf specs

	byte climate_availability;  ///< Bitmask, giving landscape enums as bit position

	StringID name;
	StringID closure_text;
	StringID production_up_text;
	StringID production_down_text;
} IndustrySpec;

const IndustrySpec *GetIndustrySpec(IndustryType thistype);

DECLARE_OLD_POOL(Industry, Industry, 3, 8000)

/**
 * Check if an Industry really exists.
 */
static inline bool IsValidIndustry(const Industry *industry)
{
	return industry->xy != 0;
}

VARDEF int _total_industries;

static inline IndustryID GetMaxIndustryIndex(void)
{
	/* TODO - This isn't the real content of the function, but
	 *  with the new pool-system this will be replaced with one that
	 *  _really_ returns the highest index. Now it just returns
	 *  the next safe value we are sure about everything is below.
	 */
	return GetIndustryPoolSize() - 1;
}

static inline uint GetNumIndustries(void)
{
	return _total_industries;
}

/**
 * Return a random valid town.
 */
static inline Industry *GetRandomIndustry(void)
{
	uint num = RandomRange(GetNumIndustries());
	uint index = 0;

	if (GetNumIndustries() == 0) return NULL;

	while (num > 0) {
		num--;

		index++;
		/* Make sure we have a valid industry */
		while (GetIndustry(index) == NULL) {
			index++;
			if (index > GetMaxIndustryIndex()) index = 0;
		}
	}

	return GetIndustry(index);
}

void DestroyIndustry(Industry *i);

static inline void DeleteIndustry(Industry *i)
{
	DestroyIndustry(i);
	i->xy = 0;
}

#define FOR_ALL_INDUSTRIES_FROM(i, start) for (i = GetIndustry(start); i != NULL; i = (i->index + 1U < GetIndustryPoolSize()) ? GetIndustry(i->index + 1U) : NULL) if (IsValidIndustry(i))
#define FOR_ALL_INDUSTRIES(i) FOR_ALL_INDUSTRIES_FROM(i, 0)

VARDEF const Industry** _industry_sort;
VARDEF bool _industry_sort_dirty;


void DeleteIndustry(Industry *is);
void PlantRandomFarmField(const Industry *i);

enum {
	IT_COAL_MINE           =   0,
	IT_POWER_STATION       =   1,
	IT_SAWMILL             =   2,
	IT_FOREST              =   3,
	IT_OIL_REFINERY        =   4,
	IT_OIL_RIG             =   5,
	IT_FACTORY             =   6,
	IT_PRINTING_WORKS      =   7,
	IT_STEEL_MILL          =   8,
	IT_FARM                =   9,
	IT_COPPER_MINE         =  10,
	IT_OIL_WELL            =  11,
	IT_BANK_TEMP           =  12,
	IT_FOOD_PROCESS        =  13,
	IT_PAPER_MILL          =  14,
	IT_GOLD_MINE           =  15,
	IT_BANK_TROPIC_ARCTIC  =  16,
	IT_DIAMOND_MINE        =  17,
	IT_IRON_MINE           =  18,
	IT_FRUIT_PLANTATION    =  19,
	IT_RUBBER_PLANTATION   =  20,
	IT_WATER_SUPPLY        =  21,
	IT_WATER_TOWER         =  22,
	IT_FACTORY_2           =  23,
	IT_FARM_2              =  24,
	IT_LUMBER_MILL         =  25,
	IT_COTTON_CANDY        =  26,
	IT_CANDY_FACTORY       =  27,
	IT_BATTERY_FARM        =  28,
	IT_COLA_WELLS          =  29,
	IT_TOY_SHOP            =  30,
	IT_TOY_FACTORY         =  31,
	IT_PLASTIC_FOUNTAINS   =  32,
	IT_FIZZY_DRINK_FACTORY =  33,
	IT_BUBBLE_GENERATOR    =  34,
	IT_TOFFEE_QUARRY       =  35,
	IT_SUGAR_MINE          =  36,
	IT_END,
	IT_INVALID             = 255,
};

#endif /* INDUSTRY_H */
