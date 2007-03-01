/* $Id$ */

#ifndef RAIL_MAP_H
#define RAIL_MAP_H

#include "direction.h"
#include "rail.h"
#include "tile.h"


typedef enum RailTileType {
	RAIL_TILE_NORMAL   = 0,
	RAIL_TILE_SIGNALS  = 1,
	RAIL_TILE_WAYPOINT = 2,
	RAIL_TILE_DEPOT    = 3,
} RailTileType;

static inline RailTileType GetRailTileType(TileIndex t)
{
	assert(IsTileType(t, MP_RAILWAY));
	return (RailTileType)GB(_m[t].m5, 6, 2);
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

/**
 * Add/remove the 'has signal' bit from the RailTileType
 */
static inline void SetHasSignals(TileIndex tile, bool signals)
{
	assert(IsPlainRailTile(tile));
	SB(_m[tile].m5, 6, 1, signals);
}


static inline bool IsRailDepot(TileIndex t)
{
	return GetRailTileType(t) == RAIL_TILE_DEPOT;
}


static inline bool IsRailWaypoint(TileIndex t)
{
	return GetRailTileType(t) == RAIL_TILE_WAYPOINT;
}


static inline RailType GetRailType(TileIndex t)
{
	return (RailType)GB(_m[t].m3, 0, 4);
}

static inline void SetRailType(TileIndex t, RailType r)
{
	SB(_m[t].m3, 0, 4, r);
}


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


static inline Axis GetWaypointAxis(TileIndex t)
{
	return (Axis)GB(_m[t].m5, 0, 1);
}

static inline Track GetRailWaypointTrack(TileIndex t)
{
	return AxisToTrack(GetWaypointAxis(t));
}

static inline TrackBits GetRailWaypointBits(TileIndex t)
{
	return TrackToTrackBits(GetRailWaypointTrack(t));
}

static inline WaypointID GetWaypointIndex(TileIndex t)
{
	return (WaypointID)_m[t].m2;
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
	return (SignalType)GB(_m[t].m2, 0, 2);
}

static inline void SetSignalType(TileIndex t, SignalType s)
{
	assert(GetRailTileType(t) == RAIL_TILE_SIGNALS);
	SB(_m[t].m2, 0, 2, s);
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
	return (SignalVariant)GB(_m[t].m2, 2, 1);
}

static inline void SetSignalVariant(TileIndex t, SignalVariant v)
{
	SB(_m[t].m2, 2, 1, v);
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
	SIGNAL_STATE_RED   = 0,
	SIGNAL_STATE_GREEN = 1,
} SignalState;

static inline SignalState GetSingleSignalState(TileIndex t, byte signalbit)
{
	return (SignalState)HASBIT(_m[t].m2, signalbit + 4);
}


/**
 * Checks for the presence of signals (either way) on the given track on the
 * given rail tile.
 */
static inline bool HasSignalOnTrack(TileIndex tile, Track track)
{
	assert(IsValidTrack(track));
	return
		GetRailTileType(tile) == RAIL_TILE_SIGNALS &&
		(_m[tile].m3 & SignalOnTrack(track)) != 0;
}

/**
 * Checks for the presence of signals along the given trackdir on the given
 * rail tile.
 *
 * Along meaning if you are currently driving on the given trackdir, this is
 * the signal that is facing us (for which we stop when it's red).
 */
static inline bool HasSignalOnTrackdir(TileIndex tile, Trackdir trackdir)
{
	assert (IsValidTrackdir(trackdir));
	return
		GetRailTileType(tile) == RAIL_TILE_SIGNALS &&
		_m[tile].m3 & SignalAlongTrackdir(trackdir);
}

/**
 * Gets the state of the signal along the given trackdir.
 *
 * Along meaning if you are currently driving on the given trackdir, this is
 * the signal that is facing us (for which we stop when it's red).
 */
static inline SignalState GetSignalStateByTrackdir(TileIndex tile, Trackdir trackdir)
{
	assert(IsValidTrackdir(trackdir));
	assert(HasSignalOnTrack(tile, TrackdirToTrack(trackdir)));
	return _m[tile].m2 & SignalAlongTrackdir(trackdir) ?
		SIGNAL_STATE_GREEN : SIGNAL_STATE_RED;
}


/**
 * Return the rail type of tile, or INVALID_RAILTYPE if this is no rail tile.
 */
RailType GetTileRailType(TileIndex tile);


typedef enum RailGroundType {
	RAIL_GROUND_BARREN       =  0,
	RAIL_GROUND_GRASS        =  1,
	RAIL_GROUND_FENCE_NW     =  2,
	RAIL_GROUND_FENCE_SE     =  3,
	RAIL_GROUND_FENCE_SENW   =  4,
	RAIL_GROUND_FENCE_NE     =  5,
	RAIL_GROUND_FENCE_SW     =  6,
	RAIL_GROUND_FENCE_NESW   =  7,
	RAIL_GROUND_FENCE_VERT1  =  8,
	RAIL_GROUND_FENCE_VERT2  =  9,
	RAIL_GROUND_FENCE_HORIZ1 = 10,
	RAIL_GROUND_FENCE_HORIZ2 = 11,
	RAIL_GROUND_ICE_DESERT   = 12,
} RailGroundType;

static inline void SetRailGroundType(TileIndex t, RailGroundType rgt)
{
	SB(_m[t].m4, 0, 4, rgt);
}

static inline RailGroundType GetRailGroundType(TileIndex t)
{
	return (RailGroundType)GB(_m[t].m4, 0, 4);
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
	_m[t].m5 = RAIL_TILE_NORMAL << 6 | b;
}


static inline void MakeRailDepot(TileIndex t, Owner o, DiagDirection d, RailType r)
{
	SetTileType(t, MP_RAILWAY);
	SetTileOwner(t, o);
	_m[t].m2 = 0;
	_m[t].m3 = r;
	_m[t].m4 = 0;
	_m[t].m5 = RAIL_TILE_DEPOT << 6 | d;
}


static inline void MakeRailWaypoint(TileIndex t, Owner o, Axis a, RailType r, uint index)
{
	SetTileType(t, MP_RAILWAY);
	SetTileOwner(t, o);
	_m[t].m2 = index;
	_m[t].m3 = r;
	_m[t].m4 = 0;
	_m[t].m5 = RAIL_TILE_WAYPOINT << 6 | a;
}

#endif /* RAIL_MAP_H */
