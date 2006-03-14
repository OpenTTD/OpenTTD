/* $Id$ */

#ifndef BRIDGE_MAP_H
#define BRIDGE_MAP_H

#include "direction.h"
#include "macros.h"
#include "map.h"
#include "rail.h"
#include "tile.h"


/**
 * Get the direction pointing onto the bridge
 */
static inline DiagDirection GetBridgeRampDirection(TileIndex t)
{
	/* Heavy wizardry to convert the X/Y (bit 0) + N/S (bit 5) encoding of
	 * bridges to a DiagDirection
	 */
	return (DiagDirection)((6 - (_m[t].m5 >> 4 & 2) - (_m[t].m5 & 1)) % 4);
}


static inline void SetClearUnderBridge(TileIndex t)
{
	SetTileOwner(t, OWNER_NONE);
	SB(_m[t].m5, 3, 3, 0 << 2 | 0);
}

static inline void SetWaterUnderBridge(TileIndex t)
{
	SetTileOwner(t, OWNER_WATER);
	SB(_m[t].m5, 3, 3, 0 << 2 | 1);
}

static inline void SetRailUnderBridge(TileIndex t, Owner o, RailType r)
{
	SetTileOwner(t, o);
	SB(_m[t].m5, 3, 3, 1 << 2 | TRANSPORT_RAIL);
	SB(_m[t].m3, 0, 4, r);
}

static inline void SetRoadUnderBridge(TileIndex t, Owner o)
{
	SetTileOwner(t, o);
	SB(_m[t].m5, 3, 3, 1 << 2 | TRANSPORT_ROAD);
}

#endif
