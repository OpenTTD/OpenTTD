/* $Id$ */

/** @file ai_waypoint.cpp Implementation of AIWaypoint. */

#include "ai_waypoint.hpp"
#include "ai_rail.hpp"
#include "../../command_func.h"
#include "../../string_func.h"
#include "../../strings_func.h"
#include "../../company_func.h"
#include "../../waypoint.h"
#include "../../core/alloc_func.hpp"
#include "table/strings.h"

/* static */ bool AIWaypoint::IsValidWaypoint(WaypointID waypoint_id)
{
	return ::IsValidWaypointID(waypoint_id) && ::GetWaypoint(waypoint_id)->owner == _current_company;
}

/* static */ WaypointID AIWaypoint::GetWaypointID(TileIndex tile)
{
	if (!AIRail::IsRailWaypointTile(tile)) return WAYPOINT_INVALID;

	return ::GetWaypointIndex(tile);
}

/* static */ char *AIWaypoint::GetName(WaypointID waypoint_id)
{
	if (!IsValidWaypoint(waypoint_id)) return NULL;

	static const int len = 64;
	char *waypoint_name = MallocT<char>(len);

	::SetDParam(0, waypoint_id);
	::GetString(waypoint_name, STR_WAYPOINT_RAW, &waypoint_name[len - 1]);
	return waypoint_name;
}

/* static */ bool AIWaypoint::SetName(WaypointID waypoint_id, const char *name)
{
	EnforcePrecondition(false, IsValidWaypoint(waypoint_id));
	EnforcePrecondition(false, !::StrEmpty(name));
	EnforcePreconditionCustomError(false, ::strlen(name) < MAX_LENGTH_WAYPOINT_NAME_BYTES, AIError::ERR_PRECONDITION_STRING_TOO_LONG);

	return AIObject::DoCommand(0, waypoint_id, 0, CMD_RENAME_WAYPOINT, name);
}

/* static */ TileIndex AIWaypoint::GetLocation(WaypointID waypoint_id)
{
	if (!IsValidWaypoint(waypoint_id)) return INVALID_TILE;

	return ::GetWaypoint(waypoint_id)->xy;
}
