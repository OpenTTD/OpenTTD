#include "stdafx.h"
#include "ttd.h"
#include "table/strings.h"
#include "gui.h"
#include "command.h"
#include "player.h"

#define DEF_COMMAND(yyyy) int32 yyyy(int x, int y, uint32 flags, uint32 p1, uint32 p2)

DEF_COMMAND(CmdBuildRailroadTrack);
DEF_COMMAND(CmdRemoveRailroadTrack);
DEF_COMMAND(CmdBuildSingleRail);
DEF_COMMAND(CmdRemoveSingleRail);

DEF_COMMAND(CmdLandscapeClear);

DEF_COMMAND(CmdBuildBridge);

DEF_COMMAND(CmdBuildRailroadStation);
DEF_COMMAND(CmdRemoveFromRailroadStation);
DEF_COMMAND(CmdConvertRail);

DEF_COMMAND(CmdBuildSignals);
DEF_COMMAND(CmdRemoveSignals);

DEF_COMMAND(CmdTerraformLand);

DEF_COMMAND(CmdPurchaseLandArea);
DEF_COMMAND(CmdSellLandArea);

DEF_COMMAND(CmdBuildTunnel);

DEF_COMMAND(CmdBuildTrainDepot);
DEF_COMMAND(CmdBuildTrainWaypoint);
DEF_COMMAND(CmdRenameWaypoint);
DEF_COMMAND(CmdRemoveTrainWaypoint);

DEF_COMMAND(CmdBuildTruckStation);

DEF_COMMAND(CmdBuildBusStation);

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

DEF_COMMAND(CmdTrainGotoDepot);
DEF_COMMAND(CmdForceTrainProceed);
DEF_COMMAND(CmdReverseTrainDirection);

DEF_COMMAND(CmdModifyOrder);
DEF_COMMAND(CmdSkipOrder);
DEF_COMMAND(CmdDeleteOrder);
DEF_COMMAND(CmdInsertOrder);
DEF_COMMAND(CmdChangeTrainServiceInt);
DEF_COMMAND(CmdRestoreOrderIndex);

DEF_COMMAND(CmdBuildIndustry);
//DEF_COMMAND(CmdDestroyIndustry);

DEF_COMMAND(CmdBuildCompanyHQ);
DEF_COMMAND(CmdDestroyCompanyHQ);
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
DEF_COMMAND(CmdChangeAircraftServiceInt);
DEF_COMMAND(CmdRefitAircraft);

DEF_COMMAND(CmdPlaceSign);
DEF_COMMAND(CmdRenameSign);

DEF_COMMAND(CmdBuildRoadVeh);
DEF_COMMAND(CmdStartStopRoadVeh);
DEF_COMMAND(CmdSellRoadVeh);
DEF_COMMAND(CmdSendRoadVehToDepot);
DEF_COMMAND(CmdTurnRoadVeh);
DEF_COMMAND(CmdChangeRoadVehServiceInt);

DEF_COMMAND(CmdPause);
DEF_COMMAND(CmdResume);

DEF_COMMAND(CmdBuyShareInCompany);
DEF_COMMAND(CmdSellShareInCompany);
DEF_COMMAND(CmdBuyCompany);

DEF_COMMAND(CmdBuildTown);

DEF_COMMAND(CmdRenameTown);
DEF_COMMAND(CmdDoTownAction);

DEF_COMMAND(CmdSetRoadDriveSide);
DEF_COMMAND(CmdSetTownNameType);

DEF_COMMAND(CmdChangeDifficultyLevel);

DEF_COMMAND(CmdStartStopShip);
DEF_COMMAND(CmdSellShip);
DEF_COMMAND(CmdBuildShip);
DEF_COMMAND(CmdSendShipToDepot);
DEF_COMMAND(CmdChangeShipServiceInt);
DEF_COMMAND(CmdRefitShip);


DEF_COMMAND(CmdStartNewGame);
DEF_COMMAND(CmdLoadGame);
DEF_COMMAND(CmdCreateScenario);
DEF_COMMAND(CmdSetSinglePlayer);

DEF_COMMAND(CmdSetNewLandscapeType);

DEF_COMMAND(CmdGenRandomNewGame);
DEF_COMMAND(CmdCloneOrder);

DEF_COMMAND(CmdClearArea);

DEF_COMMAND(CmdMoneyCheat);
DEF_COMMAND(CmdBuildCanal);
DEF_COMMAND(CmdBuildLock);

DEF_COMMAND(CmdPlayerCtrl);

