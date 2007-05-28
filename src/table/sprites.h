/* $Id$ */

#ifndef SPRITES_H
#define SPRITES_H

/** @file sprites.h
 * This file contails all sprite-related enums and defines. These consist mainly of
 * the sprite numbers and a bunch of masks and macros to handle sprites and to get
 * rid of all the magic numbers in the code.
 *
 * @NOTE:
 * ALL SPRITE NUMBERS BELOW 5126 are in the main files
 * SPR_CANALS_BASE is in canalsw.grf
 * SPR_SLOPES_BASE is in trkfoundw.grf
 * SPR_OPENTTD_BASE is in openttd.grf
 *
 * All elements which consist of two elements should
 * have the same name and then suffixes
 *   _GROUND and _BUILD for building-type sprites
 *   _REAR and _FRONT for transport-type sprites (tiles where vehicles are on)
 * These sprites are split because of the Z order of the elements
 *  (like some parts of a bridge are behind the vehicle, while others are before)
 *
 *
 * All sprites which are described here are referenced only one to a handful of times
 * throughout the code. When introducing new sprite enums, use meaningful names.
 * Don't be lazy and typing, and only use abbrevations when their meaning is clear or
 * the length of the enum would get out of hand. In that case EXPLAIN THE ABBREVATION
 * IN THIS FILE, and perhaps add some comments in the code where it is used.
 * Now, don't whine about this being too much typing work if the enums are like
 * 30 characters in length. If your editor doen't help you simplifying your work,
 * get a proper editor. If your Operating Systems don't have any decent editors,
 * get a proper Operating System.
 *
 * @todo Split the "Sprites" enum into smaller chunks and document them
 */

enum Sprites {
	SPR_SELECT_TILE  = 752,
	SPR_DOT          = 774, // corner marker for lower/raise land
	SPR_DOT_SMALL    = 4078,
	SPR_WHITE_POINT  = 4079,

	/* ASCII */
	SPR_ASCII_SPACE       = 2,
	SPR_ASCII_SPACE_SMALL = 226,
	SPR_ASCII_SPACE_BIG   = 450,

	/* Extra graphic spritenumbers */
	OPENTTD_SPRITES_COUNT = 116, // number of gfx-sprites in openttd.grf
	SPR_SIGNALS_BASE  = 4896,
	SPR_CANALS_BASE   = SPR_SIGNALS_BASE + 486,
	SPR_SLOPES_BASE   = SPR_CANALS_BASE + 70,
	SPR_AUTORAIL_BASE = SPR_SLOPES_BASE + 78,
	SPR_ELRAIL_BASE   = SPR_AUTORAIL_BASE + 55,
	SPR_2CCMAP_BASE   = SPR_ELRAIL_BASE + 53,
	SPR_OPENTTD_BASE  = SPR_2CCMAP_BASE + 256,

	SPR_BLOT = SPR_OPENTTD_BASE + 29, // colored circle (mainly used as vehicle profit marker and for sever compatibility)

	SPR_PIN_UP        = SPR_OPENTTD_BASE + 55,   // pin icon
	SPR_PIN_DOWN      = SPR_OPENTTD_BASE + 56,
	SPR_BOX_EMPTY     = SPR_OPENTTD_BASE + 59,
	SPR_BOX_CHECKED   = SPR_OPENTTD_BASE + 60,
	SPR_WINDOW_RESIZE = SPR_OPENTTD_BASE + 86,   // resize icon
	SPR_HOUSE_ICON    = SPR_OPENTTD_BASE + 93,
	// arrow icons pointing in all 4 directions
	SPR_ARROW_DOWN    = SPR_OPENTTD_BASE + 87,
	SPR_ARROW_UP      = SPR_OPENTTD_BASE + 88,
	SPR_ARROW_LEFT    = SPR_OPENTTD_BASE + 89,
	SPR_ARROW_RIGHT   = SPR_OPENTTD_BASE + 90,

	SPR_LARGE_SMALL_WINDOW = 682,

	/* Clone vehicles stuff */
	SPR_CLONE_TRAIN    = SPR_OPENTTD_BASE + 91,
	SPR_CLONE_ROADVEH  = SPR_OPENTTD_BASE + 108,
	SPR_CLONE_SHIP     = SPR_OPENTTD_BASE + 110,
	SPR_CLONE_AIRCRAFT = SPR_OPENTTD_BASE + 112,

	SPR_SELL_TRAIN        = SPR_OPENTTD_BASE + 95,
	SPR_SELL_ROADVEH      = SPR_OPENTTD_BASE + 96,
	SPR_SELL_SHIP         = SPR_OPENTTD_BASE + 97,
	SPR_SELL_AIRCRAFT     = SPR_OPENTTD_BASE + 98,
	SPR_SELL_ALL_TRAIN    = SPR_OPENTTD_BASE + 99,
	SPR_SELL_ALL_ROADVEH  = SPR_OPENTTD_BASE + 100,
	SPR_SELL_ALL_SHIP     = SPR_OPENTTD_BASE + 101,
	SPR_SELL_ALL_AIRCRAFT = SPR_OPENTTD_BASE + 102,
	SPR_REPLACE_TRAIN     = SPR_OPENTTD_BASE + 103,
	SPR_REPLACE_ROADVEH   = SPR_OPENTTD_BASE + 104,
	SPR_REPLACE_SHIP      = SPR_OPENTTD_BASE + 105,
	SPR_REPLACE_AIRCRAFT  = SPR_OPENTTD_BASE + 106,
	SPR_SELL_CHAIN_TRAIN  = SPR_OPENTTD_BASE + 107,

	SPR_SHARED_ORDERS_ICON = SPR_OPENTTD_BASE + 114,

	SPR_WARNING_SIGN      = SPR_OPENTTD_BASE + 115, // warning sign (shown if there are any newgrf errors)

	/* Network GUI sprites */
	SPR_SQUARE = SPR_OPENTTD_BASE + 20,     // colored square (used for newgrf compatibility)
	SPR_LOCK = SPR_OPENTTD_BASE + 19,       // lock icon (for password protected servers)
	SPR_FLAGS_BASE = SPR_OPENTTD_BASE + 82, // start of the flags block (in same order as enum NetworkLanguage)

	SPR_AIRPORTX_BASE = SPR_OPENTTD_BASE + OPENTTD_SPRITES_COUNT, // The sprites used for other airport angles
	SPR_NEWAIRPORT_TARMAC = SPR_AIRPORTX_BASE,
	SPR_NSRUNWAY1 = SPR_AIRPORTX_BASE + 1,
	SPR_NSRUNWAY2 = SPR_AIRPORTX_BASE + 2,
	SPR_NSRUNWAY3 = SPR_AIRPORTX_BASE + 3,
	SPR_NSRUNWAY4 = SPR_AIRPORTX_BASE + 4,
	SPR_NSRUNWAY_END = SPR_AIRPORTX_BASE + 5,
	SPR_NEWHANGAR_S = SPR_AIRPORTX_BASE + 6,
	SPR_NEWHANGAR_S_WALL = SPR_AIRPORTX_BASE + 7,
	SPR_NEWHANGAR_W = SPR_AIRPORTX_BASE + 8,
	SPR_NEWHANGAR_W_WALL = SPR_AIRPORTX_BASE + 9,
	SPR_NEWHANGAR_N = SPR_AIRPORTX_BASE + 10,
	SPR_NEWHANGAR_E = SPR_AIRPORTX_BASE + 11,
	SPR_NEWHELIPAD = SPR_AIRPORTX_BASE + 12,
	SPR_GRASS_RIGHT = SPR_AIRPORTX_BASE + 13,
	SPR_GRASS_LEFT = SPR_AIRPORTX_BASE + 14,

	SPR_ROADSTOP_BASE = SPR_AIRPORTX_BASE + 15, // The sprites used for drive-through road stops
	SPR_BUS_STOP_DT_Y_W = SPR_ROADSTOP_BASE,
	SPR_BUS_STOP_DT_Y_E = SPR_ROADSTOP_BASE + 1,
	SPR_BUS_STOP_DT_X_W = SPR_ROADSTOP_BASE + 2,
	SPR_BUS_STOP_DT_X_E = SPR_ROADSTOP_BASE + 3,
	SPR_TRUCK_STOP_DT_Y_W = SPR_ROADSTOP_BASE + 4,
	SPR_TRUCK_STOP_DT_Y_E = SPR_ROADSTOP_BASE + 5,
	SPR_TRUCK_STOP_DT_X_W = SPR_ROADSTOP_BASE + 6,
	SPR_TRUCK_STOP_DT_X_E = SPR_ROADSTOP_BASE + 7,

	SPR_GROUP_BASE                 = SPR_ROADSTOP_BASE + 8, // The sprites used for the group interface
	SPR_GROUP_CREATE_TRAIN         = SPR_GROUP_BASE,
	SPR_GROUP_CREATE_ROADVEH       = SPR_GROUP_BASE + 1,
	SPR_GROUP_CREATE_SHIP          = SPR_GROUP_BASE + 2,
	SPR_GROUP_CREATE_AIRCRAFT      = SPR_GROUP_BASE + 3,
	SPR_GROUP_DELETE_TRAIN         = SPR_GROUP_BASE + 4,
	SPR_GROUP_DELETE_ROADVEH       = SPR_GROUP_BASE + 5,
	SPR_GROUP_DELETE_SHIP          = SPR_GROUP_BASE + 6,
	SPR_GROUP_DELETE_AIRCRAFT      = SPR_GROUP_BASE + 7,
	SPR_GROUP_RENAME_TRAIN         = SPR_GROUP_BASE + 8,
	SPR_GROUP_RENAME_ROADVEH       = SPR_GROUP_BASE + 9,
	SPR_GROUP_RENAME_SHIP          = SPR_GROUP_BASE + 10,
	SPR_GROUP_RENAME_AIRCRAFT      = SPR_GROUP_BASE + 11,
	SPR_GROUP_REPLACE_ON_TRAIN     = SPR_GROUP_BASE + 12,
	SPR_GROUP_REPLACE_ON_ROADVEH   = SPR_GROUP_BASE + 13,
	SPR_GROUP_REPLACE_ON_SHIP      = SPR_GROUP_BASE + 14,
	SPR_GROUP_REPLACE_ON_AIRCRAFT  = SPR_GROUP_BASE + 15,
	SPR_GROUP_REPLACE_OFF_TRAIN    = SPR_GROUP_BASE + 16,
	SPR_GROUP_REPLACE_OFF_ROADVEH  = SPR_GROUP_BASE + 17,
	SPR_GROUP_REPLACE_OFF_SHIP     = SPR_GROUP_BASE + 18,
	SPR_GROUP_REPLACE_OFF_AIRCRAFT = SPR_GROUP_BASE + 19,

	/* Tramway sprites */
	SPR_TRAMWAY_BASE                 = SPR_GROUP_BASE + 20,
	SPR_TRAMWAY_OVERLAY              = SPR_TRAMWAY_BASE + 4,
	SPR_TRAMWAY_TRAM                 = SPR_TRAMWAY_BASE + 27,
	SPR_TRAMWAY_SLOPED_OFFSET        = 11,
	SPR_TRAMWAY_BUS_STOP_DT_Y_W      = SPR_TRAMWAY_BASE + 25,
	SPR_TRAMWAY_BUS_STOP_DT_Y_E      = SPR_TRAMWAY_BASE + 23,
	SPR_TRAMWAY_BUS_STOP_DT_X_W      = SPR_TRAMWAY_BASE + 24,
	SPR_TRAMWAY_BUS_STOP_DT_X_E      = SPR_TRAMWAY_BASE + 26,
	SPR_TRAMWAY_PAVED_STRAIGHT_Y     = SPR_TRAMWAY_BASE + 46,
	SPR_TRAMWAY_PAVED_STRAIGHT_X     = SPR_TRAMWAY_BASE + 47,
	SPR_TRAMWAY_BACK_WIRES_STRAIGHT  = SPR_TRAMWAY_BASE + 55,
	SPR_TRAMWAY_FRONT_WIRES_STRAIGHT = SPR_TRAMWAY_BASE + 56,
	SPR_TRAMWAY_BACK_WIRES_SLOPED    = SPR_TRAMWAY_BASE + 72,
	SPR_TRAMWAY_FRONT_WIRES_SLOPED   = SPR_TRAMWAY_BASE + 68,
	SPR_TRAMWAY_TUNNEL_WIRES         = SPR_TRAMWAY_BASE + 80,
	SPR_TRAMWAY_BRIDGE               = SPR_TRAMWAY_BASE + 107,

