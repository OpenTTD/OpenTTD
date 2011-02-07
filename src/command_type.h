/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file command_type.h Types related to commands. */

#ifndef COMMAND_TYPE_H
#define COMMAND_TYPE_H

#include "economy_type.h"
#include "strings_type.h"
#include "tile_type.h"

/**
 * Common return value for all commands. Wraps the cost and
 * a possible error message/state together.
 */
class CommandCost {
	ExpensesType expense_type; ///< the type of expence as shown on the finances view
	Money cost;       ///< The cost of this action
	StringID message; ///< Warning message for when success is unset
	bool success;     ///< Whether the comment went fine up to this moment

public:
	/**
	 * Creates a command cost return with no cost and no error
	 */
	CommandCost() : expense_type(INVALID_EXPENSES), cost(0), message(INVALID_STRING_ID), success(true) {}

	/**
	 * Creates a command return value the is failed with the given message
	 */
	explicit CommandCost(StringID msg) : expense_type(INVALID_EXPENSES), cost(0), message(msg), success(false) {}

	/**
	 * Creates a command cost with given expense type and start cost of 0
	 * @param ex_t the expense type
	 */
	explicit CommandCost(ExpensesType ex_t) : expense_type(ex_t), cost(0), message(INVALID_STRING_ID), success(true) {}

	/**
	 * Creates a command return value with the given start cost and expense type
	 * @param ex_t the expense type
	 * @param cst the initial cost of this command
	 */
	CommandCost(ExpensesType ex_t, const Money &cst) : expense_type(ex_t), cost(cst), message(INVALID_STRING_ID), success(true) {}


	/**
	 * Adds the given cost to the cost of the command.
	 * @param cost the cost to add
	 */
	FORCEINLINE void AddCost(const Money &cost)
	{
		this->cost += cost;
	}

	void AddCost(const CommandCost &cmd_cost);

	/**
	 * Multiplies the cost of the command by the given factor.
	 * @param factor factor to multiply the costs with
	 */
	FORCEINLINE void MultiplyCost(int factor)
	{
		this->cost *= factor;
	}

	/**
	 * The costs as made up to this moment
	 * @return the costs
	 */
	FORCEINLINE Money GetCost() const
	{
		return this->cost;
	}

	/**
	 * The expense type of the cost
	 * @return the expense type
	 */
	FORCEINLINE ExpensesType GetExpensesType() const
	{
		return this->expense_type;
	}

	/**
	 * Makes this #CommandCost behave like an error command.
	 * @param message The error message.
	 */
	void MakeError(StringID message)
	{
		assert(message != INVALID_STRING_ID);
		this->success = false;
		this->message = message;
	}

	/**
	 * Returns the error message of a command
	 * @return the error message, if succeeded #INVALID_STRING_ID
	 */
	StringID GetErrorMessage() const
	{
		if (this->success) return INVALID_STRING_ID;
		return this->message;
	}

	/**
	 * Did this command succeed?
	 * @return true if and only if it succeeded
	 */
	FORCEINLINE bool Succeeded() const
	{
		return this->success;
	}

	/**
	 * Did this command fail?
	 * @return true if and only if it failed
	 */
	FORCEINLINE bool Failed() const
	{
		return !this->success;
	}
};

/**
 * List of commands.
 *
 * This enum defines all possible commands which can be executed to the game
 * engine. Observing the game like the query-tool or checking the profit of a
 * vehicle don't result in a command which should be executed in the engine
 * nor send to the server in a network game.
 *
 * @see _command_proc_table
 */
enum Commands {
	CMD_BUILD_RAILROAD_TRACK,         ///< build a rail track
	CMD_REMOVE_RAILROAD_TRACK,        ///< remove a rail track
	CMD_BUILD_SINGLE_RAIL,            ///< build a single rail track
	CMD_REMOVE_SINGLE_RAIL,           ///< remove a single rail track
	CMD_LANDSCAPE_CLEAR,              ///< demolish a tile
	CMD_BUILD_BRIDGE,                 ///< build a bridge
	CMD_BUILD_RAIL_STATION,           ///< build a rail station
	CMD_BUILD_TRAIN_DEPOT,            ///< build a train depot
	CMD_BUILD_SIGNALS,                ///< build a signal
	CMD_REMOVE_SIGNALS,               ///< remove a signal
	CMD_TERRAFORM_LAND,               ///< terraform a tile
	CMD_BUILD_OBJECT,                 ///< build an object
	CMD_BUILD_TUNNEL,                 ///< build a tunnel

	CMD_REMOVE_FROM_RAIL_STATION,     ///< remove a (rectangle of) tiles from a rail station
	CMD_CONVERT_RAIL,                 ///< convert a rail type

