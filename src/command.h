/* $Id$ */

/** @file command.h */

#ifndef COMMAND_H
#define COMMAND_H

enum {
	CMD_BUILD_RAILROAD_TRACK         =   0,
	CMD_REMOVE_RAILROAD_TRACK        =   1,
	CMD_BUILD_SINGLE_RAIL            =   2,
	CMD_REMOVE_SINGLE_RAIL           =   3,
	CMD_LANDSCAPE_CLEAR              =   4,
	CMD_BUILD_BRIDGE                 =   5,
	CMD_BUILD_RAILROAD_STATION       =   6,
	CMD_BUILD_TRAIN_DEPOT            =   7,
	CMD_BUILD_SIGNALS                =   8,
	CMD_REMOVE_SIGNALS               =   9,
	CMD_TERRAFORM_LAND               =  10,
	CMD_PURCHASE_LAND_AREA           =  11,
	CMD_SELL_LAND_AREA               =  12,
	CMD_BUILD_TUNNEL                 =  13,

	CMD_REMOVE_FROM_RAILROAD_STATION =  14,
	CMD_CONVERT_RAIL                 =  15,

	CMD_BUILD_TRAIN_WAYPOINT         =  16,
	CMD_RENAME_WAYPOINT              =  17,
	CMD_REMOVE_TRAIN_WAYPOINT        =  18,

	CMD_BUILD_ROAD_STOP              =  21,
	CMD_REMOVE_ROAD_STOP             =  22,
	CMD_BUILD_LONG_ROAD              =  23,
	CMD_REMOVE_LONG_ROAD             =  24,
	CMD_BUILD_ROAD                   =  25,
	CMD_REMOVE_ROAD                  =  26,
	CMD_BUILD_ROAD_DEPOT             =  27,

	CMD_BUILD_AIRPORT                =  29,

	CMD_BUILD_DOCK                   =  30,

	CMD_BUILD_SHIP_DEPOT             =  31,
	CMD_BUILD_BUOY                   =  32,

	CMD_PLANT_TREE                   =  33,

	CMD_BUILD_RAIL_VEHICLE           =  34,
	CMD_MOVE_RAIL_VEHICLE            =  35,

	CMD_START_STOP_TRAIN             =  36,

	CMD_SELL_RAIL_WAGON              =  38,

	CMD_SEND_TRAIN_TO_DEPOT          =  39,
	CMD_FORCE_TRAIN_PROCEED          =  40,
	CMD_REVERSE_TRAIN_DIRECTION      =  41,

	CMD_MODIFY_ORDER                 =  42,
	CMD_SKIP_ORDER                   =  43,
	CMD_DELETE_ORDER                 =  44,
	CMD_INSERT_ORDER                 =  45,

	CMD_CHANGE_SERVICE_INT           =  46,

	CMD_BUILD_INDUSTRY               =  47,

	CMD_BUILD_COMPANY_HQ             =  48,
	CMD_SET_PLAYER_FACE              =  49,
	CMD_SET_PLAYER_COLOR             =  50,

	CMD_INCREASE_LOAN                =  51,
	CMD_DECREASE_LOAN                =  52,

	CMD_WANT_ENGINE_PREVIEW          =  53,

	CMD_NAME_VEHICLE                 =  54,
	CMD_RENAME_ENGINE                =  55,
	CMD_CHANGE_COMPANY_NAME          =  56,
	CMD_CHANGE_PRESIDENT_NAME        =  57,
	CMD_RENAME_STATION               =  58,

	CMD_SELL_AIRCRAFT                =  59,
	CMD_START_STOP_AIRCRAFT          =  60,
	CMD_BUILD_AIRCRAFT               =  61,
	CMD_SEND_AIRCRAFT_TO_HANGAR      =  62,
	CMD_REFIT_AIRCRAFT               =  64,

	CMD_PLACE_SIGN                   =  65,
	CMD_RENAME_SIGN                  =  66,

	CMD_BUILD_ROAD_VEH               =  67,
	CMD_START_STOP_ROADVEH           =  68,
	CMD_SELL_ROAD_VEH                =  69,
	CMD_SEND_ROADVEH_TO_DEPOT        =  70,
	CMD_TURN_ROADVEH                 =  71,
	CMD_REFIT_ROAD_VEH               =  72,

	CMD_PAUSE                        =  73,

	CMD_BUY_SHARE_IN_COMPANY         =  74,
	CMD_SELL_SHARE_IN_COMPANY        =  75,
	CMD_BUY_COMPANY                  =  76,

	CMD_BUILD_TOWN                   =  77,