	/* Manager face sprites */
	SPR_GRADIENT = 874, // background gradient behind manager face

	/* Icon showing player colour. */
	SPR_PLAYER_ICON = 747,

	/* is itself no foundation sprite, because tileh 0 has no foundation */
	SPR_FOUNDATION_BASE = 989,

	/* Shadow cell */
	SPR_SHADOW_CELL = 1004,

	/* Sliced view shadow cells */
	/* Maybe we have different ones in the future */
	SPR_MAX_SLICE = SPR_OPENTTD_BASE + 64,
	SPR_MIN_SLICE = SPR_OPENTTD_BASE + 64,

	/* Unmovables spritenumbers */
	SPR_UNMOVABLE_TRANSMITTER   = 2601,
	SPR_UNMOVABLE_LIGHTHOUSE    = 2602,
	SPR_TINYHQ_NORTH            = 2603,
	SPR_TINYHQ_EAST             = 2604,
	SPR_TINYHQ_WEST             = 2605,
	SPR_TINYHQ_SOUTH            = 2606,
	SPR_SMALLHQ_NORTH           = 2607,
	SPR_SMALLHQ_EAST            = 2608,
	SPR_SMALLHQ_WEST            = 2609,
	SPR_SMALLHQ_SOUTH           = 2610,
	SPR_MEDIUMHQ_NORTH          = 2611,
	SPR_MEDIUMHQ_NORTH_WALL     = 2612,
	SPR_MEDIUMHQ_EAST           = 2613,
	SPR_MEDIUMHQ_EAST_WALL      = 2614,
	SPR_MEDIUMHQ_WEST           = 2615,
	SPR_MEDIUMHQ_WEST_WALL      = 2616, //very tiny piece of wall
	SPR_MEDIUMHQ_SOUTH          = 2617,
	SPR_LARGEHQ_NORTH_GROUND    = 2618,
	SPR_LARGEHQ_NORTH_BUILD     = 2619,
	SPR_LARGEHQ_EAST_GROUND     = 2620,
	SPR_LARGEHQ_EAST_BUILD      = 2621,
	SPR_LARGEHQ_WEST_GROUND     = 2622,
	SPR_LARGEHQ_WEST_BUILD      = 2623,
	SPR_LARGEHQ_SOUTH           = 2624,
	SPR_HUGEHQ_NORTH_GROUND     = 2625,
	SPR_HUGEHQ_NORTH_BUILD      = 2626,
	SPR_HUGEHQ_EAST_GROUND      = 2627,
	SPR_HUGEHQ_EAST_BUILD       = 2628,
	SPR_HUGEHQ_WEST_GROUND      = 2629,
	SPR_HUGEHQ_WEST_BUILD       = 2630,
	SPR_HUGEHQ_SOUTH            = 2631,
	SPR_CONCRETE_GROUND         = 1420,
	SPR_STATUE_COMPANY          = 2632,
	SPR_BOUGHT_LAND             = 4790,

	/* sprites for rail and rail stations*/
	SPR_RAIL_SNOW_OFFSET        = 26,
	SPR_MONO_SNOW_OFFSET        = 26,
	SPR_MGLV_SNOW_OFFSET        = 26,

	SPR_RAIL_SINGLE_Y           = 1005,
	SPR_RAIL_SINGLE_X           = 1006,
	SPR_RAIL_SINGLE_NORTH       = 1007,
	SPR_RAIL_SINGLE_SOUTH       = 1008,
	SPR_RAIL_SINGLE_EAST        = 1009,
	SPR_RAIL_SINGLE_WEST        = 1010,
	SPR_RAIL_TRACK_Y            = 1011,
	SPR_RAIL_TRACK_X            = 1012,
	SPR_RAIL_TRACK_BASE         = 1018,
	SPR_RAIL_TRACK_N_S          = 1035,
	SPR_RAIL_TRACK_Y_SNOW       = 1037,
	SPR_RAIL_TRACK_X_SNOW       = 1038,
	SPR_RAIL_DEPOT_SE_1         = 1063,
	SPR_RAIL_DEPOT_SE_2         = 1064,
	SPR_RAIL_DEPOT_SW_1         = 1065,
	SPR_RAIL_DEPOT_SW_2         = 1066,
	SPR_RAIL_DEPOT_NE           = 1067,
	SPR_RAIL_DEPOT_NW           = 1068,
	SPR_RAIL_PLATFORM_Y_FRONT         = 1069,
	SPR_RAIL_PLATFORM_X_REAR          = 1070,
	SPR_RAIL_PLATFORM_Y_REAR          = 1071,
	SPR_RAIL_PLATFORM_X_FRONT         = 1072,
	SPR_RAIL_PLATFORM_BUILDING_X      = 1073,
	SPR_RAIL_PLATFORM_BUILDING_Y      = 1074,
	SPR_RAIL_PLATFORM_PILLARS_Y_FRONT = 1075,
	SPR_RAIL_PLATFORM_PILLARS_X_REAR  = 1076,
	SPR_RAIL_PLATFORM_PILLARS_Y_REAR  = 1077,
	SPR_RAIL_PLATFORM_PILLARS_X_FRONT = 1078,
	SPR_RAIL_ROOF_STRUCTURE_X_TILE_A  = 1079, //First half of the roof structure
	SPR_RAIL_ROOF_STRUCTURE_Y_TILE_A  = 1080,
	SPR_RAIL_ROOF_STRUCTURE_X_TILE_B  = 1081, //Second half of the roof structure
	SPR_RAIL_ROOF_STRUCTURE_Y_TILE_B  = 1082,
	SPR_RAIL_ROOF_GLASS_X_TILE_A      = 1083, //First half of the roof glass
	SPR_RAIL_ROOF_GLASS_Y_TILE_A      = 1084,
	SPR_RAIL_ROOF_GLASS_X_TILE_B      = 1085, //second half of the roof glass
	SPR_RAIL_ROOF_GLASS_Y_TILE_B      = 1086,
	SPR_MONO_SINGLE_Y                 = 1087,
	SPR_MONO_SINGLE_X                 = 1088,
	SPR_MONO_SINGLE_NORTH             = 1089,
	SPR_MONO_SINGLE_SOUTH             = 1090,
	SPR_MONO_SINGLE_EAST              = 1091,
	SPR_MONO_SINGLE_WEST              = 1092,
	SPR_MONO_TRACK_Y                  = 1093,
	SPR_MONO_TRACK_BASE               = 1100,
	SPR_MONO_TRACK_N_S                = 1117,
	SPR_MGLV_SINGLE_Y                 = 1169,
	SPR_MGLV_SINGLE_X                 = 1170,
	SPR_MGLV_SINGLE_NORTH             = 1171,
	SPR_MGLV_SINGLE_SOUTH             = 1172,
	SPR_MGLV_SINGLE_EAST              = 1173,
	SPR_MGLV_SINGLE_WEST              = 1174,
	SPR_MGLV_TRACK_Y                  = 1175,
	SPR_MGLV_TRACK_BASE               = 1182,
	SPR_MGLV_TRACK_N_S                = 1199,
	SPR_WAYPOINT_X_1            = SPR_OPENTTD_BASE + 15,
	SPR_WAYPOINT_X_2            = SPR_OPENTTD_BASE + 16,
	SPR_WAYPOINT_Y_1            = SPR_OPENTTD_BASE + 17,
	SPR_WAYPOINT_Y_2            = SPR_OPENTTD_BASE + 18,
	OFFSET_TILEH_IMPOSSIBLE     = 0,
	OFFSET_TILEH_1              = 14,
	OFFSET_TILEH_2              = 15,
	OFFSET_TILEH_3              = 22,
	OFFSET_TILEH_4              = 13,
	OFFSET_TILEH_6              = 21,
	OFFSET_TILEH_7              = 17,
	OFFSET_TILEH_8              = 12,
	OFFSET_TILEH_9              = 23,
	OFFSET_TILEH_11             = 18,
	OFFSET_TILEH_12             = 20,
	OFFSET_TILEH_13             = 19,
	OFFSET_TILEH_14             = 16,

	/* Elrail stuff */
	/* Wires. First identifier is the direction of the track, second is the required placement of the pylon.
	 * "short" denotes a wire that requires a pylon on each end. Third identifier is the direction of the slope
	 * (in positive coordinate direction) */
	SPR_WIRE_X_SHORT = SPR_ELRAIL_BASE + 3,
	SPR_WIRE_Y_SHORT = SPR_ELRAIL_BASE + 4,
	SPR_WIRE_EW_SHORT = SPR_ELRAIL_BASE + 5,
	SPR_WIRE_NS_SHORT = SPR_ELRAIL_BASE + 6,
	SPR_WIRE_X_SHORT_DOWN = SPR_ELRAIL_BASE + 7,
	SPR_WIRE_Y_SHORT_UP = SPR_ELRAIL_BASE + 8,
	SPR_WIRE_X_SHORT_UP = SPR_ELRAIL_BASE + 9,
	SPR_WIRE_Y_SHORT_DOWN = SPR_ELRAIL_BASE + 10,

	SPR_WIRE_X_SW = SPR_ELRAIL_BASE + 11,
	SPR_WIRE_Y_SE = SPR_ELRAIL_BASE + 12,
	SPR_WIRE_EW_E = SPR_ELRAIL_BASE + 13,
	SPR_WIRE_NS_S = SPR_ELRAIL_BASE + 14,
	SPR_WIRE_X_SW_DOWN = SPR_ELRAIL_BASE + 15,
	SPR_WIRE_Y_SE_UP = SPR_ELRAIL_BASE + 16,
	SPR_WIRE_X_SW_UP = SPR_ELRAIL_BASE + 17,
	SPR_WIRE_Y_SE_DOWN = SPR_ELRAIL_BASE + 18,

	SPR_WIRE_X_NE = SPR_ELRAIL_BASE + 19,
	SPR_WIRE_Y_NW = SPR_ELRAIL_BASE + 20,
	SPR_WIRE_EW_W = SPR_ELRAIL_BASE + 21,
	SPR_WIRE_NS_N = SPR_ELRAIL_BASE + 22,
	SPR_WIRE_X_NE_DOWN = SPR_ELRAIL_BASE + 23,
	SPR_WIRE_Y_NW_UP = SPR_ELRAIL_BASE + 24,
	SPR_WIRE_X_NE_UP = SPR_ELRAIL_BASE + 25,
	SPR_WIRE_Y_NW_DOWN = SPR_ELRAIL_BASE + 26,

	/* Tunnel entries */
	SPR_WIRE_TUNNEL_NE = SPR_ELRAIL_BASE + 27,
	SPR_WIRE_TUNNEL_SE = SPR_ELRAIL_BASE + 28,
	SPR_WIRE_TUNNEL_SW = SPR_ELRAIL_BASE + 29,
	SPR_WIRE_TUNNEL_NW = SPR_ELRAIL_BASE + 30,

