#ifndef SPRITES_H
#define SPRITES_H

/* NOTE:
	ALL SPRITE NUMBERS BELOW 5126 are in the main files
	SPR_CANALS_BASE is in canalsw.grf
	SPR_SLOPES_BASE is in trkfoundw.grf
	SPR_OPENTTD_BASE is in openttd.grf
*/

/*
	All elements which consist of two elements should
	have the same name and then suffixes
		_GROUND and _BUILD for building-type sprites
		_REAR and _FRONT for transport-type sprites (tiles where vehicles are on)
	These sprites are split because of the Z order of the elements
		(like some parts of a bridge are behind the vehicle, while others are before)
*/

/*
	All sprites which are described here are referenced only one to a handful of times
	throughout the code. When introducing new sprite enums, use meaningful names.
	Don't be lazy and typing, and only use abbrevations when their meaning is clear or
	the length of the enum would get out of hand. In that case EXPLAIN THE ABBREVATION
	IN THIS FILE, and perhaps add some comments in the code where it is used.
	Now, don't whine about this being too much typing work if the enums are like
	30 characters in length. If your editor doen't help you simplifying your work,
	get a proper editor. If your Operating Systems don't have any decent editors,
	get a proper Operating System.
*/


enum Sprites {
	SPR_SELECT_TILE	 = 752,
	SPR_DOT          = 774, // corner marker for lower/raise land
	SPR_DOT_SMALL    = 4078,
	SPR_WHITE_POINT  = 4079,

	/* ASCII */
	SPR_ASCII_SPACE       = 2,
	SPR_ASCII_SPACE_SMALL = 226,
	SPR_ASCII_SPACE_BIG   = 450,

	/* Extra graphic spritenumbers */
	SPR_CANALS_BASE		= 5126,
	SPR_SLOPES_BASE		= SPR_CANALS_BASE + 70,
	SPR_OPENTTD_BASE	= SPR_SLOPES_BASE + 74,		//5270

	SPR_BLOT = SPR_OPENTTD_BASE + 32, // colored circle (mainly used as vehicle profit marker and for sever compatibility)

	SPR_PIN_UP = SPR_OPENTTD_BASE + 62,   // pin icon 
	SPR_PIN_DOWN = SPR_OPENTTD_BASE + 63,
	
	
	/* Network GUI sprites */
	SPR_SQUARE = SPR_OPENTTD_BASE + 23,     // colored square (used for newgrf compatibility)
	SPR_LOCK = SPR_OPENTTD_BASE + 22,       // lock icon (for password protected servers)
	SPR_FLAGS_BASE = SPR_OPENTTD_BASE + 90, // start of the flags block (in same order as enum NetworkLanguage)

	/* Manager face sprites */
	SPR_GRADIENT = 874, // background gradient behind manager face

	/* Shadow cell */
	SPR_SHADOW_CELL = 1004,
	
	/* Sliced view shadow cells */
	/* Maybe we have differen ones in the future */
	SPR_MAX_SLICE = SPR_OPENTTD_BASE + 71,
	SPR_MIN_SLICE = SPR_OPENTTD_BASE + 71,

	/* Unmovables spritenumbers */
	SPR_UNMOVABLE_TRANSMITTER 	= 2601,
	SPR_UNMOVABLE_LIGHTHOUSE		= 2602,
	SPR_TINYHQ_NORTH						= 2603,
	SPR_TINYHQ_EAST							= 2604,
	SPR_TINYHQ_WEST							= 2605,
	SPR_TINYHQ_SOUTH						= 2606,
	SPR_SMALLHQ_NORTH						= 2607,
	SPR_SMALLHQ_EAST						= 2608,
	SPR_SMALLHQ_WEST						= 2609,
	SPR_SMALLHQ_SOUTH						= 2610,
	SPR_MEDIUMHQ_NORTH					= 2611,
	SPR_MEDIUMHQ_NORTH_WALL			= 2612,
	SPR_MEDIUMHQ_EAST						= 2613,
	SPR_MEDIUMHQ_EAST_WALL			= 2614,
	SPR_MEDIUMHQ_WEST						= 2615,
	SPR_MEDIUMHQ_WEST_WALL			= 2616,	//very tiny piece of wall
	SPR_MEDIUMHQ_SOUTH					= 2617,
	SPR_LARGEHQ_NORTH_GROUND		= 2618,
	SPR_LARGEHQ_NORTH_BUILD			= 2619,
	SPR_LARGEHQ_EAST_GROUND			= 2620,
	SPR_LARGEHQ_EAST_BUILD			= 2621,
	SPR_LARGEHQ_WEST_GROUND			= 2622,
	SPR_LARGEHQ_WEST_BUILD			= 2623,
	SPR_LARGEHQ_SOUTH						= 2624,
	SPR_HUGEHQ_NORTH_GROUND			= 2625,
	SPR_HUGEHQ_NORTH_BUILD			= 2626,
	SPR_HUGEHQ_EAST_GROUND			= 2627,
	SPR_HUGEHQ_EAST_BUILD				=	2628,
	SPR_HUGEHQ_WEST_GROUND			= 2629,
	SPR_HUGEHQ_WEST_BUILD				= 2630,
	SPR_HUGEHQ_SOUTH						= 2631,
	SPR_STATUE_GROUND						= 1420,
	SPR_STATUE_COMPANY					=	2623,
	SPR_BOUGHT_LAND							= 4790,

