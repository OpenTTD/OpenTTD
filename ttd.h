#ifndef TTD_H
#define TTD_H

// include strings
#include "table/strings.h"

#ifndef VARDEF
#define VARDEF extern
#endif

// use this on non static functions
#define PUBLIC 

typedef struct Rect {
	int left,top,right,bottom;
} Rect;

typedef struct SmallPoint {
	int16 x,y;
} SmallPoint;

typedef struct Point {
	int x,y;
} Point;

typedef struct Pair {
	int a;
	int b;
} Pair;

typedef struct YearMonthDay {
	int year, month, day;
} YearMonthDay;

#include "macros.h"

// Forward declarations of structs.
typedef struct Vehicle Vehicle;
typedef struct Depot Depot;
typedef struct Checkpoint Checkpoint;
typedef struct Window Window;
typedef struct Station Station;
typedef struct ViewPort ViewPort;
typedef struct Town Town;
typedef struct NewsItem NewsItem;
typedef struct Industry Industry;
typedef struct DrawPixelInfo DrawPixelInfo;
typedef uint16 VehicleID;
typedef uint16 StringID;
typedef uint16 SpriteID;
typedef uint32 PalSpriteID;

typedef uint32 WindowNumber;
typedef byte WindowClass;


enum GameModes {
	GM_MENU,
	GM_NORMAL,
	GM_EDITOR
};

enum SwitchModes {
	SM_NONE = 0,
	SM_NEWGAME = 1,
	SM_EDITOR = 2,
	SM_LOAD = 3,
	SM_MENU = 4,
	SM_SAVE = 5,
	SM_GENRANDLAND = 6,
	SM_LOAD_SCENARIO = 9,

};

enum MapTileTypes {
	MP_CLEAR,
	MP_RAILWAY,
	MP_STREET,
	MP_HOUSE,
	MP_TREES,
	MP_STATION,
	MP_WATER,
	MP_STRANGE,
	MP_INDUSTRY,
	MP_TUNNELBRIDGE,
	MP_UNMOVABLE
};

typedef struct TileInfo {
	uint x;
	uint y;
	uint tileh;
	uint type;
	uint map5;
	uint tile;
	uint z;
} TileInfo;

enum {
	NG_EDGE = 1,
};

/* Display Options */
enum {
	DO_SHOW_TOWN_NAMES  = 1,
	DO_SHOW_STATION_NAMES = 2,
	DO_SHOW_SIGNS = 4,
	DO_FULL_ANIMATION = 8,
	DO_TRANS_BUILDINGS = 0x10,
	DO_FULL_DETAIL = 0x20,
	DO_CHECKPOINTS = 0x40,
};

/* Landscape types */
enum {
	LT_NORMAL = 0,
	LT_HILLY = 1,
	LT_DESERT = 2,
	LT_CANDY = 3,

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
	int competitor_intelligence;
	int vehicle_breakdowns;
	int subsidy_multiplier;
	int construction_cost;
	int terrain_type;
	int quantity_sea_lakes;
	int economy;
	int line_reverse_mode;
	int disasters;
	int town_council_tolerance;	// minimum required town ratings to be allowed to demolish stuff
} GameDifficulty;

typedef struct AcceptedCargo {
	int type_1, amount_1;
	int type_2, amount_2;
	int type_3, amount_3;
} AcceptedCargo;

typedef struct TileDesc {
	StringID str;
	byte owner;
	uint16 build_date;
	uint32 dparam[2];
} TileDesc;

typedef struct {
	int16 left, top;
	byte width_1, width_2;
} ViewportSign;

typedef struct SignStruct {
	StringID str;
	ViewportSign sign;
	int16 x,y;
	byte z;
} SignStruct;


typedef int32 CommandProc(int x, int y, uint32 flags, uint32 p1, uint32 p2);

typedef void DrawTileProc(TileInfo *ti);
typedef uint GetSlopeZProc(TileInfo *ti);
typedef int32 ClearTileProc(uint tile, byte flags);
typedef void GetAcceptedCargoProc(uint tile, AcceptedCargo *res);
typedef void GetTileDescProc(uint tile, TileDesc *td);
typedef uint32 GetTileTrackStatusProc(uint tile, int mode);
typedef void GetProducedCargoProc(uint tile, byte *b);
typedef void ClickTileProc(uint tile);
typedef void AnimateTileProc(uint tile);
typedef void TileLoopProc(uint tile);
typedef void ChangeTileOwnerProc(uint tile, byte old_player, byte new_player);
/* Return value has bit 0x2 set, when the vehicle enters a station. Then,
 * result << 8 contains the id of the station entered. If the return value has
 * bit 0x8 set, the vehicle could not and did not enter the tile. Are there
 * other bits that can be set? */
typedef uint32 VehicleEnterTileProc(Vehicle *v, uint tile, int x, int y);
typedef void VehicleLeaveTileProc(Vehicle *v, uint tile, int x, int y);
typedef uint GetSlopeTilehProc(TileInfo *ti);

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
	VehicleLeaveTileProc *vehicle_leave_tile_proc;
	GetSlopeTilehProc *get_slope_tileh_proc;
} TileTypeProcs;



