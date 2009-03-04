/* $Id$ */

/** @file ai_vehiclelist.hpp List all the vehicles (you own). */

#ifndef AI_VEHICLELIST_HPP
#define AI_VEHICLELIST_HPP

#include "ai_abstractlist.hpp"
#include "ai_vehicle.hpp"

/**
 * Creates a list of vehicles of which you are the owner.
 * @ingroup AIList
 */
class AIVehicleList : public AIAbstractList {
public:
	static const char *GetClassName() { return "AIVehicleList"; }
	AIVehicleList();
};

/**
 * Creates a list of vehicles that have orders to a given station.
 * @ingroup AIList
 */
class AIVehicleList_Station : public AIAbstractList {
public:
	static const char *GetClassName() { return "AIVehicleList_Station"; }

	/**
	 * @param station_id The station to get the list of vehicles that have orders to him from.
	 */
	AIVehicleList_Station(StationID station_id);
};

/**
 * Creates a list of vehicles that share orders.
 * @ingroup AIList
 */
class AIVehicleList_SharedOrders : public AIAbstractList {
public:
	static const char *GetClassName() { return "AIVehicleList_SharedOrders"; }

	/**
	 * @param station_id The vehicle that the rest shared orders with.
	 */
	AIVehicleList_SharedOrders(VehicleID vehicle_id);
};

/**
 * Creates a list of vehicles that are in a group.
 * @ingroup AIList
 */
class AIVehicleList_Group : public AIAbstractList {
public:
	static const char *GetClassName() { return "AIVehicleList_Group"; }

	/**
	 * @param group_id The ID of the group the vehicles are in.
	 */
	AIVehicleList_Group(GroupID group_id);
};

/**
 * Creates a list of vehicles that are in the default group.
 * @ingroup AIList
 */
class AIVehicleList_DefaultGroup : public AIAbstractList {
public:
	static const char *GetClassName() { return "AIVehicleList_DefaultGroup"; }

	/**
	 * @param vehicle_type The VehicleType to get the list of vehicles for.
	 */
	AIVehicleList_DefaultGroup(AIVehicle::VehicleType vehicle_type);
};

#endif /* AI_VEHICLELIST_HPP */
