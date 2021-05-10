/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_waypointlist.hpp List all the waypoints (you own). */

#ifndef SCRIPT_WAYPOINTLIST_HPP
#define SCRIPT_WAYPOINTLIST_HPP

#include "script_list.hpp"
#include "script_waypoint.hpp"

/**
 * Creates a list of waypoints of which you are the owner.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptWaypointList : public ScriptList {
public:
	/**
	 * @param waypoint_type The type of waypoint to make a list of waypoints for.
	 */
	ScriptWaypointList(ScriptWaypoint::WaypointType waypoint_type);
};

/**
 * Creates a list of waypoints which the vehicle has in its orders.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptWaypointList_Vehicle : public ScriptList {
public:
	/**
	 * @param vehicle_id The vehicle to get the list of waypoints he has in its orders from.
	 */
	ScriptWaypointList_Vehicle(VehicleID vehicle_id);
};

#endif /* SCRIPT_WAYPOINTLIST_HPP */
