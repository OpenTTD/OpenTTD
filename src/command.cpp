/* $Id$ */

/** @file command.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "table/strings.h"
#include "strings.h"
#include "functions.h"
#include "landscape.h"
#include "map.h"
#include "gui.h"
#include "command.h"
#include "player.h"
#include "network/network.h"
#include "variables.h"
#include "genworld.h"

const char *_cmd_text = NULL;

/**
 * Helper macro to define the header of all command handler macros.
 *
 * This macro create the function header for a given command handler function, as
 * all command handler functions got the parameters from the #CommandProc callback
 * type.
 *
 * @param yyyy The desired function name of the new command handler function.
 */
#define DEF_COMMAND(yyyy) CommandCost yyyy(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)

DEF_COMMAND(CmdBuildRailroadTrack);
DEF_COMMAND(CmdRemoveRailroadTrack);
DEF_COMMAND(CmdBuildSingleRail);
DEF_COMMAND(CmdRemoveSingleRail);

DEF_COMMAND(CmdLandscapeClear);

DEF_COMMAND(CmdBuildBridge);

DEF_COMMAND(CmdBuildRailroadStation);
DEF_COMMAND(CmdRemoveFromRailroadStation);
DEF_COMMAND(CmdConvertRail);

DEF_COMMAND(CmdBuildSingleSignal);
DEF_COMMAND(CmdRemoveSingleSignal);

DEF_COMMAND(CmdTerraformLand);

DEF_COMMAND(CmdPurchaseLandArea);
DEF_COMMAND(CmdSellLandArea);

DEF_COMMAND(CmdBuildTunnel);

DEF_COMMAND(CmdBuildTrainDepot);
DEF_COMMAND(CmdBuildTrainWaypoint);
DEF_COMMAND(CmdRenameWaypoint);
DEF_COMMAND(CmdRemoveTrainWaypoint);

DEF_COMMAND(CmdBuildRoadStop);
DEF_COMMAND(CmdRemoveRoadStop);

DEF_COMMAND(CmdBuildLongRoad);
DEF_COMMAND(CmdRemoveLongRoad);
DEF_COMMAND(CmdBuildRoad);
DEF_COMMAND(CmdRemoveRoad);

DEF_COMMAND(CmdBuildRoadDepot);

DEF_COMMAND(CmdBuildAirport);

DEF_COMMAND(CmdBuildDock);

DEF_COMMAND(CmdBuildShipDepot);

DEF_COMMAND(CmdBuildBuoy);

DEF_COMMAND(CmdPlantTree);

DEF_COMMAND(CmdBuildRailVehicle);
DEF_COMMAND(CmdMoveRailVehicle);

DEF_COMMAND(CmdStartStopTrain);

DEF_COMMAND(CmdSellRailWagon);

DEF_COMMAND(CmdSendTrainToDepot);
DEF_COMMAND(CmdForceTrainProceed);
DEF_COMMAND(CmdReverseTrainDirection);

DEF_COMMAND(CmdModifyOrder);
DEF_COMMAND(CmdSkipToOrder);
DEF_COMMAND(CmdDeleteOrder);
DEF_COMMAND(CmdInsertOrder);
DEF_COMMAND(CmdChangeServiceInt);
DEF_COMMAND(CmdRestoreOrderIndex);

DEF_COMMAND(CmdBuildIndustry);

DEF_COMMAND(CmdBuildCompanyHQ);
DEF_COMMAND(CmdSetPlayerFace);
DEF_COMMAND(CmdSetPlayerColor);

DEF_COMMAND(CmdIncreaseLoan);
DEF_COMMAND(CmdDecreaseLoan);

DEF_COMMAND(CmdWantEnginePreview);

DEF_COMMAND(CmdNameVehicle);
DEF_COMMAND(CmdRenameEngine);

DEF_COMMAND(CmdChangeCompanyName);
DEF_COMMAND(CmdChangePresidentName);

DEF_COMMAND(CmdRenameStation);

DEF_COMMAND(CmdSellAircraft);
DEF_COMMAND(CmdStartStopAircraft);
DEF_COMMAND(CmdBuildAircraft);
DEF_COMMAND(CmdSendAircraftToHangar);
DEF_COMMAND(CmdRefitAircraft);

