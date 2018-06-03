/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file roadtypes.h
 * All the roadtype-specific information is stored here.
 */

#ifndef ROADTYPES_H
#define ROADTYPES_H

/**
 * Global Roadtype definition
 */
static const RoadtypeInfo _original_roadtypes[] = {
	/** Road */
	{
		/* GUI sprites */
		{
			SPR_IMG_ROAD_X_DIR,
			SPR_IMG_ROAD_Y_DIR,
			SPR_IMG_AUTOROAD,
			SPR_IMG_ROAD_DEPOT,
			SPR_IMG_ROAD_TUNNEL,
			SPR_IMG_CONVERT_ROAD,
		},

		{
			SPR_CURSOR_ROAD_NESW,
			SPR_CURSOR_ROAD_NWSE,
			SPR_CURSOR_AUTOROAD,
			SPR_CURSOR_ROAD_DEPOT,
			SPR_CURSOR_TUNNEL_RAIL,
			SPR_CURSOR_CONVERT_ROAD,
		},

		/* strings */
		{
			STR_ROAD_NAME_ROAD,
			STR_ROAD_TOOLBAR_ROAD_CONSTRUCTION_CAPTION,
			STR_ROAD_MENU_ROAD_CONSTRUCTION,
			STR_BUY_VEHICLE_ROAD_VEHICLE_CAPTION,
			STR_REPLACE_ROAD_VEHICLES,
			STR_ENGINE_PREVIEW_ROAD_VEHICLE,

			STR_ERROR_CAN_T_BUILD_ROAD_HERE,
			STR_ERROR_CAN_T_REMOVE_ROAD_FROM,
			STR_ERROR_CAN_T_BUILD_ROAD_DEPOT,
			{ STR_ERROR_CAN_T_BUILD_BUS_STATION,         STR_ERROR_CAN_T_BUILD_TRUCK_STATION },
			{ STR_ERROR_CAN_T_REMOVE_BUS_STATION,        STR_ERROR_CAN_T_REMOVE_TRUCK_STATION },
			STR_ERROR_CAN_T_CONVERT_ROAD,
			{ STR_STATION_BUILD_BUS_ORIENTATION,         STR_STATION_BUILD_TRUCK_ORIENTATION },
			{ STR_STATION_BUILD_BUS_ORIENTATION_TOOLTIP, STR_STATION_BUILD_TRUCK_ORIENTATION_TOOLTIP },
		},

		/* Powered roadtypes */
		ROADSUBTYPES_NORMAL | ROADSUBTYPES_ELECTRIC,

		/* flags */
		ROTFB_NONE,

		/* cost multiplier */
		8,

		/* maintenance cost multiplier */
		16,

		/* max speed */
		0,

		/* road type label */
		'ROAD',

		/* alternate labels */
		RoadTypeLabelList(),

		/* map colour */
		0x01,

		/* introduction date */
		INVALID_DATE,

		/* roadtypes required for this to be introduced */
		ROADSUBTYPES_NONE,

		/* introduction road types */
		ROADSUBTYPES_NORMAL,

		/* sort order */
		0x07,

		{ NULL },
		{ NULL },
	},
	/** Electrified Road */
	{
		/* GUI sprites */
		{
			SPR_IMG_ROAD_X_DIR,
			SPR_IMG_ROAD_Y_DIR,
			SPR_IMG_AUTOROAD,
			SPR_IMG_ROAD_DEPOT,
			SPR_IMG_ROAD_TUNNEL,
			SPR_IMG_CONVERT_ROAD,
		},

		{
			SPR_CURSOR_ROAD_NESW,
			SPR_CURSOR_ROAD_NWSE,
			SPR_CURSOR_AUTOROAD,
			SPR_CURSOR_ROAD_DEPOT,
			SPR_CURSOR_TUNNEL_RAIL,
			SPR_CURSOR_CONVERT_ROAD,
		},

		/* strings */
		{
			STR_ROAD_NAME_ELROAD,
			STR_ROAD_TOOLBAR_ELROAD_CONSTRUCTION_CAPTION,
			STR_ROAD_MENU_ELROAD_CONSTRUCTION,
			STR_BUY_VEHICLE_ELROAD_VEHICLE_CAPTION,
			STR_REPLACE_ELROAD_VEHICLES,
			STR_ENGINE_PREVIEW_ELROAD_VEHICLE,

			STR_ERROR_CAN_T_BUILD_ROAD_HERE,
			STR_ERROR_CAN_T_REMOVE_ROAD_FROM,
			STR_ERROR_CAN_T_BUILD_ROAD_DEPOT,
			{ STR_ERROR_CAN_T_BUILD_BUS_STATION,         STR_ERROR_CAN_T_BUILD_TRUCK_STATION },
			{ STR_ERROR_CAN_T_REMOVE_BUS_STATION,        STR_ERROR_CAN_T_REMOVE_TRUCK_STATION },
			STR_ERROR_CAN_T_CONVERT_ROAD,
			{ STR_STATION_BUILD_BUS_ORIENTATION,         STR_STATION_BUILD_TRUCK_ORIENTATION },
			{ STR_STATION_BUILD_BUS_ORIENTATION_TOOLTIP, STR_STATION_BUILD_TRUCK_ORIENTATION_TOOLTIP },
		},

		/* Powered roadtypes */
		ROADSUBTYPES_ELECTRIC,

		/* flags */
		ROTFB_CATENARY,

		/* cost multiplier */
		16,

		/* maintenance cost multiplier */
		24,

		/* max speed */
		0,

		/* road type label */
		'ELRD',

		/* alternate labels */
		RoadTypeLabelList(),

		/* map colour */
		0x01,

		/* introduction date */
		INVALID_DATE,

		/* roadtypes required for this to be introduced */
		ROADSUBTYPES_NONE,

		/* introduction road types */
		ROADSUBTYPES_ELECTRIC,

		/* sort order */
		0x17,

		{ NULL },
		{ NULL },
	},
};

