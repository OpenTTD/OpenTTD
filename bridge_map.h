/* $Id$ */

#ifndef BRIDGE_MAP_H
#define BRIDGE_MAP_H

#include "direction.h"
#include "macros.h"
#include "map.h"
#include "rail.h"
#include "tile.h"


static inline bool IsBridgeRamp(TileIndex t)
{
	return !HASBIT(_m[t].m5, 6);
}

static inline bool IsBridgeMiddle(TileIndex t)
{
	return HASBIT(_m[t].m5, 6);
}



/**
 * Get the direction pointing onto the bridge
 */
static inline DiagDirection GetBridgeRampDirection(TileIndex t)
{
	/* Heavy wizardry to convert the X/Y (bit 0) + N/S (bit 5) encoding of
	 * bridges to a DiagDirection
	 */
	return (DiagDirection)((6 - (_m[t].m5 >> 4 & 2) - (_m[t].m5 & 1)) % 4);
}


static inline Axis GetBridgeAxis(TileIndex t)
{
	return (Axis)GB(_m[t].m5, 0, 1);
}


static inline bool IsClearUnderBridge(TileIndex t)
{
	return GB(_m[t].m5, 3, 3) == 0;
}

static inline bool IsWaterUnderBridge(TileIndex t)
{
	return GB(_m[t].m5, 3, 3) == 1;
}


static inline bool IsTransportUnderBridge(TileIndex t)
{
	return HASBIT(_m[t].m5, 5);
}

static inline TransportType GetTransportTypeUnderBridge(TileIndex t)
{
	return (TransportType)GB(_m[t].m5, 3, 2);
}


/**
 * Finds the end of a bridge in the specified direction starting at a middle tile
 */
TileIndex GetBridgeEnd(TileIndex, DiagDirection);

/**
 * Finds the southern end of a bridge starting at a middle tile
 */
TileIndex GetSouthernBridgeEnd(TileIndex t);


/**
 * Starting at one bridge end finds the other bridge end
 */
TileIndex GetOtherBridgeEnd(TileIndex);


static inline void SetClearUnderBridge(TileIndex t)
{
	SetTileOwner(t, OWNER_NONE);
	SB(_m[t].m5, 3, 3, 0 << 2 | 0);
}

static inline void SetWaterUnderBridge(TileIndex t)
{
	SetTileOwner(t, OWNER_WATER);
	SB(_m[t].m5, 3, 3, 0 << 2 | 1);
}

static inline void SetRailUnderBridge(TileIndex t, Owner o, RailType r)
{
	SetTileOwner(t, o);
	SB(_m[t].m5, 3, 3, 1 << 2 | TRANSPORT_RAIL);
	SB(_m[t].m3, 0, 4, r);
}

static inline void SetRoadUnderBridge(TileIndex t, Owner o)
{
	SetTileOwner(t, o);
	SB(_m[t].m5, 3, 3, 1 << 2 | TRANSPORT_ROAD);
}


static inline void MakeBridgeRamp(TileIndex t, Owner o, uint bridgetype, DiagDirection d, TransportType tt)
{
	uint northsouth = (d == DIAGDIR_NE || d == DIAGDIR_NW);

	SetTileType(t, MP_TUNNELBRIDGE);
	SetTileOwner(t, o);
	_m[t].m2 = bridgetype << 4;
	_m[t].m4 = 0;
	_m[t].m5 = 1 << 7 | 0 << 6 | northsouth << 5 | tt << 1 | DiagDirToAxis(d);
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


static inline void MakeBridgeMiddle(TileIndex t, uint bridgetype, uint piece, Axis a, TransportType tt)
{
	SetTileType(t, MP_TUNNELBRIDGE);
	SetTileOwner(t, OWNER_NONE);
	_m[t].m2 = bridgetype << 4 | piece;
	_m[t].m3 = 0;
	_m[t].m4 = 0;
	_m[t].m5 = 1 << 7 | 1 << 6 | 0 << 5 | 0 << 3 | tt << 1 | a;
}

static inline void MakeRoadBridgeMiddle(TileIndex t, uint bridgetype, uint piece, Axis a)
{
	MakeBridgeMiddle(t, bridgetype, piece, a, TRANSPORT_ROAD);
}

static inline void MakeRailBridgeMiddle(TileIndex t, uint bridgetype, uint piece, Axis a, RailType r)
{
	MakeBridgeMiddle(t, bridgetype, piece, a, TRANSPORT_RAIL);
	SB(_m[t].m3, 4, 4, r);
}


#endif