DEF_COMMAND(CmdPlaceSign);
DEF_COMMAND(CmdRenameSign);

DEF_COMMAND(CmdBuildRoadVeh);
DEF_COMMAND(CmdStartStopRoadVeh);
DEF_COMMAND(CmdSellRoadVeh);
DEF_COMMAND(CmdSendRoadVehToDepot);
DEF_COMMAND(CmdTurnRoadVeh);
DEF_COMMAND(CmdRefitRoadVeh);

DEF_COMMAND(CmdPause);

DEF_COMMAND(CmdBuyShareInCompany);
DEF_COMMAND(CmdSellShareInCompany);
DEF_COMMAND(CmdBuyCompany);

DEF_COMMAND(CmdBuildTown);

DEF_COMMAND(CmdRenameTown);
DEF_COMMAND(CmdDoTownAction);

DEF_COMMAND(CmdSetRoadDriveSide);

DEF_COMMAND(CmdChangeDifficultyLevel);
DEF_COMMAND(CmdChangePatchSetting);

DEF_COMMAND(CmdStartStopShip);
DEF_COMMAND(CmdSellShip);
DEF_COMMAND(CmdBuildShip);
DEF_COMMAND(CmdSendShipToDepot);
DEF_COMMAND(CmdRefitShip);

DEF_COMMAND(CmdOrderRefit);
DEF_COMMAND(CmdCloneOrder);

DEF_COMMAND(CmdClearArea);

DEF_COMMAND(CmdGiveMoney);
DEF_COMMAND(CmdMoneyCheat);
DEF_COMMAND(CmdBuildCanal);
DEF_COMMAND(CmdBuildLock);

DEF_COMMAND(CmdPlayerCtrl);

DEF_COMMAND(CmdLevelLand);

DEF_COMMAND(CmdRefitRailVehicle);

DEF_COMMAND(CmdBuildSignalTrack);
DEF_COMMAND(CmdRemoveSignalTrack);

DEF_COMMAND(CmdSetAutoReplace);

DEF_COMMAND(CmdCloneVehicle);
DEF_COMMAND(CmdMassStartStopVehicle);
DEF_COMMAND(CmdDepotSellAllVehicles);
DEF_COMMAND(CmdDepotMassAutoReplace);

DEF_COMMAND(CmdCreateGroup);
DEF_COMMAND(CmdRenameGroup);
DEF_COMMAND(CmdDeleteGroup);
DEF_COMMAND(CmdAddVehicleGroup);
DEF_COMMAND(CmdAddSharedVehicleGroup);
DEF_COMMAND(CmdRemoveAllVehiclesGroup);
DEF_COMMAND(CmdSetGroupReplaceProtection);

DEF_COMMAND(CmdMoveOrder);
DEF_COMMAND(CmdChangeTimetable);
DEF_COMMAND(CmdSetVehicleOnTime);
DEF_COMMAND(CmdAutofillTimetable);
#undef DEF_COMMAND

/**
 * The master command table
 *
 * This table contains all possible CommandProc functions with
 * the flags which belongs to it. The indizes are the same
 * as the value from the CMD_* enums.
 */
