/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_vehicle.hpp Everything to query and build vehicles. */

#ifndef SCRIPT_VEHICLE_HPP
#define SCRIPT_VEHICLE_HPP

#include "script_road.hpp"

/**
 * Class that handles all vehicle related functions.
 * @api ai game
 */
class ScriptVehicle : public ScriptObject {
public:
	/**
	 * All vehicle related error messages.
	 */
	enum ErrorMessages {
		/** Base for vehicle related errors */
		ERR_VEHICLE_BASE = ScriptError::ERR_CAT_VEHICLE << ScriptError::ERR_CAT_BIT_SIZE,

		/** Too many vehicles in the game, can't build any more. */
		ERR_VEHICLE_TOO_MANY,                   // [STR_ERROR_TOO_MANY_VEHICLES_IN_GAME]

		/** Vehicle is not available */
		ERR_VEHICLE_NOT_AVAILABLE,              // [STR_ERROR_AIRCRAFT_NOT_AVAILABLE, STR_ERROR_ROAD_VEHICLE_NOT_AVAILABLE, STR_ERROR_SHIP_NOT_AVAILABLE, STR_ERROR_RAIL_VEHICLE_NOT_AVAILABLE]

		/** Vehicle can't be build due to game settigns */
		ERR_VEHICLE_BUILD_DISABLED,             // [STR_ERROR_CAN_T_BUY_TRAIN, STR_ERROR_CAN_T_BUY_ROAD_VEHICLE, STR_ERROR_CAN_T_BUY_SHIP, STR_ERROR_CAN_T_BUY_AIRCRAFT]

		/** Vehicle can't be build in the selected depot */
		ERR_VEHICLE_WRONG_DEPOT,                // [STR_ERROR_DEPOT_WRONG_DEPOT_TYPE]

		/** Vehicle can't return to the depot */
		ERR_VEHICLE_CANNOT_SEND_TO_DEPOT,       // [STR_ERROR_CAN_T_SEND_TRAIN_TO_DEPOT, STR_ERROR_CAN_T_SEND_ROAD_VEHICLE_TO_DEPOT, STR_ERROR_CAN_T_SEND_SHIP_TO_DEPOT, STR_ERROR_CAN_T_SEND_AIRCRAFT_TO_HANGAR]

		/** Vehicle can't start / stop */
		ERR_VEHICLE_CANNOT_START_STOP,          // [STR_ERROR_CAN_T_STOP_START_TRAIN, STR_ERROR_CAN_T_STOP_START_ROAD_VEHICLE, STR_ERROR_CAN_T_STOP_START_SHIP, STR_ERROR_CAN_T_STOP_START_AIRCRAFT]

		/** Vehicle can't turn */
		ERR_VEHICLE_CANNOT_TURN,                // [STR_ERROR_CAN_T_MAKE_ROAD_VEHICLE_TURN, STR_ERROR_CAN_T_REVERSE_DIRECTION_TRAIN, STR_ERROR_CAN_T_REVERSE_DIRECTION_RAIL_VEHICLE, STR_ERROR_CAN_T_REVERSE_DIRECTION_RAIL_VEHICLE_MULTIPLE_UNITS]

		/** Vehicle can't be refit */
		ERR_VEHICLE_CANNOT_REFIT,               // [STR_ERROR_CAN_T_REFIT_TRAIN, STR_ERROR_CAN_T_REFIT_ROAD_VEHICLE, STR_ERROR_CAN_T_REFIT_SHIP, STR_ERROR_CAN_T_REFIT_AIRCRAFT]

		/** Vehicle is destroyed */
		ERR_VEHICLE_IS_DESTROYED,               // [STR_ERROR_VEHICLE_IS_DESTROYED]

		/** Vehicle is not in a depot */
		ERR_VEHICLE_NOT_IN_DEPOT,               // [STR_ERROR_AIRCRAFT_MUST_BE_STOPPED_INSIDE_HANGAR, STR_ERROR_ROAD_VEHICLE_MUST_BE_STOPPED_INSIDE_DEPOT, STR_ERROR_TRAIN_MUST_BE_STOPPED_INSIDE_DEPOT, STR_ERROR_SHIP_MUST_BE_STOPPED_INSIDE_DEPOT]

		/** Vehicle is flying */
		ERR_VEHICLE_IN_FLIGHT,                  // [STR_ERROR_AIRCRAFT_IS_IN_FLIGHT]

