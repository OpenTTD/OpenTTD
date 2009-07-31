/* $Id$ */

/** @file ai_waypoint.hpp Everything to query and build waypoints. */

#ifndef AI_WAYPOINT_HPP
#define AI_WAYPOINT_HPP

#include "ai_object.hpp"
#include "ai_error.hpp"
#include "ai_basestation.hpp"

/**
 * Class that handles all waypoint related functions.
 */
class AIWaypoint : public AIBaseStation {
public:
	static const char *GetClassName() { return "AIWaypoint"; }

	/**
	 * Checks whether the given waypoint is valid and owned by you.
	 * @param waypoint_id The waypoint to check.
	 * @return True if and only if the waypoint is valid.
	 */
	static bool IsValidWaypoint(StationID waypoint_id);

	/**
	 * Get the StationID of a tile.
	 * @param tile The tile to find the StationID of.
	 * @pre AIRail::IsRailWaypointTile(tile).
	 * @return StationID of the waypoint.
	 */
	static StationID GetWaypointID(TileIndex tile);
};

#endif /* AI_WAYPOINT_HPP */
