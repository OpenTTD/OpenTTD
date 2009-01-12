/* $Id$ */

/** @file ai_enginelist.hpp List all the engines. */

#ifndef AI_ENGINELIST_HPP
#define AI_ENGINELIST_HPP

#include "ai_abstractlist.hpp"
#include "ai_vehicle.hpp"

/**
 * Create a list of engines based on a vehicle type.
 * @ingroup AIList
 */
class AIEngineList : public AIAbstractList {
public:
	static const char *GetClassName() { return "AIEngineList"; }

	/**
	 * @param vehicle_type The type of vehicle to make a list of engines for.
	 */
	AIEngineList(AIVehicle::VehicleType vehicle_type);
};

#endif /* AI_ENGINELIST_HPP */
