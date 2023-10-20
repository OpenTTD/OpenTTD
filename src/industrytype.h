/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file industrytype.h %Industry type specs. */

#ifndef INDUSTRYTYPE_H
#define INDUSTRYTYPE_H

#include "map_type.h"
#include "slope_type.h"
#include "industry_type.h"
#include "landscape_type.h"
#include "cargo_type.h"
#include "newgrf_animation_type.h"
#include "newgrf_commons.h"

enum IndustryCleanupType {
	CLEAN_RANDOMSOUNDS,    ///< Free the dynamically allocated sounds table
};

/** Available types of industry lifetimes. */
enum IndustryLifeType {
	INDUSTRYLIFE_BLACK_HOLE =      0, ///< Like power plants and banks
	INDUSTRYLIFE_EXTRACTIVE = 1 << 0, ///< Like mines
	INDUSTRYLIFE_ORGANIC    = 1 << 1, ///< Like forests
	INDUSTRYLIFE_PROCESSING = 1 << 2, ///< Like factories
};

/**
 * Available procedures to check whether an industry may build at a given location.
 * @see CheckNewIndustryProc, _check_new_industry_procs[]
 */
enum CheckProc {
	CHECK_NOTHING,    ///< Always succeeds.
	CHECK_FOREST,     ///< %Industry should be build above snow-line in arctic climate.
	CHECK_REFINERY,   ///< %Industry should be positioned near edge of the map.
	CHECK_FARM,       ///< %Industry should be below snow-line in arctic.
	CHECK_PLANTATION, ///< %Industry should NOT be in the desert.
	CHECK_WATER,      ///< %Industry should be in the desert.
	CHECK_LUMBERMILL, ///< %Industry should be in the rain forest.
	CHECK_BUBBLEGEN,  ///< %Industry should be in low land.
	CHECK_OIL_RIG,    ///< Industries at sea should be positioned near edge of the map.
	CHECK_END,        ///< End marker of the industry check procedures.
};

/** How was the industry created */
enum IndustryConstructionType {
	ICT_UNKNOWN,          ///< in previous game version or without newindustries activated
	ICT_NORMAL_GAMEPLAY,  ///< either by user or random creation process
	ICT_MAP_GENERATION,   ///< during random map creation
	ICT_SCENARIO_EDITOR,  ///< while editing a scenario
};

/** Various industry behaviours mostly to represent original TTD specialities */
enum IndustryBehaviour {
	INDUSTRYBEH_NONE                  =      0,
	INDUSTRYBEH_PLANT_FIELDS          = 1 << 0,  ///< periodically plants fields around itself (temp and arctic farms)
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
	INDUSTRYBEH_CARGOTYPES_UNLIMITED  = 1 << 18, ///< Allow produced/accepted cargoes callbacks to supply more than 2 and 3 types
	INDUSTRYBEH_NO_PAX_PROD_CLAMP     = 1 << 19, ///< Do not clamp production of passengers. (smooth economy only)
};
DECLARE_ENUM_AS_BIT_SET(IndustryBehaviour)

/** Flags for miscellaneous industry tile specialities */
enum IndustryTileSpecialFlags {
	INDTILE_SPECIAL_NONE                  = 0,
	INDTILE_SPECIAL_NEXTFRAME_RANDOMBITS  = 1 << 0, ///< Callback 0x26 needs random bits
	INDTILE_SPECIAL_ACCEPTS_ALL_CARGO     = 1 << 1, ///< Tile always accepts all cargoes the associated industry accepts
};
DECLARE_ENUM_AS_BIT_SET(IndustryTileSpecialFlags)

/** Definition of one tile in an industry tile layout */
struct IndustryTileLayoutTile {
	TileIndexDiffC ti;
	IndustryGfx gfx;
};

/** A complete tile layout for an industry is a list of tiles */
using IndustryTileLayout = std::vector<IndustryTileLayoutTile>;

/**
 * Defines the data structure for constructing industry.
 */
