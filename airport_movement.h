#ifndef AIRPORT_MOVEMENT_H
#define AIRPORT_MOVEMENT_H

#include "stdafx.h"
#include "macros.h"

// don't forget to change the airport_depots too for larger mapsizes. TILE_X_BITS 16
// won't fit in uint16 for example and overflow will occur in the checking code!
// TrueLight -- So make it a TileIndex..

typedef struct AirportMovingData {
	int x,y;
	byte flag;
	byte direction;
} AirportMovingData;

// state machine input struct (from external file, etc.)
// Finite sTate mAchine --> FTA
typedef struct AirportFTAbuildup {
	byte position;							// the position that an airplane is at
	byte heading;								// the current orders (eg. TAKEOFF, HANGAR, ENDLANDING, etc.)
	uint32 block;								// the block this position is on on the airport (st->airport_flags)
	byte next_in_chain;					// next position from this position
} AirportFTAbuildup;

enum {
	AMED_NOSPDCLAMP = 1<<0,
	AMED_TAKEOFF = 1<<1,
	AMED_SLOWTURN = 1<<2,
	AMED_LAND = 1<<3,
	AMED_EXACTPOS = 1<<4,
	AMED_BRAKE = 1<<5,
	AMED_HELI_RAISE = 1<<6,
	AMED_HELI_LOWER = 1<<7,
};

enum {MAX_ELEMENTS = 255};
enum {MAX_HEADINGS = 18};

///////////////////////////////////////////////////////////////////////
///////***********Movement States on Airports********************//////
// headings target
enum {
	TO_ALL = 0,
	HANGAR = 1,
	TERM1 = 2,
	TERM2 = 3,
	TERM3 = 4,
	TERM4 = 5,
	TERM5 = 6,
	TERM6 = 7,
	HELIPAD1 = 8,
	HELIPAD2 = 9,
	TAKEOFF = 10,
	STARTTAKEOFF = 11,
	ENDTAKEOFF = 12,
	HELITAKEOFF = 13,
	FLYING = 14,
	LANDING = 15,
	ENDLANDING = 16,
	HELILANDING = 17,
	HELIENDLANDING = 18
};

///////////////////////////////////////////////////////////////////////
///////**********Movement Blocks on Airports*********************//////
// blocks (eg_airport_flags)
enum {
	TERM1_block = 1 << 0,
	TERM2_block = 1 << 1,
	TERM3_block = 1 << 2,
	TERM4_block = 1 << 3,
	TERM5_block = 1 << 4,
	TERM6_block = 1 << 5,
	HELIPAD1_block = 1 << 6,
	HELIPAD2_block = 1 << 7,
	RUNWAY_IN_OUT_block = 1 << 8,
	RUNWAY_IN_block     = 1 << 8,
	AIRPORT_BUSY_block  = 1 << 8,
	RUNWAY_OUT_block = 1 << 9,
	TAXIWAY_BUSY_block = 1 << 10,
	OUT_WAY_block = 1 << 11,
	IN_WAY_block = 1 << 12,
	AIRPORT_ENTRANCE_block = 1 << 13,
	TERM_GROUP1_block = 1 << 14,
	TERM_GROUP2_block = 1 << 15,
	HANGAR2_AREA_block = 1 << 16,
	TERM_GROUP2_ENTER1_block = 1 << 17,
	TERM_GROUP2_ENTER2_block = 1 << 18,
	TERM_GROUP2_EXIT1_block = 1 << 19,
	TERM_GROUP2_EXIT2_block = 1 << 20,
	PRE_HELIPAD_block = 1 << 21,
	NOTHING_block = 1 << 30
};

