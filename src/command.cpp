/* $Id$ */

/** @file command.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "table/strings.h"
#include "functions.h"
#include "landscape.h"
#include "map.h"
#include "gui.h"
#include "command.h"
#include "player.h"
#include "network/network.h"
#include "variables.h"
#include "genworld.h"

const char* _cmd_text = NULL;

#define DEF_COMMAND(yyyy) int32 yyyy(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)

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

/* The master command table */
static const Command _command_proc_table[] = {
	{CmdBuildRailroadTrack,                  0}, /*   0 */
	{CmdRemoveRailroadTrack,                 0}, /*   1 */
	{CmdBuildSingleRail,                     0}, /*   2 */
	{CmdRemoveSingleRail,                    0}, /*   3 */
	{CmdLandscapeClear,                      0}, /*   4 */
	{CmdBuildBridge,                         0}, /*   5 */
	{CmdBuildRailroadStation,                0}, /*   6 */
	{CmdBuildTrainDepot,                     0}, /*   7 */
	{CmdBuildSingleSignal,                   0}, /*   8 */
	{CmdRemoveSingleSignal,                  0}, /*   9 */
	{CmdTerraformLand,                       0}, /*  10 */
	{CmdPurchaseLandArea,                    0}, /*  11 */
	{CmdSellLandArea,                        0}, /*  12 */
	{CmdBuildTunnel,                         0}, /*  13 */
	{CmdRemoveFromRailroadStation,           0}, /*  14 */
	{CmdConvertRail,                         0}, /*  15 */
	{CmdBuildTrainWaypoint,                  0}, /*  16 */
	{CmdRenameWaypoint,                      0}, /*  17 */
	{CmdRemoveTrainWaypoint,                 0}, /*  18 */
	{NULL,                                   0}, /*  19 */
	{NULL,                                   0}, /*  20 */
	{CmdBuildRoadStop,                       0}, /*  21 */
	{CmdRemoveRoadStop,                      0}, /*  22 */
	{CmdBuildLongRoad,                       0}, /*  23 */
	{CmdRemoveLongRoad,                      0}, /*  24 */
	{CmdBuildRoad,                           0}, /*  25 */
	{CmdRemoveRoad,                          0}, /*  26 */
	{CmdBuildRoadDepot,                      0}, /*  27 */
	{NULL,                                   0}, /*  28 */
	{CmdBuildAirport,                        0}, /*  29 */
	{CmdBuildDock,                           0}, /*  30 */
	{CmdBuildShipDepot,                      0}, /*  31 */
	{CmdBuildBuoy,                           0}, /*  32 */
	{CmdPlantTree,                           0}, /*  33 */
	{CmdBuildRailVehicle,                    0}, /*  34 */
	{CmdMoveRailVehicle,                     0}, /*  35 */
	{CmdStartStopTrain,                      0}, /*  36 */
	{NULL,                                   0}, /*  37 */
	{CmdSellRailWagon,                       0}, /*  38 */
	{CmdSendTrainToDepot,                    0}, /*  39 */
	{CmdForceTrainProceed,                   0}, /*  40 */
	{CmdReverseTrainDirection,               0}, /*  41 */

	{CmdModifyOrder,                         0}, /*  42 */
	{CmdSkipToOrder,                         0}, /*  43 */
	{CmdDeleteOrder,                         0}, /*  44 */
	{CmdInsertOrder,                         0}, /*  45 */

	{CmdChangeServiceInt,                    0}, /*  46 */

	{CmdBuildIndustry,                       0}, /*  47 */
	{CmdBuildCompanyHQ,                      0}, /*  48 */
	{CmdSetPlayerFace,                       0}, /*  49 */
	{CmdSetPlayerColor,                      0}, /*  50 */

	{CmdIncreaseLoan,                        0}, /*  51 */
	{CmdDecreaseLoan,                        0}, /*  52 */

	{CmdWantEnginePreview,                   0}, /*  53 */

	{CmdNameVehicle,                         0}, /*  54 */
	{CmdRenameEngine,                        0}, /*  55 */

	{CmdChangeCompanyName,                   0}, /*  56 */
	{CmdChangePresidentName,                 0}, /*  57 */

	{CmdRenameStation,                       0}, /*  58 */

	{CmdSellAircraft,                        0}, /*  59 */
	{CmdStartStopAircraft,                   0}, /*  60 */

	{CmdBuildAircraft,                       0}, /*  61 */
	{CmdSendAircraftToHangar,                0}, /*  62 */
	{NULL,                                   0}, /*  63 */
	{CmdRefitAircraft,                       0}, /*  64 */

	{CmdPlaceSign,                           0}, /*  65 */
	{CmdRenameSign,                          0}, /*  66 */

	{CmdBuildRoadVeh,                        0}, /*  67 */
	{CmdStartStopRoadVeh,                    0}, /*  68 */
	{CmdSellRoadVeh,                         0}, /*  69 */
	{CmdSendRoadVehToDepot,                  0}, /*  70 */
	{CmdTurnRoadVeh,                         0}, /*  71 */
	{CmdRefitRoadVeh,                        0}, /*  72 */

	{CmdPause,                      CMD_SERVER}, /*  73 */

	{CmdBuyShareInCompany,                   0}, /*  74 */
	{CmdSellShareInCompany,                  0}, /*  75 */
	{CmdBuyCompany,                          0}, /*  76 */

	{CmdBuildTown,                 CMD_OFFLINE}, /*  77 */
	{NULL,                                   0}, /*  78 */
	{NULL,                                   0}, /*  79 */
	{CmdRenameTown,                 CMD_SERVER}, /*  80 */
	{CmdDoTownAction,                        0}, /*  81 */

	{CmdSetRoadDriveSide,           CMD_SERVER}, /*  82 */
	{NULL,                                   0}, /*  83 */
	{NULL,                                   0}, /*  84 */
	{CmdChangeDifficultyLevel,      CMD_SERVER}, /*  85 */

	{CmdStartStopShip,                       0}, /*  86 */
	{CmdSellShip,                            0}, /*  87 */
	{CmdBuildShip,                           0}, /*  88 */
	{CmdSendShipToDepot,                     0}, /*  89 */
	{NULL,                                   0}, /*  90 */
	{CmdRefitShip,                           0}, /*  91 */

	{NULL,                                   0}, /*  92 */
	{NULL,                                   0}, /*  93 */
	{NULL,                                   0}, /*  94 */
	{NULL,                                   0}, /*  95 */
	{NULL,                                   0}, /*  96 */
	{NULL,                                   0}, /*  97 */

	{CmdOrderRefit,                          0}, /*  98 */
	{CmdCloneOrder,                          0}, /*  99 */

	{CmdClearArea,                           0}, /* 100 */
	{NULL,                                   0}, /* 101 */

	{CmdMoneyCheat,                CMD_OFFLINE}, /* 102 */
	{CmdBuildCanal,                          0}, /* 103 */
	{CmdPlayerCtrl,                          0}, /* 104 */

	{CmdLevelLand,                           0}, /* 105 */

	{CmdRefitRailVehicle,                    0}, /* 106 */
	{CmdRestoreOrderIndex,                   0}, /* 107 */
	{CmdBuildLock,                           0}, /* 108 */
	{NULL,                                   0}, /* 109 */
	{CmdBuildSignalTrack,                    0}, /* 110 */
	{CmdRemoveSignalTrack,                   0}, /* 111 */
	{NULL,                                   0}, /* 112 */
	{CmdGiveMoney,                           0}, /* 113 */
	{CmdChangePatchSetting,         CMD_SERVER}, /* 114 */
	{CmdSetAutoReplace,                      0}, /* 115 */
	{CmdCloneVehicle,                        0}, /* 116 */
	{CmdMassStartStopVehicle,                0}, /* 117 */
	{CmdDepotSellAllVehicles,                0}, /* 118 */
	{CmdDepotMassAutoReplace,                0}, /* 119 */
	{CmdCreateGroup,                         0}, /* 120 */
	{CmdDeleteGroup,                         0}, /* 121 */
	{CmdRenameGroup,                         0}, /* 122 */
	{CmdAddVehicleGroup,                     0}, /* 123 */
	{CmdAddSharedVehicleGroup,               0}, /* 124 */
	{CmdRemoveAllVehiclesGroup,              0}, /* 125 */
	{CmdSetGroupReplaceProtection,           0}, /* 126 */
};

