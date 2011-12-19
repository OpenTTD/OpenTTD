/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_stationlist.hpp List all the stations (you own). */

#ifndef SCRIPT_STATIONLIST_HPP
#define SCRIPT_STATIONLIST_HPP

#include "script_list.hpp"
#include "script_station.hpp"

/**
 * Creates a list of stations of which you are the owner.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptStationList : public ScriptList {
public:
	/**
	 * @param station_type The type of station to make a list of stations for.
	 */
	ScriptStationList(ScriptStation::StationType station_type);
};

/**
 * Creates a list of stations which the vehicle has in its orders.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptStationList_Vehicle : public ScriptList {
public:
	/**
	 * @param vehicle_id The vehicle to get the list of stations he has in its orders from.
	 */
	ScriptStationList_Vehicle(VehicleID vehicle_id);
};

#endif /* SCRIPT_STATIONLIST_HPP */
