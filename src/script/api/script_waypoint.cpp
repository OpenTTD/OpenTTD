/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file script_waypoint.cpp Implementation of ScriptWaypoint. */

#include "../../stdafx.h"
#include "script_waypoint.hpp"
#include "script_rail.hpp"
#include "script_marine.hpp"
#include "../../waypoint_base.h"

#include "../../safeguards.h"

/* static */ bool ScriptWaypoint::IsValidWaypoint(StationID waypoint_id)
{
	EnforceDeityOrCompanyModeValid(false);
	const Waypoint *wp = ::Waypoint::GetIfValid(waypoint_id);
	return wp != nullptr && (wp->owner == ScriptObject::GetCompany() || ScriptCompanyMode::IsDeity() || wp->owner == OWNER_NONE);
}

/* static */ StationID ScriptWaypoint::GetWaypointID(TileIndex tile)
{
	if (!::IsValidTile(tile) || !::IsTileType(tile, MP_STATION) || ::Waypoint::GetByTile(tile) == nullptr) return StationID::Invalid();
	return ::GetStationIndex(tile);
}

/* static */ bool ScriptWaypoint::HasWaypointType(StationID waypoint_id, WaypointType waypoint_type)
{
	if (!IsValidWaypoint(waypoint_id)) return false;
	if (!HasExactlyOneBit(waypoint_type)) return false;

	return ::Waypoint::Get(waypoint_id)->facilities.Any(static_cast<StationFacilities>(waypoint_type));
}
