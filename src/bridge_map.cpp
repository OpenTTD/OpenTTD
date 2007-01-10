/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "bridge_map.h"
#include "variables.h"


TileIndex GetBridgeEnd(TileIndex tile, DiagDirection dir)
{
	TileIndexDiff delta = TileOffsByDiagDir(dir);

	dir = ReverseDiagDir(dir);
	do {
		tile += delta;
	} while (!IsBridgeTile(tile) || GetBridgeRampDirection(tile) != dir);

	return tile;
}


TileIndex GetNorthernBridgeEnd(TileIndex t)
{
	return GetBridgeEnd(t, ReverseDiagDir(AxisToDiagDir(GetBridgeAxis(t))));
}


TileIndex GetSouthernBridgeEnd(TileIndex t)
{
	return GetBridgeEnd(t, AxisToDiagDir(GetBridgeAxis(t)));
}


TileIndex GetOtherBridgeEnd(TileIndex tile)
{
	assert(IsBridgeTile(tile));
	return GetBridgeEnd(tile, GetBridgeRampDirection(tile));
}

uint GetBridgeHeight(TileIndex t)
{
	uint h;
	Slope tileh = GetTileSlope(t, &h);
	uint f = GetBridgeFoundation(tileh, DiagDirToAxis(GetBridgeRampDirection(t)));

	// one height level extra if the ramp is on a flat foundation
	return
		h + TILE_HEIGHT +
		(IS_INT_INSIDE(f, 1, 15) ? TILE_HEIGHT : 0) +
		(IsSteepSlope(tileh) ? TILE_HEIGHT : 0);
}
