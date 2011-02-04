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

AIVehicle.SkipToVehicleOrder <- function(vehicle_id, order_position)
{
	return AIOrder.SkipToOrder(vehicle_id, order_position);
}

AIEngine.IsValidEngine <- function(engine_id)
{
	return AIEngine.IsBuildable(engine_id);
}

AIEngine._GetName <- AIEngine.GetName;
AIEngine.GetName <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return null;
	return AIEngine._GetName(engine_id);
}

AIEngine._GetCargoType <- AIEngine.GetCargoType;
AIEngine.GetCargoType <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return 255;
	return AIEngine._GetCargoType(engine_id);
}

AIEngine._CanRefitCargo <- AIEngine.CanRefitCargo;
AIEngine.CanRefitCargo <- function(engine_id, cargo_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return false;
	return AIEngine._CanRefitCargo(engine_id, cargo_id);
}

AIEngine._CanPullCargo <- AIEngine.CanPullCargo;
AIEngine.CanPullCargo <- function(engine_id, cargo_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return false;
	return AIEngine._CanPullCargo(engine_id, cargo_id);
}

AIEngine._GetCapacity <- AIEngine.GetCapacity;
AIEngine.GetCapacity <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return -1;
	return AIEngine._GetCapacity(engine_id);
}

AIEngine._GetReliability <- AIEngine.GetReliability;
AIEngine.GetReliability <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return -1;
	return AIEngine._GetReliability(engine_id);
}

AIEngine._GetMaxSpeed <- AIEngine.GetMaxSpeed;
AIEngine.GetMaxSpeed <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return -1;
	return AIEngine._GetMaxSpeed(engine_id);
}

AIEngine._GetPrice <- AIEngine.GetPrice;
AIEngine.GetPrice <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return -1;
	return AIEngine._GetPrice(engine_id);
}

AIEngine._GetMaxAge <- AIEngine.GetMaxAge;
AIEngine.GetMaxAge <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return -1;
	return AIEngine._GetMaxAge(engine_id);
}

AIEngine._GetRunningCost <- AIEngine.GetRunningCost;
AIEngine.GetRunningCost <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return -1;
	return AIEngine._GetRunningCost(engine_id);
}

AIEngine._GetPower <- AIEngine.GetPower;
AIEngine.GetPower <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return -1;
	return AIEngine._GetPower(engine_id);
}

AIEngine._GetWeight <- AIEngine.GetWeight;
AIEngine.GetWeight <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return -1;
	return AIEngine._GetWeight(engine_id);
}

AIEngine._GetMaxTractiveEffort <- AIEngine.GetMaxTractiveEffort;
AIEngine.GetMaxTractiveEffort <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return -1;
	return AIEngine._GetMaxTractiveEffort(engine_id);
}

AIEngine._GetDesignDate <- AIEngine.GetDesignDate;
AIEngine.GetDesignDate <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return -1;
	return AIEngine._GetDesignDate(engine_id);
}

AIEngine._GetVehicleType <- AIEngine.GetVehicleType;
AIEngine.GetVehicleType <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return AIVehicle.VT_INVALID;
	return AIEngine._GetVehicleType(engine_id);
}

AIEngine._IsWagon <- AIEngine.IsWagon;
AIEngine.IsWagon <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return false;
	return AIEngine._IsWagon(engine_id);
}

AIEngine._CanRunOnRail <- AIEngine.CanRunOnRail;
AIEngine.CanRunOnRail <- function(engine_id, track_rail_type)
{
	if (!AIEngine.IsBuildable(engine_id)) return false;
	return AIEngine._CanRunOnRail(engine_id, track_rail_type);
}

AIEngine._HasPowerOnRail <- AIEngine.HasPowerOnRail;
AIEngine.HasPowerOnRail <- function(engine_id, track_rail_type)
{
	if (!AIEngine.IsBuildable(engine_id)) return false;
	return AIEngine._HasPowerOnRail(engine_id, track_rail_type);
}

AIEngine._GetRoadType <- AIEngine.GetRoadType;
AIEngine.GetRoadType <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return AIRoad.ROADTYPE_INVALID;
	return AIEngine._GetRoadType(engine_id);
}

AIEngine._GetRailType <- AIEngine.GetRailType;
AIEngine.GetRailType <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return AIRail.RAILTYPE_INVALID;
	return AIEngine._GetRailType(engine_id);
}

AIEngine._IsArticulated <- AIEngine.IsArticulated;
AIEngine.IsArticulated <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return false;
	return AIEngine._IsArticulated(engine_id);
}

AIEngine._GetPlaneType <- AIEngine.GetPlaneType;
AIEngine.GetPlaneType <- function(engine_id)
{
	if (!AIEngine.IsBuildable(engine_id)) return -1;
	return AIEngine._GetPlaneType(engine_id);
}

_AIWaypointList <- AIWaypointList;
class AIWaypointList extends _AIWaypointList {
	constructor()
	{
		::_AIWaypointList.constructor(AIWaypoint.WAYPOINT_RAIL);
	}
}

AIRoad._BuildRoadStation <- AIRoad.BuildRoadStation;
AIRoad.BuildRoadStation <- function(tile, front, road_veh_type, station_id)
{
	if (AIRoad.IsRoadStationTile(tile) && AICompany.IsMine(AITile.GetOwner(tile))) return false;

	return AIRoad._BuildRoadStation(tile, front, road_veh_type, station_id);
}

AIRoad._BuildDriveThroughRoadStation <- AIRoad.BuildDriveThroughRoadStation;
AIRoad.BuildDriveThroughRoadStation <- function(tile, front, road_veh_type, station_id)
{
	if (AIRoad.IsRoadStationTile(tile) && AICompany.IsMine(AITile.GetOwner(tile))) return false;

	return AIRoad._BuildDriveThroughRoadStation(tile, front, road_veh_type, station_id);
}

AIBridgeList.HasNext <-
AIBridgeList_Length.HasNext <-
AICargoList.HasNext <-
AICargoList_IndustryAccepting.HasNext <-
AICargoList_IndustryProducing.HasNext <-
AIDepotList.HasNext <-
AIEngineList.HasNext <-
AIGroupList.HasNext <-
AIIndustryList.HasNext <-
AIIndustryList_CargoAccepting.HasNext <-
AIIndustryList_CargoProducing.HasNext <-
AIIndustryTypeList.HasNext <-
AIList.HasNext <-
AIRailTypeList.HasNext <-
AISignList.HasNext <-
AIStationList.HasNext <-
AIStationList_Vehicle.HasNext <-
AISubsidyList.HasNext <-
AITileList.HasNext <-
AITileList_IndustryAccepting.HasNext <-
AITileList_IndustryProducing.HasNext <-
AITileList_StationType.HasNext <-
AITownList.HasNext <-
AIVehicleList.HasNext <-
AIVehicleList_DefaultGroup.HasNext <-
AIVehicleList_Group.HasNext <-
AIVehicleList_SharedOrders.HasNext <-
AIVehicleList_Station.HasNext <-
AIWaypointList.HasNext <-
AIWaypointList_Vehicle.HasNext <-
function()
{
	return !this.IsEnd();
}

AIIndustry._IsCargoAccepted <- AIIndustry.IsCargoAccepted;
AIIndustry.IsCargoAccepted <- function(industry_id, cargo_id)
{
	return AIIndustry._IsCargoAccepted(industry_id, cargo_id) != AIIndustry.CAS_NOT_ACCEPTED;
}

AIAbstractList <- AIList;

AIList.ChangeItem <- AIList.SetValue;

AIRail.ERR_NONUNIFORM_STATIONS_DISABLED <- 0xFFFF;
