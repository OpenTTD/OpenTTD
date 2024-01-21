/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_enginelist.h List all the engines. */

#ifndef SCRIPT_ENGINELIST_HPP
#define SCRIPT_ENGINELIST_HPP

#include "script_list.h"
#include "script_vehicle.h"

/**
 * Create a list of engines based on a vehicle type.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptEngineList : public ScriptList {
public:
	/**
	 * @param vehicle_type The type of vehicle to make a list of engines for.
	 */
	ScriptEngineList(ScriptVehicle::VehicleType vehicle_type);
};

#endif /* SCRIPT_ENGINELIST_HPP */
