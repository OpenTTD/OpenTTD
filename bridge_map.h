/* $Id$ */

#ifndef BRIDGE_MAP_H
#define BRIDGE_MAP_H

#include "direction.h"
#include "macros.h"
#include "map.h"
#include "rail.h"
#include "tile.h"


static inline bool IsBridge(TileIndex t)
{
	assert(IsTileType(t, MP_TUNNELBRIDGE));
	return HASBIT(_m[t].m5, 7);
}

static inline bool IsBridgeTile(TileIndex t)
{
	return IsTileType(t, MP_TUNNELBRIDGE) && IsBridge(t);
}


static inline bool IsBridgeRamp(TileIndex t)
{
	assert(IsBridgeTile(t));
	return !HASBIT(_m[t].m5, 6);
}

static inline bool IsBridgeMiddle(TileIndex t)
{
	assert(IsBridgeTile(t));
	return HASBIT(_m[t].m5, 6);
}


/**
 * Get the direction pointing onto the bridge
 */
static inline DiagDirection GetBridgeRampDirection(TileIndex t)
{
	assert(IsBridgeRamp(t));
	/* Heavy wizardry to convert the X/Y (bit 0) + N/S (bit 5) encoding of
	 * bridges to a DiagDirection
	 */
	return (DiagDirection)((6 - (_m[t].m5 >> 4 & 2) - (_m[t].m5 & 1)) % 4);
}


static inline Axis GetBridgeAxis(TileIndex t)
{
	assert(IsBridgeMiddle(t));
	return (Axis)GB(_m[t].m5, 0, 1);
}


static inline bool IsTransportUnderBridge(TileIndex t)
{
	assert(IsBridgeMiddle(t));
	return HASBIT(_m[t].m5, 5);
}

#endif
