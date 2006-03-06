/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "tile.h"
#include "tunnel_map.h"

TileIndex GetOtherTunnelEnd(TileIndex tile)
{
	DiagDirection dir = GetTunnelDirection(tile);
	TileIndexDiff delta = TileOffsByDir(dir);
	uint z = GetTileZ(tile);

	dir = ReverseDiagDir(dir);
	do {
		tile += delta;
	} while (
		!IsTileType(tile, MP_TUNNELBRIDGE) ||
		GB(_m[tile].m5, 4, 4) != 0 ||
		GetTunnelDirection(tile) != dir ||
		GetTileZ(tile) != z
	);

	return tile;
}
