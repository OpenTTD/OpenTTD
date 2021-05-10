/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_waypoint.hpp Everything to query and build waypoints. */

#ifndef SCRIPT_WAYPOINT_HPP
#define SCRIPT_WAYPOINT_HPP

#include "script_basestation.hpp"
#include "script_error.hpp"
#include "../../station_type.h"

/**
 * Class that handles all waypoint related functions.
 * @api ai game
 */
class ScriptWaypoint : public ScriptBaseStation {
public:
	/**
	 * All waypoint related error messages.
	 */
	enum ErrorMessages {
		/** Base for waypoint related errors */
		ERR_WAYPOINT_BASE = ScriptError::ERR_CAT_WAYPOINT << ScriptError::ERR_CAT_BIT_SIZE,

		/** The waypoint is build too close to another waypoint */
		ERR_WAYPOINT_TOO_CLOSE_TO_ANOTHER_WAYPOINT, // [STR_ERROR_TOO_CLOSE_TO_ANOTHER_WAYPOINT]

		/** The waypoint would join more than one existing waypoint together. */
		ERR_WAYPOINT_ADJOINS_MULTIPLE_WAYPOINTS,    // [STR_ERROR_WAYPOINT_ADJOINS_MORE_THAN_ONE_EXISTING]
	};

	/**
	 * Type of waypoints known in the game.
	 */
	enum WaypointType {
		/* Note: these values represent part of the in-game StationFacility enum */
		WAYPOINT_RAIL      = (int)::FACIL_TRAIN, ///< Rail waypoint
		WAYPOINT_BUOY      = (int)::FACIL_DOCK,  ///< Buoy
		WAYPOINT_ANY       = WAYPOINT_RAIL | WAYPOINT_BUOY, ///< All waypoint types
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
	 * @pre ScriptRail::IsRailWaypointTile(tile).
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

DECLARE_ENUM_AS_BIT_SET(ScriptWaypoint::WaypointType)

#endif /* SCRIPT_WAYPOINT_HPP */
