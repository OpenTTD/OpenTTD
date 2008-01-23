/* $Id$ */

/** @file tunnelbridge.h Header file for things common for tunnels and bridges */

#ifndef TUNNELBRIDGE_H
#define TUNNELBRIDGE_H

#include "tile_type.h"
#include "map_func.h"
#include "tunnelbridge_map.h"

/**
 * Calculates the length of a tunnel or a bridge (without end tiles)
 * @return length of bridge/tunnel middle
 */
static inline uint GetTunnelBridgeLength(TileIndex begin, TileIndex end)
{
	int x1 = TileX(begin);
	int y1 = TileY(begin);
	int x2 = TileX(end);
	int y2 = TileY(end);

	return abs(x2 + y2 - x1 - y1) - 1;
}

#endif /* TUNNELBRIDGE_H */
