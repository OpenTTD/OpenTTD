/* $Id$ */

/** @file industry.h */

#ifndef INDUSTRY_H
#define INDUSTRY_H

#include "oldpool.h"
#include "helpers.hpp"

typedef byte IndustryGfx;
typedef uint8 IndustryType;

enum {
	INVALID_INDUSTRY = 0xFFFF,
	NUM_INDUSTRYTYPES = 37,
	INDUTILE_NOANIM = 0xFF,                   ///< flag to mark industry tiles as having no animation
	INVALID_INDUSTRYTYPE = NUM_INDUSTRYTYPES, ///< one above amount is considered invalid
};

enum IndustryLifeType {
	INDUSTRYLIFE_NOT_CLOSABLE,     ///< Industry can never close
	INDUSTRYLIFE_PRODUCTION,       ///< Industry can close and change of production
	INDUSTRYLIFE_CLOSABLE,         ///< Industry can only close (no production change)
};

/* Procedures that can be run to check whether an industry may
 * build at location the given to the procedure */
enum CheckProc {
	CHECK_NOTHING    = 0,
	CHECK_FOREST     = 1,
	CHECK_REFINERY   = 2,
	CHECK_FARM       = 3,
	CHECK_PLANTATION = 4,
	CHECK_WATER      = 5,
	CHECK_LUMBERMILL = 6,
	CHECK_BUBBLEGEN  = 7,
	CHECK_OIL_RIG    = 8,
	CHECK_END,
};

enum IndustyBehaviour {
	INDUSTRYBEH_NONE                  =      0,
	INDUSTRYBEH_PLANT_FIELDS          = 1 << 0,  ///< periodically plants fileds around itself (temp and artic farms)
	INDUSTRYBEH_CUT_TREES             = 1 << 1,  ///< cuts trees and produce first output cargo from them (lumber mill)
	INDUSTRYBEH_BUILT_ONWATER         = 1 << 2,  ///< is built on water (oil rig)
	INDUSTRYBEH_TOWN1200_MORE         = 1 << 3,  ///< can only be built in towns larger then 1200 inhabitants (temperate bank)
	INDUSTRYBEH_ONLY_INTOWN           = 1 << 4,  ///< can only be built in towns (arctic/tropic banks, water tower)
	INDUSTRYBEH_ONLY_NEARTOWN         = 1 << 5,  ///< is always built near towns (toy shop)
	INDUSTRYBEH_PLANT_ON_BUILT        = 1 << 6,  ///< Fields are planted around when built (all farms)
	INDUSTRYBEH_DONT_INCR_PROD        = 1 << 7,  ///< do not increase production (oil wells)
	INDUSTRYBEH_BEFORE_1950           = 1 << 8,  ///< can only be built before 1950 (oil wells)
	INDUSTRYBEH_AFTER_1960            = 1 << 9,  ///< can only be built after 1960 (oil rigs)
	INDUSTRYBEH_AI_AIRSHIP_ROUTES     = 1 << 10, ///< ai will attempt to establish air/ship routes to this industry (oil rig)
	INDUSTRYBEH_AIRPLANE_ATTACKS      = 1 << 11, ///< can be exploded by a military airplane (oil refinery)
	INDUSTRYBEH_CHOPPER_ATTACKS       = 1 << 12, ///< can be exploded by a military helicopter (factory)
	INDUSTRYBEH_CAN_SUBSIDENCE        = 1 << 13, ///< can cause a subsidence (coal mine, shaft that collapses)
};


DECLARE_ENUM_AS_BIT_SET(IndustyBehaviour);

/**
 * Defines the internal data of a functionnal industry
 */
struct Industry {
	TileIndex xy;                   ///< coordinates of the primary tile the industry is built one
	byte width;
	byte height;
	const Town* town;               ///< Nearest town
	uint16 cargo_waiting[2];        ///< amount of cargo produced per cargo
	byte production_rate[2];        ///< production rate for each cargo
	byte prod_level;                ///< general production level
	uint16 last_mo_production[2];   ///< stats of last month production per cargo
	uint16 last_mo_transported[2];  ///< stats of last month transport per cargo
	byte pct_transported[2];        ///< percentage transported per cargo
	uint16 total_production[2];     ///< total units produced per cargo
	uint16 total_transported[2];    ///< total units transported per cargo
	uint16 counter;                 ///< used for animation and/or production (if available cargo)

	IndustryType type;              ///< type of industry. see IT_COAL_MINE and others
	OwnerByte owner;                ///< owner of the industry.  Which SHOULD always be (imho) OWNER_NONE
	byte random_color;              ///< randomized colour of the industry, for display purpose
	Year last_prod_year;            ///< last year of production
	byte was_cargo_delivered;       ///< flag that indicate this has been the closest industry chosen for cargo delivery by a station. see DeliverGoodsToIndustry

	IndustryID index;               ///< index of the industry in the pool of industries
};

struct IndustryTileTable {
	TileIndexDiffC ti;
	IndustryGfx gfx;
};

