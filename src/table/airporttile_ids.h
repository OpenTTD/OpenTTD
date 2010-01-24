/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file airporttile_id.h Enum of the default airport tiles. */

#ifndef AIRPORTTILE_IDS_H
#define AIRPORTTILE_IDS_H

enum AirportTiles {
	APT_APRON                  = 0,
	APT_APRON_FENCE_NW         = 1,
	APT_APRON_FENCE_SW         = 2,
	APT_STAND                  = 3,
	APT_APRON_W                = 4,
	APT_APRON_S                = 5,
	APT_APRON_VER_CROSSING_S   = 6,
	APT_APRON_HOR_CROSSING_W   = 7,
	APT_APRON_VER_CROSSING_N   = 8,
	APT_APRON_HOR_CROSSING_E   = 9,
	APT_APRON_E                = 10,
	APT_ARPON_N                = 11,
	APT_APRON_HOR              = 12,
	APT_APRON_N_FENCE_SW       = 13,
	APT_RUNWAY_1               = 14,
	APT_RUNWAY_2               = 15,
	APT_RUNWAY_3               = 16,
	APT_RUNWAY_4               = 17,
	APT_RUNWAY_END_FENCE_SE    = 18,
	APT_BUILDING_2             = 19,
	APT_TOWER_FENCE_SW         = 20,
	APT_ROUND_TERMINAL         = 21,
	APT_BUILDING_3             = 22,
	APT_BUILDING_1             = 23,
	APT_DEPOT_SE               = 24,
	APT_STAND_1                = 25,
	APT_STAND_PIER_NE          = 26,
	APT_PIER_NW_NE             = 27,
	APT_PIER                   = 28,
	APT_EMPTY                  = 29,
	APT_EMPTY_FENCE_NE         = 30,
	APT_RADAR_GRASS_FENCE_SW   = 31,
	/* 32-42 are for turning the radar */
	APT_RADIO_TOWER_FENCE_NE   = 43,
	APT_SMALL_BUILDING_3       = 44,
	APT_SMALL_BUILDING_2       = 45,
	APT_SMALL_BUILDING_1       = 46,
	APT_GRASS_FENCE_SW         = 47,
	APT_GRASS_2                = 48,
	APT_GRASS_1                = 49,
	APT_GRASS_FENCE_NE_FLAG    = 50,
	/* 51-53 are for flag animation */
	APT_RUNWAY_SMALL_NEAR_END  = 54,
	APT_RUNWAY_SMALL_MIDDLE    = 55,
	APT_RUNWAY_SMALL_FAR_END   = 56,
	APT_SMALL_DEPOT_SE         = 57,
	APT_HELIPORT               = 58,
	APT_RUNWAY_END             = 59,
	APT_RUNWAY_5               = 60,
	APT_TOWER                  = 61,
	APT_SMALL_DEPOT_SE_2       = 62, // unused (copy of APT_SMALL_DEPOT_SE)
	APT_APRON_FENCE_NE         = 63,
	APT_RUNWAY_END_FENCE_NW    = 64,
	APT_RUNWAY_FENCE_NW        = 65,
	APT_RADAR_FENCE_SW         = 66,
	/* 67-77 are for turning the radar */
	APT_RADAR_FENCE_NE         = 78,
	/* 79-89 are for turning the radar */
	APT_HELIPAD_1              = 90,
	APT_HELIPAD_2_FENCE_NW     = 91,
	APT_HELIPAD_2              = 92,
	APT_APRON_FENCE_NE_SW      = 93,
	APT_RUNWAY_END_FENCE_NW_SW = 94,
	APT_RUNWAY_END_FENCE_SE_SW = 95,
	APT_RUNWAY_END_FENCE_NE_NW = 96,
	APT_RUNWAY_END_FENCE_NE_SE = 97,
	APT_HELIPAD_2_FENCE_NE_SE  = 98,
	APT_APRON_FENCE_SE_SW      = 99,
	APT_LOW_BUILDING_FENCE_N   = 100,
	APT_ROT_RUNWAY_FENCE_NE    = 101, // unused
	APT_ROT_RUNWAY_END_FENCE_NE= 102, // unused
	APT_ROT_RUNWAY_FENCE_SW    = 103, // unused
	APT_ROT_RUNWAY_END_FENCE_SW= 104, // unused
	APT_DEPOT_SW               = 105, // unused
	APT_DEPOT_NW               = 106, // unused
	APT_DEPOT_NE               = 107, // unused
	APT_HELIPAD_2_FENCE_SE_SW  = 108, // unused
	APT_HELIPAD_2_FENCE_SE     = 109, // unused
	APT_LOW_BUILDING_FENCE_NW  = 110,
	APT_LOW_BUILDING_FENCE_NE  = 111, // unused
	APT_LOW_BUILDING_FENCE_SW  = 112, // unused
	APT_LOW_BUILDING_FENCE_SE  = 113, // unused
	APT_STAND_FENCE_NE         = 114, // unused
	APT_STAND_FENCE_SE         = 115, // unused
	APT_STAND_FENCE_SW         = 116, // unused
	APT_APRON_FENCE_NE_2       = 117, // unused (copy of APT_APRON_FENCE_NE)
	APT_APRON_FENCE_SE         = 118,
	APT_HELIPAD_2_FENCE_NW_SW  = 119, // unused
	APT_HELIPAD_2_FENCE_SW     = 120, // unused
	APT_RADAR_FENCE_SE         = 121, // unused
	/* 122-132 used for radar rotation */
	APT_HELIPAD_3_FENCE_SE_SW  = 133,
	APT_HELIPAD_3_FENCE_NW_SW  = 134,
	APT_HELIPAD_3_FENCE_NW     = 135,
	APT_LOW_BUILDING           = 136,
	APT_APRON_FENCE_NE_SE      = 137,
	APT_APRON_HALF_EAST        = 138,
	APT_APRON_HALF_WEST        = 139,
	APT_GRASS_FENCE_NE_FLAG_2  = 140,
	/* 141-143 used for flag animation */
};

#endif /* AIRPORTTILE_IDS_H */