		/** Vehicle is without power */
		ERR_VEHICLE_NO_POWER,                   // [STR_ERROR_TRAIN_START_NO_POWER]

		/** Vehicle would get too long during construction. */
		ERR_VEHICLE_TOO_LONG,                   // [STR_ERROR_TRAIN_TOO_LONG]
	};

	/**
	 * The type of a vehicle available in the game. Trams for example are
	 *  road vehicles, as maglev is a rail vehicle.
	 */
	enum VehicleType {
		VT_RAIL,           ///< Rail type vehicle.
		VT_ROAD,           ///< Road type vehicle (bus / truck).
		VT_WATER,          ///< Water type vehicle.
		VT_AIR,            ///< Air type vehicle.
		VT_INVALID = 0xFF, ///< Invalid vehicle type.
	};

	/**
	 * The different states a vehicle can be in.
	 */
	enum VehicleState {
		VS_RUNNING,        ///< The vehicle is currently running.
		VS_STOPPED,        ///< The vehicle is stopped manually.
		VS_IN_DEPOT,       ///< The vehicle is stopped in the depot.
		VS_AT_STATION,     ///< The vehicle is stopped at a station and is currently loading or unloading.
		VS_BROKEN,         ///< The vehicle has broken down and will start running again in a while.
		VS_CRASHED,        ///< The vehicle is crashed (and will never run again).

		VS_INVALID = 0xFF, ///< An invalid vehicle state.
	};

	static const VehicleID VEHICLE_INVALID = 0xFFFFF; ///< Invalid VehicleID.

	/**
	 * Checks whether the given vehicle is valid and owned by you.
	 * @param vehicle_id The vehicle to check.
	 * @return True if and only if the vehicle is valid.
	 */
	static bool IsValidVehicle(VehicleID vehicle_id);

	/**
	 * Get the number of wagons a vehicle has.
	 * @param vehicle_id The vehicle to get the number of wagons from.
	 * @pre IsValidVehicle(vehicle_id).
	 * @return The number of wagons the vehicle has.
	 */
	static int32 GetNumWagons(VehicleID vehicle_id);

	/**
	 * Set the name of a vehicle.
	 * @param vehicle_id The vehicle to set the name for.
	 * @param name The name for the vehicle (can be either a raw string, or a ScriptText object).
	 * @pre IsValidVehicle(vehicle_id).
	 * @pre name != NULL && len(name) != 0.
	 * @game @pre Valid ScriptCompanyMode active in scope.
	 * @exception ScriptError::ERR_NAME_IS_NOT_UNIQUE
	 * @return True if and only if the name was changed.
	 */
	static bool SetName(VehicleID vehicle_id, Text *name);

	/**
	 * Get the name of a vehicle.
	 * @param vehicle_id The vehicle to get the name of.
	 * @pre IsValidVehicle(vehicle_id).
	 * @return The name the vehicle has.
	 */
	static char *GetName(VehicleID vehicle_id);

	/**
	 * Get the owner of a vehicle.
	 * @param vehicle_id The vehicle to get the owner of.
	 * @pre IsValidVehicle(vehicle_id).
	 * @return The owner the vehicle has.
	 * @api -ai
	 */
	static ScriptCompany::CompanyID GetOwner(VehicleID vehicle_id);

	/**
	 * Get the current location of a vehicle.
	 * @param vehicle_id The vehicle to get the location of.
	 * @pre IsValidVehicle(vehicle_id).
	 * @return The tile the vehicle is currently on.
	 */
	static TileIndex GetLocation(VehicleID vehicle_id);

	/**
	 * Get the engine-type of a vehicle.
	 * @param vehicle_id The vehicle to get the engine-type of.
	 * @pre IsValidVehicle(vehicle_id).
	 * @return The engine type the vehicle has.
	 */
	static EngineID GetEngineType(VehicleID vehicle_id);

	/**
	 * Get the engine-type of a wagon.
	 * @param vehicle_id The vehicle to get the engine-type of.
	 * @param wagon The wagon in the vehicle to get the engine-type of.
	 * @pre IsValidVehicle(vehicle_id).
	 * @pre wagon < GetNumWagons(vehicle_id).
	 * @return The engine type the vehicle has.
	 */
	static EngineID GetWagonEngineType(VehicleID vehicle_id, int wagon);