/* This function range-checks a cmd, and checks if the cmd is not NULL */
bool IsValidCommand(uint cmd)
{
	cmd &= 0xFF;

	return
		cmd < lengthof(_command_proc_table) &&
		_command_proc_table[cmd].proc != NULL;
}

byte GetCommandFlags(uint cmd)
{
	return _command_proc_table[cmd & 0xFF].flags;
}


static int _docommand_recursive;

int32 DoCommand(TileIndex tile, uint32 p1, uint32 p2, uint32 flags, uint procc)
{
	int32 res;
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
			if (res & 0xFFFF) _error_message = res & 0xFFFF;
			goto error;
		}

		if (_docommand_recursive == 1 &&
				!(flags & DC_QUERY_COST) &&
				res != 0 &&
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
		if (res & 0xFFFF) _error_message = res & 0xFFFF;
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

int32 GetAvailableMoneyForCommand()
{
	PlayerID pid = _current_player;
	if (!IsValidPlayer(pid)) return 0x7FFFFFFF; // max int
	return GetPlayer(pid)->player_money;
}

/* toplevel network safe docommand function for the current player. must not be called recursively.
 * the callback is called when the command succeeded or failed. */
bool DoCommandP(TileIndex tile, uint32 p1, uint32 p2, CommandCallback *callback, uint32 cmd)
{
	int32 res = 0, res2;
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
		ShowErrorMessage(_error_message, error_part1, x, y);
		_cmd_text = NULL;
		return false;
	}

	flags = 0;
	if (cmd & CMD_AUTO) flags |= DC_AUTO;
	if (cmd & CMD_NO_WATER) flags |= DC_NO_WATER;

	/* get pointer to command handler */
	assert((cmd & 0xFF) < lengthof(_command_proc_table));
	proc = _command_proc_table[cmd & 0xFF].proc;
	if (proc == NULL) {
		_cmd_text = NULL;
		return false;
	}

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
			if (res & 0xFFFF) _error_message = res & 0xFFFF;
			ShowErrorMessage(_error_message, error_part1, x, y);
		} else {
			ShowEstimatedCostOrIncome(res, x, y);
		}

		_docommand_recursive = 0;
		_cmd_text = NULL;
		return false;
	}


	if (!((cmd & CMD_NO_TEST_IF_IN_NETWORK) && _networking)) {
		/* first test if the command can be executed. */
		res = proc(tile, flags, p1, p2);
		if (CmdFailed(res)) {
			if (res & 0xFFFF) _error_message = res & 0xFFFF;
			goto show_error;
		}
		/* no money? Only check if notest is off */
		if (!notest && res != 0 && !CheckPlayerHasMoney(res)) goto show_error;
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
		assert(res == res2); // sanity check
	} else {
		if (CmdFailed(res2)) {
			if (res2 & 0xFFFF) _error_message = res2 & 0xFFFF;
			goto show_error;
		}
	}

	SubtractMoneyFromPlayer(res2);

	if (IsLocalPlayer() && _game_mode != GM_EDITOR) {
		if (res2 != 0) ShowCostOrIncomeAnimation(x, y, GetSlopeZ(x, y), res2);
		if (_additional_cash_required) {
			SetDParam(0, _additional_cash_required);
			ShowErrorMessage(STR_0003_NOT_ENOUGH_CASH_REQUIRES, error_part1, x, y);
			if (res2 == 0) goto callb_err;
		}
	}

	_docommand_recursive = 0;

	if (callback) callback(true, tile, p1, p2);
	_cmd_text = NULL;
	return true;

show_error:
	/* show error message if the command fails? */
	if (IsLocalPlayer() && error_part1 != 0) {
		ShowErrorMessage(_error_message, error_part1, x, y);
	}

callb_err:
	_docommand_recursive = 0;

	if (callback) callback(false, tile, p1, p2);
	_cmd_text = NULL;
	return false;
}
