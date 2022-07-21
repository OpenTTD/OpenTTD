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