	/**
	 * Get the unitnumber of a vehicle.
	 * @param vehicle_id The vehicle to get the unitnumber of.
	 * @pre IsValidVehicle(vehicle_id).
	 * @return The unitnumber the vehicle has.
	 */
	static int32 GetUnitNumber(VehicleID vehicle_id);

	/**
	 * Get the current age of a vehicle.
	 * @param vehicle_id The vehicle to get the age of.
	 * @pre IsValidVehicle(vehicle_id).
	 * @return The current age the vehicle has.
	 * @note The age is in days.
	 */
	static int32 GetAge(VehicleID vehicle_id);

	/**
	 * Get the current age of a second (or third, etc.) engine in a train vehicle.
	 * @param vehicle_id The vehicle to get the age of.
	 * @param wagon The wagon in the vehicle to get the age of.
	 * @pre IsValidVehicle(vehicle_id).
	 * @pre wagon < GetNumWagons(vehicle_id).
	 * @return The current age the vehicle has.
	 * @note The age is in days.
	 */
	static int32 GetWagonAge(VehicleID vehicle_id, int wagon);

	/**
	 * Get the maximum age of a vehicle.
	 * @param vehicle_id The vehicle to get the age of.
	 * @pre IsValidVehicle(vehicle_id).
	 * @return The maximum age the vehicle has.
	 * @note The age is in days.
	 */
	static int32 GetMaxAge(VehicleID vehicle_id);

	/**
	 * Get the age a vehicle has left (maximum - current).
	 * @param vehicle_id The vehicle to get the age of.
	 * @pre IsValidVehicle(vehicle_id).
	 * @return The age the vehicle has left.
	 * @note The age is in days.
	 */
	static int32 GetAgeLeft(VehicleID vehicle_id);

	/**
	 * Get the current speed of a vehicle.
	 * @param vehicle_id The vehicle to get the speed of.
	 * @pre IsValidVehicle(vehicle_id).
	 * @return The current speed of the vehicle.
	 * @note The speed is in OpenTTD's internal speed unit.
	 *       This is mph / 1.6, which is roughly km/h.
	 *       To get km/h multiply this number by 1.00584.
	 */
	static int32 GetCurrentSpeed(VehicleID vehicle_id);

	/**
	 * Get the current state of a vehicle.
	 * @param vehicle_id The vehicle to get the state of.
	 * @pre IsValidVehicle(vehicle_id).
	 * @return The current state of the vehicle.
	 */
	static VehicleState GetState(VehicleID vehicle_id);

	/**
	 * Get the running cost of this vehicle.
	 * @param vehicle_id The vehicle to get the running cost of.
	 * @pre IsValidVehicle(vehicle_id).
	 * @return The running cost of the vehicle per year.
	 * @note Cost is per year; divide by 365 to get per day.
	 * @note This is not equal to ScriptEngine::GetRunningCost for Trains, because
	 *   wagons and second engines can add up in the calculation too.
	 */
	static Money GetRunningCost(VehicleID vehicle_id);

	/**
	 * Get the current profit of a vehicle.
	 * @param vehicle_id The vehicle to get the profit of.
	 * @pre IsValidVehicle(vehicle_id).
	 * @return The current profit the vehicle has.
	 */
	static Money GetProfitThisYear(VehicleID vehicle_id);

	/**
	 * Get the profit of last year of a vehicle.
	 * @param vehicle_id The vehicle to get the profit of.
	 * @pre IsValidVehicle(vehicle_id).
	 * @return The profit the vehicle had last year.
	 */
	static Money GetProfitLastYear(VehicleID vehicle_id);


	/**
	 * Get the current value of a vehicle.
	 * @param vehicle_id The vehicle to get the value of.
	 * @pre IsValidVehicle(vehicle_id).
	 * @return The value the vehicle currently has (the amount you should get
	 *  when you would sell the vehicle right now).
	 */
	static Money GetCurrentValue(VehicleID vehicle_id);

	/**
	 * Get the type of vehicle.
	 * @param vehicle_id The vehicle to get the type of.
	 * @pre IsValidVehicle(vehicle_id).
	 * @return The vehicle type.
	 */
	static ScriptVehicle::VehicleType GetVehicleType(VehicleID vehicle_id);

