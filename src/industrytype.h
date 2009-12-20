/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file industrytype.h Industry type specs. */

#ifndef INDUSTRYTYPE_H
#define INDUSTRYTYPE_H

#include "core/enum_type.hpp"
#include "economy_type.h"
#include "map_type.h"
#include "slope_type.h"
#include "industry_type.h"
#include "landscape_type.h"
#include "tile_type.h"

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

enum IndustryBehaviour {
	INDUSTRYBEH_NONE                  =      0,
	INDUSTRYBEH_PLANT_FIELDS          = 1 << 0,  ///< periodically plants fileds around itself (temp and artic farms)
	INDUSTRYBEH_CUT_TREES             = 1 << 1,  ///< cuts trees and produce first output cargo from them (lumber mill)
	INDUSTRYBEH_BUILT_ONWATER         = 1 << 2,  ///< is built on water (oil rig)
	INDUSTRYBEH_TOWN1200_MORE         = 1 << 3,  ///< can only be built in towns larger than 1200 inhabitants (temperate bank)
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
	/* The following flags are only used for newindustries and do no represent any normal behaviour */
	INDUSTRYBEH_PROD_MULTI_HNDLING    = 1 << 14, ///< Automatic production multiplier handling
	INDUSTRYBEH_PRODCALLBACK_RANDOM   = 1 << 15, ///< Production callback needs random bits in var 10
	INDUSTRYBEH_NOBUILT_MAPCREATION   = 1 << 16, ///< Do not force one instance of this type to appear on map generation
	INDUSTRYBEH_CANCLOSE_LASTINSTANCE = 1 << 17, ///< Allow closing down the last instance of this type
};
DECLARE_ENUM_AS_BIT_SET(IndustryBehaviour);

struct IndustryTileTable {
	TileIndexDiffC ti;
	IndustryGfx gfx;
};

/** Data related to the handling of grf files.  Common to both industry and industry tile */
struct GRFFileProps {
	uint16 subst_id;
	uint16 local_id;                      ///< id defined by the grf file for this industry
	struct SpriteGroup *spritegroup;      ///< pointer to the different sprites of the industry
	const struct GRFFile *grffile;        ///< grf file that introduced this industry
	uint16 override;                      ///< id of the entity been replaced by
};

/**
 * Defines the data structure for constructing industry.
 */
struct IndustrySpec {
	const IndustryTileTable * const *table;///< List of the tiles composing the industry
	byte num_table;                       ///< Number of elements in the table
	uint8 cost_multiplier;                ///< Base construction cost multiplier.
	uint32 removal_cost_multiplier;       ///< Base removal cost multiplier.
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
	IndustryBehaviour behaviour;           ///< How this industry will behave, and how others entities can use it
	byte map_colour;                      ///< colour used for the small map
	StringID name;                        ///< Displayed name of the industry
	StringID new_industry_text;           ///< Message appearing when the industry is built
	StringID closure_text;                ///< Message appearing when the industry closes
	StringID production_up_text;          ///< Message appearing when the industry's production is increasing
	StringID production_down_text;        ///< Message appearing when the industry's production is decreasing
	StringID station_name;                ///< Default name for nearby station
	byte appear_ingame[NUM_LANDSCAPE];    ///< Probability of appearance in game
	byte appear_creation[NUM_LANDSCAPE];  ///< Probability of appearance during map creation
	uint8 number_of_sounds;               ///< Number of sounds available in the sounds array
	const uint8 *random_sounds;           ///< array of random sounds.
	/* Newgrf data */
	uint16 callback_mask;                 ///< Bitmask of industry callbacks that have to be called
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

	/**
	 * Get the cost for removing this industry
	 * Take note that the cost will always be zero for non-grf industries.
	 * Only if the grf author did specified a cost will it be applicable.
	 * @return the cost (inflation corrected etc)
	 */
	Money GetRemovalCost() const;
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
	uint8 callback_mask;                  ///< Bitmask of industry tile callbacks that have to be called
	uint16 animation_info;                ///< Information about the animation (is it looping, how many loops etc)
	uint8 animation_speed;                ///< The speed of the animation
	uint8 animation_triggers;             ///< When to start the animation
	uint8 animation_special_flags;        ///< Extra flags to influence the animation
	bool enabled;                         ///< entity still avaible (by default true).newgrf can disable it, though
	struct GRFFileProps grf_prop;
};

/* industry_cmd.cpp*/
const IndustrySpec *GetIndustrySpec(IndustryType thistype);    ///< Array of industries data
const IndustryTileSpec *GetIndustryTileSpec(IndustryGfx gfx);  ///< Array of industry tiles data
void ResetIndustries();

/* writable arrays of specs */
extern IndustrySpec _industry_specs[NUM_INDUSTRYTYPES];
extern IndustryTileSpec _industry_tile_specs[NUM_INDUSTRYTILES];

/**
 * Do industry gfx ID translation for NewGRFs.
 * @param gfx the type to get the override for.
 * @return the gfx to actually work with.
 */
static inline IndustryGfx GetTranslatedIndustryTileID(IndustryGfx gfx)
{
	/* the 0xFF should be GFX_WATERTILE_SPECIALCHECK but for reasons of include mess,
	 * we'll simplify the writing.
	 * Basically, the first test is required since the GFX_WATERTILE_SPECIALCHECK value
	 * will never be assigned as a tile index and is only required in order to do some
	 * tests while building the industry (as in WATER REQUIRED */
	if (gfx != 0xFF) {
		assert(gfx < INVALID_INDUSTRYTILE);
		const IndustryTileSpec *it = &_industry_tile_specs[gfx];
		return it->grf_prop.override == INVALID_INDUSTRYTILE ? gfx : it->grf_prop.override;
	} else {
		return gfx;
	}
}

extern uint16 _industry_counts[NUM_INDUSTRYTYPES]; // Number of industries per type ingame

/** Increment the count of industries for this type
 * @param type IndustryType to increment
 * @pre type < INVALID_INDUSTRYTYPE */
static inline void IncIndustryTypeCount(IndustryType type)
{
	assert(type < INVALID_INDUSTRYTYPE);
	_industry_counts[type]++;
}

/** Decrement the count of industries for this type
 * @param type IndustryType to decrement
 * @pre type < INVALID_INDUSTRYTYPE */
static inline void DecIndustryTypeCount(IndustryType type)
{
	assert(type < INVALID_INDUSTRYTYPE);
	_industry_counts[type]--;
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
	memset(&_industry_counts, 0, sizeof(_industry_counts));
}

static const uint8 IT_INVALID = 255;

#endif /* INDUSTRYTYPE_H */
