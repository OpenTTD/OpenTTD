/* $Id$ */

/** @file bridge_map.h Map accessor functions for bridges. */

#ifndef BRIDGE_MAP_H
#define BRIDGE_MAP_H

#include "direction.h"
#include "macros.h"
#include "map.h"
#include "rail.h"
#include "road_map.h"
#include "tile.h"


/**
 * Checks if this is a bridge, instead of a tunnel
 * @param t The tile to analyze
 * @pre IsTileType(t, MP_TUNNELBRIDGE)
 * @return true if the structure is a bridge one
 */
static inline bool IsBridge(TileIndex t)
{
	assert(IsTileType(t, MP_TUNNELBRIDGE));
	return HasBit(_m[t].m5, 7);
}

/**
 * checks if there is a bridge on this tile
 * @param t The tile to analyze
 * @return true if a bridge is present
 */
static inline bool IsBridgeTile(TileIndex t)
{
	return IsTileType(t, MP_TUNNELBRIDGE) && IsBridge(t);
}

/**
 * checks for the possibility that a bridge may be on this tile
 * These are in fact all the tile types on which a bridge can be found
 * @param t The tile to analyze
 * @return true if a bridge migh be present
 */
static inline bool MayHaveBridgeAbove(TileIndex t)
{
	return
		IsTileType(t, MP_CLEAR) ||
		IsTileType(t, MP_RAILWAY) ||
		IsTileType(t, MP_ROAD) ||
		IsTileType(t, MP_WATER) ||
		IsTileType(t, MP_TUNNELBRIDGE) ||
		IsTileType(t, MP_UNMOVABLE);
}

/**
 * checks if a bridge is set above the ground of this tile
 * @param t The tile to analyze
 * @pre MayHaveBridgeAbove(t)
 * @return true if a bridge is detected above
 */
static inline bool IsBridgeAbove(TileIndex t)
{
	assert(MayHaveBridgeAbove(t));
	return GB(_m[t].m6, 6, 2) != 0;
}


/**
 * Determines the type of bridge on a tile
 * @param t The tile to analyze
 * @pre IsBridgeTile(t)
 * @return The bridge type
 */
static inline uint GetBridgeType(TileIndex t)
{
	assert(IsBridgeTile(t));
	return GB(_m[t].m2, 4, 4);
}


/**
 * Get the direction pointing onto the bridge
 * @param t The tile to analyze
 * @pre IsBridgeTile(t)
 * @return the above mentionned direction
 */
static inline DiagDirection GetBridgeRampDirection(TileIndex t)
{
	assert(IsBridgeTile(t));
	return (DiagDirection)GB(_m[t].m5, 0, 2);
}


/**
 * Get the axis of the bridge that goes over the tile. Not the axis or the ramp.
 * @param t The tile to analyze
 * @pre IsBridgeAbove(t)
 * @return the above mentioned axis
 */
static inline Axis GetBridgeAxis(TileIndex t)
{
	assert(IsBridgeAbove(t));
	return (Axis)(GB(_m[t].m6, 6, 2) - 1);
}


/**
 * Get the transport type of the bridge's ramp.
 * @param t The ramp tile to analyze
 * @pre IsBridgeTile(t)
 * @return the transport type of the bridge
 */
static inline TransportType GetBridgeTransportType(TileIndex t)
{
	assert(IsBridgeTile(t));
	return (TransportType)GB(_m[t].m5, 2, 2);
}


/**
 * Does the bridge ramp lie in a snow or desert area?
 * @param t The ramp tile to analyze
 * @pre IsBridgeTile(t)
 * @return true if and only if in a snow or desert area
 */
static inline bool HasBridgeSnowOrDesert(TileIndex t)
{
	assert(IsBridgeTile(t));
	return HasBit(_m[t].m4, 7);
}


/**
 * Sets whether the bridge ramp lies in a snow or desert area.
 * @param t              The ramp tile to set (un)make a snow/desert area
 * @param snow_or_desert Make (true) or unmake the tile a snow/desert area
 * @pre IsBridgeTile(t)
 */
static inline void SetBridgeSnowOrDesert(TileIndex t, bool snow_or_desert)
{
	assert(IsBridgeTile(t));
	SB(_m[t].m4, 7, 1, snow_or_desert);
}