	/**
	 * Get the RoadType of the vehicle.
	 * @param vehicle_id The vehicle to get the RoadType of.
	 * @pre IsValidVehicle(vehicle_id).
	 * @pre GetVehicleType(vehicle_id) == VT_ROAD.
	 * @return The RoadType the vehicle has.
	 */
	static ScriptRoad::RoadType GetRoadType(VehicleID vehicle_id);

	/**
	 * Check if a vehicle is in a depot.
	 * @param vehicle_id The vehicle to check.
	 * @pre IsValidVehicle(vehicle_id).
	 * @return True if and only if the vehicle is in a depot.
	 */
	static bool IsInDepot(VehicleID vehicle_id);

	/**
	 * Check if a vehicle is in a depot and stopped.
	 * @param vehicle_id The vehicle to check.
	 * @pre IsValidVehicle(vehicle_id).
	 * @return True if and only if the vehicle is in a depot and stopped.
	 */
	static bool IsStoppedInDepot(VehicleID vehicle_id);

	/**
	 * Builds a vehicle with the given engine at the given depot.
	 * @param depot The depot where the vehicle will be build.
	 * @param engine_id The engine to use for this vehicle.
	 * @pre The tile at depot has a depot that can build the engine and
	 *   is owned by you.
	 * @pre ScriptEngine::IsBuildable(engine_id).
	 * @game @pre Valid ScriptCompanyMode active in scope.
	 * @exception ScriptVehicle::ERR_VEHICLE_TOO_MANY
	 * @exception ScriptVehicle::ERR_VEHICLE_BUILD_DISABLED
	 * @exception ScriptVehicle::ERR_VEHICLE_WRONG_DEPOT
	 * @return The VehicleID of the new vehicle, or an invalid VehicleID when
	 *   it failed. Check the return value using IsValidVehicle. In test-mode
	 *   0 is returned if it was successful; any other value indicates failure.
	 * @note In Test Mode it means you can't assign orders yet to this vehicle,
	 *   as the vehicle isn't really built yet. Build it for real first before
	 *   assigning orders.
	 */
	static VehicleID BuildVehicle(TileIndex depot, EngineID engine_id);

	/**
	 * Builds a vehicle with the given engine at the given depot and refits it to the given cargo.
	 * @param depot The depot where the vehicle will be build.
	 * @param engine_id The engine to use for this vehicle.
	 * @param cargo The cargo to refit to.
	 * @pre The tile at depot has a depot that can build the engine and
	 *   is owned by you.
	 * @pre ScriptEngine::IsBuildable(engine_id).
	 * @pre ScriptCargo::IsValidCargo(cargo).
	 * @game @pre Valid ScriptCompanyMode active in scope.
	 * @exception ScriptVehicle::ERR_VEHICLE_TOO_MANY
	 * @exception ScriptVehicle::ERR_VEHICLE_BUILD_DISABLED
	 * @exception ScriptVehicle::ERR_VEHICLE_WRONG_DEPOT
	 * @return The VehicleID of the new vehicle, or an invalid VehicleID when
	 *   it failed. Check the return value using IsValidVehicle. In test-mode
	 *   0 is returned if it was successful; any other value indicates failure.
	 * @note In Test Mode it means you can't assign orders yet to this vehicle,
	 *   as the vehicle isn't really built yet. Build it for real first before
	 *   assigning orders.
	 */
	static VehicleID BuildVehicleWithRefit(TileIndex depot, EngineID engine_id, CargoID cargo);

	/**
	 * Gets the capacity of a vehicle built at the given depot with the given engine and refitted to the given cargo.
	 * @param depot The depot where the vehicle will be build.
	 * @param engine_id The engine to use for this vehicle.
	 * @param cargo The cargo to refit to.
	 * @pre The tile at depot has a depot that can build the engine and
	 *   is owned by you.
	 * @pre ScriptEngine::IsBuildable(engine_id).
	 * @pre ScriptCargo::IsValidCargo(cargo).
	 * @return The capacity the vehicle will have when refited.
	 */
	static int GetBuildWithRefitCapacity(TileIndex depot, EngineID engine_id, CargoID cargo);