	/* Depot entries */
	SPR_WIRE_DEPOT_SW = SPR_ELRAIL_BASE + 27,
	SPR_WIRE_DEPOT_NW = SPR_ELRAIL_BASE + 28,
	SPR_WIRE_DEPOT_NE = SPR_ELRAIL_BASE + 29,
	SPR_WIRE_DEPOT_SE = SPR_ELRAIL_BASE + 30,


	/* Pylons, first identifier is the direction of the track, second the placement relative to the track */
	SPR_PYLON_Y_NE = SPR_ELRAIL_BASE + 31,
	SPR_PYLON_Y_SW = SPR_ELRAIL_BASE + 32,
	SPR_PYLON_X_NW = SPR_ELRAIL_BASE + 33,
	SPR_PYLON_X_SE = SPR_ELRAIL_BASE + 34,
	SPR_PYLON_EW_N = SPR_ELRAIL_BASE + 35,
	SPR_PYLON_EW_S = SPR_ELRAIL_BASE + 36,
	SPR_PYLON_NS_W = SPR_ELRAIL_BASE + 37,
	SPR_PYLON_NS_E = SPR_ELRAIL_BASE + 38,

	/* sprites for roads */
	SPR_ROAD_PAVED_STRAIGHT_Y       = 1313,
	SPR_ROAD_PAVED_STRAIGHT_X       = 1314,

	/* sprites for airports and airfields*/
	/* Small airports are AIRFIELD, everything else is AIRPORT */
	SPR_HELIPORT                    = 2633,
	SPR_AIRPORT_APRON               = 2634,
	SPR_AIRPORT_AIRCRAFT_STAND      = 2635,
	SPR_AIRPORT_TAXIWAY_NS_WEST     = 2636,
	SPR_AIRPORT_TAXIWAY_EW_SOUTH    = 2637,
	SPR_AIRPORT_TAXIWAY_XING_SOUTH  = 2638,
	SPR_AIRPORT_TAXIWAY_XING_WEST   = 2639,
	SPR_AIRPORT_TAXIWAY_NS_CTR      = 2640,
	SPR_AIRPORT_TAXIWAY_XING_EAST   = 2641,
	SPR_AIRPORT_TAXIWAY_NS_EAST     = 2642,
	SPR_AIRPORT_TAXIWAY_EW_NORTH    = 2643,
	SPR_AIRPORT_TAXIWAY_EW_CTR      = 2644,
	SPR_AIRPORT_RUNWAY_EXIT_A       = 2645,
	SPR_AIRPORT_RUNWAY_EXIT_B       = 2646,
	SPR_AIRPORT_RUNWAY_EXIT_C       = 2647,
	SPR_AIRPORT_RUNWAY_EXIT_D       = 2648,
	SPR_AIRPORT_RUNWAY_END          = 2649, //We should have different ends
	SPR_AIRPORT_TERMINAL_A          = 2650,
	SPR_AIRPORT_TOWER               = 2651,
	SPR_AIRPORT_CONCOURSE           = 2652,
	SPR_AIRPORT_TERMINAL_B          = 2653,
	SPR_AIRPORT_TERMINAL_C          = 2654,
	SPR_AIRPORT_HANGAR_FRONT        = 2655,
	SPR_AIRPORT_HANGAR_REAR         = 2656,
	SPR_AIRFIELD_HANGAR_FRONT       = 2657,
	SPR_AIRFIELD_HANGAR_REAR        = 2658,
	SPR_AIRPORT_JETWAY_1            = 2659,
	SPR_AIRPORT_JETWAY_2            = 2660,
	SPR_AIRPORT_JETWAY_3            = 2661,
	SPR_AIRPORT_PASSENGER_TUNNEL    = 2662,
	SPR_AIRPORT_FENCE_Y             = 2663,
	SPR_AIRPORT_FENCE_X             = 2664,
	SPR_AIRFIELD_TERM_A             = 2665,
	SPR_AIRFIELD_TERM_B             = 2666,
	SPR_AIRFIELD_TERM_C_GROUND      = 2667,
	SPR_AIRFIELD_TERM_C_BUILD       = 2668,
	SPR_AIRFIELD_APRON_A            = 2669,
	SPR_AIRFIELD_APRON_B            = 2670,
	SPR_AIRFIELD_APRON_C            = 2671,
	SPR_AIRFIELD_APRON_D            = 2672,
	SPR_AIRFIELD_RUNWAY_NEAR_END    = 2673,
	SPR_AIRFIELD_RUNWAY_MIDDLE      = 2674,
	SPR_AIRFIELD_RUNWAY_FAR_END     = 2675,
	SPR_AIRFIELD_WIND_1             = 2676,
	SPR_AIRFIELD_WIND_2             = 2677,
	SPR_AIRFIELD_WIND_3             = 2678,
	SPR_AIRFIELD_WIND_4             = 2679,
	SPR_AIRPORT_RADAR_1             = 2680,
	SPR_AIRPORT_RADAR_2             = 2681,
	SPR_AIRPORT_RADAR_3             = 2682,
	SPR_AIRPORT_RADAR_4             = 2683,
	SPR_AIRPORT_RADAR_5             = 2684,
	SPR_AIRPORT_RADAR_6             = 2685,
	SPR_AIRPORT_RADAR_7             = 2686,
	SPR_AIRPORT_RADAR_8             = 2687,
	SPR_AIRPORT_RADAR_9             = 2688,
	SPR_AIRPORT_RADAR_A             = 2689,
	SPR_AIRPORT_RADAR_B             = 2690,
	SPR_AIRPORT_RADAR_C             = 2691,
	SPR_AIRPORT_HELIPAD             = SPR_OPENTTD_BASE + 28,
	SPR_AIRPORT_HELIDEPOT_OFFICE    = 2095,

	/* Road Stops */
	/* Road stops have a ground tile and 3 buildings, one on each side
	 * (except the side where the entry is). These are marked _A _B and _C */
	SPR_BUS_STOP_NE_GROUND          = 2692,
	SPR_BUS_STOP_SE_GROUND          = 2693,
	SPR_BUS_STOP_SW_GROUND          = 2694,
	SPR_BUS_STOP_NW_GROUND          = 2695,
	SPR_BUS_STOP_NE_BUILD_A         = 2696,
	SPR_BUS_STOP_SE_BUILD_A         = 2697,
	SPR_BUS_STOP_SW_BUILD_A         = 2698,
	SPR_BUS_STOP_NW_BUILD_A         = 2699,
	SPR_BUS_STOP_NE_BUILD_B         = 2700,
	SPR_BUS_STOP_SE_BUILD_B         = 2701,
	SPR_BUS_STOP_SW_BUILD_B         = 2702,
	SPR_BUS_STOP_NW_BUILD_B         = 2703,
	SPR_BUS_STOP_NE_BUILD_C         = 2704,
	SPR_BUS_STOP_SE_BUILD_C         = 2705,
	SPR_BUS_STOP_SW_BUILD_C         = 2706,
	SPR_BUS_STOP_NW_BUILD_C         = 2707,
	SPR_TRUCK_STOP_NE_GROUND        = 2708,
	SPR_TRUCK_STOP_SE_GROUND        = 2709,
	SPR_TRUCK_STOP_SW_GROUND        = 2710,
	SPR_TRUCK_STOP_NW_GROUND        = 2711,
	SPR_TRUCK_STOP_NE_BUILD_A       = 2712,
	SPR_TRUCK_STOP_SE_BUILD_A       = 2713,
	SPR_TRUCK_STOP_SW_BUILD_A       = 2714,
	SPR_TRUCK_STOP_NW_BUILD_A       = 2715,
	SPR_TRUCK_STOP_NE_BUILD_B       = 2716,
	SPR_TRUCK_STOP_SE_BUILD_B       = 2717,
	SPR_TRUCK_STOP_SW_BUILD_B       = 2718,
	SPR_TRUCK_STOP_NW_BUILD_B       = 2719,
	SPR_TRUCK_STOP_NE_BUILD_C       = 2720,
	SPR_TRUCK_STOP_SE_BUILD_C       = 2721,
	SPR_TRUCK_STOP_SW_BUILD_C       = 2722,
	SPR_TRUCK_STOP_NW_BUILD_C       = 2723,

	/* Sprites for docks */
	/* Docks consist of two tiles, the sloped one and the flat one */
	SPR_DOCK_SLOPE_NE               = 2727,
	SPR_DOCK_SLOPE_SE               = 2728,
	SPR_DOCK_SLOPE_SW               = 2729,
	SPR_DOCK_SLOPE_NW               = 2730,
	SPR_DOCK_FLAT_X                 = 2731, //for NE and SW
	SPR_DOCK_FLAT_Y                 = 2732, //for NW and SE
	SPR_BUOY                        = 4076, //XXX this sucks, because it displays wrong stuff on canals

	/* Sprites for road */
	SPR_ROAD_Y                  = 1332,
	SPR_ROAD_X                  = 1333,
	SPR_ROAD_Y_SNOW             = 1351,
	SPR_ROAD_X_SNOW             = 1352,

	SPR_EXCAVATION_X = 1414,
	SPR_EXCAVATION_Y = 1415,

	/* Landscape sprites */
	SPR_FLAT_BARE_LAND          = 3924,
	SPR_FLAT_1_THIRD_GRASS_TILE = 3943,
	SPR_FLAT_2_THIRD_GRASS_TILE = 3962,
	SPR_FLAT_GRASS_TILE         = 3981,
	SPR_FLAT_ROUGH_LAND         = 4000,
	SPR_FLAT_ROUGH_LAND_1       = 4019,
	SPR_FLAT_ROUGH_LAND_2       = 4020,
	SPR_FLAT_ROUGH_LAND_3       = 4021,
	SPR_FLAT_ROUGH_LAND_4       = 4022,
	SPR_FLAT_ROCKY_LAND_1       = 4023,
	SPR_FLAT_ROCKY_LAND_2       = 4042,
	SPR_FLAT_WATER_TILE         = 4061,
	SPR_FLAT_1_QUART_SNOWY_TILE = 4493,
	SPR_FLAT_2_QUART_SNOWY_TILE = 4512,
	SPR_FLAT_3_QUART_SNOWY_TILE = 4531,
	SPR_FLAT_SNOWY_TILE         = 4550,

	/* Hedge, Farmland-fence sprites */
	SPR_HEDGE_BUSHES            = 4090,
	SPR_HEDGE_BUSHES_WITH_GATE  = 4096,
	SPR_HEDGE_FENCE             = 4102,
	SPR_HEDGE_BLOOMBUSH_YELLOW  = 4108,
	SPR_HEDGE_BLOOMBUSH_RED     = 4114,
	SPR_HEDGE_STONE             = 4120,

	/* Farmland sprites, only flat tiles listed, various stages */
	SPR_FARMLAND_BARE           = 4126,
	SPR_FARMLAND_STATE_1        = 4145,
	SPR_FARMLAND_STATE_2        = 4164,
	SPR_FARMLAND_STATE_3        = 4183,
	SPR_FARMLAND_STATE_4        = 4202,
	SPR_FARMLAND_STATE_5        = 4221,
	SPR_FARMLAND_STATE_6        = 4240,
	SPR_FARMLAND_STATE_7        = 4259,
	SPR_FARMLAND_HAYPACKS       = 4278,

	/* Shores */
	SPR_NO_SHORE                = 0,  //used for tileh which have no shore
	SPR_SHORE_TILEH_4           = 4062,
	SPR_SHORE_TILEH_1           = 4063,
	SPR_SHORE_TILEH_2           = 4064,
	SPR_SHORE_TILEH_8           = 4065,
	SPR_SHORE_TILEH_6           = 4066,
	SPR_SHORE_TILEH_12          = 4067,
	SPR_SHORE_TILEH_3           = 4068,
	SPR_SHORE_TILEH_9           = 4069,

