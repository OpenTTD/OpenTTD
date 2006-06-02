/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "bridge_map.h"
#include "functions.h"
#include "road_map.h"
#include "station.h"
#include "tunnel_map.h"
#include "station_map.h"
#include "depot.h"


RoadBits GetAnyRoadBits(TileIndex tile)
{
	switch (GetTileType(tile)) {
		case MP_STREET:
			switch (GetRoadTileType(tile)) {
				default:
				case ROAD_TILE_NORMAL:   return GetRoadBits(tile);
				case ROAD_TILE_CROSSING: return GetCrossingRoadBits(tile);
				case ROAD_TILE_DEPOT:    return DiagDirToRoadBits(GetRoadDepotDirection(tile));
			}

		case MP_STATION:
			if (!IsRoadStopTile(tile)) return 0;
			return DiagDirToRoadBits(GetRoadStopDir(tile));

		case MP_TUNNELBRIDGE:
			if (IsTunnel(tile)) {
				if (GetTunnelTransportType(tile) != TRANSPORT_ROAD) return 0;
				return DiagDirToRoadBits(ReverseDiagDir(GetTunnelDirection(tile)));
			} else {
				if (GetBridgeTransportType(tile) != TRANSPORT_ROAD) return 0;
				return DiagDirToRoadBits(ReverseDiagDir(GetBridgeRampDirection(tile)));
			}

		default: return 0;
	}
}


TrackBits GetAnyRoadTrackBits(TileIndex tile)
{
	if (IsTileType(tile, MP_STREET) && !IsTileDepotType(tile, TRANSPORT_ROAD)) {
		uint32 r = GetTileTrackStatus(tile, TRANSPORT_ROAD);
		return (byte)(r | (r >> 8));
	}
	return 0;
}
