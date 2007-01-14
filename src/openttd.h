/* $Id$ */
/** @file openttd.h */

#ifndef OPENTTD_H
#define OPENTTD_H

#ifndef VARDEF
#define VARDEF extern
#endif

#include "hal.h"
#include "helpers.hpp"

typedef struct Oblong {
	int x, y;
	int width, height;
} Oblong;

typedef struct BoundingRect {
	int width;
	int height;
} BoundingRect;

typedef struct Pair {
	int a;
	int b;
} Pair;

#include "map.h"
#include "slope.h"

// Forward declarations of structs.
typedef struct Vehicle Vehicle;
typedef struct Depot Depot;
typedef struct Waypoint Waypoint;
typedef struct Window Window;
typedef struct Station Station;
typedef struct ViewPort ViewPort;
typedef struct Town Town;
typedef struct NewsItem NewsItem;
typedef struct Industry Industry;
typedef struct DrawPixelInfo DrawPixelInfo;
typedef byte VehicleOrderID;  ///< The index of an order within its current vehicle (not pool related)
typedef byte CargoID;
typedef byte LandscapeID;
typedef uint32 SpriteID;      ///< The number of a sprite, without mapping bits and colortables
typedef struct PalSpriteID {
	SpriteID sprite;
	SpriteID pal;
} PalSpriteID;
typedef uint16 EngineID;
typedef uint16 UnitID;
typedef uint16 StringID;
typedef EngineID *EngineList; ///< engine list type placeholder acceptable for C code (see helpers.cpp)

/* IDs used in Pools */
typedef uint16 VehicleID;
typedef uint16 StationID;
typedef uint16 RoadStopID;
typedef uint16 TownID;
typedef uint16 IndustryID;
typedef uint16 DepotID;
typedef uint16 WaypointID;
typedef uint16 OrderID;
typedef uint16 SignID;
typedef uint16 EngineRenewID;
typedef uint16 DestinationID;

/* DestinationID must be at least as large as every these below, because it can
 * be any of them
 */
assert_compile(sizeof(DestinationID) == sizeof(DepotID));
assert_compile(sizeof(DestinationID) == sizeof(WaypointID));
assert_compile(sizeof(DestinationID) == sizeof(StationID));

typedef int32 WindowNumber;
typedef byte WindowClass;

enum {
	INVALID_YEAR = -1,
	INVALID_DATE = -1,
};

typedef int32 Year;
typedef int32 Date;


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


typedef enum TransportTypes {
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
} TransportType;

/** Define basic enum properties */
template <> struct EnumPropsT<TransportType> : MakeEnumPropsT<TransportType, byte, TRANSPORT_BEGIN, TRANSPORT_END, INVALID_TRANSPORT> {};
typedef TinyEnumT<TransportType> TransportTypeByte;


typedef struct TileInfo {
	uint x;
	uint y;
	Slope tileh;
	TileIndex tile;
	uint z;
} TileInfo;


/* Display Options */
enum {
	DO_SHOW_TOWN_NAMES    = 1 << 0,
	DO_SHOW_STATION_NAMES = 1 << 1,
	DO_SHOW_SIGNS         = 1 << 2,
	DO_FULL_ANIMATION     = 1 << 3,
	DO_TRANS_BUILDINGS    = 1 << 4,
	DO_FULL_DETAIL        = 1 << 5,
	DO_WAYPOINTS          = 1 << 6,
	DO_TRANS_SIGNS        = 1 << 7,
};

/* Landscape types */
enum {
	LT_NORMAL     = 0,
	LT_HILLY      = 1,
	LT_DESERT     = 2,
	LT_CANDY      = 3,

	NUM_LANDSCAPE = 4,
};

enum {
	NUM_PRICES = 49,
};

typedef struct Prices {
	int32 station_value;
	int32 build_rail;
	int32 build_road;
	int32 build_signals;
	int32 build_bridge;
	int32 build_train_depot;
	int32 build_road_depot;
	int32 build_ship_depot;
	int32 build_tunnel;
	int32 train_station_track;
	int32 train_station_length;
	int32 build_airport;
	int32 build_bus_station;
	int32 build_truck_station;
	int32 build_dock;
	int32 build_railvehicle;
	int32 build_railwagon;
	int32 aircraft_base;
	int32 roadveh_base;
	int32 ship_base;
	int32 build_trees;
	int32 terraform;
	int32 clear_1;
	int32 purchase_land;
	int32 clear_2;
	int32 clear_3;
	int32 remove_trees;
	int32 remove_rail;
	int32 remove_signals;
	int32 clear_bridge;
	int32 remove_train_depot;
	int32 remove_road_depot;
	int32 remove_ship_depot;
	int32 clear_tunnel;
	int32 clear_water;
	int32 remove_rail_station;
	int32 remove_airport;
	int32 remove_bus_station;
	int32 remove_truck_station;
	int32 remove_dock;
	int32 remove_house;
	int32 remove_road;
	int32 running_rail[3];
	int32 aircraft_running;
	int32 roadveh_running;
	int32 ship_running;
	int32 build_industry;
} Prices;

