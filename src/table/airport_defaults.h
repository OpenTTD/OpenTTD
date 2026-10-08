/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file airport_defaults.h Tables with default values for airports and airport tiles. */

#ifndef AIRPORT_DEFAULTS_H
#define AIRPORT_DEFAULTS_H

#include "../timer/timer_game_calendar.h"

/** Tiles for Country Airfield (small) */
static const std::initializer_list<AirportTileTable> _tile_table_country_0 = {
	{{0, 0}, APT_SMALL_BUILDING_1},
	{{1, 0}, APT_SMALL_BUILDING_2},
	{{2, 0}, APT_SMALL_BUILDING_3},
	{{3, 0}, APT_SMALL_DEPOT_SE},
	{{0, 1}, APT_GRASS_FENCE_NE_FLAG},
	{{1, 1}, APT_GRASS_1},
	{{2, 1}, APT_GRASS_2},
	{{3, 1}, APT_GRASS_FENCE_SW},
	{{0, 2}, APT_RUNWAY_SMALL_FAR_END},
	{{1, 2}, APT_RUNWAY_SMALL_MIDDLE},
	{{2, 2}, APT_RUNWAY_SMALL_MIDDLE},
	{{3, 2}, APT_RUNWAY_SMALL_NEAR_END},
};

static const std::initializer_list<AirportTileLayout> _tile_table_country = {
	{ _tile_table_country_0, Direction::N },
};

/** Tiles for Commuter Airfield (small) */
static const std::initializer_list<AirportTileTable> _tile_table_commuter_0 = {
	{{0, 0}, APT_TOWER},
	{{1, 0}, APT_BUILDING_3},
	{{2, 0}, APT_HELIPAD_2_FENCE_NW},
	{{3, 0}, APT_HELIPAD_2_FENCE_NW},
	{{4, 0}, APT_DEPOT_SE},
	{{0, 1}, APT_APRON_FENCE_NE},
	{{1, 1}, APT_APRON},
	{{2, 1}, APT_APRON},
	{{3, 1}, APT_APRON},
	{{4, 1}, APT_APRON_FENCE_SW},
	{{0, 2}, APT_APRON_FENCE_NE},
	{{1, 2}, APT_STAND},
	{{2, 2}, APT_STAND},
	{{3, 2}, APT_STAND},
	{{4, 2}, APT_APRON_FENCE_SW},
	{{0, 3}, APT_RUNWAY_END_FENCE_SE},
	{{1, 3}, APT_RUNWAY_2},
	{{2, 3}, APT_RUNWAY_2},
	{{3, 3}, APT_RUNWAY_2},
	{{4, 3}, APT_RUNWAY_END_FENCE_SE},
};

static const std::initializer_list<AirportTileLayout> _tile_table_commuter = {
	{ _tile_table_commuter_0, Direction::N },
};

/** Tiles for City Airport (large) */
static const std::initializer_list<AirportTileTable> _tile_table_city_0 = {
	{{0, 0}, APT_BUILDING_1},
	{{1, 0}, APT_APRON_FENCE_NW},
	{{2, 0}, APT_STAND_1},
	{{3, 0}, APT_APRON_FENCE_NW},
	{{4, 0}, APT_APRON_FENCE_NW},
	{{5, 0}, APT_DEPOT_SE},
	{{0, 1}, APT_BUILDING_2},
	{{1, 1}, APT_PIER},
	{{2, 1}, APT_ROUND_TERMINAL},
	{{3, 1}, APT_STAND_PIER_NE},
	{{4, 1}, APT_APRON},
	{{5, 1}, APT_APRON_FENCE_SW},
	{{0, 2}, APT_BUILDING_3},
	{{1, 2}, APT_STAND},
	{{2, 2}, APT_PIER_NW_NE},
	{{3, 2}, APT_APRON_S},
	{{4, 2}, APT_APRON_HOR},
	{{5, 2}, APT_APRON_N_FENCE_SW},
	{{0, 3}, APT_RADIO_TOWER_FENCE_NE},
	{{1, 3}, APT_APRON_W},
	{{2, 3}, APT_APRON_VER_CROSSING_S},
	{{3, 3}, APT_APRON_HOR_CROSSING_E},
	{{4, 3}, APT_ARPON_N},
	{{5, 3}, APT_TOWER_FENCE_SW},
	{{0, 4}, APT_EMPTY_FENCE_NE},
	{{1, 4}, APT_APRON_S},
	{{2, 4}, APT_APRON_HOR_CROSSING_W},
	{{3, 4}, APT_APRON_VER_CROSSING_N},
	{{4, 4}, APT_APRON_E},
	{{5, 4}, APT_RADAR_GRASS_FENCE_SW},
	{{0, 5}, APT_RUNWAY_END_FENCE_SE},
	{{1, 5}, APT_RUNWAY_1},
	{{2, 5}, APT_RUNWAY_2},
	{{3, 5}, APT_RUNWAY_3},
	{{4, 5}, APT_RUNWAY_4},
	{{5, 5}, APT_RUNWAY_END_FENCE_SE},
};