	/* Water-related sprites */
	SPR_SHIP_DEPOT_SE_FRONT     = 4070,
	SPR_SHIP_DEPOT_SW_FRONT     = 4071,
	SPR_SHIP_DEPOT_NW           = 4072,
	SPR_SHIP_DEPOT_NE           = 4073,
	SPR_SHIP_DEPOT_SE_REAR      = 4074,
	SPR_SHIP_DEPOT_SW_REAR      = 4075,
	//here come sloped water sprites
	SPR_WATER_SLOPE_Y_UP        = SPR_CANALS_BASE + 5, //Water flowing negative Y direction
	SPR_WATER_SLOPE_X_DOWN      = SPR_CANALS_BASE + 6, //positive X
	SPR_WATER_SLOPE_X_UP        = SPR_CANALS_BASE + 7, //negative X
	SPR_WATER_SLOPE_Y_DOWN      = SPR_CANALS_BASE + 8,  //positive Y
	//sprites for the shiplifts
	//there are 4 kinds of shiplifts, each of them is 3 tiles long.
	//the four kinds are running in the X and Y direction and
	//are "lowering" either in the "+" or the "-" direction.
	//the three tiles are the center tile (where the slope is)
	//and a bottom and a top tile
	SPR_SHIPLIFT_Y_UP_CENTER_REAR     = SPR_CANALS_BASE + 9,
	SPR_SHIPLIFT_X_DOWN_CENTER_REAR   = SPR_CANALS_BASE + 10,
	SPR_SHIPLIFT_X_UP_CENTER_REAR     = SPR_CANALS_BASE + 11,
	SPR_SHIPLIFT_Y_DOWN_CENTER_REAR   = SPR_CANALS_BASE + 12,
	SPR_SHIPLIFT_Y_UP_CENTER_FRONT    = SPR_CANALS_BASE + 13,
	SPR_SHIPLIFT_X_DOWN_CENTER_FRONT  = SPR_CANALS_BASE + 14,
	SPR_SHIPLIFT_X_UP_CENTER_FRONT    = SPR_CANALS_BASE + 15,
	SPR_SHIPLIFT_Y_DOWN_CENTER_FRONT  = SPR_CANALS_BASE + 16,
	SPR_SHIPLIFT_Y_UP_BOTTOM_REAR     = SPR_CANALS_BASE + 17,
	SPR_SHIPLIFT_X_DOWN_BOTTOM_REAR   = SPR_CANALS_BASE + 18,
	SPR_SHIPLIFT_X_UP_BOTTOM_REAR     = SPR_CANALS_BASE + 19,
	SPR_SHIPLIFT_Y_DOWN_BOTTOM_REAR   = SPR_CANALS_BASE + 20,
	SPR_SHIPLIFT_Y_UP_BOTTOM_FRONT    = SPR_CANALS_BASE + 21,
	SPR_SHIPLIFT_X_DOWN_BOTTOM_FRONT  = SPR_CANALS_BASE + 22,
	SPR_SHIPLIFT_X_UP_BOTTOM_FRONT    = SPR_CANALS_BASE + 23,
	SPR_SHIPLIFT_Y_DOWN_BOTTOM_FRONT  = SPR_CANALS_BASE + 24,
	SPR_SHIPLIFT_Y_UP_TOP_REAR        = SPR_CANALS_BASE + 25,
	SPR_SHIPLIFT_X_DOWN_TOP_REAR      = SPR_CANALS_BASE + 26,
	SPR_SHIPLIFT_X_UP_TOP_REAR        = SPR_CANALS_BASE + 27,
	SPR_SHIPLIFT_Y_DOWN_TOP_REAR      = SPR_CANALS_BASE + 28,
	SPR_SHIPLIFT_Y_UP_TOP_FRONT       = SPR_CANALS_BASE + 29,
	SPR_SHIPLIFT_X_DOWN_TOP_FRONT     = SPR_CANALS_BASE + 30,
	SPR_SHIPLIFT_X_UP_TOP_FRONT       = SPR_CANALS_BASE + 31,
	SPR_SHIPLIFT_Y_DOWN_TOP_FRONT     = SPR_CANALS_BASE + 32,

	/* Sprites for tunnels and bridges */
	SPR_TUNNEL_ENTRY_REAR_RAIL   = 2365,
	SPR_TUNNEL_ENTRY_REAR_MONO   = 2373,
	SPR_TUNNEL_ENTRY_REAR_MAGLEV = 2381,
	SPR_TUNNEL_ENTRY_REAR_ROAD   = 2389,

	/* Level crossings */
	SPR_CROSSING_OFF_X_RAIL   = 1370,
	SPR_CROSSING_OFF_X_MONO   = 1382,
	SPR_CROSSING_OFF_X_MAGLEV = 1394,

	/* bridge type sprites */
	SPR_PILLARS_BASE = SPR_OPENTTD_BASE + 30,

	/* Wooden bridge (type 0) */
	SPR_BTWDN_RAIL_Y_REAR       = 2545,
	SPR_BTWDN_RAIL_X_REAR       = 2546,
	SPR_BTWDN_ROAD_Y_REAR       = 2547,
	SPR_BTWDN_ROAD_X_REAR       = 2548,
	SPR_BTWDN_Y_FRONT           = 2549,
	SPR_BTWDN_X_FRONT           = 2550,
	SPR_BTWDN_Y_PILLAR          = 2551,
	SPR_BTWDN_X_PILLAR          = 2552,
	SPR_BTWDN_MONO_Y_REAR       = 4360,
	SPR_BTWDN_MONO_X_REAR       = 4361,
	SPR_BTWDN_MGLV_Y_REAR       = 4400,
	SPR_BTWDN_MGLV_X_REAR       = 4401,
	/* ramps */
	SPR_BTWDN_ROAD_RAMP_Y_DOWN  = 2529,
	SPR_BTWDN_ROAD_RAMP_X_DOWN  = 2530,
	SPR_BTWDN_ROAD_RAMP_X_UP    = 2531, //for some weird reason the order is swapped
	SPR_BTWDN_ROAD_RAMP_Y_UP    = 2532, //between X and Y.
	SPR_BTWDN_ROAD_Y_SLOPE_UP   = 2533,
	SPR_BTWDN_ROAD_X_SLOPE_UP   = 2534,
	SPR_BTWDN_ROAD_Y_SLOPE_DOWN = 2535,
	SPR_BTWDN_ROAD_X_SLOPE_DOWN = 2536,
	SPR_BTWDN_RAIL_RAMP_Y_DOWN  = 2537,
	SPR_BTWDN_RAIL_RAMP_X_DOWN  = 2538,
	SPR_BTWDN_RAIL_RAMP_X_UP    = 2539, //for some weird reason the order is swapped
	SPR_BTWDN_RAIL_RAMP_Y_UP    = 2540, //between X and Y.
	SPR_BTWDN_RAIL_Y_SLOPE_UP   = 2541,
	SPR_BTWDN_RAIL_X_SLOPE_UP   = 2542,
	SPR_BTWDN_RAIL_Y_SLOPE_DOWN = 2543,
	SPR_BTWDN_RAIL_X_SLOPE_DOWN = 2544,
	SPR_BTWDN_MONO_RAMP_Y_DOWN  = 4352,
	SPR_BTWDN_MONO_RAMP_X_DOWN  = 4353,
	SPR_BTWDN_MONO_RAMP_X_UP    = 4354, //for some weird reason the order is swapped
	SPR_BTWDN_MONO_RAMP_Y_UP    = 4355, //between X and Y.
	SPR_BTWDN_MONO_Y_SLOPE_UP   = 4356,
	SPR_BTWDN_MONO_X_SLOPE_UP   = 4357,
	SPR_BTWDN_MONO_Y_SLOPE_DOWN = 4358,
	SPR_BTWDN_MONO_X_SLOPE_DOWN = 4359,
	SPR_BTWDN_MGLV_RAMP_Y_DOWN  = 4392,
	SPR_BTWDN_MGLV_RAMP_X_DOWN  = 4393,
	SPR_BTWDN_MGLV_RAMP_X_UP    = 4394, //for some weird reason the order is swapped
	SPR_BTWDN_MGLV_RAMP_Y_UP    = 4395, //between X and Y.
	SPR_BTWDN_MGLV_Y_SLOPE_UP   = 4396,
	SPR_BTWDN_MGLV_X_SLOPE_UP   = 4397,
	SPR_BTWDN_MGLV_Y_SLOPE_DOWN = 4398,
	SPR_BTWDN_MGLV_X_SLOPE_DOWN = 4399,

	/* Steel Girder with arches */
	/* BTSGA == Bridge Type Steel Girder Arched */
	/* This is bridge type number 2 */
	SPR_BTSGA_RAIL_X_REAR       = 2499,
	SPR_BTSGA_RAIL_Y_REAR       = 2500,
	SPR_BTSGA_ROAD_X_REAR       = 2501,
	SPR_BTSGA_ROAD_Y_REAR       = 2502,
	SPR_BTSGA_X_FRONT           = 2503,
	SPR_BTSGA_Y_FRONT           = 2504,
	SPR_BTSGA_X_PILLAR          = 2505,
	SPR_BTSGA_Y_PILLAR          = 2606,
	SPR_BTSGA_MONO_X_REAR       = 4324,
	SPR_BTSGA_MONO_Y_REAR       = 4325,
	SPR_BTSGA_MGLV_X_REAR       = 4364,
	SPR_BTSGA_MGLV_Y_REAR       = 4365,