#define GAME_DIFFICULTY_NUM 18

typedef struct GameDifficulty {
	int max_no_competitors;
	int competitor_start_time;
	int number_towns;
	int number_industries;
	int max_loan;
	int initial_interest;
	int vehicle_costs;
	int competitor_speed;
	int competitor_intelligence; // no longer in use
	int vehicle_breakdowns;
	int subsidy_multiplier;
	int construction_cost;
	int terrain_type;
	int quantity_sea_lakes;
	int economy;
	int line_reverse_mode;
	int disasters;
	int town_council_tolerance; // minimum required town ratings to be allowed to demolish stuff
} GameDifficulty;

enum {
	// Temperate
	CT_PASSENGERS   =  0,
	CT_COAL         =  1,
	CT_MAIL         =  2,
	CT_OIL          =  3,
	CT_LIVESTOCK    =  4,
	CT_GOODS        =  5,
	CT_GRAIN        =  6,
	CT_WOOD         =  7,
	CT_IRON_ORE     =  8,
	CT_STEEL        =  9,
	CT_VALUABLES    = 10,
	CT_FOOD         = 11,

	// Arctic
	CT_WHEAT        =  6,
	CT_HILLY_UNUSED =  8,
	CT_PAPER        =  9,
	CT_GOLD         = 10,

	// Tropic
	CT_RUBBER       =  1,
	CT_FRUIT        =  4,
	CT_MAIZE        =  6,
	CT_COPPER_ORE   =  8,
	CT_WATER        =  9,
	CT_DIAMONDS     = 10,

	// Toyland
	CT_SUGAR        =  1,
	CT_TOYS         =  3,
	CT_BATTERIES    =  4,
	CT_CANDY        =  5,
	CT_TOFFEE       =  6,
	CT_COLA         =  7,
	CT_COTTON_CANDY =  8,
	CT_BUBBLES      =  9,
	CT_PLASTIC      = 10,
	CT_FIZZY_DRINKS = 11,

	NUM_CARGO       = 12,

	CT_NO_REFIT     = 0xFE,
	CT_INVALID      = 0xFF
};

typedef uint AcceptedCargo[NUM_CARGO];

typedef struct TileDesc {
	StringID str;
	Owner owner;
	Date build_date;
	uint32 dparam[2];
} TileDesc;

typedef struct {
	int32 left;
	int32 top;
	byte width_1, width_2;
} ViewportSign;


typedef void DrawTileProc(TileInfo *ti);
typedef uint GetSlopeZProc(TileIndex tile, uint x, uint y);
typedef int32 ClearTileProc(TileIndex tile, byte flags);
typedef void GetAcceptedCargoProc(TileIndex tile, AcceptedCargo res);
typedef void GetTileDescProc(TileIndex tile, TileDesc *td);
/* GetTileTrackStatusProcs return a value that contains the possible tracks
 * that can be taken on a given tile by a given transport. The return value is
 * composed as follows: 0xaabbccdd. ccdd and aabb are bitmasks of trackdirs,
 * where bit n corresponds to trackdir n. ccdd are the trackdirs that are
 * present in the tile (1==present, 0==not present), aabb is the signal
 * status, if applicable (0==green/no signal, 1==red, note that this is
 * reversed from map3/2[tile] for railway signals).
 *
 * The result (let's call it ts) is often used as follows:
 * tracks = (byte)(ts | ts >>8)
 * This effectively converts the present part of the result (ccdd) to a
 * track bitmask, which disregards directions. Normally, this is the same as just
 * doing (byte)ts I think, although I am not really sure
 *
 * A trackdir is combination of a track and a dir, where the lower three bits
 * are a track, the fourth bit is the direction. these give 12 (or 14)
 * possible options: 0-5 and 8-13, so we need 14 bits for a trackdir bitmask
 * above.
 */
typedef uint32 GetTileTrackStatusProc(TileIndex tile, TransportType mode);
typedef void GetProducedCargoProc(TileIndex tile, CargoID *b);
typedef void ClickTileProc(TileIndex tile);
typedef void AnimateTileProc(TileIndex tile);
typedef void TileLoopProc(TileIndex tile);
typedef void ChangeTileOwnerProc(TileIndex tile, PlayerID old_player, PlayerID new_player);
/* Return value has bit 0x2 set, when the vehicle enters a station. Then,
 * result << 8 contains the id of the station entered. If the return value has
 * bit 0x8 set, the vehicle could not and did not enter the tile. Are there
 * other bits that can be set? */
