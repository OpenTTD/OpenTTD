/* $Id$ */

#ifndef TUNNEL_MAP_H
#define TUNNEL_MAP_H

#include "macros.h"
#include "map.h"


static inline bool IsTunnel(TileIndex t)
{
	assert(IsTileType(t, MP_TUNNELBRIDGE));
	return !HASBIT(_m[t].m5, 7);
}


static inline uint GetTunnelDirection(TileIndex t)
{
	assert(IsTunnelTile(t));
	return (uint)GB(_m[t].m5, 0, 2);
}

#endif