	/* BTSUS == Suspension bridge */
	/* TILE_* denotes the different tiles a suspension bridge
	 * can have
	 * TILE_A and TILE_B are the "beginnings" and "ends" of the
	 *   suspension system. they have small rectangluar endcaps
	 * TILE_C and TILE_D look almost identical to TILE_A and
	 *   TILE_B, but they do not have the "endcaps". They form the
	 *   middle part
	 * TILE_E is a condensed configuration of two pillars. while they
	 *   are usually 2 pillars apart, they only have 1 pillar separation
	 *   here
	 * TILE_F is an extended configuration of pillars. they are
	 *   plugged in when pillars should be 3 tiles apart
	 */
	SPR_BTSUS_ROAD_Y_REAR_TILE_A  = 2453,
	SPR_BTSUS_ROAD_Y_REAR_TILE_B  = 2454,
	SPR_BTSUS_Y_FRONT_TILE_A      = 2455,
	SPR_BTSUS_Y_FRONT_TILE_B      = 2456,
	SPR_BTSUS_ROAD_Y_REAR_TILE_D  = 2457,
	SPR_BTSUS_ROAD_Y_REAR_TILE_C  = 2458,
	SPR_BTSUS_Y_FRONT_TILE_D      = 2459,
	SPR_BTSUS_Y_FRONT_TILE_C      = 2460,
	SPR_BTSUS_ROAD_X_REAR_TILE_A  = 2461,
	SPR_BTSUS_ROAD_X_REAR_TILE_B  = 2462,
	SPR_BTSUS_X_FRONT_TILE_A      = 2463,
	SPR_BTSUS_X_FRONT_TILE_B      = 2464,
	SPR_BTSUS_ROAD_X_TILE_D       = 2465,
	SPR_BTSUS_ROAD_X_TILE_C       = 2466,
	SPR_BTSUS_X_FRONT_TILE_D      = 2467,
	SPR_BTSUS_X_FRONT_TILE_C      = 2468,
	SPR_BTSUS_RAIL_Y_REAR_TILE_A  = 2469,
	SPR_BTSUS_RAIL_Y_REAR_TILE_B  = 2470,
	SPR_BTSUS_RAIL_Y_REAR_TILE_D  = 2471,
	SPR_BTSUS_RAIL_Y_REAR_TILE_C  = 2472,
	SPR_BTSUS_RAIL_X_REAR_TILE_A  = 2473,
	SPR_BTSUS_RAIL_X_REAR_TILE_B  = 2474,
	SPR_BTSUS_RAIL_X_REAR_TILE_D  = 2475,
	SPR_BTSUS_RAIL_X_REAR_TILE_C  = 2476,
	SPR_BTSUS_Y_PILLAR_TILE_A     = 2477,
	SPR_BTSUS_Y_PILLAR_TILE_B     = 2478,
	SPR_BTSUS_Y_PILLAR_TILE_D     = 2479,
	SPR_BTSUS_Y_PILLAR_TILE_C     = 2480,
	SPR_BTSUS_X_PILLAR_TILE_A     = 2481,
	SPR_BTSUS_X_PILLAR_TILE_B     = 2482,
	SPR_BTSUS_X_PILLAR_TILE_D     = 2483,
	SPR_BTSUS_X_PILLAR_TILE_C     = 2484,
	SPR_BTSUS_RAIL_Y_REAR_TILE_E  = 2485,
	SPR_BTSUS_RAIL_X_REAR_TILE_E  = 2486,
	SPR_BTSUS_ROAD_Y_REAR_TILE_E  = 2487,
	SPR_BTSUS_ROAD_X_REAR_TILE_E  = 2488,
	SPR_BTSUS_Y_FRONT_TILE_E      = 2489,
	SPR_BTSUS_X_FRONT_TILE_E      = 2490,
	SPR_BTSUS_Y_PILLAR_TILE_E     = 2491,
	SPR_BTSUS_X_PILLAR_TILE_E     = 2492,
	SPR_BTSUS_RAIL_X_REAR_TILE_F  = 2493,
	SPR_BTSUS_RAIL_Y_REAR_TILE_F  = 2494,
	SPR_BTSUS_ROAD_X_REAR_TILE_F  = 2495,
	SPR_BTSUS_ROAD_Y_REAR_TILE_F  = 2496,
	SPR_BTSUS_X_FRONT             = 2497,
	SPR_BTSUS_Y_FRONT             = 2498,
	SPR_BTSUS_MONO_Y_REAR_TILE_A  = 4334,
	SPR_BTSUS_MONO_Y_REAR_TILE_B  = 4335,
	SPR_BTSUS_MONO_Y_REAR_TILE_D  = 4336,
	SPR_BTSUS_MONO_Y_REAR_TILE_C  = 4337,
	SPR_BTSUS_MONO_X_REAR_TILE_A  = 4338,
	SPR_BTSUS_MONO_X_REAR_TILE_B  = 4339,
	SPR_BTSUS_MONO_X_REAR_TILE_D  = 4340,
	SPR_BTSUS_MONO_X_REAR_TILE_C  = 4341,
	SPR_BTSUS_MONO_Y_REAR_TILE_E  = 4342,
	SPR_BTSUS_MONO_X_REAR_TILE_E  = 4343,
	SPR_BTSUS_MONO_X_REAR_TILE_F  = 4344,
	SPR_BTSUS_MONO_Y_REAR_TILE_F  = 4345,
	SPR_BTSUS_MGLV_Y_REAR_TILE_A  = 4374,
	SPR_BTSUS_MGLV_Y_REAR_TILE_B  = 4375,
	SPR_BTSUS_MGLV_Y_REAR_TILE_D  = 4376,
	SPR_BTSUS_MGLV_Y_REAR_TILE_C  = 4377,
	SPR_BTSUS_MGLV_X_REAR_TILE_A  = 4378,
	SPR_BTSUS_MGLV_X_REAR_TILE_B  = 4379,
	SPR_BTSUS_MGLV_X_REAR_TILE_D  = 4380,
	SPR_BTSUS_MGLV_X_REAR_TILE_C  = 4381,
	SPR_BTSUS_MGLV_Y_REAR_TILE_E  = 4382,
	SPR_BTSUS_MGLV_X_REAR_TILE_E  = 4383,
	SPR_BTSUS_MGLV_X_REAR_TILE_F  = 4384,
	SPR_BTSUS_MGLV_Y_REAR_TILE_F  = 4385,

	/* cantilever bridges */
	/* They have three different kinds of tiles:
	 * END(ing), MID(dle), BEG(ginning) */
	SPR_BTCAN_RAIL_X_BEG          = 2507,
	SPR_BTCAN_RAIL_X_MID          = 2508,
	SPR_BTCAN_RAIL_X_END          = 2509,
	SPR_BTCAN_RAIL_Y_END          = 2510,
	SPR_BTCAN_RAIL_Y_MID          = 2511,
	SPR_BTCAN_RAIL_Y_BEG          = 2512,
	SPR_BTCAN_ROAD_X_BEG          = 2513,
	SPR_BTCAN_ROAD_X_MID          = 2514,
	SPR_BTCAN_ROAD_X_END          = 2515,
	SPR_BTCAN_ROAD_Y_END          = 2516,
	SPR_BTCAN_ROAD_Y_MID          = 2517,
	SPR_BTCAN_ROAD_Y_BEG          = 2518,
	SPR_BTCAN_X_FRONT_BEG         = 2519,
	SPR_BTCAN_X_FRONT_MID         = 2520,
	SPR_BTCAN_X_FRONT_END         = 2521,
	SPR_BTCAN_Y_FRONT_END         = 2522,
	SPR_BTCAN_Y_FRONT_MID         = 2523,
	SPR_BTCAN_Y_FRONT_BEG         = 2524,
	SPR_BTCAN_X_PILLAR_BEG        = 2525,
	SPR_BTCAN_X_PILLAR_MID        = 2526,
	SPR_BTCAN_Y_PILLAR_MID        = 2527,
	SPR_BTCAN_Y_PILLAR_BEG        = 2528,
	SPR_BTCAN_MONO_X_BEG          = 4346,
	SPR_BTCAN_MONO_X_MID          = 4347,
	SPR_BTCAN_MONO_X_END          = 4348,
	SPR_BTCAN_MONO_Y_END          = 4349,
	SPR_BTCAN_MONO_Y_MID          = 4350,
	SPR_BTCAN_MONO_Y_BEG          = 4351,
	SPR_BTCAN_MGLV_X_BEG          = 4386,
	SPR_BTCAN_MGLV_X_MID          = 4387,
	SPR_BTCAN_MGLV_X_END          = 4388,
	SPR_BTCAN_MGLV_Y_END          = 4389,
	SPR_BTCAN_MGLV_Y_MID          = 4390,
	SPR_BTCAN_MGLV_Y_BEG          = 4391,

	/* little concrete bridge */
	SPR_BTCON_RAIL_X        = 2493,
	SPR_BTCON_RAIL_Y        = 2494,
	SPR_BTCON_ROAD_X        = 2495,
	SPR_BTCON_ROAD_Y        = 2496,
	SPR_BTCON_X_FRONT       = 2497,
	SPR_BTCON_Y_FRONT       = 2498,
	SPR_BTCON_X_PILLAR      = 2505,
	SPR_BTCON_Y_PILLAR      = 2506,
	SPR_BTCON_MONO_X        = 4344,
	SPR_BTCON_MONO_Y        = 4345,
	SPR_BTCON_MGLV_X        = 4384,
	SPR_BTCON_MGLV_Y        = 4385,

	/* little steel girder bridge */
	SPR_BTGIR_RAIL_X        = 2553,
	SPR_BTGIR_RAIL_Y        = 2554,
	SPR_BTGIR_ROAD_X        = 2555,
	SPR_BTGIR_ROAD_Y        = 2556,
	SPR_BTGIR_X_FRONT       = 2557,
	SPR_BTGIR_Y_FRONT       = 2558,
	SPR_BTGIR_X_PILLAR      = 2505,
	SPR_BTGIR_Y_PILLAR      = 2506,
	SPR_BTGIR_MONO_X        = 4362,
	SPR_BTGIR_MONO_Y        = 4363,
	SPR_BTGIR_MGLV_X        = 4402,
	SPR_BTGIR_MGLV_Y        = 4403,

	/* tubular bridges */
	/* tubular bridges have 3 kinds of tiles:
	 *  a start tile (with only half a tube on the far side, marked _BEG
	 *  a middle tile (full tunnel), marked _MID
	 *  and an end tile (half a tube on the near side, maked _END
	 */
	SPR_BTTUB_X_FRONT_BEG       = 2559,
	SPR_BTTUB_X_FRONT_MID       = 2660,
	SPR_BTTUB_X_FRONT_END       = 2561,
	SPR_BTTUB_Y_FRONT_END       = 2562,
	SPR_BTTUB_Y_FRONT_MID       = 2563,
	SPR_BTTUB_Y_FRONT_BEG       = 2564,
	SPR_BTTUB_X_RAIL_REAR_BEG   = 2569,
	SPR_BTTUB_X_RAIL_REAR_MID   = 2570,
	SPR_BTTUB_X_RAIL_REAR_END   = 2571,
	SPR_BTTUB_Y_RAIL_REAR_BEG   = 2572,
	SPR_BTTUB_Y_RAIL_REAR_MID   = 2573,
	SPR_BTTUB_Y_RAIL_REAR_END   = 2574,
	SPR_BTTUB_X_ROAD_REAR_BEG   = 2575,
	SPR_BTTUB_X_ROAD_REAR_MID   = 2576,
	SPR_BTTUB_X_ROAD_REAR_END   = 2577,
	SPR_BTTUB_Y_ROAD_REAR_BEG   = 2578,
	SPR_BTTUB_Y_ROAD_REAR_MID   = 2579,
	SPR_BTTUB_Y_ROAD_REAR_END   = 2580,
	SPR_BTTUB_X_MONO_REAR_BEG   = 2581,
	SPR_BTTUB_X_MONO_REAR_MID   = 2582,
	SPR_BTTUB_X_MONO_REAR_END   = 2583,
	SPR_BTTUB_Y_MONO_REAR_BEG   = 2584,
	SPR_BTTUB_Y_MONO_REAR_MID   = 2585,
	SPR_BTTUB_Y_MONO_REAR_END   = 2586,
	SPR_BTTUB_X_MGLV_REAR_BEG   = 2587,
	SPR_BTTUB_X_MGLV_REAR_MID   = 2588,
	SPR_BTTUB_X_MGLV_REAR_END   = 2589,
	SPR_BTTUB_Y_MGLV_REAR_BEG   = 2590,
	SPR_BTTUB_Y_MGLV_REAR_MID   = 2591,
	SPR_BTTUB_Y_MGLV_REAR_END   = 2592,