static const std::initializer_list<AirportTileLayout> _tile_table_city = {
	{ _tile_table_city_0, Direction::N },
};

/** Tiles for Metropolitan Airport (large) - 2 runways */
static const std::initializer_list<AirportTileTable> _tile_table_metropolitan_0 = {
	{{0, 0}, APT_BUILDING_1},
	{{1, 0}, APT_APRON_FENCE_NW},
	{{2, 0}, APT_STAND_1},
	{{3, 0}, APT_APRON_FENCE_NW},
	{{4, 0}, APT_APRON_FENCE_NW},
	{{5, 0}, APT_DEPOT_SE},
	{{0, 1}, APT_BUILDING_2},
	{{1, 1}, APT_PIER},
	{{2, 1}, APT_ROUND_TERMINAL},
	{{3, 1}, APT_STAND_PIER_NE},
	{{4, 1}, APT_APRON},
	{{5, 1}, APT_APRON_FENCE_SW},
	{{0, 2}, APT_BUILDING_3},
	{{1, 2}, APT_STAND},
	{{2, 2}, APT_PIER_NW_NE},
	{{3, 2}, APT_APRON_S},
	{{4, 2}, APT_APRON_HOR},
	{{5, 2}, APT_APRON_N_FENCE_SW},
	{{0, 3}, APT_RADAR_FENCE_NE},
	{{1, 3}, APT_APRON},
	{{2, 3}, APT_APRON},
	{{3, 3}, APT_APRON},
	{{4, 3}, APT_APRON},
	{{5, 3}, APT_TOWER_FENCE_SW},
	{{0, 4}, APT_RUNWAY_END},
	{{1, 4}, APT_RUNWAY_5},
	{{2, 4}, APT_RUNWAY_5},
	{{3, 4}, APT_RUNWAY_5},
	{{4, 4}, APT_RUNWAY_5},
	{{5, 4}, APT_RUNWAY_END},
	{{0, 5}, APT_RUNWAY_END_FENCE_SE},
	{{1, 5}, APT_RUNWAY_2},
	{{2, 5}, APT_RUNWAY_2},
	{{3, 5}, APT_RUNWAY_2},
	{{4, 5}, APT_RUNWAY_2},
	{{5, 5}, APT_RUNWAY_END_FENCE_SE},
};

static const std::initializer_list<AirportTileLayout> _tile_table_metropolitan = {
	{ _tile_table_metropolitan_0, Direction::N },
};

/** Tiles for International Airport (large) - 2 runways */
static const std::initializer_list<AirportTileTable> _tile_table_international_0 = {
	{{0, 0}, APT_RUNWAY_END_FENCE_NW},
	{{1, 0}, APT_RUNWAY_FENCE_NW},
	{{2, 0}, APT_RUNWAY_FENCE_NW},
	{{3, 0}, APT_RUNWAY_FENCE_NW},
	{{4, 0}, APT_RUNWAY_FENCE_NW},
	{{5, 0}, APT_RUNWAY_FENCE_NW},
	{{6, 0}, APT_RUNWAY_END_FENCE_NW},
	{{0, 1}, APT_RADIO_TOWER_FENCE_NE},
	{{1, 1}, APT_APRON},
	{{2, 1}, APT_APRON},
	{{3, 1}, APT_APRON},
	{{4, 1}, APT_APRON},
	{{5, 1}, APT_APRON},
	{{6, 1}, APT_DEPOT_SE},
	{{0, 2}, APT_BUILDING_3},
	{{1, 2}, APT_APRON},
	{{2, 2}, APT_STAND},
	{{3, 2}, APT_BUILDING_2},
	{{4, 2}, APT_STAND},
	{{5, 2}, APT_APRON},
	{{6, 2}, APT_APRON_FENCE_SW},
	{{0, 3}, APT_DEPOT_SE},
	{{1, 3}, APT_APRON},
	{{2, 3}, APT_STAND},
	{{3, 3}, APT_BUILDING_2},
	{{4, 3}, APT_STAND},
	{{5, 3}, APT_APRON},
	{{6, 3}, APT_HELIPAD_1},
	{{0, 4}, APT_APRON_FENCE_NE},
	{{1, 4}, APT_APRON},
	{{2, 4}, APT_STAND},
	{{3, 4}, APT_TOWER},
	{{4, 4}, APT_STAND},
	{{5, 4}, APT_APRON},
	{{6, 4}, APT_HELIPAD_1},
	{{0, 5}, APT_APRON_FENCE_NE},
	{{1, 5}, APT_APRON},
	{{2, 5}, APT_APRON},
	{{3, 5}, APT_APRON},
	{{4, 5}, APT_APRON},
	{{5, 5}, APT_APRON},
	{{6, 5}, APT_RADAR_FENCE_SW},
	{{0, 6}, APT_RUNWAY_END_FENCE_SE},
	{{1, 6}, APT_RUNWAY_2},
	{{2, 6}, APT_RUNWAY_2},
	{{3, 6}, APT_RUNWAY_2},
	{{4, 6}, APT_RUNWAY_2},
	{{5, 6}, APT_RUNWAY_2},
	{{6, 6}, APT_RUNWAY_END_FENCE_SE},
};

