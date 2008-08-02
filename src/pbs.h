/* $Id$ */

/** @file pbs.h PBS support routines */

#ifndef PBS_H
#define PBS_H

#include "tile_type.h"
#include "direction_type.h"
#include "track_type.h"

TrackBits GetReservedTrackbits(TileIndex t);

void SetRailwayStationPlatformReservation(TileIndex start, DiagDirection dir, bool b);

bool TryReserveRailTrack(TileIndex tile, Track t);
void UnreserveRailTrack(TileIndex tile, Track t);

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
