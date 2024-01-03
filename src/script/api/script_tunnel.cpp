/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_tunnel.cpp Implementation of ScriptTunnel. */

#include "../../stdafx.h"
#include "script_tunnel.hpp"
#include "script_rail.hpp"
#include "../script_instance.hpp"
#include "../../tunnel_map.h"
#include "../../landscape_cmd.h"
#include "../../road_cmd.h"
#include "../../tunnelbridge_cmd.h"

#include "../../safeguards.h"

/* static */ bool ScriptTunnel::IsTunnelTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;
	return ::IsTunnelTile(tile);
}

/* static */ TileIndex ScriptTunnel::GetOtherTunnelEnd(TileIndex tile)
{
	if (!::IsValidTile(tile)) return INVALID_TILE;

	/* If it's a tunnel already, take the easy way out! */
	if (IsTunnelTile(tile)) return ::GetOtherTunnelEnd(tile);

	int start_z;
	Slope start_tileh = ::GetTileSlope(tile, &start_z);
	DiagDirection direction = ::GetInclinedSlopeDirection(start_tileh);
	if (direction == INVALID_DIAGDIR) return INVALID_TILE;

	TileIndexDiff delta = ::TileOffsByDiagDir(direction);
	int end_z;
	do {
		tile += delta;
		if (!::IsValidTile(tile)) return INVALID_TILE;

		::GetTileSlope(tile, &end_z);
	} while (start_z != end_z);

	return tile;
}

/**
 * Helper function to connect a just built tunnel to nearby roads.
 * @param instance The script instance we have to built the road for.
 */
static void _DoCommandReturnBuildTunnel2(class ScriptInstance *instance)
{
	if (!ScriptTunnel::_BuildTunnelRoad2()) {
		ScriptInstance::DoCommandReturn(instance);
		return;
	}

	/* This can never happen, as in test-mode this callback is never executed,
	 *  and in execute-mode, the other callback is called. */
	NOT_REACHED();
}

/**
 * Helper function to connect a just built tunnel to nearby roads.
 * @param instance The script instance we have to built the road for.
 */
static void _DoCommandReturnBuildTunnel1(class ScriptInstance *instance)
{
	if (!ScriptTunnel::_BuildTunnelRoad1()) {
		ScriptInstance::DoCommandReturn(instance);
		return;
	}

	/* This can never happen, as in test-mode this callback is never executed,
	 *  and in execute-mode, the other callback is called. */
	NOT_REACHED();
}

/* static */ bool ScriptTunnel::BuildTunnel(ScriptVehicle::VehicleType vehicle_type, TileIndex start)
{
	EnforceDeityOrCompanyModeValid(false);
	EnforcePrecondition(false, ::IsValidTile(start));
	EnforcePrecondition(false, vehicle_type == ScriptVehicle::VT_RAIL || vehicle_type == ScriptVehicle::VT_ROAD);
	EnforcePrecondition(false, vehicle_type != ScriptVehicle::VT_RAIL || ScriptRail::IsRailTypeAvailable(ScriptRail::GetCurrentRailType()));
	EnforcePrecondition(false, vehicle_type != ScriptVehicle::VT_ROAD || ScriptRoad::IsRoadTypeAvailable(ScriptRoad::GetCurrentRoadType()));
	EnforcePrecondition(false, ScriptCompanyMode::IsValid() || vehicle_type == ScriptVehicle::VT_ROAD);

	if (vehicle_type == ScriptVehicle::VT_RAIL) {
		/* For rail we do nothing special */
		return ScriptObject::Command<CMD_BUILD_TUNNEL>::Do(start, TRANSPORT_RAIL, ScriptRail::GetCurrentRailType());
	} else {
		ScriptObject::SetCallbackVariable(0, start.base());
		return ScriptObject::Command<CMD_BUILD_TUNNEL>::Do(&::_DoCommandReturnBuildTunnel1, start, TRANSPORT_ROAD, ScriptRoad::GetCurrentRoadType());
	}
}

/* static */ bool ScriptTunnel::_BuildTunnelRoad1()
{
	EnforceDeityOrCompanyModeValid(false);

	/* Build the piece of road on the 'start' side of the tunnel */
	TileIndex end = ScriptObject::GetCallbackVariable(0);
	TileIndex start = ScriptTunnel::GetOtherTunnelEnd(end);

	DiagDirection dir_1 = ::DiagdirBetweenTiles(end, start);
	DiagDirection dir_2 = ::ReverseDiagDir(dir_1);

	return ScriptObject::Command<CMD_BUILD_ROAD>::Do(&::_DoCommandReturnBuildTunnel2, start + ::TileOffsByDiagDir(dir_1), ::DiagDirToRoadBits(dir_2), ScriptRoad::GetRoadType(), DRD_NONE, 0);
}

/* static */ bool ScriptTunnel::_BuildTunnelRoad2()
{
	EnforceDeityOrCompanyModeValid(false);

	/* Build the piece of road on the 'end' side of the tunnel */
	TileIndex end = ScriptObject::GetCallbackVariable(0);
	TileIndex start = ScriptTunnel::GetOtherTunnelEnd(end);

	DiagDirection dir_1 = ::DiagdirBetweenTiles(end, start);
	DiagDirection dir_2 = ::ReverseDiagDir(dir_1);

	return ScriptObject::Command<CMD_BUILD_ROAD>::Do(end + ::TileOffsByDiagDir(dir_2), ::DiagDirToRoadBits(dir_1), ScriptRoad::GetRoadType(), DRD_NONE, 0);
}

/* static */ bool ScriptTunnel::RemoveTunnel(TileIndex tile)
{
	EnforceCompanyModeValid(false);
	EnforcePrecondition(false, IsTunnelTile(tile));

	return ScriptObject::Command<CMD_LANDSCAPE_CLEAR>::Do(tile);
}
