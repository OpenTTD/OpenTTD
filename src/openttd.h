/* $Id$ */
/** @file openttd.h */

#ifndef OPENTTD_H
#define OPENTTD_H

#ifndef VARDEF
#define VARDEF extern
#endif

#include "core/enum_type.hpp"
#include "strings_type.h"

// Forward declarations of structs.
struct Depot;
struct Waypoint;
struct Station;
struct ViewPort;
struct NewsItem;
struct DrawPixelInfo;
struct Group;
typedef byte VehicleOrderID;  ///< The index of an order within its current vehicle (not pool related)
typedef byte LandscapeID;
typedef uint16 EngineID;
typedef uint16 UnitID;

typedef EngineID *EngineList; ///< engine list type placeholder acceptable for C code (see helpers.cpp)

/* IDs used in Pools */
typedef uint16 StationID;
static const StationID INVALID_STATION = 0xFFFF;
typedef uint16 RoadStopID;
typedef uint16 DepotID;
typedef uint16 WaypointID;
typedef uint16 OrderID;
typedef uint16 SignID;
typedef uint16 GroupID;
typedef uint16 EngineRenewID;
typedef uint16 DestinationID;

/* DestinationID must be at least as large as every these below, because it can
 * be any of them
 */
assert_compile(sizeof(DestinationID) == sizeof(DepotID));
assert_compile(sizeof(DestinationID) == sizeof(WaypointID));
assert_compile(sizeof(DestinationID) == sizeof(StationID));

typedef uint32 PlayerFace; ///< player face bits, info see in player_face.h

enum GameModes {
	GM_MENU,
	GM_NORMAL,
	GM_EDITOR
};

enum SwitchModes {
	SM_NONE            =  0,
	SM_NEWGAME         =  1,
	SM_EDITOR          =  2,
	SM_LOAD            =  3,
	SM_MENU            =  4,
	SM_SAVE            =  5,
	SM_GENRANDLAND     =  6,
	SM_LOAD_SCENARIO   =  9,
	SM_START_SCENARIO  = 10,
	SM_START_HEIGHTMAP = 11,
	SM_LOAD_HEIGHTMAP  = 12,
};


/* Modes for GenerateWorld */
enum GenerateWorldModes {
	GW_NEWGAME   = 0,    /* Generate a map for a new game */
	GW_EMPTY     = 1,    /* Generate an empty map (sea-level) */
	GW_RANDOM    = 2,    /* Generate a random map for SE */
	GW_HEIGHTMAP = 3,    /* Generate a newgame from a heightmap */
};

/* Modes for InitializeGame, those are _bits_! */
enum InitializeGameModes {
	IG_NONE       = 0,  /* Don't do anything special */
	IG_DATE_RESET = 1,  /* Reset the date when initializing a game */
};

enum Owner {
	PLAYER_INACTIVE_CLIENT = 253,
	PLAYER_NEW_COMPANY = 254,
	PLAYER_SPECTATOR = 255,
	OWNER_BEGIN     = 0x00,
	PLAYER_FIRST    = 0x00,
	MAX_PLAYERS     = 8,
	OWNER_TOWN      = 0x0F, // a town owns the tile
	OWNER_NONE      = 0x10, // nobody owns the tile
	OWNER_WATER     = 0x11, // "water" owns the tile
	OWNER_END       = 0x12,
	INVALID_OWNER   = 0xFF,
	INVALID_PLAYER  = 0xFF,
	/* Player identifiers All players below MAX_PLAYERS are playable
	* players, above, they are special, computer controlled players */
};

typedef Owner PlayerID;

DECLARE_POSTFIX_INCREMENT(Owner);

/** Define basic enum properties */
template <> struct EnumPropsT<Owner> : MakeEnumPropsT<Owner, byte, OWNER_BEGIN, OWNER_END, INVALID_OWNER> {};
typedef TinyEnumT<Owner> OwnerByte;
typedef OwnerByte PlayerByte;


enum TransportType {
	/* These constants are for now linked to the representation of bridges
	 * and tunnels, so they can be used by GetTileTrackStatus_TunnelBridge.
	 * In an ideal world, these constants would be used everywhere when
	 * accessing tunnels and bridges. For now, you should just not change
	 * the values for road and rail.
	 */
	TRANSPORT_BEGIN = 0,
	TRANSPORT_RAIL = 0,
	TRANSPORT_ROAD = 1,
	TRANSPORT_WATER, // = 2
	TRANSPORT_END,
	INVALID_TRANSPORT = 0xff,
};

/** Define basic enum properties */
template <> struct EnumPropsT<TransportType> : MakeEnumPropsT<TransportType, byte, TRANSPORT_BEGIN, TRANSPORT_END, INVALID_TRANSPORT> {};
typedef TinyEnumT<TransportType> TransportTypeByte;


/* Display Options */
enum {
	DO_SHOW_TOWN_NAMES    = 0,
	DO_SHOW_STATION_NAMES = 1,
	DO_SHOW_SIGNS         = 2,
	DO_FULL_ANIMATION     = 3,
	DO_FULL_DETAIL        = 5,
	DO_WAYPOINTS          = 6,
};

/* Landscape types */
enum {
	LT_TEMPERATE  = 0,
	LT_ARCTIC     = 1,
	LT_TROPIC     = 2,
	LT_TOYLAND    = 3,

	NUM_LANDSCAPE = 4,
};

#define GAME_DIFFICULTY_NUM 18

/** Specific type for Game Difficulty to ease changing the type */
typedef uint16 GDType;
struct GameDifficulty {
	GDType max_no_competitors;
	GDType competitor_start_time;
	GDType number_towns;
	GDType number_industries;
	GDType max_loan;
	GDType initial_interest;
	GDType vehicle_costs;
	GDType competitor_speed;
	GDType competitor_intelligence; // no longer in use
	GDType vehicle_breakdowns;
	GDType subsidy_multiplier;
	GDType construction_cost;
	GDType terrain_type;
	GDType quantity_sea_lakes;
	GDType economy;
	GDType line_reverse_mode;
	GDType disasters;
	GDType town_council_tolerance; // minimum required town ratings to be allowed to demolish stuff
};

struct ViewportSign {
	int32 left;
	int32 top;
	byte width_1, width_2;
};

enum {
	SORT_ASCENDING  = 0,
	SORT_DESCENDING = 1,
	SORT_BY_DATE    = 0,
	SORT_BY_NAME    = 2
};

VARDEF byte _savegame_sort_order;

enum {
	MAX_SCREEN_WIDTH  = 2048,
	MAX_SCREEN_HEIGHT = 1200,
};

/* In certain windows you navigate with the arrow keys. Do not scroll the
 * gameview when here. Bitencoded variable that only allows scrolling if all
 * elements are zero */
enum {
	SCROLL_CON  = 0,
	SCROLL_EDIT = 1,
	SCROLL_SAVE = 2,
	SCROLL_CHAT = 4,
};
VARDEF byte _no_scroll;

/** To have a concurrently running thread interface with the main program, use
 * the OTTD_SendThreadMessage() function. Actions to perform upon the message are handled
 * in the ProcessSentMessage() function */
enum ThreadMsg {
	MSG_OTTD_NO_MESSAGE,
	MSG_OTTD_SAVETHREAD_DONE,
	MSG_OTTD_SAVETHREAD_ERROR,
};

void OTTD_SendThreadMessage(ThreadMsg msg);

extern byte _game_mode;
extern bool _exit_game;
extern byte _pause_game;

#endif /* OPENTTD_H */