	/* ramps (for all bridges except wood and tubular?)*/
	SPR_BTGEN_RAIL_X_SLOPE_DOWN = 2437,
	SPR_BTGEN_RAIL_X_SLOPE_UP   = 2438,
	SPR_BTGEN_RAIL_Y_SLOPE_DOWN = 2439,
	SPR_BTGEN_RAIL_Y_SLOPE_UP   = 2440,
	SPR_BTGEN_RAIL_RAMP_X_UP    = 2441,
	SPR_BTGEN_RAIL_RAMP_X_DOWN  = 2442,
	SPR_BTGEN_RAIL_RAMP_Y_UP    = 2443,
	SPR_BTGEN_RAIL_RAMP_Y_DOWN  = 2444,
	SPR_BTGEN_ROAD_X_SLOPE_DOWN = 2445,
	SPR_BTGEN_ROAD_X_SLOPE_UP   = 2446,
	SPR_BTGEN_ROAD_Y_SLOPE_DOWN = 2447,
	SPR_BTGEN_ROAD_Y_SLOPE_UP   = 2448,
	SPR_BTGEN_ROAD_RAMP_X_UP    = 2449,
	SPR_BTGEN_ROAD_RAMP_X_DOWN  = 2450,
	SPR_BTGEN_ROAD_RAMP_Y_UP    = 2451,
	SPR_BTGEN_ROAD_RAMP_Y_DOWN  = 2452,
	SPR_BTGEN_MONO_X_SLOPE_DOWN = 4326,
	SPR_BTGEN_MONO_X_SLOPE_UP   = 4327,
	SPR_BTGEN_MONO_Y_SLOPE_DOWN = 4328,
	SPR_BTGEN_MONO_Y_SLOPE_UP   = 4329,
	SPR_BTGEN_MONO_RAMP_X_UP    = 4330,
	SPR_BTGEN_MONO_RAMP_X_DOWN  = 4331,
	SPR_BTGEN_MONO_RAMP_Y_UP    = 4332,
	SPR_BTGEN_MONO_RAMP_Y_DOWN  = 4333,
	SPR_BTGEN_MGLV_X_SLOPE_DOWN = 4366,
	SPR_BTGEN_MGLV_X_SLOPE_UP   = 4367,
	SPR_BTGEN_MGLV_Y_SLOPE_DOWN = 4368,
	SPR_BTGEN_MGLV_Y_SLOPE_UP   = 4369,
	SPR_BTGEN_MGLV_RAMP_X_UP    = 4370,
	SPR_BTGEN_MGLV_RAMP_X_DOWN  = 4371,
	SPR_BTGEN_MGLV_RAMP_Y_UP    = 4372,
	SPR_BTGEN_MGLV_RAMP_Y_DOWN  = 4373,

	/* Vehicle view sprites */
	SPR_CENTRE_VIEW_VEHICLE   = 683,
	SPR_SEND_TRAIN_TODEPOT    = 685,
	SPR_SEND_ROADVEH_TODEPOT  = 686,
	SPR_SEND_AIRCRAFT_TODEPOT = 687,
	SPR_SEND_SHIP_TODEPOT     = 688,

	SPR_IGNORE_SIGNALS        = 689,
	SPR_SHOW_ORDERS           = 690,
	SPR_SHOW_VEHICLE_DETAILS  = 691,
	SPR_REFIT_VEHICLE         = 692,
	SPR_FORCE_VEHICLE_TURN    = 715,

	/* Vehicle sprite-flags (red/green) */
	SPR_FLAG_VEH_STOPPED  = 3090,
	SPR_FLAG_VEH_RUNNING  = 3091,

	SPR_VEH_BUS_SW_VIEW   = 3097,
	SPR_VEH_BUS_SIDE_VIEW = 3098,

	/* Rotor sprite numbers */
	SPR_ROTOR_STOPPED   = 3901,
	SPR_ROTOR_MOVING_1  = 3902,
	SPR_ROTOR_MOVING_3  = 3904,

	/* Town/house sprites */
	SPR_LIFT = 1443,

	//used in town_land.h
	//CNST1..3 = Those are the different stages of construction
	//The last 2 hexas correspond to the type of building it represent, if any
	SPR_CNST1_TALLOFFICE_00                 = 1421,
	SPR_CNST2_TALLOFFICE_00                 = 1422,
	SPR_CNST3_TALLOFFICE_00                 = 1423,
	SPR_GROUND_TALLOFFICE_00                = 1424,
	SPR_BUILD_TALLOFFICE_00                 = 1425, //temperate
	SPR_CNST1_OFFICE_01                     = 1426,
	SPR_CNST2_OFFICE_01                     = 1427,
	SPR_BUILD_OFFICE_01                     = 1428, //temperate
	SPR_GROUND_OFFICE_01                    = 1429,
	SPR_CNST1_SMLBLCKFLATS_02               = 1430, //Small Block of Flats
	SPR_CNST2_SMLBLCKFLATS_02               = 1431,
	SPR_BUILD_SMLBLCKFLATS_02               = 1432, //temperate
	SPR_GROUND_SMLBLCKFLATS_02              = 1433,
	SPR_CNST1_TEMPCHURCH                    = 1434,
	SPR_CNST2_TEMPCHURCH                    = 1435,
	SPR_BUILD_TEMPCHURCH                    = 1436,
	SPR_GROUND_TEMPCHURCH                   = 1437,
	SPR_CNST1_LARGEOFFICE_04                = 1440,
	SPR_CNST2_LARGEOFFICE_04                = 1441,
	SPR_BUILD_LARGEOFFICE_04                = 1442, // temperate, sub-arctic, subtropical
	SPR_BUILD_LARGEOFFICE_04_SNOW           = 4569, // same, with snow
	// These are in fact two houses for the same houseID.  so V1 and V2
	SPR_CNST1_TOWNHOUSE_06_V1               = 1444,
	SPR_CNST2_TOWNHOUSE_06_V1               = 1445,
	SPR_BUILD_TOWNHOUSE_06_V1               = 1446, // 1st variation
	SPR_GRND_TOWNHOUSE_06_V1                = 1447,
	SPR_CNST1_TOWNHOUSE_06_V2               = 1501, // used as ground, but is stage1
	SPR_CNST1_TOWNHOUSE_06_V2_P             = 1502, // pipes extensions for previous
	SPR_CNST2_TOWNHOUSE_06_V2_G             = 1503, // Ground of cnst stage 2
	SPR_CNST2_TOWNHOUSE_06_V2               = 1504, // real cnst stage 2
	SPR_GRND_TOWNHOUSE_06_V2                = 1505,
	SPR_BUILD_TOWNHOUSE_06_V2               = 1506, // 2nd variation
	SPR_CNST1_HOTEL_07_NW                   = 1448,
	SPR_CNST2_HOTEL_07_NW                   = 1449,
	SPR_BUILD_HOTEL_07_NW                   = 1450,
	SPR_CNST1_HOTEL_07_SE                   = 1451,
	SPR_CNST2_HOTEL_07_SE                   = 1452,
	SPR_BUILD_HOTEL_07_SE                   = 1453,
	SPR_STATUE_HORSERIDER_09                = 1454,
	SPR_FOUNTAIN_0A                         = 1455,
	SPR_PARKSTATUE_0B                       = 1456,
	SPR_PARKALLEY_0C                        = 1457,
	SPR_CNST1_OFFICE_0D                     = 1458,
	SPR_CNST2_OFFICE_0D                     = 1459,
	SPR_BUILD_OFFICE_0D                     = 1460,
	SPR_CNST1_SHOPOFFICE_0E                 = 1461,
	SPR_CNST2_SHOPOFFICE_0E                 = 1462,
	SPR_BUILD_SHOPOFFICE_0E                 = 1463,
	SPR_CNST1_SHOPOFFICE_0F                 = 1464,
	SPR_CNST2_SHOPOFFICE_0F                 = 1465,
	SPR_BUILD_SHOPOFFICE_0F                 = 1466,

	/* Easter egg/disaster sprites */
	SPR_BLIMP                  = 3905, // Zeppelin
	SPR_BLIMP_CRASHING         = 3906,
	SPR_BLIMP_CRASHED          = 3907,
	SPR_UFO_SMALL_SCOUT        = 3908, // XCOM - UFO Defense
	SPR_UFO_SMALL_SCOUT_DARKER = 3909,
	SPR_SUB_SMALL_NE           = 3910, // Silent Service
	SPR_SUB_SMALL_SE           = 3911,
	SPR_SUB_SMALL_SW           = 3912,
	SPR_SUB_SMALL_NW           = 3913,
	SPR_SUB_LARGE_NE           = 3914,
	SPR_SUB_LARGE_SE           = 3915,
	SPR_SUB_LARGE_SW           = 3916,
	SPR_SUB_LARGE_NW           = 3917,
	SPR_F_15                   = 3918, // F-15 Strike Eagle
	SPR_F_15_FIRING            = 3919,
	SPR_UFO_HARVESTER          = 3920, // XCOM - UFO Defense
	SPR_XCOM_SKYRANGER         = 3921,
	SPR_AH_64A                 = 3922, // Gunship
	SPR_AH_64A_FIRING          = 3923,

	/* main_gui.c */
	SPR_IMG_TERRAFORM_UP    = 694,
	SPR_IMG_TERRAFORM_DOWN  = 695,
	SPR_IMG_DYNAMITE        = 703,
	SPR_IMG_ROCKS           = 4084,
	SPR_IMG_LIGHTHOUSE_DESERT = 4085, // XXX - is Desert image on the desert-climate
	SPR_IMG_TRANSMITTER     = 4086,
	SPR_IMG_LEVEL_LAND      = SPR_OPENTTD_BASE + 61,
	SPR_IMG_BUILD_CANAL     = SPR_OPENTTD_BASE + 58,
	SPR_IMG_BUILD_LOCK      = SPR_CANALS_BASE + 69,
	SPR_IMG_PAUSE           = 726,
	SPR_IMG_FASTFORWARD     = SPR_OPENTTD_BASE + 54,
	SPR_IMG_SETTINGS        = 751,
	SPR_IMG_SAVE            = 724,
	SPR_IMG_SMALLMAP        = 708,
	SPR_IMG_TOWN            = 4077,
	SPR_IMG_SUBSIDIES       = 679,
	SPR_IMG_COMPANY_LIST    = 1299,
	SPR_IMG_COMPANY_FINANCE = 737,
	SPR_IMG_COMPANY_GENERAL = 743,
	SPR_IMG_GRAPHS          = 745,
	SPR_IMG_COMPANY_LEAGUE  = 684,
	SPR_IMG_SHOW_COUNTOURS  = 738,
	SPR_IMG_SHOW_VEHICLES   = 739,
	SPR_IMG_SHOW_ROUTES     = 740,
	SPR_IMG_INDUSTRY        = 741,
	SPR_IMG_PLANTTREES      = 742,
	SPR_IMG_TRAINLIST       = 731,
	SPR_IMG_TRUCKLIST       = 732,
	SPR_IMG_SHIPLIST        = 733,
	SPR_IMG_AIRPLANESLIST   = 734,
	SPR_IMG_ZOOMIN          = 735,
	SPR_IMG_ZOOMOUT         = 736,
	SPR_IMG_BUILDRAIL       = 727,
	SPR_IMG_BUILDROAD       = 728,
	SPR_IMG_BUILDWATER      = 729,
	SPR_IMG_BUILDAIR        = 730,
	SPR_IMG_LANDSCAPING     = 4083,
	SPR_IMG_MUSIC           = 713,
	SPR_IMG_MESSAGES        = 680,
	SPR_IMG_QUERY           = 723,
	SPR_IMG_SIGN            = 4082,
	SPR_IMG_BUY_LAND        = 4791,

	/* OPEN TRANSPORT TYCOON in gamescreen */
	SPR_OTTD_O                = 4842,
	SPR_OTTD_P                = 4841,
	SPR_OTTD_E                = SPR_OPENTTD_BASE + 13,
	SPR_OTTD_D                = SPR_OPENTTD_BASE + 14,
	SPR_OTTD_N                = 4839,
	SPR_OTTD_T                = 4836,
	SPR_OTTD_R                = 4837,
	SPR_OTTD_A                = 4838,
	SPR_OTTD_S                = 4840,
	SPR_OTTD_Y                = 4843,
	SPR_OTTD_C                = 4844,

	SPR_HIGHSCORE_CHART_BEGIN = 4804,
	SPR_TYCOON_IMG1_BEGIN     = 4814,
	SPR_TYCOON_IMG2_BEGIN     = 4824,

