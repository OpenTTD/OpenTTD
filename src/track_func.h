/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file track_func.h Different conversion functions from one kind of track to another. */

#ifndef TRACK_FUNC_H
#define TRACK_FUNC_H

#include "core/bitmath_func.hpp"
#include "track_type.h"
#include "direction_func.h"
#include "slope_func.h"

/**
 * Iterate through each set Track in a TrackBits value.
 * For more informations see FOR_EACH_SET_BIT_EX.
 *
 * @param var Loop index variable that stores fallowing set track. Must be of type Track.
 * @param track_bits The value to iterate through (any expression).
 *
 * @see FOR_EACH_SET_BIT_EX
 */
#define FOR_EACH_SET_TRACK(var, track_bits) FOR_EACH_SET_BIT_EX(Track, var, TrackBits, track_bits)

/**
 * Checks if a Track is valid.
 *
 * @param track The value to check
 * @return true if the given value is a valid track.
 * @note Use this in an assert()
 */
static inline bool IsValidTrack(Track track)
{
	return track < TRACK_END;
}

/**
 * Checks if a Trackdir is valid for road vehicles.
 *
 * @param trackdir The value to check
 * @return true if the given value is a valid Trackdir
 * @note Use this in an assert()
 */
static inline bool IsValidTrackdirForRoadVehicle(Trackdir trackdir)
{
	return trackdir < TRACKDIR_END;
}

/**
 * Checks if a Trackdir is valid for non-road vehicles.
 *
 * @param trackdir The value to check
 * @return true if the given value is a valid Trackdir
 * @note Use this in an assert()
 */
static inline bool IsValidTrackdir(Trackdir trackdir)
{
	return (1 << trackdir & TRACKDIR_BIT_MASK) != 0;
}

/**
 * Convert an Axis to the corresponding Track
 * AXIS_X -> TRACK_X
 * AXIS_Y -> TRACK_Y
 * Uses the fact that they share the same internal encoding
 *
 * @param a the axis to convert
 * @return the track corresponding to the axis
 */
static inline Track AxisToTrack(Axis a)
{
	assert(IsValidAxis(a));
	return (Track)a;
}

/**
 * Maps a Track to the corresponding TrackBits value
 * @param track the track to convert
 * @return the converted TrackBits value of the track
 */
static inline TrackBits TrackToTrackBits(Track track)
{
	assert(IsValidTrack(track));
	return (TrackBits)(1 << track);
}

/**
 * Maps an Axis to the corresponding TrackBits value
 * @param a the axis to convert
 * @return the converted TrackBits value of the axis
 */
static inline TrackBits AxisToTrackBits(Axis a)
{
	return TrackToTrackBits(AxisToTrack(a));
}

/**
 * Returns a single horizontal/vertical trackbit that is in a specific tile corner.
 *
 * @param corner The corner of a tile.
 * @return The TrackBits of the track in the corner.
 */
static inline TrackBits CornerToTrackBits(Corner corner)
{
	extern const TrackBits _corner_to_trackbits[];
	assert(IsValidCorner(corner));
	return _corner_to_trackbits[corner];
}

/**
 * Maps a Trackdir to the corresponding TrackdirBits value
 * @param trackdir the track direction to convert
 * @return the converted TrackdirBits value
 */
static inline TrackdirBits TrackdirToTrackdirBits(Trackdir trackdir)
{
	assert(IsValidTrackdir(trackdir));
	return (TrackdirBits)(1 << trackdir);
}

/**
 * Removes first Track from TrackBits and returns it
 *
 * This function searchs for the first bit in the TrackBits,
 * remove this bit from the parameter and returns the found
 * bit as Track value. It returns INVALID_TRACK if the
 * parameter was TRACK_BIT_NONE or INVALID_TRACK_BIT. This
 * is basically used in while-loops to get up to 6 possible
 * tracks on a tile until the parameter becomes TRACK_BIT_NONE.
 *
 * @param tracks The value with the TrackBits
 * @return The first Track from the TrackBits value
 * @see FindFirstTrack
 */
