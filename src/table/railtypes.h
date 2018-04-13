/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file railtypes.h
 * All the railtype-specific information is stored here.
 */

#ifndef RAILTYPES_H
#define RAILTYPES_H

/**
 * Global Railtype definition
 */
static const RailtypeInfo _original_railtypes[] = {
	/** Railway */
	{ // Main Sprites
		{ SPR_RAIL_TRACK_Y, SPR_RAIL_TRACK_N_S, SPR_RAIL_TRACK_BASE, SPR_RAIL_SINGLE_X, SPR_RAIL_SINGLE_Y,
			SPR_RAIL_SINGLE_NORTH, SPR_RAIL_SINGLE_SOUTH, SPR_RAIL_SINGLE_EAST, SPR_RAIL_SINGLE_WEST,
			SPR_TRACKS_FOR_SLOPES_RAIL_BASE,
			SPR_CROSSING_OFF_X_RAIL,
			SPR_TUNNEL_ENTRY_REAR_RAIL
		},

		/* GUI sprites */
		{ 0x4E3, 0x4E4, 0x4E5, 0x4E6,
			SPR_IMG_AUTORAIL,
			SPR_IMG_DEPOT_RAIL,
			SPR_IMG_TUNNEL_RAIL,
			SPR_IMG_CONVERT_RAIL,
			{}
		},

		{
			SPR_CURSOR_NS_TRACK,
			SPR_CURSOR_SWNE_TRACK,
			SPR_CURSOR_EW_TRACK,
			SPR_CURSOR_NWSE_TRACK,
			SPR_CURSOR_AUTORAIL,
			SPR_CURSOR_RAIL_DEPOT,
			SPR_CURSOR_TUNNEL_RAIL,
			SPR_CURSOR_CONVERT_RAIL
		},

		/* strings */
		{
			STR_RAIL_NAME_RAILROAD,
			STR_RAIL_TOOLBAR_RAILROAD_CONSTRUCTION_CAPTION,
			STR_RAIL_MENU_RAILROAD_CONSTRUCTION,
			STR_BUY_VEHICLE_TRAIN_RAIL_CAPTION,
			STR_REPLACE_RAIL_VEHICLES,
			STR_ENGINE_PREVIEW_RAILROAD_LOCOMOTIVE,
		},

		/* Offset of snow tiles */
		SPR_RAIL_SNOW_OFFSET,

		/* Powered railtypes */
		RAILTYPES_RAIL | RAILTYPES_ELECTRIC,

		/* Compatible railtypes */
		RAILTYPES_RAIL | RAILTYPES_ELECTRIC,

		/* bridge offset */
		0,

		/* fallback_railtype */
		0,

		/* curve speed advantage (multiplier) */
		0,

		/* flags */
		RTFB_NONE,

		/* cost multiplier */
		8,

		/* maintenance cost multiplier */
		8,

		/* acceleration type */
		0,

		/* max speed */
		0,

		/* rail type label */
		'RAIL',

		/* alternate labels */
		RailTypeLabelList(),

		/* map colour */
		0x0A,

		/* introduction date */
		INVALID_DATE,

		/* railtypes required for this to be introduced */
		RAILTYPES_NONE,

		/* introduction rail types */
		RAILTYPES_RAIL,

		/* sort order */
		0 << 4 | 7,

		{ NULL },
		{ NULL },
	},

	/** Electrified railway */
	{ // Main Sprites
		{ SPR_RAIL_TRACK_Y, SPR_RAIL_TRACK_N_S, SPR_RAIL_TRACK_BASE, SPR_RAIL_SINGLE_X, SPR_RAIL_SINGLE_Y,
			SPR_RAIL_SINGLE_NORTH, SPR_RAIL_SINGLE_SOUTH, SPR_RAIL_SINGLE_EAST, SPR_RAIL_SINGLE_WEST,
			SPR_TRACKS_FOR_SLOPES_RAIL_BASE,
			SPR_CROSSING_OFF_X_RAIL,
			SPR_TUNNEL_ENTRY_REAR_RAIL
		},

		/* GUI sprites */
		{
			SPR_BUILD_NS_ELRAIL,
			SPR_BUILD_X_ELRAIL,
			SPR_BUILD_EW_ELRAIL,
			SPR_BUILD_Y_ELRAIL,
			SPR_IMG_AUTOELRAIL,
			SPR_IMG_DEPOT_ELRAIL,
			SPR_BUILD_TUNNEL_ELRAIL,
			SPR_IMG_CONVERT_ELRAIL,
			{}
		},

		{
			SPR_CURSOR_NS_ELRAIL,
			SPR_CURSOR_SWNE_ELRAIL,
			SPR_CURSOR_EW_ELRAIL,
			SPR_CURSOR_NWSE_ELRAIL,
			SPR_CURSOR_AUTOELRAIL,
			SPR_CURSOR_ELRAIL_DEPOT,
			SPR_CURSOR_TUNNEL_ELRAIL,
			SPR_CURSOR_CONVERT_ELRAIL
		},

		/* strings */
		{
			STR_RAIL_NAME_ELRAIL,
			STR_RAIL_TOOLBAR_ELRAIL_CONSTRUCTION_CAPTION,
			STR_RAIL_MENU_ELRAIL_CONSTRUCTION,
			STR_BUY_VEHICLE_TRAIN_ELRAIL_CAPTION,
			STR_REPLACE_ELRAIL_VEHICLES,
			STR_ENGINE_PREVIEW_ELRAIL_LOCOMOTIVE,
		},

		/* Offset of snow tiles */
		SPR_RAIL_SNOW_OFFSET,

		/* Powered railtypes */
		RAILTYPES_ELECTRIC,

		/* Compatible railtypes */
		RAILTYPES_ELECTRIC | RAILTYPES_RAIL,

		/* bridge offset */
		0,

		/* fallback_railtype */
		0,

		/* curve speed advantage (multiplier) */
		0,

		/* flags */
		RTFB_CATENARY,

		/* cost multiplier */
		12,

		/* maintenance cost multiplier */
		12,

		/* acceleration type */
		0,

		/* max speed */
		0,

		/* rail type label */
		'ELRL',

		/* alternate labels */
		RailTypeLabelList(),

		/* map colour */
		0x0A,

		/* introduction date */
		INVALID_DATE,

		/* railtypes required for this to be introduced */
		RAILTYPES_NONE,

		/* introduction rail types */
		RAILTYPES_ELECTRIC,

		/* sort order */
		1 << 4 | 7,

		{ NULL },
		{ NULL },
	},

	/** Monorail */
	{ // Main Sprites
		{ SPR_MONO_TRACK_Y, SPR_MONO_TRACK_N_S, SPR_MONO_TRACK_BASE, SPR_MONO_SINGLE_X, SPR_MONO_SINGLE_Y,
			SPR_MONO_SINGLE_NORTH, SPR_MONO_SINGLE_SOUTH, SPR_MONO_SINGLE_EAST, SPR_MONO_SINGLE_WEST,
			SPR_TRACKS_FOR_SLOPES_MONO_BASE,
			SPR_CROSSING_OFF_X_MONO,
			SPR_TUNNEL_ENTRY_REAR_MONO
		},

		/* GUI sprites */
		{ 0x4E7, 0x4E8, 0x4E9, 0x4EA,
			SPR_IMG_AUTOMONO,
			SPR_IMG_DEPOT_MONO,
			SPR_IMG_TUNNEL_MONO,
			SPR_IMG_CONVERT_MONO,
			{}
		},

		{
			SPR_CURSOR_NS_MONO,
			SPR_CURSOR_SWNE_MONO,
			SPR_CURSOR_EW_MONO,
			SPR_CURSOR_NWSE_MONO,
			SPR_CURSOR_AUTOMONO,
			SPR_CURSOR_MONO_DEPOT,
			SPR_CURSOR_TUNNEL_MONO,
			SPR_CURSOR_CONVERT_MONO
		},

		/* strings */
		{
			STR_RAIL_NAME_MONORAIL,
			STR_RAIL_TOOLBAR_MONORAIL_CONSTRUCTION_CAPTION,
			STR_RAIL_MENU_MONORAIL_CONSTRUCTION,
			STR_BUY_VEHICLE_TRAIN_MONORAIL_CAPTION,
			STR_REPLACE_MONORAIL_VEHICLES,
			STR_ENGINE_PREVIEW_MONORAIL_LOCOMOTIVE,
		},

		/* Offset of snow tiles */
		SPR_MONO_SNOW_OFFSET,

		/* Powered railtypes */
		RAILTYPES_MONO,

		/* Compatible Railtypes */
		RAILTYPES_MONO,

		/* bridge offset */
		16,

		/* fallback_railtype */
		1,

		/* curve speed advantage (multiplier) */
		1,

		/* flags */
		RTFB_NONE,

		/* cost multiplier */
		16,

		/* maintenance cost multiplier */
		16,

		/* acceleration type */
		1,

		/* max speed */
		0,

		/* rail type label */
		'MONO',

		/* alternate labels */
		RailTypeLabelList(),

		/* map colour */
		0x0A,

		/* introduction date */
		INVALID_DATE,

		/* railtypes required for this to be introduced */
		RAILTYPES_NONE,

		/* introduction rail types */
		RAILTYPES_MONO,

		/* sort order */
		2 << 4 | 7,

		{ NULL },
		{ NULL },
	},

	/** Maglev */
	{ // Main sprites
		{ SPR_MGLV_TRACK_Y, SPR_MGLV_TRACK_N_S, SPR_MGLV_TRACK_BASE, SPR_MGLV_SINGLE_X, SPR_MGLV_SINGLE_Y,
			SPR_MGLV_SINGLE_NORTH, SPR_MGLV_SINGLE_SOUTH, SPR_MGLV_SINGLE_EAST, SPR_MGLV_SINGLE_WEST,
			SPR_TRACKS_FOR_SLOPES_MAGLEV_BASE,
			SPR_CROSSING_OFF_X_MAGLEV,
			SPR_TUNNEL_ENTRY_REAR_MAGLEV
		},

		/* GUI sprites */
		{ 0x4EB, 0x4EC, 0x4EE, 0x4ED,
			SPR_IMG_AUTOMAGLEV,
			SPR_IMG_DEPOT_MAGLEV,
			SPR_IMG_TUNNEL_MAGLEV,
			SPR_IMG_CONVERT_MAGLEV,
			{}
		},

		{
			SPR_CURSOR_NS_MAGLEV,
			SPR_CURSOR_SWNE_MAGLEV,
			SPR_CURSOR_EW_MAGLEV,
			SPR_CURSOR_NWSE_MAGLEV,
			SPR_CURSOR_AUTOMAGLEV,
			SPR_CURSOR_MAGLEV_DEPOT,
			SPR_CURSOR_TUNNEL_MAGLEV,
			SPR_CURSOR_CONVERT_MAGLEV
		},

		/* strings */
		{
			STR_RAIL_NAME_MAGLEV,
			STR_RAIL_TOOLBAR_MAGLEV_CONSTRUCTION_CAPTION,
			STR_RAIL_MENU_MAGLEV_CONSTRUCTION,
			STR_BUY_VEHICLE_TRAIN_MAGLEV_CAPTION,
			STR_REPLACE_MAGLEV_VEHICLES,
			STR_ENGINE_PREVIEW_MAGLEV_LOCOMOTIVE,
		},

		/* Offset of snow tiles */
		SPR_MGLV_SNOW_OFFSET,

		/* Powered railtypes */
		RAILTYPES_MAGLEV,

		/* Compatible Railtypes */
		RAILTYPES_MAGLEV,

		/* bridge offset */
		24,

		/* fallback_railtype */
		2,

		/* curve speed advantage (multiplier) */
		2,

		/* flags */
		RTFB_NONE,

		/* cost multiplier */
		24,

		/* maintenance cost multiplier */
		24,

		/* acceleration type */
		2,

		/* max speed */
		0,

		/* rail type label */
		'MGLV',

		/* alternate labels */
		RailTypeLabelList(),

		/* map colour */
		0x0A,

		/* introduction date */
		INVALID_DATE,

		/* railtypes required for this to be introduced */
		RAILTYPES_NONE,

		/* introduction rail types */
		RAILTYPES_MAGLEV,

		/* sort order */
		3 << 4 | 7,

		{ NULL },
		{ NULL },
	},
};

#endif /* RAILTYPES_H */