static const std::initializer_list<AirportTileLayout> _tile_table_international = {
	{ _tile_table_international_0, Direction::N },
};

/** Tiles for International Airport (large) - 2 runways */
static const std::initializer_list<AirportTileTable> _tile_table_intercontinental_0 = {
	{{0, 0}, APT_RADAR_FENCE_NE},
	{{1, 0}, APT_RUNWAY_END_FENCE_NE_NW},
	{{2, 0}, APT_RUNWAY_FENCE_NW},
	{{3, 0}, APT_RUNWAY_FENCE_NW},
	{{4, 0}, APT_RUNWAY_FENCE_NW},
	{{5, 0}, APT_RUNWAY_FENCE_NW},
	{{6, 0}, APT_RUNWAY_FENCE_NW},
	{{7, 0}, APT_RUNWAY_FENCE_NW},
	{{8, 0}, APT_RUNWAY_END_FENCE_NW_SW},
	{{0, 1}, APT_RUNWAY_END_FENCE_NE_NW},
	{{1, 1}, APT_RUNWAY_2},
	{{2, 1}, APT_RUNWAY_2},
	{{3, 1}, APT_RUNWAY_2},
	{{4, 1}, APT_RUNWAY_2},
	{{5, 1}, APT_RUNWAY_2},
	{{6, 1}, APT_RUNWAY_2},
	{{7, 1}, APT_RUNWAY_END_FENCE_SE_SW},
	{{8, 1}, APT_APRON_FENCE_NE_SW},
	{{0, 2}, APT_APRON_FENCE_NE_SW},
	{{1, 2}, APT_EMPTY},
	{{2, 2}, APT_APRON_FENCE_NE},
	{{3, 2}, APT_APRON},
	{{4, 2}, APT_APRON},
	{{5, 2}, APT_APRON},
	{{6, 2}, APT_APRON},
	{{7, 2}, APT_RADIO_TOWER_FENCE_NE},
	{{8, 2}, APT_APRON_FENCE_NE_SW},
	{{0, 3}, APT_APRON_FENCE_NE},
	{{1, 3}, APT_APRON_HALF_EAST},
	{{2, 3}, APT_APRON_FENCE_NE},
	{{3, 3}, APT_TOWER},
	{{4, 3}, APT_HELIPAD_2},
	{{5, 3}, APT_HELIPAD_2},
	{{6, 3}, APT_APRON},
	{{7, 3}, APT_APRON_FENCE_NW},
	{{8, 3}, APT_APRON_FENCE_SW},
	{{0, 4}, APT_APRON_FENCE_NE},
	{{1, 4}, APT_APRON},
	{{2, 4}, APT_APRON},
	{{3, 4}, APT_STAND},
	{{4, 4}, APT_BUILDING_1},
	{{5, 4}, APT_STAND},
	{{6, 4}, APT_APRON},
	{{7, 4}, APT_LOW_BUILDING},
	{{8, 4}, APT_DEPOT_SE},
	{{0, 5}, APT_DEPOT_SE},
	{{1, 5}, APT_LOW_BUILDING},
	{{2, 5}, APT_APRON},
	{{3, 5}, APT_STAND},
	{{4, 5}, APT_BUILDING_2},
	{{5, 5}, APT_STAND},
	{{6, 5}, APT_APRON},
	{{7, 5}, APT_APRON},
	{{8, 5}, APT_APRON_FENCE_SW},
	{{0, 6}, APT_APRON_FENCE_NE},
	{{1, 6}, APT_APRON},
	{{2, 6}, APT_APRON},
	{{3, 6}, APT_STAND},
	{{4, 6}, APT_BUILDING_3},
	{{5, 6}, APT_STAND},
	{{6, 6}, APT_APRON},
	{{7, 6}, APT_APRON},
	{{8, 6}, APT_APRON_FENCE_SW},
	{{0, 7}, APT_APRON_FENCE_NE},
	{{1, 7}, APT_APRON_FENCE_SE},
	{{2, 7}, APT_APRON},
	{{3, 7}, APT_STAND},
	{{4, 7}, APT_ROUND_TERMINAL},
	{{5, 7}, APT_STAND},
	{{6, 7}, APT_APRON_FENCE_SW},
	{{7, 7}, APT_APRON_HALF_WEST},
	{{8, 7}, APT_APRON_FENCE_SW},
	{{0, 8}, APT_APRON_FENCE_NE},
	{{1, 8}, APT_GRASS_FENCE_NE_FLAG_2},
	{{2, 8}, APT_APRON_FENCE_NE},
	{{3, 8}, APT_APRON},
	{{4, 8}, APT_APRON},
	{{5, 8}, APT_APRON},
	{{6, 8}, APT_APRON_FENCE_SW},
	{{7, 8}, APT_EMPTY},
	{{8, 8}, APT_APRON_FENCE_NE_SW},
	{{0, 9}, APT_APRON_FENCE_NE},
	{{1, 9}, APT_RUNWAY_END_FENCE_NE_NW},
	{{2, 9}, APT_RUNWAY_FENCE_NW},
	{{3, 9}, APT_RUNWAY_FENCE_NW},
	{{4, 9}, APT_RUNWAY_FENCE_NW},
	{{5, 9}, APT_RUNWAY_FENCE_NW},
	{{6, 9}, APT_RUNWAY_FENCE_NW},
	{{7, 9}, APT_RUNWAY_FENCE_NW},
	{{8, 9}, APT_RUNWAY_END_FENCE_SE_SW},
	{{0, 10}, APT_RUNWAY_END_FENCE_NE_SE},
	{{1, 10}, APT_RUNWAY_2},
	{{2, 10}, APT_RUNWAY_2},
	{{3, 10}, APT_RUNWAY_2},
	{{4, 10}, APT_RUNWAY_2},
	{{5, 10}, APT_RUNWAY_2},
	{{6, 10}, APT_RUNWAY_2},
	{{7, 10}, APT_RUNWAY_END_FENCE_SE_SW},
	{{8, 10}, APT_EMPTY},
};

