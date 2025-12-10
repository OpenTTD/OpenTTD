/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/* This file contains code to downgrade the API from 1.0 to 0.7. */

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

AIVehicle.SkipToVehicleOrder <- function(vehicle_id, order_position)
{
	return AIOrder.SkipToOrder(vehicle_id, order_position);
}

AIEngine.IsValidEngine <- function(engine_id)
{
	return AIEngine.IsBuildable(engine_id);
}

AIEngine.GetNameCompat0_7 <- AIEngine.GetName;
AIEngine.GetName <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return null;
	return AIEngine.GetNameCompat0_7(engine_id);
}

AIEngine.GetCargoTypeCompat0_7 <- AIEngine.GetCargoType;
AIEngine.GetCargoType <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return 255;
	return AIEngine.GetCargoTypeCompat0_7(engine_id);
}

AIEngine.CanRefitCargoCompat0_7 <- AIEngine.CanRefitCargo;
AIEngine.CanRefitCargo <- function(engine_id, cargo_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return false;
	return AIEngine.CanRefitCargoCompat0_7(engine_id, cargo_id);
}

AIEngine.CanPullCargoCompat0_7 <- AIEngine.CanPullCargo;
AIEngine.CanPullCargo <- function(engine_id, cargo_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return false;
	return AIEngine.CanPullCargoCompat0_7(engine_id, cargo_id);
}

AIEngine.GetCapacityCompat0_7 <- AIEngine.GetCapacity;
AIEngine.GetCapacity <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return -1;
	return AIEngine.GetCapacityCompat0_7(engine_id);
}

AIEngine.GetReliabilityCompat0_7 <- AIEngine.GetReliability;
AIEngine.GetReliability <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return -1;
	return AIEngine.GetReliabilityCompat0_7(engine_id);
}

AIEngine.GetMaxSpeedCompat0_7 <- AIEngine.GetMaxSpeed;
AIEngine.GetMaxSpeed <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return -1;
	return AIEngine.GetMaxSpeedCompat0_7(engine_id);
}

AIEngine.GetPriceCompat0_7 <- AIEngine.GetPrice;
AIEngine.GetPrice <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return -1;
	return AIEngine.GetPriceCompat0_7(engine_id);
}

AIEngine.GetMaxAgeCompat0_7 <- AIEngine.GetMaxAge;
AIEngine.GetMaxAge <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return -1;
	return AIEngine.GetMaxAgeCompat0_7(engine_id);
}

AIEngine.GetRunningCostCompat0_7 <- AIEngine.GetRunningCost;
AIEngine.GetRunningCost <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return -1;
	return AIEngine.GetRunningCostCompat0_7(engine_id);
}

AIEngine.GetPowerCompat0_7 <- AIEngine.GetPower;
AIEngine.GetPower <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return -1;
	return AIEngine.GetPowerCompat0_7(engine_id);
}

AIEngine.GetWeightCompat0_7 <- AIEngine.GetWeight;
AIEngine.GetWeight <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return -1;
	return AIEngine.GetWeightCompat0_7(engine_id);
}

AIEngine.GetMaxTractiveEffortCompat0_7 <- AIEngine.GetMaxTractiveEffort;
AIEngine.GetMaxTractiveEffort <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return -1;
	return AIEngine.GetMaxTractiveEffortCompat0_7(engine_id);
}

AIEngine.GetDesignDateCompat0_7 <- AIEngine.GetDesignDate;
AIEngine.GetDesignDate <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return -1;
	return AIEngine.GetDesignDateCompat0_7(engine_id);
}

AIEngine.GetVehicleTypeCompat0_7 <- AIEngine.GetVehicleType;
AIEngine.GetVehicleType <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return AIVehicle.VT_INVALID;
	return AIEngine.GetVehicleTypeCompat0_7(engine_id);
}

AIEngine.IsWagonCompat0_7 <- AIEngine.IsWagon;
AIEngine.IsWagon <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return false;
	return AIEngine.IsWagonCompat0_7(engine_id);
}

AIEngine.CanRunOnRailCompat0_7 <- AIEngine.CanRunOnRail;
AIEngine.CanRunOnRail <- function(engine_id, track_rail_type)
{
	if (!AIEngine.IsBuildable(engine_id)) return false;
	return AIEngine.CanRunOnRailCompat0_7(engine_id, track_rail_type);
}

AIEngine.HasPowerOnRailCompat0_7 <- AIEngine.HasPowerOnRail;
AIEngine.HasPowerOnRail <- function(engine_id, track_rail_type)
{
	if (!AIEngine.IsBuildable(engine_id)) return false;
	return AIEngine.HasPowerOnRailCompat0_7(engine_id, track_rail_type);
}

AIEngine.GetRoadTypeCompat0_7 <- AIEngine.GetRoadType;
AIEngine.GetRoadType <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return AIRoad.ROADTYPE_INVALID;
	return AIEngine.GetRoadTypeCompat0_7(engine_id);
}

AIEngine.GetRailTypeCompat0_7 <- AIEngine.GetRailType;
AIEngine.GetRailType <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return AIRail.RAILTYPE_INVALID;
	return AIEngine.GetRailTypeCompat0_7(engine_id);
}

AIEngine.IsArticulatedCompat0_7 <- AIEngine.IsArticulated;
AIEngine.IsArticulated <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return false;
	return AIEngine.IsArticulatedCompat0_7(engine_id);
}

AIEngine.GetPlaneTypeCompat0_7 <- AIEngine.GetPlaneType;
AIEngine.GetPlaneType <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return -1;
	return AIEngine.GetPlaneTypeCompat0_7(engine_id);
}

_AIWaypointList <- AIWaypointList;
class AIWaypointList extends _AIWaypointList {
	constructor()
	{
		::_AIWaypointList.constructor(AIWaypoint.WAYPOINT_RAIL);
	}
}
