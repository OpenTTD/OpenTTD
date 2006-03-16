/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "bridge_map.h"
#include "functions.h"
#include "road_map.h"
#include "station.h"
#include "tunnel_map.h"


RoadBits GetAnyRoadBits(TileIndex tile)
{
	switch (GetTileType(tile)) {
		case MP_STREET:
			switch (GetRoadType(tile)) {
				default:
				case ROAD_NORMAL:   return GetRoadBits(tile);
				case ROAD_CROSSING: return GetCrossingRoadBits(tile);
				case ROAD_DEPOT:    return DiagDirToRoadBits(GetRoadDepotDirection(tile));
			}

		case MP_STATION:
			if (!IsRoadStationTile(tile)) return 0;
			return DiagDirToRoadBits(GetRoadStationDir(tile));

		case MP_TUNNELBRIDGE:
			if (IsBridge(tile)) {
				if (IsBridgeMiddle(tile)) {
					if (!IsTransportUnderBridge(tile) ||
							GetBridgeTransportType(tile) != TRANSPORT_ROAD) {
						return 0;
					}
					return GetRoadBitsUnderBridge(tile);
				} else {
					// ending
					if (GetBridgeTransportType(tile) != TRANSPORT_ROAD) return 0;
					return DiagDirToRoadBits(ReverseDiagDir(GetBridgeRampDirection(tile)));
				}
			} else {
				// tunnel
				if (GetTunnelTransportType(tile) != TRANSPORT_ROAD) return 0;
				return DiagDirToRoadBits(ReverseDiagDir(GetTunnelDirection(tile)));
			}

		default: return 0;
	}
}


TrackBits GetAnyRoadTrackBits(TileIndex tile)
{
	uint32 r = GetTileTrackStatus(tile, TRANSPORT_ROAD);
	return (byte)(r | (r >> 8));
}
