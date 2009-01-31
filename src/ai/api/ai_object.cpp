/* $Id$ */

/** @file ai_object.cpp Implementation of AIObject. */

#include "ai_log.hpp"
#include "table/strings.h"
#include "../ai.hpp"
#include "../ai_storage.hpp"
#include "../ai_instance.hpp"

static AIStorage *GetStorage()
{
	return AIInstance::GetStorage();
}

void AIObject::SetDoCommandDelay(uint ticks)
{
	assert(ticks > 0);
	GetStorage()->delay = ticks;
}

uint AIObject::GetDoCommandDelay()
{
	return GetStorage()->delay;
}

void AIObject::SetDoCommandMode(AIModeProc *proc, AIObject *instance)
{
	GetStorage()->mode = proc;
	GetStorage()->mode_instance = instance;
}

AIModeProc *AIObject::GetDoCommandMode()
{
	return GetStorage()->mode;
}

AIObject *AIObject::GetDoCommandModeInstance()
{
	return GetStorage()->mode_instance;
}

void AIObject::SetDoCommandCosts(Money value)
{
	GetStorage()->costs = CommandCost(value);
}

void AIObject::IncreaseDoCommandCosts(Money value)
{
	GetStorage()->costs.AddCost(value);
}

Money AIObject::GetDoCommandCosts()
{
	return GetStorage()->costs.GetCost();
}

void AIObject::SetLastError(AIErrorType last_error)
{
	GetStorage()->last_error = last_error;
}

AIErrorType AIObject::GetLastError()
{
	return GetStorage()->last_error;
}

void AIObject::SetLastCost(Money last_cost)
{
	GetStorage()->last_cost = last_cost;
}

Money AIObject::GetLastCost()
{
	return GetStorage()->last_cost;
}

void AIObject::SetRoadType(RoadType road_type)
{
	GetStorage()->road_type = road_type;
}

RoadType AIObject::GetRoadType()
{
	return GetStorage()->road_type;
}

void AIObject::SetRailType(RailType rail_type)
{
	GetStorage()->rail_type = rail_type;
}

RailType AIObject::GetRailType()
{
	return GetStorage()->rail_type;
}

void AIObject::SetLastCommandRes(bool res)
{
	GetStorage()->last_command_res = res;
	/* Also store the results of various global variables */
	SetNewVehicleID(_new_vehicle_id);
	SetNewSignID(_new_sign_id);
	SetNewTunnelEndtile(_build_tunnel_endtile);
	SetNewGroupID(_new_group_id);
}

bool AIObject::GetLastCommandRes()
{
	return GetStorage()->last_command_res;
}

void AIObject::SetNewVehicleID(VehicleID vehicle_id)
{
	GetStorage()->new_vehicle_id = vehicle_id;
}

VehicleID AIObject::GetNewVehicleID()
{
	return GetStorage()->new_vehicle_id;
}

void AIObject::SetNewSignID(SignID sign_id)
{
	GetStorage()->new_sign_id = sign_id;
}

SignID AIObject::GetNewSignID()
{
	return GetStorage()->new_sign_id;
}

void AIObject::SetNewTunnelEndtile(TileIndex tile)
{
	GetStorage()->new_tunnel_endtile = tile;
}

TileIndex AIObject::GetNewTunnelEndtile()
{
	return GetStorage()->new_tunnel_endtile;
}

void AIObject::SetNewGroupID(GroupID group_id)
{
	GetStorage()->new_group_id = group_id;
}

GroupID AIObject::GetNewGroupID()
{
	return GetStorage()->new_group_id;
}

void AIObject::SetAllowDoCommand(bool allow)
{
	GetStorage()->allow_do_command = allow;
}

bool AIObject::GetAllowDoCommand()
{
	return GetStorage()->allow_do_command;
}

void *&AIObject::GetEventPointer()
{
	return GetStorage()->event_data;
}

void *&AIObject::GetLogPointer()
{
	return GetStorage()->log_data;
}

void AIObject::SetCallbackVariable(int index, int value)
{
	if ((size_t)index >= GetStorage()->callback_value.size()) GetStorage()->callback_value.resize(index + 1);
	GetStorage()->callback_value[index] = value;
}

int AIObject::GetCallbackVariable(int index)
{
	return GetStorage()->callback_value[index];
}

bool AIObject::DoCommand(TileIndex tile, uint32 p1, uint32 p2, uint cmd, const char *text, AISuspendCallbackProc *callback)
{
	if (AIObject::GetAllowDoCommand() == false) {
		AILog::Error("You are not allowed to execute any DoCommand (even indirect) in your constructor, Save(), Load(), and any valuator.\n");
		return false;
	}

	CommandCost res;

	/* Set the default callback to return a true/false result of the DoCommand */
	if (callback == NULL) callback = &AIInstance::DoCommandReturn;

	/* Make sure the last error is reset, so we don't give faulty warnings */
	SetLastError(AIError::ERR_NONE);

	/* First, do a test-run to see if we can do this */
	res = ::DoCommand(tile, p1, p2, CommandFlagsToDCFlags(GetCommandFlags(cmd)), cmd, text);
	/* The command failed, so return */
	if (::CmdFailed(res)) {
		SetLastError(AIError::StringToError(_error_message));
		return false;
	}

	/* Check what the callback wants us to do */
	if (GetDoCommandMode() != NULL && !GetDoCommandMode()(tile, p1, p2, cmd, res)) {
		IncreaseDoCommandCosts(res.GetCost());
		return true;
	}

#ifdef ENABLE_NETWORK
	/* Send the command */
	if (_networking) {
		/* NetworkSend_Command needs _local_company to be set correctly, so
		 * adjust it, and put it back right after the function */
		CompanyID old_company = _local_company;
		_local_company = _current_company;
		::NetworkSend_Command(tile, p1, p2, cmd, CcAI, text);
		_local_company = old_company;
		SetLastCost(res.GetCost());

		/* Suspend the AI till the command is really executed */
		throw AI_VMSuspend(-(int)GetDoCommandDelay(), callback);
	} else {
#else
	{
#endif
		/* For SinglePlayer we execute the command immediatly */
		if (!::DoCommandP(tile, p1, p2, cmd, NULL, text)) res = CMD_ERROR;
		SetLastCommandRes(!::CmdFailed(res));

		if (::CmdFailed(res)) {
			SetLastError(AIError::StringToError(_error_message));
			return false;
		}
		SetLastCost(res.GetCost());
		IncreaseDoCommandCosts(res.GetCost());

		/* Suspend the AI player for 1+ ticks, so it simulates multiplayer. This
		 *  both avoids confusion when a developer launched his AI in a
		 *  multiplayer game, but also gives time for the GUI and human player
		 *  to interact with the game. */
		throw AI_VMSuspend(GetDoCommandDelay(), callback);
	}

	NOT_REACHED();
}
