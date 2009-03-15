/* $Id$ */

/** @file command.cpp Handling of commands. */

#include "stdafx.h"
#include "openttd.h"
#include "landscape.h"
#include "tile_map.h"
#include "gui.h"
#include "command_func.h"
#include "network/network.h"
#include "genworld.h"
#include "newgrf_storage.h"
#include "strings_func.h"
#include "gfx_func.h"
#include "functions.h"
#include "town.h"
#include "date_func.h"
#include "debug.h"
#include "company_func.h"
#include "company_base.h"
#include "signal_func.h"

#include "table/strings.h"

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
#define DEF_COMMAND(yyyy) CommandCost yyyy(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)

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
DEF_COMMAND(CmdSetCompanyManagerFace);
DEF_COMMAND(CmdSetCompanyColour);

DEF_COMMAND(CmdIncreaseLoan);
DEF_COMMAND(CmdDecreaseLoan);

DEF_COMMAND(CmdWantEnginePreview);

DEF_COMMAND(CmdRenameVehicle);
DEF_COMMAND(CmdRenameEngine);

DEF_COMMAND(CmdRenameCompany);
DEF_COMMAND(CmdRenamePresident);

DEF_COMMAND(CmdRenameStation);

DEF_COMMAND(CmdSellAircraft);
DEF_COMMAND(CmdBuildAircraft);
DEF_COMMAND(CmdSendAircraftToHangar);
DEF_COMMAND(CmdRefitAircraft);

DEF_COMMAND(CmdPlaceSign);
DEF_COMMAND(CmdRenameSign);

DEF_COMMAND(CmdBuildRoadVeh);
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

DEF_COMMAND(CmdChangeSetting);

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

DEF_COMMAND(CmdCompanyCtrl);

DEF_COMMAND(CmdLevelLand);

DEF_COMMAND(CmdRefitRailVehicle);

DEF_COMMAND(CmdBuildSignalTrack);
DEF_COMMAND(CmdRemoveSignalTrack);

DEF_COMMAND(CmdSetAutoReplace);

