#ifndef PBS_H
#define PBS_H

/** @file pbs.h Path-Based-Signalling header file
 *  @see pbs.c */

#include "vehicle.h"
#include "map.h"
#include "rail.h"

/**
 * constants used for pbs_mode argument of npf-functions
 */
enum pbs_modes {
	PBS_MODE_NONE = 0,    // no pbs
	PBS_MODE_GREEN = 1,   // look for green exit signal from pbs block
	PBS_MODE_ANY = 2,     // look for any exit signal from block
};

/**
 * constants used for v->u.rail.pbs_status
 */
enum PBSStatus {
	PBS_STAT_NONE = 0,
	PBS_STAT_HAS_PATH = 1,
	PBS_STAT_NEED_PATH = 2,
};


void PBSReserveTrack(TileIndex tile, Track track);
/**<
 * Marks a track as reserved.
 * @param tile The tile of the track.
 * @param track The track to reserve, valid values 0-5.
 */

byte PBSTileReserved(TileIndex tile);
/**<
 * Check which tracks are reserved on a tile.
 * @param tile The tile which you want to check.
 * @return The tracks reserved on that tile, each of the bits 0-5 is set when the corresponding track is reserved.
 */

uint16 PBSTileUnavail(TileIndex tile);
/**<
 * Check which trackdirs are unavailable due to reserved tracks on a tile.
 * @param tile The tile which you want to check.
 * @return The tracks reserved on that tile, each of the bits 0-5,8-13 is set when the corresponding trackdir is unavailable.
 */

void PBSClearTrack(TileIndex tile, Track track);
/**<
 * Unreserves a track.
 * @param tile The tile of the track.
 * @param track The track to unreserve, valid values 0-5.
 */

void PBSClearPath(TileIndex tile, Trackdir trackdir, TileIndex end_tile, Trackdir end_trackdir);
/**<
 * Follows a planned(reserved) path, and unreserves the tracks.
 * @param tile The tile on which the path starts
 * @param trackdir The trackdirection in which the path starts
 * @param end_tile The tile on which the path ends
 * @param end_trackdir The trackdirection in which the path ends
 */

bool PBSIsPbsSignal(TileIndex tile, Trackdir trackdir);
/**<
 * Checks if there are pbs signals on a track.
 * @param tile The tile you want to check
 * @param trackdir The trackdir you want to check
 * @return True when there are pbs signals on that tile
 */

bool PBSIsPbsSegment(uint tile, Trackdir trackdir);
/**<
 * Checks if a signal/depot leads to a pbs block.
 * This means that the block needs to have at least 1 signal, and that all signals in it need to be pbs signals.
 * @param tile The tile to check
 * @param trackdir The direction in which to check
 * @return True when the depot is inside a pbs block
 */

#endif