	CMD_BUILD_RAIL_WAYPOINT,          ///< build a waypoint
	CMD_RENAME_WAYPOINT,              ///< rename a waypoint
	CMD_REMOVE_FROM_RAIL_WAYPOINT,    ///< remove a (rectangle of) tiles from a rail waypoint

	CMD_BUILD_ROAD_STOP,              ///< build a road stop
	CMD_REMOVE_ROAD_STOP,             ///< remove a road stop
	CMD_BUILD_LONG_ROAD,              ///< build a complete road (not a "half" one)
	CMD_REMOVE_LONG_ROAD,             ///< remove a complete road (not a "half" one)
	CMD_BUILD_ROAD,                   ///< build a "half" road
	CMD_BUILD_ROAD_DEPOT,             ///< build a road depot

	CMD_BUILD_AIRPORT,                ///< build an airport

	CMD_BUILD_DOCK,                   ///< build a dock

	CMD_BUILD_SHIP_DEPOT,             ///< build a ship depot
	CMD_BUILD_BUOY,                   ///< build a buoy

	CMD_PLANT_TREE,                   ///< plant a tree

	CMD_BUILD_VEHICLE,                ///< build a vehicle
	CMD_SELL_VEHICLE,                 ///< sell a vehicle
	CMD_REFIT_VEHICLE,                ///< refit the cargo space of a vehicle
	CMD_SEND_VEHICLE_TO_DEPOT,        ///< send a vehicle to a depot

	CMD_MOVE_RAIL_VEHICLE,            ///< move a rail vehicle (in the depot)
	CMD_FORCE_TRAIN_PROCEED,          ///< proceed a train to pass a red signal
	CMD_REVERSE_TRAIN_DIRECTION,      ///< turn a train around

	CMD_CLEAR_ORDER_BACKUP,           ///< clear the order backup of a given user/tile
	CMD_MODIFY_ORDER,                 ///< modify an order (like set full-load)
	CMD_SKIP_TO_ORDER,                ///< skip an order to the next of specific one
	CMD_DELETE_ORDER,                 ///< delete an order
	CMD_INSERT_ORDER,                 ///< insert a new order

	CMD_CHANGE_SERVICE_INT,           ///< change the server interval of a vehicle

	CMD_BUILD_INDUSTRY,               ///< build a new industry

	CMD_SET_COMPANY_MANAGER_FACE,     ///< set the manager's face of the company
	CMD_SET_COMPANY_COLOUR,           ///< set the colour of the company

	CMD_INCREASE_LOAN,                ///< increase the loan from the bank
	CMD_DECREASE_LOAN,                ///< decrease the loan from the bank

	CMD_WANT_ENGINE_PREVIEW,          ///< confirm the preview of an engine

	CMD_RENAME_VEHICLE,               ///< rename a whole vehicle
	CMD_RENAME_ENGINE,                ///< rename a engine (in the engine list)
	CMD_RENAME_COMPANY,               ///< change the company name
	CMD_RENAME_PRESIDENT,             ///< change the president name
	CMD_RENAME_STATION,               ///< rename a station
	CMD_RENAME_DEPOT,                 ///< rename a depot

	CMD_PLACE_SIGN,                   ///< place a sign
	CMD_RENAME_SIGN,                  ///< rename a sign

	CMD_TURN_ROADVEH,                 ///< turn a road vehicle around

	CMD_PAUSE,                        ///< pause the game

	CMD_BUY_SHARE_IN_COMPANY,         ///< buy a share from a company
	CMD_SELL_SHARE_IN_COMPANY,        ///< sell a share from a company
	CMD_BUY_COMPANY,                  ///< buy a company which is bankrupt

	CMD_FOUND_TOWN,                   ///< found a town
	CMD_RENAME_TOWN,                  ///< rename a town
	CMD_DO_TOWN_ACTION,               ///< do a action from the town detail window (like advertises or bribe)
	CMD_EXPAND_TOWN,                  ///< expand a town
	CMD_DELETE_TOWN,                  ///< delete a town

	CMD_ORDER_REFIT,                  ///< change the refit informaction of an order (for "goto depot" )
	CMD_CLONE_ORDER,                  ///< clone (and share) an order
	CMD_CLEAR_AREA,                   ///< clear an area

	CMD_MONEY_CHEAT,                  ///< do the money cheat
	CMD_BUILD_CANAL,                  ///< build a canal

	CMD_COMPANY_CTRL,                 ///< used in multiplayer to create a new companies etc.
	CMD_LEVEL_LAND,                   ///< level land

