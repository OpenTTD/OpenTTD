/* $Id$ */

/** @file industry.h */

#ifndef INDUSTRY_H
#define INDUSTRY_H

#include "oldpool.h"
#include "helpers.hpp"

typedef byte IndustryGfx;
typedef uint8 IndustryType;

enum {
	INVALID_INDUSTRY       = 0xFFFF,
	NEW_INDUSTRYOFFSET     = 37,                         ///< original number of industries
	NUM_INDUSTRYTYPES      = 37,                         ///< total number of industries, new and old
	INDUSTRYTILE_NOANIM    = 0xFF,                       ///< flag to mark industry tiles as having no animation
	NEW_INDUSTRYTILEOFFSET = 175,                        ///< original number of tiles
	INVALID_INDUSTRYTYPE   = NUM_INDUSTRYTYPES,          ///< one above amount is considered invalid
	NUM_INDUSTRYTILES      = NEW_INDUSTRYTILEOFFSET,     ///< total number of industry tiles, new and old
	INVALID_INDUSTRYTILE   = NUM_INDUSTRYTILES,          ///< one above amount is considered invalid
	INDUSTRY_COMPLETED     = 3,                          ///< final stage of industry construction.
};

enum {
	CLEAN_RANDOMSOUNDS,    ///< Free the dynamically allocated sounds table
	CLEAN_TILELSAYOUT,     ///< Free the dynamically allocated tile layout structure
};

enum IndustryLifeType {
	INDUSTRYLIFE_BLACK_HOLE =      0, ///< Like power plants and banks
	INDUSTRYLIFE_EXTRACTIVE = 1 << 0, ///< Like mines
	INDUSTRYLIFE_ORGANIC    = 1 << 1, ///< Like forests
	INDUSTRYLIFE_PROCESSING = 1 << 2, ///< Like factories
};

/* Procedures that can be run to check whether an industry may
 * build at location the given to the procedure */
enum CheckProc {
	CHECK_NOTHING,
	CHECK_FOREST,
	CHECK_REFINERY,
	CHECK_FARM,
	CHECK_PLANTATION,
	CHECK_WATER,
	CHECK_LUMBERMILL,
	CHECK_BUBBLEGEN,
	CHECK_OIL_RIG,
	CHECK_END,
};

/** How was the industry created */
enum IndustryConstructionType {
	ICT_UNKNOWN,          ///< in previous game version or without newindustries activated
	ICT_NORMAL_GAMEPLAY,  ///< either by user or random creation proccess
	ICT_MAP_GENERATION,   ///< during random map creation
	ICT_SCENARIO_EDITOR   ///< while scenarion edition
};

/** From where is callback CBID_INDUSTRY_AVAILABLE been called */
enum IndustryAvailabilityCallType {
	IACT_MAPGENERATION,   ///< during random map generation
	IACT_RANDOMCREATION,  ///< during creation of random ingame industry
	IACT_USERCREATION,    ///< from the Fund/build window
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
	INDUSTRYBEH_DONT_INCR_PROD        = 1 << 7,  ///< do not increase production (oil wells) in the temperate climate
	INDUSTRYBEH_BEFORE_1950           = 1 << 8,  ///< can only be built before 1950 (oil wells)
	INDUSTRYBEH_AFTER_1960            = 1 << 9,  ///< can only be built after 1960 (oil rigs)
	INDUSTRYBEH_AI_AIRSHIP_ROUTES     = 1 << 10, ///< ai will attempt to establish air/ship routes to this industry (oil rig)
	INDUSTRYBEH_AIRPLANE_ATTACKS      = 1 << 11, ///< can be exploded by a military airplane (oil refinery)
	INDUSTRYBEH_CHOPPER_ATTACKS       = 1 << 12, ///< can be exploded by a military helicopter (factory)
	INDUSTRYBEH_CAN_SUBSIDENCE        = 1 << 13, ///< can cause a subsidence (coal mine, shaft that collapses)
};


DECLARE_ENUM_AS_BIT_SET(IndustyBehaviour);

struct Industry;
DECLARE_OLD_POOL(Industry, Industry, 3, 8000)

/**
 * Defines the internal data of a functionnal industry
 */
struct Industry : PoolItem<Industry, IndustryID, &_Industry_pool> {
	TileIndex xy;                       ///< coordinates of the primary tile the industry is built one
	byte width;
	byte height;
	const Town *town;                   ///< Nearest town
	uint16 produced_cargo_waiting[2];   ///< amount of cargo produced per cargo
	uint16 incoming_cargo_waiting[3];   ///< incoming cargo waiting to be processed
	byte production_rate[2];            ///< production rate for each cargo
	byte prod_level;                    ///< general production level
	uint16 this_month_production[2];    ///< stats of this month's production per cargo
	uint16 this_month_transported[2];   ///< stats of this month's transport per cargo
	byte last_month_pct_transported[2]; ///< percentage transported per cargo in the last full month
	uint16 last_month_production[2];    ///< total units produced per cargo in the last full month
	uint16 last_month_transported[2];   ///< total units transported per cargo in the last full month
	uint16 counter;                     ///< used for animation and/or production (if available cargo)

	IndustryType type;                  ///< type of industry.
	OwnerByte owner;                    ///< owner of the industry.  Which SHOULD always be (imho) OWNER_NONE
	byte random_color;                  ///< randomized colour of the industry, for display purpose
	Year last_prod_year;                ///< last year of production
	byte was_cargo_delivered;           ///< flag that indicate this has been the closest industry chosen for cargo delivery by a station. see DeliverGoodsToIndustry

