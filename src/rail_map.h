/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file rail_map.h Hides the direct accesses to the map array with map accessors */

#ifndef RAIL_MAP_H
#define RAIL_MAP_H

#include "rail_type.h"
#include "depot_type.h"
#include "signal_func.h"
#include "track_func.h"
#include "tile_map.h"
#include "water_map.h"
#include "signal_type.h"


/** Different types of Rail-related tiles */
enum RailTileType {
	RAIL_TILE_NORMAL   = 0, ///< Normal rail tile without signals
	RAIL_TILE_SIGNALS  = 1, ///< Normal rail tile with signals
	RAIL_TILE_DEPOT    = 2, ///< Depot
};

/**
 * Returns the RailTileType (normal with or without signals,
 * waypoint or depot).
 * @param t the tile to get the information from
 * @pre IsTileType(t, MP_RAILWAY)
 * @return the RailTileType
 */
debug_inline static RailTileType GetRailTileType(Tile t)
{
	assert(IsTileType(t, MP_RAILWAY));
	return (RailTileType)GB(t.m5(), 6, 2);
}

/**
 * Returns whether this is plain rails, with or without signals. Iow, if this
 * tiles RailTileType is RAIL_TILE_NORMAL or RAIL_TILE_SIGNALS.
 * @param t the tile to get the information from
 * @pre IsTileType(t, MP_RAILWAY)
 * @return true if and only if the tile is normal rail (with or without signals)
 */
debug_inline static bool IsPlainRail(Tile t)
{
	RailTileType rtt = GetRailTileType(t);
	return rtt == RAIL_TILE_NORMAL || rtt == RAIL_TILE_SIGNALS;
}

/**
 * Checks whether the tile is a rail tile or rail tile with signals.
 * @param t the tile to get the information from
 * @return true if and only if the tile is normal rail (with or without signals)
 */
debug_inline static bool IsPlainRailTile(Tile t)
{
	return IsTileType(t, MP_RAILWAY) && IsPlainRail(t);
}


/**
 * Checks if a rail tile has signals.
 * @param t the tile to get the information from
 * @pre IsTileType(t, MP_RAILWAY)
 * @return true if and only if the tile has signals
 */
inline bool HasSignals(Tile t)
{
	return GetRailTileType(t) == RAIL_TILE_SIGNALS;
}

/**
 * Add/remove the 'has signal' bit from the RailTileType
 * @param tile the tile to add/remove the signals to/from
 * @param signals whether the rail tile should have signals or not
 * @pre IsPlainRailTile(tile)
 */
inline void SetHasSignals(Tile tile, bool signals)
{
	assert(IsPlainRailTile(tile));
	SB(tile.m5(), 6, 1, signals);
}

/**
 * Is this rail tile a rail depot?
 * @param t the tile to get the information from
 * @pre IsTileType(t, MP_RAILWAY)
 * @return true if and only if the tile is a rail depot
 */
debug_inline static bool IsRailDepot(Tile t)
{
	return GetRailTileType(t) == RAIL_TILE_DEPOT;
}

/**
 * Is this tile rail tile and a rail depot?
 * @param t the tile to get the information from
 * @return true if and only if the tile is a rail depot
 */
debug_inline static bool IsRailDepotTile(Tile t)
{
	return IsTileType(t, MP_RAILWAY) && IsRailDepot(t);
}

/**
 * Gets the rail type of the given tile
 * @param t the tile to get the rail type from
 * @return the rail type of the tile
 */
inline RailType GetRailType(Tile t)
{
	return (RailType)GB(t.m8(), 0, 6);
}

/**
 * Sets the rail type of the given tile
 * @param t the tile to set the rail type of
 * @param r the new rail type for the tile
 */
inline void SetRailType(Tile t, RailType r)
{
	SB(t.m8(), 0, 6, r);
}


/**
 * Gets the track bits of the given tile
 * @param tile the tile to get the track bits from
 * @return the track bits of the tile
 */
inline TrackBits GetTrackBits(Tile tile)
{
	assert(IsPlainRailTile(tile));
	return (TrackBits)GB(tile.m5(), 0, 6);
}

/**
 * Sets the track bits of the given tile
 * @param t the tile to set the track bits of
 * @param b the new track bits for the tile
 */
inline void SetTrackBits(Tile t, TrackBits b)
{
	assert(IsPlainRailTile(t));
	SB(t.m5(), 0, 6, b);
}

/**
 * Returns whether the given track is present on the given tile.
 * @param tile  the tile to check the track presence of
 * @param track the track to search for on the tile
 * @pre IsPlainRailTile(tile)
 * @return true if and only if the given track exists on the tile
 */