///////////////////////////////////////////////////////////////////////
/////*********Movement Positions on Airports********************///////
// Country Airfield (small) 4x3
static const AirportMovingData _airport_moving_data_country[22] = {
	{53,3,AMED_EXACTPOS,3},											// 00 In Hangar
	{53,27,0,0},																// 01 Taxi to right outside depot
	{32,23,AMED_EXACTPOS,7},										// 02 Terminal 1
	{10,23,AMED_EXACTPOS,7},										// 03 Terminal 2
	{43,37,0,0},																// 04 Going towards terminal 2
	{24,37,0,0},																// 05 Going towards terminal 2
	{53,37,0,0},																// 06 Going for takeoff
	{61,40,AMED_EXACTPOS,1},										// 07 Taxi to start of runway (takeoff)
	{3,40,AMED_NOSPDCLAMP,0},										// 08 Accelerate to end of runway
	{-79,40,AMED_NOSPDCLAMP | AMED_TAKEOFF,0},	// 09 Take off
	{177,40,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},	// 10 Fly to landing position in air
	{56,40,AMED_NOSPDCLAMP | AMED_LAND,0},			// 11 Going down for land
	{3,40,AMED_NOSPDCLAMP | AMED_BRAKE,0},			// 12 Just landed, brake until end of runway
	{ 7,40,0,0},																// 13 Just landed, turn around and taxi 1 square
	{53,40,0,0},																// 14 Taxi from runway to crossing
	{-31,193,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},// 15 Fly around waiting for a landing spot (north-east)
	{1,1,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},		// 16 Fly around waiting for a landing spot (north-west)
	{257,1,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},	// 17 Fly around waiting for a landing spot (south-west)
	{273,49,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},	// 18 Fly around waiting for a landing spot (south)
	{44,37,AMED_HELI_RAISE,0},									// 19 Helicopter takeoff
	{44,40,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},	// 20 In position above landing spot helicopter
	{44,40,AMED_HELI_LOWER,0}										// 21 Helicopter landing
};

// City Airport (large) 6x6
static const AirportMovingData _airport_moving_data_town[25] = {
	{85,3,AMED_EXACTPOS,3},											// 00 In Hangar
	{85,27,0,0},																// 01 Taxi to right outside depot
	{26,41,AMED_EXACTPOS,5},										// 02 Terminal 1
	{56,20,AMED_EXACTPOS,3},										// 03 Terminal 2
	{38,8,AMED_EXACTPOS,5},											// 04 Terminal 3
	{65,6,0,0},																	// 05 Taxi to right in infront of terminal 2/3
	{80,27,0,0},																// 06 Taxiway terminals 2-3
	{44,63,0,0},																// 07 Taxi to Airport center
	{58,71,0,0},																// 08 Towards takeoff
	{72,85,0,0},																// 09 Taxi to runway (takeoff)
	{89,85,AMED_EXACTPOS,1},										// 10 Taxi to start of runway (takeoff)
	{3,85,AMED_NOSPDCLAMP,0},										// 11 Accelerate to end of runway
	{-79,85,AMED_NOSPDCLAMP | AMED_TAKEOFF,0},	// 12 Take off
	{177,85,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},	// 13 Fly to landing position in air
	{89,85,AMED_NOSPDCLAMP | AMED_LAND,0},			// 14 Going down for land
	{3,85,AMED_NOSPDCLAMP | AMED_BRAKE,0},			// 15 Just landed, brake until end of runway
	{20,87,0,0},																// 16 Just landed, turn around and taxi 1 square
	{36,71,0,0},																// 17 Taxi from runway to crossing
	{-31,193,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},// 18 Fly around waiting for a landing spot (north-east)
	{1,1,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},		// 19 Fly around waiting for a landing spot (north-west)
	{257,1,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},	// 20 Fly around waiting for a landing spot (south-west)
	{273,49,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},	// 21 Fly around waiting for a landing spot (south)
	{44,63,AMED_HELI_RAISE,0},									// 22 Helicopter takeoff
	{28,74,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},	// 23 In position above landing spot helicopter
	{28,74,AMED_HELI_LOWER,0}										// 24 Helicopter landing
};

