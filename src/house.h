/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file house.h definition of HouseSpec and accessors */

#ifndef HOUSE_H
#define HOUSE_H

#include "cargo_type.h"
#include "timer/timer_game_calendar.h"
#include "house_type.h"
#include "newgrf_animation_type.h"
#include "newgrf_commons.h"

/**
 * Simple value that indicates the house has reached the final stage of
 * construction.
 */
static const byte TOWN_HOUSE_COMPLETED = 3;

static const HouseID NUM_HOUSES_PER_GRF = 255;    ///< Number of supported houses per NewGRF; limited to 255 to allow extending Action3 with an extended byte later on.

static const uint HOUSE_NO_CLASS      = 0;
static const HouseID NEW_HOUSE_OFFSET = 110;    ///< Offset for new houses.
static const HouseID NUM_HOUSES       = 512;    ///< Total number of houses.
static const HouseID INVALID_HOUSE_ID = 0xFFFF;

static const uint HOUSE_NUM_ACCEPTS = 16; ///< Max number of cargoes accepted by a tile

/**
 * There can only be as many classes as there are new houses, plus one for
 * NO_CLASS, as the original houses don't have classes.
 */
static const uint HOUSE_CLASS_MAX  = NUM_HOUSES - NEW_HOUSE_OFFSET + 1;

enum BuildingFlags {
	TILE_NO_FLAG         =       0,
	TILE_SIZE_1x1        = 1U << 0,
	TILE_NOT_SLOPED      = 1U << 1,
	TILE_SIZE_2x1        = 1U << 2,
	TILE_SIZE_1x2        = 1U << 3,
	TILE_SIZE_2x2        = 1U << 4,
	BUILDING_IS_ANIMATED = 1U << 5,
	BUILDING_IS_CHURCH   = 1U << 6,
	BUILDING_IS_STADIUM  = 1U << 7,
	BUILDING_HAS_1_TILE  = TILE_SIZE_1x1 | TILE_SIZE_2x1 | TILE_SIZE_1x2 | TILE_SIZE_2x2,
	BUILDING_HAS_2_TILES = TILE_SIZE_2x1 | TILE_SIZE_1x2 | TILE_SIZE_2x2,
	BUILDING_2_TILES_X   = TILE_SIZE_2x1 | TILE_SIZE_2x2,
	BUILDING_2_TILES_Y   = TILE_SIZE_1x2 | TILE_SIZE_2x2,
	BUILDING_HAS_4_TILES = TILE_SIZE_2x2,
};
DECLARE_ENUM_AS_BIT_SET(BuildingFlags)

enum HouseZonesBits {
	HZB_BEGIN     = 0,
	HZB_TOWN_EDGE = 0,
	HZB_TOWN_OUTSKIRT,
	HZB_TOWN_OUTER_SUBURB,
	HZB_TOWN_INNER_SUBURB,
	HZB_TOWN_CENTRE,
	HZB_END,
};
static_assert(HZB_END == 5);

DECLARE_POSTFIX_INCREMENT(HouseZonesBits)

enum HouseZones {                  ///< Bit  Value       Meaning
	HZ_NOZNS             = 0x0000,  ///<       0          This is just to get rid of zeros, meaning none
	HZ_ZON1              = 1U << HZB_TOWN_EDGE,    ///< 0..4 1,2,4,8,10  which town zones the building can be built in, Zone1 been the further suburb
	HZ_ZON2              = 1U << HZB_TOWN_OUTSKIRT,
	HZ_ZON3              = 1U << HZB_TOWN_OUTER_SUBURB,
	HZ_ZON4              = 1U << HZB_TOWN_INNER_SUBURB,
	HZ_ZON5              = 1U << HZB_TOWN_CENTRE,  ///<  center of town
	HZ_ZONALL            = 0x001F,  ///<       1F         This is just to englobe all above types at once
	HZ_SUBARTC_ABOVE     = 0x0800,  ///< 11    800        can appear in sub-arctic climate above the snow line
	HZ_TEMP              = 0x1000,  ///< 12   1000        can appear in temperate climate
	HZ_SUBARTC_BELOW     = 0x2000,  ///< 13   2000        can appear in sub-arctic climate below the snow line
	HZ_SUBTROPIC         = 0x4000,  ///< 14   4000        can appear in subtropical climate
	HZ_TOYLND            = 0x8000,  ///< 15   8000        can appear in toyland climate
	HZ_CLIMALL           = 0xF800,  ///< Bitmask of all climate bits
};
DECLARE_ENUM_AS_BIT_SET(HouseZones)

enum HouseExtraFlags {
	NO_EXTRA_FLAG            =       0,
	BUILDING_IS_HISTORICAL   = 1U << 0,  ///< this house will only appear during town generation in random games, thus the historical
	BUILDING_IS_PROTECTED    = 1U << 1,  ///< towns and AI will not remove this house, while human players will be able to
	SYNCHRONISED_CALLBACK_1B = 1U << 2,  ///< synchronized callback 1B will be performed, on multi tile houses
	CALLBACK_1A_RANDOM_BITS  = 1U << 3,  ///< callback 1A needs random bits
};

DECLARE_ENUM_AS_BIT_SET(HouseExtraFlags)

struct HouseSpec {
	/* Standard properties */
	TimerGameCalendar::Year min_year;         ///< introduction year of the house
	TimerGameCalendar::Year max_year;         ///< last year it can be built
	byte population;                          ///< population (Zero on other tiles in multi tile house.)
	byte removal_cost;                        ///< cost multiplier for removing it
	StringID building_name;                   ///< building name
	uint16_t remove_rating_decrease;            ///< rating decrease if removed
	byte mail_generation;                     ///< mail generation multiplier (tile based, as the acceptances below)
	byte cargo_acceptance[HOUSE_NUM_ACCEPTS]; ///< acceptance level for the cargo slots
	CargoID accepts_cargo[HOUSE_NUM_ACCEPTS]; ///< input cargo slots
	BuildingFlags building_flags;             ///< some flags that describe the house (size, stadium etc...)
	HouseZones building_availability;         ///< where can it be built (climates, zones)
	bool enabled;                             ///< the house is available to build (true by default, but can be disabled by newgrf)

	/* NewHouses properties */
	GRFFileProps grf_prop;                    ///< Properties related the the grf file
	uint16_t callback_mask;                     ///< Bitmask of house callbacks that have to be called
	byte random_colour[4];                    ///< 4 "random" colours
	byte probability;                         ///< Relative probability of appearing (16 is the standard value)
	HouseExtraFlags extra_flags;              ///< some more flags
	HouseClassID class_id;                    ///< defines the class this house has (not grf file based)
	AnimationInfo animation;                  ///< information about the animation.
	byte processing_time;                     ///< Periodic refresh multiplier
	byte minimum_life;                        ///< The minimum number of years this house will survive before the town rebuilds it
	CargoTypes watched_cargoes;               ///< Cargo types watched for acceptance.

	Money GetRemovalCost() const;

	static inline HouseSpec *Get(size_t house_id)
	{
		assert(house_id < NUM_HOUSES);
		extern HouseSpec _house_specs[];
		return &_house_specs[house_id];
	}
};

/**
 * Do HouseID translation for NewGRFs.
 * @param hid the HouseID to get the override for.
 * @return the HouseID to actually work with.
 */
static inline HouseID GetTranslatedHouseID(HouseID hid)
{
	const HouseSpec *hs = HouseSpec::Get(hid);
	return hs->grf_prop.override == INVALID_HOUSE_ID ? hid : hs->grf_prop.override;
}

#endif /* HOUSE_H */