static const std::initializer_list<AirportTileLayout> _tile_table_intercontinental = {
	{ _tile_table_intercontinental_0, Direction::N },
};

/** Tiles for Heliport */
static const std::initializer_list<AirportTileTable> _tile_table_heliport_0 = {
	{{0, 0}, APT_HELIPORT},
};

static const std::initializer_list<AirportTileLayout> _tile_table_heliport = {
	{ _tile_table_heliport_0, Direction::N },
};

/** Tiles for Helidepot */
static const std::initializer_list<AirportTileTable> _tile_table_helidepot_0 = {
	{{0, 0}, APT_LOW_BUILDING_FENCE_N},
	{{1, 0}, APT_DEPOT_SE},
	{{0, 1}, APT_HELIPAD_2_FENCE_NE_SE},
	{{1, 1}, APT_APRON_FENCE_SE_SW},
};

static const std::initializer_list<AirportTileLayout> _tile_table_helidepot = {
	{ _tile_table_helidepot_0, Direction::N },
};

/** Tiles for Helistation */
static const std::initializer_list<AirportTileTable> _tile_table_helistation_0 = {
	{{0, 0}, APT_DEPOT_SE},
	{{1, 0}, APT_LOW_BUILDING_FENCE_NW},
	{{2, 0}, APT_HELIPAD_3_FENCE_NW},
	{{3, 0}, APT_HELIPAD_3_FENCE_NW_SW},
	{{0, 1}, APT_APRON_FENCE_NE_SE},
	{{1, 1}, APT_APRON_FENCE_SE},
	{{2, 1}, APT_APRON_FENCE_SE},
	{{3, 1}, APT_HELIPAD_3_FENCE_SE_SW},
};

static const std::initializer_list<AirportTileLayout> _tile_table_helistation = {
	{ _tile_table_helistation_0, Direction::N },
};

