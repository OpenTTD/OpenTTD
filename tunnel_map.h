/* $Id$ */

#ifndef TUNNEL_MAP_H
#define TUNNEL_MAP_H

#include "direction.h"
#include "macros.h"
#include "map.h"
#include "rail.h"


static inline DiagDirection GetTunnelDirection(TileIndex t)
{
	return (DiagDirection)GB(_m[t].m5, 0, 2);
}


static inline TransportType GetTunnelTransportType(TileIndex t)
{
	return (TransportType)GB(_m[t].m5, 2, 2);
}


TileIndex GetOtherTunnelEnd(TileIndex);


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

#endif