inline bool HasTrack(Tile tile, Track track)
{
	return HasBit(GetTrackBits(tile), track);
}

/**
 * Returns the direction the depot is facing to
 * @param t the tile to get the depot facing from
 * @pre IsRailDepotTile(t)
 * @return the direction the depot is facing
 */
inline DiagDirection GetRailDepotDirection(Tile t)
{
	return (DiagDirection)GB(t.m5(), 0, 2);
}

/**
 * Returns the track of a depot, ignoring direction
 * @pre IsRailDepotTile(t)
 * @param t the tile to get the depot track from
 * @return the track of the depot
 */
inline Track GetRailDepotTrack(Tile t)
{
	return DiagDirToDiagTrack(GetRailDepotDirection(t));
}


/**
 * Returns the reserved track bits of the tile
 * @pre IsPlainRailTile(t)
 * @param t the tile to query
 * @return the track bits
 */
inline TrackBits GetRailReservationTrackBits(Tile t)
{
	assert(IsPlainRailTile(t));
	uint8_t track_b = GB(t.m2(), 8, 3);
	Track track = (Track)(track_b - 1);    // map array saves Track+1
	if (track_b == 0) return TRACK_BIT_NONE;
	return (TrackBits)(TrackToTrackBits(track) | (HasBit(t.m2(), 11) ? TrackToTrackBits(TrackToOppositeTrack(track)) : 0));
}

/**
 * Sets the reserved track bits of the tile
 * @pre IsPlainRailTile(t) && !TracksOverlap(b)
 * @param t the tile to change
 * @param b the track bits
 */
inline void SetTrackReservation(Tile t, TrackBits b)
{
	assert(IsPlainRailTile(t));
	assert(b != INVALID_TRACK_BIT);
	assert(!TracksOverlap(b));
	Track track = RemoveFirstTrack(&b);
	SB(t.m2(), 8, 3, track == INVALID_TRACK ? 0 : track + 1);
	SB(t.m2(), 11, 1, (uint8_t)(b != TRACK_BIT_NONE));
}

/**
 * Try to reserve a specific track on a tile
 * @pre IsPlainRailTile(t) && HasTrack(tile, t)
 * @param tile the tile
 * @param t the rack to reserve
 * @return true if successful
 */
inline bool TryReserveTrack(Tile tile, Track t)
{
	assert(HasTrack(tile, t));
	TrackBits bits = TrackToTrackBits(t);
	TrackBits res = GetRailReservationTrackBits(tile);
	if ((res & bits) != TRACK_BIT_NONE) return false;  // already reserved
	res |= bits;
	if (TracksOverlap(res)) return false;  // crossing reservation present
	SetTrackReservation(tile, res);
	return true;
}

/**
 * Lift the reservation of a specific track on a tile
 * @pre IsPlainRailTile(t) && HasTrack(tile, t)
 * @param tile the tile
 * @param t the track to free
 */
inline void UnreserveTrack(Tile tile, Track t)
{
	assert(HasTrack(tile, t));
	TrackBits res = GetRailReservationTrackBits(tile);
	res &= ~TrackToTrackBits(t);
	SetTrackReservation(tile, res);
}

/**
 * Get the reservation state of the depot
 * @pre IsRailDepot(t)
 * @param t the depot tile
 * @return reservation state
 */
inline bool HasDepotReservation(Tile t)
{
	assert(IsRailDepot(t));
	return HasBit(t.m5(), 4);
}

/**
 * Set the reservation state of the depot
 * @pre IsRailDepot(t)
 * @param t the depot tile
 * @param b the reservation state
 */
inline void SetDepotReservation(Tile t, bool b)
{
	assert(IsRailDepot(t));
	SB(t.m5(), 4, 1, (uint8_t)b);
}

/**
 * Get the reserved track bits for a depot
 * @pre IsRailDepot(t)
 * @param t the tile
 * @return reserved track bits
 */
inline TrackBits GetDepotReservationTrackBits(Tile t)
{
	return HasDepotReservation(t) ? TrackToTrackBits(GetRailDepotTrack(t)) : TRACK_BIT_NONE;
}


inline bool IsPbsSignal(SignalType s)
{
	return s == SIGTYPE_PBS || s == SIGTYPE_PBS_ONEWAY;
}

inline SignalType GetSignalType(Tile t, Track track)
{
	assert(GetRailTileType(t) == RAIL_TILE_SIGNALS);
	uint8_t pos = (track == TRACK_LOWER || track == TRACK_RIGHT) ? 4 : 0;
	return (SignalType)GB(t.m2(), pos, 3);
}