// Metropolitan Airport (metropolitan) - 2 runways
static const AirportMovingData _airport_moving_data_metropolitan[27] = {
	{85,3,AMED_EXACTPOS,3},											// 00 In Hangar
	{85,27,0,0},																// 01 Taxi to right outside depot
	{26,41,AMED_EXACTPOS,5},										// 02 Terminal 1
	{56,20,AMED_EXACTPOS,3},										// 03 Terminal 2
	{38,8,AMED_EXACTPOS,5},											// 04 Terminal 3
	{65,6,0,0},																	// 05 Taxi to right in infront of terminal 2/3
	{70,33,0,0},																// 06 Taxiway terminals 2-3
	{44,58,0,0},																// 07 Taxi to Airport center
	{72,58,0,0},																// 08 Towards takeoff
	{72,69,0,0},																// 09 Taxi to runway (takeoff)
	{89,69,AMED_EXACTPOS,1},										// 10 Taxi to start of runway (takeoff)
	{3,69,AMED_NOSPDCLAMP,0},										// 11 Accelerate to end of runway
	{-79,69,AMED_NOSPDCLAMP | AMED_TAKEOFF,0},	// 12 Take off
	{177,85,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},	// 13 Fly to landing position in air
	{89,85,AMED_NOSPDCLAMP | AMED_LAND,0},			// 14 Going down for land
	{3,85,AMED_NOSPDCLAMP | AMED_BRAKE,0},			// 15 Just landed, brake until end of runway
	{21,85,0,0},																// 16 Just landed, turn around and taxi 1 square
	{21,69,0,0},																// 17 On Runway-out taxiing to In-Way
	{21,54,AMED_EXACTPOS,5},										// 18 Taxi from runway to crossing
	{-31,193,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},// 19 Fly around waiting for a landing spot (north-east)
	{1,1,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},		// 20 Fly around waiting for a landing spot (north-west)
	{257,1,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},	// 21 Fly around waiting for a landing spot (south-west)
	{273,49,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},	// 22 Fly around waiting for a landing spot (south)
	{44,58,0,0},																// 23 Helicopter takeoff spot on ground (to clear airport sooner)
	{44,63,AMED_HELI_RAISE,0},									// 24 Helicopter takeoff
	{15,54,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},	// 25 Get in position above landing spot helicopter
	{15,54,AMED_HELI_LOWER,0}										// 26 Helicopter landing
};