	/* Industry sprites */
	SPR_IT_SUGAR_MINE_SIEVE         = 4775,
	SPR_IT_SUGAR_MINE_CLOUDS        = 4784,
	SPR_IT_SUGAR_MINE_PILE          = 4780,
	SPR_IT_TOFFEE_QUARRY_TOFFEE     = 4766,
	SPR_IT_TOFFEE_QUARRY_SHOVEL     = 4767,
	SPR_IT_BUBBLE_GENERATOR_SPRING  = 4746,
	SPR_IT_BUBBLE_GENERATOR_BUBBLE  = 4747,
	SPR_IT_TOY_FACTORY_STAMP_HOLDER = 4717,
	SPR_IT_TOY_FACTORY_STAMP        = 4718,
	SPR_IT_TOY_FACTORY_CLAY         = 4719,
	SPR_IT_TOY_FACTORY_ROBOT        = 4720,
	SPR_IT_POWER_PLANT_TRANSFORMERS = 2054,

	/*small icons of cargo available in station waiting*/
	SPR_CARGO_PASSENGER             = 4297,
	SPR_CARGO_COAL                  = 4298,
	SPR_CARGO_MAIL                  = 4299,
	SPR_CARGO_OIL                   = 4300,
	SPR_CARGO_LIVESTOCK             = 4301,
	SPR_CARGO_GOODS                 = 4302,
	SPR_CARGO_GRAIN                 = 4303,
	SPR_CARGO_WOOD                  = 4304,
	SPR_CARGO_IRON_ORE              = 4305,
	SPR_CARGO_STEEL                 = 4306,
	SPR_CARGO_VALUES_GOLD           = 4307,  /*shared between temperate and arctic*/
	SPR_CARGO_FRUIT                 = 4308,
	SPR_CARGO_COPPER_ORE            = 4309,
	SPR_CARGO_WATERCOLA             = 4310,  /*shared between desert and toyland*/
	SPR_CARGO_DIAMONDS              = 4311,
	SPR_CARGO_FOOD                  = 4312,
	SPR_CARGO_PAPER                 = 4313,
	SPR_CARGO_RUBBER                = 4314,
	SPR_CARGO_CANDY                 = 4315,
	SPR_CARGO_SUGAR                 = 4316,
	SPR_CARGO_TOYS                  = 4317,
	SPR_CARGO_COTTONCANDY           = 4318,
	SPR_CARGO_FIZZYDRINK            = 4319,
	SPR_CARGO_TOFFEE                = 4320,
	SPR_CARGO_BUBBLES               = 4321,
	SPR_CARGO_PLASTIC               = 4322,
	SPR_CARGO_BATTERIES             = 4323,

	/* Effect vehicles */
	SPR_BULLDOZER_NE = 1416,
	SPR_BULLDOZER_SE = 1417,
	SPR_BULLDOZER_SW = 1418,
	SPR_BULLDOZER_NW = 1419,

	SPR_SMOKE_0 = 2040,
	SPR_SMOKE_1 = 2041,
	SPR_SMOKE_2 = 2042,
	SPR_SMOKE_3 = 2043,
	SPR_SMOKE_4 = 2044,

	SPR_DIESEL_SMOKE_0 = 3073,
	SPR_DIESEL_SMOKE_1 = 3074,
	SPR_DIESEL_SMOKE_2 = 3075,
	SPR_DIESEL_SMOKE_3 = 3076,
	SPR_DIESEL_SMOKE_4 = 3077,
	SPR_DIESEL_SMOKE_5 = 3078,

	SPR_STEAM_SMOKE_0 = 3079,
	SPR_STEAM_SMOKE_1 = 3080,
	SPR_STEAM_SMOKE_2 = 3081,
	SPR_STEAM_SMOKE_3 = 3082,
	SPR_STEAM_SMOKE_4 = 3083,

	SPR_ELECTRIC_SPARK_0 = 3084,
	SPR_ELECTRIC_SPARK_1 = 3085,
	SPR_ELECTRIC_SPARK_2 = 3086,
	SPR_ELECTRIC_SPARK_3 = 3087,
	SPR_ELECTRIC_SPARK_4 = 3088,
	SPR_ELECTRIC_SPARK_5 = 3089,

	SPR_CHIMNEY_SMOKE_0 = 3701,
	SPR_CHIMNEY_SMOKE_1 = 3702,
	SPR_CHIMNEY_SMOKE_2 = 3703,
	SPR_CHIMNEY_SMOKE_3 = 3704,
	SPR_CHIMNEY_SMOKE_4 = 3705,
	SPR_CHIMNEY_SMOKE_5 = 3706,
	SPR_CHIMNEY_SMOKE_6 = 3707,
	SPR_CHIMNEY_SMOKE_7 = 3708,

	SPR_EXPLOSION_LARGE_0 = 3709,
	SPR_EXPLOSION_LARGE_1 = 3710,
	SPR_EXPLOSION_LARGE_2 = 3711,
	SPR_EXPLOSION_LARGE_3 = 3712,
	SPR_EXPLOSION_LARGE_4 = 3713,
	SPR_EXPLOSION_LARGE_5 = 3714,
	SPR_EXPLOSION_LARGE_6 = 3715,
	SPR_EXPLOSION_LARGE_7 = 3716,
	SPR_EXPLOSION_LARGE_8 = 3717,
	SPR_EXPLOSION_LARGE_9 = 3718,
	SPR_EXPLOSION_LARGE_A = 3719,
	SPR_EXPLOSION_LARGE_B = 3720,
	SPR_EXPLOSION_LARGE_C = 3721,
	SPR_EXPLOSION_LARGE_D = 3722,
	SPR_EXPLOSION_LARGE_E = 3723,
	SPR_EXPLOSION_LARGE_F = 3724,

	SPR_EXPLOSION_SMALL_0 = 3725,
	SPR_EXPLOSION_SMALL_1 = 3726,
	SPR_EXPLOSION_SMALL_2 = 3727,
	SPR_EXPLOSION_SMALL_3 = 3728,
	SPR_EXPLOSION_SMALL_4 = 3729,
	SPR_EXPLOSION_SMALL_5 = 3730,
	SPR_EXPLOSION_SMALL_6 = 3731,
	SPR_EXPLOSION_SMALL_7 = 3732,
	SPR_EXPLOSION_SMALL_8 = 3733,
	SPR_EXPLOSION_SMALL_9 = 3734,
	SPR_EXPLOSION_SMALL_A = 3735,
	SPR_EXPLOSION_SMALL_B = 3736,

	SPR_BREAKDOWN_SMOKE_0 = 3737,
	SPR_BREAKDOWN_SMOKE_1 = 3738,
	SPR_BREAKDOWN_SMOKE_2 = 3739,
	SPR_BREAKDOWN_SMOKE_3 = 3740,

	SPR_BUBBLE_0 = 4748,
	SPR_BUBBLE_1 = 4749,
	SPR_BUBBLE_2 = 4750,
	SPR_BUBBLE_GENERATE_0 = 4751,
	SPR_BUBBLE_GENERATE_1 = 4752,
	SPR_BUBBLE_GENERATE_2 = 4753,
	SPR_BUBBLE_GENERATE_3 = 4754,
	SPR_BUBBLE_BURST_0 = 4755,
	SPR_BUBBLE_BURST_1 = 4756,
	SPR_BUBBLE_BURST_2 = 4757,
	SPR_BUBBLE_ABSORB_0 = 4758,
	SPR_BUBBLE_ABSORB_1 = 4759,
	SPR_BUBBLE_ABSORB_2 = 4760,
	SPR_BUBBLE_ABSORB_3 = 4761,
	SPR_BUBBLE_ABSORB_4 = 4762,

	/* Electrified rail build menu */
	SPR_BUILD_NS_ELRAIL = SPR_ELRAIL_BASE + 39,
	SPR_BUILD_X_ELRAIL  = SPR_ELRAIL_BASE + 40,
	SPR_BUILD_EW_ELRAIL = SPR_ELRAIL_BASE + 41,
	SPR_BUILD_Y_ELRAIL  = SPR_ELRAIL_BASE + 42,
	SPR_BUILD_TUNNEL_ELRAIL = SPR_ELRAIL_BASE + 47,

	/* airport_gui.c */
	SPR_IMG_AIRPORT       = 744,

	/* dock_gui.c */
	SPR_IMG_SHIP_DEPOT    = 748,
	SPR_IMG_SHIP_DOCK     = 746,
	SPR_IMG_BOUY          = 693,

	/* music_gui.c */
	SPR_IMG_SKIP_TO_PREV  = 709,
	SPR_IMG_SKIP_TO_NEXT  = 710,
	SPR_IMG_STOP_MUSIC    = 711,
	SPR_IMG_PLAY_MUSIC    = 712,

	/* road_gui.c */
	SPR_IMG_ROAD_NW       = 1309,
	SPR_IMG_ROAD_NE       = 1310,
	SPR_IMG_ROAD_DEPOT    = 1295,
	SPR_IMG_BUS_STATION   = 749,
	SPR_IMG_TRUCK_BAY     = 750,
	SPR_IMG_BRIDGE        = 2594,
	SPR_IMG_ROAD_TUNNEL   = 2429,
	SPR_IMG_REMOVE        = 714,
	SPR_IMG_TRAMWAY_NW    = SPR_TRAMWAY_BASE + 0,
	SPR_IMG_TRAMWAY_NE    = SPR_TRAMWAY_BASE + 1,

	/* rail_gui.c */
	SPR_IMG_RAIL_NS    = 1251,
	SPR_IMG_RAIL_NE    = 1252,
	SPR_IMG_RAIL_EW    = 1253,
	SPR_IMG_RAIL_NW    = 1254,
	SPR_IMG_AUTORAIL   = SPR_OPENTTD_BASE + 0,
	SPR_IMG_AUTOMONO   = SPR_OPENTTD_BASE + 1,
	SPR_IMG_AUTOMAGLEV = SPR_OPENTTD_BASE + 2,

	SPR_IMG_WAYPOINT = SPR_OPENTTD_BASE + 3,

	SPR_IMG_DEPOT_RAIL   = 1294,
	SPR_IMG_DEPOT_MONO   = SPR_OPENTTD_BASE + 9,
	SPR_IMG_DEPOT_MAGLEV = SPR_OPENTTD_BASE + 10,

	SPR_IMG_RAIL_STATION = 1298,
	SPR_IMG_RAIL_SIGNALS = 1291,

	SPR_IMG_TUNNEL_RAIL   = 2430,
	SPR_IMG_TUNNEL_MONO   = 2431,
	SPR_IMG_TUNNEL_MAGLEV = 2432,

	SPR_IMG_CONVERT_RAIL   = SPR_OPENTTD_BASE + 22,
	SPR_IMG_CONVERT_MONO   = SPR_OPENTTD_BASE + 24,
	SPR_IMG_CONVERT_MAGLEV = SPR_OPENTTD_BASE + 26,

	/* intro_gui.c, genworld_gui.c */
	SPR_SELECT_TEMPERATE           = 4882,
	SPR_SELECT_TEMPERATE_PUSHED    = 4883,
	SPR_SELECT_SUB_ARCTIC          = 4884,
	SPR_SELECT_SUB_ARCTIC_PUSHED   = 4885,
	SPR_SELECT_SUB_TROPICAL        = 4886,
	SPR_SELECT_SUB_TROPICAL_PUSHED = 4887,
	SPR_SELECT_TOYLAND             = 4888,
	SPR_SELECT_TOYLAND_PUSHED      = 4889,
};

/** Cursor sprite numbers */
enum CursorSprite {
	/* Terraform */
	/* Cursors */
	SPR_CURSOR_MOUSE          = 0,
	SPR_CURSOR_ZZZ            = 1,
	SPR_CURSOR_BOUY           = 702,
	SPR_CURSOR_QUERY          = 719,
	SPR_CURSOR_HQ             = 720,
	SPR_CURSOR_SHIP_DEPOT     = 721,
	SPR_CURSOR_SIGN           = 722,

	SPR_CURSOR_TREE           = 2010,
	SPR_CURSOR_BUY_LAND       = 4792,
	SPR_CURSOR_LEVEL_LAND     = SPR_OPENTTD_BASE + 62,

