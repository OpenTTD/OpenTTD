/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_vehiclelist.hpp List all the vehicles (you own). */

#ifndef SCRIPT_VEHICLELIST_HPP
#define SCRIPT_VEHICLELIST_HPP

#include "script_list.hpp"
#include "script_vehicle.hpp"

/**
 * Creates a list of vehicles of which you are the owner.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptVehicleList : public ScriptList {
public:
#ifdef DOXYGEN_API
	ScriptVehicleList();

	/**
	 * Apply a filter when building the list.
	 * @param filter_function The function which will be doing the filtering.
	 * @param params The params to give to the filters (minus the first param,
	 *  which is always the index-value).
	 * @note You can write your own filters and use them. Just remember that
	 *  the first parameter should be the index-value, and it should return
	 *  a bool.
	 * @note Example:
	 *  ScriptVehicleList(ScriptVehicle.IsInDepot);
	 *  function IsType(vehicle_id, type)
	 *  {
	 *    return ScriptVehicle.GetVehicleType(vehicle_id) == type;
	 *  }
	 *  ScriptVehicleList(IsType, ScriptVehicle.VT_ROAD);
	 */
	ScriptVehicleList(void *filter_function, int params, ...);
#else
	/**
	 * The constructor wrapper from Squirrel.
	 */
	ScriptVehicleList(HSQUIRRELVM vm);
#endif
};

/**
 * Creates a list of vehicles that have orders to a given station.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptVehicleList_Station : public ScriptList {
public:
	/**
	 * @param station_id The station to get the list of vehicles from, which have orders to it.
	 * @pre ScriptBaseStation::IsValidBaseStation(station_id)
	 */
	ScriptVehicleList_Station(StationID station_id);
};

/**
 * Creates a list of vehicles that have orders to a given depot.
 * The list is created with a tile. If the tile is part of an airport all
 * aircraft having a depot order on a hangar of that airport will be
 * returned. For all other vehicle types the tile has to be a depot or
 * an empty list will be returned.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptVehicleList_Depot : public ScriptList {
public:
	/**
	 * @param tile The tile of the depot to get the list of vehicles from, which have orders to it.
	 */
	ScriptVehicleList_Depot(TileIndex tile);
};

/**
 * Creates a list of vehicles that share orders.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptVehicleList_SharedOrders : public ScriptList {
public:
	/**
	 * @param vehicle_id The vehicle that the rest shared orders with.
	 */
	ScriptVehicleList_SharedOrders(VehicleID vehicle_id);
};

/**
 * Creates a list of vehicles that are in a group.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptVehicleList_Group : public ScriptList {
public:
	/**
	 * @param group_id The ID of the group the vehicles are in.
	 * @game @pre ScriptCompanyMode::IsValid().
	 */
	ScriptVehicleList_Group(GroupID group_id);
};

/**
 * Creates a list of vehicles that are in the default group.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptVehicleList_DefaultGroup : public ScriptList {
public:
	/**
	 * @param vehicle_type The VehicleType to get the list of vehicles for.
	 * @game @pre ScriptCompanyMode::IsValid().
	 */
	ScriptVehicleList_DefaultGroup(ScriptVehicle::VehicleType vehicle_type);
};

#endif /* SCRIPT_VEHICLELIST_HPP */
