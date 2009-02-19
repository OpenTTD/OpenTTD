/* $Id$ */

/** @file ai_waypoint.hpp Everything to query and build waypoints. */

#ifndef AI_WAYPOINT_HPP
#define AI_WAYPOINT_HPP

#include "ai_object.hpp"
#include "ai_error.hpp"

/**
 * Class that handles all waypoint related functions.
 */
class AIWaypoint : public AIObject {
public:
	static const char *GetClassName() { return "AIWaypoint"; }

	enum SpecialWaypointIDs {
		WAYPOINT_INVALID = -1, //!< An invalid WaypointID.
	};

	/**
	 * Checks whether the given waypoint is valid and owned by you.
	 * @param waypoint_id The waypoint to check.
	 * @return True if and only if the waypoint is valid.
	 */
	static bool IsValidWaypoint(WaypointID waypoint_id);

	/**
	 * Get the WaypointID of a tile.
	 * @param tile The tile to find the WaypointID of.
	 * @pre AIRail::IsRailWaypointTile(tile).
	 * @return WaypointID of the waypoint.
	 */
	static WaypointID GetWaypointID(TileIndex tile);

	/**
	 * Get the name of a waypoint.
	 * @param waypoint_id The waypoint to get the name of.
	 * @pre IsValidWaypoint(waypoint_id).
	 * @return The name of the waypoint.
	 */
	static char *GetName(WaypointID waypoint_id);

	/**
	 * Set the name this waypoint.
	 * @param waypoint_id The waypoint to set the name of.
	 * @param name The new name of the waypoint.
	 * @pre IsValidWaypointwaypoint_id).
	 * @pre 'name' must have at least one character.
	 * @pre 'name' must have at most 30 characters.
	 * @exception AIError::ERR_NAME_IS_NOT_UNIQUE
	 * @return True if the name was changed.
	 */
	static bool SetName(WaypointID waypoint_id, const char *name);

	/**
	 * Get the current location of a waypoint.
	 * @param waypoint_id The waypoint to get the location of.
	 * @pre IsValidWaypoint(waypoint_id).
	 * @return The tile the waypoint is currently on.
	 */
	static TileIndex GetLocation(WaypointID waypoint_id);
};

#endif /* AI_WAYPOINT_HPP */