DEF_COMMAND(CmdLevelLand);

DEF_COMMAND(CmdRefitRailVehicle);

DEF_COMMAND(CmdStartScenario);

DEF_COMMAND(CmdBuildManySignals);

/* The master command table */
static CommandProc * const _command_proc_table[] = {
	CmdBuildRailroadTrack,				/* 0  */
	CmdRemoveRailroadTrack,				/* 1  */
	CmdBuildSingleRail,						/* 2  */
	CmdRemoveSingleRail,					/* 3  */
	CmdLandscapeClear,						/* 4  */
	CmdBuildBridge,								/* 5  */
	CmdBuildRailroadStation,			/* 6  */
	CmdBuildTrainDepot,						/* 7  */
	CmdBuildSignals,							/* 8  */
	CmdRemoveSignals,							/* 9  */
	CmdTerraformLand,							/* 10 */
	CmdPurchaseLandArea,					/* 11 */
	CmdSellLandArea,							/* 12 */
	CmdBuildTunnel,								/* 13 */
	CmdRemoveFromRailroadStation,	/* 14 */
	CmdConvertRail,								/* 15 */
	CmdBuildTrainWaypoint,				/* 16 */
	CmdRenameWaypoint,						/* 17 */
	CmdRemoveTrainWaypoint,				/* 18 */
	CmdBuildTruckStation,					/* 19 */
	NULL,													/* 20 */
	CmdBuildBusStation,						/* 21 */
	NULL,													/* 22 */
	CmdBuildLongRoad,							/* 23 */
	CmdRemoveLongRoad,						/* 24 */
	CmdBuildRoad,									/* 25 */
	CmdRemoveRoad,								/* 26 */
	CmdBuildRoadDepot,						/* 27 */
	NULL,													/* 28 */
	CmdBuildAirport,							/* 29 */
	CmdBuildDock,									/* 30 */
	CmdBuildShipDepot,						/* 31 */
	CmdBuildBuoy,									/* 32 */
	CmdPlantTree,									/* 33 */
	CmdBuildRailVehicle,					/* 34 */
	CmdMoveRailVehicle,						/* 35 */
	CmdStartStopTrain,						/* 36 */
	NULL,													/* 37 */
	CmdSellRailWagon,							/* 38 */
	CmdTrainGotoDepot,						/* 39 */
	CmdForceTrainProceed,					/* 40 */
	CmdReverseTrainDirection,			/* 41 */

	CmdModifyOrder,								/* 42 */
	CmdSkipOrder,									/* 43 */
	CmdDeleteOrder,								/* 44 */
	CmdInsertOrder,								/* 45 */

	CmdChangeTrainServiceInt,			/* 46 */

	CmdBuildIndustry,							/* 47 */
	CmdBuildCompanyHQ,						/* 48 */
	CmdSetPlayerFace,							/* 49 */
	CmdSetPlayerColor,						/* 50 */

	CmdIncreaseLoan,							/* 51 */
	CmdDecreaseLoan,							/* 52 */

	CmdWantEnginePreview,					/* 53 */

	CmdNameVehicle,								/* 54 */
	CmdRenameEngine,							/* 55 */

	CmdChangeCompanyName,					/* 56 */
	CmdChangePresidentName,				/* 57 */

	CmdRenameStation,							/* 58 */

	CmdSellAircraft,							/* 59 */
	CmdStartStopAircraft,					/* 60 */
	CmdBuildAircraft,							/* 61 */
	CmdSendAircraftToHangar,			/* 62 */
	CmdChangeAircraftServiceInt,	/* 63 */
	CmdRefitAircraft,							/* 64 */

	CmdPlaceSign,									/* 65 */
	CmdRenameSign,								/* 66 */

	CmdBuildRoadVeh,							/* 67 */
	CmdStartStopRoadVeh,					/* 68 */
	CmdSellRoadVeh,								/* 69 */
	CmdSendRoadVehToDepot,				/* 70 */
	CmdTurnRoadVeh,								/* 71 */
	CmdChangeRoadVehServiceInt,		/* 72 */

	CmdPause,											/* 73 */

	CmdBuyShareInCompany,					/* 74 */
	CmdSellShareInCompany,				/* 75 */
	CmdBuyCompany,								/* 76 */

	CmdBuildTown,									/* 77 */
	NULL,													/* 78 */
	NULL,													/* 79 */
	CmdRenameTown,								/* 80 */
	CmdDoTownAction,							/* 81 */

	CmdSetRoadDriveSide,					/* 82 */
	CmdSetTownNameType,						/* 83 */
	NULL,													/* 84 */
	CmdChangeDifficultyLevel,			/* 85 */

	CmdStartStopShip,							/* 86 */
	CmdSellShip,									/* 87 */
	CmdBuildShip,									/* 88 */
	CmdSendShipToDepot,						/* 89 */
	CmdChangeShipServiceInt,			/* 90 */
	CmdRefitShip,									/* 91 */

	CmdStartNewGame,							/* 92 */
	CmdLoadGame,									/* 93 */
	CmdCreateScenario,						/* 94 */
	CmdSetSinglePlayer,						/* 95 */
	NULL,													/* 96 */
	CmdSetNewLandscapeType,				/* 97 */

	CmdGenRandomNewGame,					/* 98 */

	CmdCloneOrder,								/* 99 */

	CmdClearArea,									/* 100 */
	CmdResume,										/* 101 */

	CmdMoneyCheat,								/* 102 */
	CmdBuildCanal,								/* 103 */
	CmdPlayerCtrl,								/* 104 */

	CmdLevelLand,									/* 105 */

	CmdRefitRailVehicle,					/* 106 */
	CmdRestoreOrderIndex,					/* 107 */
	CmdBuildLock,									/* 108 */
	CmdStartScenario,							/* 109 */
	CmdBuildManySignals,					/* 110 */
	//CmdDestroyIndustry,						/* 109 */
	CmdDestroyCompanyHQ,					/* 111 */
};

