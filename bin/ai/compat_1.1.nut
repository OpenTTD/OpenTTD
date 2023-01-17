/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

AILog.Info("1.1 API compatibility in effect.");

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
AIEvent.AI_ET_COMPANY_ASK_MERGER <- AIEvent.ET_COMPANY_ASK_MERGER;
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
AIEvent.AI_ET_TOWN_FOUNDED <- AIEvent.ET_TOWN_FOUNDED;
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

/* 14 Rand no longer returns negative values */
AIBase._Rand <- AIBase.Rand;
AIBase.Rand <- function()
{
	local r = AIBase._Rand();
	return (r & 1 << 31) != 0 ? r | ~((1 << 31) - 1) : r;
}

/* 14 RandItem no longer returns negative values */
AIBase.RandItem <- function(unused_param)
{
	return AIBase.Rand();
}

/* 14 RandRange no longer returns negative values */
AIBase._RandRange <- AIBase.RandRange
AIBase.RandRange <- function(max)
{
	local r = AIBase._RandRange(max);
	return (r & 1 << 31) != 0 ? r | ~((1 << 31) - 1) : r;
}

/* 14 RandRangeItem no longer returns negative values */
AIBase.RandRangeItem <- function(unused_param, max)
{
	return AIBase.RandRange(max);
}

/* 14 Chance no longer compares against randomized negative values */
AIBase._Chance <- AIBase.Chance
AIBase.Chance <- function(out, max)
{
	if (out > max) return AIBase._Chance(out, max);
	return AIBase.RandRange(max) < out;
}

/* 14 ChanceItem no longer compares against randomized negative values */
AIBase.ChanceItem <- function(unused_param, out, max)
{
	return AIBase.Chance(out, max);
}

/* 14 airport maintenance cost factor returns -1 for unavailable airports */
AIAirport._GetMaintenanceCostFactor <- AIAirport.GetMaintenanceCostFactor
AIAirport.GetMaintenanceCostFactor <- function(type)
{
	if (!AIAirport.IsAirportInformationAvailable(type)) return 0xFFFF;
	return AIAirport._GetMaintenanceCostFactor(type);
}

/* 14 railtype maintenance cost factor returns -1 for unavailable railtypes */
AIRail._GetMaintenanceCostFactor <- AIRail.GetMaintenanceCostFactor
AIRail.GetMaintenanceCostFactor <- function(railtype)
{
	if (!AIRail.IsRailTypeAvailable(railtype)) return 0;
	return AIRail._GetMaintenanceCostFactor(railtype);
}

/* 14 roadtype maintenance cost factor returns -1 for unavailable roadtypes */
AIRoad._GetMaintenanceCostFactor <- AIRoad.GetMaintenanceCostFactor
AIRoad.GetMaintenanceCostFactor <- function(roadtype)
{
	if (!AIRoad.IsRoadTypeAvailable(roadtype)) return 0;
	return AIRoad._GetMaintenanceCostFactor(roadtype);
}

/* 14 returns a max speed of -1 for unavailable road types */
AIRoad._GetMaxSpeed <- AIRoad.GetMaxSpeed
AIRoad.GetMaxSpeed <- function(road_type)
{
	if (!AIRoad.IsRoadTypeAvailable(road_type)) return 0;
	return AIRoad._GetMaxSpeed(road_type);
}

/* 14 returns a distance of -1 for invalid engines */
AIEngine._GetMaximumOrderDistance <- AIEngine.GetMaximumOrderDistance
AIEngine.GetMaximumOrderDistance <- function (engine_id)
{
	if (!AIEngine.IsValidEngine(engine_id)) return 0;
	return AIEngine._GetMaximumOrderDistance(engine_id);
}
