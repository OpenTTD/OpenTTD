/* $Id$ */

/** @file yapf.h */

#ifndef  YAPF_H
#define  YAPF_H

#include "../debug.h"
#include "../depot_type.h"

/** Finds the best path for given ship.
 * @param v        the ship that needs to find a path
 * @param tile     the tile to find the path from (should be next tile the ship is about to enter)
 * @param enterdir diagonal direction which the ship will enter this new tile from
 * @param tracks   available tracks on the new tile (to choose from)
 * @return         the best trackdir for next turn or INVALID_TRACKDIR if the path could not be found
 */
Trackdir YapfChooseShipTrack(Vehicle *v, TileIndex tile, DiagDirection enterdir, TrackBits tracks);

/** Finds the best path for given road vehicle.
 * @param v        the RV that needs to find a path
 * @param tile     the tile to find the path from (should be next tile the RV is about to enter)
 * @param enterdir diagonal direction which the RV will enter this new tile from
 * @return         the best trackdir for next turn or INVALID_TRACKDIR if the path could not be found
 */
Trackdir YapfChooseRoadTrack(Vehicle *v, TileIndex tile, DiagDirection enterdir);

/** Finds the best path for given train.
 * @param v        the train that needs to find a path
 * @param tile     the tile to find the path from (should be next tile the train is about to enter)
 * @param enterdir diagonal direction which the RV will enter this new tile from
 * @param tracks   available trackdirs on the new tile (to choose from)
 * @param path_not_found [out] true is returned if no path can be found (returned Trackdir is only a 'guess')
 * @return         the best trackdir for next turn or INVALID_TRACKDIR if the path could not be found
 */
Trackdir YapfChooseRailTrack(Vehicle *v, TileIndex tile, DiagDirection enterdir, TrackBits tracks, bool *path_not_found);

/** Used by RV multistop feature to find the nearest road stop that has a free slot.
 * @param v      RV (its current tile will be the origin)
 * @param tile   destination tile
 * @return       distance from origin tile to the destination (number of road tiles) or UINT_MAX if path not found
 */
uint YapfRoadVehDistanceToTile(const Vehicle* v, TileIndex tile);

/** Used when user sends RV to the nearest depot or if RV needs servicing.
 * Returns the nearest depot (or NULL if depot was not found).
 */
Depot* YapfFindNearestRoadDepot(const Vehicle *v);

/** Used when user sends train to the nearest depot or if train needs servicing.
 * @param v            train that needs to go to some depot
 * @param max_distance max distance (number of track tiles) from the current train position
 *                  (used also as optimization - the pathfinder can stop path finding if max_distance
 *                  was reached and no depot was seen)
 * @param reverse_penalty penalty that should be added for the path that requires reversing the train first
 * @param depot_tile   receives the depot tile if depot was found
 * @param reversed     receives true if train needs to reversed first
 * @return       the true if depot was found.
 */
bool YapfFindNearestRailDepotTwoWay(Vehicle *v, int max_distance, int reverse_penalty, TileIndex* depot_tile, bool* reversed);

/** Returns true if it is better to reverse the train before leaving station */
bool YapfCheckReverseTrain(Vehicle* v);

/** Use this function to notify YAPF that track layout (or signal configuration) has change */
void YapfNotifyTrackLayoutChange(TileIndex tile, Track track);

/** performance measurement helpers */
void* NpfBeginInterval();
int NpfEndInterval(void* perf);


extern int _aystar_stats_open_size;
extern int _aystar_stats_closed_size;


/** Base tile length units */
enum {
	YAPF_TILE_LENGTH = 100,
	YAPF_TILE_CORNER_LENGTH = 71
};

#endif /* YAPF_H */
