/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

GSLog.Info("13 API compatibility in effect.");

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

/* 14 airport maintenance cost factor returns -1 for unavailable airports */
GSAirport._GetMaintenanceCostFactor <- GSAirport.GetMaintenanceCostFactor
GSAirport.GetMaintenanceCostFactor <- function(type)
{
	if (!GSAirport.IsAirportInformationAvailable(type)) return 0xFFFF;
	return GSAirport._GetMaintenanceCostFactor(type);
}

/* 14 railtype maintenance cost factor returns -1 for unavailable railtypes */
GSRail._GetMaintenanceCostFactor <- GSRail.GetMaintenanceCostFactor
GSRail.GetMaintenanceCostFactor <- function(railtype)
{
	if (!GSRail.IsRailTypeAvailable(railtype)) return 0;
	return GSRail._GetMaintenanceCostFactor(railtype);
}

/* 14 roadtype maintenance cost factor returns -1 for unavailable roadtypes */
GSRoad._GetMaintenanceCostFactor <- GSRoad.GetMaintenanceCostFactor
GSRoad.GetMaintenanceCostFactor <- function(roadtype)
{
	if (!GSRoad.IsRoadTypeAvailable(roadtype)) return 0;
	return GSRoad._GetMaintenanceCostFactor(roadtype);
}
