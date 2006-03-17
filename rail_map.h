/* $Id$ */

#ifndef RAIL_MAP_H
#define RAIL_MAP_H

#include "direction.h"
#include "tile.h"


typedef enum RailTileType {
	RAIL_TYPE_NORMAL         = 0x0,
	RAIL_TYPE_SIGNALS        = 0x40,
	RAIL_TYPE_UNUSED         = 0x80, /* XXX: Maybe this could become waypoints? */
	RAIL_TYPE_DEPOT_WAYPOINT = 0xC0, /* Is really depots and waypoints... */
	RAIL_TILE_TYPE_MASK      = 0xC0
} RailTileType;

static inline RailTileType GetRailTileType(TileIndex t)
{
	assert(IsTileType(t, MP_RAILWAY));
	return _m[t].m5 & RAIL_TILE_TYPE_MASK;
}


/** These specify the subtype when the main rail type is
 * RAIL_TYPE_DEPOT_WAYPOINT */
typedef enum RailTileSubtypes {
	RAIL_SUBTYPE_DEPOT    = 0x00,
	RAIL_SUBTYPE_WAYPOINT = 0x04,
	RAIL_SUBTYPE_MASK     = 0x3C
} RailTileSubtype;


typedef enum RailTypes {
	RAILTYPE_RAIL   = 0,
	RAILTYPE_MONO   = 1,
	RAILTYPE_MAGLEV = 2,
	RAILTYPE_END,
	RAILTYPE_MASK   = 0x3,
	INVALID_RAILTYPE = 0xFF
} RailType;

static inline RailType GetRailType(TileIndex t)
{
	return (RailType)GB(_m[t].m3, 0, 4);
}


/** These are used to specify a single track.
 * Can be translated to a trackbit with TrackToTrackbit */
typedef enum Track {
	TRACK_X     = 0,
	TRACK_Y     = 1,
	TRACK_UPPER = 2,
	TRACK_LOWER = 3,
	TRACK_LEFT  = 4,
	TRACK_RIGHT = 5,
	TRACK_END,
	INVALID_TRACK = 0xFF
} Track;


/** Bitfield corresponding to Track */
typedef enum TrackBits {
	TRACK_BIT_X     = 1U << TRACK_X,
	TRACK_BIT_Y     = 1U << TRACK_Y,
	TRACK_BIT_UPPER = 1U << TRACK_UPPER,
	TRACK_BIT_LOWER = 1U << TRACK_LOWER,
	TRACK_BIT_LEFT  = 1U << TRACK_LEFT,
	TRACK_BIT_RIGHT = 1U << TRACK_RIGHT,
	TRACK_BIT_MASK  = 0x3FU
} TrackBits;


static inline DiagDirection GetRailDepotDirection(TileIndex t)
{
	return (DiagDirection)GB(_m[t].m5, 0, 2);
}


static inline TrackBits GetRailWaypointBits(TileIndex t)
{
	return _m[t].m5 & 1 ? TRACK_BIT_Y : TRACK_BIT_X;
}


typedef enum SignalType {
	SIGTYPE_NORMAL  = 0, // normal signal
	SIGTYPE_ENTRY   = 1, // presignal block entry
	SIGTYPE_EXIT    = 2, // presignal block exit
	SIGTYPE_COMBO   = 3  // presignal inter-block
} SignalType;

static inline SignalType GetSignalType(TileIndex t)
{
	assert(GetRailTileType(t) == RAIL_TYPE_SIGNALS);
	return (SignalType)GB(_m[t].m4, 0, 2);
}

static inline void SetSignalType(TileIndex t, SignalType s)
{
	assert(GetRailTileType(t) == RAIL_TYPE_SIGNALS);
	SB(_m[t].m4, 0, 2, s);
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