// International Airport (international) - 2 runways, 6 terminals, dedicated helipod
static const AirportMovingData _airport_moving_data_international[51] = {
	{7,55,AMED_EXACTPOS,3},											// 00 In Hangar 1
	{100,21,AMED_EXACTPOS,3},										// 01 In Hangar 2
	{7,70,0,0},																	// 02 Taxi to right outside depot
	{100,36,0,0},																// 03 Taxi to right outside depot
	{38,70,AMED_EXACTPOS,5},										// 04 Terminal 1
	{38,54,AMED_EXACTPOS,5},										// 05 Terminal 2
	{38,38,AMED_EXACTPOS,5},										// 06 Terminal 3
	{70,70,AMED_EXACTPOS,1},										// 07 Terminal 4
	{70,54,AMED_EXACTPOS,1},										// 08 Terminal 5
	{70,38,AMED_EXACTPOS,1},										// 09 Terminal 6
	{104,71,AMED_EXACTPOS,1},										// 10 Helipad 1
	{104,55,AMED_EXACTPOS,1},										// 11 Helipad 2
	{22,87,0,0},																// 12 Towards Terminals 4/5/6, Helipad 1/2
	{60,87,0,0},																// 13 Towards Terminals 4/5/6, Helipad 1/2
	{66,87,0,0},																// 14 Towards Terminals 4/5/6, Helipad 1/2
	{86,87,AMED_EXACTPOS,7},										// 15 Towards Terminals 4/5/6, Helipad 1/2
	{86, 70,0,0},																// 16 In Front of Terminal 4 / Helipad 1
	{86, 54,0,0},																// 17 In Front of Terminal 5 / Helipad 2
	{86, 38,0,0},																// 18 In Front of Terminal 6
	{86, 22,0,0},																// 19 Towards Terminals Takeoff (Taxiway)
	{66,22,0,0},																// 20 Towards Terminals Takeoff (Taxiway)
	{60,22,0,},																	// 21 Towards Terminals Takeoff (Taxiway)
	{38,22,0,0},																// 22 Towards Terminals Takeoff (Taxiway)
	{22, 70,0,0},																// 23 In Front of Terminal 1
	{22, 58,0,0},																// 24 In Front of Terminal 2
	{22, 38,0,0},																// 25 In Front of Terminal 3
	{22, 22,AMED_EXACTPOS,7},										// 26 Going for Takeoff
	{22,  6,0,0},																// 27 On Runway-out, prepare for takeoff
	{3,6,AMED_EXACTPOS,5},											// 28 Accelerate to end of runway
	{60,6,AMED_NOSPDCLAMP,0},										// 29 Release control of runway, for smoother movement
	{105,6,AMED_NOSPDCLAMP,0},									// 30 End of runway
	{190, 6,AMED_NOSPDCLAMP | AMED_TAKEOFF,0},	// 31 Take off
	{193,104,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},// 32 Fly to landing position in air
	{105,104,AMED_NOSPDCLAMP | AMED_LAND,0},		// 33 Going down for land
	{  3,104,AMED_NOSPDCLAMP | AMED_BRAKE,0},		// 34 Just landed, brake until end of runway
	{ 12,104,0,0},															// 35 Just landed, turn around and taxi 1 square
	{  7,84,0,0},																// 36 Taxi from runway to crossing
	{-31,209,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},// 37 Fly around waiting for a landing spot (north-east)
	{1,6,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},		// 38 Fly around waiting for a landing spot (north-west)
	{273,6,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},	// 39 Fly around waiting for a landing spot (south-west)
	{305,81,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},	// 40 Fly around waiting for a landing spot (south)
	// Helicopter
	{128,80,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},	// 41 Bufferspace before helipad
	{128,80,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},	// 42 Bufferspace before helipad
	{96,71,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},	// 43 Get in position for Helipad1
	{96,55,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},	// 44 Get in position for Helipad2
	{96,71,AMED_HELI_LOWER,0},									// 45 Land at Helipad1
	{96,55,AMED_HELI_LOWER,0},									// 46 Land at Helipad2
	{104,71,AMED_HELI_RAISE,0},									// 47 Takeoff Helipad1
	{104,55,AMED_HELI_RAISE,0},									// 48 Takeoff Helipad2
	{104,32,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},	// 49 Go to position for Hangarentrance in air
	{104,32,AMED_HELI_LOWER,0}									// 50 Land in HANGAR2_AREA to go to hangar
};

// Heliport (heliport)
static const AirportMovingData _airport_moving_data_heliport[9] = {
	{ 5,9,AMED_EXACTPOS,1},											// 0 - At heliport terminal
	{ 2,9,AMED_HELI_RAISE,0},										// 1 - Take off (play sound)
	{-3,9,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},		// 2 - In position above landing spot helicopter
	{-3,9,AMED_HELI_LOWER,0},										// 3 - Land
	{ 2,9,0,0},																	// 4 - Goto terminal on ground
	{-31, 59,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},// 5 - Circle #1 (north-east)
	{-31,-49,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},// 6 - Circle #2 (north-west)
	{ 49,-49,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},// 7 - Circle #3 (south-west)
	{ 70,  9,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},// 8 - Circle #4 (south)
};

// Oilrig
static const AirportMovingData _airport_moving_data_oilrig[9] = {
	{31,9,AMED_EXACTPOS,1},											// 0 - At oilrig terminal
	{28,9,AMED_HELI_RAISE,0},										// 1 - Take off (play sound)
	{23,9,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},		// 2 - In position above landing spot helicopter
	{23,9,AMED_HELI_LOWER,0},										// 3 - Land
	{28,9,0,0},																	// 4 - Goto terminal on ground
	{-31, 69,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},// 5 - circle #1 (north-east)
	{-31,-49,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},// 6 - circle #2 (north-west)
	{ 69,-49,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},// 7 - circle #3 (south-west)
	{ 70,  9,AMED_NOSPDCLAMP | AMED_SLOWTURN,0},// 8 - circle #4 (south)
};