	/* sprites for rail and rail stations*/
	SPR_RAIL_TRACK_Y						= 1011,
	SPR_RAIL_TRACK_X						= 1012,
	SPR_RAIL_TRACK_Y_SNOW				= 1037,
	SPR_RAIL_TRACK_X_SNOW				= 1038,
	SPR_RAIL_DEPOT_SE_1					= 1063,
	SPR_RAIL_DEPOT_SE_2					= 1064,
	SPR_RAIL_DEPOT_SW_1					= 1065,
	SPR_RAIL_DEPOT_SW_2					= 1066,
	SPR_RAIL_DEPOT_NE						= 1067,
	SPR_RAIL_DEPOT_NW						= 1068,
	SPR_RAIL_PLATFORM_Y_FRONT					= 1069,
	SPR_RAIL_PLATFORM_X_REAR					= 1070,
	SPR_RAIL_PLATFORM_Y_REAR					= 1071,
	SPR_RAIL_PLATFORM_X_FRONT					= 1072,
	SPR_RAIL_PLATFORM_BUILDING_X			= 1073,
	SPR_RAIL_PLATFORM_BUILDING_Y			= 1074,
	SPR_RAIL_PLATFORM_PILLARS_Y_FRONT	= 1075,
	SPR_RAIL_PLATFORM_PILLARS_X_REAR	= 1076,
	SPR_RAIL_PLATFORM_PILLARS_Y_REAR	= 1077,
	SPR_RAIL_PLATFORM_PILLARS_X_FRONT	= 1078,
	SPR_RAIL_ROOF_STRUCTURE_X_TILE_A	= 1079,	//First half of the roof structure
	SPR_RAIL_ROOF_STRUCTURE_Y_TILE_A	= 1080,
	SPR_RAIL_ROOF_STRUCTURE_X_TILE_B	= 1081,	//Second half of the roof structure
	SPR_RAIL_ROOF_STRUCTURE_Y_TILE_B	= 1082,
	SPR_RAIL_ROOF_GLASS_X_TILE_A			= 1083,	//First half of the roof glass
	SPR_RAIL_ROOF_GLASS_Y_TILE_A			= 1084,
	SPR_RAIL_ROOF_GLASS_X_TILE_B			= 1085,	//second half of the roof glass
	SPR_RAIL_ROOF_GLASS_Y_TILE_B			= 1086,
	SPR_CHECKPOINT_X_1					= SPR_OPENTTD_BASE + 18,
	SPR_CHECKPOINT_X_2					= SPR_OPENTTD_BASE + 19,
	SPR_CHECKPOINT_Y_1					= SPR_OPENTTD_BASE + 20,
	SPR_CHECKPOINT_Y_2					= SPR_OPENTTD_BASE + 21,
	OFFSET_TILEH_IMPOSSIBLE			= 0,
	OFFSET_TILEH_1							= 14,
	OFFSET_TILEH_2							= 15,
	OFFSET_TILEH_3							= 22,
	OFFSET_TILEH_4							= 13,
	OFFSET_TILEH_6							= 21,
	OFFSET_TILEH_7							= 17,
	OFFSET_TILEH_8							= 12,
	OFFSET_TILEH_9							= 23,
	OFFSET_TILEH_11							= 18,
	OFFSET_TILEH_12							= 20,
	OFFSET_TILEH_13							= 19,
	OFFSET_TILEH_14							= 16,

	/* sprites for airports and airfields*/
	/* Small airports are AIRFIELD, everything else is AIRPORT */
	SPR_HELIPORT										= 2633,
	SPR_AIRPORT_APRON								= 2634,
	SPR_AIRPORT_AIRCRAFT_STAND			= 2635,
	SPR_AIRPORT_TAXIWAY_NS_WEST			= 2636,
	SPR_AIRPORT_TAXIWAY_EW_SOUTH		=	2637,
	SPR_AIRPORT_TAXIWAY_XING_SOUTH	= 2638,
	SPR_AIRPORT_TAXIWAY_XING_WEST		= 2639,
	SPR_AIRPORT_TAXIWAY_NS_CTR			= 2640,
	SPR_AIRPORT_TAXIWAY_XING_EAST		= 2641,
	SPR_AIRPORT_TAXIWAY_NS_EAST			= 2642,
	SPR_AIRPORT_TAXIWAY_EW_NORTH		= 2643,
	SPR_AIRPORT_TAXIWAY_EW_CTR			= 2644,
	SPR_AIRPORT_RUNWAY_EXIT_A				= 2645,
	SPR_AIRPORT_RUNWAY_EXIT_B				= 2646,
	SPR_AIRPORT_RUNWAY_EXIT_C				= 2647,
	SPR_AIRPORT_RUNWAY_EXIT_D				= 2648,
	SPR_AIRPORT_RUNWAY_END					= 2649,	//We should have different ends
	SPR_AIRPORT_TERMINAL_A					= 2650,
	SPR_AIRPORT_TOWER								= 2651,
	SPR_AIRPORT_CONCOURSE						= 2652,
	SPR_AIRPORT_TERMINAL_B					= 2653,
	SPR_AIRPORT_TERMINAL_C					= 2654,
	SPR_AIRPORT_HANGAR_FRONT				= 2655,
	SPR_AIRPORT_HANGAR_REAR					= 2656,
	SPR_AIRFIELD_HANGAR_FRONT				= 2657,
	SPR_AIRFIELD_HANGAR_REAR				= 2658,
	SPR_AIRPORT_JETWAY_1						= 2659,
	SPR_AIRPORT_JETWAY_2						= 2660,
	SPR_AIRPORT_JETWAY_3						= 2661,
	SPR_AIRPORT_PASSENGER_TUNNEL		= 2662,
	SPR_AIRPORT_FENCE_Y							= 2663,
	SPR_AIRPORT_FENCE_X							= 2664,
	SPR_AIRFIELD_TERM_A							= 2665,
	SPR_AIRFIELD_TERM_B							= 2666,
	SPR_AIRFIELD_TERM_C_GROUND			= 2667,
	SPR_AIRFIELD_TERM_C_BUILD				= 2668,
	SPR_AIRFIELD_APRON_A						= 2669,
	SPR_AIRFIELD_APRON_B						= 2670,
	SPR_AIRFIELD_APRON_C						= 2671,
	SPR_AIRFIELD_APRON_D						= 2672,
	SPR_AIRFIELD_RUNWAY_NEAR_END		= 2673,
	SPR_AIRFIELD_RUNWAY_MIDDLE			= 2674,
	SPR_AIRFIELD_RUNWAY_FAR_END			= 2675,
	SPR_AIRFIELD_WIND_1							= 2676,
	SPR_AIRFIELD_WIND_2							= 2677,
	SPR_AIRFIELD_WIND_3							= 2678,
	SPR_AIRFIELD_WIND_4							= 2679,
	SPR_AIRPORT_RADAR_1							= 2680,
	SPR_AIRPORT_RADAR_2							= 2681,
	SPR_AIRPORT_RADAR_3							= 2682,
	SPR_AIRPORT_RADAR_4							= 2683,
	SPR_AIRPORT_RADAR_5							= 2684,
	SPR_AIRPORT_RADAR_6							= 2685,
	SPR_AIRPORT_RADAR_7							= 2686,
	SPR_AIRPORT_RADAR_8							= 2687,
	SPR_AIRPORT_RADAR_9							= 2688,
	SPR_AIRPORT_RADAR_A							= 2689,
	SPR_AIRPORT_RADAR_B							= 2690,
	SPR_AIRPORT_RADAR_C							= 2691,
	SPR_AIRPORT_HELIPAD							= SPR_OPENTTD_BASE + 31,

