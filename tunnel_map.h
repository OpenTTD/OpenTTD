/* $Id$ */

#ifndef TUNNEL_MAP_H
#define TUNNEL_MAP_H

#include "direction.h"
#include "macros.h"
#include "map.h"
#include "rail.h"


static inline bool IsTunnel(TileIndex t)
{
	assert(IsTileType(t, MP_TUNNELBRIDGE));
	return !HASBIT(_m[t].m5, 7);
}


static inline bool IsTunnelTile(TileIndex t)
{
	return IsTileType(t, MP_TUNNELBRIDGE) && IsTunnel(t);
}


static inline DiagDirection GetTunnelDirection(TileIndex t)
{
	assert(IsTunnelTile(t));
	return (DiagDirection)GB(_m[t].m5, 0, 2);
}


static inline TransportType GetTunnelTransportType(TileIndex t)
{
	assert(IsTunnelTile(t));
	return (TransportType)GB(_m[t].m5, 2, 2);
}


TileIndex GetOtherTunnelEnd(TileIndex);
bool IsTunnelInWay(TileIndex, uint z);
bool IsTunnelInWayDir(TileIndex tile, uint z, DiagDirection dir);


static inline void MakeRoadTunnel(TileIndex t, Owner o, DiagDirection d)
{
	SetTileType(t, MP_TUNNELBRIDGE);
	SetTileOwner(t, o);
	_m[t].m2 = 0;
	_m[t].m3 = 0;
	_m[t].m4 = 0;
	_m[t].m5 = TRANSPORT_ROAD << 2 | d;
}

static inline void MakeRailTunnel(TileIndex t, Owner o, DiagDirection d, RailType r)
{
	SetTileType(t, MP_TUNNELBRIDGE);
	SetTileOwner(t, o);
	_m[t].m2 = 0;
	_m[t].m3 = r;
	_m[t].m4 = 0;
	_m[t].m5 = TRANSPORT_RAIL << 2 | d;
}

#endif /* TUNNEL_MAP_H */
