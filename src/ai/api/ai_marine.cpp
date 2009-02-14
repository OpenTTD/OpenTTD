/* $Id$ */

/** @file ai_marine.cpp Implementation of AIMarine. */

#include "ai_marine.hpp"
#include "ai_station.hpp"
#include "../../station_map.h"
#include "../../tile_cmd.h"


/* static */ bool AIMarine::IsWaterDepotTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return ::IsTileType(tile, MP_WATER) && ::GetWaterTileType(tile) == WATER_TILE_DEPOT;
}

/* static */ bool AIMarine::IsDockTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return ::IsTileType(tile, MP_STATION) && ::IsDock(tile);
}

/* static */ bool AIMarine::IsBuoyTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return ::IsTileType(tile, MP_STATION) && ::IsBuoy(tile);
}

/* static */ bool AIMarine::IsLockTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return ::IsTileType(tile, MP_WATER) && ::GetWaterTileType(tile) == WATER_TILE_LOCK;
}

/* static */ bool AIMarine::IsCanalTile(TileIndex tile)
{
	if (!::IsValidTile(tile)) return false;

	return ::IsTileType(tile, MP_WATER) && ::IsCanal(tile);
}

/* static */ bool AIMarine::AreWaterTilesConnected(TileIndex t1, TileIndex t2)
{
	if (!::IsValidTile(t1)) return false;
	if (!::IsValidTile(t2)) return false;

	/* Tiles not neighbouring */
	if (::DistanceManhattan(t1, t2) != 1) return false;
	if (t1 > t2) Swap(t1, t2);

	DiagDirection to_other_tile = (TileX(t1) == TileX(t2)) ? DIAGDIR_SE : DIAGDIR_SW;

	/* Determine the reachable tracks from the shared edge */
	TrackBits gtts2 = ::TrackStatusToTrackBits(::GetTileTrackStatus(t2, TRANSPORT_WATER, 0, to_other_tile)) & ::DiagdirReachesTracks(to_other_tile);
	if (gtts2 == TRACK_BIT_NONE) return false;

	to_other_tile = ReverseDiagDir(to_other_tile);
	TrackBits gtts1 = ::TrackStatusToTrackBits(::GetTileTrackStatus(t1, TRANSPORT_WATER, 0, to_other_tile)) & ::DiagdirReachesTracks(to_other_tile);

	return gtts1 != TRACK_BIT_NONE;
}

/* static */ bool AIMarine::BuildWaterDepot(TileIndex tile, TileIndex front)
{
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, ::IsValidTile(front));
	EnforcePrecondition(false, (::TileX(front) == ::TileX(tile)) != (::TileY(front) == ::TileY(tile)));

	return AIObject::DoCommand(tile, ::TileY(front) == ::TileY(tile), 0, CMD_BUILD_SHIP_DEPOT);
}

/* static */ bool AIMarine::BuildDock(TileIndex tile, StationID station_id)
{
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, station_id == AIStation::STATION_NEW || station_id == AIStation::STATION_JOIN_ADJACENT || AIStation::IsValidStation(station_id));

	uint p1 = station_id == AIStation::STATION_JOIN_ADJACENT ? 0 : 1;
	uint p2 = (AIStation::IsValidStation(station_id) ? station_id : INVALID_STATION) << 16;
	return AIObject::DoCommand(tile, p1, p2, CMD_BUILD_DOCK);
}

/* static */ bool AIMarine::BuildBuoy(TileIndex tile)
{
	EnforcePrecondition(false, ::IsValidTile(tile));

	return AIObject::DoCommand(tile, 0, 0, CMD_BUILD_BUOY);
}

/* static */ bool AIMarine::BuildLock(TileIndex tile)
{
	EnforcePrecondition(false, ::IsValidTile(tile));

	return AIObject::DoCommand(tile, 0, 0, CMD_BUILD_LOCK);
}

/* static */ bool AIMarine::BuildCanal(TileIndex tile)
{
	EnforcePrecondition(false, ::IsValidTile(tile));

	return AIObject::DoCommand(tile, tile, 0, CMD_BUILD_CANAL);
}

/* static */ bool AIMarine::RemoveWaterDepot(TileIndex tile)
{
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, IsWaterDepotTile(tile));

	return AIObject::DoCommand(tile, 0, 0, CMD_LANDSCAPE_CLEAR);
}

/* static */ bool AIMarine::RemoveDock(TileIndex tile)
{
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, IsDockTile(tile));

	return AIObject::DoCommand(tile, 0, 0, CMD_LANDSCAPE_CLEAR);
}

/* static */ bool AIMarine::RemoveBuoy(TileIndex tile)
{
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, IsBuoyTile(tile));

	return AIObject::DoCommand(tile, 0, 0, CMD_LANDSCAPE_CLEAR);
}

/* static */ bool AIMarine::RemoveLock(TileIndex tile)
{
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, IsLockTile(tile));

	return AIObject::DoCommand(tile, 0, 0, CMD_LANDSCAPE_CLEAR);
}

/* static */ bool AIMarine::RemoveCanal(TileIndex tile)
{
	EnforcePrecondition(false, ::IsValidTile(tile));
	EnforcePrecondition(false, IsCanalTile(tile));

	return AIObject::DoCommand(tile, 0, 0, CMD_LANDSCAPE_CLEAR);
}
