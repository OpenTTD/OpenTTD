/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file pricebase.h Price Bases */

extern const PriceBaseSpec _price_base_specs[] = {
	{    100, PCAT_NONE,         GSF_END        }, ///< PR_STATION_VALUE
	{    100, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_BUILD_RAIL
	{     95, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_BUILD_ROAD
	{     65, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_BUILD_SIGNALS
	{    275, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_BUILD_BRIDGE
	{    600, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_BUILD_DEPOT_TRAIN
	{    500, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_BUILD_DEPOT_ROAD
	{    700, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_BUILD_DEPOT_SHIP
	{    450, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_BUILD_TUNNEL
	{    200, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_BUILD_STATION_RAIL
	{    180, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_BUILD_STATION_RAIL_LENGTH
	{    600, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_BUILD_STATION_AIRPORT
	{    200, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_BUILD_STATION_BUS
	{    200, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_BUILD_STATION_TRUCK
	{    350, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_BUILD_STATION_DOCK
	{ 400000, PCAT_CONSTRUCTION, GSF_TRAIN      }, ///< PR_BUILD_VEHICLE_TRAIN
	{   2000, PCAT_CONSTRUCTION, GSF_TRAIN      }, ///< PR_BUILD_VEHICLE_WAGON
	{ 700000, PCAT_CONSTRUCTION, GSF_AIRCRAFT   }, ///< PR_BUILD_VEHICLE_AIRCRAFT
	{  14000, PCAT_CONSTRUCTION, GSF_ROAD       }, ///< PR_BUILD_VEHICLE_ROAD
	{  65000, PCAT_CONSTRUCTION, GSF_SHIP       }, ///< PR_BUILD_VEHICLE_SHIP
	{     20, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_BUILD_TREES
	{    250, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_TERRAFORM
	{     20, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_CLEAR_GRASS
	{     40, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_CLEAR_ROUGH
	{    200, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_CLEAR_ROCKS
	{    500, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_CLEAR_FILEDS
	{     20, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_CLEAR_TREES
	{    -70, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_CLEAR_RAIL
	{     10, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_CLEAR_SIGNALS
	{     50, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_CLEAR_BRIDGE
	{     80, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_CLEAR_DEPOT_TRAIN
	{     80, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_CLEAR_DEPOT_ROAD
	{     90, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_CLEAR_DEPOT_SHIP
	{     30, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_CLEAR_TUNNEL
	{  10000, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_CLEAR_WATER
	{     50, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_CLEAR_STATION_RAIL
	{     30, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_CLEAR_STATION_AIRPORT
	{     50, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_CLEAR_STATION_BUS
	{     50, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_CLEAR_STATION_TRUCK
	{     55, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_CLEAR_STATION_DOCK
	{   1600, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_CLEAR_HOUSE
	{     40, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_CLEAR_ROAD
	{   5600, PCAT_RUNNING,      GSF_TRAIN      }, ///< PR_RUNNING_TRAIN_STEAM
	{   5200, PCAT_RUNNING,      GSF_TRAIN      }, ///< PR_RUNNING_TRAIN_DIESEL
	{   4800, PCAT_RUNNING,      GSF_TRAIN      }, ///< PR_RUNNING_TRAIN_ELECTRIC
	{   9600, PCAT_RUNNING,      GSF_AIRCRAFT   }, ///< PR_RUNNING_AIRCRAFT
	{   1600, PCAT_RUNNING,      GSF_ROAD       }, ///< PR_RUNNING_ROADVEH
	{   5600, PCAT_RUNNING,      GSF_SHIP       }, ///< PR_RUNNING_SHIP
	{1000000, PCAT_CONSTRUCTION, GSF_END        }, ///< PR_BUILD_INDUSTRY
};
assert_compile(lengthof(_price_base_specs) == PR_END);
