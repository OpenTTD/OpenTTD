/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file bridge_map.h Map accessor functions for bridges. */

#ifndef BRIDGE_MAP_H
#define BRIDGE_MAP_H

#include "rail_map.h"
#include "road_map.h"
#include "bridge.h"
#include "water_map.h"

/**
 * Checks if this is a bridge, instead of a tunnel
 * @param t The tile to analyze
 * @pre IsTileType(t, MP_TUNNELBRIDGE)
 * @return true if the structure is a bridge one
 */
inline bool IsBridge(Tile t)
{
	assert(IsTileType(t, MP_TUNNELBRIDGE));
	return HasBit(t.m5(), 7);
}

/**
 * checks if there is a bridge on this tile
 * @param t The tile to analyze
 * @return true if a bridge is present
 */
inline bool IsBridgeTile(Tile t)
{
	return IsTileType(t, MP_TUNNELBRIDGE) && IsBridge(t);
}

/**
 * checks if a bridge is set above the ground of this tile
 * @param t The tile to analyze
 * @return true if a bridge is detected above
 */
inline bool IsBridgeAbove(Tile t)
{
	return GB(t.type(), 2, 2) != 0;
}

/**
 * Determines the type of bridge on a tile
 * @param t The tile to analyze
 * @pre IsBridgeTile(t)
 * @return The bridge type
 */
inline BridgeType GetBridgeType(Tile t)
{
	assert(IsBridgeTile(t));
	return GB(t.m6(), 2, 4);
}

/**
 * Get the axis of the bridge that goes over the tile. Not the axis or the ramp.
 * @param t The tile to analyze
 * @pre IsBridgeAbove(t)
 * @return the above mentioned axis
 */
inline Axis GetBridgeAxis(Tile t)
{
	assert(IsBridgeAbove(t));
	return (Axis)(GB(t.type(), 2, 2) - 1);
}

TileIndex GetNorthernBridgeEnd(TileIndex t);
TileIndex GetSouthernBridgeEnd(TileIndex t);
TileIndex GetOtherBridgeEnd(TileIndex t);

int GetBridgeHeight(TileIndex tile);
/**
 * Get the height ('z') of a bridge in pixels.
 * @param tile the bridge ramp tile to get the bridge height from
 * @return the height of the bridge in pixels
 */
inline int GetBridgePixelHeight(TileIndex tile)
{
	return GetBridgeHeight(tile) * TILE_HEIGHT;
}

/**
 * Remove the bridge over the given axis.
 * @param t the tile to remove the bridge from
 * @param a the axis of the bridge to remove
 */
inline void ClearSingleBridgeMiddle(Tile t, Axis a)
{
	ClrBit(t.type(), 2 + a);
}

/**
 * Removes bridges from the given, that is bridges along the X and Y axis.
 * @param t the tile to remove the bridge from
 */
inline void ClearBridgeMiddle(Tile t)
{
	ClearSingleBridgeMiddle(t, AXIS_X);
	ClearSingleBridgeMiddle(t, AXIS_Y);
}

/**
 * Set that there is a bridge over the given axis.
 * @param t the tile to add the bridge to
 * @param a the axis of the bridge to add
 */
inline void SetBridgeMiddle(Tile t, Axis a)
{
	SetBit(t.type(), 2 + a);
}

/**
 * Generic part to make a bridge ramp for both roads and rails.
 * @param t          the tile to make a bridge ramp
 * @param o          the new owner of the bridge ramp
 * @param bridgetype the type of bridge this bridge ramp belongs to
 * @param d          the direction this ramp must be facing
 * @param tt         the transport type of the bridge
 * @note this function should not be called directly.
 */
inline void MakeBridgeRamp(Tile t, Owner o, BridgeType bridgetype, DiagDirection d, TransportType tt)
{
	SetTileType(t, MP_TUNNELBRIDGE);
	SetTileOwner(t, o);
	SetDockingTile(t, false);
	t.m2() = 0;
	t.m3() = 0;
	t.m4() = INVALID_ROADTYPE;
	t.m5() = 1 << 7 | tt << 2 | d;
	SB(t.m6(), 2, 4, bridgetype);
	t.m7() = 0;
	t.m8() = INVALID_ROADTYPE << 6;
}

/**
 * Make a bridge ramp for roads.
 * @param t          the tile to make a bridge ramp
 * @param o          the new owner of the bridge ramp
 * @param owner_road the new owner of the road on the bridge
 * @param owner_tram the new owner of the tram on the bridge
 * @param bridgetype the type of bridge this bridge ramp belongs to
 * @param d          the direction this ramp must be facing
 * @param road_rt    the road type of the bridge
 * @param tram_rt    the tram type of the bridge
 */
inline void MakeRoadBridgeRamp(Tile t, Owner o, Owner owner_road, Owner owner_tram, BridgeType bridgetype, DiagDirection d, RoadType road_rt, RoadType tram_rt)
{
	MakeBridgeRamp(t, o, bridgetype, d, TRANSPORT_ROAD);
	SetRoadOwner(t, RTT_ROAD, owner_road);
	if (owner_tram != OWNER_TOWN) SetRoadOwner(t, RTT_TRAM, owner_tram);
	SetRoadTypes(t, road_rt, tram_rt);
}

/**
 * Make a bridge ramp for rails.
 * @param t          the tile to make a bridge ramp
 * @param o          the new owner of the bridge ramp
 * @param bridgetype the type of bridge this bridge ramp belongs to
 * @param d          the direction this ramp must be facing
 * @param rt         the rail type of the bridge
 */
inline void MakeRailBridgeRamp(Tile t, Owner o, BridgeType bridgetype, DiagDirection d, RailType rt)
{
	MakeBridgeRamp(t, o, bridgetype, d, TRANSPORT_RAIL);
	SetRailType(t, rt);
}

/**
 * Make a bridge ramp for aqueducts.
 * @param t          the tile to make a bridge ramp
 * @param o          the new owner of the bridge ramp
 * @param d          the direction this ramp must be facing
 */
inline void MakeAqueductBridgeRamp(Tile t, Owner o, DiagDirection d)
{
	MakeBridgeRamp(t, o, 0, d, TRANSPORT_WATER);
}

#endif /* BRIDGE_MAP_H */