inline void SetSignalType(Tile t, Track track, SignalType s)
{
	assert(GetRailTileType(t) == RAIL_TILE_SIGNALS);
	uint8_t pos = (track == TRACK_LOWER || track == TRACK_RIGHT) ? 4 : 0;
	SB(t.m2(), pos, 3, s);
	if (track == INVALID_TRACK) SB(t.m2(), 4, 3, s);
}

inline bool IsPresignalEntry(Tile t, Track track)
{
	return GetSignalType(t, track) == SIGTYPE_ENTRY || GetSignalType(t, track) == SIGTYPE_COMBO;
}

inline bool IsPresignalExit(Tile t, Track track)
{
	return GetSignalType(t, track) == SIGTYPE_EXIT || GetSignalType(t, track) == SIGTYPE_COMBO;
}

/** One-way signals can't be passed the 'wrong' way. */
inline bool IsOnewaySignal(Tile t, Track track)
{
	return GetSignalType(t, track) != SIGTYPE_PBS;
}

inline void CycleSignalSide(Tile t, Track track)
{
	uint8_t sig;
	uint8_t pos = (track == TRACK_LOWER || track == TRACK_RIGHT) ? 4 : 6;

	sig = GB(t.m3(), pos, 2);
	if (--sig == 0) sig = IsPbsSignal(GetSignalType(t, track)) ? 2 : 3;
	SB(t.m3(), pos, 2, sig);
}

inline SignalVariant GetSignalVariant(Tile t, Track track)
{
	uint8_t pos = (track == TRACK_LOWER || track == TRACK_RIGHT) ? 7 : 3;
	return (SignalVariant)GB(t.m2(), pos, 1);
}

inline void SetSignalVariant(Tile t, Track track, SignalVariant v)
{
	uint8_t pos = (track == TRACK_LOWER || track == TRACK_RIGHT) ? 7 : 3;
	SB(t.m2(), pos, 1, v);
	if (track == INVALID_TRACK) SB(t.m2(), 7, 1, v);
}

/**
 * Set the states of the signals (Along/AgainstTrackDir)
 * @param tile  the tile to set the states for
 * @param state the new state
 */
inline void SetSignalStates(Tile tile, uint state)
{
	SB(tile.m4(), 4, 4, state);
}

/**
 * Set the states of the signals (Along/AgainstTrackDir)
 * @param tile  the tile to set the states for
 * @return the state of the signals
 */
inline uint GetSignalStates(Tile tile)
{
	return GB(tile.m4(), 4, 4);
}

/**
 * Get the state of a single signal
 * @param t         the tile to get the signal state for
 * @param signalbit the signal
 * @return the state of the signal
 */
inline SignalState GetSingleSignalState(Tile t, uint8_t signalbit)
{
	return (SignalState)HasBit(GetSignalStates(t), signalbit);
}

/**
 * Set whether the given signals are present (Along/AgainstTrackDir)
 * @param tile    the tile to set the present signals for
 * @param signals the signals that have to be present
 */
inline void SetPresentSignals(Tile tile, uint signals)
{
	SB(tile.m3(), 4, 4, signals);
}

/**
 * Get whether the given signals are present (Along/AgainstTrackDir)
 * @param tile the tile to get the present signals for
 * @return the signals that are present
 */
inline uint GetPresentSignals(Tile tile)
{
	return GB(tile.m3(), 4, 4);
}

/**
 * Checks whether the given signals is present
 * @param t         the tile to check on
 * @param signalbit the signal
 * @return true if and only if the signal is present
 */
inline bool IsSignalPresent(Tile t, uint8_t signalbit)
{
	return HasBit(GetPresentSignals(t), signalbit);
}

/**
 * Checks for the presence of signals (either way) on the given track on the
 * given rail tile.
 */
inline bool HasSignalOnTrack(Tile tile, Track track)
{
	assert(IsValidTrack(track));
	return GetRailTileType(tile) == RAIL_TILE_SIGNALS && (GetPresentSignals(tile) & SignalOnTrack(track)) != 0;
}

/**
 * Checks for the presence of signals along the given trackdir on the given
 * rail tile.
 *
 * Along meaning if you are currently driving on the given trackdir, this is
 * the signal that is facing us (for which we stop when it's red).
 */
inline bool HasSignalOnTrackdir(Tile tile, Trackdir trackdir)
{
	assert (IsValidTrackdir(trackdir));
	return GetRailTileType(tile) == RAIL_TILE_SIGNALS && GetPresentSignals(tile) & SignalAlongTrackdir(trackdir);
}

/**
 * Gets the state of the signal along the given trackdir.
 *
 * Along meaning if you are currently driving on the given trackdir, this is
 * the signal that is facing us (for which we stop when it's red).
 */