	/* Road Stops */
	/* Road stops have a ground tile and 3 buildings, one on each side
			(except the side where the entry is). These are marked _A _B and _C
	*/
	SPR_BUS_STOP_NE_GROUND					= 2692,
	SPR_BUS_STOP_SE_GROUND					= 2693,
	SPR_BUS_STOP_SW_GROUND					= 2694,
	SPR_BUS_STOP_NW_GROUND					= 2695,
	SPR_BUS_STOP_NE_BUILD_A					= 2696,
	SPR_BUS_STOP_SE_BUILD_A					= 2697,
	SPR_BUS_STOP_SW_BUILD_A					= 2698,
	SPR_BUS_STOP_NW_BUILD_A					= 2699,
	SPR_BUS_STOP_NE_BUILD_B					= 2700,
	SPR_BUS_STOP_SE_BUILD_B					= 2701,
	SPR_BUS_STOP_SW_BUILD_B					= 2702,
	SPR_BUS_STOP_NW_BUILD_B					= 2703,
	SPR_BUS_STOP_NE_BUILD_C					= 2704,
	SPR_BUS_STOP_SE_BUILD_C					= 2705,
	SPR_BUS_STOP_SW_BUILD_C					= 2706,
	SPR_BUS_STOP_NW_BUILD_C					= 2707,
	SPR_TRUCK_STOP_NE_GROUND				= 2708,
	SPR_TRUCK_STOP_SE_GROUND				= 2709,
	SPR_TRUCK_STOP_SW_GROUND				= 2710,
	SPR_TRUCK_STOP_NW_GROUND				= 2711,
	SPR_TRUCK_STOP_NE_BUILD_A				= 2712,
	SPR_TRUCK_STOP_SE_BUILD_A				= 2713,
	SPR_TRUCK_STOP_SW_BUILD_A				= 2714,
	SPR_TRUCK_STOP_NW_BUILD_A				= 2715,
	SPR_TRUCK_STOP_NE_BUILD_B				= 2716,
	SPR_TRUCK_STOP_SE_BUILD_B				= 2717,
	SPR_TRUCK_STOP_SW_BUILD_B				= 2718,
	SPR_TRUCK_STOP_NW_BUILD_B				= 2719,
	SPR_TRUCK_STOP_NE_BUILD_C				= 2720,
	SPR_TRUCK_STOP_SE_BUILD_C				= 2721,
	SPR_TRUCK_STOP_SW_BUILD_C				= 2722,
	SPR_TRUCK_STOP_NW_BUILD_C				= 2723,

	/* Sprites for docks */
	/* Docks consist of two tiles, the sloped one and the flat one */
	SPR_DOCK_SLOPE_NE								= 2727,
	SPR_DOCK_SLOPE_SE								= 2728,
	SPR_DOCK_SLOPE_SW								= 2729,
	SPR_DOCK_SLOPE_NW								= 2730,
	SPR_DOCK_FLAT_X 								= 2731,	//for NE and SW
	SPR_DOCK_FLAT_Y									= 2732,	//for NW and SE
	SPR_BUOY												= 4076,	//XXX this sucks, because it displays wrong stuff on canals

	/* Sprites for road */
	SPR_ROAD_Y									= 1332,
	SPR_ROAD_X									= 1333,
	SPR_ROAD_Y_SNOW							= 1351,
	SPR_ROAD_X_SNOW							= 1352,

	/* Landscape sprites */
	SPR_FLAT_BARE_LAND					= 3924,
	SPR_FLAT_1_THIRD_GRASS_TILE	= 3943,
	SPR_FLAT_2_THIRD_GRASS_TILE	= 3962,
	SPR_FLAT_GRASS_TILE					= 3981,
	SPR_FLAT_ROUGH_LAND					= 4000,
	SPR_FLAT_ROUGH_LAND_1				= 4019,
	SPR_FLAT_ROUGH_LAND_2				= 4020,
	SPR_FLAT_ROUGH_LAND_3				= 4021,
	SPR_FLAT_ROUGH_LAND_4				= 4022,
	SPR_FLAT_ROCKY_LAND_1				= 4023,
	SPR_FLAT_ROCKY_LAND_2				= 4042,
	SPR_FLAT_WATER_TILE					= 4061,
	SPR_FLAT_1_QUART_SNOWY_TILE	= 4493,
	SPR_FLAT_2_QUART_SNOWY_TILE	= 4512,
	SPR_FLAT_3_QUART_SNOWY_TILE	= 4531,
	SPR_FLAT_SNOWY_TILE					= 4550,

	/* Hedge, Farmland-fence sprites */
	SPR_HEDGE_BUSHES						= 4090,
	SPR_HEDGE_BUSHES_WITH_GATE	= 4096,
	SPR_HEDGE_FENCE							= 4102,
	SPR_HEDGE_BLOOMBUSH_YELLOW	= 4108,
	SPR_HEDGE_BLOOMBUSH_RED			= 4114,
	SPR_HEDGE_STONE							= 4120,

	/* Farmland sprites, only flat tiles listed, various stages */
	SPR_FARMLAND_BARE						= 4126,
	SPR_FARMLAND_STATE_1				= 4145,
	SPR_FARMLAND_STATE_2				= 4164,
	SPR_FARMLAND_STATE_3				= 4183,
	SPR_FARMLAND_STATE_4				= 4202,
	SPR_FARMLAND_STATE_5				= 4221,
	SPR_FARMLAND_STATE_6				= 4240,
	SPR_FARMLAND_STATE_7				= 4259,
	SPR_FARMLAND_HAYPACKS				= 4278,

	/* Shores */
	SPR_NO_SHORE								= 0,	//used for tileh which have no shore
	SPR_SHORE_TILEH_4						= 4062,
	SPR_SHORE_TILEH_1						= 4063,
	SPR_SHORE_TILEH_2						= 4064,
	SPR_SHORE_TILEH_8						= 4065,
	SPR_SHORE_TILEH_6						= 4066,
	SPR_SHORE_TILEH_12					= 4067,
	SPR_SHORE_TILEH_3						= 4068,
	SPR_SHORE_TILEH_9						= 4069,

