/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

AILog.Info("0.7 API compatibility in effect:");
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

AICompany.GetCompanyValue <- function(company)
{
	return AICompany.GetQuarterlyCompanyValue(company, AICompany.CURRENT_QUARTER);
}

AITown.GetLastMonthTransported <- AITown.GetLastMonthSupplied;

AIEvent.AI_ET_INVALID <- AIEvent.ET_INVALID;
AIEvent.AI_ET_TEST <- AIEvent.ET_TEST;
AIEvent.AI_ET_SUBSIDY_OFFER <- AIEvent.ET_SUBSIDY_OFFER;
AIEvent.AI_ET_SUBSIDY_OFFER_EXPIRED <- AIEvent.ET_SUBSIDY_OFFER_EXPIRED;
AIEvent.AI_ET_SUBSIDY_AWARDED <- AIEvent.ET_SUBSIDY_AWARDED;
AIEvent.AI_ET_SUBSIDY_EXPIRED <- AIEvent.ET_SUBSIDY_EXPIRED;
AIEvent.AI_ET_ENGINE_PREVIEW <- AIEvent.ET_ENGINE_PREVIEW;
AIEvent.AI_ET_COMPANY_NEW <- AIEvent.ET_COMPANY_NEW;
AIEvent.AI_ET_COMPANY_IN_TROUBLE <- AIEvent.ET_COMPANY_IN_TROUBLE;
AIEvent.AI_ET_COMPANY_MERGER <- AIEvent.ET_COMPANY_MERGER;
AIEvent.AI_ET_COMPANY_BANKRUPT <- AIEvent.ET_COMPANY_BANKRUPT;
AIEvent.AI_ET_VEHICLE_CRASHED <- AIEvent.ET_VEHICLE_CRASHED;
AIEvent.AI_ET_VEHICLE_LOST <- AIEvent.ET_VEHICLE_LOST;
AIEvent.AI_ET_VEHICLE_WAITING_IN_DEPOT <- AIEvent.ET_VEHICLE_WAITING_IN_DEPOT;
AIEvent.AI_ET_VEHICLE_UNPROFITABLE <- AIEvent.ET_VEHICLE_UNPROFITABLE;
AIEvent.AI_ET_INDUSTRY_OPEN <- AIEvent.ET_INDUSTRY_OPEN;
AIEvent.AI_ET_INDUSTRY_CLOSE <- AIEvent.ET_INDUSTRY_CLOSE;
AIEvent.AI_ET_ENGINE_AVAILABLE <- AIEvent.ET_ENGINE_AVAILABLE;
AIEvent.AI_ET_STATION_FIRST_VEHICLE <- AIEvent.ET_STATION_FIRST_VEHICLE;
AIEvent.AI_ET_DISASTER_ZEPPELINER_CRASHED <- AIEvent.ET_DISASTER_ZEPPELINER_CRASHED;
AIEvent.AI_ET_DISASTER_ZEPPELINER_CLEARED <- AIEvent.ET_DISASTER_ZEPPELINER_CLEARED;
AIOrder.AIOF_NONE <- AIOrder.OF_NONE
AIOrder.AIOF_NON_STOP_INTERMEDIATE <- AIOrder.OF_NON_STOP_INTERMEDIATE
AIOrder.AIOF_NON_STOP_DESTINATION <- AIOrder.OF_NON_STOP_DESTINATION
AIOrder.AIOF_UNLOAD <- AIOrder.OF_UNLOAD
AIOrder.AIOF_TRANSFER <- AIOrder.OF_TRANSFER
AIOrder.AIOF_NO_UNLOAD <- AIOrder.OF_NO_UNLOAD
AIOrder.AIOF_FULL_LOAD <- AIOrder.OF_FULL_LOAD
AIOrder.AIOF_FULL_LOAD_ANY <- AIOrder.OF_FULL_LOAD_ANY
AIOrder.AIOF_NO_LOAD <- AIOrder.OF_NO_LOAD
AIOrder.AIOF_SERVICE_IF_NEEDED <- AIOrder.OF_SERVICE_IF_NEEDED
AIOrder.AIOF_STOP_IN_DEPOT <- AIOrder.OF_STOP_IN_DEPOT
AIOrder.AIOF_GOTO_NEAREST_DEPOT <- AIOrder.OF_GOTO_NEAREST_DEPOT
AIOrder.AIOF_NON_STOP_FLAGS <- AIOrder.OF_NON_STOP_FLAGS
AIOrder.AIOF_UNLOAD_FLAGS <- AIOrder.OF_UNLOAD_FLAGS
AIOrder.AIOF_LOAD_FLAGS <- AIOrder.OF_LOAD_FLAGS
AIOrder.AIOF_DEPOT_FLAGS <- AIOrder.OF_DEPOT_FLAGS
AIOrder.AIOF_INVALID <- AIOrder.OF_INVALID

/* 1.9 adds a vehicle type parameter. */
AIBridge._GetName <- AIBridge.GetName;
AIBridge.GetName <- function(bridge_id)
{
	return AIBridge._GetName(bridge_id, AIVehicle.VT_RAIL);
}

