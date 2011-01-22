/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_object.cpp Implementation of AIObject. */

#include "../../stdafx.h"
#include "../../script/squirrel.hpp"
#include "../../command_func.h"
#include "../../company_base.h"
#include "../../company_func.h"
#include "../../network/network.h"
#include "../../tunnelbridge.h"

#include "../ai_storage.hpp"
#include "../ai_instance.hpp"
#include "ai_error.hpp"

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

bool AIObject::CanSuspend()
{
	Squirrel *squirrel = Company::Get(_current_company)->ai_instance->engine;
	return GetStorage()->allow_do_command && squirrel->CanSuspend();
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
	if (!AIObject::CanSuspend()) {
		throw AI_FatalError("You are not allowed to execute any DoCommand (even indirect) in your constructor, Save(), Load(), and any valuator.");
	}

	/* Set the default callback to return a true/false result of the DoCommand */
	if (callback == NULL) callback = &AIInstance::DoCommandReturn;

	/* Are we only interested in the estimate costs? */
	bool estimate_only = GetDoCommandMode() != NULL && !GetDoCommandMode()();

#ifdef ENABLE_NETWORK
	/* Only set p2 when the command does not come from the network. */
	if (GetCommandFlags(cmd) & CMD_CLIENT_ID && p2 == 0) p2 = UINT32_MAX;
#endif

	/* Try to perform the command. */
	CommandCost res = ::DoCommandPInternal(tile, p1, p2, cmd, _networking ? CcAI : NULL, text, false, estimate_only);

	/* We failed; set the error and bail out */
	if (res.Failed()) {
		SetLastError(AIError::StringToError(res.GetErrorMessage()));
		return false;
	}

	/* No error, then clear it. */
	SetLastError(AIError::ERR_NONE);

	/* Estimates, update the cost for the estimate and be done */
	if (estimate_only) {
		IncreaseDoCommandCosts(res.GetCost());
		return true;
	}

	/* Costs of this operation. */
	SetLastCost(res.GetCost());
	SetLastCommandRes(true);

	if (_networking) {
		/* Suspend the AI till the command is really executed. */
		throw AI_VMSuspend(-(int)GetDoCommandDelay(), callback);
	} else {
		IncreaseDoCommandCosts(res.GetCost());

		/* Suspend the AI player for 1+ ticks, so it simulates multiplayer. This
		 *  both avoids confusion when a developer launched his AI in a
		 *  multiplayer game, but also gives time for the GUI and human player
		 *  to interact with the game. */
		throw AI_VMSuspend(GetDoCommandDelay(), callback);
	}

	NOT_REACHED();
}
