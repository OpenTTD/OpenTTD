/* $Id$ */

/** @file tunnelbridge_map.h Functions that have tunnels and bridges in common */

#ifndef TUNNELBRIDGE_MAP_H
#define TUNNELBRIDGE_MAP_H

#include "direction_func.h"
#include "core/bitmath_func.hpp"  /* GB, HasBit, SB */
#include "map.h"                  /* Tile, TileIndex */
#include "tile_map.h"             /* TileType, IsTileType */
#include "openttd.h"              /* TransportType */


/**
 * Tunnel: Get the direction facing out of the tunnel
 * Bridge: Get the direction pointing onto the bridge
 * @param t The tile to analyze
 * @pre IsTileType(t, MP_TUNNELBRIDGE)
 * @return the above mentionned direction
 */
static inline DiagDirection GetTunnelBridgeDirection(TileIndex t)
{
	assert(IsTileType(t, MP_TUNNELBRIDGE));
	return (DiagDirection)GB(_m[t].m5, 0, 2);
}

/**
 * Tunnel: Get the transport type of the tunnel (road or rail)
 * Bridge: Get the transport type of the bridge's ramp
 * @param t The tile to analyze
 * @pre IsTileType(t, MP_TUNNELBRIDGE)
 * @return the transport type in the tunnel/bridge
 */
static inline TransportType GetTunnelBridgeTransportType(TileIndex t)
{
	assert(IsTileType(t, MP_TUNNELBRIDGE));
	return (TransportType)GB(_m[t].m5, 2, 2);
}

/**
 * Tunnel: Is this tunnel entrance in a snowy or desert area?
 * Bridge: Does the bridge ramp lie in a snow or desert area?
 * @param t The tile to analyze
 * @pre IsTileType(t, MP_TUNNELBRIDGE)
 * @return true if and only if the tile is in a snowy/desert area
 */
static inline bool HasTunnelBridgeSnowOrDesert(TileIndex t)
{
	assert(IsTileType(t, MP_TUNNELBRIDGE));
	return HasBit(_m[t].m4, 7);
}

/**
 * Tunnel: Places this tunnel entrance in a snowy or desert area, or takes it out of there.
 * Bridge: Sets whether the bridge ramp lies in a snow or desert area.
 * @param t the tunnel entrance / bridge ramp tile
 * @param snow_or_desert is the entrance/ramp in snow or desert (true), when
 *                       not in snow and not in desert false
 * @pre IsTileType(t, MP_TUNNELBRIDGE)
 */
static inline void SetTunnelBridgeSnowOrDesert(TileIndex t, bool snow_or_desert)
{
	assert(IsTileType(t, MP_TUNNELBRIDGE));
	SB(_m[t].m4, 7, 1, snow_or_desert);
}

#endif /* TUNNELBRIDGE_MAP_H */