DEF_COMMAND(CmdCloneVehicle);
DEF_COMMAND(CmdStartStopVehicle);
DEF_COMMAND(CmdMassStartStopVehicle);
DEF_COMMAND(CmdAutoreplaceVehicle);
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
	{CmdBuildRailroadTrack,   CMD_NO_WATER | CMD_AUTO}, // CMD_BUILD_RAILROAD_TRACK
	{CmdRemoveRailroadTrack,                 CMD_AUTO}, // CMD_REMOVE_RAILROAD_TRACK
	{CmdBuildSingleRail,      CMD_NO_WATER | CMD_AUTO}, // CMD_BUILD_SINGLE_RAIL
	{CmdRemoveSingleRail,                    CMD_AUTO}, // CMD_REMOVE_SINGLE_RAIL
	{CmdLandscapeClear,                             0}, // CMD_LANDSCAPE_CLEAR
	{CmdBuildBridge,                         CMD_AUTO}, // CMD_BUILD_BRIDGE
	{CmdBuildRailroadStation, CMD_NO_WATER | CMD_AUTO}, // CMD_BUILD_RAILROAD_STATION
	{CmdBuildTrainDepot,      CMD_NO_WATER | CMD_AUTO}, // CMD_BUILD_TRAIN_DEPOT
	{CmdBuildSingleSignal,                   CMD_AUTO}, // CMD_BUILD_SIGNALS
	{CmdRemoveSingleSignal,                  CMD_AUTO}, // CMD_REMOVE_SIGNALS
	{CmdTerraformLand,       CMD_ALL_TILES | CMD_AUTO}, // CMD_TERRAFORM_LAND
	{CmdPurchaseLandArea,     CMD_NO_WATER | CMD_AUTO}, // CMD_PURCHASE_LAND_AREA
	{CmdSellLandArea,                               0}, // CMD_SELL_LAND_AREA
	{CmdBuildTunnel,                         CMD_AUTO}, // CMD_BUILD_TUNNEL
	{CmdRemoveFromRailroadStation,                  0}, // CMD_REMOVE_FROM_RAILROAD_STATION
	{CmdConvertRail,                                0}, // CMD_CONVERT_RAILD
	{CmdBuildTrainWaypoint,                         0}, // CMD_BUILD_TRAIN_WAYPOINT
	{CmdRenameWaypoint,                             0}, // CMD_RENAME_WAYPOINT
	{CmdRemoveTrainWaypoint,                        0}, // CMD_REMOVE_TRAIN_WAYPOINT

	{CmdBuildRoadStop,        CMD_NO_WATER | CMD_AUTO}, // CMD_BUILD_ROAD_STOP
	{CmdRemoveRoadStop,                             0}, // CMD_REMOVE_ROAD_STOP
	{CmdBuildLongRoad,        CMD_NO_WATER | CMD_AUTO}, // CMD_BUILD_LONG_ROAD
	{CmdRemoveLongRoad,        CMD_NO_TEST | CMD_AUTO}, // CMD_REMOVE_LONG_ROAD; towns may disallow removing road bits (as they are connected) in test, but in exec they're removed and thus removing is allowed.
	{CmdBuildRoad,                                  0}, // CMD_BUILD_ROAD
	{CmdRemoveRoad,                                 0}, // CMD_REMOVE_ROAD
	{CmdBuildRoadDepot,       CMD_NO_WATER | CMD_AUTO}, // CMD_BUILD_ROAD_DEPOT

	{CmdBuildAirport,         CMD_NO_WATER | CMD_AUTO}, // CMD_BUILD_AIRPORT
	{CmdBuildDock,                           CMD_AUTO}, // CMD_BUILD_DOCK
	{CmdBuildShipDepot,                      CMD_AUTO}, // CMD_BUILD_SHIP_DEPOT
	{CmdBuildBuoy,                           CMD_AUTO}, // CMD_BUILD_BUOY
	{CmdPlantTree,                           CMD_AUTO}, // CMD_PLANT_TREE
	{CmdBuildRailVehicle,                           0}, // CMD_BUILD_RAIL_VEHICLE
	{CmdMoveRailVehicle,                            0}, // CMD_MOVE_RAIL_VEHICLE

	{CmdSellRailWagon,                              0}, // CMD_SELL_RAIL_WAGON
	{CmdSendTrainToDepot,                           0}, // CMD_SEND_TRAIN_TO_DEPOT
	{CmdForceTrainProceed,                          0}, // CMD_FORCE_TRAIN_PROCEED
	{CmdReverseTrainDirection,                      0}, // CMD_REVERSE_TRAIN_DIRECTION

	{CmdModifyOrder,                                0}, // CMD_MODIFY_ORDER
	{CmdSkipToOrder,                                0}, // CMD_SKIP_TO_ORDER
	{CmdDeleteOrder,                                0}, // CMD_DELETE_ORDER
	{CmdInsertOrder,                                0}, // CMD_INSERT_ORDER

	{CmdChangeServiceInt,                           0}, // CMD_CHANGE_SERVICE_INT

	{CmdBuildIndustry,                              0}, // CMD_BUILD_INDUSTRY
	{CmdBuildCompanyHQ,       CMD_NO_WATER | CMD_AUTO}, // CMD_BUILD_COMPANY_HQ
	{CmdSetCompanyManagerFace,                      0}, // CMD_SET_COMPANY_MANAGER_FACE
	{CmdSetCompanyColour,                           0}, // CMD_SET_COMPANY_COLOUR

	{CmdIncreaseLoan,                               0}, // CMD_INCREASE_LOAN
	{CmdDecreaseLoan,                               0}, // CMD_DECREASE_LOAN

	{CmdWantEnginePreview,                          0}, // CMD_WANT_ENGINE_PREVIEW

	{CmdRenameVehicle,                              0}, // CMD_RENAME_VEHICLE
	{CmdRenameEngine,                               0}, // CMD_RENAME_ENGINE

	{CmdRenameCompany,                              0}, // CMD_RENAME_COMPANY
	{CmdRenamePresident,                            0}, // CMD_RENAME_PRESIDENT

	{CmdRenameStation,                              0}, // CMD_RENAME_STATION

	{CmdSellAircraft,                               0}, // CMD_SELL_AIRCRAFT

	{CmdBuildAircraft,                              0}, // CMD_BUILD_AIRCRAFT
	{CmdSendAircraftToHangar,                       0}, // CMD_SEND_AIRCRAFT_TO_HANGAR
	{CmdRefitAircraft,                              0}, // CMD_REFIT_AIRCRAFT

	{CmdPlaceSign,                                  0}, // CMD_PLACE_SIGN
	{CmdRenameSign,                                 0}, // CMD_RENAME_SIGN

	{CmdBuildRoadVeh,                               0}, // CMD_BUILD_ROAD_VEH
	{CmdSellRoadVeh,                                0}, // CMD_SELL_ROAD_VEH
	{CmdSendRoadVehToDepot,                         0}, // CMD_SEND_ROADVEH_TO_DEPOT
	{CmdTurnRoadVeh,                                0}, // CMD_TURN_ROADVEH
	{CmdRefitRoadVeh,                               0}, // CMD_REFIT_ROAD_VEH

	{CmdPause,                             CMD_SERVER}, // CMD_PAUSE

	{CmdBuyShareInCompany,                          0}, // CMD_BUY_SHARE_IN_COMPANY
	{CmdSellShareInCompany,                         0}, // CMD_SELL_SHARE_IN_COMPANY
	{CmdBuyCompany,                                 0}, // CMD_BUY_COMANY

	{CmdBuildTown,                        CMD_OFFLINE}, // CMD_BUILD_TOWN
	{CmdRenameTown,                        CMD_SERVER}, // CMD_RENAME_TOWN
	{CmdDoTownAction,                               0}, // CMD_DO_TOWN_ACTION

	{CmdSellShip,                                   0}, // CMD_SELL_SHIP
	{CmdBuildShip,                                  0}, // CMD_BUILD_SHIP
	{CmdSendShipToDepot,                            0}, // CMD_SEND_SHIP_TO_DEPOT
	{CmdRefitShip,                                  0}, // CMD_REFIT_SHIP

	{CmdOrderRefit,                                 0}, // CMD_ORDER_REFIT
	{CmdCloneOrder,                                 0}, // CMD_CLONE_ORDER

	{CmdClearArea,                        CMD_NO_TEST}, // CMD_CLEAR_AREA; destroying multi-tile houses makes town rating differ between test and execution

	{CmdMoneyCheat,                       CMD_OFFLINE}, // CMD_MONEY_CHEAT
	{CmdBuildCanal,                          CMD_AUTO}, // CMD_BUILD_CANAL
	{CmdCompanyCtrl,                    CMD_SPECTATOR}, // CMD_COMPANY_CTRL

	{CmdLevelLand, CMD_ALL_TILES | CMD_NO_TEST | CMD_AUTO}, // CMD_LEVEL_LAND; test run might clear tiles multiple times, in execution that only happens once

	{CmdRefitRailVehicle,                           0}, // CMD_REFIT_RAIL_VEHICLE
	{CmdRestoreOrderIndex,                          0}, // CMD_RESTORE_ORDER_INDEX
	{CmdBuildLock,                           CMD_AUTO}, // CMD_BUILD_LOCK

	{CmdBuildSignalTrack,                    CMD_AUTO}, // CMD_BUILD_SIGNAL_TRACK
	{CmdRemoveSignalTrack,                   CMD_AUTO}, // CMD_REMOVE_SIGNAL_TRACK

	{CmdGiveMoney,                                  0}, // CMD_GIVE_MONEY
	{CmdChangeSetting,                     CMD_SERVER}, // CMD_CHANGE_SETTING
	{CmdSetAutoReplace,                             0}, // CMD_SET_AUTOREPLACE
	{CmdCloneVehicle,                     CMD_NO_TEST}, // CMD_CLONE_VEHICLE; NewGRF callbacks influence building and refitting making it impossible to correctly estimate the cost
	{CmdStartStopVehicle,                           0}, // CMD_START_STOP_VEHICLE
	{CmdMassStartStopVehicle,                       0}, // CMD_MASS_START_STOP
	{CmdAutoreplaceVehicle,                         0}, // CMD_AUTOREPLACE_VEHICLE
	{CmdDepotSellAllVehicles,                       0}, // CMD_DEPOT_SELL_ALL_VEHICLES
	{CmdDepotMassAutoReplace,                       0}, // CMD_DEPOT_MASS_AUTOREPLACE
	{CmdCreateGroup,                                0}, // CMD_CREATE_GROUP
	{CmdDeleteGroup,                                0}, // CMD_DELETE_GROUP
	{CmdRenameGroup,                                0}, // CMD_RENAME_GROUP
	{CmdAddVehicleGroup,                            0}, // CMD_ADD_VEHICLE_GROUP
	{CmdAddSharedVehicleGroup,                      0}, // CMD_ADD_SHARE_VEHICLE_GROUP
	{CmdRemoveAllVehiclesGroup,                     0}, // CMD_REMOVE_ALL_VEHICLES_GROUP
	{CmdSetGroupReplaceProtection,                  0}, // CMD_SET_GROUP_REPLACE_PROTECTION
	{CmdMoveOrder,                                  0}, // CMD_MOVE_ORDER
	{CmdChangeTimetable,                            0}, // CMD_CHANGE_TIMETABLE
	{CmdSetVehicleOnTime,                           0}, // CMD_SET_VEHICLE_ON_TIME
	{CmdAutofillTimetable,                          0}, // CMD_AUTOFILL_TIMETABLE
};