/* 1.9 adds parent_group_id to CreateGroup function */
AIGroup._CreateGroup <- AIGroup.CreateGroup;
AIGroup.CreateGroup <- function(vehicle_type)
{
	return AIGroup._CreateGroup(vehicle_type, AIGroup.GROUP_INVALID);
}

/* 13 really checks RoadType against RoadType */
AIRoad._HasRoadType <- AIRoad.HasRoadType;
AIRoad.HasRoadType <- function(tile, road_type)
{
	local list = AIRoadTypeList(AIRoad.GetRoadTramType(road_type));
	foreach (rt, _ in list) {
		if (AIRoad._HasRoadType(tile, rt)) {
			return true;
		}
	}
	return false;
}

/* 14 ensures rail can't be built or removed backwards and allows
 * 'from' and 'to' tiles to be invalid, as long as they're void tiles. */
__BuildRemoveRailHelper <- function(from, tile, to)
{
	local map_log_x = function()
	{
		local x = AIMap.GetMapSizeX();
		local pos = 0;

		if ((x & 0xFFFFFFFF) == 0) { x = x >> 32; pos += 32; }
		if ((x & 0x0000FFFF) == 0) { x = x >> 16; pos += 16; }
		if ((x & 0x000000FF) == 0) { x = x >> 8;  pos += 8;  }
		if ((x & 0x0000000F) == 0) { x = x >> 4;  pos += 4;  }
		if ((x & 0x00000003) == 0) { x = x >> 2;  pos += 2;  }
		if ((x & 0x00000001) == 0) { pos += 1; }

		return pos;
	};

	/* Workaround to circumvent IsValidTile. We want to give a pass to void tiles */
	local is_tile_inside_map = function(t)
	{
		return t >= 0 && t < AIMap.GetMapSize();
	};

	/* Workaround to circumvent GetTileX which tests IsValidTile */
	local tile_x = function(t)
	{
		return t & (AIMap.GetMapSizeX() - 1);
	};

	/* Workaround to circumvent GetTileY which tests IsValidTile */
	local tile_y = function(t, maplogx = map_log_x)
	{
		return t >> maplogx();
	};

	/* Workaround to circumvent DistanceManhattan which tests IsValidTile */
	local distance_manhattan = function(t_void, t_valid, tx = tile_x, ty = tile_y)
	{
		return abs(tx(t_void) - AIMap.GetTileX(t_valid)) + abs(ty(t_void) - AIMap.GetTileY(t_valid));
	};

	local diag_offset = function(t_void, t_valid, tx = tile_x, ty = tile_y)
	{
		return abs(abs(tx(t_void) - AIMap.GetTileX(t_valid)) - abs(ty(t_void) - AIMap.GetTileY(t_valid)));
	};

	/* Run common tests to both API versions */
	if (AICompany.ResolveCompanyID(AICompany.COMPANY_SELF) != AICompany.COMPANY_INVALID &&
			is_tile_inside_map(from) && AIMap.IsValidTile(tile) && is_tile_inside_map(to) &&
			distance_manhattan(from, tile) == 1 && distance_manhattan(to, tile) >= 1 &&
			AIRail.IsRailTypeAvailable(AIRail.GetCurrentRailType()) && (diag_offset(to, tile) <= 1 ||
			(tile_x(from) == AIMap.GetTileX(tile) && AIMap.GetTileX(tile) == tile_x(to)) ||
			(tile_y(from) == AIMap.GetTileY(tile) && AIMap.GetTileY(tile) == tile_y(to)))) {
		/* Run tests which differ from version 14 */
		if (AIMap.IsValidTile(from) && AIMap.IsValidTile(to)) {
			if (from == to ||
					(AIMap.GetTileX(from) == AIMap.GetTileX(tile) && AIMap.GetTileX(tile) == AIMap.GetTileX(to) &&
					AIMap.GetTileY(from) - AIMap.GetTileY(tile) == -(AIMap.GetTileY(tile) - AIMap.GetTileY(to)) / abs(AIMap.GetTileY(tile) - AIMap.GetTileY(to))) ||
					(AIMap.GetTileY(from) == AIMap.GetTileY(tile) && AIMap.GetTileY(tile) == AIMap.GetTileY(to) &&
					AIMap.GetTileX(from) - AIMap.GetTileX(tile) == -(AIMap.GetTileX(tile) - AIMap.GetTileX(to)) / abs(AIMap.GetTileX(tile) - AIMap.GetTileX(to)))) {
				/* Adjust 'from' to simulate pre-14 behaviour */
				from = tile + (tile - from);
			}
		} else {
			/* Provoke a precondition fail to simulate pre-14 behaviour */
			from = tile;
		}
	}
	return from;
}

AIRail._BuildRail <- AIRail.BuildRail
AIRail.BuildRail <- function(from, tile, to)
{
	return AIRail._BuildRail(__BuildRemoveRailHelper(from, tile, to), tile, to);
}

AIRail._RemoveRail <- AIRail.RemoveRail
AIRail.RemoveRail <- function(from, tile, to)
{
	return AIRail._RemoveRail(__BuildRemoveRailHelper(from, tile, to), tile, to);
}
