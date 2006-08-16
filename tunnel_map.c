/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "tile.h"
#include "tunnel_map.h"
#include "vehicle.h"

TileIndex GetOtherTunnelEnd(TileIndex tile)
{
	DiagDirection dir = GetTunnelDirection(tile);
	TileIndexDiff delta = TileOffsByDir(dir);
	uint z = GetTileZ(tile);

	dir = ReverseDiagDir(dir);
	do {
		tile += delta;
	} while (
		!IsTunnelTile(tile) ||
		GetTunnelDirection(tile) != dir ||
		GetTileZ(tile) != z
	);

	return tile;
}


/** Retrieve the exit-tile of the vehicle from inside a tunnel
 * Very similar to GetOtherTunnelEnd(), but we use the vehicle's
 * direction for determining which end of the tunnel to find
 * @param v the vehicle which is inside the tunnel and needs an exit
 * @return the exit-tile of the tunnel based on the vehicle's direction */
TileIndex GetVehicleOutOfTunnelTile(const Vehicle *v)
{
	TileIndex tile = TileVirtXY(v->x_pos, v->y_pos);
	DiagDirection dir = DirToDiagDir(v->direction);
	TileIndexDiff delta = TileOffsByDir(dir);
	uint z = v->z_pos;

	dir = ReverseDiagDir(dir);
	while (
		!IsTunnelTile(tile) ||
		GetTunnelDirection(tile) != dir ||
		GetTileZ(tile) != z
	) {
		tile += delta;
	}

	return tile;
}


static bool IsTunnelInWayDir(TileIndex tile, uint z, DiagDirection dir)
{
	TileIndexDiff delta = TileOffsByDir(dir);
	uint height;

	do {
		tile -= delta;
		height = GetTileZ(tile);
	} while (z < height);

	return
		z == height &&
		IsTunnelTile(tile) &&
		GetTunnelDirection(tile) == dir;
}

bool IsTunnelInWay(TileIndex tile, uint z)
{
	return
		IsTunnelInWayDir(tile, z, DIAGDIR_NE) ||
		IsTunnelInWayDir(tile, z, DIAGDIR_SE) ||
		IsTunnelInWayDir(tile, z, DIAGDIR_SW) ||
		IsTunnelInWayDir(tile, z, DIAGDIR_NW);
}
