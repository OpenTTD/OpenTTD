/* $Id$ */

/** @file tunnel_map.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "tile.h"
#include "tunnel_map.h"
#include "tunnelbridge_map.h"


/**
 * Gets the other end of the tunnel. Where a vehicle would reappear when it
 * enters at the given tile.
 * @param tile the tile to search from.
 * @return the tile of the other end of the tunnel.
 */
TileIndex GetOtherTunnelEnd(TileIndex tile)
{
	DiagDirection dir = GetTunnelBridgeDirection(tile);
	TileIndexDiff delta = TileOffsByDiagDir(dir);
	uint z = GetTileZ(tile);

	dir = ReverseDiagDir(dir);
	do {
		tile += delta;
	} while (
		!IsTunnelTile(tile) ||
		GetTunnelBridgeDirection(tile) != dir ||
		GetTileZ(tile) != z
	);

	return tile;
}


/**
 * Is there a tunnel in the way in the given direction?
 * @param tile the tile to search from.
 * @param z    the 'z' to search on.
 * @param dir  the direction to start searching to.
 * @return true if and only if there is a tunnel.
 */
bool IsTunnelInWayDir(TileIndex tile, uint z, DiagDirection dir)
{
	TileIndexDiff delta = TileOffsByDiagDir(dir);
	uint height;

	do {
		tile -= delta;
		height = GetTileZ(tile);
	} while (z < height);

	return
		z == height &&
		IsTunnelTile(tile) &&
		GetTunnelBridgeDirection(tile) == dir;
}

/**
 * Is there a tunnel in the way in any direction?
 * @param tile the tile to search from.
 * @param z the 'z' to search on.
 * @return true if and only if there is a tunnel.
 */
bool IsTunnelInWay(TileIndex tile, uint z)
{
	return
		IsTunnelInWayDir(tile, z, (TileX(tile) > (MapMaxX() / 2)) ? DIAGDIR_NE : DIAGDIR_SW) ||
		IsTunnelInWayDir(tile, z, (TileY(tile) > (MapMaxY() / 2)) ? DIAGDIR_NW : DIAGDIR_SE);
}
