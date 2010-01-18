/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file command.cpp Handling of commands. */

#include "stdafx.h"
#include "openttd.h"
#include "landscape.h"
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

CommandProc CmdBuildRailroadTrack;
CommandProc CmdRemoveRailroadTrack;
CommandProc CmdBuildSingleRail;
CommandProc CmdRemoveSingleRail;

CommandProc CmdLandscapeClear;

CommandProc CmdBuildBridge;

CommandProc CmdBuildRailStation;
CommandProc CmdRemoveFromRailStation;
CommandProc CmdConvertRail;

CommandProc CmdBuildSingleSignal;
CommandProc CmdRemoveSingleSignal;

CommandProc CmdTerraformLand;

CommandProc CmdPurchaseLandArea;
CommandProc CmdSellLandArea;

CommandProc CmdBuildTunnel;

CommandProc CmdBuildTrainDepot;
CommandProc CmdBuildRailWaypoint;
CommandProc CmdRenameWaypoint;
CommandProc CmdRemoveFromRailWaypoint;

CommandProc CmdBuildRoadStop;
CommandProc CmdRemoveRoadStop;

CommandProc CmdBuildLongRoad;
CommandProc CmdRemoveLongRoad;
CommandProc CmdBuildRoad;
CommandProc CmdRemoveRoad;

CommandProc CmdBuildRoadDepot;

CommandProc CmdBuildAirport;

CommandProc CmdBuildDock;

CommandProc CmdBuildShipDepot;

CommandProc CmdBuildBuoy;

CommandProc CmdPlantTree;

CommandProc CmdBuildRailVehicle;
CommandProc CmdMoveRailVehicle;

CommandProc CmdSellRailWagon;

CommandProc CmdSendTrainToDepot;
CommandProc CmdForceTrainProceed;
CommandProc CmdReverseTrainDirection;

CommandProc CmdModifyOrder;
CommandProc CmdSkipToOrder;
CommandProc CmdDeleteOrder;
CommandProc CmdInsertOrder;
CommandProc CmdChangeServiceInt;
CommandProc CmdRestoreOrderIndex;

CommandProc CmdBuildIndustry;

CommandProc CmdBuildCompanyHQ;
CommandProc CmdSetCompanyManagerFace;
CommandProc CmdSetCompanyColour;

CommandProc CmdIncreaseLoan;
CommandProc CmdDecreaseLoan;

CommandProc CmdWantEnginePreview;

CommandProc CmdRenameVehicle;
CommandProc CmdRenameEngine;

CommandProc CmdRenameCompany;
CommandProc CmdRenamePresident;

CommandProc CmdRenameStation;

CommandProc CmdSellAircraft;
CommandProc CmdBuildAircraft;
CommandProc CmdSendAircraftToHangar;
CommandProc CmdRefitAircraft;

CommandProc CmdPlaceSign;
CommandProc CmdRenameSign;

CommandProc CmdBuildRoadVeh;
CommandProc CmdSellRoadVeh;
CommandProc CmdSendRoadVehToDepot;
CommandProc CmdTurnRoadVeh;
CommandProc CmdRefitRoadVeh;

CommandProc CmdPause;

CommandProc CmdBuyShareInCompany;
CommandProc CmdSellShareInCompany;
CommandProc CmdBuyCompany;

CommandProc CmdFoundTown;

CommandProc CmdRenameTown;
CommandProc CmdDoTownAction;

CommandProc CmdChangeSetting;
CommandProc CmdChangeCompanySetting;

CommandProc CmdSellShip;
CommandProc CmdBuildShip;
CommandProc CmdSendShipToDepot;
CommandProc CmdRefitShip;

CommandProc CmdOrderRefit;
CommandProc CmdCloneOrder;

CommandProc CmdClearArea;

CommandProc CmdGiveMoney;
CommandProc CmdMoneyCheat;
CommandProc CmdBuildCanal;
CommandProc CmdBuildLock;

CommandProc CmdCompanyCtrl;

CommandProc CmdLevelLand;

CommandProc CmdRefitRailVehicle;

CommandProc CmdBuildSignalTrack;
CommandProc CmdRemoveSignalTrack;

CommandProc CmdSetAutoReplace;