static const Command _command_proc_table[] = {
	{CmdBuildRailroadTrack,           CMD_AUTO}, /*   0, CMD_BUILD_RAILROAD_TRACK */
	{CmdRemoveRailroadTrack,          CMD_AUTO}, /*   1, CMD_REMOVE_RAILROAD_TRACK */
	{CmdBuildSingleRail,              CMD_AUTO}, /*   2, CMD_BUILD_SINGLE_RAIL */
	{CmdRemoveSingleRail,             CMD_AUTO}, /*   3, CMD_REMOVE_SINGLE_RAIL */
	{CmdLandscapeClear,                      0}, /*   4, CMD_LANDSCAPE_CLEAR */
	{CmdBuildBridge,                  CMD_AUTO}, /*   5, CMD_BUILD_BRIDGE */
	{CmdBuildRailroadStation,         CMD_AUTO}, /*   6, CMD_BUILD_RAILROAD_STATION */
	{CmdBuildTrainDepot,              CMD_AUTO}, /*   7, CMD_BUILD_TRAIN_DEPOT */
	{CmdBuildSingleSignal,            CMD_AUTO}, /*   8, CMD_BUILD_SIGNALS */
	{CmdRemoveSingleSignal,           CMD_AUTO}, /*   9, CMD_REMOVE_SIGNALS */
	{CmdTerraformLand,                CMD_AUTO}, /*  10, CMD_TERRAFORM_LAND */
	{CmdPurchaseLandArea,             CMD_AUTO}, /*  11, CMD_PURCHASE_LAND_AREA */
	{CmdSellLandArea,                        0}, /*  12, CMD_SELL_LAND_AREA */
	{CmdBuildTunnel,                  CMD_AUTO}, /*  13, CMD_BUILD_TUNNEL */
	{CmdRemoveFromRailroadStation,           0}, /*  14, CMD_REMOVE_FROM_RAILROAD_STATION */
	{CmdConvertRail,                         0}, /*  15, CMD_CONVERT_RAILD */
	{CmdBuildTrainWaypoint,                  0}, /*  16, CMD_BUILD_TRAIN_WAYPOINT */
	{CmdRenameWaypoint,                      0}, /*  17, CMD_RENAME_WAYPOINT */
	{CmdRemoveTrainWaypoint,                 0}, /*  18, CMD_REMOVE_TRAIN_WAYPOINT */
	{NULL,                                   0}, /*  19, unused */
	{NULL,                                   0}, /*  20, unused */
	{CmdBuildRoadStop,                CMD_AUTO}, /*  21, CMD_BUILD_ROAD_STOP */
	{CmdRemoveRoadStop,                      0}, /*  22, CMD_REMOVE_ROAD_STOP */
	{CmdBuildLongRoad,                CMD_AUTO}, /*  23, CMD_BUILD_LONG_ROAD */
	{CmdRemoveLongRoad,               CMD_AUTO}, /*  24, CMD_REMOVE_LONG_ROAD */
	{CmdBuildRoad,                           0}, /*  25, CMD_BUILD_ROAD */
	{CmdRemoveRoad,                          0}, /*  26, CMD_REMOVE_ROAD */
	{CmdBuildRoadDepot,               CMD_AUTO}, /*  27, CMD_BUILD_ROAD_DEPOT */
	{NULL,                                   0}, /*  28, unused */
	{CmdBuildAirport,                 CMD_AUTO}, /*  29, CMD_BUILD_AIRPORT */
	{CmdBuildDock,                    CMD_AUTO}, /*  30, CMD_BUILD_DOCK */
	{CmdBuildShipDepot,               CMD_AUTO}, /*  31, CMD_BUILD_SHIP_DEPOT */
	{CmdBuildBuoy,                    CMD_AUTO}, /*  32, CMD_BUILD_BUOY */
	{CmdPlantTree,                    CMD_AUTO}, /*  33, CMD_PLANT_TREE */
	{CmdBuildRailVehicle,                    0}, /*  34, CMD_BUILD_RAIL_VEHICLE */
	{CmdMoveRailVehicle,                     0}, /*  35, CMD_MOVE_RAIL_VEHICLE */
	{CmdStartStopTrain,                      0}, /*  36, CMD_START_STOP_TRAIN */
	{NULL,                                   0}, /*  37, unused */
	{CmdSellRailWagon,                       0}, /*  38, CMD_SELL_RAIL_WAGON */
	{CmdSendTrainToDepot,                    0}, /*  39, CMD_SEND_TRAIN_TO_DEPOT */
	{CmdForceTrainProceed,                   0}, /*  40, CMD_FORCE_TRAIN_PROCEED */
	{CmdReverseTrainDirection,               0}, /*  41, CMD_REVERSE_TRAIN_DIRECTION */

	{CmdModifyOrder,                         0}, /*  42, CMD_MODIFY_ORDER */
	{CmdSkipToOrder,                         0}, /*  43, CMD_SKIP_TO_ORDER */
	{CmdDeleteOrder,                         0}, /*  44, CMD_DELETE_ORDER */
	{CmdInsertOrder,                         0}, /*  45, CMD_INSERT_ORDER */

	{CmdChangeServiceInt,                    0}, /*  46, CMD_CHANGE_SERVICE_INT */

	{CmdBuildIndustry,                       0}, /*  47, CMD_BUILD_INDUSTRY */
	{CmdBuildCompanyHQ,               CMD_AUTO}, /*  48, CMD_BUILD_COMPANY_HQ */
	{CmdSetPlayerFace,                       0}, /*  49, CMD_SET_PLAYER_FACE */
	{CmdSetPlayerColor,                      0}, /*  50, CMD_SET_PLAYER_COLOR */

	{CmdIncreaseLoan,                        0}, /*  51, CMD_INCREASE_LOAN */
	{CmdDecreaseLoan,                        0}, /*  52, CMD_DECREASE_LOAN */

	{CmdWantEnginePreview,                   0}, /*  53, CMD_WANT_ENGINE_PREVIEW */

	{CmdNameVehicle,                         0}, /*  54, CMD_NAME_VEHICLE */
	{CmdRenameEngine,                        0}, /*  55, CMD_RENAME_ENGINE */

	{CmdChangeCompanyName,                   0}, /*  56, CMD_CHANGE_COMPANY_NAME */
	{CmdChangePresidentName,                 0}, /*  57, CMD_CHANGE_PRESIDENT_NAME */

	{CmdRenameStation,                       0}, /*  58, CMD_RENAME_STATION */

	{CmdSellAircraft,                        0}, /*  59, CMD_SELL_AIRCRAFT */
	{CmdStartStopAircraft,                   0}, /*  60, CMD_START_STOP_AIRCRAFT */

	{CmdBuildAircraft,                       0}, /*  61, CMD_BUILD_AIRCRAFT */
	{CmdSendAircraftToHangar,                0}, /*  62, CMD_SEND_AIRCRAFT_TO_HANGAR */
	{NULL,                                   0}, /*  63, unused */
	{CmdRefitAircraft,                       0}, /*  64, CMD_REFIT_AIRCRAFT */

	{CmdPlaceSign,                           0}, /*  65, CMD_PLACE_SIGN */
	{CmdRenameSign,                          0}, /*  66, CMD_RENAME_SIGN */

	{CmdBuildRoadVeh,                        0}, /*  67, CMD_BUILD_ROAD_VEH */
	{CmdStartStopRoadVeh,                    0}, /*  68, CMD_START_STOP_ROADVEH */
	{CmdSellRoadVeh,                         0}, /*  69, CMD_SELL_ROAD_VEH */
	{CmdSendRoadVehToDepot,                  0}, /*  70, CMD_SEND_ROADVEH_TO_DEPOT */
	{CmdTurnRoadVeh,                         0}, /*  71, CMD_TURN_ROADVEH */
	{CmdRefitRoadVeh,                        0}, /*  72, CMD_REFIT_ROAD_VEH */

	{CmdPause,                      CMD_SERVER}, /*  73, CMD_PAUSE */

	{CmdBuyShareInCompany,                   0}, /*  74, CMD_BUY_SHARE_IN_COMPANY */
	{CmdSellShareInCompany,                  0}, /*  75, CMD_SELL_SHARE_IN_COMPANY */
	{CmdBuyCompany,                          0}, /*  76, CMD_BUY_COMANY */

	{CmdBuildTown,                 CMD_OFFLINE}, /*  77, CMD_BUILD_TOWN */
	{NULL,                                   0}, /*  78, unused */
	{NULL,                                   0}, /*  79, unused */
	{CmdRenameTown,                 CMD_SERVER}, /*  80, CMD_RENAME_TOWN */
	{CmdDoTownAction,                        0}, /*  81, CMD_DO_TOWN_ACTION */

	{CmdSetRoadDriveSide,           CMD_SERVER}, /*  82, CMD_SET_ROAD_DRIVE_SIDE */
	{NULL,                                   0}, /*  83, unused */
	{NULL,                                   0}, /*  84, unused */
	{CmdChangeDifficultyLevel,      CMD_SERVER}, /*  85, CMD_CHANGE_DIFFICULTY_LEVEL */

	{CmdStartStopShip,                       0}, /*  86, CMD_START_STOP_SHIP */
	{CmdSellShip,                            0}, /*  87, CMD_SELL_SHIP */
	{CmdBuildShip,                           0}, /*  88, CMD_BUILD_SHIP */
	{CmdSendShipToDepot,                     0}, /*  89, CMD_SEND_SHIP_TO_DEPOT */
	{NULL,                                   0}, /*  90, unused */
	{CmdRefitShip,                           0}, /*  91, CMD_REFIT_SHIP */

	{NULL,                                   0}, /*  92, unused */
	{NULL,                                   0}, /*  93, unused */
	{NULL,                                   0}, /*  94, unused */
	{NULL,                                   0}, /*  95, unused */
	{NULL,                                   0}, /*  96, unused */
	{NULL,                                   0}, /*  97, unused */

	{CmdOrderRefit,                          0}, /*  98, CMD_ORDER_REFIT */
	{CmdCloneOrder,                          0}, /*  99, CMD_CLONE_ORDER */

	{CmdClearArea,                           0}, /* 100, CMD_CLEAR_AREA */
	{NULL,                                   0}, /* 101, unused */

	{CmdMoneyCheat,                CMD_OFFLINE}, /* 102, CMD_MONEY_CHEAT */
	{CmdBuildCanal,                   CMD_AUTO}, /* 103, CMD_BUILD_CANAL */
	{CmdPlayerCtrl,                          0}, /* 104, CMD_PLAYER_CTRL */

	{CmdLevelLand,                    CMD_AUTO}, /* 105, CMD_LEVEL_LAND */

	{CmdRefitRailVehicle,                    0}, /* 106, CMD_REFIT_RAIL_VEHICLE */
	{CmdRestoreOrderIndex,                   0}, /* 107, CMD_RESTORE_ORDER_INDEX */
	{CmdBuildLock,                    CMD_AUTO}, /* 108, CMD_BUILD_LOCK */
	{NULL,                                   0}, /* 109, unused */
	{CmdBuildSignalTrack,             CMD_AUTO}, /* 110, CMD_BUILD_SIGNAL_TRACK */
	{CmdRemoveSignalTrack,            CMD_AUTO}, /* 111, CMD_REMOVE_SIGNAL_TRACK */
	{NULL,                                   0}, /* 112, unused */
	{CmdGiveMoney,                           0}, /* 113, CMD_GIVE_MONEY */
	{CmdChangePatchSetting,         CMD_SERVER}, /* 114, CMD_CHANGE_PATCH_SETTING */
	{CmdSetAutoReplace,                      0}, /* 115, CMD_SET_AUTOREPLACE */
	{CmdCloneVehicle,                        0}, /* 116, CMD_CLONE_VEHICLE */
	{CmdMassStartStopVehicle,                0}, /* 117, CMD_MASS_START_STOP */
	{CmdDepotSellAllVehicles,                0}, /* 118, CMD_DEPOT_SELL_ALL_VEHICLES */
	{CmdDepotMassAutoReplace,                0}, /* 119, CMD_DEPOT_MASS_AUTOREPLACE */
	{CmdCreateGroup,                         0}, /* 120, CMD_CREATE_GROUP */
	{CmdDeleteGroup,                         0}, /* 121, CMD_DELETE_GROUP */
	{CmdRenameGroup,                         0}, /* 122, CMD_RENAME_GROUP */
	{CmdAddVehicleGroup,                     0}, /* 123, CMD_ADD_VEHICLE_GROUP */
	{CmdAddSharedVehicleGroup,               0}, /* 124, CMD_ADD_SHARE_VEHICLE_GROUP */
	{CmdRemoveAllVehiclesGroup,              0}, /* 125, CMD_REMOVE_ALL_VEHICLES_GROUP */
	{CmdSetGroupReplaceProtection,           0}, /* 126, CMD_SET_GROUP_REPLACE_PROTECTION */
	{CmdMoveOrder,                           0}, /* 127, CMD_MOVE_ORDER */
	{CmdChangeTimetable,                     0}, /* 128, CMD_CHANGE_TIMETABLE */
	{CmdSetVehicleOnTime,                    0}, /* 129, CMD_SET_VEHICLE_ON_TIME */
	{CmdAutofillTimetable,                   0}, /* 130, CMD_AUTOFILL_TIMETABLE */
};

