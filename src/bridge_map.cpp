/* $Id$ */

/** @file bridge_map.cpp Map accessor functions for bridges. */

#include "stdafx.h"
#include "openttd.h"
#include "bridge_map.h"
#include "bridge.h"
#include "variables.h"
#include "landscape.h"


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
	Foundation f = GetBridgeFoundation(tileh, DiagDirToAxis(GetBridgeRampDirection(t)));

	/* one height level extra for the ramp */
	return h + TILE_HEIGHT + ApplyFoundationToSlope(f, &tileh);
}