typedef uint32 VehicleEnterTileProc(Vehicle *v, TileIndex tile, int x, int y);
typedef Slope GetSlopeTilehProc(TileIndex, Slope tileh);

typedef struct {
	DrawTileProc *draw_tile_proc;
	GetSlopeZProc *get_slope_z_proc;
	ClearTileProc *clear_tile_proc;
	GetAcceptedCargoProc *get_accepted_cargo_proc;
	GetTileDescProc *get_tile_desc_proc;
	GetTileTrackStatusProc *get_tile_track_status_proc;
	ClickTileProc *click_tile_proc;
	AnimateTileProc *animate_tile_proc;
	TileLoopProc *tile_loop_proc;
	ChangeTileOwnerProc *change_tile_owner_proc;
	GetProducedCargoProc *get_produced_cargo_proc;
	VehicleEnterTileProc *vehicle_enter_tile_proc;
	GetSlopeTilehProc *get_slope_tileh_proc;
} TileTypeProcs;


enum {
	WC_MAIN_WINDOW,
	WC_MAIN_TOOLBAR,
	WC_STATUS_BAR,
	WC_BUILD_TOOLBAR,
	WC_NEWS_WINDOW,
	WC_TOWN_DIRECTORY,
	WC_STATION_LIST,
	WC_TOWN_VIEW,
	WC_SMALLMAP,
	WC_TRAINS_LIST,
	WC_ROADVEH_LIST,
	WC_SHIPS_LIST,
	WC_AIRCRAFT_LIST,
	WC_VEHICLE_VIEW,
	WC_VEHICLE_DETAILS,
	WC_VEHICLE_REFIT,
	WC_VEHICLE_ORDERS,
	WC_STATION_VIEW,
	WC_VEHICLE_DEPOT,
	WC_BUILD_VEHICLE,
	WC_BUILD_BRIDGE,
	WC_ERRMSG,
	WC_BUILD_STATION,
	WC_BUS_STATION,
	WC_TRUCK_STATION,
	WC_BUILD_DEPOT,
	WC_COMPANY,
	WC_FINANCES,
	WC_PLAYER_COLOR,
	WC_QUERY_STRING,
	WC_SAVELOAD,
	WC_SELECT_GAME,
	WC_TOOLBAR_MENU,
	WC_INCOME_GRAPH,
	WC_OPERATING_PROFIT,
	WC_TOOLTIPS,
	WC_INDUSTRY_VIEW,
	WC_PLAYER_FACE,
	WC_LAND_INFO,
	WC_TOWN_AUTHORITY,
	WC_SUBSIDIES_LIST,
	WC_GRAPH_LEGEND,
	WC_DELIVERED_CARGO,
	WC_PERFORMANCE_HISTORY,
	WC_COMPANY_VALUE,
	WC_COMPANY_LEAGUE,
	WC_BUY_COMPANY,
	WC_PAYMENT_RATES,
	WC_ENGINE_PREVIEW,
	WC_MUSIC_WINDOW,
	WC_MUSIC_TRACK_SELECTION,
	WC_SCEN_LAND_GEN,
	WC_SCEN_TOWN_GEN,
	WC_SCEN_INDUSTRY,
	WC_SCEN_BUILD_ROAD,
	WC_BUILD_TREES,
	WC_SEND_NETWORK_MSG,
	WC_DROPDOWN_MENU,
	WC_BUILD_INDUSTRY,
	WC_GAME_OPTIONS,
	WC_NETWORK_WINDOW,
	WC_INDUSTRY_DIRECTORY,
	WC_MESSAGE_HISTORY,
	WC_CHEATS,
	WC_PERFORMANCE_DETAIL,
	WC_CONSOLE,
	WC_EXTRA_VIEW_PORT,
	WC_CLIENT_LIST,
	WC_NETWORK_STATUS_WINDOW,
	WC_CUSTOM_CURRENCY,
	WC_REPLACE_VEHICLE,
	WC_HIGHSCORE,
	WC_ENDSCREEN,
	WC_SIGN_LIST,
	WC_GENERATE_LANDSCAPE,
	WC_GENERATE_PROGRESS_WINDOW,
	WC_CONFIRM_POPUP_QUERY,
};