/*!
 * This function range-checks a cmd, and checks if the cmd is not NULL
 *
 * @param cmd The integervalue of a command
 * @return true if the command is valid (and got a CommandProc function)
 */
bool IsValidCommand(uint cmd)
{
	cmd &= 0xFF;

	return
		cmd < lengthof(_command_proc_table) &&
		_command_proc_table[cmd].proc != NULL;
}

/*!
 * This function mask the parameter with 0xFF and returns
 * the flags which belongs to the given command.
 *
 * @param cmd The integer value of the command
 * @return The flags for this command
 * @bug integervalues which are less equals 0xFF and greater than the
 *      size of _command_proc_table can result in an index out of bounce
 *      error (which doesn't happend anyway). Check function #IsValidCommand(). (Progman)
 */
byte GetCommandFlags(uint cmd)
{
	return _command_proc_table[cmd & 0xFF].flags;
}

static int _docommand_recursive;

/*!
 * This function executes a given command with the parameters from the #CommandProc parameter list.
 * Depending on the flags parameter it execute or test a command.
 *
 * @param tile The tile to apply the command on (for the #CommandProc)
 * @param p1 Additional data for the command (for the #CommandProc)
 * @param p2 Additional data for the command (for the #CommandProc)
 * @param flags Flags for the command and how to execute the command
 * @param procc The command-id to execute (a value of the CMD_* enums)
 * @see CommandProc
 */