	/* Water-related sprites */
	SPR_SHIP_DEPOT_SE_FRONT			= 4070,
	SPR_SHIP_DEPOT_SW_FRONT			= 4071,
	SPR_SHIP_DEPOT_NW						= 4072,
	SPR_SHIP_DEPOT_NE						= 4073,
	SPR_SHIP_DEPOT_SE_REAR			= 4074,
	SPR_SHIP_DEPOT_SW_REAR			= 4075,
	//here come sloped water sprites
	SPR_WATER_SLOPE_Y_UP				= SPR_CANALS_BASE + 5, //Water flowing negative Y direction
	SPR_WATER_SLOPE_X_DOWN			= SPR_CANALS_BASE + 6, //positive X
	SPR_WATER_SLOPE_X_UP				= SPR_CANALS_BASE + 7, //negative X
	SPR_WATER_SLOPE_Y_DOWN			= SPR_CANALS_BASE + 8,	//positive Y
	//sprites for the shiplifts
	//there are 4 kinds of shiplifts, each of them is 3 tiles long.
	//the four kinds are running in the X and Y direction and
	//are "lowering" either in the "+" or the "-" direction.
	//the three tiles are the center tile (where the slope is)
	//and a bottom and a top tile
	SPR_SHIPLIFT_Y_UP_CENTER_REAR			= SPR_CANALS_BASE + 9,
	SPR_SHIPLIFT_X_DOWN_CENTER_REAR		=	SPR_CANALS_BASE + 10,
	SPR_SHIPLIFT_X_UP_CENTER_REAR			= SPR_CANALS_BASE + 11,
	SPR_SHIPLIFT_Y_DOWN_CENTER_REAR		= SPR_CANALS_BASE + 12,
	SPR_SHIPLIFT_Y_UP_CENTER_FRONT		= SPR_CANALS_BASE + 13,
	SPR_SHIPLIFT_X_DOWN_CENTER_FRONT 	= SPR_CANALS_BASE + 14,
	SPR_SHIPLIFT_X_UP_CENTER_FRONT		= SPR_CANALS_BASE + 15,
	SPR_SHIPLIFT_Y_DOWN_CENTER_FRONT	= SPR_CANALS_BASE + 16,
	SPR_SHIPLIFT_Y_UP_BOTTOM_REAR			= SPR_CANALS_BASE + 17,
	SPR_SHIPLIFT_X_DOWN_BOTTOM_REAR		= SPR_CANALS_BASE + 18,
	SPR_SHIPLIFT_X_UP_BOTTOM_REAR			= SPR_CANALS_BASE + 19,
	SPR_SHIPLIFT_Y_DOWN_BOTTOM_REAR		= SPR_CANALS_BASE + 20,
	SPR_SHIPLIFT_Y_UP_BOTTOM_FRONT		= SPR_CANALS_BASE + 21,
	SPR_SHIPLIFT_X_DOWN_BOTTOM_FRONT	= SPR_CANALS_BASE + 22,
	SPR_SHIPLIFT_X_UP_BOTTOM_FRONT		= SPR_CANALS_BASE + 23,
	SPR_SHIPLIFT_Y_DOWN_BOTTOM_FRONT	= SPR_CANALS_BASE + 24,
	SPR_SHIPLIFT_Y_UP_TOP_REAR			  = SPR_CANALS_BASE + 25,
	SPR_SHIPLIFT_X_DOWN_TOP_REAR			= SPR_CANALS_BASE + 26,
	SPR_SHIPLIFT_X_UP_TOP_REAR				= SPR_CANALS_BASE + 27,
	SPR_SHIPLIFT_Y_DOWN_TOP_REAR			= SPR_CANALS_BASE + 28,
	SPR_SHIPLIFT_Y_UP_TOP_FRONT				= SPR_CANALS_BASE + 29,
	SPR_SHIPLIFT_X_DOWN_TOP_FRONT			= SPR_CANALS_BASE + 30,
	SPR_SHIPLIFT_X_UP_TOP_FRONT				= SPR_CANALS_BASE + 31,
	SPR_SHIPLIFT_Y_DOWN_TOP_FRONT			= SPR_CANALS_BASE + 32,

	/* Sprites for tunnels and bridges */
	SPR_TUNNEL_ENTRY_REAR_RAIL	= 2365,
	SPR_TUNNEL_ENTRY_REAR_ROAD	= 2389,

		/* bridge type sprites */

