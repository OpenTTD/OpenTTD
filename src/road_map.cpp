/* $Id$ */

/** @file road_map.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "bridge_map.h"
#include "functions.h"
#include "landscape.h"
#include "road_map.h"
#include "station.h"
#include "tunnel_map.h"
#include "station_map.h"
#include "depot.h"


RoadBits GetAnyRoadBits(TileIndex tile, RoadType rt)
{
	if (!HASBIT(GetRoadTypes(tile), rt)) return ROAD_NONE;

	switch (GetTileType(tile)) {
		case MP_STREET:
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
			if (IsTunnel(tile)) {
				if (GetTunnelTransportType(tile) != TRANSPORT_ROAD) return ROAD_NONE;
				return DiagDirToRoadBits(ReverseDiagDir(GetTunnelDirection(tile)));
			} else {
				if (GetBridgeTransportType(tile) != TRANSPORT_ROAD) return ROAD_NONE;
				return DiagDirToRoadBits(ReverseDiagDir(GetBridgeRampDirection(tile)));
			}

		default: return ROAD_NONE;
	}
}


TrackBits GetAnyRoadTrackBits(TileIndex tile, RoadType rt)
{
	uint32 r;

	/* Don't allow local authorities to build roads through road depots or road stops. */
	if ((IsTileType(tile, MP_STREET) && IsTileDepotType(tile, TRANSPORT_ROAD)) || (IsTileType(tile, MP_STATION) && !IsDriveThroughStopTile(tile)) || !HASBIT(GetRoadTypes(tile), rt)) {
		return TRACK_BIT_NONE;
	}

	r = GetTileTrackStatus(tile, TRANSPORT_ROAD, RoadTypeToRoadTypes(rt));

	return (TrackBits)(byte)(r | (r >> 8));
}
