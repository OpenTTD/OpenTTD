/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

GSLog.Info("1.6 API compatibility in effect.");

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

/* 14 Rand no longer returns negative values */
GSBase._Rand <- GSBase.Rand;
GSBase.Rand <- function()
{
	local r = GSBase._Rand();
	return (r & 1 << 31) != 0 ? r | ~((1 << 31) - 1) : r;
}

/* 14 RandItem no longer returns negative values */
GSBase.RandItem <- function(unused_param)
{
	return GSBase.Rand();
}

/* 14 RandRange no longer returns negative values */
GSBase._RandRange <- GSBase.RandRange
GSBase.RandRange <- function(max)
{
	local r = GSBase._RandRange(max);
	return (r & 1 << 31) != 0 ? r | ~((1 << 31) - 1) : r;
}

/* 14 RandRangeItem no longer returns negative values */
GSBase.RandRangeItem <- function(unused_param, max)
{
	return GSBase.RandRange(max);
}

/* 14 Chance no longer compares against randomized negative values */
GSBase._Chance <- GSBase.Chance
GSBase.Chance <- function(out, max)
{
	if (out > max) return GSBase._Chance(out, max);
	return GSBase.RandRange(max) < out;
}

/* 14 ChanceItem no longer compares against randomized negative values */
GSBase.ChanceItem <- function(unused_param, out, max)
{
	return GSBase.Chance(out, max);
}