	CMD_BUILD_LOCK,                   ///< build a lock

	CMD_BUILD_SIGNAL_TRACK,           ///< add signals along a track (by dragging)
	CMD_REMOVE_SIGNAL_TRACK,          ///< remove signals along a track (by dragging)

	CMD_GIVE_MONEY,                   ///< give money to another company
	CMD_CHANGE_SETTING,               ///< change a setting
	CMD_CHANGE_COMPANY_SETTING,       ///< change a company etting

	CMD_SET_AUTOREPLACE,              ///< set an autoreplace entry

	CMD_CLONE_VEHICLE,                ///< clone a vehicle
	CMD_START_STOP_VEHICLE,           ///< start or stop a vehicle
	CMD_MASS_START_STOP,              ///< start/stop all vehicles (in a depot)
	CMD_AUTOREPLACE_VEHICLE,          ///< replace/renew a vehicle while it is in a depot
	CMD_DEPOT_SELL_ALL_VEHICLES,      ///< sell all vehicles which are in a given depot
	CMD_DEPOT_MASS_AUTOREPLACE,       ///< force the autoreplace to take action in a given depot

	CMD_CREATE_GROUP,                 ///< create a new group
	CMD_DELETE_GROUP,                 ///< delete a group
	CMD_RENAME_GROUP,                 ///< rename a group
	CMD_ADD_VEHICLE_GROUP,            ///< add a vehicle to a group
	CMD_ADD_SHARED_VEHICLE_GROUP,     ///< add all other shared vehicles to a group which are missing
	CMD_REMOVE_ALL_VEHICLES_GROUP,    ///< remove all vehicles from a group
	CMD_SET_GROUP_REPLACE_PROTECTION, ///< set the autoreplace-protection for a group

	CMD_MOVE_ORDER,                   ///< move an order
	CMD_CHANGE_TIMETABLE,             ///< change the timetable for a vehicle
	CMD_SET_VEHICLE_ON_TIME,          ///< set the vehicle on time feature (timetable)
	CMD_AUTOFILL_TIMETABLE,           ///< autofill the timetable
	CMD_SET_TIMETABLE_START,          ///< set the date that a timetable should start

	CMD_END                           ///< Must ALWAYS be on the end of this list!! (period)
};

/**
 * List of flags for a command.
 *
 * This enums defines some flags which can be used for the commands.
 */
enum DoCommandFlag {
	DC_NONE                  = 0x000, ///< no flag is set
	DC_EXEC                  = 0x001, ///< execute the given command
	DC_AUTO                  = 0x002, ///< don't allow building on structures
	DC_QUERY_COST            = 0x004, ///< query cost only,  don't build.
	DC_NO_WATER              = 0x008, ///< don't allow building on water
	DC_NO_RAIL_OVERLAP       = 0x010, ///< don't allow overlap of rails (used in buildrail)
	DC_NO_TEST_TOWN_RATING   = 0x020, ///< town rating does not disallow you from building
	DC_BANKRUPT              = 0x040, ///< company bankrupts, skip money check, skip vehicle on tile check in some cases
	DC_AUTOREPLACE           = 0x080, ///< autoreplace/autorenew is in progress, this shall disable vehicle limits when building, and ignore certain restrictions when undoing things (like vehicle attach callback)
	DC_ALL_TILES             = 0x100, ///< allow this command also on MP_VOID tiles
	DC_NO_MODIFY_TOWN_RATING = 0x200, ///< do not change town rating
	DC_FORCE_CLEAR_TILE      = 0x400, ///< do not only remove the object on the tile, but also clear any water left on it
};
DECLARE_ENUM_AS_BIT_SET(DoCommandFlag)

/**
 * Used to combine a StringID with the command.
 *
 * This macro can be used to add a StringID (the error message to show) on a command-id
 * (CMD_xxx). Use the binary or-operator "|" to combine the command with the result from
 * this macro.
 *
 * @param x The StringID to combine with a command-id
 */
#define CMD_MSG(x) ((x) << 16)

/**
 * Defines some flags.
 *
 * This enumeration defines some flags which are binary-or'ed on a command.
 */
enum FlaggedCommands {
	CMD_NETWORK_COMMAND       = 0x0100, ///< execute the command without sending it on the network
	CMD_NO_TEST_IF_IN_NETWORK = 0x0200, ///< When enabled, the command will bypass the no-DC_EXEC round if in network
	CMD_FLAGS_MASK            = 0xFF00, ///< mask for all command flags
	CMD_ID_MASK               = 0x00FF, ///< mask for the command ID
};

