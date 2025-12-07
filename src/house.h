/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file house.h definition of HouseSpec and accessors */

#ifndef HOUSE_H
#define HOUSE_H

#include "cargo_type.h"
#include "timer/timer_game_calendar.h"
#include "house_type.h"
#include "newgrf_animation_type.h"
#include "newgrf_badge_type.h"
#include "newgrf_callbacks.h"
#include "newgrf_commons.h"

/**
 * Simple value that indicates the house has reached the final stage of
 * construction.
 */
static const uint8_t TOWN_HOUSE_COMPLETED = 3;

static const uint HOUSE_NO_CLASS      = 0;
static const HouseID NEW_HOUSE_OFFSET = 110; ///< Offset for new houses.
static const HouseID NUM_HOUSES       = 4096; ///< Total number of houses.
static const HouseID INVALID_HOUSE_ID = UINT16_MAX;

static const HouseID NUM_HOUSES_PER_GRF = NUM_HOUSES; ///< Number of supported houses per NewGRF.

static const uint HOUSE_NUM_ACCEPTS = 16; ///< Max number of cargoes accepted by a tile
static const uint HOUSE_ORIGINAL_NUM_ACCEPTS = 3; ///< Original number of accepted cargo types.

enum class BuildingFlag : uint8_t {
	Size1x1    = 0,
	NotSloped  = 1,
	Size2x1    = 2,
	Size1x2    = 3,
	Size2x2    = 4,
	IsAnimated = 5,
	IsChurch   = 6,
	IsStadium  = 7,
};
using BuildingFlags = EnumBitSet<BuildingFlag, uint8_t>;

static constexpr BuildingFlags BUILDING_HAS_1_TILE  = {BuildingFlag::Size1x1, BuildingFlag::Size2x1, BuildingFlag::Size1x2, BuildingFlag::Size2x2};
static constexpr BuildingFlags BUILDING_HAS_2_TILES = {BuildingFlag::Size2x1, BuildingFlag::Size1x2, BuildingFlag::Size2x2};
static constexpr BuildingFlags BUILDING_2_TILES_X   = {BuildingFlag::Size2x1, BuildingFlag::Size2x2};
static constexpr BuildingFlags BUILDING_2_TILES_Y   = {BuildingFlag::Size1x2, BuildingFlag::Size2x2};
static constexpr BuildingFlags BUILDING_HAS_4_TILES = {BuildingFlag::Size2x2};

enum class HouseZone : uint8_t {
	TownEdge = 0,
	TownOutskirt = 1,
	TownOuterSuburb = 2,
	TownInnerSuburb = 3,
	TownCentre = 4,
	TownEnd,

	ClimateSubarcticAboveSnow = 11, ///< Building can appear in sub-arctic climate above the snow line
	ClimateTemperate = 12, ///< Building can appear in temperate climate
	ClimateSubarcticBelowSnow = 13, ///< Building can appear in sub-arctic climate below the snow line
	ClimateSubtropic = 14, ///< Building can appear in subtropical climate
	ClimateToyland = 15, ///< Building can appear in toyland climate
};
using HouseZones = EnumBitSet<HouseZone, uint16_t>;

static constexpr uint NUM_HOUSE_ZONES = to_underlying(HouseZone::TownEnd);
static_assert(NUM_HOUSE_ZONES == 5);

static constexpr HouseZones HZ_ZONE_ALL = {
	HouseZone::TownEdge,
	HouseZone::TownOutskirt,
	HouseZone::TownOuterSuburb,
	HouseZone::TownInnerSuburb,
	HouseZone::TownCentre,
};

static constexpr HouseZones HZ_CLIMATE_ALL = {
	HouseZone::ClimateSubarcticAboveSnow,
	HouseZone::ClimateTemperate,
	HouseZone::ClimateSubarcticBelowSnow,
	HouseZone::ClimateSubtropic,
	HouseZone::ClimateToyland,
};

enum class HouseExtraFlag : uint8_t {
	BuildingIsHistorical   = 0, ///< this house will only appear during town generation in random games, thus the historical
	BuildingIsProtected    = 1, ///< towns and AI will not remove this house, while human players will be able to
	SynchronisedCallback1B = 2, ///< synchronized callback 1B will be performed, on multi tile houses
	Callback1ARandomBits   = 3, ///< callback 1A needs random bits
};
using HouseExtraFlags = EnumBitSet<HouseExtraFlag, uint8_t>;

struct HouseSpec {
	/* Standard properties */
	TimerGameCalendar::Year min_year;         ///< introduction year of the house
	TimerGameCalendar::Year max_year;         ///< last year it can be built
	uint8_t population;                          ///< population (Zero on other tiles in multi tile house.)
	uint8_t removal_cost;                        ///< cost multiplier for removing it
	StringID building_name;                   ///< building name
	uint16_t remove_rating_decrease;            ///< rating decrease if removed
	uint8_t mail_generation;                     ///< mail generation multiplier (tile based, as the acceptances below)
	uint8_t cargo_acceptance[HOUSE_NUM_ACCEPTS]; ///< acceptance level for the cargo slots
	CargoType accepts_cargo[HOUSE_NUM_ACCEPTS]; ///< input cargo slots
	BuildingFlags building_flags;             ///< some flags that describe the house (size, stadium etc...)
	HouseZones building_availability;         ///< where can it be built (climates, zones)
	bool enabled;                             ///< the house is available to build (true by default, but can be disabled by newgrf)

	/* NewHouses properties */
	SubstituteGRFFileProps grf_prop; ///< Properties related the the grf file
	HouseCallbackMasks callback_mask;                     ///< Bitmask of house callbacks that have to be called
	Colours random_colour[4];                 ///< 4 "random" colours
	uint8_t probability;                         ///< Relative probability of appearing (16 is the standard value)
	HouseExtraFlags extra_flags{};              ///< some more flags
	HouseClassID class_id;                    ///< defines the class this house has (not grf file based)
	AnimationInfo<void> animation; ///< information about the animation.
	uint8_t processing_time;                     ///< Periodic refresh multiplier
	uint8_t minimum_life;                        ///< The minimum number of years this house will survive before the town rebuilds it
	CargoTypes watched_cargoes;               ///< Cargo types watched for acceptance.
	std::vector<BadgeID> badges;

	CargoLabel accepts_cargo_label[HOUSE_ORIGINAL_NUM_ACCEPTS]; ///< input landscape cargo slots

	HouseID Index() const;
	Money GetRemovalCost() const;

	static std::vector<HouseSpec> &Specs();
	static HouseSpec *Get(size_t house_id);
};

/**
 * Do HouseID translation for NewGRFs.
 * @param hid the HouseID to get the override for.
 * @return the HouseID to actually work with.
 */
inline HouseID GetTranslatedHouseID(HouseID hid)
{
	const HouseSpec *hs = HouseSpec::Get(hid);
	return hs->grf_prop.override_id == INVALID_HOUSE_ID ? hid : hs->grf_prop.override_id;
}

void ShowBuildHousePicker(struct Window *);
HouseZones GetClimateMaskForLandscape();

#endif /* HOUSE_H */
