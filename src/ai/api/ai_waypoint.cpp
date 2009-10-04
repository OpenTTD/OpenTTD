/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_waypoint.cpp Implementation of AIWaypoint. */

#include "ai_waypoint.hpp"
#include "ai_rail.hpp"
#include "../../company_func.h"
#include "../../waypoint_base.h"

/* static */ bool AIWaypoint::IsValidWaypoint(StationID waypoint_id)
{
	const Waypoint *wp = ::Waypoint::GetIfValid(waypoint_id);
	return wp != NULL && wp->owner == _current_company;
}

/* static */ StationID AIWaypoint::GetWaypointID(TileIndex tile)
{
	if (!AIRail::IsRailWaypointTile(tile)) return STATION_INVALID;

	return ::GetStationIndex(tile);
}