CommandProc CmdCloneVehicle;
CommandProc CmdStartStopVehicle;
CommandProc CmdMassStartStopVehicle;
CommandProc CmdAutoreplaceVehicle;
CommandProc CmdDepotSellAllVehicles;
CommandProc CmdDepotMassAutoReplace;

CommandProc CmdCreateGroup;
CommandProc CmdRenameGroup;
CommandProc CmdDeleteGroup;
CommandProc CmdAddVehicleGroup;
CommandProc CmdAddSharedVehicleGroup;
CommandProc CmdRemoveAllVehiclesGroup;
CommandProc CmdSetGroupReplaceProtection;

CommandProc CmdMoveOrder;
CommandProc CmdChangeTimetable;
CommandProc CmdSetVehicleOnTime;
CommandProc CmdAutofillTimetable;
CommandProc CmdSetTimetableStart;

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
	{CmdBuildRailStation,     CMD_NO_WATER | CMD_AUTO}, // CMD_BUILD_RAIL_STATION
	{CmdBuildTrainDepot,      CMD_NO_WATER | CMD_AUTO}, // CMD_BUILD_TRAIN_DEPOT
	{CmdBuildSingleSignal,                   CMD_AUTO}, // CMD_BUILD_SIGNALS
	{CmdRemoveSingleSignal,                  CMD_AUTO}, // CMD_REMOVE_SIGNALS
	{CmdTerraformLand,       CMD_ALL_TILES | CMD_AUTO}, // CMD_TERRAFORM_LAND
	{CmdPurchaseLandArea,     CMD_NO_WATER | CMD_AUTO}, // CMD_PURCHASE_LAND_AREA
	{CmdSellLandArea,                               0}, // CMD_SELL_LAND_AREA
	{CmdBuildTunnel,                         CMD_AUTO}, // CMD_BUILD_TUNNEL
	{CmdRemoveFromRailStation,                      0}, // CMD_REMOVE_FROM_RAIL_STATION
	{CmdConvertRail,                                0}, // CMD_CONVERT_RAILD
	{CmdBuildRailWaypoint,                          0}, // CMD_BUILD_RAIL_WAYPOINT
	{CmdRenameWaypoint,                             0}, // CMD_RENAME_WAYPOINT
	{CmdRemoveFromRailWaypoint,                     0}, // CMD_REMOVE_FROM_RAIL_WAYPOINT

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

	{CmdFoundTown,                        CMD_NO_TEST}, // CMD_FOUND_TOWN; founding random town can fail only in exec run
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
	{CmdChangeCompanySetting,                       0}, // CMD_CHANGE_COMPANY_SETTING
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
	{CmdSetTimetableStart,                          0}, // CMD_SET_TIMETABLE_START
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
 * @param text The text to pass
 * @see CommandProc
 * @return the cost
 */
