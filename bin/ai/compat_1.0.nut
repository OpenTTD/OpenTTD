/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/* This file contains code to downgrade the API from 1.1 to 1.0. */

AIRoad.BuildRoadStationCompat1_0 <- AIRoad.BuildRoadStation;
AIRoad.BuildRoadStation <- function(tile, front, road_veh_type, station_id)
{
	if (AIRoad.IsRoadStationTile(tile) && AICompany.IsMine(AITile.GetOwner(tile))) return false;

	return AIRoad.BuildRoadStationCompat1_0(tile, front, road_veh_type, station_id);
}

AIRoad.BuildDriveThroughRoadStationCompat1_0 <- AIRoad.BuildDriveThroughRoadStation;
AIRoad.BuildDriveThroughRoadStation <- function(tile, front, road_veh_type, station_id)
{
	if (AIRoad.IsRoadStationTile(tile) && AICompany.IsMine(AITile.GetOwner(tile))) return false;

	return AIRoad.BuildDriveThroughRoadStationCompat1_0(tile, front, road_veh_type, station_id);
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
AIVehicleList_Depot.HasNext <-
AIVehicleList_Group.HasNext <-
AIVehicleList_SharedOrders.HasNext <-
AIVehicleList_Station.HasNext <-
AIWaypointList.HasNext <-
AIWaypointList_Vehicle.HasNext <-
function()
{
	return !this.IsEnd();
}

AIIndustry.IsCargoAcceptedCompat1_0 <- AIIndustry.IsCargoAccepted;
AIIndustry.IsCargoAccepted <- function(industry_id, cargo_id)
{
	return AIIndustry.IsCargoAcceptedCompat1_0(industry_id, cargo_id) != AIIndustry.CAS_NOT_ACCEPTED;
}

AIAbstractList <- AIList;

AIList.ChangeItem <- AIList.SetValue;

AIRail.ERR_NONUNIFORM_STATIONS_DISABLED <- 0xFFFF;
