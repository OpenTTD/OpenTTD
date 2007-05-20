/* $Id$ */

/** @file tunnel_map.h */

#ifndef TUNNEL_MAP_H
#define TUNNEL_MAP_H

#include "direction.h"
#include "macros.h"
#include "map.h"
#include "rail.h"
#include "road.h"

/**
 * Is this a tunnel (entrance)?
 * @param t the tile that might be a tunnel
 * @pre IsTileType(t, MP_TUNNELBRIDGE)
 * @return true if and only if this tile is a tunnel (entrance)
 */
static inline bool IsTunnel(TileIndex t)
{
	assert(IsTileType(t, MP_TUNNELBRIDGE));
	return !HASBIT(_m[t].m5, 7);
}


/**
 * Is this a tunnel (entrance)?
 * @param t the tile that might be a tunnel
 * @return true if and only if this tile is a tunnel (entrance)
 */
static inline bool IsTunnelTile(TileIndex t)
{
	return IsTileType(t, MP_TUNNELBRIDGE) && IsTunnel(t);
}

/**
 * Gets the direction facing out of the tunnel
 * @param t the tile to get the tunnel facing direction of
 * @pre IsTunnelTile(t)
 * @return the direction the tunnel is facing
 */
static inline DiagDirection GetTunnelDirection(TileIndex t)
{
	assert(IsTunnelTile(t));
	return (DiagDirection)GB(_m[t].m5, 0, 2);
}

/**
 * Gets the transport type of the tunnel (road or rail)
 * @param t the tunnel entrance tile to get the type of
 * @pre IsTunnelTile(t)
 * @return the transport type in the tunnel
 */
static inline TransportType GetTunnelTransportType(TileIndex t)
{
	assert(IsTunnelTile(t));
	return (TransportType)GB(_m[t].m5, 2, 2);
}

/**
 * Is this tunnel entrance in a snowy or desert area?
 * @param t the tunnel entrance tile
 * @pre IsTunnelTile(t)
 * @return true if and only if the tunnel entrance is in a snowy/desert area
 */
static inline bool HasTunnelSnowOrDesert(TileIndex t)
{
	assert(IsTunnelTile(t));
	return HASBIT(_m[t].m4, 7);
}

/**
 * Places this tunnel entrance in a snowy or desert area,
 * or takes it out of there.
 * @param t the tunnel entrance tile
 * @param snow_or_desert is the entrance in snow or desert (true), when
 *                       not in snow and not in desert false
 * @pre IsTunnelTile(t)
 */
static inline void SetTunnelSnowOrDesert(TileIndex t, bool snow_or_desert)
{
	assert(IsTunnelTile(t));
	SB(_m[t].m4, 7, 1, snow_or_desert);
}


TileIndex GetOtherTunnelEnd(TileIndex);
bool IsTunnelInWay(TileIndex, uint z);


/**
 * Makes a road tunnel entrance
 * @param t the entrance of the tunnel
 * @param o the owner of the entrance
 * @param d the direction facing out of the tunnel
 * @param r the road type used in the tunnel
 */
static inline void MakeRoadTunnel(TileIndex t, Owner o, DiagDirection d, RoadTypes r)
{
	SetTileType(t, MP_TUNNELBRIDGE);
	SetTileOwner(t, o);
	_m[t].m2 = 0;
	_m[t].m3 = r;
	_m[t].m4 = 0;
	_m[t].m5 = TRANSPORT_ROAD << 2 | d;
}

/**
 * Makes a rail tunnel entrance
 * @param t the entrance of the tunnel
 * @param o the owner of the entrance
 * @param d the direction facing out of the tunnel
 * @param r the rail type used in the tunnel
 */
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