	/**
	 * Clones a vehicle at the given depot, copying or cloning its orders.
	 * @param depot The depot where the vehicle will be build.
	 * @param vehicle_id The vehicle to use as example for the new vehicle.
	 * @param share_orders Should the orders be copied or shared?
	 * @pre The tile 'depot' has a depot on it, allowing 'vehicle_id'-type vehicles.
	 * @pre IsValidVehicle(vehicle_id).
	 * @game @pre Valid ScriptCompanyMode active in scope.
	 * @exception ScriptVehicle::ERR_VEHICLE_TOO_MANY
	 * @exception ScriptVehicle::ERR_VEHICLE_BUILD_DISABLED
	 * @exception ScriptVehicle::ERR_VEHICLE_WRONG_DEPOT
	 * @return The VehicleID of the new vehicle, or an invalid VehicleID when
	 *   it failed. Check the return value using IsValidVehicle. In test-mode
	 *   0 is returned if it was successful; any other value indicates failure.
	 */
	static VehicleID CloneVehicle(TileIndex depot, VehicleID vehicle_id, bool share_orders);

	/**
	 * Move a wagon after another wagon.
	 * @param source_vehicle_id The vehicle to move a wagon away from.
	 * @param source_wagon The wagon in source_vehicle to move.
	 * @param dest_vehicle_id The vehicle to move the wagon to, or -1 to create a new vehicle.
	 * @param dest_wagon The wagon in dest_vehicle to place source_wagon after.
	 * @pre IsValidVehicle(source_vehicle_id).
	 * @pre source_wagon < GetNumWagons(source_vehicle_id).
	 * @pre dest_vehicle_id == -1 || (IsValidVehicle(dest_vehicle_id) && dest_wagon < GetNumWagons(dest_vehicle_id)).
	 * @pre GetVehicleType(source_vehicle_id) == VT_RAIL.
	 * @pre dest_vehicle_id == -1 || GetVehicleType(dest_vehicle_id) == VT_RAIL.
	 * @game @pre Valid ScriptCompanyMode active in scope.
	 * @return Whether or not moving the wagon succeeded.
	 */
	static bool MoveWagon(VehicleID source_vehicle_id, int source_wagon, int dest_vehicle_id, int dest_wagon);

	/**
	 * Move a chain of wagons after another wagon.
	 * @param source_vehicle_id The vehicle to move a wagon away from.
	 * @param source_wagon The first wagon in source_vehicle to move.
	 * @param dest_vehicle_id The vehicle to move the wagons to, or -1 to create a new vehicle.
	 * @param dest_wagon The wagon in dest_vehicle to place source_wagon and following wagons after.
	 * @pre IsValidVehicle(source_vehicle_id).
	 * @pre source_wagon < GetNumWagons(source_vehicle_id).
	 * @pre dest_vehicle_id == -1 || (IsValidVehicle(dest_vehicle_id) && dest_wagon < GetNumWagons(dest_vehicle_id)).
	 * @pre GetVehicleType(source_vehicle_id) == VT_RAIL.
	 * @pre dest_vehicle_id == -1 || GetVehicleType(dest_vehicle_id) == VT_RAIL.
	 * @game @pre Valid ScriptCompanyMode active in scope.
	 * @return Whether or not moving the wagons succeeded.
	 */
	static bool MoveWagonChain(VehicleID source_vehicle_id, int source_wagon, int dest_vehicle_id, int dest_wagon);

	/**
	 * Gets the capacity of the given vehicle when refitted to the given cargo type.
	 * @param vehicle_id The vehicle to refit.
	 * @param cargo The cargo to refit to.
	 * @pre IsValidVehicle(vehicle_id).
	 * @pre ScriptCargo::IsValidCargo(cargo).
	 * @pre You must own the vehicle.
	 * @pre The vehicle must be stopped in the depot.
	 * @return The capacity the vehicle will have when refited.
	 */
	static int GetRefitCapacity(VehicleID vehicle_id, CargoID cargo);

	/**
	 * Refits a vehicle to the given cargo type.
	 * @param vehicle_id The vehicle to refit.
	 * @param cargo The cargo to refit to.
	 * @pre IsValidVehicle(vehicle_id).
	 * @pre ScriptCargo::IsValidCargo(cargo).
	 * @pre You must own the vehicle.
	 * @pre The vehicle must be stopped in the depot.
	 * @game @pre Valid ScriptCompanyMode active in scope.
	 * @exception ScriptVehicle::ERR_VEHICLE_CANNOT_REFIT
	 * @exception ScriptVehicle::ERR_VEHICLE_IS_DESTROYED
	 * @exception ScriptVehicle::ERR_VEHICLE_NOT_IN_DEPOT
	 * @return True if and only if the refit succeeded.
	 */
	static bool RefitVehicle(VehicleID vehicle_id, CargoID cargo);

