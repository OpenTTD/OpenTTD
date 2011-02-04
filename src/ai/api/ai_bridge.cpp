/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_bridge.cpp Implementation of AIBridge. */

#include "../../stdafx.h"
#include "ai_bridge.hpp"
#include "ai_rail.hpp"
#include "../ai_instance.hpp"
#include "../../bridge_map.h"
#include "../../strings_func.h"
#include "../../economy_func.h"
#include "../../date_func.h"

/* static */ bool AIBridge::IsValidBridge(BridgeID bridge_id)
{
	return bridge_id < MAX_BRIDGES && ::GetBridgeSpec(bridge_id)->avail_year <= _cur_year;
}

/* static */ bool AIBridge::IsBridgeTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;
	return ::IsBridgeTile(tile);
}

/* static */ BridgeID AIBridge::GetBridgeID(TileIndex tile)
{
	if (!IsBridgeTile(tile)) return (BridgeID)-1;
	return (BridgeID)::GetBridgeType(tile);
}

static void _DoCommandReturnBuildBridge2(class AIInstance *instance)
{
	if (!AIBridge::_BuildBridgeRoad2()) {
		AIInstance::DoCommandReturn(instance);
		return;
	}

	/* This can never happen, as in test-mode this callback is never executed,
	 *  and in execute-mode, the other callback is called. */
	NOT_REACHED();
}

static void _DoCommandReturnBuildBridge1(class AIInstance *instance)
{
	if (!AIBridge::_BuildBridgeRoad1()) {
		AIInstance::DoCommandReturn(instance);
		return;
	}

	/* This can never happen, as in test-mode this callback is never executed,
	 *  and in execute-mode, the other callback is called. */
	NOT_REACHED();
}

/* static */ bool AIBridge::BuildBridge(AIVehicle::VehicleType vehicle_type, BridgeID bridge_id, TileIndex start, TileIndex end)
{
	EnforcePrecondition(false, start != end);
	EnforcePrecondition(false, ::IsValidTile(start) && ::IsValidTile(end));
	EnforcePrecondition(false, TileX(start) == TileX(end) || TileY(start) == TileY(end));
	EnforcePrecondition(false, vehicle_type == AIVehicle::VT_ROAD || vehicle_type == AIVehicle::VT_RAIL || vehicle_type == AIVehicle::VT_WATER);
	EnforcePrecondition(false, vehicle_type != AIVehicle::VT_RAIL || AIRail::IsRailTypeAvailable(AIRail::GetCurrentRailType()));

	uint type = 0;
	switch (vehicle_type) {
		case AIVehicle::VT_ROAD:
			type |= (TRANSPORT_ROAD << 15);
			type |= (::RoadTypeToRoadTypes((::RoadType)AIObject::GetRoadType()) << 8);
			break;
		case AIVehicle::VT_RAIL:
			type |= (TRANSPORT_RAIL << 15);
			type |= (AIRail::GetCurrentRailType() << 8);
			break;
		case AIVehicle::VT_WATER:
			type |= (TRANSPORT_WATER << 15);
			break;
		default: NOT_REACHED();
	}

	/* For rail and water we do nothing special */
	if (vehicle_type == AIVehicle::VT_RAIL || vehicle_type == AIVehicle::VT_WATER) {
		return AIObject::DoCommand(end, start, type | bridge_id, CMD_BUILD_BRIDGE);
	}

	AIObject::SetCallbackVariable(0, start);
	AIObject::SetCallbackVariable(1, end);
	return AIObject::DoCommand(end, start, type | bridge_id, CMD_BUILD_BRIDGE, NULL, &_DoCommandReturnBuildBridge1);
}

/* static */ bool AIBridge::_BuildBridgeRoad1()
{
	/* Build the piece of road on the 'start' side of the bridge */
	TileIndex end = AIObject::GetCallbackVariable(0);
	TileIndex start = AIObject::GetCallbackVariable(1);

	DiagDirection dir_1 = ::DiagdirBetweenTiles(end, start);
	DiagDirection dir_2 = ::ReverseDiagDir(dir_1);

	return AIObject::DoCommand(start + ::TileOffsByDiagDir(dir_1), ::DiagDirToRoadBits(dir_2) | (AIObject::GetRoadType() << 4), 0, CMD_BUILD_ROAD, NULL, &_DoCommandReturnBuildBridge2);
}

/* static */ bool AIBridge::_BuildBridgeRoad2()
{
	/* Build the piece of road on the 'end' side of the bridge */
	TileIndex end = AIObject::GetCallbackVariable(0);
	TileIndex start = AIObject::GetCallbackVariable(1);

	DiagDirection dir_1 = ::DiagdirBetweenTiles(end, start);
	DiagDirection dir_2 = ::ReverseDiagDir(dir_1);

	return AIObject::DoCommand(end + ::TileOffsByDiagDir(dir_2), ::DiagDirToRoadBits(dir_1) | (AIObject::GetRoadType() << 4), 0, CMD_BUILD_ROAD);
}

/* static */ bool AIBridge::RemoveBridge(TileIndex tile)
{
	EnforcePrecondition(false, IsBridgeTile(tile));
	return AIObject::DoCommand(tile, 0, 0, CMD_LANDSCAPE_CLEAR);
}

/* static */ char *AIBridge::GetName(BridgeID bridge_id)
{
	if (!IsValidBridge(bridge_id)) return NULL;

	static const int len = 64;
	char *bridge_name = MallocT<char>(len);

	::GetString(bridge_name, ::GetBridgeSpec(bridge_id)->transport_name[0], &bridge_name[len - 1]);
	return bridge_name;
}

/* static */ int32 AIBridge::GetMaxSpeed(BridgeID bridge_id)
{
	if (!IsValidBridge(bridge_id)) return -1;

	return ::GetBridgeSpec(bridge_id)->speed; // km-ish/h
}

/* static */ Money AIBridge::GetPrice(BridgeID bridge_id, uint length)
{
	if (!IsValidBridge(bridge_id)) return -1;

	return ::CalcBridgeLenCostFactor(length) * _price[PR_BUILD_BRIDGE] * ::GetBridgeSpec(bridge_id)->price >> 8;
}

/* static */ int32 AIBridge::GetMaxLength(BridgeID bridge_id)
{
	if (!IsValidBridge(bridge_id)) return -1;

	return min(::GetBridgeSpec(bridge_id)->max_length, _settings_game.construction.max_bridge_length) + 2;
}

/* static */ int32 AIBridge::GetMinLength(BridgeID bridge_id)
{
	if (!IsValidBridge(bridge_id)) return -1;

	return ::GetBridgeSpec(bridge_id)->min_length + 2;
}

/* static */ TileIndex AIBridge::GetOtherBridgeEnd(TileIndex tile)
{
	if (!::IsValidTile(tile)) return INVALID_TILE;
	if (!IsBridgeTile(tile)) return INVALID_TILE;

	return ::GetOtherBridgeEnd(tile);
}
