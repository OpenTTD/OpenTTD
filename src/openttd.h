/* $Id$ */
/** @file openttd.h */

#ifndef OPENTTD_H
#define OPENTTD_H

#ifndef VARDEF
#define VARDEF extern
#endif

#include "helpers.hpp"
#include "strings_type.h"

struct Oblong {
	int x, y;
	int width, height;
};

struct BoundingRect {
	int width;
	int height;
};

struct Pair {
	int a;
	int b;
};

#include "map.h"
#include "slope_type.h"
#include "vehicle_type.h"

// Forward declarations of structs.
struct Depot;
struct Waypoint;
struct Station;
struct ViewPort;
struct Town;
struct NewsItem;
struct Industry;
struct DrawPixelInfo;
struct Group;
typedef byte VehicleOrderID;  ///< The index of an order within its current vehicle (not pool related)
typedef byte CargoID;
typedef byte LandscapeID;
typedef uint32 SpriteID;      ///< The number of a sprite, without mapping bits and colortables
struct PalSpriteID {
	SpriteID sprite;
	SpriteID pal;
};
typedef uint16 EngineID;
typedef uint16 UnitID;

typedef EngineID *EngineList; ///< engine list type placeholder acceptable for C code (see helpers.cpp)

/* IDs used in Pools */
typedef uint16 StationID;
static const StationID INVALID_STATION = 0xFFFF;
typedef uint16 RoadStopID;
typedef uint16 TownID;
typedef uint16 IndustryID;
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


enum {
	INVALID_YEAR = -1,
	INVALID_DATE = -1,
};

typedef int32 Year;
typedef int32 Date;

typedef uint32 PlayerFace; ///< player face bits, info see in player_face.h

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


struct TileInfo {
	uint x;
	uint y;
	Slope tileh;
	TileIndex tile;
	uint z;
};


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

/**
 * Town Layouts
 */
enum TownLayout {
	TL_NO_ROADS     = 0, ///< Build no more roads, but still build houses
	TL_ORIGINAL,         ///< Original algorithm (min. 1 distance between roads)
	TL_BETTER_ROADS,     ///< Extended original algorithm (min. 2 distance between roads)
	TL_2X2_GRID,         ///< Geometric 2x2 grid algorithm
	TL_3X3_GRID,         ///< Geometric 3x3 grid algorithm

	NUM_TLS,             ///< Number of town layouts
};

/* It needs to be 8bits, because we save and load it as such */
/** Define basic enum properties */
template <> struct EnumPropsT<TownLayout> : MakeEnumPropsT<TownLayout, byte, TL_NO_ROADS, NUM_TLS, NUM_TLS> {};
typedef TinyEnumT<TownLayout> TownLayoutByte; //typedefing-enumification of TownLayout

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

	// Arctic
	CT_WHEAT        =  6,
	CT_HILLY_UNUSED =  8,
	CT_PAPER        =  9,
	CT_GOLD         = 10,
	CT_FOOD         = 11,

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

	NUM_CARGO       = 32,

	CT_NO_REFIT     = 0xFE,
	CT_INVALID      = 0xFF
};

typedef uint AcceptedCargo[NUM_CARGO];

struct TileDesc {
	StringID str;
	Owner owner;
	Date build_date;
	uint64 dparam[2];
};

struct ViewportSign {
	int32 left;
	int32 top;
	byte width_1, width_2;
};


#include "command_type.h"
typedef void DrawTileProc(TileInfo *ti);
typedef uint GetSlopeZProc(TileIndex tile, uint x, uint y);
typedef CommandCost ClearTileProc(TileIndex tile, byte flags);
typedef void GetAcceptedCargoProc(TileIndex tile, AcceptedCargo res);
typedef void GetTileDescProc(TileIndex tile, TileDesc *td);
/**
 * GetTileTrackStatusProcs return a value that contains the possible tracks
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
 * @param tile     the tile to get the track status from
 * @param mode     the mode of transportation
 * @param sub_mode used to differentiate between different kinds within the mode
 * @return the above mentions track status information
 */
typedef uint32 GetTileTrackStatusProc(TileIndex tile, TransportType mode, uint sub_mode);
typedef void GetProducedCargoProc(TileIndex tile, CargoID *b);
typedef void ClickTileProc(TileIndex tile);
typedef void AnimateTileProc(TileIndex tile);
typedef void TileLoopProc(TileIndex tile);
typedef void ChangeTileOwnerProc(TileIndex tile, PlayerID old_player, PlayerID new_player);
/** @see VehicleEnterTileStatus to see what the return values mean */
typedef uint32 VehicleEnterTileProc(Vehicle *v, TileIndex tile, int x, int y);
typedef Foundation GetFoundationProc(TileIndex tile, Slope tileh);
/**
 * Called when a tile is affected by a terraforming operation.
 * The function has to check if terraforming of the tile is allowed and return extra terraform-cost that depend on the tiletype.
 * With DC_EXEC in flags it has to perform tiletype-specific actions (like clearing land etc., but not the terraforming itself).
 *
 * @note The terraforming has not yet taken place. So GetTileZ() and GetTileSlope() refer to the landscape before the terraforming operation.
 *
 * @param tile      The involved tile.
 * @param flags     Command flags passed to the terraform command (DC_EXEC, DC_QUERY_COST, etc.).
 * @param z_new     TileZ after terraforming.
 * @param tileh_new Slope after terraforming.
 * @return Error code or extra cost for terraforming (like clearing land, building foundations, etc., but not the terraforming itself.)
 */
typedef CommandCost TerraformTileProc(TileIndex tile, uint32 flags, uint z_new, Slope tileh_new);

struct TileTypeProcs {
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
	GetFoundationProc *get_foundation_proc;
	TerraformTileProc *terraform_tile_proc;
};

typedef void PlaceProc(TileIndex tile);

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

#endif /* OPENTTD_H */
