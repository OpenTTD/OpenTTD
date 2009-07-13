/* $Id$ */

/** @file road_map.cpp Complex road accessors. */

#include "stdafx.h"
#include "station_map.h"
#include "tunnelbridge_map.h"


RoadBits GetAnyRoadBits(TileIndex tile, RoadType rt, bool straight_tunnel_bridge_entrance)
{
	if (!HasTileRoadType(tile, rt)) return ROAD_NONE;

	switch (GetTileType(tile)) {
		case MP_ROAD:
			switch (GetRoadTileType(tile)) {
				default:
				case ROAD_TILE_NORMAL:   return GetRoadBits(tile, rt);
				case ROAD_TILE_CROSSING: return GetCrossingRoadBits(tile);
				case ROAD_TILE_DEPOT:    return DiagDirToRoadBits(GetRoadDepotDirection(tile));
			}

		case MP_STATION:
			if (!IsRoadStopTile(tile)) return ROAD_NONE;
			if (IsDriveThroughStopTile(tile)) return (GetRoadStopDir(tile) == DIAGDIR_NE) ? ROAD_X : ROAD_Y;
			return DiagDirToRoadBits(GetRoadStopDir(tile));

		case MP_TUNNELBRIDGE:
			if (GetTunnelBridgeTransportType(tile) != TRANSPORT_ROAD) return ROAD_NONE;
			return straight_tunnel_bridge_entrance ?
					AxisToRoadBits(DiagDirToAxis(GetTunnelBridgeDirection(tile))) :
					DiagDirToRoadBits(ReverseDiagDir(GetTunnelBridgeDirection(tile)));

		default: return ROAD_NONE;
	}
}
