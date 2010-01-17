/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file airport_defaults.h Tables with default values for airports and airport tiles. */

#ifndef AIRPORT_DEFAULTS_H
#define AIRPORT_DEFAULTS_H

/**
 * Definition of an airport tiles layout.
 * @param x offset x of this tile
 * @param y offset y of this tile
 * @param m AirportGfx of the tile
 * @see _airport_specs
 * @see AirportTileTable
 */
#define MK(x, y, m) {{x, y}, m}

/**
 * Terminator of airport tiles layout definition
 */
#define MKEND {{-0x80, 0}, 0}

/** Tiles for Country Airfield (small) */
static AirportTileTable _tile_table_country_0[] = {
	MK(0, 0, APT_SMALL_BUILDING_1),
	MK(1, 0, APT_SMALL_BUILDING_2),
	MK(2, 0, APT_SMALL_BUILDING_3),
	MK(3, 0, APT_SMALL_DEPOT_SE),
	MK(0, 1, APT_GRASS_FENCE_NE_FLAG),
	MK(1, 1, APT_GRASS_1),
	MK(2, 1, APT_GRASS_2),
	MK(3, 1, APT_GRASS_FENCE_SW),
	MK(0, 2, APT_RUNWAY_SMALL_FAR_END),
	MK(1, 2, APT_RUNWAY_SMALL_MIDDLE),
	MK(2, 2, APT_RUNWAY_SMALL_MIDDLE),
	MK(3, 2, APT_RUNWAY_SMALL_NEAR_END),
	MKEND
};

static AirportTileTable *_tile_table_country[] = {
	_tile_table_country_0,
};

/** Tiles for Commuter Airfield (small) */
static AirportTileTable _tile_table_commuter_0[] = {
	MK(0, 0, APT_TOWER),
	MK(1, 0, APT_BUILDING_3),
	MK(2, 0, APT_HELIPAD_2_FENCE_NW),
	MK(3, 0, APT_HELIPAD_2_FENCE_NW),
	MK(4, 0, APT_DEPOT_SE),
	MK(0, 1, APT_APRON_FENCE_NE),
	MK(1, 1, APT_APRON),
	MK(2, 1, APT_APRON),
	MK(3, 1, APT_APRON),
	MK(4, 1, APT_APRON_FENCE_SW),
	MK(0, 2, APT_APRON_FENCE_NE),
	MK(1, 2, APT_STAND),
	MK(2, 2, APT_STAND),
	MK(3, 2, APT_STAND),
	MK(4, 2, APT_APRON_FENCE_SW),
	MK(0, 3, APT_RUNWAY_END_FENCE_SE),
	MK(1, 3, APT_RUNWAY_2),
	MK(2, 3, APT_RUNWAY_2),
	MK(3, 3, APT_RUNWAY_2),
	MK(4, 3, APT_RUNWAY_END_FENCE_SE),
	MKEND
};

static AirportTileTable *_tile_table_commuter[] = {
	_tile_table_commuter_0,
};