#define MP_SETTYPE(x) ((x+1) << 8)

enum {
	MP_MAP2 = 1<<0,
	MP_MAP3LO = 1<<1,
	MP_MAP3HI = 1<<2,
	MP_MAP5 = 1<<3,
	MP_MAPOWNER_CURRENT = 1<<4,
	MP_MAPOWNER = 1<<5,
		
	MP_TYPE_MASK = 0xF << 8,

	MP_MAP2_CLEAR = 1 << 12,
	MP_MAP3LO_CLEAR = 1 << 13,
	MP_MAP3HI_CLEAR = 1 << 14,

	MP_NODIRTY = 1<<15,
};

enum {
	// Temperate
	CT_PASSENGERS = 0,
	CT_COAL = 1,
	CT_MAIL = 2,
	CT_OIL = 3,
	CT_LIVESTOCK = 4,
	CT_GOODS = 5,
	CT_GRAIN = 6,
	CT_WOOD = 7,
	CT_IRON_ORE = 8,
	CT_STEEL = 9,
	CT_VALUABLES = 10,
	CT_FOOD = 11,

	// Arctic
	CT_HILLY_UNUSED = 8,
	CT_PAPER = 9,

	// Tropic
	CT_RUBBER = 1,
	CT_FRUIT = 4,
	CT_COPPER_ORE = 8,
	CT_WATER = 9,

	// Toyland
	CT_SUGAR = 1,
	CT_TOYS = 3,
	CT_BATTERIES = 4,
	CT_CANDY = 5,
	CT_TOFFEE = 6,
	CT_COLA = 7,
	CT_COTTON_CANDY = 8,
	CT_BUBBLES = 9,
	CT_PLASTIC = 10,
	CT_FIZZY_DRINKS = 11,

	NUM_CARGO = 12,
};

enum {
	WC_MAIN_WINDOW = 0x0,
	WC_MAIN_TOOLBAR = 0x1,
	WC_STATUS_BAR = 0x2,
	WC_BUILD_TOOLBAR = 0x3,
	WC_NEWS_WINDOW = 0x4,
	WC_TOWN_DIRECTORY = 0x5,
	WC_STATION_LIST = 0x6,
	WC_TOWN_VIEW = 0x7,
	WC_SMALLMAP = 0x8,
	WC_TRAINS_LIST = 0x9,
	WC_ROADVEH_LIST = 0xA,
	WC_SHIPS_LIST = 0xB,
	WC_AIRCRAFT_LIST = 0xC,
	WC_VEHICLE_VIEW = 0xD,
	WC_VEHICLE_DETAILS = 0xE,
	WC_VEHICLE_REFIT = 0xF,
	WC_VEHICLE_ORDERS = 0x10,
	WC_STATION_VIEW = 0x11,
	WC_VEHICLE_DEPOT = 0x12,
	WC_BUILD_VEHICLE = 0x13,
	WC_BUILD_BRIDGE = 0x14,
	WC_ERRMSG = 0x15,
	WC_ASK_ABANDON_GAME = 0x16,
	WC_QUIT_GAME = 0x17,
	WC_BUILD_STATION = 0x18,
	WC_BUS_STATION = 0x19,
	WC_TRUCK_STATION = 0x1A,
	WC_BUILD_DEPOT = 0x1B,
	WC_DEBUGGER = 0x1C,
	WC_COMPANY = 0x1D,
	WC_FINANCES = 0x1E,
	WC_PLAYER_COLOR = 0x1F,
	WC_QUERY_STRING = 0x20,
	WC_SAVELOAD = 0x21,
	WC_SELECT_GAME = 0x22,
	WC_TOOLBAR_MENU = 0x24,
	WC_INCOME_GRAPH = 0x25,
	WC_OPERATING_PROFIT = 0x26,
	WC_TOOLTIPS = 0x27,
	WC_INDUSTRY_VIEW = 0x28,
	WC_PLAYER_FACE = 0x29,
	WC_LAND_INFO = 0x2A,
	WC_TOWN_AUTHORITY = 0x2B,
	WC_SUBSIDIES_LIST = 0x2C,
	WC_GRAPH_LEGEND = 0x2D,
	WC_DELIVERED_CARGO = 0x2E,
	WC_PERFORMANCE_HISTORY = 0x2F,
	WC_COMPANY_VALUE = 0x30,
	WC_COMPANY_LEAGUE = 0x31,
	WC_BUY_COMPANY = 0x32,
	WC_PAYMENT_RATES = 0x33,
	WC_SELECT_TUTORIAL = 0x34,
	WC_ENGINE_PREVIEW = 0x35,
	WC_MUSIC_WINDOW = 0x36,
	WC_MUSIC_TRACK_SELECTION = 0x37,
	WC_SCEN_LAND_GEN = 0x38,
	WC_ASK_RESET_LANDSCAPE = 0x39,
	WC_SCEN_TOWN_GEN = 0x3A,
	WC_SCEN_INDUSTRY = 0x3B,
	WC_SCEN_BUILD_ROAD = 0x3C,
	WC_SCEN_BUILD_TREES = 0x3D,
	WC_SEND_NETWORK_MSG = 0x3E,
	WC_DROPDOWN_MENU = 0x3F,
	WC_BUILD_INDUSTRY = 0x40,
	WC_GAME_OPTIONS = 0x41,
	WC_NETWORK_WINDOW = 0x42,
	WC_INDUSTRY_DIRECTORY = 0x43,
	WC_MESSAGE_HISTORY = 0x44,
	WC_CHEATS = 0x45,
	WC_PERFORMANCE_DETAIL = 0x46,
	WC_CONSOLE = 0x47,
	WC_EXTRA_VIEW_PORT = 0x48,
};