static inline Track RemoveFirstTrack(TrackBits *tracks)
{
	if (*tracks != TRACK_BIT_NONE && *tracks != INVALID_TRACK_BIT) {
		assert((*tracks & ~TRACK_BIT_MASK) == TRACK_BIT_NONE);
		Track first = (Track)FIND_FIRST_BIT(*tracks);
		ClrBit(*tracks, first);
		return first;
	}
	return INVALID_TRACK;
}

/**
 * Removes first Trackdir from TrackdirBits and returns it
 *
 * This function searches for the first bit in the TrackdirBits parameter,
 * remove this bit from the parameter and returns the fnound bit as
 * Trackdir value. It returns INVALID_TRACKDIR if the trackdirs is
 * TRACKDIR_BIT_NONE or INVALID_TRACKDIR_BIT. This is basically used in a
 * while-loop to get all track-directions step by step until the value
 * reaches TRACKDIR_BIT_NONE.
 *
 * @param trackdirs The value with the TrackdirBits
 * @return The first Trackdir from the TrackdirBits value
 * @see FindFirstTrackdir
 */
static inline Trackdir RemoveFirstTrackdir(TrackdirBits *trackdirs)
{
	if (*trackdirs != TRACKDIR_BIT_NONE && *trackdirs != INVALID_TRACKDIR_BIT) {
		assert((*trackdirs & ~TRACKDIR_BIT_MASK) == TRACKDIR_BIT_NONE);
		Trackdir first = (Trackdir)FindFirstBit2x64(*trackdirs);
		ClrBit(*trackdirs, first);
		return first;
	}
	return INVALID_TRACKDIR;
}

/**
 * Returns first Track from TrackBits or INVALID_TRACK
 *
 * This function returns the first Track found in the TrackBits value as Track-value.
 * It returns INVALID_TRACK if the parameter is TRACK_BIT_NONE or INVALID_TRACK_BIT.
 *
 * @param tracks The TrackBits value
 * @return The first Track found or INVALID_TRACK
 * @see RemoveFirstTrack
 */
static inline Track FindFirstTrack(TrackBits tracks)
{
	return (tracks != TRACK_BIT_NONE && tracks != INVALID_TRACK_BIT) ? (Track)FIND_FIRST_BIT(tracks) : INVALID_TRACK;
}

/**
 * Converts TrackBits to Track.
 *
 * This function converts a TrackBits value to a Track value. As it
 * is not possible to convert two or more tracks to one track the
 * parameter must contain only one track or be the INVALID_TRACK_BIT value.
 *
 * @param tracks The TrackBits value to convert
 * @return The Track from the value or INVALID_TRACK
 * @pre tracks must contains only one Track or be INVALID_TRACK_BIT
 */
static inline Track TrackBitsToTrack(TrackBits tracks)
{
	assert(tracks == INVALID_TRACK_BIT || (tracks != TRACK_BIT_NONE && KillFirstBit(tracks & TRACK_BIT_MASK) == TRACK_BIT_NONE));
	return tracks != INVALID_TRACK_BIT ? (Track)FIND_FIRST_BIT(tracks & TRACK_BIT_MASK) : INVALID_TRACK;
}

/**
 * Returns first Trackdir from TrackdirBits or INVALID_TRACKDIR
 *
 * This function returns the first Trackdir in the given TrackdirBits value or
 * INVALID_TRACKDIR if the value is TRACKDIR_BIT_NONE. The TrackdirBits must
 * not be INVALID_TRACKDIR_BIT.
 *
 * @param trackdirs The TrackdirBits value
 * @return The first Trackdir from the TrackdirBits or INVALID_TRACKDIR on TRACKDIR_BIT_NONE.
 * @pre trackdirs must not be INVALID_TRACKDIR_BIT
 * @see RemoveFirstTrackdir
 */
static inline Trackdir FindFirstTrackdir(TrackdirBits trackdirs)
{
	assert((trackdirs & ~TRACKDIR_BIT_MASK) == TRACKDIR_BIT_NONE);
	return (trackdirs != TRACKDIR_BIT_NONE) ? (Trackdir)FindFirstBit2x64(trackdirs) : INVALID_TRACKDIR;
}