CommandCost DoCommand(TileIndex tile, uint32 p1, uint32 p2, uint32 flags, uint procc)
{
	CommandCost res;
	CommandProc *proc;

	/* Do not even think about executing out-of-bounds tile-commands */
	if (tile >= MapSize() || IsTileType(tile, MP_VOID)) {
		_cmd_text = NULL;
		return CMD_ERROR;
	}

	proc = _command_proc_table[procc].proc;

	if (_docommand_recursive == 0) _error_message = INVALID_STRING_ID;

	_docommand_recursive++;

	/* only execute the test call if it's toplevel, or we're not execing. */
	if (_docommand_recursive == 1 || !(flags & DC_EXEC) || (flags & DC_FORCETEST) ) {
		res = proc(tile, flags & ~DC_EXEC, p1, p2);
		if (CmdFailed(res)) {
			res.SetGlobalErrorMessage();
			goto error;
		}

		if (_docommand_recursive == 1 &&
				!(flags & DC_QUERY_COST) &&
				res.GetCost() != 0 &&
				!CheckPlayerHasMoney(res)) {
			goto error;
		}

		if (!(flags & DC_EXEC)) {
			_docommand_recursive--;
			_cmd_text = NULL;
			return res;
		}
	}

	/* Execute the command here. All cost-relevant functions set the expenses type
	 * themselves with "SET_EXPENSES_TYPE(...);" at the beginning of the function */
	res = proc(tile, flags, p1, p2);
	if (CmdFailed(res)) {
		res.SetGlobalErrorMessage();
error:
		_docommand_recursive--;
		_cmd_text = NULL;
		return CMD_ERROR;
	}

	/* if toplevel, subtract the money. */
	if (--_docommand_recursive == 0) {
		SubtractMoneyFromPlayer(res);
		/* XXX - Old AI hack which doesn't use DoCommandDP; update last build coord of player */
		if (tile != 0 && IsValidPlayer(_current_player)) {
			GetPlayer(_current_player)->last_build_coordinate = tile;
		}
	}

	_cmd_text = NULL;
	return res;
}

