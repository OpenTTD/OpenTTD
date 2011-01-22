/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_waypoint.hpp Everything to query and build waypoints. */

#ifndef AI_WAYPOINT_HPP
#define AI_WAYPOINT_HPP

#include "ai_basestation.hpp"

/**
 * Class that handles all waypoint related functions.
 */
class AIWaypoint : public AIBaseStation {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIWaypoint"; }

	/**
	 * Type of waypoints known in the game.
	 */
	enum WaypointType {
		/* Values are important, as they represent the internal state of the game. */
		WAYPOINT_RAIL      = 0x01, ///< Rail waypoint
		WAYPOINT_BUOY      = 0x10, ///< Buoy
		WAYPOINT_ANY       = 0x11, ///< All waypoint types
	};

	/**
	 * All waypoint related error messages.
	 */
	enum ErrorMessages {
		/** Base for waypoint related errors */
		ERR_WAYPOINT_BASE = AIError::ERR_CAT_WAYPOINT << AIError::ERR_CAT_BIT_SIZE,

		/** The waypoint is build too close to another waypoint */
		ERR_WAYPOINT_TOO_CLOSE_TO_ANOTHER_WAYPOINT, // [STR_ERROR_TOO_CLOSE_TO_ANOTHER_WAYPOINT]

		/** The waypoint would join more than one existing waypoint together. */
		ERR_WAYPOINT_ADJOINS_MULTIPLE_WAYPOINTS,    // [STR_ERROR_WAYPOINT_ADJOINS_MORE_THAN_ONE_EXISTING]
	};

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

	/**
	 * Check if any part of the waypoint contains a waypoint of the type waypoint_type
	 * @param waypoint_id The waypoint to look at.
	 * @param waypoint_type The WaypointType to look for.
	 * @return True if the waypoint has a waypoint part of the type waypoint_type.
	 */
	static bool HasWaypointType(StationID waypoint_id, WaypointType waypoint_type);
};

DECLARE_ENUM_AS_BIT_SET(AIWaypoint::WaypointType)

#endif /* AI_WAYPOINT_HPP */
