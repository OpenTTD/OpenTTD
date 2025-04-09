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
#include "newgrf_badge_type.h"
#include "newgrf_callbacks.h"
#include "newgrf_commons.h"

/** Available types of industry lifetimes. */
enum class IndustryLifeType : uint8_t {
	Extractive = 0, ///< Like mines
	Organic = 1, ///< Like forests
	Processing = 2, ///< Like factories
};
using IndustryLifeTypes = EnumBitSet<IndustryLifeType, uint8_t>;

static constexpr IndustryLifeTypes INDUSTRYLIFE_BLACK_HOLE{}; ///< Like power plants and banks

/**
 * Available procedures to check whether an industry may build at a given location.
 * @see CheckNewIndustryProc, _check_new_industry_procs[]
 */
enum CheckProc : uint8_t {
	CHECK_NOTHING,    ///< Always succeeds.
	CHECK_FOREST,     ///< %Industry should be build above snow-line in arctic climate.
	CHECK_REFINERY,   ///< %Industry should be positioned near edge of the map.
	CHECK_FARM,       ///< %Industry should be below snow-line in arctic.
	CHECK_PLANTATION, ///< %Industry should NOT be in the desert.
	CHECK_WATER,      ///< %Industry should be in the desert.
	CHECK_LUMBERMILL, ///< %Industry should be in the rainforest.
	CHECK_BUBBLEGEN,  ///< %Industry should be in low land.
	CHECK_OIL_RIG,    ///< Industries at sea should be positioned near edge of the map.
	CHECK_END,        ///< End marker of the industry check procedures.
};

/** How was the industry created */
enum IndustryConstructionType : uint8_t {
	ICT_UNKNOWN,          ///< in previous game version or without newindustries activated
	ICT_NORMAL_GAMEPLAY,  ///< either by user or random creation process
	ICT_MAP_GENERATION,   ///< during random map creation
	ICT_SCENARIO_EDITOR,  ///< while editing a scenario
};

/** Various industry behaviours mostly to represent original TTD specialities */
enum class IndustryBehaviour : uint8_t {
	PlantFields = 0, ///< periodically plants fields around itself (temp and arctic farms)
	CutTrees = 1, ///< cuts trees and produce first output cargo from them (lumber mill)
	BuiltOnWater = 2, ///< is built on water (oil rig)
	Town1200More = 3, ///< can only be built in towns larger than 1200 inhabitants (temperate bank)
	OnlyInTown = 4, ///< can only be built in towns (arctic/tropic banks, water tower)
	OnlyNearTown = 5, ///< is always built near towns (toy shop)
	PlantOnBuild = 6, ///< Fields are planted around when built (all farms)
	DontIncrProd = 7, ///< do not increase production (oil wells) in the temperate climate
	Before1950 = 8, ///< can only be built before 1950 (oil wells)
	After1960 = 9, ///< can only be built after 1960 (oil rigs)
	AIAirShipRoutes = 10, ///< ai will attempt to establish air/ship routes to this industry (oil rig)
	AirplaneAttacks = 11, ///< can be exploded by a military airplane (oil refinery)
	ChopperAttacks = 12, ///< can be exploded by a military helicopter (factory)
	CanSubsidence = 13, ///< can cause a subsidence (coal mine, shaft that collapses)
	/* The following flags are only used for newindustries and do no represent any normal behaviour */
	ProdMultiHandling = 14, ///< Automatic production multiplier handling
	ProdCallbackRandom = 15, ///< Production callback needs random bits in var 10
	NoBuildMapCreation = 16, ///< Do not force one instance of this type to appear on map generation
	CanCloseLastInstance = 17, ///< Allow closing down the last instance of this type
	CargoTypesUnlimited = 18, ///< Allow produced/accepted cargoes callbacks to supply more than 2 and 3 types
	NoPaxProdClamp = 19, ///< Do not clamp production of passengers. (smooth economy only)
};
using IndustryBehaviours = EnumBitSet<IndustryBehaviour, uint32_t>;

