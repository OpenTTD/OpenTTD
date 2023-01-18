/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

AILog.Info("1.7 API compatibility in effect.");

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

/* 14 returns a distance of -1 for invalid vehicles */
AIVehicle._GetMaximumOrderDistance <- AIVehicle.GetMaximumOrderDistance
AIVehicle.GetMaximumOrderDistance <- function(vehicle_id)
{
	if (!AIVehicle.IsValidVehicle(vehicle_id)) return 0;
	return AIVehicle._GetMaximumOrderDistance(vehicle_id);
}

/* 14 returns -1 for invalid towns and invalid town effect ids */
AITown._GetCargoGoal <- AITown.GetCargoGoal
AITown.GetCargoGoal <- function(town_id, towneffect_id)
{
	if (!AITown.IsValidTown(town_id)) return 0xFFFFFFFF;
	if (!AICargo.IsValidTownEffect(towneffect_id)) return 0xFFFFFFFF;
	return AITown._GetCargoGoal(town_id, towneffect_id);
}

/* 14 tests whether the vehicle type is valid */
AIOrder._GetOrderDistance <- AIOrder.GetOrderDistance
AIOrder.GetOrderDistance <- function(vehicle_type, origin_tile, dest_tile)
{
	if (vehicle_type < AIVehicle.VT_RAIL || vehicle_type > AIVehicle.VT_AIR) return AIMap.DistanceManhattan(origin_tile, dest_tile);
	return AIOrder._GetOrderDistance(vehicle_type, origin_tile, dest_tile);
}
