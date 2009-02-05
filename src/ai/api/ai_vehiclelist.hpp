/* $Id$ */

/** @file ai_vehiclelist.hpp List all the vehicles (you own). */

#ifndef AI_VEHICLELIST_HPP
#define AI_VEHICLELIST_HPP

#include "ai_abstractlist.hpp"

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

#endif /* AI_VEHICLELIST_HPP */