///////////////////////////////////////////////////////////////////////
/////**********Movement Machine on Airports*********************///////
// first element of depots array tells us how many depots there are (to know size of array)
// this may be changed later when airports are moved to external file
static const TileIndex _airport_depots_country[] = {1, TILE_XY(3,0)};
static const AirportFTAbuildup _airport_fta_country[] = {
	{0,HANGAR,NOTHING_block,1},
	{1,255,AIRPORT_BUSY_block,0}, {1,HANGAR,0,0}, {1,TERM1,TERM1_block,2}, {1,TERM2,0,4}, {1,HELITAKEOFF,0,19}, {1,0,0,6},
	{2,TERM1,TERM1_block,1},
	{3,TERM2,TERM2_block,5},
	{4,255,AIRPORT_BUSY_block,0}, {4,TERM2,0,5}, {4,HANGAR,0,1}, {4,TAKEOFF,0,6}, {4,HELITAKEOFF,0,1},
	{5,255,AIRPORT_BUSY_block,0}, {5,TERM2,TERM2_block,3}, {5,0,0,4},
	{6,0,AIRPORT_BUSY_block,7},
	// takeoff
	{7,TAKEOFF,AIRPORT_BUSY_block,8},
	{8,STARTTAKEOFF,NOTHING_block,9},
	{9,ENDTAKEOFF,NOTHING_block,0},
	// landing
	{10,FLYING,NOTHING_block,15}, {10,LANDING,0,11}, {10,HELILANDING,0,20},
	{11,LANDING,AIRPORT_BUSY_block,12},
	{12,0,AIRPORT_BUSY_block,13},
	{13,ENDLANDING,AIRPORT_BUSY_block,14}, {13,TERM2,0,5}, {13,0,0,14},
	{14,0,AIRPORT_BUSY_block,1},
	// In air
	{15,0,NOTHING_block,16},
	{16,0,NOTHING_block,17},
	{17,0,NOTHING_block,18},
	{18,0,NOTHING_block,10},
	{19,HELITAKEOFF,NOTHING_block,0},
	{20,HELILANDING,AIRPORT_BUSY_block,21},
	{21,HELIENDLANDING,AIRPORT_BUSY_block,1},
	{MAX_ELEMENTS,0,0,0} // end marker. DO NOT REMOVE
};

static const TileIndex _airport_depots_city[] = {1, TILE_XY(5,0)};
static const AirportFTAbuildup _airport_fta_city[] = {
	{0,HANGAR,NOTHING_block,1}, {0,TAKEOFF,OUT_WAY_block,1}, {0,0,0,1},
	{1,255,TAXIWAY_BUSY_block,0}, {1,HANGAR,0,0}, {1,TERM2,0,6}, {1,TERM3,0,6}, {1,0,0,7}, // for all else, go to 7
	{2,TERM1,TERM1_block,7}, {2,TAKEOFF,OUT_WAY_block,7}, {2,0,0,7},
	{3,TERM2,TERM2_block,5}, {3,TAKEOFF,OUT_WAY_block,5}, {3,0,0,5},
	{4,TERM3,TERM3_block,5}, {4,TAKEOFF,OUT_WAY_block,5}, {4,0,0,5},
	{5,255,TAXIWAY_BUSY_block,0}, {5,TERM2,TERM2_block,3}, {5,TERM3,TERM3_block,4}, {5,0,0,6},
	{6,255,TAXIWAY_BUSY_block,0}, {6,TERM2,0,5}, {6,TERM3,0,5}, {6,HANGAR,0,1}, {6,0,0,7},
	{7,255,TAXIWAY_BUSY_block,0}, {7,TERM1,TERM1_block,2}, {7,TAKEOFF,OUT_WAY_block,8}, {7,HELITAKEOFF,0,22}, {7,HANGAR,0,1}, {7,0,0,6},
	{8,0,OUT_WAY_block,9},
	{9,0,RUNWAY_IN_OUT_block,10},
	// takeoff
	{10,TAKEOFF,RUNWAY_IN_OUT_block,11},
	{11,STARTTAKEOFF,NOTHING_block,12},
	{12,ENDTAKEOFF,NOTHING_block,0},
	// landing
	{13,FLYING,NOTHING_block,18}, {13,LANDING,0,14}, {13,HELILANDING,0,23},
	{14,LANDING,RUNWAY_IN_OUT_block,15},
	{15,0,RUNWAY_IN_OUT_block,16},
	{16,0,RUNWAY_IN_OUT_block,17},
	{17,ENDLANDING,IN_WAY_block,7},
	// In Air
	{18,0,NOTHING_block,19},
	{19,0,NOTHING_block,20},
	{20,0,NOTHING_block,21},
	{21,0,NOTHING_block,13},
	// helicopter
	{22,HELITAKEOFF,NOTHING_block,0},
	{23,HELILANDING,IN_WAY_block,24},
	{24,HELIENDLANDING,IN_WAY_block,17},
	{MAX_ELEMENTS,0,0,0} // end marker. DO NOT REMOVE
};