/*
 * Functions describing logical relations between Tracks, TrackBits, Trackdirs
 * TrackdirBits, Direction and DiagDirections.
 */

/**
 * Find the opposite track to a given track.
 *
 * TRACK_LOWER -> TRACK_UPPER and vice versa, likewise for left/right.
 * TRACK_X is mapped to TRACK_Y and reversed.
 *
 * @param t the track to convert
 * @return the opposite track
 */
static inline Track TrackToOppositeTrack(Track t)
{
	assert(IsValidTrack(t));
	return (Track)(t ^ 1);
}

/**
 * Maps a trackdir to the reverse trackdir.
 *
 * Returns the reverse trackdir of a Trackdir value. The reverse trackdir
 * is the same track with the other direction on it.
 *
 * @param trackdir The Trackdir value
 * @return The reverse trackdir
 * @pre trackdir must not be INVALID_TRACKDIR
 */
static inline Trackdir ReverseTrackdir(Trackdir trackdir)
{
	assert(IsValidTrackdirForRoadVehicle(trackdir));
	return (Trackdir)(trackdir ^ 8);
}

/**
 * Returns the Track that a given Trackdir represents
 *
 * This function filters the Track which is used in the Trackdir value and
 * returns it as a Track value.
 *
 * @param trackdir The trackdir value
 * @return The Track which is used in the value
 */
static inline Track TrackdirToTrack(Trackdir trackdir)
{
	assert(IsValidTrackdir(trackdir));
	return (Track)(trackdir & 0x7);
}

/**
 * Returns a Trackdir for the given Track
 *
 * Since every Track corresponds to two Trackdirs, we choose the
 * one which points between NE and S. Note that the actual
 * implementation is quite futile, but this might change
 * in the future.
 *
 * @param track The given Track
 * @return The Trackdir from the given Track
 */
static inline Trackdir TrackToTrackdir(Track track)
{
	assert(IsValidTrack(track));
	return (Trackdir)track;
}

/**
 * Returns a TrackdirBit mask from a given Track
 *
 * The TrackdirBit mask contains the two TrackdirBits that
 * correspond with the given Track (one for each direction).
 *
 * @param track The track to get the TrackdirBits from
 * @return The TrackdirBits which the selected tracks
 */
static inline TrackdirBits TrackToTrackdirBits(Track track)
{
	Trackdir td = TrackToTrackdir(track);
	return (TrackdirBits)(TrackdirToTrackdirBits(td) | TrackdirToTrackdirBits(ReverseTrackdir(td)));
}

/**
 * Discards all directional information from a TrackdirBits value
 *
 * Any Track which is present in either direction will be present in the result.
 *
 * @param bits The TrackdirBits to get the TrackBits from
 * @return The TrackBits
 */
static inline TrackBits TrackdirBitsToTrackBits(TrackdirBits bits)
{
	return (TrackBits)((bits | (bits >> 8)) & TRACK_BIT_MASK);
}

/**
 * Converts TrackBits to TrackdirBits while allowing both directions.
 *
 * @param bits The TrackBits
 * @return The TrackdirBits containing of bits in both directions.
 */
static inline TrackdirBits TrackBitsToTrackdirBits(TrackBits bits)
{
	return (TrackdirBits)(bits * 0x101);
}

/**
 * Returns the present-trackdir-information of a TrackStatus.
 *
 * @param ts The TrackStatus returned by GetTileTrackStatus()
 * @return the present trackdirs
 */
static inline TrackdirBits TrackStatusToTrackdirBits(TrackStatus ts)
{
	return (TrackdirBits)(ts & TRACKDIR_BIT_MASK);
}

/**
 * Returns the present-track-information of a TrackStatus.
 *
 * @param ts The TrackStatus returned by GetTileTrackStatus()
 * @return the present tracks
 */
static inline TrackBits TrackStatusToTrackBits(TrackStatus ts)
{
	return TrackdirBitsToTrackBits(TrackStatusToTrackdirBits(ts));
}