	OwnerByte founder;                  ///< Founder of the industry
	Date construction_date;             ///< Date of the construction of the industry
	uint8 construction_type;            ///< Way the industry was constructed (@see IndustryConstructionType)
	Date last_cargo_accepted_at;        ///< Last day cargo was accepted by this industry

	Industry(TileIndex tile = 0) : xy(tile) {}
	~Industry();

	bool IsValid() const { return this->xy != 0; }
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
	const struct GRFFile *grffile;        ///< grf file that introduced this industry
	uint16 override;                      ///< id of the entity been replaced by
};

/**
 * Defines the data structure for constructing industry.
 */
struct IndustrySpec {
	const IndustryTileTable *const *table;///< List of the tiles composing the industry
	byte num_table;                       ///< Number of elements in the table
	uint8 cost_multiplier;                ///< Base cost multiplier.
	uint16 raw_industry_cost_multiplier;  ///< Multiplier for the raw industries cost
	uint32 prospecting_chance;            ///< Chance prospecting succeeds
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
	uint8 cleanup_flag;                   ///< flags indicating which data should be freed upon cleaning up
	bool enabled;                         ///< entity still avaible (by default true).newgrf can disable it, though
	struct GRFFileProps grf_prop;         ///< properties related the the grf file

	/**
	 * Is an industry with the spec a raw industry?
	 * @return true if it should be handled as a raw industry
	 */
	bool IsRawIndustry() const;

	/**
	 * Get the cost for constructing this industry
	 * @return the cost (inflation corrected etc)
	 */
	Money GetConstructionCost() const;
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
	uint8 callback_flags;                 ///< Flags telling which grf callback is set
	uint16 animation_info;                ///< Information about the animation (is it looping, how many loops etc)
	uint8 animation_speed;                ///< The speed of the animation
	uint8 animation_triggers;             ///< When to start the animation
	uint8 animation_special_flags;        ///< Extra flags to influence the animation
	bool enabled;                         ///< entity still avaible (by default true).newgrf can disable it, though
	struct GRFFileProps grf_prop;
};

/* industry_cmd.cpp*/
const IndustrySpec *GetIndustrySpec(IndustryType thistype);    ///< Array of industries data
const IndustryTileSpec *GetIndustryTileSpec(IndustryGfx gfx, bool full_check = true);  ///< Array of industry tiles data
void ResetIndustries();
void PlantRandomFarmField(const Industry *i);

/* writable arrays of specs */
extern IndustrySpec _industry_specs[NUM_INDUSTRYTYPES];
extern IndustryTileSpec _industry_tile_specs[NUM_INDUSTRYTILES];

/* smallmap_gui.cpp */
void BuildIndustriesLegend();

/**
 * Check if an Industry exists whithin the pool of industries
 * @param index of the desired industry
 * @return true if it is inside the pool
 */
static inline bool IsValidIndustryID(IndustryID index)
{
	return index < GetIndustryPoolSize() && GetIndustry(index)->IsValid();
}


static inline IndustryID GetMaxIndustryIndex()
{
	/* TODO - This isn't the real content of the function, but
	 *  with the new pool-system this will be replaced with one that
	 *  _really_ returns the highest index. Now it just returns
	 *  the next safe value we are sure about everything is below.
	 */
	return GetIndustryPoolSize() - 1;
}

extern int _total_industries;  // general counter
extern uint16 _industry_counts[NUM_INDUSTRYTYPES]; // Number of industries per type ingame

static inline uint GetNumIndustries()
{
	return _total_industries;
}

/** Increment the count of industries for this type
 * @param type IndustryType to increment
 * @pre type < INVALID_INDUSTRYTYPE */
static inline void IncIndustryTypeCount(IndustryType type)
{
	assert(type < INVALID_INDUSTRYTYPE);
	_industry_counts[type]++;
	_total_industries++;
}

/** Decrement the count of industries for this type
 * @param type IndustryType to decrement
 * @pre type < INVALID_INDUSTRYTYPE */
static inline void DecIndustryTypeCount(IndustryType type)
{
	assert(type < INVALID_INDUSTRYTYPE);
	_industry_counts[type]--;
	_total_industries--;
}

/** get the count of industries for this type
 * @param type IndustryType to query
 * @pre type < INVALID_INDUSTRYTYPE */
static inline uint8 GetIndustryTypeCount(IndustryType type)
{
	assert(type < INVALID_INDUSTRYTYPE);
	return min(_industry_counts[type], 0xFF); // callback expects only a byte, so cut it
}

/** Resets both the total_industries and the _industry_counts
 * This way, we centralize all counts activities */
static inline void ResetIndustryCounts()
{
	_total_industries = 0;
	memset(&_industry_counts, 0, sizeof(_industry_counts));
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

#define FOR_ALL_INDUSTRIES_FROM(i, start) for (i = GetIndustry(start); i != NULL; i = (i->index + 1U < GetIndustryPoolSize()) ? GetIndustry(i->index + 1U) : NULL) if (i->IsValid())
#define FOR_ALL_INDUSTRIES(i) FOR_ALL_INDUSTRIES_FROM(i, 0)

extern const Industry **_industry_sort;
extern bool _industry_sort_dirty;

static const uint8 IT_INVALID = 255;

#endif /* INDUSTRY_H */