/** Tiles for City Airport (large) */
static AirportTileTable _tile_table_city_0[] = {
	MK(0, 0, APT_BUILDING_1),
	MK(1, 0, APT_APRON_FENCE_NW),
	MK(2, 0, APT_STAND_1),
	MK(3, 0, APT_APRON_FENCE_NW),
	MK(4, 0, APT_APRON_FENCE_NW),
	MK(5, 0, APT_DEPOT_SE),
	MK(0, 1, APT_BUILDING_2),
	MK(1, 1, APT_PIER),
	MK(2, 1, APT_ROUND_TERMINAL),
	MK(3, 1, APT_STAND_PIER_NE),
	MK(4, 1, APT_APRON),
	MK(5, 1, APT_APRON_FENCE_SW),
	MK(0, 2, APT_BUILDING_3),
	MK(1, 2, APT_STAND),
	MK(2, 2, APT_PIER_NW_NE),
	MK(3, 2, APT_APRON_S),
	MK(4, 2, APT_APRON_HOR),
	MK(5, 2, APT_APRON_N_FENCE_SW),
	MK(0, 3, APT_RADIO_TOWER_FENCE_NE),
	MK(1, 3, APT_APRON_W),
	MK(2, 3, APT_APRON_VER_CROSSING_S),
	MK(3, 3, APT_APRON_HOR_CROSSING_E),
	MK(4, 3, APT_ARPON_N),
	MK(5, 3, APT_TOWER_FENCE_SW),
	MK(0, 4, APT_EMPTY_FENCE_NE),
	MK(1, 4, APT_APRON_S),
	MK(2, 4, APT_APRON_HOR_CROSSING_W),
	MK(3, 4, APT_APRON_VER_CROSSING_N),
	MK(4, 4, APT_APRON_E),
	MK(5, 4, APT_RADAR_GRASS_FENCE_SW),
	MK(0, 5, APT_RUNWAY_END_FENCE_SE),
	MK(1, 5, APT_RUNWAY_1),
	MK(2, 5, APT_RUNWAY_2),
	MK(3, 5, APT_RUNWAY_3),
	MK(4, 5, APT_RUNWAY_4),
	MK(5, 5, APT_RUNWAY_END_FENCE_SE),
	MKEND
};

static AirportTileTable *_tile_table_city[] = {
	_tile_table_city_0,
};

/** Tiles for Metropolitain Airport (large) - 2 runways */
static AirportTileTable _tile_table_metropolitan_0[] = {
	MK(0, 0, APT_BUILDING_1),
	MK(1, 0, APT_APRON_FENCE_NW),
	MK(2, 0, APT_STAND_1),
	MK(3, 0, APT_APRON_FENCE_NW),
	MK(4, 0, APT_APRON_FENCE_NW),
	MK(5, 0, APT_DEPOT_SE),
	MK(0, 1, APT_BUILDING_2),
	MK(1, 1, APT_PIER),
	MK(2, 1, APT_ROUND_TERMINAL),
	MK(3, 1, APT_STAND_PIER_NE),
	MK(4, 1, APT_APRON),
	MK(5, 1, APT_APRON_FENCE_SW),
	MK(0, 2, APT_BUILDING_3),
	MK(1, 2, APT_STAND),
	MK(2, 2, APT_PIER_NW_NE),
	MK(3, 2, APT_APRON_S),
	MK(4, 2, APT_APRON_HOR),
	MK(5, 2, APT_APRON_N_FENCE_SW),
	MK(0, 3, APT_RADAR_FENCE_NE),
	MK(1, 3, APT_APRON),
	MK(2, 3, APT_APRON),
	MK(3, 3, APT_APRON),
	MK(4, 3, APT_APRON),
	MK(5, 3, APT_TOWER_FENCE_SW),
	MK(0, 4, APT_RUNWAY_END),
	MK(1, 4, APT_RUNWAY_5),
	MK(2, 4, APT_RUNWAY_5),
	MK(3, 4, APT_RUNWAY_5),
	MK(4, 4, APT_RUNWAY_5),
	MK(5, 4, APT_RUNWAY_END),
	MK(0, 5, APT_RUNWAY_END_FENCE_SE),
	MK(1, 5, APT_RUNWAY_2),
	MK(2, 5, APT_RUNWAY_2),
	MK(3, 5, APT_RUNWAY_2),
	MK(4, 5, APT_RUNWAY_2),
	MK(5, 5, APT_RUNWAY_END_FENCE_SE),
	MKEND
};

static AirportTileTable *_tile_table_metropolitan[] = {
	_tile_table_metropolitan_0,
};

