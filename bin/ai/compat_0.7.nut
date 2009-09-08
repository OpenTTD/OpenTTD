/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

AILog.Info("0.7 API compatability in effect:");
AILog.Info(" - AITown::GetLastMonthProduction's behaviour has slightly changed.");
AILog.Info(" - AISubsidy::GetDestination returns STATION_INVALID for awarded subsidies.");
AILog.Info(" - AISubsidy::GetSource returns STATION_INVALID for awarded subsidies.");

AISign.GetMaxSignID <- function()
{
	local list = AISignList();
	local max_id = 0;
	foreach (id, d in list) {
		if (id > max_id) max_id = id;
	}
	return max_id;
}

AITile.GetHeight <- function(tile)
{
	if (!AIMap.IsValidTile(tile)) return -1;

	return AITile.GetCornerHeight(tile, AITile.CORNER_N);
}

AIOrder.ChangeOrder <- function(vehicle_id, order_position, order_flags)
{
	return AIOrder.SetOrderFlags(vehicle_id, order_position, order_flags);
}

AIWaypoint.WAYPOINT_INVALID <- 0xFFFF;

AISubsidy.SourceIsTown <- function(subsidy_id)
{
	if (!AISubsidy.IsValidSubsidy(subsidy_id) || AISubsidy.IsAwarded(subsidy_id)) return false;

	return AISubsidy.GetSourceType(subsidy_id) == AISubsidy.SPT_TOWN;
}

AISubsidy.GetSource <- function(subsidy_id)
{
	if (!AISubsidy.IsValidSubsidy(subsidy_id)) return AIBaseStation.STATION_INVALID;

	if (AISubsidy.IsAwarded(subsidy_id)) {
		return AIBaseStation.STATION_INVALID;
	}

	return AISubsidy.GetSourceIndex(subsidy_id);
}

AISubsidy.DestinationIsTown <- function(subsidy_id)
{
	if (!AISubsidy.IsValidSubsidy(subsidy_id) || AISubsidy.IsAwarded(subsidy_id)) return false;

	return AISubsidy.GetDestinationType(subsidy_id) == AISubsidy.SPT_TOWN;
}

AISubsidy.GetDestination <- function(subsidy_id)
{
	if (!AISubsidy.IsValidSubsidy(subsidy_id)) return AIBaseStation.STATION_INVALID;

	if (AISubsidy.IsAwarded(subsidy_id)) {
		return AIBaseStation.STATION_INVALID;
	}

	return AISubsidy.GetDestinationIndex(subsidy_id);
}

AITown.GetMaxProduction <- function(town_id, cargo_id)
{
	return AITown.GetLastMonthProduction(town_id, cargo_id);
}

AIRail.RemoveRailWaypoint <- function(tile)
{
	return AIRail.RemoveRailWaypointTileRect(tile, tile, true);
}

AIRail.RemoveRailStationTileRect <- function(tile, tile2)
{
	return AIRail.RemoveRailStationTileRectangle(tile, tile2, false);
}
