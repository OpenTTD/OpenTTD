/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "tile.h"
#include "tunnel_map.h"

TileIndex GetOtherTunnelEnd(TileIndex tile)
{
	DiagDirection dir = GetTunnelDirection(tile);
	TileIndexDiff delta = TileOffsByDiagDir(dir);
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