int32 DoCommandByTile(TileIndex tile, uint32 p1, uint32 p2, uint32 flags, uint procc)
{
	return DoCommand(GET_TILE_X(tile)*16, GET_TILE_Y(tile)*16, p1, p2, flags, procc);
}


//extern void _stdcall Sleep(int s);

int32 DoCommand(int x, int y, uint32 p1, uint32 p2, uint32 flags, uint procc)
{
	int32 res;
	CommandProc *proc;

	proc = _command_proc_table[procc];

	if (_docommand_recursive == 0) {
		_error_message = INVALID_STRING_ID;
		// update last build coord of player
		if ( (x|y) != 0 && _current_player < MAX_PLAYERS) {
			DEREF_PLAYER(_current_player)->last_build_coordinate = TILE_FROM_XY(x,y);
		}
	}

	_docommand_recursive++;

	// only execute the test call if it's toplevel, or we're not execing.
	if (_docommand_recursive == 1 || !(flags & DC_EXEC) || (flags & DC_FORCETEST) ) {
		res = proc(x, y, flags&~DC_EXEC, p1, p2);
		if ((uint32)res >> 16 == 0x8000) {
			if (res & 0xFFFF) _error_message = res & 0xFFFF;
			goto error;
		}

		if (_docommand_recursive == 1) {
			if (!(flags&DC_QUERY_COST) && res != 0 && !CheckPlayerHasMoney(res))
				goto error;
		}

		if (!(flags & DC_EXEC)) {
			_docommand_recursive--;
			return res;
		}
	}

	// execute the command here.
	_yearly_expenses_type = 0;
	res = proc(x, y, flags, p1, p2);
	if ((uint32)res >> 16 == 0x8000) {
		if (res & 0xFFFF) _error_message = res & 0xFFFF;
error:
		_docommand_recursive--;
		return CMD_ERROR;
	}

	// if toplevel, subtract the money.
	if (--_docommand_recursive == 0) {
		SubtractMoneyFromPlayer(res);
	}

	return res;
}

int32 GetAvailableMoneyForCommand()
{
	uint pid = _current_player;
	if (pid >= MAX_PLAYERS) return 0x7FFFFFFF; // max int
	return DEREF_PLAYER(pid)->player_money;
}

