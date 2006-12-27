/* $Id$ */

#ifndef BRIDGE_MAP_H
#define BRIDGE_MAP_H

#include "direction.h"
#include "macros.h"
#include "map.h"
#include "rail.h"
#include "road_map.h"
#include "tile.h"


void DrawBridgeMiddle(const TileInfo* ti); // XXX


static inline bool IsBridge(TileIndex t)
{
	assert(IsTileType(t, MP_TUNNELBRIDGE));
	return HASBIT(_m[t].m5, 7);
}

static inline bool IsBridgeTile(TileIndex t)
{
	return IsTileType(t, MP_TUNNELBRIDGE) && IsBridge(t);
}


static inline bool MayHaveBridgeAbove(TileIndex t)
{
	return
		IsTileType(t, MP_CLEAR) ||
		IsTileType(t, MP_RAILWAY) ||
		IsTileType(t, MP_STREET) ||
		IsTileType(t, MP_WATER) ||
		IsTileType(t, MP_TUNNELBRIDGE) ||
		IsTileType(t, MP_UNMOVABLE);
}


static inline bool IsBridgeAbove(TileIndex t)
{
	assert(MayHaveBridgeAbove(t));
	return GB(_m[t].extra, 6, 2) != 0;
}


/**
 * Determines the type of bridge on a tile
 * @param tile The tile to analyze
 * @return The bridge type
 */
static inline uint GetBridgeType(TileIndex t)
{
	assert(IsBridgeTile(t));
	return GB(_m[t].m2, 4, 4);
}


/**
 * Get the direction pointing onto the bridge
 */
static inline DiagDirection GetBridgeRampDirection(TileIndex t)
{
	assert(IsBridgeTile(t));
	return (DiagDirection)GB(_m[t].m5, 0, 2);
}


static inline Axis GetBridgeAxis(TileIndex t)
{
	assert(IsBridgeAbove(t));
	return (Axis)(GB(_m[t].extra, 6, 2) - 1);
}


static inline TransportType GetBridgeTransportType(TileIndex t)
{
	assert(IsBridgeTile(t));
	return (TransportType)GB(_m[t].m5, 2, 2);
}


/**
 * Finds the end of a bridge in the specified direction starting at a middle tile
 */
TileIndex GetBridgeEnd(TileIndex, DiagDirection);

/**
 * Finds the northern end of a bridge starting at a middle tile
 */
TileIndex GetNorthernBridgeEnd(TileIndex t);

/**
 * Finds the southern end of a bridge starting at a middle tile
 */
TileIndex GetSouthernBridgeEnd(TileIndex t);


/**
 * Starting at one bridge end finds the other bridge end
 */
TileIndex GetOtherBridgeEnd(TileIndex);

uint GetBridgeHeight(TileIndex tile);
uint GetBridgeFoundation(Slope tileh, Axis axis);

static inline void ClearSingleBridgeMiddle(TileIndex t, Axis a)
{
	assert(MayHaveBridgeAbove(t));
	CLRBIT(_m[t].extra, 6 + a);
}


static inline void ClearBridgeMiddle(TileIndex t)
{
	ClearSingleBridgeMiddle(t, AXIS_X);
	ClearSingleBridgeMiddle(t, AXIS_Y);
}

static inline void SetBridgeMiddle(TileIndex t, Axis a)
{
	assert(MayHaveBridgeAbove(t));
	SETBIT(_m[t].extra, 6 + a);
}


static inline void MakeBridgeRamp(TileIndex t, Owner o, uint bridgetype, DiagDirection d, TransportType tt)
{
	SetTileType(t, MP_TUNNELBRIDGE);
	SetTileOwner(t, o);
	_m[t].m2 = bridgetype << 4;
	_m[t].m4 = 0;
	_m[t].m5 = 1 << 7 | tt << 2 | d;
}

static inline void MakeRoadBridgeRamp(TileIndex t, Owner o, uint bridgetype, DiagDirection d)
{
	MakeBridgeRamp(t, o, bridgetype, d, TRANSPORT_ROAD);
	_m[t].m3 = 0;
}

static inline void MakeRailBridgeRamp(TileIndex t, Owner o, uint bridgetype, DiagDirection d, RailType r)
{
	MakeBridgeRamp(t, o, bridgetype, d, TRANSPORT_RAIL);
	_m[t].m3 = r;
}


#endif /* BRIDGE_MAP_H */