/**
 * Command flags for the command table _command_proc_table.
 *
 * This enumeration defines flags for the _command_proc_table.
 */
enum CommandFlags {
	CMD_SERVER    = 0x01, ///< the command can only be initiated by the server
	CMD_SPECTATOR = 0x02, ///< the command may be initiated by a spectator
	CMD_OFFLINE   = 0x04, ///< the command cannot be executed in a multiplayer game; single-player only
	CMD_AUTO      = 0x08, ///< set the DC_AUTO flag on this command
	CMD_ALL_TILES = 0x10, ///< allow this command also on MP_VOID tiles
	CMD_NO_TEST   = 0x20, ///< the command's output may differ between test and execute due to town rating changes etc.
	CMD_NO_WATER  = 0x40, ///< set the DC_NO_WATER flag on this command
	CMD_CLIENT_ID = 0x80, ///< set p2 with the ClientID of the sending client.
};

/** Types of commands we have. */
enum CommandType {
	CMDT_LANDSCAPE_CONSTRUCTION, ///< Construction and destruction of objects on the map.
	CMDT_VEHICLE_CONSTRUCTION,   ///< Construction, modification (incl. refit) and destruction of vehicles.
	CMDT_MONEY_MANAGEMENT,       ///< Management of money, i.e. loans and shares.
	CMDT_VEHICLE_MANAGEMENT,     ///< Stopping, starting, sending to depot, turning around, replace orders etc.
	CMDT_ROUTE_MANAGEMENT,       ///< Modifications to route management (orders, groups, etc).
	CMDT_OTHER_MANAGEMENT,       ///< Renaming stuff, changing company colours, placing signs, etc.
	CMDT_COMPANY_SETTING,        ///< Changing settings related to a company.
	CMDT_SERVER_SETTING,         ///< Pausing/removing companies/server settings.
	CMDT_CHEAT,                  ///< A cheat of some sorts.

	CMDT_END,                    ///< Magic end marker.
};

/** Different command pause levels. */
enum CommandPauseLevel {
	CMDPL_NO_ACTIONS,      ///< No user actions may be executed.
	CMDPL_NO_CONSTRUCTION, ///< No construction actions may be executed.
	CMDPL_NO_LANDSCAPING,  ///< No landscaping actions may be executed.
	CMDPL_ALL_ACTIONS,     ///< All actions may be executed.
};

/**
 * Defines the callback type for all command handler functions.
 *
 * This type defines the function header for all functions which handles a CMD_* command.
 * A command handler use the parameters to act according to the meaning of the command.
 * The tile parameter defines the tile to perform an action on.
 * The flag parameter is filled with flags from the DC_* enumeration. The parameters
 * p1 and p2 are filled with parameters for the command like "which road type", "which
 * order" or "direction". Each function should mentioned in there doxygen comments
 * the usage of these parameters.
 *
 * @param tile The tile to apply a command on
 * @param flags Flags for the command, from the DC_* enumeration
 * @param p1 Additional data for the command
 * @param p2 Additional data for the command
 * @param text Additional text
 * @return The CommandCost of the command, which can be succeeded or failed.
 */
typedef CommandCost CommandProc(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text);

/**
 * Define a command with the flags which belongs to it.
 *
 * This struct connect a command handler function with the flags created with
 * the #CMD_AUTO, #CMD_OFFLINE and #CMD_SERVER values.
 */
struct Command {
	CommandProc *proc; ///< The procedure to actually executing
	const char *name;  ///< A human readable name for the procedure
	byte flags;        ///< The (command) flags to that apply to this command
	CommandType type;  ///< The type of command.
};

/**
 * Define a callback function for the client, after the command is finished.
 *
 * Functions of this type are called after the command is finished. The parameters
 * are from the #CommandProc callback type. The boolean parameter indicates if the
 * command succeeded or failed.
 *
 * @param result The result of the executed command
 * @param tile The tile of the command action
 * @param p1 Additional data of the command
 * @param p1 Additional data of the command
 * @see CommandProc
 */
typedef void CommandCallback(const CommandCost &result, TileIndex tile, uint32 p1, uint32 p2);

/**
 * Structure for buffering the build command when selecting a station to join.
 */
struct CommandContainer {
	TileIndex tile;                  ///< tile command being executed on.
	uint32 p1;                       ///< parameter p1.
	uint32 p2;                       ///< parameter p2.
	uint32 cmd;                      ///< command being executed.
	CommandCallback *callback;       ///< any callback function executed upon successful completion of the command.
	char text[32 * MAX_CHAR_LENGTH]; ///< possible text sent for name changes etc, in bytes including '\0'.
};

#endif /* COMMAND_TYPE_H */