	/* Wooden bridge (type 0) */
	SPR_BTWDN_RAIL_Y_REAR				= 2545,
	SPR_BTWDN_RAIL_X_REAR				= 2546,
	SPR_BTWDN_ROAD_Y_REAR				= 2547,
	SPR_BTWDN_ROAD_X_REAR				= 2548,
	SPR_BTWDN_Y_FRONT						= 2549,
	SPR_BTWDN_X_FRONT						= 2550,
	SPR_BTWDN_Y_PILLAR					= 2551,
	SPR_BTWDN_X_PILLAR					= 2552,
	SPR_BTWDN_MONO_Y_REAR				= 4361,
	SPR_BTWDN_MONO_X_REAR				= 4362,
	SPR_BTWDN_MGLV_Y_REAR				= 4400,
	SPR_BTWDN_MGLV_X_REAR				= 4401,
	/* ramps */
	SPR_BTWDN_ROAD_RAMP_Y_DOWN	= 2529,
	SPR_BTWDN_ROAD_RAMP_X_DOWN	= 2530,
	SPR_BTWDN_ROAD_RAMP_X_UP		= 2531,	//for some weird reason the order is swapped
	SPR_BTWDN_ROAD_RAMP_Y_UP		= 2532,	//between X and Y.
	SPR_BTWDN_ROAD_Y_SLOPE_UP		= 2533,
	SPR_BTWDN_ROAD_X_SLOPE_UP		= 2534,
	SPR_BTWDN_ROAD_Y_SLOPE_DOWN	= 2535,
	SPR_BTWDN_ROAD_X_SLOPE_DOWN = 2536,
	SPR_BTWDN_RAIL_RAMP_Y_DOWN	= 2537,
	SPR_BTWDN_RAIL_RAMP_X_DOWN	= 2538,
	SPR_BTWDN_RAIL_RAMP_X_UP		= 2539,	//for some weird reason the order is swapped
	SPR_BTWDN_RAIL_RAMP_Y_UP		= 2540,	//between X and Y.
	SPR_BTWDN_RAIL_Y_SLOPE_UP		= 2541,
	SPR_BTWDN_RAIL_X_SLOPE_UP		= 2542,
	SPR_BTWDN_RAIL_Y_SLOPE_DOWN	= 2543,
	SPR_BTWDN_RAIL_X_SLOPE_DOWN = 2544,
	SPR_BTWDN_MONO_RAMP_Y_DOWN	= 4352,
	SPR_BTWDN_MONO_RAMP_X_DOWN	= 4353,
	SPR_BTWDN_MONO_RAMP_X_UP		= 4354,	//for some weird reason the order is swapped
	SPR_BTWDN_MONO_RAMP_Y_UP		= 4355,	//between X and Y.
	SPR_BTWDN_MONO_Y_SLOPE_UP		= 4356,
	SPR_BTWDN_MONO_X_SLOPE_UP		= 4357,
	SPR_BTWDN_MONO_Y_SLOPE_DOWN	= 4358,
	SPR_BTWDN_MONO_X_SLOPE_DOWN = 4359,
	SPR_BTWDN_MGLV_RAMP_Y_DOWN	= 4392,
	SPR_BTWDN_MGLV_RAMP_X_DOWN	= 4393,
	SPR_BTWDN_MGLV_RAMP_X_UP		= 4394,	//for some weird reason the order is swapped
	SPR_BTWDN_MGLV_RAMP_Y_UP		= 4395,	//between X and Y.
	SPR_BTWDN_MGLV_Y_SLOPE_UP		= 4396,
	SPR_BTWDN_MGLV_X_SLOPE_UP		= 4397,
	SPR_BTWDN_MGLV_Y_SLOPE_DOWN	= 4398,
	SPR_BTWDN_MGLV_X_SLOPE_DOWN = 4399,

	/* Steel Girder with arches */
	/* BTSGA == Bridge Type Steel Girder Arched */
	/* This is bridge type number 2 */
	SPR_BTSGA_RAIL_X_REAR				= 2499,
	SPR_BTSGA_RAIL_Y_REAR				= 2500,
	SPR_BTSGA_ROAD_X_REAR				= 2501,
	SPR_BTSGA_ROAD_Y_REAR				= 2502,
	SPR_BTSGA_X_FRONT						= 2503,
	SPR_BTSGA_Y_FRONT						= 2504,
	SPR_BTSGA_X_PILLAR					= 2505,
	SPR_BTSGA_Y_PILLAR					= 2606,
	SPR_BTSGA_MONO_X_REAR				= 4324,
	SPR_BTSGA_MONO_Y_REAR				= 4325,
	SPR_BTSGA_MGLV_X_REAR				= 4364,
	SPR_BTSGA_MGLV_Y_REAR				= 4365,

	/* BTSUS == Suspension bridge */
	/* TILE_* denotes the different tiles a suspension bridge
		can have
		TILE_A and TILE_B are the "beginnings" and "ends" of the
			suspension system. they have small rectangluar endcaps
	 	TILE_C and TILE_D look almost identical to TILE_A and
			TILE_B, but they do not have the "endcaps". They form the
			middle part
		TILE_E is a condensed configuration of two pillars. while they
			are usually 2 pillars apart, they only have 1 pillar separation
			here
		TILE_F is an extended configuration of pillars. they are
			plugged in when pillars should be 3 tiles apart

	*/
	SPR_BTSUS_ROAD_Y_REAR_TILE_A	= 2453,
	SPR_BTSUS_ROAD_Y_REAR_TILE_B	= 2454,
	SPR_BTSUS_Y_FRONT_TILE_A			= 2455,
	SPR_BTSUS_Y_FRONT_TILE_B			= 2456,
	SPR_BTSUS_ROAD_Y_REAR_TILE_D	= 2457,
	SPR_BTSUS_ROAD_Y_REAR_TILE_C	= 2458,
	SPR_BTSUS_Y_FRONT_TILE_D			= 2459,
	SPR_BTSUS_Y_FRONT_TILE_C			= 2460,
	SPR_BTSUS_ROAD_X_REAR_TILE_A	= 2461,
	SPR_BTSUS_ROAD_X_REAR_TILE_B	= 2462,
	SPR_BTSUS_X_FRONT_TILE_A			= 2463,
	SPR_BTSUS_X_FRONT_TILE_B			= 2464,
	SPR_BTSUS_ROAD_X_TILE_D				= 2465,
	SPR_BTSUS_ROAD_X_TILE_C				= 2466,
	SPR_BTSUS_X_FRONT_TILE_D			= 2467,
	SPR_BTSUS_X_FRONT_TILE_C			= 2468,
	SPR_BTSUS_RAIL_Y_REAR_TILE_A	= 2469,
	SPR_BTSUS_RAIL_Y_REAR_TILE_B	= 2470,
	SPR_BTSUS_RAIL_Y_REAR_TILE_D	= 2471,
	SPR_BTSUS_RAIL_Y_REAR_TILE_C	= 2472,
	SPR_BTSUS_RAIL_X_REAR_TILE_A	= 2473,
	SPR_BTSUS_RAIL_X_REAR_TILE_B 	= 2474,
	SPR_BTSUS_RAIL_X_REAR_TILE_D 	= 2475,
	SPR_BTSUS_RAIL_X_REAR_TILE_C	= 2476,
	SPR_BTSUS_Y_PILLAR_TILE_A			= 2477,
	SPR_BTSUS_Y_PILLAR_TILE_B			= 2478,
	SPR_BTSUS_Y_PILLAR_TILE_D			= 2479,
	SPR_BTSUS_Y_PILLAR_TILE_C			= 2480,
	SPR_BTSUS_X_PILLAR_TILE_A			= 2481,
	SPR_BTSUS_X_PILLAR_TILE_B			= 2482,
	SPR_BTSUS_X_PILLAR_TILE_D			= 2483,
	SPR_BTSUS_X_PILLAR_TILE_C			= 2484,
	SPR_BTSUS_RAIL_Y_REAR_TILE_E	= 2485,
	SPR_BTSUS_RAIL_X_REAR_TILE_E	= 2486,
	SPR_BTSUS_ROAD_Y_REAR_TILE_E	= 2487,
	SPR_BTSUS_ROAD_X_REAR_TILE_E	= 2488,
	SPR_BTSUS_Y_FRONT_TILE_E			= 2489,
	SPR_BTSUS_X_FRONT_TILE_E			= 2490,
	SPR_BTSUS_Y_PILLAR_TILE_E			= 2491,
	SPR_BTSUS_X_PILLAR_TILE_E			= 2492,
	SPR_BTSUS_RAIL_X_REAR_TILE_F	= 2493,
	SPR_BTSUS_RAIL_Y_REAR_TILE_F	= 2494,
	SPR_BTSUS_ROAD_X_REAR_TILE_F	= 2495,
	SPR_BTSUS_ROAD_Y_REAR_TILE_F	= 2496,
	SPR_BTSUS_Y_FRONT							= 2497,
	SPR_BTSUS_X_FRONT							= 2498,
	SPR_BTSUS_MONO_Y_REAR_TILE_A	= 4334,
	SPR_BTSUS_MONO_Y_REAR_TILE_B	= 4335,
	SPR_BTSUS_MONO_Y_REAR_TILE_D	= 4336,
	SPR_BTSUS_MONO_Y_REAR_TILE_C	= 4337,
	SPR_BTSUS_MONO_X_REAR_TILE_A	= 4338,
	SPR_BTSUS_MONO_X_REAR_TILE_B	= 4339,
	SPR_BTSUS_MONO_X_REAR_TILE_D	= 4340,
	SPR_BTSUS_MONO_X_REAR_TILE_C	= 4341,
	SPR_BTSUS_MONO_Y_REAR_TILE_E	= 4342,
	SPR_BTSUS_MONO_X_REAR_TILE_E	= 4343,
	SPR_BTSUS_MONO_X_REAR_TILE_F	= 4344,
	SPR_BTSUS_MONO_Y_REAR_TILE_F	= 4345,
	SPR_BTSUS_MGLV_Y_REAR_TILE_A 	=	4374,
	SPR_BTSUS_MGLV_Y_REAR_TILE_B 	=	4375,
	SPR_BTSUS_MGLV_Y_REAR_TILE_D 	=	4376,
	SPR_BTSUS_MGLV_Y_REAR_TILE_C	= 4377,
	SPR_BTSUS_MGLV_X_REAR_TILE_A	= 4378,
	SPR_BTSUS_MGLV_X_REAR_TILE_B	= 4379,
	SPR_BTSUS_MGLV_X_REAR_TILE_D	= 4380,
	SPR_BTSUS_MGLV_X_REAR_TILE_C	= 4381,
	SPR_BTSUS_MGLV_Y_REAR_TILE_E	= 4382,
	SPR_BTSUS_MGLV_X_REAR_TILE_E	= 4383,
	SPR_BTSUS_MGLV_X_REAR_TILE_F	= 4384,
	SPR_BTSUS_MGLV_Y_REAR_TILE_F	= 4385,