	SPR_CURSOR_TOWN           = 4080,
	SPR_CURSOR_INDUSTRY       = 4081,
	SPR_CURSOR_ROCKY_AREA     = 4087,
	SPR_CURSOR_LIGHTHOUSE     = 4088,
	SPR_CURSOR_TRANSMITTER    = 4089,

	/* airport cursors */
	SPR_CURSOR_AIRPORT        = 2724,

	/* dock cursors */
	SPR_CURSOR_DOCK           = 3668,
	SPR_CURSOR_CANAL          = SPR_OPENTTD_BASE + 8,
	SPR_CURSOR_LOCK           = SPR_OPENTTD_BASE + 57,

	/* shared road & rail cursors */
	SPR_CURSOR_BRIDGE         = 2593,

	/* rail cursors */
	SPR_CURSOR_NS_TRACK       = 1263,
	SPR_CURSOR_SWNE_TRACK     = 1264,
	SPR_CURSOR_EW_TRACK       = 1265,
	SPR_CURSOR_NWSE_TRACK     = 1266,

	SPR_CURSOR_NS_MONO        = 1267,
	SPR_CURSOR_SWNE_MONO      = 1268,
	SPR_CURSOR_EW_MONO        = 1269,
	SPR_CURSOR_NWSE_MONO      = 1270,

	SPR_CURSOR_NS_MAGLEV      = 1271,
	SPR_CURSOR_SWNE_MAGLEV    = 1272,
	SPR_CURSOR_EW_MAGLEV      = 1273,
	SPR_CURSOR_NWSE_MAGLEV    = 1274,

	SPR_CURSOR_NS_ELRAIL      = SPR_ELRAIL_BASE + 43,
	SPR_CURSOR_SWNE_ELRAIL    = SPR_ELRAIL_BASE + 44,
	SPR_CURSOR_EW_ELRAIL      = SPR_ELRAIL_BASE + 45,
	SPR_CURSOR_NWSE_ELRAIL    = SPR_ELRAIL_BASE + 46,

	SPR_CURSOR_RAIL_STATION   = 1300,

	SPR_CURSOR_TUNNEL_RAIL    = 2434,
	SPR_CURSOR_TUNNEL_ELRAIL  = SPR_ELRAIL_BASE + 48,
	SPR_CURSOR_TUNNEL_MONO    = 2435,
	SPR_CURSOR_TUNNEL_MAGLEV  = 2436,

	SPR_CURSOR_AUTORAIL       = SPR_OPENTTD_BASE + 4,
	SPR_CURSOR_AUTOMONO       = SPR_OPENTTD_BASE + 5,
	SPR_CURSOR_AUTOMAGLEV     = SPR_OPENTTD_BASE + 6,

	SPR_CURSOR_WAYPOINT       = SPR_OPENTTD_BASE + 7,

	SPR_CURSOR_RAIL_DEPOT     = 1296,
	SPR_CURSOR_MONO_DEPOT     = SPR_OPENTTD_BASE + 11,
	SPR_CURSOR_MAGLEV_DEPOT   = SPR_OPENTTD_BASE + 12,

	SPR_CURSOR_CONVERT_RAIL   = SPR_OPENTTD_BASE + 23,
	SPR_CURSOR_CONVERT_MONO   = SPR_OPENTTD_BASE + 25,
	SPR_CURSOR_CONVERT_MAGLEV = SPR_OPENTTD_BASE + 27,

	/* road cursors */
	SPR_CURSOR_ROAD_NESW      = 1311,
	SPR_CURSOR_ROAD_NWSE      = 1312,
	SPR_CURSOR_TRAMWAY_NESW   = SPR_TRAMWAY_BASE + 2,
	SPR_CURSOR_TRAMWAY_NWSE   = SPR_TRAMWAY_BASE + 3,

	SPR_CURSOR_ROAD_DEPOT     = 1297,
	SPR_CURSOR_BUS_STATION    = 2725,
	SPR_CURSOR_TRUCK_STATION  = 2726,
	SPR_CURSOR_ROAD_TUNNEL    = 2433,

	SPR_CURSOR_CLONE_TRAIN    = SPR_OPENTTD_BASE +  92,
	SPR_CURSOR_CLONE_ROADVEH  = SPR_OPENTTD_BASE + 109,
	SPR_CURSOR_CLONE_SHIP     = SPR_OPENTTD_BASE + 111,
	SPR_CURSOR_CLONE_AIRPLANE = SPR_OPENTTD_BASE + 113,
};

/// Animation macro in table/animcursors.h (_animcursors[])
enum AnimCursors {
	ANIMCURSOR_DEMOLISH     = -1, ///<  704 -  707 - demolish dynamite
	ANIMCURSOR_LOWERLAND    = -2, ///<  699 -  701 - lower land tool
	ANIMCURSOR_RAISELAND    = -3, ///<  696 -  698 - raise land tool
	ANIMCURSOR_PICKSTATION  = -4, ///<  716 -  718 - goto-order icon
	ANIMCURSOR_BUILDSIGNALS = -5, ///< 1292 - 1293 - build signal
};

/**
 * Bitmask setup. For the graphics system, 32 bits are used to define
 * the sprite to be displayed. This variable contains various information:<p>
 * <ul><li> SPRITE_WIDTH is the number of bits used for the actual sprite to be displayed.
 * This always starts at bit 0.</li>
 * <li> TRANSPARENT_BIT is the bit number which toggles sprite transparency</li>
 * <li> RECOLOR_BIT toggles the recoloring system</li>
 * <li> PALETTE_SPRITE_WIDTH and PALETTE_SPRITE_START determine the position and number of
 * bits used for the recoloring process. For transparency, it must be 0x322.</li></ul>
 */
enum SpriteSetup {
	TRANSPARENT_BIT = 31,       ///< toggles transparency in the sprite
	RECOLOR_BIT = 30,           ///< toggles recoloring in the sprite
	OFFSET_BIT = 29,

	PALETTE_WIDTH = 24,         ///< number of bits of the sprite containing the recolor palette
	SPRITE_WIDTH = 24,          ///< number of bits for the sprite number
};

/**
 * these masks change the colors of the palette for a sprite.
 * Apart from this bit, a sprite number is needed to define
 * the palette used for recoloring. This palette is stored
 * in the bits marked by PALETTE_SPRITE_MASK.
 * @note Do not modify this enum. Alter SpriteSetup instead
 * @see SpriteSetup
 */
enum Modifiers {
	SPRITE_MODIFIER_USE_OFFSET    = OFFSET_BIT,
	SPRITE_MODIFIER_OPAQUE        = OFFSET_BIT,
	///when a sprite is to be displayed transparently, this bit needs to be set.
	PALETTE_MODIFIER_TRANSPARENT  = TRANSPARENT_BIT,
	///this bit is set when a recoloring process is in action
	PALETTE_MODIFIER_COLOR        = RECOLOR_BIT,

	//This is used for the GfxFillRect function
	///Used to draw a "grey out" rectangle. @see GfxFillRect
	PALETTE_MODIFIER_GREYOUT        = TRANSPARENT_BIT,
	///Set when a colortable mode is used. @see GfxFillRect
	USE_COLORTABLE                  = RECOLOR_BIT,
};

/** Masks needed for sprite operations.
 * @note Do not modify this enum. Alter SpriteSetup instead
 * @see SpriteSetup */
enum SpriteMasks {
	///Maximum number of sprites that can be loaded at a given time.
	MAX_SPRITES = 1 << SPRITE_WIDTH,
	///The mask to for the main sprite
	SPRITE_MASK = MAX_SPRITES - 1,

	MAX_PALETTES = 1 << PALETTE_WIDTH,
	///The mask for the auxiliary sprite (the one that takes care of recoloring)
	PALETTE_MASK = MAX_PALETTES - 1,
};

assert_compile( (1 << TRANSPARENT_BIT & SPRITE_MASK) == 0 );
assert_compile( (1 << RECOLOR_BIT & SPRITE_MASK) == 0 );
assert_compile( !(TRANSPARENT_BIT == RECOLOR_BIT) );
assert_compile( (1 << TRANSPARENT_BIT & PALETTE_MASK) == 0);
assert_compile( (1 << RECOLOR_BIT & PALETTE_MASK) == 0 );

enum Recoloring {
	PALETTE_RECOLOR_START       = 0x307,
};


static const SpriteID PALETTE_CRASH = 0x324;
static const SpriteID PAL_NONE = 0;

	//note: these numbers are already the modified once the renderer needs.
	//the actual sprite number is the upper 16 bits of the number

	///Here a puslating red tile is drawn if you try to build a wrong tunnel or raise/lower land where it is not possible
static const SpriteID PALETTE_TILE_RED_PULSATING  = 0x303;
	///makes a square red. is used when removing rails or other stuff
static const SpriteID PALETTE_SEL_TILE_RED        = 0x304;
	///This draws a blueish square (catchment areas for example)
static const SpriteID PALETTE_SEL_TILE_BLUE       = 0x305;
	//0x306 is a real sprite (the little dot you get when you try to raise/lower a corner of the map
	//here the color switches begin
	//use this if you add stuff to the value, so that the resulting color
	//is not a fixed value.
	//NOTE THAT THE SWITCH 0x8000 is NOT present in _TO_COLORS yet!
enum PaletteSprites {
	PALETTE_TO_DARK_BLUE        = 0x307,
	PALETTE_TO_PALE_GREEN       = 0x308,
	PALETTE_TO_PINK             = 0x309,
	PALETTE_TO_YELLOW           = 0x30A,
	PALETTE_TO_RED              = 0x30B,
	PALETTE_TO_LIGHT_BLUE       = 0x30C,
	PALETTE_TO_GREEN            = 0x30D,
	PALETTE_TO_DARK_GREEN       = 0x30E,
	PALETTE_TO_BLUE             = 0x30F,
	PALETTE_TO_CREAM            = 0x310,
	//maybe don't use as player color because it doesn't display in the graphs?
	PALETTE_TO_MAUVE            = 0x311,
	PALETTE_TO_PURPLE           = 0x312,
	PALETTE_TO_ORANGE           = 0x313,
	PALETTE_TO_BROWN            = 0x314,
	PALETTE_TO_GREY             = 0x315,
	PALETTE_TO_WHITE            = 0x316,
	//sets color to bare land stuff, for rail and road (and crossings)
	PALETTE_TO_BARE_LAND        = 0x317,
	//XXX is 318-31A really not used?
	PALETTE_TO_STRUCT_BLUE      = 0x31B,
	//structure color to something brownish (for the cantilever bridges for example)
	PALETTE_TO_STRUCT_BROWN     = 0x31C,
	PALETTE_TO_STRUCT_WHITE     = 0x31D,
	//sets bridge or structure to red, little concrete one and cantilever use this one for example
	PALETTE_TO_STRUCT_RED       = 0x31E,
	PALETTE_TO_STRUCT_GREEN     = 0x31F,
	PALETTE_TO_STRUCT_CONCRETE  = 0x320, //Sets the suspension bridge to concrete, also other strucutures use it
	PALETTE_TO_STRUCT_YELLOW    = 0x321, //Sets the bridge color to yellow (suspension and tubular)
	PALETTE_TO_TRANSPARENT      = 0x322, //This sets the sprite to transparent
	//This is used for changing the tubular bridges to the silicon display, or some grayish color
	PALETTE_TO_STRUCT_GREY      = 0x323,

	//XXX - const - PALETTE_CRASH               = 0x324,  //this changes stuff to the "crash color"
	//XXX another place where structures are colored.
	//I'm not sure which colors these are
	PALETTE_59E                 = 0x59E,
	PALETTE_59F                 = 0x59F,
};

#endif /* SPRITES_H */