/** Tiles for International Airport (large) - 2 runways */
static AirportTileTable _tile_table_international_0[] = {
	MK(0, 0, APT_RUNWAY_END_FENCE_NW),
	MK(1, 0, APT_RUNWAY_FENCE_NW),
	MK(2, 0, APT_RUNWAY_FENCE_NW),
	MK(3, 0, APT_RUNWAY_FENCE_NW),
	MK(4, 0, APT_RUNWAY_FENCE_NW),
	MK(5, 0, APT_RUNWAY_FENCE_NW),
	MK(6, 0, APT_RUNWAY_END_FENCE_NW),
	MK(0, 1, APT_RADIO_TOWER_FENCE_NE),
	MK(1, 1, APT_APRON),
	MK(2, 1, APT_APRON),
	MK(3, 1, APT_APRON),
	MK(4, 1, APT_APRON),
	MK(5, 1, APT_APRON),
	MK(6, 1, APT_DEPOT_SE),
	MK(0, 2, APT_BUILDING_3),
	MK(1, 2, APT_APRON),
	MK(2, 2, APT_STAND),
	MK(3, 2, APT_BUILDING_2),
	MK(4, 2, APT_STAND),
	MK(5, 2, APT_APRON),
	MK(6, 2, APT_APRON_FENCE_SW),
	MK(0, 3, APT_DEPOT_SE),
	MK(1, 3, APT_APRON),
	MK(2, 3, APT_STAND),
	MK(3, 3, APT_BUILDING_2),
	MK(4, 3, APT_STAND),
	MK(5, 3, APT_APRON),
	MK(6, 3, APT_HELIPAD_1),
	MK(0, 4, APT_APRON_FENCE_NE),
	MK(1, 4, APT_APRON),
	MK(2, 4, APT_STAND),
	MK(3, 4, APT_TOWER),
	MK(4, 4, APT_STAND),
	MK(5, 4, APT_APRON),
	MK(6, 4, APT_HELIPAD_1),
	MK(0, 5, APT_APRON_FENCE_NE),
	MK(1, 5, APT_APRON),
	MK(2, 5, APT_APRON),
	MK(3, 5, APT_APRON),
	MK(4, 5, APT_APRON),
	MK(5, 5, APT_APRON),
	MK(6, 5, APT_RADAR_FENCE_SW),
	MK(0, 6, APT_RUNWAY_END_FENCE_SE),
	MK(1, 6, APT_RUNWAY_2),
	MK(2, 6, APT_RUNWAY_2),
	MK(3, 6, APT_RUNWAY_2),
	MK(4, 6, APT_RUNWAY_2),
	MK(5, 6, APT_RUNWAY_2),
	MK(6, 6, APT_RUNWAY_END_FENCE_SE),
	MKEND
};

static AirportTileTable *_tile_table_international[] = {
	_tile_table_international_0,
};