/*!
 * This function range-checks a cmd, and checks if the cmd is not NULL
 *
 * @param cmd The integervalue of a command
 * @return true if the command is valid (and got a CommandProc function)
 */
bool IsValidCommand(uint32 cmd)
{
	cmd &= CMD_ID_MASK;

	return
		cmd < lengthof(_command_proc_table) &&
		_command_proc_table[cmd].proc != NULL;
}

/*!
 * This function mask the parameter with CMD_ID_MASK and returns
 * the flags which belongs to the given command.
 *
 * @param cmd The integer value of the command
 * @return The flags for this command
 */
byte GetCommandFlags(uint32 cmd)
{
	assert(IsValidCommand(cmd));

	return _command_proc_table[cmd & CMD_ID_MASK].flags;
}

static int _docommand_recursive = 0;

/**
 * Shorthand for calling the long DoCommand with a container.
 *
 * @param container Container with (almost) all information
 * @param flags Flags for the command and how to execute the command
 * @see CommandProc
 * @return the cost
 */
CommandCost DoCommand(const CommandContainer *container, DoCommandFlag flags)
{
	return DoCommand(container->tile, container->p1, container->p2, flags, container->cmd & CMD_ID_MASK, container->text);
}

/*!
 * This function executes a given command with the parameters from the #CommandProc parameter list.
 * Depending on the flags parameter it execute or test a command.
 *
 * @param tile The tile to apply the command on (for the #CommandProc)
 * @param p1 Additional data for the command (for the #CommandProc)
 * @param p2 Additional data for the command (for the #CommandProc)
 * @param flags Flags for the command and how to execute the command
 * @param cmd The command-id to execute (a value of the CMD_* enums)
 * @see CommandProc
 * @return the cost
 */