enum {
	EXPENSES_CONSTRUCTION = 0,
	EXPENSES_NEW_VEHICLES = 1,
	EXPENSES_TRAIN_RUN = 2,
	EXPENSES_ROADVEH_RUN = 3,
	EXPENSES_AIRCRAFT_RUN = 4,
	EXPENSES_SHIP_RUN = 5,
	EXPENSES_PROPERTY = 6,
	EXPENSES_TRAIN_INC = 7,
	EXPENSES_ROADVEH_INC = 8,
	EXPENSES_AIRCRAFT_INC = 9,
	EXPENSES_SHIP_INC = 10,
	EXPENSES_LOAN_INT = 11,
	EXPENSES_OTHER = 12,
};

// Tile type misc constants, don't know where to put these
enum {
	TRACKTYPE_SPRITE_PITCH = 0x52,
};


// special string constants
enum SpecialStrings {
	
	// special strings for town names. the town name is generated dynamically on request.
	SPECSTR_TOWNNAME_START = 0x20C0,
	SPECSTR_TOWNNAME_ENGLISH = SPECSTR_TOWNNAME_START,
	SPECSTR_TOWNNAME_FRENCH,
	SPECSTR_TOWNNAME_GERMAN,
	SPECSTR_TOWNNAME_AMERICAN,
	SPECSTR_TOWNNAME_LATIN,
	SPECSTR_TOWNNAME_SILLY,
	SPECSTR_TOWNNAME_SWEDISH,
	SPECSTR_TOWNNAME_DUTCH,
	SPECSTR_TOWNNAME_FINNISH,
	SPECSTR_TOWNNAME_POLISH,
	SPECSTR_TOWNNAME_CZECH,
	SPECSTR_TOWNNAME_SLOVAKISH,
	SPECSTR_TOWNNAME_HUNGARIAN,
	SPECSTR_TOWNNAME_AUSTRIAN,
	SPECSTR_TOWNNAME_LAST = SPECSTR_TOWNNAME_AUSTRIAN,

	// special strings for player names on the form "TownName transport".
	SPECSTR_PLAYERNAME_START = 0x70EA,
	SPECSTR_PLAYERNAME_ENGLISH = SPECSTR_PLAYERNAME_START,
	SPECSTR_PLAYERNAME_FRENCH,
	SPECSTR_PLAYERNAME_GERMAN,
	SPECSTR_PLAYERNAME_AMERICAN,
	SPECSTR_PLAYERNAME_LATIN,
	SPECSTR_PLAYERNAME_SILLY,
	SPECSTR_PLAYERNAME_LAST = SPECSTR_PLAYERNAME_SILLY, 

	SPECSTR_ANDCO_NAME = 0x70E6,
	SPECSTR_PRESIDENT_NAME = 0x70E7,
	SPECSTR_SONGNAME = 0x70E8,

	// reserve 32 strings for the *.lng files
	SPECSTR_LANGUAGE_START = 0x7100,
	SPECSTR_LANGUAGE_END = 0x711f,

	// reserve 32 strings for various screen resolutions
	SPECSTR_RESOLUTION_START = 0x7120,
	SPECSTR_RESOLUTION_END = 0x713f,

	// reserve 32 strings for screenshot formats
	SPECSTR_SCREENSHOT_START = 0x7140,
	SPECSTR_SCREENSHOT_END = 0x715F,

	STR_SPEC_SCREENSHOT_NAME = 0xF800,
	STR_SPEC_USERSTRING = 0xF801,
};

typedef void PlaceProc(uint tile);

enum Sprites {
	SPR_CANALS_BASE = 0x1406,
	SPR_SLOPES_BASE = SPR_CANALS_BASE + 70,
	SPR_OPENTTD_BASE = SPR_SLOPES_BASE + 74,
};

enum MAP_OWNERS {
	OWNER_TOWN			= 0xf, // a town owns the tile
	OWNER_NONE			= 0x10,// nobody owns the tile
	OWNER_WATER			= 0x11,// "water" owns the tile
	OWNER_SPECTATOR	= 0xff, // spectator in MP or in scenario editor
};

VARDEF bool _savegame_sort_dirty;
VARDEF byte _savegame_sort_order;

#define INVALID_UINT_TILE (uint)0xFFFFFFFF
#define INVALID_STRING_ID 0xFFFF

enum {
	MAX_SCREEN_WIDTH = 2048,
	MAX_SCREEN_HEIGHT = 1200,
};

#include "functions.h"
#include "variables.h"

#endif /* TTD_H */
