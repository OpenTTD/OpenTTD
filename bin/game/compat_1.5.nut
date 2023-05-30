/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

GSLog.Info("1.5 API compatibility in effect.");

/* 1.9 adds a vehicle type parameter. */
GSBridge._GetName <- GSBridge.GetName;
GSBridge.GetName <- function(bridge_id)
{
	return GSBridge._GetName(bridge_id, GSVehicle.VT_RAIL);
}

/* 1.11 adds a tile parameter. */
GSCompany._ChangeBankBalance <- GSCompany.ChangeBankBalance;
GSCompany.ChangeBankBalance <- function(company, delta, expenses_type)
{
	return GSCompany._ChangeBankBalance(company, delta, expenses_type, GSMap.TILE_INVALID);
}

/* 13 really checks RoadType against RoadType */
GSRoad._HasRoadType <- GSRoad.HasRoadType;
GSRoad.HasRoadType <- function(tile, road_type)
{
	local list = GSRoadTypeList(GSRoad.GetRoadTramType(road_type));
	foreach (rt, _ in list) {
		if (GSRoad._HasRoadType(tile, rt)) {
			return true;
		}
	}
	return false;
}

/* 14.0 removes small and medium advertise options */
GSTown.TOWN_ACTION_ADVERTISE_SMALL <- GSTown.TOWN_ACTION_ADVERTISE
GSTown.TOWN_ACTION_ADVERTISE_MEDIUM <- GSTown.TOWN_ACTION_ADVERTISE
GSTown.TOWN_ACTION_ADVERTISE_LARGE <- GSTown.TOWN_ACTION_ADVERTISE