/** Flags for miscellaneous industry tile specialities */
enum class IndustryTileSpecialFlag : uint8_t {
	NextFrameRandomBits = 0, ///< Callback 0x26 needs random bits
	AcceptsAllCargo = 1, ///< Tile always accepts all cargoes the associated industry accepts
};
using IndustryTileSpecialFlags = EnumBitSet<IndustryTileSpecialFlag, uint8_t>;

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
	uint8_t check_proc;                            ///< Index to a procedure to check for conflicting circumstances
	std::array<CargoType, INDUSTRY_NUM_OUTPUTS> produced_cargo;
	uint8_t production_rate[INDUSTRY_NUM_OUTPUTS];
	/**
	 * minimum amount of cargo transported to the stations.
	 * If the waiting cargo is less than this number, no cargo is moved to it.
	 */
	uint8_t minimal_cargo;
	std::array<CargoType, INDUSTRY_NUM_INPUTS> accepts_cargo; ///< 16 accepted cargoes.
	uint16_t input_cargo_multiplier[INDUSTRY_NUM_INPUTS][INDUSTRY_NUM_OUTPUTS]; ///< Input cargo multipliers (multiply amount of incoming cargo for the produced cargoes)
	IndustryLifeTypes life_type;                 ///< This is also known as Industry production flag, in newgrf specs
	LandscapeTypes climate_availability; ///< Bitmask, giving landscape enums as bit position
	IndustryBehaviours behaviour;                ///< How this industry will behave, and how others entities can use it
	uint8_t map_colour;                            ///< colour used for the small map
	StringID name;                              ///< Displayed name of the industry
	StringID new_industry_text;                 ///< Message appearing when the industry is built
	StringID closure_text;                      ///< Message appearing when the industry closes
	StringID production_up_text;                ///< Message appearing when the industry's production is increasing
	StringID production_down_text;              ///< Message appearing when the industry's production is decreasing
	StringID station_name;                      ///< Default name for nearby station
	uint8_t appear_ingame[NUM_LANDSCAPE];          ///< Probability of appearance in game
	uint8_t appear_creation[NUM_LANDSCAPE];        ///< Probability of appearance during map creation
	/* Newgrf data */
	IndustryCallbackMasks callback_mask;                       ///< Bitmask of industry callbacks that have to be called
	bool enabled;                               ///< entity still available (by default true).newgrf can disable it, though
	GRFFileProps grf_prop;                      ///< properties related to the grf file
	std::vector<uint8_t> random_sounds; ///< Random sounds;
	std::vector<BadgeID> badges;

	std::array<std::variant<CargoLabel, MixedCargoType>, INDUSTRY_ORIGINAL_NUM_OUTPUTS> produced_cargo_label; ///< Cargo labels of produced cargo for default industries.
	std::array<std::variant<CargoLabel, MixedCargoType>, INDUSTRY_ORIGINAL_NUM_INPUTS> accepts_cargo_label; ///< Cargo labels of accepted cargo for default industries.

	bool IsRawIndustry() const;
	bool IsProcessingIndustry() const;
	Money GetConstructionCost() const;
	Money GetRemovalCost() const;
	bool UsesOriginalEconomy() const;
};

/**
 * Defines the data structure of each individual tile of an industry.
 * @note A tile can at most accept 3 types of cargo, even if an industry as a whole can accept more types.
 */
struct IndustryTileSpec {
	std::array<CargoType, INDUSTRY_NUM_INPUTS> accepts_cargo; ///< Cargo accepted by this tile
	std::array<int8_t, INDUSTRY_NUM_INPUTS> acceptance; ///< Level of acceptance per cargo type (signed, may be negative!)
	Slope slopes_refused;                 ///< slope pattern on which this tile cannot be built
	uint8_t anim_production;                 ///< Animation frame to start when goods are produced
	uint8_t anim_next;                       ///< Next frame in an animation
	/**
	 * When true, the tile has to be drawn using the animation
	 * state instead of the construction state
	 */
	bool anim_state;
	/* Newgrf data */
	IndustryTileCallbackMasks callback_mask;                  ///< Bitmask of industry tile callbacks that have to be called
	AnimationInfo animation;              ///< Information about the animation (is it looping, how many loops etc)
	IndustryTileSpecialFlags special_flags; ///< Bitmask of extra flags used by the tile
	bool enabled;                         ///< entity still available (by default true).newgrf can disable it, though
	GRFFileProps grf_prop;                ///< properties related to the grf file
	std::vector<BadgeID> badges;

	std::array<std::variant<CargoLabel, MixedCargoType>, INDUSTRY_ORIGINAL_NUM_INPUTS> accepts_cargo_label; ///< Cargo labels of accepted cargo for default industry tiles.
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
inline IndustryGfx GetTranslatedIndustryTileID(IndustryGfx gfx)
{
	/* the 0xFF should be GFX_WATERTILE_SPECIALCHECK but for reasons of include mess,
	 * we'll simplify the writing.
	 * Basically, the first test is required since the GFX_WATERTILE_SPECIALCHECK value
	 * will never be assigned as a tile index and is only required in order to do some
	 * tests while building the industry (as in WATER REQUIRED */
	if (gfx != 0xFF) {
		assert(gfx < NUM_INDUSTRYTILES);
		const IndustryTileSpec *it = &_industry_tile_specs[gfx];
		return it->grf_prop.override_id == INVALID_INDUSTRYTILE ? gfx : it->grf_prop.override_id;
	} else {
		return gfx;
	}
}

#endif /* INDUSTRYTYPE_H */