struct IndustrySpec {
	std::vector<IndustryTileLayout> layouts;    ///< List of possible tile layouts for the industry
	uint8_t cost_multiplier;                      ///< Base construction cost multiplier.
	uint32_t removal_cost_multiplier;             ///< Base removal cost multiplier.
	uint32_t prospecting_chance;                  ///< Chance prospecting succeeds
	IndustryType conflicting[3];                ///< Industries this industry cannot be close to
	byte check_proc;                            ///< Index to a procedure to check for conflicting circumstances
	CargoID produced_cargo[INDUSTRY_NUM_OUTPUTS];
	byte production_rate[INDUSTRY_NUM_OUTPUTS];
	/**
	 * minimum amount of cargo transported to the stations.
	 * If the waiting cargo is less than this number, no cargo is moved to it.
	 */
	byte minimal_cargo;
	CargoID accepts_cargo[INDUSTRY_NUM_INPUTS]; ///< 16 accepted cargoes.
	uint16_t input_cargo_multiplier[INDUSTRY_NUM_INPUTS][INDUSTRY_NUM_OUTPUTS]; ///< Input cargo multipliers (multiply amount of incoming cargo for the produced cargoes)
	IndustryLifeType life_type;                 ///< This is also known as Industry production flag, in newgrf specs
	byte climate_availability;                  ///< Bitmask, giving landscape enums as bit position
	IndustryBehaviour behaviour;                ///< How this industry will behave, and how others entities can use it
	byte map_colour;                            ///< colour used for the small map
	StringID name;                              ///< Displayed name of the industry
	StringID new_industry_text;                 ///< Message appearing when the industry is built
	StringID closure_text;                      ///< Message appearing when the industry closes
	StringID production_up_text;                ///< Message appearing when the industry's production is increasing
	StringID production_down_text;              ///< Message appearing when the industry's production is decreasing
	StringID station_name;                      ///< Default name for nearby station
	byte appear_ingame[NUM_LANDSCAPE];          ///< Probability of appearance in game
	byte appear_creation[NUM_LANDSCAPE];        ///< Probability of appearance during map creation
	uint8_t number_of_sounds;                     ///< Number of sounds available in the sounds array
	const uint8_t *random_sounds;                 ///< array of random sounds.
	/* Newgrf data */
	uint16_t callback_mask;                       ///< Bitmask of industry callbacks that have to be called
	uint8_t cleanup_flag;                         ///< flags indicating which data should be freed upon cleaning up
	bool enabled;                               ///< entity still available (by default true).newgrf can disable it, though
	GRFFileProps grf_prop;                      ///< properties related to the grf file

	bool IsRawIndustry() const;
	bool IsProcessingIndustry() const;
	Money GetConstructionCost() const;
	Money GetRemovalCost() const;
	bool UsesOriginalEconomy() const;

	~IndustrySpec();
};

/**
 * Defines the data structure of each individual tile of an industry.
 * @note A tile can at most accept 3 types of cargo, even if an industry as a whole can accept more types.
 */
struct IndustryTileSpec {
	std::array<CargoID, INDUSTRY_NUM_INPUTS> accepts_cargo; ///< Cargo accepted by this tile
	std::array<int8_t, INDUSTRY_NUM_INPUTS> acceptance; ///< Level of acceptance per cargo type (signed, may be negative!)
	Slope slopes_refused;                 ///< slope pattern on which this tile cannot be built
	byte anim_production;                 ///< Animation frame to start when goods are produced
	byte anim_next;                       ///< Next frame in an animation
	/**
	 * When true, the tile has to be drawn using the animation
	 * state instead of the construction state
	 */
	bool anim_state;
	/* Newgrf data */
	uint8_t callback_mask;                  ///< Bitmask of industry tile callbacks that have to be called
	AnimationInfo animation;              ///< Information about the animation (is it looping, how many loops etc)
	IndustryTileSpecialFlags special_flags; ///< Bitmask of extra flags used by the tile
	bool enabled;                         ///< entity still available (by default true).newgrf can disable it, though
	GRFFileProps grf_prop;                ///< properties related to the grf file
};

/* industry_cmd.cpp*/
const IndustrySpec *GetIndustrySpec(IndustryType thistype);    ///< Array of industries data
const IndustryTileSpec *GetIndustryTileSpec(IndustryGfx gfx);  ///< Array of industry tiles data
void ResetIndustries();

/* writable arrays of specs */
extern IndustrySpec _industry_specs[NUM_INDUSTRYTYPES];
extern IndustryTileSpec _industry_tile_specs[NUM_INDUSTRYTILES];

/* industry_gui.cpp */
void SortIndustryTypes();
/* Industry types sorted alphabetically by name. */
extern std::array<IndustryType, NUM_INDUSTRYTYPES> _sorted_industry_types;

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

static const uint8_t IT_INVALID = 255;

#endif /* INDUSTRYTYPE_H */
