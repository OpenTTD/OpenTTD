/* $Id$ */

/** @file ai_marine.cpp Implementation of AIMarine. */

#include "ai_marine.hpp"
#include "../../openttd.h"
#include "../../command_type.h"
#include "../../variables.h"
#include "../../station_map.h"
#include "../../water_map.h"
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

	uint32 gtts1 = ::GetTileTrackStatus(t1, TRANSPORT_WATER, 0);
	uint32 gtts2 = ::GetTileTrackStatus(t2, TRANSPORT_WATER, 0);

	/* Ship can't travel on one of the tiles. */
	if (gtts1 == 0 || gtts2 == 0) return false;

	DiagDirection to_other_tile = (TileX(t1) == TileX(t2)) ? DIAGDIR_SE : DIAGDIR_SW;

	/* Check whether we can 'leave' the tile at the border and 'enter' the other tile at the border */
	return (gtts1 & DiagdirReachesTrackdirs(ReverseDiagDir(to_other_tile))) != 0 && (gtts2 & DiagdirReachesTrackdirs(to_other_tile)) != 0;
}

/* static */ bool AIMarine::BuildWaterDepot(TileIndex tile, bool vertical)
{
	EnforcePrecondition(false, ::IsValidTile(tile));

	return AIObject::DoCommand(tile, vertical, 0, CMD_BUILD_SHIP_DEPOT);
}

/* static */ bool AIMarine::BuildDock(TileIndex tile, bool join_adjacent)
{
	EnforcePrecondition(false, ::IsValidTile(tile));

	return AIObject::DoCommand(tile, join_adjacent ? 0 : 1, INVALID_STATION << 16, CMD_BUILD_DOCK);
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
