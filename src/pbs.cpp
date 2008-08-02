/* $Id$ */

/** @file pbs.cpp */
#include "stdafx.h"
#include "openttd.h"
#include "pbs.h"
#include "rail_map.h"
#include "road_map.h"
#include "station_map.h"
#include "tunnelbridge_map.h"
#include "functions.h"
#include "debug.h"
#include "direction_func.h"
#include "settings_type.h"

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

/**
 * Set the reservation for a complete station platform.
 * @pre IsRailwayStationTile(start)
 * @param start starting tile of the platform
 * @param dir the direction in which to follow the platform
 * @param b the state the reservation should be set to
 */
void SetRailwayStationPlatformReservation(TileIndex start, DiagDirection dir, bool b)
{
	TileIndex     tile = start;
	TileIndexDiff diff = TileOffsByDiagDir(dir);

	assert(IsRailwayStationTile(start));
	assert(GetRailStationAxis(start) == DiagDirToAxis(dir));

	do {
		SetRailwayStationReservation(tile, b);
		if (_settings_client.gui.show_track_reservation) MarkTileDirtyByTile(tile);
		tile = TILE_ADD(tile, diff);
	} while (IsCompatibleTrainStationTile(tile, start));
}

/**
 * Try to reserve a specific track on a tile
 * @param tile the tile
 * @param t the track
 * @return true if reservation was successfull, i.e. the track was
 *     free and didn't cross any other reserved tracks.
 */
bool TryReserveRailTrack(TileIndex tile, Track t)
{
	assert((GetTileTrackStatus(tile, TRANSPORT_RAIL, 0) & TrackToTrackBits(t)) != 0);

	if (_settings_client.gui.show_track_reservation) {
		MarkTileDirtyByTile(tile);
	}

	switch (GetTileType(tile)) {
		case MP_RAILWAY:
			if (IsPlainRailTile(tile)) return TryReserveTrack(tile, t);
			if (IsRailWaypoint(tile) || IsRailDepot(tile)) {
				if (!GetDepotWaypointReservation(tile)) {
					SetDepotWaypointReservation(tile, true);
					return true;
				}
			}
			break;

		case MP_ROAD:
			if (IsLevelCrossing(tile) && !GetCrossingReservation(tile)) {
				SetCrossingReservation(tile, true);
				return true;
			}
			break;

		case MP_STATION:
			if (IsRailwayStation(tile) && !GetRailwayStationReservation(tile)) {
				SetRailwayStationReservation(tile, true);
				return true;
			}
			break;

		case MP_TUNNELBRIDGE:
			if (GetTunnelBridgeTransportType(tile) == TRANSPORT_RAIL && !GetRailTunnelBridgeReservation(tile)) {
				SetTunnelBridgeReservation(tile, true);
				return true;
			}
			break;

		default:
			break;
	}
	return false;
}

/**
 * Lift the reservation of a specific track on a tile
 * @param tile the tile
 * @param t the track
 */
 void UnreserveRailTrack(TileIndex tile, Track t)
{
	assert((GetTileTrackStatus(tile, TRANSPORT_RAIL, 0) & TrackToTrackBits(t)) != 0);

	if (_settings_client.gui.show_track_reservation) {
		MarkTileDirtyByTile(tile);
	}

	switch (GetTileType(tile)) {
		case MP_RAILWAY:
			if (IsRailWaypoint(tile) || IsRailDepot(tile)) SetDepotWaypointReservation(tile, false);
			if (IsPlainRailTile(tile)) UnreserveTrack(tile, t);
			break;

		case MP_ROAD:
			if (IsLevelCrossing(tile)) SetCrossingReservation(tile, false);
			break;

		case MP_STATION:
			if (IsRailwayStation(tile)) SetRailwayStationReservation(tile, false);
			break;

		case MP_TUNNELBRIDGE:
			if (GetTunnelBridgeTransportType(tile) == TRANSPORT_RAIL) SetTunnelBridgeReservation(tile, false);
			break;

		default:
			break;
	}
}