/**
 * Finds the end of a bridge in the specified direction starting at a middle tile
 * @param t the bridge tile to find the bridge ramp for
 * @param d the direction to search in
 */
TileIndex GetBridgeEnd(TileIndex t, DiagDirection d);

/**
 * Finds the northern end of a bridge starting at a middle tile
 * @param t the bridge tile to find the bridge ramp for
 */
TileIndex GetNorthernBridgeEnd(TileIndex t);

/**
 * Finds the southern end of a bridge starting at a middle tile
 * @param t the bridge tile to find the bridge ramp for
 */
TileIndex GetSouthernBridgeEnd(TileIndex t);


/**
 * Starting at one bridge end finds the other bridge end
 * @param t the bridge ramp tile to find the other bridge ramp for
 */
TileIndex GetOtherBridgeEnd(TileIndex t);

/**
 * Get the height ('z') of a bridge in pixels.
 * @param tile the bridge ramp tile to get the bridge height from
 * @return the height of the bridge in pixels
 */
uint GetBridgeHeight(TileIndex tile);

/**
 * Remove the bridge over the given axis.
 * @param t the tile to remove the bridge from
 * @param a the axis of the bridge to remove
 * @pre MayHaveBridgeAbove(t)
 */
static inline void ClearSingleBridgeMiddle(TileIndex t, Axis a)
{
	assert(MayHaveBridgeAbove(t));
	ClrBit(_m[t].m6, 6 + a);
}


/**
 * Removes bridges from the given, that is bridges along the X and Y axis.
 * @param t the tile to remove the bridge from
 * @pre MayHaveBridgeAbove(t)
 */
static inline void ClearBridgeMiddle(TileIndex t)
{
	ClearSingleBridgeMiddle(t, AXIS_X);
	ClearSingleBridgeMiddle(t, AXIS_Y);
}

/**
 * Set that there is a bridge over the given axis.
 * @param t the tile to add the bridge to
 * @param a the axis of the bridge to add
 * @pre MayHaveBridgeAbove(t)
 */
static inline void SetBridgeMiddle(TileIndex t, Axis a)
{
	assert(MayHaveBridgeAbove(t));
	SetBit(_m[t].m6, 6 + a);
}

/**
 * Generic part to make a bridge ramp for both roads and rails.
 * @param t          the tile to make a bridge ramp
 * @param o          the new owner of the bridge ramp
 * @param bridgetype the type of bridge this bridge ramp belongs to
 * @param d          the direction this ramp must be facing
 * @param tt         the transport type of the bridge
 * @param rt         the road or rail type
 * @note this function should not be called directly.
 */
static inline void MakeBridgeRamp(TileIndex t, Owner o, uint bridgetype, DiagDirection d, TransportType tt, uint rt)
{
	SetTileType(t, MP_TUNNELBRIDGE);
	SetTileOwner(t, o);
	_m[t].m2 = bridgetype << 4;
	_m[t].m3 = rt;
	_m[t].m4 = 0;
	_m[t].m5 = 1 << 7 | tt << 2 | d;
}

/**
 * Make a bridge ramp for roads.
 * @param t          the tile to make a bridge ramp
 * @param o          the new owner of the bridge ramp
 * @param bridgetype the type of bridge this bridge ramp belongs to
 * @param d          the direction this ramp must be facing
 * @param r          the road type of the bridge
 */
static inline void MakeRoadBridgeRamp(TileIndex t, Owner o, uint bridgetype, DiagDirection d, RoadTypes r)
{
	MakeBridgeRamp(t, o, bridgetype, d, TRANSPORT_ROAD, r);
}

/**
 * Make a bridge ramp for rails.
 * @param t          the tile to make a bridge ramp
 * @param o          the new owner of the bridge ramp
 * @param bridgetype the type of bridge this bridge ramp belongs to
 * @param d          the direction this ramp must be facing
 * @param r          the rail type of the bridge
 */
static inline void MakeRailBridgeRamp(TileIndex t, Owner o, uint bridgetype, DiagDirection d, RailType r)
{
	MakeBridgeRamp(t, o, bridgetype, d, TRANSPORT_RAIL, r);
}


#endif /* BRIDGE_MAP_H */