static const TileIndex _airport_depots_metropolitan[] = {1, TILE_XY(5,0)};
static const AirportFTAbuildup _airport_fta_metropolitan[] = {
	{0,HANGAR,NOTHING_block,1},
	{1,255,TAXIWAY_BUSY_block,0}, {1,HANGAR,0,0}, {1,TERM2,0,6}, {1,TERM3,0,6}, {1,0,0,7}, // for all else, go to 7
	{2,TERM1,TERM1_block,7},
	{3,TERM2,TERM2_block,5},
	{4,TERM3,TERM3_block,5},
	{5,255,TAXIWAY_BUSY_block,0}, {5,TERM2,TERM2_block,3}, {5,TERM3,TERM3_block,4}, {5,0,0,6},
	{6,255,TAXIWAY_BUSY_block,0}, {6,TERM2,0,5}, {6,TERM3,0,5}, {6,HANGAR,0,1}, {6,0,0,7},
	{7,255,TAXIWAY_BUSY_block,0}, {7,TERM1,TERM1_block,2}, {7,TAKEOFF,0,8}, {7,HELITAKEOFF,0,23}, {7,HANGAR,0,1}, {7,0,0,6},
	{8,0,OUT_WAY_block,9},
	{9,0,RUNWAY_OUT_block,10},
	// takeoff
	{10,TAKEOFF,RUNWAY_OUT_block,11},
	{11,STARTTAKEOFF,NOTHING_block,12},
	{12,ENDTAKEOFF,NOTHING_block,0},
	// landing
	{13,FLYING,NOTHING_block,19}, {13,LANDING,0,14}, {13,HELILANDING,0,25},
	{14,LANDING,RUNWAY_IN_block,15},
	{15,0,RUNWAY_IN_block,16},
	{16,255,RUNWAY_IN_block,0}, {16,ENDLANDING,IN_WAY_block,17},
	{17,255,RUNWAY_OUT_block,0}, {17,ENDLANDING,IN_WAY_block,18},
	{18,ENDLANDING,IN_WAY_block,7},
	// In Air
	{19,0,NOTHING_block,20},
	{20,0,NOTHING_block,21},
	{21,0,NOTHING_block,22},
	{22,0,NOTHING_block,13},
	// helicopter
	{23,0,NOTHING_block,24},
	{24,HELITAKEOFF,NOTHING_block,0},
	{25,HELILANDING,IN_WAY_block,26},
	{26,HELIENDLANDING,IN_WAY_block,18},
	{MAX_ELEMENTS,0,0,0} // end marker. DO NOT REMOVE
};

