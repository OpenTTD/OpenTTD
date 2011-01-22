/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_engine.hpp Everything to query and build engines. */

#ifndef AI_ENGINE_HPP
#define AI_ENGINE_HPP

#include "ai_vehicle.hpp"
#include "ai_rail.hpp"
#include "ai_airport.hpp"

/**
 * Class that handles all engine related functions.
 */
class AIEngine : public AIObject {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIEngine"; }

	/**
	 * Checks whether the given engine type is valid. An engine is valid if you
	 * have at least one vehicle of this engine or it's currently buildable.
	 * @param engine_id The engine to check.
	 * @return True if and only if the engine type is valid.
	 */
	static bool IsValidEngine(EngineID engine_id);

	/**
	 * Checks whether the given engine type is buildable by you.
	 * @param engine_id The engine to check.
	 * @return True if and only if the engine type is buildable.
	 */
	static bool IsBuildable(EngineID engine_id);

	/**
	 * Get the name of an engine.
	 * @param engine_id The engine to get the name of.
	 * @pre IsValidEngine(engine_id).
	 * @return The name the engine has.
	 */
	static char *GetName(EngineID engine_id);

	/**
	 * Get the cargo-type of an engine. In case it can transport multiple cargos, it
	 *  returns the first/main.
	 * @param engine_id The engine to get the cargo-type of.
	 * @pre IsValidEngine(engine_id).
	 * @return The cargo-type of the engine.
	 */
	static CargoID GetCargoType(EngineID engine_id);

	/**
	 * Check if the cargo of an engine can be refitted to your requested. If
	 *  the engine already allows this cargo, the function also returns true.
	 *  In case of articulated vehicles the function decides whether at least one
	 *  part can carry the cargo.
	 * @param engine_id The engine to check for refitting.
	 * @param cargo_id The cargo to check for refitting.
	 * @pre IsValidEngine(engine_id).
	 * @pre AICargo::IsValidCargo(cargo_id).
	 * @return True if the engine can carry this cargo, either via refit, or
	 *  by default.
	 */
	static bool CanRefitCargo(EngineID engine_id, CargoID cargo_id);

	/**
	 * Check if the engine can pull a wagon with the given cargo.
	 * @param engine_id The engine to check.
	 * @param cargo_id The cargo to check.
	 * @pre IsValidEngine(engine_id).
	 * @pre GetVehicleType(engine_id) == AIVehicle::VT_RAIL.
	 * @pre AICargo::IsValidCargo(cargo_id).
	 * @return True if the engine can pull wagons carrying this cargo.
	 * @note This function is not exhaustive; a true here does not mean
	 *  that the vehicle can pull the wagons, a false does mean it can't.
	 */
	static bool CanPullCargo(EngineID engine_id, CargoID cargo_id);

	/**
	 * Get the capacity of an engine. In case it can transport multiple cargos, it
	 *  returns the first/main.
	 * @param engine_id The engine to get the capacity of.
	 * @pre IsValidEngine(engine_id).
	 * @return The capacity of the engine.
	 */
	static int32 GetCapacity(EngineID engine_id);

	/**
	 * Get the reliability of an engine. The value is between 0 and 100, where
	 *  100 means 100% reliability (never breaks down) and 0 means 0%
	 *  reliability (you most likely don't want to buy it).
	 * @param engine_id The engine to get the reliability of.
	 * @pre IsValidEngine(engine_id).
	 * @pre GetVehicleType(engine_id) != AIVehicle::VT_TRAIN || !IsWagon(engine_id).
	 * @return The reliability the engine has.
	 */
	static int32 GetReliability(EngineID engine_id);

	/**
	 * Get the maximum speed of an engine.
	 * @param engine_id The engine to get the maximum speed of.
	 * @pre IsValidEngine(engine_id).
	 * @pre GetVehicleType(engine_id) != AIVehicle::VT_TRAIN || !IsWagon(engine_id).
	 * @return The maximum speed the engine has.
	 * @note The speed is in OpenTTD's internal speed unit.
	 *       This is mph / 1.6, which is roughly km/h.
	 *       To get km/h multiply this number by 1.00584.
	 */
	static int32 GetMaxSpeed(EngineID engine_id);

	/**
	 * Get the new cost of an engine.
	 * @param engine_id The engine to get the new cost of.
	 * @pre IsValidEngine(engine_id).
	 * @return The new cost the engine has.
	 */
	static Money GetPrice(EngineID engine_id);

	/**
	 * Get the maximum age of a brand new engine.
	 * @param engine_id The engine to get the maximum age of.
	 * @pre IsValidEngine(engine_id).
	 * @returns The maximum age of a new engine in days.
	 * @note Age is in days; divide by 366 to get per year.
	 */
	static int32 GetMaxAge(EngineID engine_id);

	/**
	 * Get the running cost of an engine.
	 * @param engine_id The engine to get the running cost of.
	 * @pre IsValidEngine(engine_id).
	 * @return The running cost of a vehicle per year.
	 * @note Cost is per year; divide by 365 to get per day.
	 */
	static Money GetRunningCost(EngineID engine_id);