/*!
 * This functions returns the money which can be used to execute a command.
 * This is either the money of the current player or INT64_MAX if there
 * is no such a player "at the moment" like the server itself.
 *
 * @return The available money of a player or INT64_MAX
 */
Money GetAvailableMoneyForCommand()
{
	PlayerID pid = _current_player;
	if (!IsValidPlayer(pid)) return INT64_MAX;
	return GetPlayer(pid)->player_money;
}

/*!
 * Toplevel network safe docommand function for the current player. Must not be called recursively.
 * The callback is called when the command succeeded or failed. The parameters
 * tile, p1 and p2 are from the #CommandProc function. The paramater cmd is the command to execute.
 * The parameter my_cmd is used to indicate if the command is from a player or the server.
 *
 * @param tile The tile to perform a command on (see #CommandProc)
 * @param p1 Additional data for the command (see #CommandProc)
 * @param p2 Additional data for the command (see #CommandProc)
 * @param callback A callback function to call after the command is finished
 * @param cmd The command to execute (a CMD_* value)
 * @param my_cmd indicator if the command is from a player or server (to display error messages for a user)
 * @return true if the command succeeded, else false
 */
bool DoCommandP(TileIndex tile, uint32 p1, uint32 p2, CommandCallback *callback, uint32 cmd, bool my_cmd)
{
	CommandCost res, res2;
	CommandProc *proc;
	uint32 flags;
	bool notest;
	StringID error_part1;

	int x = TileX(tile) * TILE_SIZE;
	int y = TileY(tile) * TILE_SIZE;

	/* Do not even think about executing out-of-bounds tile-commands */
	if (tile >= MapSize() || IsTileType(tile, MP_VOID)) {
		_cmd_text = NULL;
		return false;
	}

	assert(_docommand_recursive == 0);

	_error_message = INVALID_STRING_ID;
	error_part1 = GB(cmd, 16, 16);
	_additional_cash_required = 0;

	/** Spectator has no rights except for the (dedicated) server which
	 * is/can be a spectator but as the server it can do anything */
	if (_current_player == PLAYER_SPECTATOR && !_network_server) {
		if (my_cmd) ShowErrorMessage(_error_message, error_part1, x, y);
		_cmd_text = NULL;
		return false;
	}

	flags = 0;
	if (cmd & CMD_NO_WATER) flags |= DC_NO_WATER;

	/* get pointer to command handler */
	assert((cmd & 0xFF) < lengthof(_command_proc_table));
	proc = _command_proc_table[cmd & 0xFF].proc;
	if (proc == NULL) {
		_cmd_text = NULL;
		return false;
	}

	if (GetCommandFlags(cmd) & CMD_AUTO) flags |= DC_AUTO;

	/* Some commands have a different output in dryrun than the realrun
	 *  e.g.: if you demolish a whole town, the dryrun would say okay.
	 *  but by really destroying, your rating drops and at a certain point
	 *  it will fail. so res and res2 are different
	 * CMD_REMOVE_ROAD: This command has special local authority
	 * restrictions which may cause the test run to fail (the previous
	 * road fragments still stay there and the town won't let you
	 * disconnect the road system), but the exec will succeed and this
	 * fact will trigger an assertion failure. --pasky
	 * CMD_CLONE_VEHICLE: Both building new vehicles and refitting them can be
	 * influenced by newgrf callbacks, which makes it impossible to accurately
	 * estimate the cost of cloning a vehicle. */
	notest =
		(cmd & 0xFF) == CMD_CLEAR_AREA ||
		(cmd & 0xFF) == CMD_CONVERT_RAIL ||
		(cmd & 0xFF) == CMD_LEVEL_LAND ||
		(cmd & 0xFF) == CMD_REMOVE_ROAD ||
		(cmd & 0xFF) == CMD_REMOVE_LONG_ROAD ||
		(cmd & 0xFF) == CMD_CLONE_VEHICLE;

	_docommand_recursive = 1;

	/* cost estimation only? */
	if (!IsGeneratingWorld() &&
			_shift_pressed &&
			IsLocalPlayer() &&
			!(cmd & (CMD_NETWORK_COMMAND | CMD_SHOW_NO_ERROR)) &&
			(cmd & 0xFF) != CMD_PAUSE) {
		/* estimate the cost. */
		res = proc(tile, flags, p1, p2);
		if (CmdFailed(res)) {
			res.SetGlobalErrorMessage();
			ShowErrorMessage(_error_message, error_part1, x, y);
		} else {
			ShowEstimatedCostOrIncome(res.GetCost(), x, y);
		}

		_docommand_recursive = 0;
		_cmd_text = NULL;
		return false;
	}


	if (!((cmd & CMD_NO_TEST_IF_IN_NETWORK) && _networking)) {
		/* first test if the command can be executed. */
		res = proc(tile, flags, p1, p2);
		if (CmdFailed(res)) {
			res.SetGlobalErrorMessage();
			goto show_error;
		}
		/* no money? Only check if notest is off */
		if (!notest && res.GetCost() != 0 && !CheckPlayerHasMoney(res)) goto show_error;
	}

#ifdef ENABLE_NETWORK
	/** If we are in network, and the command is not from the network
	 * send it to the command-queue and abort execution
	 * If we are a dedicated server temporarily switch local player, otherwise
	 * the other parties won't be able to execute our command and will desync.
	 * We also need to do this if the server's company has gone bankrupt
	 * @todo Rewrite (dedicated) server to something more than a dirty hack!
	 */
	if (_networking && !(cmd & CMD_NETWORK_COMMAND)) {
		PlayerID pbck = _local_player;
		if (_network_dedicated || (_network_server && pbck == PLAYER_SPECTATOR)) _local_player = PLAYER_FIRST;
		NetworkSend_Command(tile, p1, p2, cmd, callback);
		if (_network_dedicated || (_network_server && pbck == PLAYER_SPECTATOR)) _local_player = pbck;
		_docommand_recursive = 0;
		_cmd_text = NULL;
		return true;
	}
#endif /* ENABLE_NETWORK */

	/* update last build coordinate of player. */
	if (tile != 0 && IsValidPlayer(_current_player)) {
		GetPlayer(_current_player)->last_build_coordinate = tile;
	}

	/* Actually try and execute the command. If no cost-type is given
	 * use the construction one */
	_yearly_expenses_type = EXPENSES_CONSTRUCTION;
	res2 = proc(tile, flags | DC_EXEC, p1, p2);

	/* If notest is on, it means the result of the test can be different than
	 *  the real command.. so ignore the test */
	if (!notest && !((cmd & CMD_NO_TEST_IF_IN_NETWORK) && _networking)) {
		assert(res.GetCost() == res2.GetCost() && CmdFailed(res) == CmdFailed(res2)); // sanity check
	} else {
		if (CmdFailed(res2)) {
			res.SetGlobalErrorMessage();
			goto show_error;
		}
	}

	SubtractMoneyFromPlayer(res2);

	if (IsLocalPlayer() && _game_mode != GM_EDITOR) {
		if (res2.GetCost() != 0 && tile != 0) ShowCostOrIncomeAnimation(x, y, GetSlopeZ(x, y), res2.GetCost());
		if (_additional_cash_required != 0) {
			SetDParam(0, _additional_cash_required);
			if (my_cmd) ShowErrorMessage(STR_0003_NOT_ENOUGH_CASH_REQUIRES, error_part1, x, y);
			if (res2.GetCost() == 0) goto callb_err;
		}
	}

	_docommand_recursive = 0;

	if (callback) callback(true, tile, p1, p2);
	_cmd_text = NULL;
	return true;

show_error:
	/* show error message if the command fails? */
	if (IsLocalPlayer() && error_part1 != 0 && my_cmd) {
		ShowErrorMessage(_error_message, error_part1, x, y);
	}

callb_err:
	_docommand_recursive = 0;

	if (callback) callback(false, tile, p1, p2);
	_cmd_text = NULL;
	return false;
}