enum {
	EXPENSES_CONSTRUCTION =  0,
	EXPENSES_NEW_VEHICLES =  1,
	EXPENSES_TRAIN_RUN    =  2,
	EXPENSES_ROADVEH_RUN  =  3,
	EXPENSES_AIRCRAFT_RUN =  4,
	EXPENSES_SHIP_RUN     =  5,
	EXPENSES_PROPERTY     =  6,
	EXPENSES_TRAIN_INC    =  7,
	EXPENSES_ROADVEH_INC  =  8,
	EXPENSES_AIRCRAFT_INC =  9,
	EXPENSES_SHIP_INC     = 10,
	EXPENSES_LOAN_INT     = 11,
	EXPENSES_OTHER        = 12,
};

enum {
	MAX_LANG = 64,
};

// special string constants
enum SpecialStrings {

	// special strings for town names. the town name is generated dynamically on request.
	SPECSTR_TOWNNAME_START     = 0x20C0,
	SPECSTR_TOWNNAME_ENGLISH   = SPECSTR_TOWNNAME_START,
	SPECSTR_TOWNNAME_FRENCH,
	SPECSTR_TOWNNAME_GERMAN,
	SPECSTR_TOWNNAME_AMERICAN,
	SPECSTR_TOWNNAME_LATIN,
	SPECSTR_TOWNNAME_SILLY,
	SPECSTR_TOWNNAME_SWEDISH,
	SPECSTR_TOWNNAME_DUTCH,
	SPECSTR_TOWNNAME_FINNISH,
	SPECSTR_TOWNNAME_POLISH,
	SPECSTR_TOWNNAME_SLOVAKISH,
	SPECSTR_TOWNNAME_NORWEGIAN,
	SPECSTR_TOWNNAME_HUNGARIAN,
	SPECSTR_TOWNNAME_AUSTRIAN,
	SPECSTR_TOWNNAME_ROMANIAN,
	SPECSTR_TOWNNAME_CZECH,
	SPECSTR_TOWNNAME_SWISS,
	SPECSTR_TOWNNAME_DANISH,
	SPECSTR_TOWNNAME_TURKISH,
	SPECSTR_TOWNNAME_ITALIAN,
	SPECSTR_TOWNNAME_CATALAN,
	SPECSTR_TOWNNAME_LAST      = SPECSTR_TOWNNAME_CATALAN,

	// special strings for player names on the form "TownName transport".
	SPECSTR_PLAYERNAME_START   = 0x70EA,
	SPECSTR_PLAYERNAME_ENGLISH = SPECSTR_PLAYERNAME_START,
	SPECSTR_PLAYERNAME_FRENCH,
	SPECSTR_PLAYERNAME_GERMAN,
	SPECSTR_PLAYERNAME_AMERICAN,
	SPECSTR_PLAYERNAME_LATIN,
	SPECSTR_PLAYERNAME_SILLY,
	SPECSTR_PLAYERNAME_LAST    = SPECSTR_PLAYERNAME_SILLY,

	SPECSTR_ANDCO_NAME         = 0x70E6,
	SPECSTR_PRESIDENT_NAME     = 0x70E7,
	SPECSTR_SONGNAME           = 0x70E8,

	// reserve MAX_LANG strings for the *.lng files
	SPECSTR_LANGUAGE_START     = 0x7100,
	SPECSTR_LANGUAGE_END       = SPECSTR_LANGUAGE_START + MAX_LANG - 1,

	// reserve 32 strings for various screen resolutions
	SPECSTR_RESOLUTION_START   = SPECSTR_LANGUAGE_END + 1,
	SPECSTR_RESOLUTION_END     = SPECSTR_RESOLUTION_START + 0x1F,

	// reserve 32 strings for screenshot formats
	SPECSTR_SCREENSHOT_START   = SPECSTR_RESOLUTION_END + 1,
	SPECSTR_SCREENSHOT_END     = SPECSTR_SCREENSHOT_START + 0x1F,

	// Used to implement SetDParamStr
	STR_SPEC_DYNSTRING         = 0xF800,
	STR_SPEC_USERSTRING        = 0xF808,
};

typedef void PlaceProc(TileIndex tile);

enum {
	SORT_ASCENDING  = 0,
	SORT_DESCENDING = 1,
	SORT_BY_DATE    = 0,
	SORT_BY_NAME    = 2
};

VARDEF byte _savegame_sort_order;

#define INVALID_STRING_ID 0xFFFF

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
typedef enum ThreadMsgs {
	MSG_OTTD_NO_MESSAGE,
	MSG_OTTD_SAVETHREAD_DONE,
	MSG_OTTD_SAVETHREAD_ERROR,
} ThreadMsg;

void OTTD_SendThreadMessage(ThreadMsg msg);

#endif /* OPENTTD_H */