	/**
	 * Get the power of an engine.
	 * @param engine_id The engine to get the power of.
	 * @pre IsValidEngine(engine_id).
	 * @pre (GetVehicleType(engine_id) == AIVehicle::VT_RAIL || GetVehicleType(engine_id) == AIVehicle::VT_ROAD) && !IsWagon(engine_id).
	 * @return The power of the engine in hp.
	 */
	static int32 GetPower(EngineID engine_id);

	/**
	 * Get the weight of an engine.
	 * @param engine_id The engine to get the weight of.
	 * @pre IsValidEngine(engine_id).
	 * @pre (GetVehicleType(engine_id) == AIVehicle::VT_RAIL || GetVehicleType(engine_id) == AIVehicle::VT_ROAD).
	 * @return The weight of the engine in metric tons.
	 */
	static int32 GetWeight(EngineID engine_id);

	/**
	 * Get the maximum tractive effort of an engine.
	 * @param engine_id The engine to get the maximum tractive effort of.
	 * @pre IsValidEngine(engine_id).
	 * @pre (GetVehicleType(engine_id) == AIVehicle::VT_RAIL || GetVehicleType(engine_id) == AIVehicle::VT_ROAD) && !IsWagon(engine_id).
	 * @return The maximum tractive effort of the engine in kN.
	 */
	static int32 GetMaxTractiveEffort(EngineID engine_id);

	/**
	 * Get the date this engine was designed.
	 * @param engine_id The engine to get the design date of.
	 * @pre IsValidEngine(engine_id).
	 * @return The date this engine was designed.
	 */
	static int32 GetDesignDate(EngineID engine_id);

	/**
	 * Get the type of an engine.
	 * @param engine_id The engine to get the type of.
	 * @pre IsValidEngine(engine_id).
	 * @return The type the engine has.
	 */
	static AIVehicle::VehicleType GetVehicleType(EngineID engine_id);

	/**
	 * Check if an engine is a wagon.
	 * @param engine_id The engine to check.
	 * @pre IsValidEngine(engine_id).
	 * @pre GetVehicleType(engine_id) == AIVehicle::VT_RAIL.
	 * @return Whether or not the engine is a wagon.
	 */
	static bool IsWagon(EngineID engine_id);

	/**
	 * Check if a train vehicle can run on a RailType.
	 * @param engine_id The engine to check.
	 * @param track_rail_type The type you want to check.
	 * @pre IsValidEngine(engine_id).
	 * @pre GetVehicleType(engine_id) == AIVehicle::VT_RAIL.
	 * @pre AIRail::IsRailTypeAvailable(track_rail_type).
	 * @return Whether an engine of type 'engine_id' can run on 'track_rail_type'.
	 * @note Even if a train can run on a RailType that doesn't mean that it'll be
	 *   able to power the train. Use HasPowerOnRail for that.
	 */
	static bool CanRunOnRail(EngineID engine_id, AIRail::RailType track_rail_type);

	/**
	 * Check if a train engine has power on a RailType.
	 * @param engine_id The engine to check.
	 * @param track_rail_type Another RailType.
	 * @pre IsValidEngine(engine_id).
	 * @pre GetVehicleType(engine_id) == AIVehicle::VT_RAIL.
	 * @pre AIRail::IsRailTypeAvailable(track_rail_type).
	 * @return Whether an engine of type 'engine_id' has power on 'track_rail_type'.
	 */
	static bool HasPowerOnRail(EngineID engine_id, AIRail::RailType track_rail_type);

	/**
	 * Get the RoadType of the engine.
	 * @param engine_id The engine to get the RoadType of.
	 * @pre IsValidEngine(engine_id).
	 * @pre GetVehicleType(engine_id) == AIVehicle::VT_ROAD.
	 * @return The RoadType the engine has.
	 */
	static AIRoad::RoadType GetRoadType(EngineID engine_id);

	/**
	 * Get the RailType of the engine.
	 * @param engine_id The engine to get the RailType of.
	 * @pre IsValidEngine(engine_id).
	 * @pre GetVehicleType(engine_id) == AIVehicle::VT_RAIL.
	 * @return The RailType the engine has.
	 */
	static AIRail::RailType GetRailType(EngineID engine_id);

	/**
	 * Check if the engine is articulated.
	 * @param engine_id The engine to check.
	 * @pre IsValidEngine(engine_id).
	 * @pre GetVehicleType(engine_id) == AIVehicle::VT_ROAD || GetVehicleType(engine_id) == AIVehicle::VT_RAIL.
	 * @return True if the engine is articulated.
	 */
	static bool IsArticulated(EngineID engine_id);

	/**
	 * Get the PlaneType of the engine.
	 * @param engine_id The engine to get the PlaneType of.
	 * @pre IsValidEngine(engine_id).
	 * @pre GetVehicleType(engine_id) == AIVehicle::VT_AIR.
	 * @return The PlaneType the engine has.
	 */
	static AIAirport::PlaneType GetPlaneType(EngineID engine_id);
};

#endif /* AI_ENGINE_HPP */