CommandCost DoCommand(TileIndex tile, uint32 p1, uint32 p2, DoCommandFlag flags, uint32 cmd, const char *text)
{
	CommandCost res;

	/* Do not even think about executing out-of-bounds tile-commands */
	if (tile != 0 && (tile >= MapSize() || (!IsValidTile(tile) && (flags & DC_ALL_TILES) == 0))) return CMD_ERROR;

	CommandProc *proc = _command_proc_table[cmd].proc;

	if (_docommand_recursive == 0) _error_message = INVALID_STRING_ID;

	_docommand_recursive++;

	/* only execute the test call if it's toplevel, or we're not execing. */
	if (_docommand_recursive == 1 || !(flags & DC_EXEC) ) {
		SetTownRatingTestMode(true);
		res = proc(tile, flags & ~DC_EXEC, p1, p2, text);
		SetTownRatingTestMode(false);
		if (CmdFailed(res)) {
			res.SetGlobalErrorMessage();
			goto error;
		}

		if (_docommand_recursive == 1 &&
				!(flags & DC_QUERY_COST) &&
				!(flags & DC_BANKRUPT) &&
				res.GetCost() != 0 &&
				!CheckCompanyHasMoney(res)) {
			goto error;
		}

		if (!(flags & DC_EXEC)) {
			_docommand_recursive--;
			return res;
		}
	}

	/* Execute the command here. All cost-relevant functions set the expenses type
	 * themselves to the cost object at some point */
	res = proc(tile, flags, p1, p2, text);
	if (CmdFailed(res)) {
		res.SetGlobalErrorMessage();
error:
		_docommand_recursive--;
		return CMD_ERROR;
	}

	/* if toplevel, subtract the money. */
	if (--_docommand_recursive == 0 && !(flags & DC_BANKRUPT)) {
		SubtractMoneyFromCompany(res);
	}

	return res;
}