inline SignalState GetSignalStateByTrackdir(Tile tile, Trackdir trackdir)
{
	assert(IsValidTrackdir(trackdir));
	assert(HasSignalOnTrack(tile, TrackdirToTrack(trackdir)));
	return GetSignalStates(tile) & SignalAlongTrackdir(trackdir) ?
		SIGNAL_STATE_GREEN : SIGNAL_STATE_RED;
}

/**
 * Sets the state of the signal along the given trackdir.
 */
inline void SetSignalStateByTrackdir(Tile tile, Trackdir trackdir, SignalState state)
{
	if (state == SIGNAL_STATE_GREEN) { // set 1
		SetSignalStates(tile, GetSignalStates(tile) | SignalAlongTrackdir(trackdir));
	} else {
		SetSignalStates(tile, GetSignalStates(tile) & ~SignalAlongTrackdir(trackdir));
	}
}

/**
 * Is a pbs signal present along the trackdir?
 * @param tile the tile to check
 * @param td the trackdir to check
 */
inline bool HasPbsSignalOnTrackdir(Tile tile, Trackdir td)
{
	return IsTileType(tile, MP_RAILWAY) && HasSignalOnTrackdir(tile, td) &&
			IsPbsSignal(GetSignalType(tile, TrackdirToTrack(td)));
}

/**
 * Is a one-way signal blocking the trackdir? A one-way signal on the
 * trackdir against will block, but signals on both trackdirs won't.
 * @param tile the tile to check
 * @param td the trackdir to check
 */
inline bool HasOnewaySignalBlockingTrackdir(Tile tile, Trackdir td)
{
	return IsTileType(tile, MP_RAILWAY) && HasSignalOnTrackdir(tile, ReverseTrackdir(td)) &&
			!HasSignalOnTrackdir(tile, td) && IsOnewaySignal(tile, TrackdirToTrack(td));
}


RailType GetTileRailType(Tile tile);

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
	RAIL_GROUND_FENCE_VERT1  =  8, ///< Grass with a fence at the eastern side
	RAIL_GROUND_FENCE_VERT2  =  9, ///< Grass with a fence at the western side
	RAIL_GROUND_FENCE_HORIZ1 = 10, ///< Grass with a fence at the southern side
	RAIL_GROUND_FENCE_HORIZ2 = 11, ///< Grass with a fence at the northern side
	RAIL_GROUND_ICE_DESERT   = 12, ///< Icy or sandy
	RAIL_GROUND_WATER        = 13, ///< Grass with a fence and shore or water on the free halftile
	RAIL_GROUND_HALF_SNOW    = 14, ///< Snow only on higher part of slope (steep or one corner raised)
};

inline void SetRailGroundType(Tile t, RailGroundType rgt)
{
	SB(t.m4(), 0, 4, rgt);
}

inline RailGroundType GetRailGroundType(Tile t)
{
	return (RailGroundType)GB(t.m4(), 0, 4);
}

inline bool IsSnowRailGround(Tile t)
{
	return GetRailGroundType(t) == RAIL_GROUND_ICE_DESERT;
}


inline void MakeRailNormal(Tile t, Owner o, TrackBits b, RailType r)
{
	SetTileType(t, MP_RAILWAY);
	SetTileOwner(t, o);
	SetDockingTile(t, false);
	t.m2() = 0;
	t.m3() = 0;
	t.m4() = 0;
	t.m5() = RAIL_TILE_NORMAL << 6 | b;
	SB(t.m6(), 2, 4, 0);
	t.m7() = 0;
	t.m8() = r;
}

/**
 * Sets the exit direction of a rail depot.
 * @param tile Tile of the depot.
 * @param dir  Direction of the depot exit.
 */
inline void SetRailDepotExitDirection(Tile tile, DiagDirection dir)
{
	assert(IsRailDepotTile(tile));
	SB(tile.m5(), 0, 2, dir);
}

/**
 * Make a rail depot.
 * @param tile      Tile to make a depot on.
 * @param owner     New owner of the depot.
 * @param depot_id  New depot ID.
 * @param dir       Direction of the depot exit.
 * @param rail_type Rail type of the depot.
 */
inline void MakeRailDepot(Tile tile, Owner owner, DepotID depot_id, DiagDirection dir, RailType rail_type)
{
	SetTileType(tile, MP_RAILWAY);
	SetTileOwner(tile, owner);
	SetDockingTile(tile, false);
	tile.m2() = depot_id;
	tile.m3() = 0;
	tile.m4() = 0;
	tile.m5() = RAIL_TILE_DEPOT << 6 | dir;
	SB(tile.m6(), 2, 4, 0);
	tile.m7() = 0;
	tile.m8() = rail_type;
}

#endif /* RAIL_MAP_H */
