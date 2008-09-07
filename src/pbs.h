/* $Id$ */

/** @file pbs.h PBS support routines */

#ifndef PBS_H
#define PBS_H

#include "tile_type.h"
#include "direction_type.h"
#include "track_type.h"
#include "vehicle_type.h"

TrackBits GetReservedTrackbits(TileIndex t);

void SetRailwayStationPlatformReservation(TileIndex start, DiagDirection dir, bool b);

bool TryReserveRailTrack(TileIndex tile, Track t);
void UnreserveRailTrack(TileIndex tile, Track t);

/** This struct contains information about the end of a reserved path. */
struct PBSTileInfo {
	TileIndex tile;      ///< Tile the path ends, INVALID_TILE if no valid path was found.
	Trackdir  trackdir;  ///< The reserved trackdir on the tile.
	bool      okay;      ///< True if tile is a safe waiting position, false otherwise.

	PBSTileInfo() : tile(INVALID_TILE), trackdir(INVALID_TRACKDIR), okay(false) {}
	PBSTileInfo(TileIndex _t, Trackdir _td, bool _okay) : tile(_t), trackdir(_td), okay(_okay) {}
};

PBSTileInfo FollowTrainReservation(const Vehicle *v, bool *train_on_res = NULL);
bool IsSafeWaitingPosition(const Vehicle *v, TileIndex tile, Trackdir trackdir, bool include_line_end, bool forbid_90deg = false);
bool IsWaitingPositionFree(const Vehicle *v, TileIndex tile, Trackdir trackdir, bool forbid_90deg = false);

Vehicle *GetTrainForReservation(TileIndex tile, Track track);

/**
 * Check whether some of tracks is reserved on a tile.
 *
 * @param tile the tile
 * @param tracks the tracks to test
 * @return true if at least on of tracks is reserved
 */
static inline bool HasReservedTracks(TileIndex tile, TrackBits tracks)
{
	return (GetReservedTrackbits(tile) & tracks) != TRACK_BIT_NONE;
}

#endif /* PBS_H */