/**
 * Returns the red-signal-information of a TrackStatus.
 *
 * Note: The result may contain red signals for non-present tracks.
 *
 * @param ts The TrackStatus returned by GetTileTrackStatus()
 * @return the The trackdirs that are blocked by red-signals
 */
static inline TrackdirBits TrackStatusToRedSignals(TrackStatus ts)
{
	return (TrackdirBits)((ts >> 16) & TRACKDIR_BIT_MASK);
}

/**
 * Builds a TrackStatus
 *
 * @param trackdirbits present trackdirs
 * @param red_signals red signals
 * @return the TrackStatus representing the given information
 */
static inline TrackStatus CombineTrackStatus(TrackdirBits trackdirbits, TrackdirBits red_signals)
{
	return (TrackStatus)(trackdirbits | (red_signals << 16));
}

/**
 * Maps a trackdir to the trackdir that you will end up on if you go straight
 * ahead.
 *
 * This will be the same trackdir for diagonal trackdirs, but a
 * different (alternating) one for straight trackdirs
 *
 * @param trackdir The given trackdir
 * @return The next Trackdir value of the next tile.
 */
static inline Trackdir NextTrackdir(Trackdir trackdir)
{
	assert(IsValidTrackdir(trackdir));
	extern const Trackdir _next_trackdir[TRACKDIR_END];
	return _next_trackdir[trackdir];
}

/**
 * Maps a track to all tracks that make 90 deg turns with it.
 *
 * For the diagonal directions these are the complement of the
 * direction, for the straight directions these are the
 * two vertical or horizontal tracks, depend on the given direction
 *
 * @param track The given track
 * @return The TrackBits with the tracks marked which cross the given track by 90 deg.
 */
static inline TrackBits TrackCrossesTracks(Track track)
{
	assert(IsValidTrack(track));
	extern const TrackBits _track_crosses_tracks[TRACK_END];
	return _track_crosses_tracks[track];
}

/**
 * Maps a trackdir to the (4-way) direction the tile is exited when following
 * that trackdir.
 *
 * For the diagonal directions these are the same directions. For
 * the straight directions these are the directions from the imagined
 * base-tile to the bordering tile which will be joined if the given
 * straight direction is leaved from the base-tile.
 *
 * @param trackdir The given track direction
 * @return The direction which points to the resulting tile if following the Trackdir
 */
static inline DiagDirection TrackdirToExitdir(Trackdir trackdir)
{
	assert(IsValidTrackdirForRoadVehicle(trackdir));
	extern const DiagDirection _trackdir_to_exitdir[TRACKDIR_END];
	return _trackdir_to_exitdir[trackdir];
}

/**
 * Maps a track and an (4-way) dir to the trackdir that represents the track
 * with the exit in the given direction.
 *
 * For the diagonal tracks the resulting track direction are clear for a given
 * DiagDirection. It either matches the direction or it returns INVALID_TRACKDIR,
 * as a TRACK_X cannot be applied with DIAG_SE.
 * For the straight tracks the resulting track direction will be the
 * direction which the DiagDirection is pointing. But this will be INVALID_TRACKDIR
 * if the DiagDirection is pointing 'away' the track.
 *
 * @param track The track to apply an direction on
 * @param diagdir The DiagDirection to apply on
 * @return The resulting track direction or INVALID_TRACKDIR if not possible.
 */
static inline Trackdir TrackExitdirToTrackdir(Track track, DiagDirection diagdir)
{
	assert(IsValidTrack(track));
	assert(IsValidDiagDirection(diagdir));
	extern const Trackdir _track_exitdir_to_trackdir[TRACK_END][DIAGDIR_END];
	return _track_exitdir_to_trackdir[track][diagdir];
}

/**
 * Maps a track and an (4-way) dir to the trackdir that represents the track
 * with the entry in the given direction.
 *
 * For the diagonal tracks the return value is clear, its either the matching
 * track direction or INVALID_TRACKDIR.
 * For the straight tracks this returns the track direction which results if
 * you follow the DiagDirection and then turn by 45 deg left or right on the
 * next tile. The new direction on the new track will be the returning Trackdir
 * value. If the parameters makes no sense like the track TRACK_UPPER and the
 * direction DIAGDIR_NE (target track cannot be reached) this function returns
 * INVALID_TRACKDIR.
 *
 * @param track The target track
 * @param diagdir The direction to "come from"
 * @return the resulting Trackdir or INVALID_TRACKDIR if not possible.
 */