	/* cantilever bridges */
	/* They have three different kinds of tiles:
		END(ing), MID(dle), BEG(gining)
	*/
	SPR_BTCAN_RAIL_X_BEG					= 2507,
	SPR_BTCAN_RAIL_X_MID					= 2508,
	SPR_BTCAN_RAIL_X_END					= 2509,
	SPR_BTCAN_RAIL_Y_END					= 2510,
	SPR_BTCAN_RAIL_Y_MID					= 2511,
	SPR_BTCAN_RAIL_Y_BEG					= 2512,
	SPR_BTCAN_ROAD_X_BEG					= 2513,
	SPR_BTCAN_ROAD_X_MID					= 2514,
	SPR_BTCAN_ROAD_X_END					= 2515,
	SPR_BTCAN_ROAD_Y_END					= 2516,
	SPR_BTCAN_ROAD_Y_MID					= 2517,
	SPR_BTCAN_ROAD_Y_BEG					= 2518,
	SPR_BTCAN_X_FRONT_BEG					= 2519,
	SPR_BTCAN_X_FRONT_MID					= 2520,
	SPR_BTCAN_X_FRONT_END					= 2521,
	SPR_BTCAN_Y_FRONT_END					= 2522,
	SPR_BTCAN_Y_FRONT_MID					= 2523,
	SPR_BTCAN_Y_FRONT_BEG					= 2524,
	SPR_BTCAN_X_PILLAR_BEG				= 2525,
	SPR_BTCAN_X_PILLAR_MID				= 2526,
	SPR_BTCAN_Y_PILLAR_MID				= 2527,
	SPR_BTCAN_Y_PILLAR_BEG				= 2528,
	SPR_BTCAN_MONO_X_BEG					= 4346,
	SPR_BTCAN_MONO_X_MID					= 4347,
	SPR_BTCAN_MONO_X_END					= 4348,
	SPR_BTCAN_MONO_Y_END					= 4349,
	SPR_BTCAN_MONO_Y_MID					= 4350,
	SPR_BTCAN_MONO_Y_BEG					= 4351,
	SPR_BTCAN_MGLV_X_BEG					= 4386,
	SPR_BTCAN_MGLV_X_MID					= 4387,
	SPR_BTCAN_MGLV_X_END					= 4388,
	SPR_BTCAN_MGLV_Y_END					= 4389,
	SPR_BTCAN_MGLV_Y_MID					= 4390,
	SPR_BTCAN_MGLV_Y_BEG					= 4391,

	/* little concrete bridge */
	SPR_BTCON_RAIL_X				= 2493,
	SPR_BTCON_RAIL_Y				= 2494,
	SPR_BTCON_ROAD_X				= 2495,
	SPR_BTCON_ROAD_Y				= 2496,
	SPR_BTCON_X_FRONT				= 2497,
	SPR_BTCON_Y_FRONT				= 2498,
	SPR_BTCON_X_PILLAR			= 2505,
	SPR_BTCON_Y_PILLAR			= 2506,
	SPR_BTCON_MONO_X				= 4344,
	SPR_BTCON_MONO_Y				= 4345,
	SPR_BTCON_MGLV_X				= 4384,
	SPR_BTCON_MGLV_Y				= 4385,

