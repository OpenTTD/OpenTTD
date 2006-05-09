/* $Id$ */

#ifndef RAIL_MAP_H
#define RAIL_MAP_H

#include "direction.h"
#include "tile.h"


typedef enum RailTileType {
	RAIL_TILE_NORMAL         = 0x0,
	RAIL_TILE_SIGNALS        = 0x40,
	RAIL_TILE_UNUSED         = 0x80, /* XXX: Maybe this could become waypoints? */
	RAIL_TILE_DEPOT_WAYPOINT = 0xC0, /* Is really depots and waypoints... */
	RAIL_TILE_TYPE_MASK      = 0xC0
} RailTileType;

static inline RailTileType GetRailTileType(TileIndex t)
{
	assert(IsTileType(t, MP_RAILWAY));
	return _m[t].m5 & RAIL_TILE_TYPE_MASK;
}

/**
 * Returns whether this is plain rails, with or without signals. Iow, if this
 * tiles RailTileType is RAIL_TILE_NORMAL or RAIL_TILE_SIGNALS.
 */
static inline bool IsPlainRailTile(TileIndex tile)
{
	RailTileType rtt = GetRailTileType(tile);
	return rtt == RAIL_TILE_NORMAL || rtt == RAIL_TILE_SIGNALS;
}

/**
 * Checks if a rail tile has signals.
 */
static inline bool HasSignals(TileIndex tile)
{
	return GetRailTileType(tile) == RAIL_TILE_SIGNALS;
}


/** These specify the subtype when the main rail type is
 * RAIL_TILE_DEPOT_WAYPOINT */
typedef enum RailTileSubtypes {
	RAIL_SUBTYPE_DEPOT    = 0x00,
	RAIL_SUBTYPE_WAYPOINT = 0x04,
	RAIL_SUBTYPE_MASK     = 0x3C
} RailTileSubtype;

/**
 * Returns the RailTileSubtype of a given rail tile with type
 * RAIL_TILE_DEPOT_WAYPOINT
 */
static inline RailTileSubtype GetRailTileSubtype(TileIndex tile)
{
	assert(GetRailTileType(tile) == RAIL_TILE_DEPOT_WAYPOINT);
	return (RailTileSubtype)(_m[tile].m5 & RAIL_SUBTYPE_MASK);
}


typedef enum RailTypes {
	RAILTYPE_RAIL     = 0,
	RAILTYPE_ELECTRIC = 1,
	RAILTYPE_MONO     = 2,
	RAILTYPE_MAGLEV   = 3,
	RAILTYPE_END,
	INVALID_RAILTYPE = 0xFF
} RailType;

typedef byte RailTypeMask;

static inline RailType GetRailType(TileIndex t)
{
	return (RailType)GB(_m[t].m3, 0, 4);
}

// TODO remove this by moving to the same bits as GetRailType()
static inline RailType GetRailTypeCrossing(TileIndex t)
{
	return (RailType)GB(_m[t].m4, 0, 4);
}

static inline RailType GetRailTypeOnBridge(TileIndex t)
{
	return (RailType)GB(_m[t].m3, 4, 4);
}

static inline void SetRailType(TileIndex t, RailType r)
{
	SB(_m[t].m3, 0, 4, r);
}

// TODO remove this by moving to the same bits as SetRailType()
static inline void SetRailTypeCrossing(TileIndex t, RailType r)
{
	SB(_m[t].m4, 0, 4, r);
}

static inline void SetRailTypeOnBridge(TileIndex t, RailType r)
{
	SB(_m[t].m3, 4, 4, r);
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
	TRACK_BIT_CROSS = TRACK_BIT_X     | TRACK_BIT_Y,
	TRACK_BIT_HORZ  = TRACK_BIT_UPPER | TRACK_BIT_LOWER,
	TRACK_BIT_VERT  = TRACK_BIT_LEFT  | TRACK_BIT_RIGHT,
	TRACK_BIT_3WAY_NE = TRACK_BIT_X | TRACK_BIT_UPPER | TRACK_BIT_RIGHT,
	TRACK_BIT_3WAY_SE = TRACK_BIT_Y | TRACK_BIT_LOWER | TRACK_BIT_RIGHT,
	TRACK_BIT_3WAY_SW = TRACK_BIT_X | TRACK_BIT_LOWER | TRACK_BIT_LEFT,
	TRACK_BIT_3WAY_NW = TRACK_BIT_Y | TRACK_BIT_UPPER | TRACK_BIT_LEFT,
	TRACK_BIT_ALL   = TRACK_BIT_CROSS | TRACK_BIT_HORZ | TRACK_BIT_VERT,
	TRACK_BIT_MASK  = 0x3FU
} TrackBits;

static inline TrackBits GetTrackBits(TileIndex tile)
{
	return (TrackBits)GB(_m[tile].m5, 0, 6);
}

static inline void SetTrackBits(TileIndex t, TrackBits b)
{
	SB(_m[t].m5, 0, 6, b);
}

/**
 * Returns whether the given track is present on the given tile. Tile must be
 * a plain rail tile (IsPlainRailTile()).
 */
static inline bool HasTrack(TileIndex tile, Track track)
{
	return HASBIT(GetTrackBits(tile), track);
}


static inline DiagDirection GetRailDepotDirection(TileIndex t)
{
	return (DiagDirection)GB(_m[t].m5, 0, 2);
}

static inline Track GetRailWaypointTrack(TileIndex t)
{
	return HASBIT(_m[t].m5, 0) ? TRACK_Y : TRACK_X;
}

static inline TrackBits GetRailWaypointBits(TileIndex t)
{
	return _m[t].m5 & 1 ? TRACK_BIT_Y : TRACK_BIT_X;
}