static const RoadtypeInfo _original_tramtypes[] = {
	/** Tram */
	{
		/* GUI sprites */
		{
			SPR_IMG_TRAMWAY_X_DIR,
			SPR_IMG_TRAMWAY_Y_DIR,
			SPR_IMG_AUTOTRAM,
			SPR_IMG_ROAD_DEPOT,
			SPR_IMG_ROAD_TUNNEL,
			SPR_IMG_CONVERT_TRAM,
		},

		{
			SPR_CURSOR_TRAMWAY_NESW,
			SPR_CURSOR_TRAMWAY_NWSE,
			SPR_CURSOR_AUTOTRAM,
			SPR_CURSOR_ROAD_DEPOT,
			SPR_CURSOR_TUNNEL_RAIL,
			SPR_CURSOR_CONVERT_TRAM,
		},

		/* strings */
		{
			STR_ROAD_NAME_TRAM,
			STR_ROAD_TOOLBAR_TRAM_CONSTRUCTION_CAPTION,
			STR_ROAD_MENU_TRAM_CONSTRUCTION,
			STR_BUY_VEHICLE_TRAM_VEHICLE_CAPTION,
			STR_REPLACE_TRAM_VEHICLES,
			STR_ENGINE_PREVIEW_TRAM_VEHICLE,

			STR_ERROR_CAN_T_BUILD_TRAMWAY_HERE,
			STR_ERROR_CAN_T_REMOVE_TRAMWAY_FROM,
			STR_ERROR_CAN_T_BUILD_TRAM_DEPOT,
			{ STR_ERROR_CAN_T_BUILD_PASSENGER_TRAM_STATION,         STR_ERROR_CAN_T_BUILD_CARGO_TRAM_STATION },
			{ STR_ERROR_CAN_T_REMOVE_PASSENGER_TRAM_STATION,        STR_ERROR_CAN_T_REMOVE_CARGO_TRAM_STATION },
			STR_ERROR_CAN_T_CONVERT_TRAMWAY,
			{ STR_STATION_BUILD_PASSENGER_TRAM_ORIENTATION,         STR_STATION_BUILD_CARGO_TRAM_ORIENTATION },
			{ STR_STATION_BUILD_PASSENGER_TRAM_ORIENTATION_TOOLTIP, STR_STATION_BUILD_CARGO_TRAM_ORIENTATION_TOOLTIP },
		},

		/* Powered roadtypes */
		ROADSUBTYPES_NORMAL | ROADSUBTYPES_ELECTRIC,

		/* flags */
		ROTFB_NO_HOUSES,

		/* cost multiplier */
		8,

		/* maintenance cost multiplier */
		16,

		/* max speed */
		0,

		/* road type label */
		'RAIL',

		/* alternate labels */
		RoadTypeLabelList(),

		/* map colour */
		0x01,

		/* introduction date */
		INVALID_DATE,

		/* roadtypes required for this to be introduced */
		ROADSUBTYPES_NONE,

		/* introduction road types */
		ROADSUBTYPES_NORMAL,

		/* sort order */
		0x07,

		{ NULL },
		{ NULL },
	},
	/** Electrified Tram */
	{
		/* GUI sprites */
		{
			SPR_IMG_TRAMWAY_X_DIR,
			SPR_IMG_TRAMWAY_Y_DIR,
			SPR_IMG_AUTOTRAM,
			SPR_IMG_ROAD_DEPOT,
			SPR_IMG_ROAD_TUNNEL,
			SPR_IMG_CONVERT_TRAM,
		},

		{
			SPR_CURSOR_TRAMWAY_NESW,
			SPR_CURSOR_TRAMWAY_NWSE,
			SPR_CURSOR_AUTOTRAM,
			SPR_CURSOR_ROAD_DEPOT,
			SPR_CURSOR_TUNNEL_RAIL,
			SPR_CURSOR_CONVERT_TRAM,
		},

		/* strings */
		{
			STR_ROAD_NAME_ELTRAM,
			STR_ROAD_TOOLBAR_ELTRAM_CONSTRUCTION_CAPTION,
			STR_ROAD_MENU_ELTRAM_CONSTRUCTION,
			STR_BUY_VEHICLE_ELTRAM_VEHICLE_CAPTION,
			STR_REPLACE_ELTRAM_VEHICLES,
			STR_ENGINE_PREVIEW_ELTRAM_VEHICLE,

			STR_ERROR_CAN_T_BUILD_TRAMWAY_HERE,
			STR_ERROR_CAN_T_REMOVE_TRAMWAY_FROM,
			STR_ERROR_CAN_T_BUILD_TRAM_DEPOT,
			{ STR_ERROR_CAN_T_BUILD_PASSENGER_TRAM_STATION,         STR_ERROR_CAN_T_BUILD_CARGO_TRAM_STATION },
			{ STR_ERROR_CAN_T_REMOVE_PASSENGER_TRAM_STATION,        STR_ERROR_CAN_T_REMOVE_CARGO_TRAM_STATION },
			STR_ERROR_CAN_T_CONVERT_TRAMWAY,
			{ STR_STATION_BUILD_PASSENGER_TRAM_ORIENTATION,         STR_STATION_BUILD_CARGO_TRAM_ORIENTATION },
			{ STR_STATION_BUILD_PASSENGER_TRAM_ORIENTATION_TOOLTIP, STR_STATION_BUILD_CARGO_TRAM_ORIENTATION_TOOLTIP },
		},

		/* Powered roadtypes */
		ROADSUBTYPES_ELECTRIC,

		/* flags */
		ROTFB_CATENARY | ROTFB_NO_HOUSES,

		/* cost multiplier */
		16,

		/* maintenance cost multiplier */
		24,

		/* max speed */
		0,

		/* road type label */
		'ELRL',

		/* alternate labels */
		RoadTypeLabelList(),

		/* map colour */
		0x01,

		/* introduction date */
		INVALID_DATE,

		/* roadtypes required for this to be introduced */
		ROADSUBTYPES_NONE,

		/* introduction road types */
		ROADSUBTYPES_ELECTRIC,

		/* sort order */
		0x17,

		{ NULL },
		{ NULL },
	},
};

#endif /* ROADTYPES_H */
