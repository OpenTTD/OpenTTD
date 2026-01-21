/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file tunnel_map.h Map accessors for tunnels. */

#ifndef TUNNEL_MAP_H
#define TUNNEL_MAP_H

#include "rail_map.h"
#include "road_map.h"


/**
 * Is this a tunnel (entrance)?
 * @param t the tile that might be a tunnel
 * @pre IsTileType(t, TileType::TunnelBridge)
 * @return true if and only if this tile is a tunnel (entrance)
 */
inline bool IsTunnel(Tile t)
{
	assert(IsTileType(t, TileType::TunnelBridge));
	return !t.GetTileExtendedAs<TileType::TunnelBridge>().is_bridge;
}

/**
 * Is this a tunnel (entrance)?
 * @param t the tile that might be a tunnel
 * @return true if and only if this tile is a tunnel (entrance)
 */
inline bool IsTunnelTile(Tile t)
{
	return IsTileType(t, TileType::TunnelBridge) && IsTunnel(t);
}

TileIndex GetOtherTunnelEnd(TileIndex);
bool IsTunnelInWay(TileIndex, int z);
bool IsTunnelInWayDir(TileIndex tile, int z, DiagDirection dir);

/**
 * Makes a road tunnel entrance
 * @param t the entrance of the tunnel
 * @param o the owner of the entrance
 * @param d the direction facing out of the tunnel
 * @param r the road type used in the tunnel
 */
inline void MakeRoadTunnel(Tile t, Owner o, DiagDirection d, RoadType road_rt, RoadType tram_rt)
{
	SetTileType(t, TileType::TunnelBridge);
	t.ResetData();
	SetTileOwner(t, o);
	auto &extended = t.GetTileExtendedAs<TileType::TunnelBridge>();
	extended.transport_type = TRANSPORT_ROAD;
	extended.direction = d;
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
	SetTileType(t, TileType::TunnelBridge);
	t.ResetData();
	SetTileOwner(t, o);
	auto &extended = t.GetTileExtendedAs<TileType::TunnelBridge>();
	extended.transport_type = TRANSPORT_RAIL;
	extended.direction = d;
	SetRailType(t, r);
}

#endif /* TUNNEL_MAP_H */