	/**
	 * Sells the given vehicle.
	 * @param vehicle_id The vehicle to sell.
	 * @pre IsValidVehicle(vehicle_id).
	 * @pre You must own the vehicle.
	 * @pre The vehicle must be stopped in the depot.
	 * @game @pre Valid ScriptCompanyMode active in scope.
	 * @exception ScriptVehicle::ERR_VEHICLE_IS_DESTROYED
	 * @exception ScriptVehicle::ERR_VEHICLE_NOT_IN_DEPOT
	 * @return True if and only if the vehicle has been sold.
	 */
	static bool SellVehicle(VehicleID vehicle_id);

	/**
	 * Sells the given wagon from the vehicle.
	 * @param vehicle_id The vehicle to sell a wagon from.
	 * @param wagon The wagon to sell.
	 * @pre IsValidVehicle(vehicle_id).
	 * @pre wagon < GetNumWagons(vehicle_id).
	 * @pre You must own the vehicle.
	 * @pre The vehicle must be stopped in the depot.
	 * @game @pre Valid ScriptCompanyMode active in scope.
	 * @exception ScriptVehicle::ERR_VEHICLE_IS_DESTROYED
	 * @exception ScriptVehicle::ERR_VEHICLE_NOT_IN_DEPOT
	 * @return True if and only if the wagon has been sold.
	 */
	static bool SellWagon(VehicleID vehicle_id, int wagon);

	/**
	 * Sells all wagons from the vehicle starting from a given position.
	 * @param vehicle_id The vehicle to sell a wagon from.
	 * @param wagon The wagon to sell.
	 * @pre IsValidVehicle(vehicle_id).
	 * @pre wagon < GetNumWagons(vehicle_id).
	 * @pre You must own the vehicle.
	 * @pre The vehicle must be stopped in the depot.
	 * @game @pre Valid ScriptCompanyMode active in scope.
	 * @exception ScriptVehicle::ERR_VEHICLE_IS_DESTROYED
	 * @exception ScriptVehicle::ERR_VEHICLE_NOT_IN_DEPOT
	 * @return True if and only if the wagons have been sold.
	 */
	static bool SellWagonChain(VehicleID vehicle_id, int wagon);

	/**
	 * Sends the given vehicle to a depot. If the vehicle has already been
	 * sent to a depot it continues with its normal orders instead.
	 * @param vehicle_id The vehicle to send to a depot.
	 * @pre IsValidVehicle(vehicle_id).
	 * @game @pre Valid ScriptCompanyMode active in scope.
	 * @exception ScriptVehicle::ERR_VEHICLE_CANNOT_SEND_TO_DEPOT
	 * @return True if the current order was changed.
	 */
	static bool SendVehicleToDepot(VehicleID vehicle_id);

	/**
	 * Sends the given vehicle to a depot for servicing. If the vehicle has
	 * already been sent to a depot it continues with its normal orders instead.
	 * @param vehicle_id The vehicle to send to a depot for servicing.
	 * @pre IsValidVehicle(vehicle_id).
	 * @game @pre Valid ScriptCompanyMode active in scope.
	 * @exception ScriptVehicle::ERR_VEHICLE_CANNOT_SEND_TO_DEPOT
	 * @return True if the current order was changed.
	 */
	static bool SendVehicleToDepotForServicing(VehicleID vehicle_id);

	/**
	 * Starts or stops the given vehicle depending on the current state.
	 * @param vehicle_id The vehicle to start/stop.
	 * @pre IsValidVehicle(vehicle_id).
	 * @game @pre Valid ScriptCompanyMode active in scope.
	 * @exception ScriptVehicle::ERR_VEHICLE_CANNOT_START_STOP
	 * @exception (For aircraft only): ScriptVehicle::ERR_VEHICLE_IN_FLIGHT
	 * @exception (For trains only): ScriptVehicle::ERR_VEHICLE_NO_POWER
	 * @return True if and only if the vehicle has been started or stopped.
	 */
	static bool StartStopVehicle(VehicleID vehicle_id);

