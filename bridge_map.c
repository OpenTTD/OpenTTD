/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "bridge_map.h"


TileIndex GetOtherBridgeEnd(TileIndex tile)
{
	TileIndexDiff delta = TileOffsByDir(GetBridgeRampDirection(tile));

	do {
		tile += delta;
	} while (!IsBridgeRamp(tile));

	return tile;
}