	/* little steel girder bridge */
	SPR_BTGIR_RAIL_X				= 2553,
	SPR_BTGIR_RAIL_Y				= 2554,
	SPR_BTGIR_ROAD_X				= 2555,
	SPR_BTGIR_ROAD_Y				= 2556,
	SPR_BTGIR_X_FRONT				= 2557,
	SPR_BTGIR_Y_FRONT				= 2558,
	SPR_BTGIR_X_PILLAR			= 2505,
	SPR_BTGIR_Y_PILLAR			= 2506,
	SPR_BTGIR_MONO_X				= 4362,
	SPR_BTGIR_MONO_Y				= 4363,
	SPR_BTGIR_MGLV_X				= 4402,
	SPR_BTGIR_MGLV_Y				= 4403,

	/* tubular bridges */
	/* tubular bridges have 3 kinds of tiles:
			a start tile (with only half a tube on the far side, marked _BEG
			a middle tile (full tunnel), marked _MID
			and an end tile (half a tube on the near side, maked _END
	*/
	SPR_BTTUB_X_FRONT_BEG				= 2559,
	SPR_BTTUB_X_FRONT_MID				= 2660,
	SPR_BTTUB_X_FRONT_END				= 2561,
	SPR_BTTUB_Y_FRONT_END				= 2562,
	SPR_BTTUB_Y_FRONT_MID				= 2563,
	SPR_BTTUB_Y_FRONT_BEG				= 2564,
	SPR_BTTUB_X_RAIL_REAR_BEG		= 2569,
	SPR_BTTUB_X_RAIL_REAR_MID		= 2570,
	SPR_BTTUB_X_RAIL_REAR_END		= 2571,


	/* ramps (for all bridges except wood and tubular?)*/
	SPR_BTGEN_RAIL_X_SLOPE_DOWN = 2437,
	SPR_BTGEN_RAIL_X_SLOPE_UP		= 2438,
	SPR_BTGEN_RAIL_Y_SLOPE_DOWN	= 2439,
	SPR_BTGEN_RAIL_Y_SLOPE_UP		= 2440,
	SPR_BTGEN_RAIL_RAMP_X_UP		= 2441,
	SPR_BTGEN_RAIL_RAMP_X_DOWN	= 2442,
	SPR_BTGEN_RAIL_RAMP_Y_UP		= 2443,
	SPR_BTGEN_RAIL_RAMP_Y_DOWN	= 2444,
	SPR_BTGEN_ROAD_X_SLOPE_DOWN = 2445,
	SPR_BTGEN_ROAD_X_SLOPE_UP		= 2446,
	SPR_BTGEN_ROAD_Y_SLOPE_DOWN	= 2447,
	SPR_BTGEN_ROAD_Y_SLOPE_UP		= 2448,
	SPR_BTGEN_ROAD_RAMP_X_UP		= 2449,
	SPR_BTGEN_ROAD_RAMP_X_DOWN	= 2450,
	SPR_BTGEN_ROAD_RAMP_Y_UP		= 2451,
	SPR_BTGEN_ROAD_RAMP_Y_DOWN	= 2452,
	SPR_BTGEN_MONO_X_SLOPE_DOWN = 4326,
	SPR_BTGEN_MONO_X_SLOPE_UP		= 4327,
	SPR_BTGEN_MONO_Y_SLOPE_DOWN	= 4328,
	SPR_BTGEN_MONO_Y_SLOPE_UP		= 4329,
	SPR_BTGEN_MONO_RAMP_X_UP		= 4330,
	SPR_BTGEN_MONO_RAMP_X_DOWN	= 4331,
	SPR_BTGEN_MONO_RAMP_Y_UP		= 4332,
	SPR_BTGEN_MONO_RAMP_Y_DOWN	= 4333,
	SPR_BTGEN_MGLV_X_SLOPE_DOWN = 4366,
	SPR_BTGEN_MGLV_X_SLOPE_UP		= 4367,
	SPR_BTGEN_MGLV_Y_SLOPE_DOWN	= 4368,
	SPR_BTGEN_MGLV_Y_SLOPE_UP		= 4369,
	SPR_BTGEN_MGLV_RAMP_X_UP		= 4370,
	SPR_BTGEN_MGLV_RAMP_X_DOWN	= 4371,
	SPR_BTGEN_MGLV_RAMP_Y_UP		= 4372,
	SPR_BTGEN_MGLV_RAMP_Y_DOWN	= 4373,


	/* Vehicle sprite-flags (red/green) */
	SPR_FLAG_VEH_STOPPED	= 3090,
	SPR_FLAG_VEH_RUNNING	= 3091,

	/* Rotor sprite numbers */
	SPR_ROTOR_STOPPED		= 3901,
	SPR_ROTOR_MOVING_1	= 3902,
	SPR_ROTOR_MOVING_3	= 3904,

	/* Town/house sprites */
	SPR_LIFT = 1443,

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
	SPR_IMG_LIGHTHOUSE      = 4085,
	SPR_IMG_TRANSMITTER     = 4086,
	SPR_IMG_LEVEL_LAND      = SPR_OPENTTD_BASE + 68,
	SPR_IMG_PAUSE           = 726,
	SPR_IMG_FASTFORWARD     = SPR_OPENTTD_BASE + 57,
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
	SPR_IMG_INDUSTRY        = 741,
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
	SPR_IMG_PLANTTREES      = 742,
	SPR_IMG_SIGN            = 4082,

	/* OPEN TRANSPORT TYCOON in gamescreen */
	SPR_OTTD_O                = 4842,
	SPR_OTTD_P                = 4841,
	SPR_OTTD_E                = SPR_OPENTTD_BASE + 16,
	SPR_OTTD_N                = 4839,
	SPR_OTTD_T                = 4836,
	SPR_OTTD_R                = 4837,
	SPR_OTTD_A                = 4838,
	SPR_OTTD_S                = 4840,
	SPR_OTTD_Y                = 4843,
	SPR_OTTD_C                = 4844
};

