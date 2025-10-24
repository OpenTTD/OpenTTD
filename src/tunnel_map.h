/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file tunnel_map.h Map accessors for tunnels. */

#ifndef TUNNEL_MAP_H
#define TUNNEL_MAP_H

#include "rail_map.h"
#include "road_map.h"


/**
 * Is this a tunnel (entrance)?
 * @param t the tile that might be a tunnel
 * @pre IsTileType(t, MP_TUNNELBRIDGE)
 * @return true if and only if this tile is a tunnel (entrance)
 */
inline bool IsTunnel(Tile t)
{
	assert(IsTileType(t, MP_TUNNELBRIDGE));
	return !HasBit(t.m5(), 7);
}

/**
 * Is this a tunnel (entrance)?
 * @param t the tile that might be a tunnel
 * @return true if and only if this tile is a tunnel (entrance)
 */
inline bool IsTunnelTile(Tile t)
{
	return IsTileType(t, MP_TUNNELBRIDGE) && IsTunnel(t);
}

/**
 * Checks if a tunnel presence flag is set for this tile.
 * @param t Tile to inspect.
 * @return true if the tile has a tunnel marked beneath it.
 */
inline bool HasTunnelFlag(Tile t)
{
	return HasBit(t.m8(), 14);
}

/**
 * Clears the tunnel presence flag for this tile.
 * @param t Tile to update.
 */
inline void ClearTunnelFlag(Tile t)
{
	ClrBit(t.m8(), 14);
}

/**
 * Sets the tunnel presence flag for this tile.
 * @param t Tile to update.
 */
inline void SetTunnelFlag(Tile t)
{
	SetBit(t.m8(), 14);
}

TileIndex GetOtherTunnelEnd(TileIndex);
bool IsTunnelInWay(TileIndex, int z);
bool IsTunnelInWayDir(TileIndex tile, int z, DiagDirection dir);
void UpdateTunnelPresenceFlags(TileIndex start, TileIndex end, DiagDirection dir);

/**
 * Makes a road tunnel entrance
 * @param t the entrance of the tunnel
 * @param o the owner of the entrance
 * @param d the direction facing out of the tunnel
 * @param r the road type used in the tunnel
 */
inline void MakeRoadTunnel(Tile t, Owner o, DiagDirection d, RoadType road_rt, RoadType tram_rt)
{
	SetTileType(t, MP_TUNNELBRIDGE);
	SetTileOwner(t, o);
	t.m2() = 0;
	t.m3() = 0;
	t.m4() = 0;
	t.m5() = TRANSPORT_ROAD << 2 | d;
	SB(t.m6(), 2, 4, 0);
	t.m7() = 0;
	t.m8() = GB(t.m8(), 14, 1) << 14;
	SetRoadOwner(t, RTT_ROAD, o);
	if (o != OWNER_TOWN) SetRoadOwner(t, RTT_TRAM, o);
	SetRoadTypes(t, road_rt, tram_rt);
}

/**
 * Makes a rail tunnel entrance
 * @param t the entrance of the tunnel
 * @param o the owner of the entrance
 * @param d the direction facing out of the tunnel
 * @param r the rail type used in the tunnel
 */
inline void MakeRailTunnel(Tile t, Owner o, DiagDirection d, RailType r)
{
	SetTileType(t, MP_TUNNELBRIDGE);
	SetTileOwner(t, o);
	t.m2() = 0;
	t.m3() = 0;
	t.m4() = 0;
	t.m5() = TRANSPORT_RAIL << 2 | d;
	SB(t.m6(), 2, 4, 0);
	t.m7() = 0;
	t.m8() = GB(t.m8(), 14, 1) << 14;
	SetRailType(t, r);
}

#endif /* TUNNEL_MAP_H */