/** Tiles for International Airport (large) - 2 runways */
static AirportTileTable _tile_table_intercontinental_0[] = {
	MK(0, 0, APT_RADAR_FENCE_NE),
	MK(1, 0, APT_RUNWAY_END_FENCE_NE_NW),
	MK(2, 0, APT_RUNWAY_FENCE_NW),
	MK(3, 0, APT_RUNWAY_FENCE_NW),
	MK(4, 0, APT_RUNWAY_FENCE_NW),
	MK(5, 0, APT_RUNWAY_FENCE_NW),
	MK(6, 0, APT_RUNWAY_FENCE_NW),
	MK(7, 0, APT_RUNWAY_FENCE_NW),
	MK(8, 0, APT_RUNWAY_END_FENCE_NW_SW),
	MK(0, 1, APT_RUNWAY_END_FENCE_NE_NW),
	MK(1, 1, APT_RUNWAY_2),
	MK(2, 1, APT_RUNWAY_2),
	MK(3, 1, APT_RUNWAY_2),
	MK(4, 1, APT_RUNWAY_2),
	MK(5, 1, APT_RUNWAY_2),
	MK(6, 1, APT_RUNWAY_2),
	MK(7, 1, APT_RUNWAY_END_FENCE_SE_SW),
	MK(8, 1, APT_APRON_FENCE_NE_SW),
	MK(0, 2, APT_APRON_FENCE_NE),
	MK(1, 2, APT_SMALL_BUILDING_1),
	MK(2, 2, APT_APRON_FENCE_NE),
	MK(3, 2, APT_APRON),
	MK(4, 2, APT_APRON),
	MK(5, 2, APT_APRON),
	MK(6, 2, APT_APRON),
	MK(7, 2, APT_RADIO_TOWER_FENCE_NE),
	MK(8, 2, APT_APRON_FENCE_NE_SW),
	MK(0, 3, APT_APRON_FENCE_NE),
	MK(1, 3, APT_APRON_HALF_EAST),
	MK(2, 3, APT_APRON_FENCE_NE),
	MK(3, 3, APT_TOWER),
	MK(4, 3, APT_HELIPAD_2),
	MK(5, 3, APT_HELIPAD_2),
	MK(6, 3, APT_APRON),
	MK(7, 3, APT_APRON_FENCE_NW),
	MK(8, 3, APT_APRON_FENCE_SW),
	MK(0, 4, APT_APRON_FENCE_NE),
	MK(1, 4, APT_APRON),
	MK(2, 4, APT_APRON),
	MK(3, 4, APT_STAND),
	MK(4, 4, APT_BUILDING_1),
	MK(5, 4, APT_STAND),
	MK(6, 4, APT_APRON),
	MK(7, 4, APT_LOW_BUILDING),
	MK(8, 4, APT_DEPOT_SE),
	MK(0, 5, APT_DEPOT_SE),
	MK(1, 5, APT_LOW_BUILDING),
	MK(2, 5, APT_APRON),
	MK(3, 5, APT_STAND),
	MK(4, 5, APT_BUILDING_2),
	MK(5, 5, APT_STAND),
	MK(6, 5, APT_APRON),
	MK(7, 5, APT_APRON),
	MK(8, 5, APT_APRON_FENCE_SW),
	MK(0, 6, APT_APRON_FENCE_NE),
	MK(1, 6, APT_APRON),
	MK(2, 6, APT_APRON),
	MK(3, 6, APT_STAND),
	MK(4, 6, APT_BUILDING_3),
	MK(5, 6, APT_STAND),
	MK(6, 6, APT_APRON),
	MK(7, 6, APT_APRON),
	MK(8, 6, APT_APRON_FENCE_SW),
	MK(0, 7, APT_APRON_FENCE_NE),
	MK(1, 7, APT_APRON_FENCE_SE),
	MK(2, 7, APT_APRON),
	MK(3, 7, APT_STAND),
	MK(4, 7, APT_ROUND_TERMINAL),
	MK(5, 7, APT_STAND),
	MK(6, 7, APT_APRON_FENCE_SW),
	MK(7, 7, APT_APRON_HALF_WEST),
	MK(8, 7, APT_APRON_FENCE_SW),
	MK(0, 8, APT_APRON_FENCE_NE),
	MK(1, 8, APT_GRASS_FENCE_NE_FLAG_2),
	MK(2, 8, APT_APRON_FENCE_NE),
	MK(3, 8, APT_APRON),
	MK(4, 8, APT_APRON),
	MK(5, 8, APT_APRON),
	MK(6, 8, APT_APRON_FENCE_SW),
	MK(7, 8, APT_EMPTY),
	MK(8, 8, APT_APRON_FENCE_NE_SW),
	MK(0, 9, APT_APRON_FENCE_NE),
	MK(1, 9, APT_RUNWAY_END_FENCE_NE_NW),
	MK(2, 9, APT_RUNWAY_FENCE_NW),
	MK(3, 9, APT_RUNWAY_FENCE_NW),
	MK(4, 9, APT_RUNWAY_FENCE_NW),
	MK(5, 9, APT_RUNWAY_FENCE_NW),
	MK(6, 9, APT_RUNWAY_FENCE_NW),
	MK(7, 9, APT_RUNWAY_FENCE_NW),
	MK(8, 9, APT_RUNWAY_END_FENCE_SE_SW),
	MK(0, 10, APT_RUNWAY_END_FENCE_NE_SE),
	MK(1, 10, APT_RUNWAY_2),
	MK(2, 10, APT_RUNWAY_2),
	MK(3, 10, APT_RUNWAY_2),
	MK(4, 10, APT_RUNWAY_2),
	MK(5, 10, APT_RUNWAY_2),
	MK(6, 10, APT_RUNWAY_2),
	MK(7, 10, APT_RUNWAY_END_FENCE_SE_SW),
	MK(8, 10, APT_EMPTY),
	MKEND
};

