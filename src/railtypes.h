/* $Id$ */

/** @file railtypes.h
 * All the railtype-specific information is stored here.
 */

#ifndef RAILTYPES_H
#define RAILTYPES_H

/** Global Railtype definition
 */
RailtypeInfo _railtypes[] = {
	/** Railway */
	{ /* Main Sprites */
		{ SPR_RAIL_TRACK_Y, SPR_RAIL_TRACK_N_S, SPR_RAIL_TRACK_BASE, SPR_RAIL_SINGLE_Y, SPR_RAIL_SINGLE_X,
			SPR_RAIL_SINGLE_NORTH, SPR_RAIL_SINGLE_SOUTH, SPR_RAIL_SINGLE_EAST, SPR_RAIL_SINGLE_WEST,
			SPR_CROSSING_OFF_X_RAIL,
			SPR_TUNNEL_ENTRY_REAR_RAIL
		},

		/* GUI sprites */
		{ 0x4E3, 0x4E4, 0x4E5, 0x4E6,
			SPR_IMG_AUTORAIL,
			SPR_IMG_DEPOT_RAIL,
			SPR_IMG_TUNNEL_RAIL,
			SPR_IMG_CONVERT_RAIL
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
		{ STR_100A_RAILROAD_CONSTRUCTION },

		/* Offset of snow tiles */
		SPR_RAIL_SNOW_OFFSET,

		/* Powered railtypes */
		1 << RAILTYPE_RAIL | 1 << RAILTYPE_ELECTRIC,

		/* Compatible railtypes */
		1 << RAILTYPE_RAIL | 1 << RAILTYPE_ELECTRIC,

		/* main offset */
		0,

		/* bridge offset */
		0,

		/* custom ground offset */
		0,
	},

	/** Electrified railway */
	{ /* Main Sprites */
		{ SPR_RAIL_TRACK_Y, SPR_RAIL_TRACK_N_S, SPR_RAIL_TRACK_BASE, SPR_RAIL_SINGLE_Y, SPR_RAIL_SINGLE_X,
			SPR_RAIL_SINGLE_NORTH, SPR_RAIL_SINGLE_SOUTH, SPR_RAIL_SINGLE_EAST, SPR_RAIL_SINGLE_WEST,
			SPR_CROSSING_OFF_X_RAIL,
			SPR_TUNNEL_ENTRY_REAR_RAIL
		},

		/* GUI sprites */
		{
			SPR_BUILD_NS_ELRAIL,
			SPR_BUILD_X_ELRAIL,
			SPR_BUILD_EW_ELRAIL,
			SPR_BUILD_Y_ELRAIL,
			SPR_OPENTTD_BASE + 0,
			0x50E,
			SPR_BUILD_TUNNEL_ELRAIL,
			SPR_IMG_CONVERT_RAIL
		},

		{
			SPR_CURSOR_NS_ELRAIL,
			SPR_CURSOR_SWNE_ELRAIL,
			SPR_CURSOR_EW_ELRAIL,
			SPR_CURSOR_NWSE_ELRAIL,
			SPR_CURSOR_AUTORAIL,
			SPR_CURSOR_RAIL_DEPOT,
			SPR_CURSOR_TUNNEL_ELRAIL,
			SPR_CURSOR_CONVERT_RAIL
		},

		/* strings */
		{ STR_TITLE_ELRAIL_CONSTRUCTION },

		/* Offset of snow tiles */
		SPR_RAIL_SNOW_OFFSET,

		/* Powered railtypes */
		1 << RAILTYPE_ELECTRIC,

		/* Compatible railtypes */
		1 << RAILTYPE_ELECTRIC | 1 << RAILTYPE_RAIL,

		/* main offset */
		0,

		/* bridge offset */
		0,

		/* custom ground offset */
		0,
	},

	/** Monorail */
	{ /* Main Sprites */
		{ SPR_MONO_TRACK_Y, SPR_MONO_TRACK_N_S, SPR_MONO_TRACK_BASE, SPR_MONO_SINGLE_Y, SPR_MONO_SINGLE_X,
			SPR_MONO_SINGLE_NORTH, SPR_MONO_SINGLE_SOUTH, SPR_MONO_SINGLE_EAST, SPR_MONO_SINGLE_WEST,
			SPR_CROSSING_OFF_X_MONO,
			SPR_TUNNEL_ENTRY_REAR_MONO
		},

		/* GUI sprites */
		{ 0x4E7, 0x4E8, 0x4E9, 0x4EA,
			SPR_IMG_AUTOMONO,
			SPR_IMG_DEPOT_MONO,
			SPR_IMG_TUNNEL_MONO,
			SPR_IMG_CONVERT_MONO
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
		{ STR_100B_MONORAIL_CONSTRUCTION },

		/* Offset of snow tiles */
		SPR_MONO_SNOW_OFFSET,

		/* Powered railtypes */
		1 << RAILTYPE_MONO,

		/* Compatible Railtypes */
		1 << RAILTYPE_MONO,

		/* main offset */
		82,

		/* bridge offset */
		16,

		/* custom ground offset */
		1,
	},

	/** Maglev */
	{ /* Main sprites */
		{ SPR_MGLV_TRACK_Y, SPR_MGLV_TRACK_N_S, SPR_MGLV_TRACK_BASE, SPR_MGLV_SINGLE_Y, SPR_MGLV_SINGLE_X,
			SPR_MGLV_SINGLE_NORTH, SPR_MGLV_SINGLE_SOUTH, SPR_MGLV_SINGLE_EAST, SPR_MGLV_SINGLE_WEST,
			SPR_CROSSING_OFF_X_MAGLEV,
			SPR_TUNNEL_ENTRY_REAR_MAGLEV
		},

		/* GUI sprites */
		{ 0x4EB, 0x4EC, 0x4EE, 0x4ED,
			SPR_IMG_AUTOMAGLEV,
			SPR_IMG_DEPOT_MAGLEV,
			SPR_IMG_TUNNEL_MAGLEV,
			SPR_IMG_CONVERT_MAGLEV
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
		{ STR_100C_MAGLEV_CONSTRUCTION },

		/* Offset of snow tiles */
		SPR_MGLV_SNOW_OFFSET,

		/* Powered railtypes */
		1 << RAILTYPE_MAGLEV,

		/* Compatible Railtypes */
		1 << RAILTYPE_MAGLEV,

		/* main offset */
		164,

		/* bridge offset */
		24,

		/* custom ground offset */
		2,
	},
};

#endif /* RAILTYPES_H */