/*!
 * This functions returns the money which can be used to execute a command.
 * This is either the money of the current company or INT64_MAX if there
 * is no such a company "at the moment" like the server itself.
 *
 * @return The available money of a company or INT64_MAX
 */
Money GetAvailableMoneyForCommand()
{
	CompanyID company = _current_company;
	if (!IsValidCompanyID(company)) return INT64_MAX;
	return GetCompany(company)->money;
}

/**
 * Shortcut for the long DoCommandP when having a container with the data.
 * @param container the container with information.
 * @param my_cmd indicator if the command is from a company or server (to display error messages for a user)
 * @return true if the command succeeded, else false
 */
bool DoCommandP(const CommandContainer *container, bool my_cmd)
{
	return DoCommandP(container->tile, container->p1, container->p2, container->cmd, container->callback, container->text, my_cmd);
}

/*!
 * Toplevel network safe docommand function for the current company. Must not be called recursively.
 * The callback is called when the command succeeded or failed. The parameters
 * tile, p1 and p2 are from the #CommandProc function. The paramater cmd is the command to execute.
 * The parameter my_cmd is used to indicate if the command is from a company or the server.
 *
 * @param tile The tile to perform a command on (see #CommandProc)
 * @param p1 Additional data for the command (see #CommandProc)
 * @param p2 Additional data for the command (see #CommandProc)
 * @param cmd The command to execute (a CMD_* value)
 * @param callback A callback function to call after the command is finished
 * @param text The text to pass
 * @param my_cmd indicator if the command is from a company or server (to display error messages for a user)
 * @return true if the command succeeded, else false
 */