static inline void SetCustomWaypointSprite(TileIndex t)
{
	SETBIT(_m[t].m3, 4);
}

static inline void ClearCustomWaypointSprite(TileIndex t)
{
	CLRBIT(_m[t].m3, 4);
}

static inline bool IsCustomWaypoint(TileIndex t)
{
	return HASBIT(_m[t].m3, 4);
}

static inline Axis GetWaypointAxis(TileIndex t)
{
	return HASBIT(_m[t].m5, 0) ? AXIS_Y : AXIS_X;
}


typedef enum SignalType {
	SIGTYPE_NORMAL  = 0, // normal signal
	SIGTYPE_ENTRY   = 1, // presignal block entry
	SIGTYPE_EXIT    = 2, // presignal block exit
	SIGTYPE_COMBO   = 3  // presignal inter-block
} SignalType;

static inline SignalType GetSignalType(TileIndex t)
{
	assert(GetRailTileType(t) == RAIL_TILE_SIGNALS);
	return (SignalType)GB(_m[t].m4, 0, 2);
}

static inline void SetSignalType(TileIndex t, SignalType s)
{
	assert(GetRailTileType(t) == RAIL_TILE_SIGNALS);
	SB(_m[t].m4, 0, 2, s);
}

static inline bool IsPresignalEntry(TileIndex t)
{
	return GetSignalType(t) == SIGTYPE_ENTRY || GetSignalType(t) == SIGTYPE_COMBO;
}

static inline bool IsPresignalExit(TileIndex t)
{
	return GetSignalType(t) == SIGTYPE_EXIT || GetSignalType(t) == SIGTYPE_COMBO;
}

static inline void CycleSignalSide(TileIndex t, Track track)
{
	byte sig;
	byte pos = 6;
	if (track == TRACK_LOWER || track == TRACK_RIGHT) pos = 4;

	sig = GB(_m[t].m3, pos, 2);
	if (--sig == 0) sig = 3;
	SB(_m[t].m3, pos, 2, sig);
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

static inline bool IsSignalPresent(TileIndex t, byte signalbit)
{
	return HASBIT(_m[t].m3, signalbit + 4);
}

/** These are states in which a signal can be. Currently these are only two, so
 * simple boolean logic will do. But do try to compare to this enum instead of
 * normal boolean evaluation, since that will make future additions easier.
 */
typedef enum SignalStates {
	SIGNAL_STATE_RED = 0,
	SIGNAL_STATE_GREEN = 1,
} SignalState;

static inline SignalState GetSingleSignalState(TileIndex t, byte signalbit)
{
	return HASBIT(_m[t].m2, signalbit + 4);
}


typedef enum RailGroundType {
	RAIL_MAP2LO_GROUND_MASK = 0xF,
	RAIL_GROUND_BARREN = 0,
	RAIL_GROUND_GRASS = 1,
	RAIL_GROUND_FENCE_NW = 2,
	RAIL_GROUND_FENCE_SE = 3,
	RAIL_GROUND_FENCE_SENW = 4,
	RAIL_GROUND_FENCE_NE = 5,
	RAIL_GROUND_FENCE_SW = 6,
	RAIL_GROUND_FENCE_NESW = 7,
	RAIL_GROUND_FENCE_VERT1 = 8,
	RAIL_GROUND_FENCE_VERT2 = 9,
	RAIL_GROUND_FENCE_HORIZ1 = 10,
	RAIL_GROUND_FENCE_HORIZ2 = 11,
	RAIL_GROUND_ICE_DESERT = 12,
} RailGroundType;

static inline void SetRailGroundType(TileIndex t, RailGroundType rgt)
{
	if (GetRailTileType(t) == RAIL_TILE_DEPOT_WAYPOINT) {
		SB(_m[t].m4, 0, 4, rgt);
		return;
	}
	SB(_m[t].m2, 0, 4, rgt);
}

static inline RailGroundType GetRailGroundType(TileIndex t)
{
	/* TODO Unify this */
	if (GetRailTileType(t) == RAIL_TILE_DEPOT_WAYPOINT) return GB(_m[t].m4, 0, 4);
	return GB(_m[t].m2, 0, 4);
}

static inline bool IsBarrenRailGround(TileIndex t)
{
	return GetRailGroundType(t) == RAIL_GROUND_BARREN;
}

static inline bool IsSnowRailGround(TileIndex t)
{
	return GetRailGroundType(t) == RAIL_GROUND_ICE_DESERT;
}


static inline void MakeRailNormal(TileIndex t, Owner o, TrackBits b, RailType r)
{
	SetTileType(t, MP_RAILWAY);
	SetTileOwner(t, o);
	_m[t].m2 = 0;
	_m[t].m3 = r;
	_m[t].m4 = 0;
	_m[t].m5 = RAIL_TILE_NORMAL | b;
}


static inline void MakeRailDepot(TileIndex t, Owner o, DiagDirection d, RailType r)
{
	SetTileType(t, MP_RAILWAY);
	SetTileOwner(t, o);
	_m[t].m2 = 0;
	_m[t].m3 = r;
	_m[t].m4 = 0;
	_m[t].m5 = RAIL_TILE_DEPOT_WAYPOINT | RAIL_SUBTYPE_DEPOT | d;
}


static inline void MakeRailWaypoint(TileIndex t, Owner o, Axis a, RailType r, uint index)
{
	SetTileType(t, MP_RAILWAY);
	SetTileOwner(t, o);
	_m[t].m2 = index;
	_m[t].m3 = r;
	_m[t].m4 = 0;
	_m[t].m5 = RAIL_TILE_DEPOT_WAYPOINT | RAIL_SUBTYPE_WAYPOINT | a;
}

#endif