/** General AirportSpec definition. */
#define AS_GENERIC(fsm, layouts, depots, size_x, size_y, noise, catchment, min_year, max_year, maint_cost, ttdpatch_type, class_id, name, preview, enabled) \
	{{class_id, 0}, fsm, layouts, depots, size_x, size_y, noise, catchment, TimerGameCalendar::Year{min_year}, TimerGameCalendar::Year{max_year}, name, ttdpatch_type, preview, maint_cost, enabled, SubstituteGRFFileProps(AT_INVALID), {}}

/** AirportSpec definition for airports without any depot. */
#define AS_ND(ap_name, size_x, size_y, min_year, max_year, catchment, noise, maint_cost, ttdpatch_type, class_id, name, preview) \
	AS_GENERIC(&_airportfta_##ap_name, _tile_table_##ap_name, {}, \
		size_x, size_y, noise, catchment, min_year, max_year, maint_cost, ttdpatch_type, class_id, name, preview, true)

/** AirportSpec definition for airports with at least one depot. */
#define AS(ap_name, size_x, size_y, min_year, max_year, catchment, noise, maint_cost, ttdpatch_type, class_id, name, preview) \
	AS_GENERIC(&_airportfta_##ap_name, _tile_table_##ap_name, _airport_depots_##ap_name, \
		size_x, size_y, noise, catchment, min_year, max_year, maint_cost, ttdpatch_type, class_id, name, preview, true)

/** The helidepot and helistation have ATP_TTDP_SMALL because they are at ground level. */
extern const AirportSpec _origin_airport_specs[] = {
	AS(country,          4, 3,     0,     1959,  4,  3,  7, ATP_TTDP_SMALL,    APC_SMALL,    STR_AIRPORT_SMALL,            SPR_AIRPORT_PREVIEW_SMALL),
	AS(city,             6, 6,  1955, CalendarTime::MAX_YEAR,  5,  5, 24, ATP_TTDP_LARGE,    APC_LARGE,    STR_AIRPORT_CITY,             SPR_AIRPORT_PREVIEW_LARGE),
	AS_ND(heliport,      1, 1,  1963, CalendarTime::MAX_YEAR,  4,  1,  4, ATP_TTDP_HELIPORT, APC_HELIPORT, STR_AIRPORT_HELIPORT,         SPR_AIRPORT_PREVIEW_HELIPORT),
	AS(metropolitan,     6, 6,  1980, CalendarTime::MAX_YEAR,  6,  8, 28, ATP_TTDP_LARGE,    APC_LARGE,    STR_AIRPORT_METRO,            SPR_AIRPORT_PREVIEW_METROPOLITAN),
	AS(international,    7, 7,  1990, CalendarTime::MAX_YEAR,  8, 17, 42, ATP_TTDP_LARGE,    APC_HUB,      STR_AIRPORT_INTERNATIONAL,    SPR_AIRPORT_PREVIEW_INTERNATIONAL),
	AS(commuter,         5, 4,  1983, CalendarTime::MAX_YEAR,  4,  4, 20, ATP_TTDP_SMALL,    APC_SMALL,    STR_AIRPORT_COMMUTER,         SPR_AIRPORT_PREVIEW_COMMUTER),
	AS(helidepot,        2, 2,  1976, CalendarTime::MAX_YEAR,  4,  2,  7, ATP_TTDP_SMALL,    APC_HELIPORT, STR_AIRPORT_HELIDEPOT,        SPR_AIRPORT_PREVIEW_HELIDEPOT),
	AS(intercontinental, 9, 11, 2002, CalendarTime::MAX_YEAR, 10, 25, 72, ATP_TTDP_LARGE,    APC_HUB,      STR_AIRPORT_INTERCONTINENTAL, SPR_AIRPORT_PREVIEW_INTERCONTINENTAL),
	AS(helistation,      4, 2,  1980, CalendarTime::MAX_YEAR,  4,  3, 14, ATP_TTDP_SMALL,    APC_HELIPORT, STR_AIRPORT_HELISTATION,      SPR_AIRPORT_PREVIEW_HELISTATION),
	AS_GENERIC(&_airportfta_oilrig, {}, {}, 1, 1, 0, 4, 0, 0, 0, ATP_TTDP_OILRIG, APC_HELIPORT, STR_NULL, 0, false),
};

static_assert(NEW_AIRPORT_OFFSET == lengthof(_origin_airport_specs));

const AirportSpec AirportSpec::dummy = AS_GENERIC(&_airportfta_dummy, {}, {}, 0, 0, 0, 0, CalendarTime::MIN_YEAR, CalendarTime::MIN_YEAR, 0, ATP_TTDP_LARGE, {}, STR_NULL, 0, false);

#undef AS
#undef AS_ND
#undef AS_GENERIC

#endif /* AIRPORT_DEFAULTS_H */
