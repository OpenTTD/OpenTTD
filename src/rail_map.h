/* $Id$ */

/** @file rail_map.h Hides the direct accesses to the map array with map accessors */

#ifndef RAIL_MAP_H
#define RAIL_MAP_H

#include "direction.h"
#include "rail.h"
#include "tile.h"


/** Different types of Rail-related tiles */
enum RailTileType {
	RAIL_TILE_NORMAL   = 0, ///< Normal rail tile without signals
	RAIL_TILE_SIGNALS  = 1, ///< Normal rail tile with signals
	RAIL_TILE_WAYPOINT = 2, ///< Waypoint (X or Y direction)
	RAIL_TILE_DEPOT    = 3, ///< Depot (one entrance)
};

/**
 * Returns the RailTileType (normal with or without signals,
 * waypoint or depot).
 * @param t the tile to get the information from
 * @pre IsTileType(t, MP_RAILWAY)
 * @return the RailTileType
 */
static inline RailTileType GetRailTileType(TileIndex t)
{
	assert(IsTileType(t, MP_RAILWAY));
	return (RailTileType)GB(_m[t].m5, 6, 2);
}

/**
 * Returns whether this is plain rails, with or without signals. Iow, if this
 * tiles RailTileType is RAIL_TILE_NORMAL or RAIL_TILE_SIGNALS.
 * @param t the tile to get the information from
 * @pre IsTileType(t, MP_RAILWAY)
 * @return true if and only if the tile is normal rail (with or without signals)
 */
static inline bool IsPlainRailTile(TileIndex t)
{
	RailTileType rtt = GetRailTileType(t);
	return rtt == RAIL_TILE_NORMAL || rtt == RAIL_TILE_SIGNALS;
}

/**
 * Checks if a rail tile has signals.
 * @param t the tile to get the information from
 * @pre IsTileType(t, MP_RAILWAY)
 * @return true if and only if the tile has signals
 */
static inline bool HasSignals(TileIndex t)
{
	return GetRailTileType(t) == RAIL_TILE_SIGNALS;
}

/**
 * Add/remove the 'has signal' bit from the RailTileType
 * @param tile the tile to add/remove the signals to/from
 * @param signals whether the rail tile should have signals or not
 * @pre IsPlainRailTile(tile)
 */
static inline void SetHasSignals(TileIndex tile, bool signals)
{
	assert(IsPlainRailTile(tile));
	SB(_m[tile].m5, 6, 1, signals);
}

/**
 * Is this tile a rail depot?
 * @param t the tile to get the information from
 * @pre IsTileType(t, MP_RAILWAY)
 * @return true if and only if the tile is a rail depot
 */
static inline bool IsRailDepot(TileIndex t)
{
	return GetRailTileType(t) == RAIL_TILE_DEPOT;
}

/**
 * Is this tile a rail waypoint?
 * @param t the tile to get the information from
 * @pre IsTileType(t, MP_RAILWAY)
 * @return true if and only if the tile is a rail waypoint
 */
static inline bool IsRailWaypoint(TileIndex t)
{
	return GetRailTileType(t) == RAIL_TILE_WAYPOINT;
}


/**
 * Gets the rail type of the given tile
 * @param t the tile to get the rail type from
 * @return the rail type of the tile
 */
static inline RailType GetRailType(TileIndex t)
{
	return (RailType)GB(_m[t].m3, 0, 4);
}

/**
 * Sets the track bits of the given tile
 * @param t the tile to set the track bits of
 * @param r the new track bits for the tile
 */
static inline void SetRailType(TileIndex t, RailType r)
{
	SB(_m[t].m3, 0, 4, r);
}


/**
 * Gets the rail type of the given tile
 * @param t the tile to get the rail type from
 * @return the rail type of the tile
 */
static inline TrackBits GetTrackBits(TileIndex tile)
{
	return (TrackBits)GB(_m[tile].m5, 0, 6);
}

/**
 * Sets the track bits of the given tile
 * @param t the tile to set the track bits of
 * @param b the new track bits for the tile
 */
static inline void SetTrackBits(TileIndex t, TrackBits b)
{
	SB(_m[t].m5, 0, 6, b);
}