// toplevel network safe docommand function for the current player. must not be called recursively.
// the callback is called when the command succeeded or failed.
bool DoCommandP(TileIndex tile, uint32 p1, uint32 p2, CommandCallback *callback, uint32 cmd)
{
	int32 res = 0,res2;
	CommandProc *proc;
	uint32 flags;
	bool notest;

	int x = GET_TILE_X(tile)*16;
	int y = GET_TILE_Y(tile)*16;

	assert(_docommand_recursive == 0);

	if (_networking && !(cmd & CMD_NET_INSTANT) && _pause) {
		// When the game is paused, and we are in a network game
		//  we do not allow any commands. This is because
		//  of some technical reasons
		ShowErrorMessage(-1, STR_MULTIPLAYER_PAUSED, x, y);
		_docommand_recursive = 0;
		return true;
	}

	_error_message = INVALID_STRING_ID;
	_error_message_2 = cmd >> 16;
	_additional_cash_required = 0;

	// spectator has no rights.
	if (_current_player == OWNER_SPECTATOR) {
		ShowErrorMessage(_error_message, _error_message_2, x, y);
		return false;
	}

	flags = 0;
	if (cmd & CMD_AUTO) flags |= DC_AUTO;
	if (cmd & CMD_NO_WATER) flags |= DC_NO_WATER;

	// get pointer to command handler
	assert((cmd & 0xFF) < lengthof(_command_proc_table));
	proc = _command_proc_table[cmd & 0xFF];

	// this command is a notest command?
	// CMD_REMOVE_ROAD: This command has special local authority
	// restrictions which may cause the test run to fail (the previous
	// road fragments still stay there and the town won't let you
	// disconnect the road system), but the exec will succeed and this
	// fact will trigger an assertion failure. --pasky
	notest =
		(cmd & 0xFF) == CMD_CLEAR_AREA ||
		(cmd & 0xFF) == CMD_CONVERT_RAIL ||
		(cmd & 0xFF) == CMD_LEVEL_LAND ||
		(cmd & 0xFF) == CMD_TRAIN_GOTO_DEPOT ||
		(cmd & 0xFF) == CMD_REMOVE_ROAD;

	if (_networking && (cmd & CMD_ASYNC)) notest = true;

	_docommand_recursive = 1;

	// cost estimation only?
	if (_shift_pressed && _current_player == _local_player && !(cmd & CMD_DONT_NETWORK)) {
		// estimate the cost.
		res = proc(x, y, flags, p1, p2);
		if ((uint32)res >> 16 == 0x8000) {
			if (res & 0xFFFF) _error_message = res & 0xFFFF;
			ShowErrorMessage(_error_message, _error_message_2, x, y);
		} else {
			ShowEstimatedCostOrIncome(res, x, y);
		}

		_docommand_recursive = 0;
		return false;
	}



	// unless the command is a notest command, check if it can be executed.
	if (!notest) {
		// first test if the command can be executed.
		res = proc(x,y, flags, p1, p2);
		if ((uint32)res >> 16 == 0x8000) {
			if (res & 0xFFFF) _error_message = res & 0xFFFF;
			goto show_error;
		}
		// no money?
		if (res != 0 && !CheckPlayerHasMoney(res)) goto show_error;
	}

	// put the command in a network queue and execute it later?
	if (_networking && !(cmd & CMD_DONT_NETWORK)) {
		if (!(cmd & CMD_NET_INSTANT)) {
			NetworkSendCommand(tile, p1, p2, cmd, callback);
			_docommand_recursive = 0;
			return true;
		} else {
			// Instant Command ... Relay and Process then
			NetworkSendCommand(tile, p1, p2, cmd, callback);
		}
	}

	// update last build coordinate of player.
	if ( tile != 0 && _current_player < MAX_PLAYERS) DEREF_PLAYER(_current_player)->last_build_coordinate = tile;

	// actually try and execute the command.
	_yearly_expenses_type = 0;
	res2 = proc(x,y, flags|DC_EXEC, p1, p2);

	if (!notest) {
		assert(res == res2); // sanity check
	} else {
		if ((uint32)res2 >> 16 == 0x8000) {
			if (res2 & 0xFFFF) _error_message = res2 & 0xFFFF;
			goto show_error;
		}
	}

	SubtractMoneyFromPlayer(res2);

	if (_current_player == _local_player && _game_mode != GM_EDITOR) {
		if (res2 != 0)
			ShowCostOrIncomeAnimation(x, y, GetSlopeZ(x, y), res2);
		if (_additional_cash_required) {
			SetDParam(0, _additional_cash_required);
			ShowErrorMessage(STR_0003_NOT_ENOUGH_CASH_REQUIRES, _error_message_2, x,y);
			if (res2 == 0) goto callb_err;
		}
	}

	_docommand_recursive = 0;

	if (callback) callback(true, tile, p1, p2);
	return true;

show_error:
	// show error message if the command fails?
	if (_current_player == _local_player && _error_message_2 != 0)
		ShowErrorMessage(_error_message, _error_message_2, x,y);

callb_err:
	_docommand_recursive = 0;

	if (callback) callback(false, tile, p1, p2);
	return false;
}
