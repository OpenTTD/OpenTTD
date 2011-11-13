/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_waypointlist.hpp List all the waypoints (you own). */

#ifndef AI_WAYPOINTLIST_HPP
#define AI_WAYPOINTLIST_HPP

#include "ai_list.hpp"
#include "ai_waypoint.hpp"

/**
 * Creates a list of waypoints of which you are the owner.
 * @ingroup AIList
 */
class AIWaypointList : public AIList {
public:
	/**
	 * @param waypoint_type The type of waypoint to make a list of waypoints for.
	 */
	AIWaypointList(AIWaypoint::WaypointType waypoint_type);
};

/**
 * Creates a list of waypoints which the vehicle has in its orders.
 * @ingroup AIList
 */
class AIWaypointList_Vehicle : public AIList {
public:
	/**
	 * @param vehicle_id The vehicle to get the list of waypoints he has in its orders from.
	 */
	AIWaypointList_Vehicle(VehicleID vehicle_id);
};

#endif /* AI_WAYPOINTLIST_HPP */