/** Data related to the handling of grf files.  Common to both industry and industry tile */
struct GRFFileProps {
	uint8 subst_id;
	uint16 local_id;                      ///< id defined by the grf file for this industry
	struct SpriteGroup *spritegroup;      ///< pointer to the different sprites of the industry
	const struct GRFFile *grffile;        ///< grf file that introduced this house
	uint8 override;                       ///< id of the entity been replaced by
	bool enabled;                         ///< entity still avaible (by default true).newgrf can disable it, though
};

/**
 * Defines the data structure for constructing industry.
 */
struct IndustrySpec {
	const IndustryTileTable *const *table;///< List of the tiles composing the industry
	byte num_table;                       ///< Number of elements in the table
	byte cost_multiplier;                 ///< Base cost multiplier. Watch out for this one, << 5  VS << 8
	IndustryType conflicting[3];          ///< Industries this industry cannot be close to
	byte check_proc;                      ///< Index to a procedure to check for conflicting circumstances
	CargoID produced_cargo[2];
	byte production_rate[2];
	byte minimal_cargo;                   ///< minimum amount of cargo transported to the stations
	                                      ///< If the waiting cargo is less than this number, no cargo is moved to it
	CargoID accepts_cargo[3];             ///< 3 accepted cargos
	uint16 input_cargo_multiplier[3][2];  ///< Input cargo multipliers (multiply amount of incoming cargo for the produced cargos)
	IndustryLifeType life_type;           ///< This is also known as Industry production flag, in newgrf specs
	byte climate_availability;            ///< Bitmask, giving landscape enums as bit position
	IndustyBehaviour behaviour;           ///< How this industry will behave, and how others entities can use it
	byte map_colour;                      ///< colour used for the small map
	StringID name;                        ///< Displayed name of the industry
	StringID new_industry_text;           ///< Message appearing when the industry is built
	StringID closure_text;                ///< Message appearing when the industry closes
	StringID production_up_text;          ///< Message appearing when the industry's production is increasing
	StringID production_down_text;        ///< Message appearing when the industry's production is decreasing
	byte appear_ingame[NUM_LANDSCAPE];    ///< Probability of appearance in game
	byte appear_creation[NUM_LANDSCAPE];  ///< Probability of appearance during map creation
	uint8 number_of_sounds;               ///< Number of sounds available in the sounds array
	const uint8 *random_sounds;           ///< array of random sounds.
	/* Newgrf data */
	uint16 callback_flags;                ///< Flags telling which grf callback is set
	struct GRFFileProps grf_prop;         ///< properties related the the grf file
};

/**
 * Defines the data structure of each indivudual tile of an industry.
 */
struct IndustryTileSpec {
	CargoID accepts_cargo[3];             ///< Cargo accepted by this tile
	uint8 acceptance[3];                  ///< Level of aceptance per cargo type
	Slope slopes_refused;                 ///< slope pattern on which this tile cannot be built
	byte anim_production;                 ///< Animation frame to start when goods are produced
	byte anim_next;                       ///< Next frame in an animation
	bool anim_state;                      ///< When true, the tile has to be drawn using the animation
	                                      ///< state instead of the construction state
	/* Newgrf data */
	uint8 callback_flags;                ///< Flags telling which grf callback is set
	struct GRFFileProps grf_prop;
};

/* industry_cmd.cpp*/
const IndustrySpec *GetIndustrySpec(IndustryType thistype);    ///< Array of industries default data
const IndustryTileSpec *GetIndustryTileSpec(IndustryGfx gfx);  ///< Array of industry tiles default data
void ResetIndustries();
void PlantRandomFarmField(const Industry *i);

/* smallmap_gui.cpp */
void BuildIndustriesLegend();

DECLARE_OLD_POOL(Industry, Industry, 3, 8000)

/**
 * Check if an Industry really exists.
 * @param industry to check
 * @return true if position is a valid one
 */
static inline bool IsValidIndustry(const Industry *industry)
{
	return industry->xy != 0;
}

/**
 * Check if an Industry exists whithin the pool of industries
 * @param index of the desired industry
 * @return true if it is inside the pool
 */
static inline bool IsValidIndustryID(IndustryID index)
{
	return index < GetIndustryPoolSize() && IsValidIndustry(GetIndustry(index));
}

VARDEF int _total_industries; //general counter

static inline IndustryID GetMaxIndustryIndex()
{
	/* TODO - This isn't the real content of the function, but
	 *  with the new pool-system this will be replaced with one that
	 *  _really_ returns the highest index. Now it just returns
	 *  the next safe value we are sure about everything is below.
	 */
	return GetIndustryPoolSize() - 1;
}

static inline uint GetNumIndustries()
{
	return _total_industries;
}

/**
 * Return a random valid industry.
 */
static inline Industry *GetRandomIndustry()
{
	int num = RandomRange(GetNumIndustries());
	IndustryID index = INVALID_INDUSTRY;

	if (GetNumIndustries() == 0) return NULL;

	while (num >= 0) {
		num--;
		index++;

		/* Make sure we have a valid industry */
		while (!IsValidIndustryID(index)) {
			index++;
			assert(index <= GetMaxIndustryIndex());
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
