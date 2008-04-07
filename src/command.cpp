/* $Id$ */

/** @file command.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "landscape.h"
#include "tile_map.h"
#include "gui.h"
#include "command_func.h"
#include "network/network.h"
#include "variables.h"
#include "genworld.h"
#include "newgrf_storage.h"
#include "strings_func.h"
#include "gfx_func.h"
#include "functions.h"
#include "town.h"
#include "date_func.h"
#include "debug.h"
#include "player_func.h"
#include "player_base.h"
#include "signal_func.h"

#include "table/strings.h"

const char *_cmd_text = NULL;
StringID _error_message;

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
	{CmdBuildRailroadTrack,           CMD_AUTO}, /* CMD_BUILD_RAILROAD_TRACK */
	{CmdRemoveRailroadTrack,          CMD_AUTO}, /* CMD_REMOVE_RAILROAD_TRACK */
	{CmdBuildSingleRail,              CMD_AUTO}, /* CMD_BUILD_SINGLE_RAIL */
	{CmdRemoveSingleRail,             CMD_AUTO}, /* CMD_REMOVE_SINGLE_RAIL */
	{CmdLandscapeClear,                      0}, /* CMD_LANDSCAPE_CLEAR */
	{CmdBuildBridge,                  CMD_AUTO}, /* CMD_BUILD_BRIDGE */
	{CmdBuildRailroadStation,         CMD_AUTO}, /* CMD_BUILD_RAILROAD_STATION */
	{CmdBuildTrainDepot,              CMD_AUTO}, /* CMD_BUILD_TRAIN_DEPOT */
	{CmdBuildSingleSignal,            CMD_AUTO}, /* CMD_BUILD_SIGNALS */
	{CmdRemoveSingleSignal,           CMD_AUTO}, /* CMD_REMOVE_SIGNALS */
	{CmdTerraformLand,                CMD_AUTO}, /* CMD_TERRAFORM_LAND */
	{CmdPurchaseLandArea,             CMD_AUTO}, /* CMD_PURCHASE_LAND_AREA */
	{CmdSellLandArea,                        0}, /* CMD_SELL_LAND_AREA */
	{CmdBuildTunnel,                  CMD_AUTO}, /* CMD_BUILD_TUNNEL */
	{CmdRemoveFromRailroadStation,           0}, /* CMD_REMOVE_FROM_RAILROAD_STATION */
	{CmdConvertRail,                         0}, /* CMD_CONVERT_RAILD */
	{CmdBuildTrainWaypoint,                  0}, /* CMD_BUILD_TRAIN_WAYPOINT */
	{CmdRenameWaypoint,                      0}, /* CMD_RENAME_WAYPOINT */
	{CmdRemoveTrainWaypoint,                 0}, /* CMD_REMOVE_TRAIN_WAYPOINT */

	{CmdBuildRoadStop,                CMD_AUTO}, /* CMD_BUILD_ROAD_STOP */
	{CmdRemoveRoadStop,                      0}, /* CMD_REMOVE_ROAD_STOP */
	{CmdBuildLongRoad,                CMD_AUTO}, /* CMD_BUILD_LONG_ROAD */
	{CmdRemoveLongRoad,               CMD_AUTO}, /* CMD_REMOVE_LONG_ROAD */
	{CmdBuildRoad,                           0}, /* CMD_BUILD_ROAD */
	{CmdRemoveRoad,                          0}, /* CMD_REMOVE_ROAD */
	{CmdBuildRoadDepot,               CMD_AUTO}, /* CMD_BUILD_ROAD_DEPOT */

	{CmdBuildAirport,                 CMD_AUTO}, /* CMD_BUILD_AIRPORT */
	{CmdBuildDock,                    CMD_AUTO}, /* CMD_BUILD_DOCK */
	{CmdBuildShipDepot,               CMD_AUTO}, /* CMD_BUILD_SHIP_DEPOT */
	{CmdBuildBuoy,                    CMD_AUTO}, /* CMD_BUILD_BUOY */
	{CmdPlantTree,                    CMD_AUTO}, /* CMD_PLANT_TREE */
	{CmdBuildRailVehicle,                    0}, /* CMD_BUILD_RAIL_VEHICLE */
	{CmdMoveRailVehicle,                     0}, /* CMD_MOVE_RAIL_VEHICLE */
	{CmdStartStopTrain,                      0}, /* CMD_START_STOP_TRAIN */

	{CmdSellRailWagon,                       0}, /* CMD_SELL_RAIL_WAGON */
	{CmdSendTrainToDepot,                    0}, /* CMD_SEND_TRAIN_TO_DEPOT */
	{CmdForceTrainProceed,                   0}, /* CMD_FORCE_TRAIN_PROCEED */
	{CmdReverseTrainDirection,               0}, /* CMD_REVERSE_TRAIN_DIRECTION */

	{CmdModifyOrder,                         0}, /* CMD_MODIFY_ORDER */
	{CmdSkipToOrder,                         0}, /* CMD_SKIP_TO_ORDER */
	{CmdDeleteOrder,                         0}, /* CMD_DELETE_ORDER */
	{CmdInsertOrder,                         0}, /* CMD_INSERT_ORDER */

	{CmdChangeServiceInt,                    0}, /* CMD_CHANGE_SERVICE_INT */

	{CmdBuildIndustry,                       0}, /* CMD_BUILD_INDUSTRY */
	{CmdBuildCompanyHQ,               CMD_AUTO}, /* CMD_BUILD_COMPANY_HQ */
	{CmdSetPlayerFace,                       0}, /* CMD_SET_PLAYER_FACE */
	{CmdSetPlayerColor,                      0}, /* CMD_SET_PLAYER_COLOR */

	{CmdIncreaseLoan,                        0}, /* CMD_INCREASE_LOAN */
	{CmdDecreaseLoan,                        0}, /* CMD_DECREASE_LOAN */

	{CmdWantEnginePreview,                   0}, /* CMD_WANT_ENGINE_PREVIEW */

	{CmdNameVehicle,                         0}, /* CMD_NAME_VEHICLE */
	{CmdRenameEngine,                        0}, /* CMD_RENAME_ENGINE */

	{CmdChangeCompanyName,                   0}, /* CMD_CHANGE_COMPANY_NAME */
	{CmdChangePresidentName,                 0}, /* CMD_CHANGE_PRESIDENT_NAME */

	{CmdRenameStation,                       0}, /* CMD_RENAME_STATION */

	{CmdSellAircraft,                        0}, /* CMD_SELL_AIRCRAFT */
	{CmdStartStopAircraft,                   0}, /* CMD_START_STOP_AIRCRAFT */

	{CmdBuildAircraft,                       0}, /* CMD_BUILD_AIRCRAFT */
	{CmdSendAircraftToHangar,                0}, /* CMD_SEND_AIRCRAFT_TO_HANGAR */
	{CmdRefitAircraft,                       0}, /* CMD_REFIT_AIRCRAFT */

	{CmdPlaceSign,                           0}, /* CMD_PLACE_SIGN */
	{CmdRenameSign,                          0}, /* CMD_RENAME_SIGN */

	{CmdBuildRoadVeh,                        0}, /* CMD_BUILD_ROAD_VEH */
	{CmdStartStopRoadVeh,                    0}, /* CMD_START_STOP_ROADVEH */
	{CmdSellRoadVeh,                         0}, /* CMD_SELL_ROAD_VEH */
	{CmdSendRoadVehToDepot,                  0}, /* CMD_SEND_ROADVEH_TO_DEPOT */
	{CmdTurnRoadVeh,                         0}, /* CMD_TURN_ROADVEH */
	{CmdRefitRoadVeh,                        0}, /* CMD_REFIT_ROAD_VEH */

	{CmdPause,                      CMD_SERVER}, /* CMD_PAUSE */

	{CmdBuyShareInCompany,                   0}, /* CMD_BUY_SHARE_IN_COMPANY */
	{CmdSellShareInCompany,                  0}, /* CMD_SELL_SHARE_IN_COMPANY */
	{CmdBuyCompany,                          0}, /* CMD_BUY_COMANY */

	{CmdBuildTown,                 CMD_OFFLINE}, /* CMD_BUILD_TOWN */
	{CmdRenameTown,                 CMD_SERVER}, /* CMD_RENAME_TOWN */
	{CmdDoTownAction,                        0}, /* CMD_DO_TOWN_ACTION */

	{CmdSetRoadDriveSide,           CMD_SERVER}, /* CMD_SET_ROAD_DRIVE_SIDE */
	{CmdChangeDifficultyLevel,      CMD_SERVER}, /* CMD_CHANGE_DIFFICULTY_LEVEL */

	{CmdStartStopShip,                       0}, /* CMD_START_STOP_SHIP */
	{CmdSellShip,                            0}, /* CMD_SELL_SHIP */
	{CmdBuildShip,                           0}, /* CMD_BUILD_SHIP */
	{CmdSendShipToDepot,                     0}, /* CMD_SEND_SHIP_TO_DEPOT */
	{CmdRefitShip,                           0}, /* CMD_REFIT_SHIP */

	{CmdOrderRefit,                          0}, /* CMD_ORDER_REFIT */
	{CmdCloneOrder,                          0}, /* CMD_CLONE_ORDER */

	{CmdClearArea,                           0}, /* CMD_CLEAR_AREA */

	{CmdMoneyCheat,                CMD_OFFLINE}, /* CMD_MONEY_CHEAT */
	{CmdBuildCanal,                   CMD_AUTO}, /* CMD_BUILD_CANAL */
	{CmdPlayerCtrl,                          0}, /* CMD_PLAYER_CTRL */

	{CmdLevelLand,                    CMD_AUTO}, /* CMD_LEVEL_LAND */

	{CmdRefitRailVehicle,                    0}, /* CMD_REFIT_RAIL_VEHICLE */
	{CmdRestoreOrderIndex,                   0}, /* CMD_RESTORE_ORDER_INDEX */
	{CmdBuildLock,                    CMD_AUTO}, /* CMD_BUILD_LOCK */

	{CmdBuildSignalTrack,             CMD_AUTO}, /* CMD_BUILD_SIGNAL_TRACK */
	{CmdRemoveSignalTrack,            CMD_AUTO}, /* CMD_REMOVE_SIGNAL_TRACK */

	{CmdGiveMoney,                           0}, /* CMD_GIVE_MONEY */
	{CmdChangePatchSetting,         CMD_SERVER}, /* CMD_CHANGE_PATCH_SETTING */
	{CmdSetAutoReplace,                      0}, /* CMD_SET_AUTOREPLACE */
	{CmdCloneVehicle,                        0}, /* CMD_CLONE_VEHICLE */
	{CmdMassStartStopVehicle,                0}, /* CMD_MASS_START_STOP */
	{CmdDepotSellAllVehicles,                0}, /* CMD_DEPOT_SELL_ALL_VEHICLES */
	{CmdDepotMassAutoReplace,                0}, /* CMD_DEPOT_MASS_AUTOREPLACE */
	{CmdCreateGroup,                         0}, /* CMD_CREATE_GROUP */
	{CmdDeleteGroup,                         0}, /* CMD_DELETE_GROUP */
	{CmdRenameGroup,                         0}, /* CMD_RENAME_GROUP */
	{CmdAddVehicleGroup,                     0}, /* CMD_ADD_VEHICLE_GROUP */
	{CmdAddSharedVehicleGroup,               0}, /* CMD_ADD_SHARE_VEHICLE_GROUP */
	{CmdRemoveAllVehiclesGroup,              0}, /* CMD_REMOVE_ALL_VEHICLES_GROUP */
	{CmdSetGroupReplaceProtection,           0}, /* CMD_SET_GROUP_REPLACE_PROTECTION */
	{CmdMoveOrder,                           0}, /* CMD_MOVE_ORDER */
	{CmdChangeTimetable,                     0}, /* CMD_CHANGE_TIMETABLE */
	{CmdSetVehicleOnTime,                    0}, /* CMD_SET_VEHICLE_ON_TIME */
	{CmdAutofillTimetable,                   0}, /* CMD_AUTOFILL_TIMETABLE */
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

