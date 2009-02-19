/* $Id$ */

/** @file ai_waypointlist.hpp List all the waypoints (you own). */

#ifndef AI_WAYPOINTLIST_HPP
#define AI_WAYPOINTLIST_HPP

#include "ai_abstractlist.hpp"

/**
 * Creates a list of waypoints of which you are the owner.
 * @ingroup AIList
 */
class AIWaypointList : public AIAbstractList {
public:
	static const char *GetClassName() { return "AIWaypointList"; }

	AIWaypointList();
};

/**
 * Creates a list of waypoints which the vehicle has in its orders.
 * @ingroup AIList
 */
class AIWaypointList_Vehicle : public AIAbstractList {
public:
	static const char *GetClassName() { return "AIWaypointList_Vehicle"; }

	/**
	 * @param vehicle_id The vehicle to get the list of waypoints he has in its orders from.
	 */
	AIWaypointList_Vehicle(VehicleID vehicle_id);
};

#endif /* AI_WAYPOINTLIST_HPP */
