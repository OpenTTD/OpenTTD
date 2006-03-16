/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "bridge_map.h"


TileIndex GetBridgeEnd(TileIndex tile, DiagDirection dir)
{
	TileIndexDiff delta = TileOffsByDir(dir);

	assert(DiagDirToAxis(dir) == GetBridgeAxis(tile));

	do {
		tile += delta;
	} while (!IsBridgeRamp(tile));

	return tile;
}


TileIndex GetSouthernBridgeEnd(TileIndex t)
{
	return GetBridgeEnd(t, AxisToDiagDir(GetBridgeAxis(t)));
}


TileIndex GetOtherBridgeEnd(TileIndex tile)
{
	TileIndexDiff delta = TileOffsByDir(GetBridgeRampDirection(tile));

	do {
		tile += delta;
	} while (!IsBridgeRamp(tile));

	return tile;
}