static inline Trackdir TrackEnterdirToTrackdir(Track track, DiagDirection diagdir)
{
	assert(IsValidTrack(track));
	assert(IsValidDiagDirection(diagdir));
	extern const Trackdir _track_enterdir_to_trackdir[TRACK_END][DIAGDIR_END];
	return _track_enterdir_to_trackdir[track][diagdir];
}

/**
 * Maps a track and a full (8-way) direction to the trackdir that represents
 * the track running in the given direction.
 */
static inline Trackdir TrackDirectionToTrackdir(Track track, Direction dir)
{
	assert(IsValidTrack(track));
	assert(IsValidDirection(dir));
	extern const Trackdir _track_direction_to_trackdir[TRACK_END][DIR_END];
	return _track_direction_to_trackdir[track][dir];
}

/**
 * Maps a (4-way) direction to the diagonal track incidating with that diagdir
 *
 * @param diagdir The direction
 * @return The resulting Track
 */
static inline Track DiagDirToDiagTrack(DiagDirection diagdir)
{
	assert(IsValidDiagDirection(diagdir));
	return (Track)(diagdir & 1);
}

/**
 * Maps a (4-way) direction to the diagonal track bits incidating with that diagdir
 *
 * @param diagdir The direction
 * @return The resulting TrackBits
 */
static inline TrackBits DiagDirToDiagTrackBits(DiagDirection diagdir)
{
	assert(IsValidDiagDirection(diagdir));
	return TrackToTrackBits(DiagDirToDiagTrack(diagdir));
}

/**
 * Maps a (4-way) direction to the diagonal trackdir that runs in that
 * direction.
 *
 * @param diagdir The direction
 * @return The resulting Trackdir direction
 */
static inline Trackdir DiagDirToDiagTrackdir(DiagDirection diagdir)
{
	assert(IsValidDiagDirection(diagdir));
	extern const Trackdir _dir_to_diag_trackdir[DIAGDIR_END];
	return _dir_to_diag_trackdir[diagdir];
}

/**
 * Returns all trackdirs that can be reached when entering a tile from a given
 * (diagonal) direction.
 *
 * This will obviously include 90 degree turns, since no information is available
 * about the exact angle of entering
 *
 * @param diagdir The joining direction
 * @return The TrackdirBits which can be used from the given direction
 * @see DiagdirReachesTracks
 */
static inline TrackdirBits DiagdirReachesTrackdirs(DiagDirection diagdir)
{
	assert(IsValidDiagDirection(diagdir));
	extern const TrackdirBits _exitdir_reaches_trackdirs[DIAGDIR_END];
	return _exitdir_reaches_trackdirs[diagdir];
}

/**
 * Returns all tracks that can be reached when entering a tile from a given
 * (diagonal) direction.
 *
 * This will obviously include 90 degree turns, since no
 * information is available about the exact angle of entering
 *
 * @param diagdir The joining direction
 * @return The tracks which can be used
 * @see DiagdirReachesTrackdirs
 */
static inline TrackBits DiagdirReachesTracks(DiagDirection diagdir) { return TrackdirBitsToTrackBits(DiagdirReachesTrackdirs(diagdir)); }

/**
 * Maps a trackdir to the trackdirs that can be reached from it (ie, when
 * entering the next tile.
 *
 * This will include 90 degree turns!
 *
 * @param trackdir The track direction which will be leaved
 * @return The track directions which can be used from this direction (in the next tile)
 */
static inline TrackdirBits TrackdirReachesTrackdirs(Trackdir trackdir)
{
	assert(IsValidTrackdir(trackdir));
	extern const TrackdirBits _exitdir_reaches_trackdirs[DIAGDIR_END];
	return _exitdir_reaches_trackdirs[TrackdirToExitdir(trackdir)];
}
/* Note that there is no direct table for this function (there used to be),
 * but it uses two simpler tables to achieve the result */