static const TileIndex _airport_depots_international[] = {2, TILE_XY(0,3), TILE_XY(6,1)};
static const AirportFTAbuildup _airport_fta_international[] = {
	{0,HANGAR,NOTHING_block,2}, {0,255,TERM_GROUP1_block,0}, {0,255,TERM_GROUP2_ENTER1_block,1}, {0,HELITAKEOFF,HELIPAD1_block,2}, {0,0,0,2},
	{1,HANGAR,NOTHING_block,3}, {1,255,HANGAR2_AREA_block,1}, {1,HELITAKEOFF,HELIPAD2_block,3}, {1,0,0,3},
	{2,255,AIRPORT_ENTRANCE_block,0}, {2,HANGAR,0,0}, {2,TERM4,0,12}, {2,TERM5,0,12}, {2,TERM6,0,12}, {2,HELIPAD1,0,12}, {2,HELIPAD2,0,12}, {2,HELITAKEOFF,0,12}, {2,0,0,23},
	{3,255,HANGAR2_AREA_block,0}, {3,HANGAR,0,1}, {3,0,0,18},
	{4,TERM1,TERM1_block,23}, {4,HANGAR,AIRPORT_ENTRANCE_block,23}, {4,0,0,23},
	{5,TERM2,TERM2_block,24}, {5,HANGAR,AIRPORT_ENTRANCE_block,24}, {5,0,0,24},
	{6,TERM3,TERM3_block,25}, {6,HANGAR,AIRPORT_ENTRANCE_block,25}, {6,0,0,25},
	{7,TERM4,TERM4_block,16}, {7,HANGAR,HANGAR2_AREA_block,16}, {7,0,0,16},
	{8,TERM5,TERM5_block,17}, {8,HANGAR,HANGAR2_AREA_block,17}, {8,0,0,17},
	{9,TERM6,TERM6_block,18}, {9,HANGAR,HANGAR2_AREA_block,18}, {9,0,0,18},
	{10,HELIPAD1,HELIPAD1_block,10}, {10,HANGAR,HANGAR2_AREA_block,16}, {10,HELITAKEOFF,0,47},
	{11,HELIPAD2,HELIPAD2_block,11}, {11,HANGAR,HANGAR2_AREA_block,17}, {11,HELITAKEOFF,0,48},
	{12,0,TERM_GROUP2_ENTER1_block,13},
	{13,0,TERM_GROUP2_ENTER1_block,14},
	{14,0,TERM_GROUP2_ENTER2_block,15},
	{15,0,TERM_GROUP2_ENTER2_block,16},
	{16,255,TERM_GROUP2_block,0}, {16,TERM4,TERM4_block,7}, {16,HELIPAD1,HELIPAD1_block,10}, {16,HELITAKEOFF,HELIPAD1_block,10}, {16,0,0,17},
	{17,255,TERM_GROUP2_block,0}, {17,TERM5,TERM5_block,8}, {17,TERM4,0,16}, {17,HELIPAD1,0,16}, {17,HELIPAD2,HELIPAD2_block,11}, {17,HELITAKEOFF,HELIPAD2_block,11}, {17,0,0,18},
	{18,255,TERM_GROUP2_block,0}, {18,TERM6,TERM6_block,9}, {18,TAKEOFF,0,19}, {18,HANGAR,HANGAR2_AREA_block,3}, {18,0,0,17},
	{19,0,TERM_GROUP2_EXIT1_block,20},
	{20,0,TERM_GROUP2_EXIT1_block,21},
	{21,0,TERM_GROUP2_EXIT2_block,22},
	{22,0,TERM_GROUP2_EXIT2_block,26},
	{23,255,TERM_GROUP1_block,0}, {23,TERM1,TERM1_block,4}, {23,HANGAR,AIRPORT_ENTRANCE_block,2}, {23,0,0,24},
	{24,255,TERM_GROUP1_block,0}, {24,TERM2,TERM2_block,5}, {24,TERM1,0,23}, {24,HANGAR,0,23}, {24,0,0,25},
	{25,255,TERM_GROUP1_block,0}, {25,TERM3,TERM3_block,6}, {25,TAKEOFF,0,26}, {25,0,0,24},
	{26,255,TAXIWAY_BUSY_block,0}, {26,TAKEOFF,0,27}, {26,0,0,25},
	{27,0,OUT_WAY_block,28},
	// takeoff
	{28,TAKEOFF,OUT_WAY_block,29},
	{29,0,RUNWAY_OUT_block,30},
	{30,STARTTAKEOFF,NOTHING_block,31},
	{31,ENDTAKEOFF,NOTHING_block,0},
	// landing
	{32,FLYING,NOTHING_block,37}, {32,LANDING,0,33}, {32,HELILANDING,0,41},
	{33,LANDING,RUNWAY_IN_block,34},
	{34,0,RUNWAY_IN_block,35},
	{35,0,RUNWAY_IN_block,36},
	{36,ENDLANDING,IN_WAY_block,36}, {36,255,TERM_GROUP1_block,0}, {36,255,TERM_GROUP2_ENTER1_block,1}, {36,TERM4,0,12}, {36,TERM5,0,12}, {36,TERM6,0,12}, {36,0,0,2},
	// In Air
	{37,0,NOTHING_block,38},
	{38,0,NOTHING_block,39},
	{39,0,NOTHING_block,40},
	{40,0,NOTHING_block,32},
	// Helicopter -- stay in air in special place as a buffer to choose from helipads
	{41,HELILANDING,PRE_HELIPAD_block,42},
	{42,HELIENDLANDING,PRE_HELIPAD_block,42}, {42,HELIPAD1,0,43}, {42,HELIPAD2,0,44}, {42,HANGAR,0,49},
	{43,0,NOTHING_block,45},
	{44,0,NOTHING_block,46},
	// landing
	{45,255,NOTHING_block,0}, {45,HELIPAD1,HELIPAD1_block,10},
	{46,255,NOTHING_block,0}, {46,HELIPAD2,HELIPAD2_block,11},
	// Helicopter -- takeoff
	{47,HELITAKEOFF,NOTHING_block,0},
	{48,HELITAKEOFF,NOTHING_block,0},
	{49,0,HANGAR2_AREA_block,50}, // need to go to hangar when waiting in air
	{50,0,HANGAR2_AREA_block,3},
	{MAX_ELEMENTS,0,0,0} // end marker. DO NOT REMOVE
};