/* Cursor sprite numbers */
typedef enum CursorSprites {
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
	SPR_CURSOR_LEVEL_LAND     = SPR_OPENTTD_BASE + 69,

	SPR_CURSOR_TOWN           = 4080,
	SPR_CURSOR_INDUSTRY       = 4081,
	SPR_CURSOR_ROCKY_AREA     = 4087,
	SPR_CURSOR_LIGHTHOUSE     = 4088,
	SPR_CURSOR_TRANSMITTER    = 4089,

	/* airport cursors */
	SPR_CURSOR_AIRPORT        = 2724,

	/* dock cursors */
	SPR_CURSOR_DOCK           = 3668,
	SPR_CURSOR_CANAL          = SPR_OPENTTD_BASE + 11,
	SPR_CURSOR_LOCK           = SPR_OPENTTD_BASE + 64,

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

	SPR_CURSOR_RAIL_DEPOT     = 1296,
	SPR_CURSOR_RAIL_STATION   = 1300,
	SPR_CURSOR_TUNNEL_RAIL    = 2434,
	SPR_CURSOR_TUNNEL_MONO    = 2435,
	SPR_CURSOR_TUNNEL_MAGLEV  = 2436,

	SPR_CURSOR_AUTORAIL       = SPR_OPENTTD_BASE + 4,
	SPR_CURSOR_CHECKPOINT     = SPR_OPENTTD_BASE + 7,
	SPR_CURSOR_MONO_DEPOT     = SPR_OPENTTD_BASE + 14,
	SPR_CURSOR_MAGLEV_DEPOT   = SPR_OPENTTD_BASE + 15,
	SPR_CURSOR_CONVERT_RAIL   = SPR_OPENTTD_BASE + 26,

	/* road cursors */
	SPR_CURSOR_ROAD_NESW      = 1311,
	SPR_CURSOR_ROAD_NWSE      = 1312,

	SPR_CURSOR_ROAD_DEPOT     = 1297,
	SPR_CURSOR_BUS_STATION    = 2725,
	SPR_CURSOR_TRUCK_STATION  = 2726,
	SPR_CURSOR_ROAD_TUNNEL    = 2433,
} CursorSprite;

// Animation macro in table/animcursors.h (_animcursors[])
enum AnimCursors {
	ANIMCURSOR_DEMOLISH     = -1,	//  704 -  707 - demolish dynamite
	ANIMCURSOR_LOWERLAND    = -2,	//  699 -  701 - lower land tool
	ANIMCURSOR_RAISELAND    = -3,	//  696 -  698 - raise land tool
	ANIMCURSOR_PICKSTATION  = -4,	//  716 -  718 - goto-order icon
	ANIMCURSOR_BUILDSIGNALS	= -5,	// 1292 - 1293 - build signal
};

enum {
	MAX_SPRITES = 0x3FFF, //the highest number a sprite can have
};

/*
	these numbers change the colors of the palette for a sprite
	both need to be fed a sprite which contains the "new" colors
*/
enum Modifiers {
	PALETTE_MODIFIER_TRANSPARENT 	= 0x4000,
	PALETTE_MODIFIER_COLOR 				= 0x8000,
};

enum PaletteSprites {
	//note: these numbers are already the modified once the renderer needs.
	//the actual sprite number is the upper 16 bits of the number

	//Here a puslating red tile is drawn if you try to build a wrong tunnel or raise/lower land where it is not possible
	PALETTE_TILE_RED_PULSATING 	= 0x3038000,
	//makes a square red. is used when removing rails or other stuff
	PALETTE_SEL_TILE_RED 				= 0x3048000,
	//This draws a blueish square (catchment areas for example)
	PALETTE_SEL_TILE_BLUE 			= 0x3058000,
	//0x306 is a real sprite (the little dot you get when you try to raise/lower a corner of the map
	//here the color switches begin
	//use this if you add stuff to the value, so that the resulting color
	//is not a fixed value.
	//NOTE THAT THE SWITCH 0x8000 is NOT present in _TO_COLORS yet!
	PALETTE_TO_COLORS 					= 0x3070000,
	PALETTE_TO_DARK_BLUE 				= 0x3078000,
	PALETTE_TO_PALE_GREEN 			= 0x3088000,
	PALETTE_TO_PINK 						= 0x3098000,
	PALETTE_TO_YELLOW 					= 0x30A8000,
	PALETTE_TO_RED 							= 0x30B8000,
	PALETTE_TO_LIGHT_BLUE 			= 0x30C8000,
	PALETTE_TO_GREEN 						= 0x30D8000,
	PALETTE_TO_DARK_GREEN 			= 0x30E8000,
	PALETTE_TO_BLUE 						= 0x30F8000,
	PALETTE_TO_CREAM 						= 0x3108000,
	//maybe don't use as player color because it doesn't display in the graphs?
	PALETTE_TO_MAUVE 						= 0x3118000,
	PALETTE_TO_PURPLE 					= 0x3128000,
	PALETTE_TO_ORANGE 					= 0x3138000,
	PALETTE_TO_BROWN 						= 0x3148000,
	PALETTE_TO_GREY 						= 0x3158000,
	PALETTE_TO_WHITE 						= 0x3168000,
 	//sets color to bare land stuff, for rail and road (and crossings)
	PALETTE_TO_BARE_LAND 				= 0x3178000,
	//XXX is 318-31A really not used?
	//XXX FIXME I dunno yet what this is
	PALETTE_31B 								= 0x31B8000,
	//structure color to something brownish (for the cantilever bridges for example)
	PALETTE_TO_STRUCT_BROWN 		= 0x31C8000,
	PALETTE_31D 								= 0x31D8000, //XXX FIXME Don't know this either
	//sets bridge or structure to red, little concrete one and cantilever use this one for example
	PALETTE_TO_STRUCT_RED 			= 0x31E8000,
	//XXX 31F
	PALETTE_TO_STRUCT_CONCRETE 	= 0x3208000,  //Sets the suspension bridge to concrete, also other strucutures use it
	PALETTE_TO_STRUCT_YELLOW 		= 0x3218000,    //Sets the bridge color to yellow (suspension and tubular)
	PALETTE_TO_TRANSPARENT 			= 0x3224000,	//This sets the sprite to transparent
	//This is used for changing the tubular bridges to the silicon display, or some grayish color
	PALETTE_TO_STRUCT_GREY 			= 0x3238000,
	PALETTE_CRASH 							= 0x3248000,	//this changes stuff to the "crash color"
	//XXX another place where structures are colored.
	//I'm not sure which colors these are
	PALETTE_59E 								= 0x59E8000,
	PALETTE_59F 								= 0x59F8000,

};

#define MAKE_TRANSPARENT(img) (img = (img & MAX_SPRITES) | PALETTE_TO_TRANSPARENT)

#endif /* SPRITES_H */
