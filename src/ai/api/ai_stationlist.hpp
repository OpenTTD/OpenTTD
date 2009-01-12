/* $Id$ */

/** @file ai_stationlist.hpp List all the stations (you own). */

#ifndef AI_STATIONLIST_HPP
#define AI_STATIONLIST_HPP

#include "ai_abstractlist.hpp"
#include "ai_station.hpp"

/**
 * Creates a list of stations of which you are the owner.
 * @ingroup AIList
 */
class AIStationList : public AIAbstractList {
public:
	static const char *GetClassName() { return "AIStationList"; }

	/**
	 * @param station_type The type of station to make a list of stations for.
	 */
	AIStationList(AIStation::StationType station_type);
};

/**
 * Creates a list of stations which the vehicle has in its orders.
 * @ingroup AIList
 */
class AIStationList_Vehicle : public AIAbstractList {
public:
	static const char *GetClassName() { return "AIStationList_Vehicle"; }

	/**
	 * @param vehicle_id The vehicle to get the list of stations he has in its orders from.
	 */
	AIStationList_Vehicle(VehicleID vehicle_id);
};

#endif /* AI_STATIONLIST_HPP */