static int _docommand_recursive = 0;

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
CommandCost DoCommand(TileIndex tile, uint32 p1, uint32 p2, uint32 flags, uint32 procc)
{
	CommandCost res;
	CommandProc *proc;

	/* Do not even think about executing out-of-bounds tile-commands */
	if (!IsValidTile(tile)) {
		_cmd_text = NULL;
		return CMD_ERROR;
	}

	proc = _command_proc_table[procc].proc;

	if (_docommand_recursive == 0) _error_message = INVALID_STRING_ID;

	_docommand_recursive++;

	/* only execute the test call if it's toplevel, or we're not execing. */
	if (_docommand_recursive == 1 || !(flags & DC_EXEC) ) {
		SetTownRatingTestMode(true);
		res = proc(tile, flags & ~DC_EXEC, p1, p2);
		SetTownRatingTestMode(false);
		if (CmdFailed(res)) {
			res.SetGlobalErrorMessage();
			goto error;
		}

		if (_docommand_recursive == 1 &&
				!(flags & DC_QUERY_COST) &&
				!(flags & DC_BANKRUPT) &&
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
	 * themselves to the cost object at some point */
	res = proc(tile, flags, p1, p2);
	if (CmdFailed(res)) {
		res.SetGlobalErrorMessage();
error:
		_docommand_recursive--;
		_cmd_text = NULL;
		return CMD_ERROR;
	}

	/* if toplevel, subtract the money. */
	if (--_docommand_recursive == 0 && !(flags & DC_BANKRUPT)) {
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
	if (!IsValidTile(tile)) {
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
	 * CMD_REMOVE_LONG_ROAD: This command has special local authority
	 * restrictions which may cause the test run to fail (the previous
	 * road fragments still stay there and the town won't let you
	 * disconnect the road system), but the exec will succeed and this
	 * fact will trigger an assertion failure. --pasky
	 * CMD_CLONE_VEHICLE: Both building new vehicles and refitting them can be
	 * influenced by newgrf callbacks, which makes it impossible to accurately
	 * estimate the cost of cloning a vehicle.
	 * CMD_DEPOT_MASS_AUTOREPLACE: we can't predict wagon removal so
	 * the test will not include income from any sold wagons.
	 * This means that the costs can sometimes be lower than estimated. */
	notest =
		(cmd & 0xFF) == CMD_CLEAR_AREA ||
		(cmd & 0xFF) == CMD_LEVEL_LAND ||
		(cmd & 0xFF) == CMD_REMOVE_LONG_ROAD ||
		(cmd & 0xFF) == CMD_CLONE_VEHICLE    ||
		(cmd & 0xFF) == CMD_DEPOT_MASS_AUTOREPLACE;

	_docommand_recursive = 1;

	/* cost estimation only? */
	if (!IsGeneratingWorld() &&
			_shift_pressed &&
			IsLocalPlayer() &&
			!(cmd & (CMD_NETWORK_COMMAND | CMD_SHOW_NO_ERROR)) &&
			(cmd & 0xFF) != CMD_PAUSE) {
		/* estimate the cost. */
		SetTownRatingTestMode(true);
		res = proc(tile, flags, p1, p2);
		SetTownRatingTestMode(false);
		if (CmdFailed(res)) {
			res.SetGlobalErrorMessage();
			ShowErrorMessage(_error_message, error_part1, x, y);
		} else {
			ShowEstimatedCostOrIncome(res.GetCost(), x, y);
		}

		_docommand_recursive = 0;
		_cmd_text = NULL;
		ClearStorageChanges(false);
		return false;
	}


	if (!((cmd & CMD_NO_TEST_IF_IN_NETWORK) && _networking)) {
		/* first test if the command can be executed. */
		SetTownRatingTestMode(true);
		res = proc(tile, flags, p1, p2);
		SetTownRatingTestMode(false);
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
		ClearStorageChanges(false);
		return true;
	}
#endif /* ENABLE_NETWORK */
	DebugDumpCommands("ddc:cmd:%d;%d;%d;%d;%d;%d;%d;%s\n", _date, _date_fract, (int)_current_player, tile, p1, p2, cmd, _cmd_text);

	/* update last build coordinate of player. */
	if (tile != 0 && IsValidPlayer(_current_player)) {
		GetPlayer(_current_player)->last_build_coordinate = tile;
	}

	/* Actually try and execute the command. If no cost-type is given
	 * use the construction one */
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

	/* update signals if needed */
	UpdateSignalsInBuffer();

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
	ClearStorageChanges(true);
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
	ClearStorageChanges(false);
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
