/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "tile.h"
#include "tunnel_map.h"

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