CommandCost CommandCost::AddCost(CommandCost ret)
{
	this->AddCost(ret.cost);
	if (this->success && !ret.success) {
		this->message = ret.message;
		this->success = false;
	}
	return *this;
}

CommandCost CommandCost::AddCost(Money cost)
{
	/* Overflow protection */
	if (cost < 0 && (this->cost + cost) > this->cost) {
		this->cost = INT64_MIN;
	} else if (cost > 0 && (this->cost + cost) < this->cost) {
		this->cost = INT64_MAX;
	} else  {
		this->cost += cost;
	}
	return *this;
}

CommandCost CommandCost::MultiplyCost(int factor)
{
	/* Overflow protection */
	if (factor != 0 && (INT64_MAX / myabs(factor)) < myabs(this->cost)) {
		this->cost = (this->cost < 0 == factor < 0) ? INT64_MAX : INT64_MIN;
	} else {
		this->cost *= factor;
	}
	return *this;
}

Money CommandCost::GetCost() const
{
	return this->cost;
}

void CommandCost::SetGlobalErrorMessage() const
{
	extern StringID _error_message;
	if (this->message != INVALID_STRING_ID) _error_message = this->message;
}

bool CommandCost::Succeeded() const
{
	return this->success;
}

bool CommandCost::Failed() const
{
	return !this->success;
}
