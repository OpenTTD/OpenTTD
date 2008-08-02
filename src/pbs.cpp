/* $Id$ */

/** @file pbs.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "pbs.h"
#include "rail_map.h"
#include "road_map.h"
#include "station_map.h"
#include "tunnelbridge_map.h"

/**
 * Get the reserved trackbits for any tile, regardless of type.
 * @param t the tile
 * @return the reserved trackbits. TRACK_BIT_NONE on nothing reserved or
 *     a tile without rail.
 */
TrackBits GetReservedTrackbits(TileIndex t)
{
	switch (GetTileType(t)) {
		case MP_RAILWAY:
			if (IsRailWaypoint(t) || IsRailDepot(t)) return GetRailWaypointReservation(t);
			if (IsPlainRailTile(t)) return GetTrackReservation(t);
			break;

		case MP_ROAD:
			if (IsLevelCrossing(t)) return GetRailCrossingReservation(t);
			break;

		case MP_STATION:
			if (IsRailwayStation(t)) return GetRailStationReservation(t);
			break;

		case MP_TUNNELBRIDGE:
			if (GetTunnelBridgeTransportType(t) == TRANSPORT_RAIL) return GetRailTunnelBridgeReservation(t);
			break;

		default:
			break;
	}
	return TRACK_BIT_NONE;
}