CommandCost DoCommand(TileIndex tile, uint32 p1, uint32 p2, DoCommandFlag flags, uint32 cmd, const char *text)
{
	CommandCost res;

	/* Do not even think about executing out-of-bounds tile-commands */
	if (tile != 0 && (tile >= MapSize() || (!IsValidTile(tile) && (flags & DC_ALL_TILES) == 0))) return CMD_ERROR;

	/* Chop of any CMD_MSG or other flags; we don't need those here */
	CommandProc *proc = _command_proc_table[cmd & CMD_ID_MASK].proc;

	if (_docommand_recursive == 0) _error_message = INVALID_STRING_ID;

	_docommand_recursive++;

	/* only execute the test call if it's toplevel, or we're not execing. */
	if (_docommand_recursive == 1 || !(flags & DC_EXEC) ) {
		SetTownRatingTestMode(true);
		res = proc(tile, flags & ~DC_EXEC, p1, p2, text);
		SetTownRatingTestMode(false);
		if (res.Failed()) {
			goto error;
		}

		if (_docommand_recursive == 1 &&
				!(flags & DC_QUERY_COST) &&
				!(flags & DC_BANKRUPT) &&
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
	if (res.Failed()) {
error:
		res.SetGlobalErrorMessage();
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
	if (!Company::IsValidID(company)) return INT64_MAX;
	return Company::Get(company)->money;
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
	/* Cost estimation is generally only done when the
	 * local user presses shift while doing somthing.
	 * However, in case of incoming network commands,
	 * map generation of the pause button we do want
	 * to execute. */
	bool estimate_only = _shift_pressed && IsLocalCompany() &&
			!IsGeneratingWorld() &&
			!(cmd & CMD_NETWORK_COMMAND) &&
			(cmd & CMD_ID_MASK) != CMD_PAUSE;

	/* We're only sending the command, so don't do
	 * fancy things for 'success'. */
	bool only_sending = _networking && !(cmd & CMD_NETWORK_COMMAND);

	/* Where to show the message? */
	int x = TileX(tile) * TILE_SIZE;
	int y = TileY(tile) * TILE_SIZE;

	CommandCost res = DoCommandPInternal(tile, p1, p2, cmd, callback, text, my_cmd, estimate_only);
	if (res.Failed()) {
		res.SetGlobalErrorMessage();

		/* Only show the error when it's for us. */
		StringID error_part1 = GB(cmd, 16, 16);
		if (estimate_only || (IsLocalCompany() && error_part1 != 0 && my_cmd)) {
			ShowErrorMessage(error_part1, _error_message, x, y);
		}
	} else if (estimate_only) {
		ShowEstimatedCostOrIncome(res.GetCost(), x, y);
	} else if (!only_sending && res.GetCost() != 0 && tile != 0 && IsLocalCompany() && _game_mode != GM_EDITOR) {
		/* Only show the cost animation when we did actually
		 * execute the command, i.e. we're not sending it to
		 * the server, when it has cost the local company
		 * something. Furthermore in the editor there is no
		 * concept of cost, so don't show it there either. */
		ShowCostOrIncomeAnimation(x, y, GetSlopeZ(x, y), res.GetCost());
	}

	if (!estimate_only && !only_sending && callback != NULL) {
		callback(res, tile, p1, p2);
	}

	return res.Succeeded();
}


/**
 * Helper to deduplicate the code for returning.
 * @param cmd   the command cost to return.
 * @param clear whether to keep the storage changes or not.
 */
#define return_dcpi(cmd, clear) { _docommand_recursive = 0; ClearStorageChanges(clear); return cmd; }

/*!
 * Helper function for the toplevel network safe docommand function for the current company.
 *
 * @param tile The tile to perform a command on (see #CommandProc)
 * @param p1 Additional data for the command (see #CommandProc)
 * @param p2 Additional data for the command (see #CommandProc)
 * @param cmd The command to execute (a CMD_* value)
 * @param callback A callback function to call after the command is finished
 * @param text The text to pass
 * @param my_cmd indicator if the command is from a company or server (to display error messages for a user)
 * @param estimate_only whether to give only the estimate or also execute the command
 * @return the command cost of this function.
 */
CommandCost DoCommandPInternal(TileIndex tile, uint32 p1, uint32 p2, uint32 cmd, CommandCallback *callback, const char *text, bool my_cmd, bool estimate_only)
{
	/* Prevent recursion; it gives a mess over the network */
	assert(_docommand_recursive == 0);
	_docommand_recursive = 1;

	/* Reset the state. */
	_error_message = INVALID_STRING_ID;
	_additional_cash_required = 0;

	/* Get pointer to command handler */
	byte cmd_id = cmd & CMD_ID_MASK;
	assert(cmd_id < lengthof(_command_proc_table));

	CommandProc *proc = _command_proc_table[cmd_id].proc;
	/* Shouldn't happen, but you never know when someone adds
	 * NULLs to the _command_proc_table. */
	assert(proc != NULL);

	/* Command flags are used internally */
	uint cmd_flags = GetCommandFlags(cmd);
	/* Flags get send to the DoCommand */
	DoCommandFlag flags = CommandFlagsToDCFlags(cmd_flags);

	/* Do not even think about executing out-of-bounds tile-commands */
	if (tile != 0 && (tile >= MapSize() || (!IsValidTile(tile) && (cmd_flags & CMD_ALL_TILES) == 0))) return_dcpi(CMD_ERROR, false);

	/* Always execute server and spectator commands as spectator */
	if (cmd_flags & (CMD_SPECTATOR | CMD_SERVER)) _current_company = COMPANY_SPECTATOR;

	CompanyID old_company = _current_company;

	/* If the company isn't valid it may only do server command or start a new company!
	 * The server will ditch any server commands a client sends to it, so effectively
	 * this guards the server from executing functions for an invalid company. */
	if (_game_mode == GM_NORMAL && (cmd_flags & (CMD_SPECTATOR | CMD_SERVER)) == 0 && !Company::IsValidID(_current_company)) {
		return_dcpi(CMD_ERROR, false);
	}

	bool test_and_exec_can_differ = (cmd_flags & CMD_NO_TEST) != 0;
	bool skip_test = _networking && (cmd & CMD_NO_TEST_IF_IN_NETWORK) != 0;

	/* Do we need to do a test run?
	 * Basically we need to always do this, except when
	 * the no-test-in-network flag is giving and we're
	 * in a network game (e.g. restoring orders would
	 * fail this test because the first order does not
	 * exist yet when inserting the second, giving that
	 * a wrong insert location and ignoring the command
	 * and thus breaking restoring). However, when we
	 * just want to do cost estimation we don't care
	 * because it's only done once anyway. */
	CommandCost res;
	if (estimate_only || !skip_test) {
		/* Test the command. */
		SetTownRatingTestMode(true);
		res = proc(tile, flags, p1, p2, text);
		SetTownRatingTestMode(false);

		/* Make sure we're not messing things up here. */
		assert(cmd_id == CMD_COMPANY_CTRL || old_company == _current_company);

		/* If the command fails, we're doing an estimate
		 * or the player does not have enough money
		 * (unless it's a command where the test and
		 * execution phase might return different costs)
		 * we bail out here. */
		if (res.Failed() || estimate_only ||
				(!test_and_exec_can_differ && !CheckCompanyHasMoney(res))) {
			return_dcpi(res, false);
		}
	}

#ifdef ENABLE_NETWORK
	/*
	 * If we are in network, and the command is not from the network
	 * send it to the command-queue and abort execution
	 */
	if (_networking && !(cmd & CMD_NETWORK_COMMAND)) {
		NetworkSend_Command(tile, p1, p2, cmd & ~CMD_FLAGS_MASK, callback, text, _current_company);

		/* Don't return anything special here; no error, no costs.
		 * This way it's not handled by DoCommand and only the
		 * actual execution of the command causes messages. Also
		 * reset the storages as we've not executed the command. */
		return_dcpi(CommandCost(), false);
	}
#endif /* ENABLE_NETWORK */
	DEBUG(desync, 1, "cmd: %08x; %08x; %1x; %06x; %08x; %08x; %04x; %s\n", _date, _date_fract, (int)_current_company, tile, p1, p2, cmd & ~CMD_NETWORK_COMMAND, text);

	/* Actually try and execute the command. If no cost-type is given
	 * use the construction one */
	CommandCost res2 = proc(tile, flags | DC_EXEC, p1, p2, text);

	/* Make sure nothing bad happened, like changing the current company. */
	assert(cmd_id == CMD_COMPANY_CTRL || old_company == _current_company);

	/* If the test and execution can differ, or we skipped the test
	 * we have to check the return of the command. Otherwise we can
	 * check whether the test and execution have yielded the same
	 * result, i.e. cost and error state are the same. */
	if (!test_and_exec_can_differ && !skip_test) {
		assert(res.GetCost() == res2.GetCost() && res.Failed() == res2.Failed()); // sanity check
	} else if (res2.Failed()) {
		return_dcpi(res2, false);
	}

	/* If we're needing more money and we haven't done
	 * anything yet, ask for the money! */
	if (_additional_cash_required != 0 && res2.GetCost() == 0) {
		SetDParam(0, _additional_cash_required);
		return_dcpi(CommandCost(STR_ERROR_NOT_ENOUGH_CASH_REQUIRES_CURRENCY), false);
	}

	/* update last build coordinate of company. */
	if (tile != 0) {
		Company *c = Company::GetIfValid(_current_company);
		if (c != NULL) c->last_build_coordinate = tile;
	}

	SubtractMoneyFromCompany(res2);

	/* update signals if needed */
	UpdateSignalsInBuffer();

	return_dcpi(res2, true);
}
#undef return_dcpi


CommandCost CommandCost::AddCost(CommandCost ret)
{
	this->AddCost(ret.cost);
	if (this->success && !ret.success) {
		this->message = ret.message;
		this->success = false;
	}
	return *this;
}
