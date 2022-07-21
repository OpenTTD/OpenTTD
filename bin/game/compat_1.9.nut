/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

GSLog.Info("1.9 API compatibility in effect.");

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

/* 14 ensures rail can't be built or removed backwards and allows
 * 'from' and 'to' tiles to be invalid, as long as they're void tiles. */
__BuildRemoveRailHelper <- function(from, tile, to)
{
	local map_log_x = function()
	{
		local x = GSMap.GetMapSizeX();
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
		return t >= 0 && t < GSMap.GetMapSize();
	};

	/* Workaround to circumvent GetTileX which tests IsValidTile */
	local tile_x = function(t)
	{
		return t & (GSMap.GetMapSizeX() - 1);
	};

	/* Workaround to circumvent GetTileY which tests IsValidTile */
	local tile_y = function(t, maplogx = map_log_x)
	{
		return t >> maplogx();
	};

	/* Workaround to circumvent DistanceManhattan which tests IsValidTile */
	local distance_manhattan = function(t_void, t_valid, tx = tile_x, ty = tile_y)
	{
		return abs(tx(t_void) - GSMap.GetTileX(t_valid)) + abs(ty(t_void) - GSMap.GetTileY(t_valid));
	};

	local diag_offset = function(t_void, t_valid, tx = tile_x, ty = tile_y)
	{
		return abs(abs(tx(t_void) - GSMap.GetTileX(t_valid)) - abs(ty(t_void) - GSMap.GetTileY(t_valid)));
	};

	/* Run common tests to both API versions */
	if (GSCompany.ResolveCompanyID(GSCompany.COMPANY_SELF) != GSCompany.COMPANY_INVALID &&
			is_tile_inside_map(from) && GSMap.IsValidTile(tile) && is_tile_inside_map(to) &&
			distance_manhattan(from, tile) == 1 && distance_manhattan(to, tile) >= 1 &&
			GSRail.IsRailTypeAvailable(GSRail.GetCurrentRailType()) && (diag_offset(to, tile) <= 1 ||
			(tile_x(from) == GSMap.GetTileX(tile) && GSMap.GetTileX(tile) == tile_x(to)) ||
			(tile_y(from) == GSMap.GetTileY(tile) && GSMap.GetTileY(tile) == tile_y(to)))) {
		/* Run tests which differ from version 14 */
		if (GSMap.IsValidTile(from) && GSMap.IsValidTile(to)) {
			if (from == to ||
					(GSMap.GetTileX(from) == GSMap.GetTileX(tile) && GSMap.GetTileX(tile) == GSMap.GetTileX(to) &&
					GSMap.GetTileY(from) - GSMap.GetTileY(tile) == -(GSMap.GetTileY(tile) - GSMap.GetTileY(to)) / abs(GSMap.GetTileY(tile) - GSMap.GetTileY(to))) ||
					(GSMap.GetTileY(from) == GSMap.GetTileY(tile) && GSMap.GetTileY(tile) == GSMap.GetTileY(to) &&
					GSMap.GetTileX(from) - GSMap.GetTileX(tile) == -(GSMap.GetTileX(tile) - GSMap.GetTileX(to)) / abs(GSMap.GetTileX(tile) - GSMap.GetTileX(to)))) {
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

GSRail._BuildRail <- GSRail.BuildRail
GSRail.BuildRail <- function(from, tile, to)
{
	return GSRail._BuildRail(__BuildRemoveRailHelper(from, tile, to), tile, to);
}

GSRail._RemoveRail <- GSRail.RemoveRail
GSRail.RemoveRail <- function(from, tile, to)
{
	return GSRail._RemoveRail(__BuildRemoveRailHelper(from, tile, to), tile, to);
}