static AirportTileTable *_tile_table_intercontinental[] = {
	_tile_table_intercontinental_0,
};

/** Tiles for Heliport */
static AirportTileTable _tile_table_heliport_0[] = {
	MK(0, 0, APT_HELIPORT),
	MKEND
};

static AirportTileTable *_tile_table_heliport[] = {
	_tile_table_heliport_0,
};

/** Tiles for Helidepot */
static AirportTileTable _tile_table_helidepot_0[] = {
	MK(0, 0, APT_LOW_BUILDING_FENCE_N),
	MK(1, 0, APT_DEPOT_SE),
	MK(0, 1, APT_HELIPAD_2_FENCE_NE_SE),
	MK(1, 1, APT_APRON_FENCE_SE_SW),
	MKEND
};

static AirportTileTable *_tile_table_helidepot[] = {
	_tile_table_helidepot_0,
};

/** Tiles for Helistation */
static AirportTileTable _tile_table_helistation_0[] = {
	MK(0, 0, APT_DEPOT_SE),
	MK(1, 0, APT_LOW_BUILDING_FENCE_NW),
	MK(2, 0, APT_HELIPAD_3_FENCE_NW),
	MK(3, 0, APT_HELIPAD_3_FENCE_NW_SW),
	MK(0, 1, APT_APRON_FENCE_NE_SE),
	MK(1, 1, APT_APRON_FENCE_SE),
	MK(2, 1, APT_APRON_FENCE_SE),
	MK(3, 1, APT_HELIPAD_3_FENCE_SE_SW),
	MKEND
};

static AirportTileTable *_tile_table_helistation[] = {
	_tile_table_helistation_0,
};

#undef MK
#undef MKEND

/** General AirportSpec definition. */
#define AS_GENERIC(att, depot_tbl, num_depots, size_x, size_y, noise, catchment, min_year, max_year) \
	{att, depot_tbl, num_depots, size_x, size_y, noise, catchment, min_year, max_year}

/** AirportSpec definition for airports without any depot. */
#define AS_ND(ap_name, size_x, size_y, min_year, max_year, catchment, noise) \
	AS_GENERIC(_tile_table_##ap_name, NULL, 0, size_x, size_y, noise, catchment, min_year, max_year)

/** AirportSpec definition for airports with at least one depot. */
#define AS(ap_name, size_x, size_y, min_year, max_year, catchment, noise) \
	AS_GENERIC(_tile_table_##ap_name, _airport_depots_##ap_name, lengthof(_airport_depots_##ap_name), size_x, size_y, noise, catchment, min_year, max_year)

static const AirportSpec _origin_airport_specs[] = {
	AS(country, 4, 3, 0, 1959, 4, 3),
	AS(city, 6, 6, 1960, MAX_YEAR, 5, 5),
	AS_ND(heliport, 1, 1, 1963, MAX_YEAR, 4, 1),
	AS(metropolitan, 6, 6, 1980, MAX_YEAR, 6, 8),
	AS(international, 7, 7, 1990, MAX_YEAR, 8, 17),
	AS(commuter, 5, 4, 1983, MAX_YEAR, 4, 4),
	AS(helidepot, 2, 2, 1976, MAX_YEAR, 4, 2),
	AS(intercontinental, 9, 11, 2002, MAX_YEAR, 10, 25),
	AS(helistation, 4, 2, 1980, MAX_YEAR, 4, 3),
};

assert_compile(NUM_AIRPORTS == lengthof(_origin_airport_specs));

#undef AS
#undef AS_ND
#undef AS_GENERIC

#endif /* AIRPORT_DEFAULTS_H */
