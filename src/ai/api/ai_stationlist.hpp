/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_stationlist.hpp List all the stations (you own). */

#ifndef AI_STATIONLIST_HPP
#define AI_STATIONLIST_HPP

#include "ai_list.hpp"
#include "ai_station.hpp"

/**
 * Creates a list of stations of which you are the owner.
 * @ingroup AIList
 */
class AIStationList : public AIList {
public:
	/**
	 * @param station_type The type of station to make a list of stations for.
	 */
	AIStationList(AIStation::StationType station_type);
};

/**
 * Creates a list of stations which the vehicle has in its orders.
 * @ingroup AIList
 */
class AIStationList_Vehicle : public AIList {
public:
	/**
	 * @param vehicle_id The vehicle to get the list of stations he has in its orders from.
	 */
	AIStationList_Vehicle(VehicleID vehicle_id);
};

#endif /* AI_STATIONLIST_HPP */