bool DoCommandP(TileIndex tile, uint32 p1, uint32 p2, uint32 cmd, CommandCallback *callback, const char *text, bool my_cmd)
{
	assert(_docommand_recursive == 0);

	CommandCost res, res2;

	int x = TileX(tile) * TILE_SIZE;
	int y = TileY(tile) * TILE_SIZE;

	_error_message = INVALID_STRING_ID;
	StringID error_part1 = GB(cmd, 16, 16);
	_additional_cash_required = 0;

	CompanyID old_company = _current_company;

	/** Spectator has no rights except for the (dedicated) server which
	 * is/can be a spectator but as the server it can do anything */
	if (_current_company == COMPANY_SPECTATOR && !_network_server) {
		if (my_cmd) ShowErrorMessage(_error_message, error_part1, x, y);
		return false;
	}

	/* get pointer to command handler */
	byte cmd_id = cmd & CMD_ID_MASK;
	assert(cmd_id < lengthof(_command_proc_table));

	CommandProc *proc = _command_proc_table[cmd_id].proc;
	if (proc == NULL) return false;

	/* Command flags are used internally */
	uint cmd_flags = GetCommandFlags(cmd);
	/* Flags get send to the DoCommand */
	DoCommandFlag flags = CommandFlagsToDCFlags(cmd_flags);

	/* Do not even think about executing out-of-bounds tile-commands */
	if (tile != 0 && (tile >= MapSize() || (!IsValidTile(tile) && (cmd_flags & CMD_ALL_TILES) == 0))) return false;

	/* If the server is a spectator, it may only do server commands! */
	if (_current_company == COMPANY_SPECTATOR && (cmd_flags & (CMD_SPECTATOR | CMD_SERVER)) == 0) return false;

	bool notest = (cmd_flags & CMD_NO_TEST) != 0;

	_docommand_recursive = 1;

	/* cost estimation only? */
	if (!IsGeneratingWorld() &&
			_shift_pressed &&
			IsLocalCompany() &&
			!(cmd & CMD_NETWORK_COMMAND) &&
			cmd_id != CMD_PAUSE) {
		/* estimate the cost. */
		SetTownRatingTestMode(true);
		res = proc(tile, flags, p1, p2, text);
		SetTownRatingTestMode(false);
		if (CmdFailed(res)) {
			res.SetGlobalErrorMessage();
			ShowErrorMessage(_error_message, error_part1, x, y);
		} else {
			ShowEstimatedCostOrIncome(res.GetCost(), x, y);
		}

		_docommand_recursive = 0;
		ClearStorageChanges(false);
		return false;
	}


	if (!((cmd & CMD_NO_TEST_IF_IN_NETWORK) && _networking)) {
		/* first test if the command can be executed. */
		SetTownRatingTestMode(true);
		res = proc(tile, flags, p1, p2, text);
		SetTownRatingTestMode(false);
		assert(cmd_id == CMD_COMPANY_CTRL || old_company == _current_company);
		if (CmdFailed(res)) {
			res.SetGlobalErrorMessage();
			goto show_error;
		}
		/* no money? Only check if notest is off */
		if (!notest && res.GetCost() != 0 && !CheckCompanyHasMoney(res)) goto show_error;
	}

#ifdef ENABLE_NETWORK
	/** If we are in network, and the command is not from the network
	 * send it to the command-queue and abort execution
	 * If we are a dedicated server temporarily switch local company, otherwise
	 * the other parties won't be able to execute our command and will desync.
	 * We also need to do this if the server's company has gone bankrupt
	 * @todo Rewrite (dedicated) server to something more than a dirty hack!
	 */
	if (_networking && !(cmd & CMD_NETWORK_COMMAND)) {
		CompanyID bck = _local_company;
		if (_network_dedicated || (_network_server && bck == COMPANY_SPECTATOR)) _local_company = COMPANY_FIRST;
		NetworkSend_Command(tile, p1, p2, cmd & ~CMD_FLAGS_MASK, callback, text);
		if (_network_dedicated || (_network_server && bck == COMPANY_SPECTATOR)) _local_company = bck;
		_docommand_recursive = 0;
		ClearStorageChanges(false);
		return true;
	}
#endif /* ENABLE_NETWORK */
	DEBUG(desync, 1, "cmd: %08x; %08x; %1x; %06x; %08x; %08x; %04x; %s\n", _date, _date_fract, (int)_current_company, tile, p1, p2, cmd & ~CMD_NETWORK_COMMAND, text);

	/* update last build coordinate of company. */
	if (tile != 0 && IsValidCompanyID(_current_company)) {
		GetCompany(_current_company)->last_build_coordinate = tile;
	}

	/* Actually try and execute the command. If no cost-type is given
	 * use the construction one */
	res2 = proc(tile, flags | DC_EXEC, p1, p2, text);

	assert(cmd_id == CMD_COMPANY_CTRL || old_company == _current_company);

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

	SubtractMoneyFromCompany(res2);

	/* update signals if needed */
	UpdateSignalsInBuffer();

	if (IsLocalCompany() && _game_mode != GM_EDITOR) {
		if (res2.GetCost() != 0 && tile != 0) ShowCostOrIncomeAnimation(x, y, GetSlopeZ(x, y), res2.GetCost());
		if (_additional_cash_required != 0) {
			SetDParam(0, _additional_cash_required);
			if (my_cmd) ShowErrorMessage(STR_0003_NOT_ENOUGH_CASH_REQUIRES, error_part1, x, y);
			if (res2.GetCost() == 0) goto callb_err;
		}
	}

	_docommand_recursive = 0;

	if (callback) callback(true, tile, p1, p2);
	ClearStorageChanges(true);
	return true;

show_error:
	/* show error message if the command fails? */
	if (IsLocalCompany() && error_part1 != 0 && my_cmd) {
		ShowErrorMessage(_error_message, error_part1, x, y);
	}

callb_err:
	_docommand_recursive = 0;

	if (callback) callback(false, tile, p1, p2);
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
