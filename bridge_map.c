/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "bridge_map.h"
#include "variables.h"


TileIndex GetBridgeEnd(TileIndex tile, DiagDirection dir)
{
	TileIndexDiff delta = TileOffsByDir(dir);

	do { tile += delta; } while (IsBridgeAbove(tile) && IsBridgeOfAxis(tile, DiagDirToAxis(dir)));

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

uint GetBridgeHeight(TileIndex tile, Axis a)
{
	uint h, f;
	uint tileh = GetTileSlope(tile, &h);

	f = GetBridgeFoundation(tileh, a);

	if (f) {
		if (f < 15) {
			h += TILE_HEIGHT;
			tileh = SLOPE_FLAT;
		} else {
			tileh = _inclined_tileh[f - 15];
		}
	}

	return h + TILE_HEIGHT;
}
