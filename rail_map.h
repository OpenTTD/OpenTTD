/* $Id$ */

#ifndef RAIL_MAP_H
#define RAIL_MAP_H

#include "rail.h"
#include "tile.h"
#include "waypoint.h"


static inline DiagDirection GetRailDepotDirection(TileIndex t)
{
	return (DiagDirection)GB(_m[t].m5, 0, 2);
}


static inline TrackBits GetRailWaypointBits(TileIndex t)
{
	return _m[t].m5 & RAIL_WAYPOINT_TRACK_MASK ? TRACK_BIT_Y : TRACK_BIT_X;
}


typedef enum SignalVariant {
	SIG_ELECTRIC  = 0,
	SIG_SEMAPHORE = 1
} SignalVariant;

static inline SignalVariant GetSignalVariant(TileIndex t)
{
	return (SignalVariant)GB(_m[t].m4, 2, 1);
}

static inline void SetSignalVariant(TileIndex t, SignalVariant v)
{
	SB(_m[t].m4, 2, 1, v);
}


static inline void MakeRailNormal(TileIndex t, Owner o, TrackBits b, RailType r)
{
	SetTileType(t, MP_RAILWAY);
	SetTileOwner(t, o);
	_m[t].m2 = 0;
	_m[t].m3 = r;
	_m[t].m4 = 0;
	_m[t].m5 = RAIL_TYPE_NORMAL | b;
}


static inline void MakeRailDepot(TileIndex t, Owner o, DiagDirection d, RailType r)
{
	SetTileType(t, MP_RAILWAY);
	SetTileOwner(t, o);
	_m[t].m2 = 0;
	_m[t].m3 = r;
	_m[t].m4 = 0;
	_m[t].m5 = RAIL_TYPE_DEPOT_WAYPOINT | RAIL_SUBTYPE_DEPOT | d;
}


static inline void MakeRailWaypoint(TileIndex t, Owner o, Axis a, RailType r, uint index)
{
	SetTileType(t, MP_RAILWAY);
	SetTileOwner(t, o);
	_m[t].m2 = index;
	_m[t].m3 = r;
	_m[t].m4 = 0;
	_m[t].m5 = RAIL_TYPE_DEPOT_WAYPOINT | RAIL_SUBTYPE_WAYPOINT | a;
}

#endif