	CMD_RENAME_TOWN                  =  80,
	CMD_DO_TOWN_ACTION               =  81,

	CMD_SET_ROAD_DRIVE_SIDE          =  82,

	CMD_CHANGE_DIFFICULTY_LEVEL      =  85,

	CMD_START_STOP_SHIP              =  86,
	CMD_SELL_SHIP                    =  87,
	CMD_BUILD_SHIP                   =  88,
	CMD_SEND_SHIP_TO_DEPOT           =  89,
	CMD_REFIT_SHIP                   =  91,

	CMD_ORDER_REFIT                  =  98,
	CMD_CLONE_ORDER                  =  99,
	CMD_CLEAR_AREA                   = 100,

	CMD_MONEY_CHEAT                  = 102,
	CMD_BUILD_CANAL                  = 103,

	CMD_PLAYER_CTRL                  = 104, ///< used in multiplayer to create a new player etc.
	CMD_LEVEL_LAND                   = 105, ///< level land

	CMD_REFIT_RAIL_VEHICLE           = 106,
	CMD_RESTORE_ORDER_INDEX          = 107,
	CMD_BUILD_LOCK                   = 108,

	CMD_BUILD_SIGNAL_TRACK           = 110,
	CMD_REMOVE_SIGNAL_TRACK          = 111,

	CMD_GIVE_MONEY                   = 113,
	CMD_CHANGE_PATCH_SETTING         = 114,

	CMD_SET_AUTOREPLACE              = 115,

	CMD_CLONE_VEHICLE                = 116,
	CMD_MASS_START_STOP              = 117,
	CMD_DEPOT_SELL_ALL_VEHICLES      = 118,
	CMD_DEPOT_MASS_AUTOREPLACE       = 119,
};

enum {
	DC_EXEC            = 0x01,
	DC_AUTO            = 0x02, ///< don't allow building on structures
	DC_QUERY_COST      = 0x04, ///< query cost only,  don't build.
	DC_NO_WATER        = 0x08, ///< don't allow building on water
	DC_NO_RAIL_OVERLAP = 0x10, ///< don't allow overlap of rails (used in buildrail)
	DC_AI_BUILDING     = 0x20, ///< special building rules for AI
	DC_NO_TOWN_RATING  = 0x40, ///< town rating does not disallow you from building
	DC_FORCETEST       = 0x80, ///< force test too.

	CMD_ERROR = ((int32)0x80000000),
};

#define CMD_MSG(x) ((x)<<16)

enum {
	CMD_AUTO                  = 0x0200,
	CMD_NO_WATER              = 0x0400,
	CMD_NETWORK_COMMAND       = 0x0800, ///< execute the command without sending it on the network
	CMD_NO_TEST_IF_IN_NETWORK = 0x1000, ///< When enabled, the command will bypass the no-DC_EXEC round if in network
	CMD_SHOW_NO_ERROR         = 0x2000,
};

/** Command flags for the command table _command_proc_table */
enum {
	CMD_SERVER  = 0x1, ///< the command can only be initiated by the server
	CMD_OFFLINE = 0x2, ///< the command cannot be executed in a multiplayer game; single-player only
};

typedef int32 CommandProc(TileIndex tile, uint32 flags, uint32 p1, uint32 p2);

struct Command {
	CommandProc *proc;
	byte flags;
};

//#define return_cmd_error(errcode) do { _error_message=(errcode); return CMD_ERROR; } while(0)
#define return_cmd_error(errcode) do { return CMD_ERROR | (errcode); } while (0)

/**
 * Check the return value of a DoCommand*() function
 * @param res the resulting value from the command to be checked
 * @return Return true if the command failed, false otherwise
 */
static inline bool CmdFailed(int32 res)
{
	/* lower 16bits are the StringID of the possible error */
	return res <= (CMD_ERROR | INVALID_STRING_ID);
}

/* command.cpp */
typedef void CommandCallback(bool success, TileIndex tile, uint32 p1, uint32 p2);
int32 DoCommand(TileIndex tile, uint32 p1, uint32 p2, uint32 flags, uint procc);
bool DoCommandP(TileIndex tile, uint32 p1, uint32 p2, CommandCallback *callback, uint32 cmd);

#ifdef ENABLE_NETWORK

void NetworkSend_Command(TileIndex tile, uint32 p1, uint32 p2, uint32 cmd, CommandCallback *callback);
#endif /* ENABLE_NETWORK */

extern const char* _cmd_text; ///< Text, which gets sent with a command

bool IsValidCommand(uint cmd);
byte GetCommandFlags(uint cmd);
int32 GetAvailableMoneyForCommand();

#endif /* COMMAND_H */
