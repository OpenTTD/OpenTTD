/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

class GSCompatibility {}

function GSCompatibility::Add(major, minor)
{
	if (major == 1 && minor < 4) {
		/* 1.4 codified resetting custom growth rate in a different way */
		GSTown._SetGrowthRate <- GSTown.SetGrowthRate;
		GSTown.SetGrowthRate <- function(town_id, days_between_town_growth)
		{
			/* Growth rate 0 caused resetting the custom growth rate. While this was undocumented, it was used nevertheless (ofc). */
			if (days_between_town_growth == 0) days_between_town_growth = GSTown.TOWN_GROWTH_NORMAL;
			return GSTown._SetGrowthRate(town_id, days_between_town_growth);
		}
	}

	if (major == 1 && minor < 5) {
		/* 1.5 adds a game element reference to the news. */
		GSNews._Create <- GSNews.Create;
		GSNews.Create <- function(type, text, company)
		{
			return GSNews._Create(type, text, company, GSNews.NR_NONE, 0);
		}
	}

	if (major == 1 && minor < 9) {
		/* 1.9 adds a vehicle type parameter. */
		GSBridge._GetName <- GSBridge.GetName;
		GSBridge.GetName <- function(bridge_id)
		{
			return GSBridge._GetName(bridge_id, GSVehicle.VT_RAIL);
		}
	}


	if (major == 1 && minor < 11) {
		/* 1.11 adds a tile parameter. */
		GSCompany._ChangeBankBalance <- GSCompany.ChangeBankBalance;
		GSCompany.ChangeBankBalance <- function(company, delta, expenses_type)
		{
			return GSCompany._ChangeBankBalance(company, delta, expenses_type, GSMap.TILE_INVALID);
		}
	}

	if (major < 13) {
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
	}

	if (major < 15) {
		/* 15 renames GetBridgeID */
		GSBridge.GetBridgeID <- GSBridge.GetBridgeType;
	}
}