static const TileIndex _airport_depots_heliport_oilrig[] = {0};
static const AirportFTAbuildup _airport_fta_heliport_oilrig[] = {
	{0,HELIPAD1,HELIPAD1_block,1},
	{1,HELITAKEOFF,NOTHING_block,0}, // takeoff
	{2,255,AIRPORT_BUSY_block,0}, {2,HELILANDING,0,3}, {2,HELITAKEOFF,0,1},
	{3,HELILANDING,AIRPORT_BUSY_block,4},
	{4,HELIENDLANDING,AIRPORT_BUSY_block,4}, {4,HELIPAD1,HELIPAD1_block,0}, {4,HELITAKEOFF,0,2},
	// In Air
	{5,0,NOTHING_block,6},
	{6,0,NOTHING_block,7},
	{7,0,NOTHING_block,8},
	{8,FLYING,NOTHING_block,5}, {8,HELILANDING,HELIPAD1_block,2}, // landing
	{MAX_ELEMENTS,0,0,0} // end marker. DO NOT REMOVE
};

static const AirportMovingData * const _airport_moving_datas[6] = {
	_airport_moving_data_country,				// Country Airfield (small) 4x3
	_airport_moving_data_town,					// City Airport (large) 6x6
	_airport_moving_data_heliport,			// Heliport
	_airport_moving_data_metropolitan,	// Metropolitain Airport (large) - 2 runways
	_airport_moving_data_international,	// International Airport (xlarge) - 2 runways
	_airport_moving_data_oilrig					// Oilrig
};

#endif /* AIRPORT_MOVEMENT_H */