	/**
	 * Turn the given vehicle so it'll drive the other way.
	 * @param vehicle_id The vehicle to turn.
	 * @pre IsValidVehicle(vehicle_id).
	 * @pre GetVehicleType(vehicle_id) == VT_ROAD || GetVehicleType(vehicle_id) == VT_RAIL.
	 * @game @pre Valid ScriptCompanyMode active in scope.
	 * @return True if and only if the vehicle has started to turn.
	 * @note Vehicles cannot always be reversed. For example busses and trucks need to be running
	 *  and not be inside a depot.
	 */
	static bool ReverseVehicle(VehicleID vehicle_id);

	/**
	 * Get the maximum amount of a specific cargo the given vehicle can transport.
	 * @param vehicle_id The vehicle to get the capacity of.
	 * @param cargo The cargo to get the capacity for.
	 * @pre IsValidVehicle(vehicle_id).
	 * @pre ScriptCargo::IsValidCargo(cargo).
	 * @return The maximum amount of the given cargo the vehicle can transport.
	 */
	static int32 GetCapacity(VehicleID vehicle_id, CargoID cargo);

	/**
	 * Get the length of a the total vehicle in 1/16's of a tile.
	 * @param vehicle_id The vehicle to get the length of.
	 * @pre IsValidVehicle(vehicle_id).
	 * @pre GetVehicleType(vehicle_id) == VT_ROAD || GetVehicleType(vehicle_id) == VT_RAIL.
	 * @return The length of the engine.
	 */
	static int GetLength(VehicleID vehicle_id);

	/**
	 * Get the amount of a specific cargo the given vehicle is transporting.
	 * @param vehicle_id The vehicle to get the load amount of.
	 * @param cargo The cargo to get the loaded amount for.
	 * @pre IsValidVehicle(vehicle_id).
	 * @pre ScriptCargo::IsValidCargo(cargo).
	 * @return The amount of the given cargo the vehicle is currently transporting.
	 */
	static int32 GetCargoLoad(VehicleID vehicle_id, CargoID cargo);

	/**
	 * Get the group of a given vehicle.
	 * @param vehicle_id The vehicle to get the group from.
	 * @return The group of the given vehicle.
	 */
	static GroupID GetGroupID(VehicleID vehicle_id);

	/**
	 * Check if the vehicle is articulated.
	 * @param vehicle_id The vehicle to check.
	 * @pre IsValidVehicle(vehicle_id).
	 * @pre GetVehicleType(vehicle_id) == VT_ROAD || GetVehicleType(vehicle_id) == VT_RAIL.
	 * @return True if the vehicle is articulated.
	 */
	static bool IsArticulated(VehicleID vehicle_id);

	/**
	 * Check if the vehicle has shared orders.
	 * @param vehicle_id The vehicle to check.
	 * @pre IsValidVehicle(vehicle_id).
	 * @return True if the vehicle has shared orders.
	 */
	static bool HasSharedOrders(VehicleID vehicle_id);

	/**
	 * Get the current reliability of a vehicle.
	 * @param vehicle_id The vehicle to check.
	 * @pre IsValidVehicle(vehicle_id).
	 * @return The current reliability (0-100%).
	 */
	static int GetReliability(VehicleID vehicle_id);

	/**
	 * Get the maximum allowed distance between two orders for a vehicle.
	 * The distance returned is a vehicle-type specific distance independent from other
	 * map distances, you may use the result of this function to compare it
	 * with the result of ScriptOrder::GetOrderDistance.
	 * @param vehicle_id The vehicle to get the distance for.
	 * @pre IsValidVehicle(vehicle_id).
	 * @return The maximum distance between two orders for this vehicle
	 *         or 0 if the distance is unlimited.
	 * @note   The unit of the order distances is unspecified and should
	 *         not be compared with map distances
	 * @see ScriptOrder::GetOrderDistance
	 */
	static uint GetMaximumOrderDistance(VehicleID vehicle_id);

private:
	/**
	 * Internal function used by BuildVehicle(WithRefit).
	 */
	static VehicleID _BuildVehicleInternal(TileIndex depot, EngineID engine_id, CargoID cargo);

	/**
	 * Internal function used by SellWagon(Chain).
	 */
	static bool _SellWagonInternal(VehicleID vehicle_id, int wagon, bool sell_attached_wagons);

	/**
	 * Internal function used by MoveWagon(Chain).
	 */
	static bool _MoveWagonInternal(VehicleID source_vehicle_id, int source_wagon, bool move_attached_wagons, int dest_vehicle_id, int dest_wagon);
};

#endif /* SCRIPT_VEHICLE_HPP */