/**
 * Maps a trackdir to all trackdirs that make 90 deg turns with it.
 *
 * For the diagonal tracks this returns the track direction bits
 * of the other axis in both directions, which cannot be joined by
 * the given track direction.
 * For the straight tracks this returns all possible 90 deg turns
 * either on the current tile (which no train can joined) or on the
 * bordering tiles.
 *
 * @param trackdir The track direction
 * @return The TrackdirBits which are (more or less) 90 deg turns.
 */
static inline TrackdirBits TrackdirCrossesTrackdirs(Trackdir trackdir)
{
	assert(IsValidTrackdirForRoadVehicle(trackdir));
	extern const TrackdirBits _track_crosses_trackdirs[TRACK_END];
	return _track_crosses_trackdirs[TrackdirToTrack(trackdir)];
}

/**
 * Checks if a given Track is diagonal
 *
 * @param track The given track to check
 * @return true if diagonal, else false
 */
static inline bool IsDiagonalTrack(Track track)
{
	assert(IsValidTrack(track));
	return (track == TRACK_X) || (track == TRACK_Y);
}

/**
 * Checks if a given Trackdir is diagonal.
 *
 * @param trackdir The given trackdir
 * @return true if the trackdir use a diagonal track
 */
static inline bool IsDiagonalTrackdir(Trackdir trackdir)
{
	assert(IsValidTrackdir(trackdir));
	return IsDiagonalTrack(TrackdirToTrack(trackdir));
}


/**
 * Checks if the given tracks overlap, ie form a crossing. Basically this
 * means when there is more than one track on the tile, exept when there are
 * two parallel tracks.
 * @param  bits The tracks present.
 * @return Whether the tracks present overlap in any way.
 */
static inline bool TracksOverlap(TrackBits bits)
{
	/* With no, or only one track, there is no overlap */
	if (bits == TRACK_BIT_NONE || KillFirstBit(bits) == TRACK_BIT_NONE) return false;
	/* We know that there are at least two tracks present. When there are more
	 * than 2 tracks, they will surely overlap. When there are two, they will
	 * always overlap unless they are lower & upper or right & left. */
	return bits != TRACK_BIT_HORZ && bits != TRACK_BIT_VERT;
}

/**
 * Check if a given track is contained within or overlaps some other tracks.
 *
 * @param tracks Tracks to be tested against
 * @param track The track to test
 * @return true if the track is already in the tracks or overlaps the tracks.
 */
static inline bool TrackOverlapsTracks(TrackBits tracks, Track track)
{
	if (HasBit(tracks, track)) return true;
	return TracksOverlap(tracks | TrackToTrackBits(track));
}

/**
 * Checks whether the trackdir means that we are reversing.
 * @param dir the trackdir to check
 * @return true if it is a reversing road trackdir
 */
static inline bool IsReversingRoadTrackdir(Trackdir dir)
{
	assert(IsValidTrackdirForRoadVehicle(dir));
	return (dir & 0x07) >= 6;
}

/**
 * Checks whether the given trackdir is a straight road
 * @param dir the trackdir to check
 * @return true if it is a straight road trackdir
 */
static inline bool IsStraightRoadTrackdir(Trackdir dir)
{
	assert(IsValidTrackdirForRoadVehicle(dir));
	return (dir & 0x06) == 0;
}

/**
 * Checks whether a trackdir on a specific slope is going uphill.
 *
 * Valid for rail and road tracks.
 * Valid for tile-slopes (under foundation) and foundation-slopes (on foundation).
 *
 * @param slope The slope of the tile.
 * @param dir The trackdir of interest.
 * @return true iff the track goes upwards.
 */
static inline bool IsUphillTrackdir(Slope slope, Trackdir dir)
{
	assert(IsValidTrackdirForRoadVehicle(dir));
	extern const TrackdirBits _uphill_trackdirs[];
	return HasBit(_uphill_trackdirs[RemoveHalftileSlope(slope)], dir);
}

#endif /* TRACK_FUNC_H */