/**
 * Returns whether the given track is present on the given tile.
 * @param tile  the tile to check the track presence of
 * @param track the track to search for on the tile
 * @pre IsPlainRailTile(tile)
 * @return true if and only if the given track exists on the tile
 */
static inline bool HasTrack(TileIndex tile, Track track)
{
	return HASBIT(GetTrackBits(tile), track);
}

/**
 * Returns the direction the depot is facing to
 * @param t the tile to get the depot facing from
 * @pre IsRailDepotTile(t)
 * @return the direction the depot is facing
 */
static inline DiagDirection GetRailDepotDirection(TileIndex t)
{
	return (DiagDirection)GB(_m[t].m5, 0, 2);
}


/**
 * Returns the axis of the waypoint
 * @param t the tile to get the waypoint axis from
 * @pre IsRailWaypointTile(t)
 * @return the axis of the waypoint
 */
static inline Axis GetWaypointAxis(TileIndex t)
{
	return (Axis)GB(_m[t].m5, 0, 1);
}

/**
 * Returns the track of the waypoint
 * @param t the tile to get the waypoint track from
 * @pre IsRailWaypointTile(t)
 * @return the track of the waypoint
 */
static inline Track GetRailWaypointTrack(TileIndex t)
{
	return AxisToTrack(GetWaypointAxis(t));
}

/**
 * Returns the track bits of the waypoint
 * @param t the tile to get the waypoint track bits from
 * @pre IsRailWaypointTile(t)
 * @return the track bits of the waypoint
 */
static inline TrackBits GetRailWaypointBits(TileIndex t)
{
	return TrackToTrackBits(GetRailWaypointTrack(t));
}

/**
 * Returns waypoint index (for the waypoint pool)
 * @param t the tile to get the waypoint index from
 * @pre IsRailWaypointTile(t)
 * @return the waypoint index
 */
static inline WaypointID GetWaypointIndex(TileIndex t)
{
	return (WaypointID)_m[t].m2;
}

/** Type of signal, i.e. how does the signal behave? */
enum SignalType {
	SIGTYPE_NORMAL  = 0, ///< normal signal
	SIGTYPE_ENTRY   = 1, ///< presignal block entry
	SIGTYPE_EXIT    = 2, ///< presignal block exit
	SIGTYPE_COMBO   = 3  ///< presignal inter-block
};

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

/** Variant of the signal, i.e. how does the signal look? */
enum SignalVariant {
	SIG_ELECTRIC  = 0, ///< Light signal
	SIG_SEMAPHORE = 1  ///< Old-fashioned semaphore signal
};

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
enum SignalState {
	SIGNAL_STATE_RED   = 0, ///< The signal is red
	SIGNAL_STATE_GREEN = 1, ///< The signal is green
};

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

/** The ground 'under' the rail */
enum RailGroundType {
	RAIL_GROUND_BARREN       =  0, ///< Nothing (dirt)
	RAIL_GROUND_GRASS        =  1, ///< Grassy
	RAIL_GROUND_FENCE_NW     =  2, ///< Grass with a fence at the NW edge
	RAIL_GROUND_FENCE_SE     =  3, ///< Grass with a fence at the SE edge
	RAIL_GROUND_FENCE_SENW   =  4, ///< Grass with a fence at the NW and SE edges
	RAIL_GROUND_FENCE_NE     =  5, ///< Grass with a fence at the NE edge
	RAIL_GROUND_FENCE_SW     =  6, ///< Grass with a fence at the SW edge
	RAIL_GROUND_FENCE_NESW   =  7, ///< Grass with a fence at the NE and SW edges
	RAIL_GROUND_FENCE_VERT1  =  8, ///< Grass with a fence at the western side
	RAIL_GROUND_FENCE_VERT2  =  9, ///< Grass with a fence at the eastern side
	RAIL_GROUND_FENCE_HORIZ1 = 10, ///< Grass with a fence at the southern side
	RAIL_GROUND_FENCE_HORIZ2 = 11, ///< Grass with a fence at the northern side
	RAIL_GROUND_ICE_DESERT   = 12, ///< Icy or sandy
};

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
