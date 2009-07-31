/* $Id$ */

/** @file ai_waypoint.cpp Implementation of AIWaypoint. */

#include "ai_waypoint.hpp"
#include "ai_rail.hpp"
#include "../../command_func.h"
#include "../../string_func.h"
#include "../../strings_func.h"
#include "../../company_func.h"
#include "../../waypoint_base.h"
#include "../../core/alloc_func.hpp"
#include "table/strings.h"

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
