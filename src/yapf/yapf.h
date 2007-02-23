/* $Id$ */

/** @file yapf.h */

#ifndef  YAPF_H
#define  YAPF_H

#include "../debug.h"

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
 * @param tracks   available tracks on the new tile (to choose from)
 * @return         the best trackdir for next turn or INVALID_TRACKDIR if the path could not be found
 */
Trackdir YapfChooseRoadTrack(Vehicle *v, TileIndex tile, DiagDirection enterdir);

/** Finds the best path for given train.
 * @param v        the train that needs to find a path
 * @param tile     the tile to find the path from (should be next tile the train is about to enter)
 * @param enterdir diagonal direction which the RV will enter this new tile from
 * @param trackdirs available trackdirs on the new tile (to choose from)
 * @param no_path_found [out] true is returned if no path can be found (returned Trackdir is only a 'guess')
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
 * @v            train that needs to go to some depot
 * @max_distance max distance (number of track tiles) from the current train position
 *                  (used also as optimization - the pathfinder can stop path finding if max_distance
 *                  was reached and no depot was seen)
 * @reverse_penalty penalty that should be added for the path that requires reversing the train first
 * @depot_tile   receives the depot tile if depot was found
 * @reversed     receives true if train needs to reversed first
 * @return       the true if depot was found.
 */
bool YapfFindNearestRailDepotTwoWay(Vehicle *v, int max_distance, int reverse_penalty, TileIndex* depot_tile, bool* reversed);

/** Returns true if it is better to reverse the train before leaving station */
bool YapfCheckReverseTrain(Vehicle* v);

/** Use this function to notify YAPF that track layout (or signal configuration) has change */
void YapfNotifyTrackLayoutChange(TileIndex tile, Track track);

/** performance measurement helpers */
void* NpfBeginInterval(void);
int NpfEndInterval(void* perf);


extern int _aystar_stats_open_size;
extern int _aystar_stats_closed_size;


/** Track followers. They should help whenever any new code will need to walk through
 * tracks, road or water tiles (pathfinders, signal controllers, vehicle controllers).
 * It is an attempt to introduce API that should simplify tasks listed above.
 * If you will need to use it:
 *   1. allocate/declare FollowTrack_t structure;
 *   2. call FollowTrackInit() and provide vehicle (if relevant)
 *   3. call one of 6 FollowTrackXxxx() APIs below
 *   4. check return value (if true then continue else stop)
 *   5. look at FollowTrack_t structure for the result
 *   6. optionally repeat steps 3..5
 *   7. in case of troubles contact KUDr
 */

/** Base struct for track followers. */
typedef struct FollowTrack_t
{
	const Vehicle*      m_veh;           ///< moving vehicle
	TileIndex           m_old_tile;      ///< the origin (vehicle moved from) before move
	Trackdir            m_old_td;        ///< the trackdir (the vehicle was on) before move
	TileIndex           m_new_tile;      ///< the new tile (the vehicle has entered)
	TrackdirBits        m_new_td_bits;   ///< the new set of available trackdirs
	DiagDirection       m_exitdir;       ///< exit direction (leaving the old tile)
	bool                m_is_tunnel;     ///< last turn passed tunnel
	bool                m_is_bridge;     ///< last turn passed bridge ramp
	bool                m_is_station;    ///< last turn passed station
	int                 m_tiles_skipped; ///< number of skipped tunnel or station tiles
} FollowTrack_t;

/** Initializes FollowTrack_t structure */
void FollowTrackInit(FollowTrack_t *This, const Vehicle* v);

/** Main track follower routines */
bool FollowTrackWater    (FollowTrack_t *This, TileIndex old_tile, Trackdir old_td);
bool FollowTrackRoad     (FollowTrack_t *This, TileIndex old_tile, Trackdir old_td);
bool FollowTrackRail     (FollowTrack_t *This, TileIndex old_tile, Trackdir old_td);
bool FollowTrackWaterNo90(FollowTrack_t *This, TileIndex old_tile, Trackdir old_td);
bool FollowTrackRoadNo90 (FollowTrack_t *This, TileIndex old_tile, Trackdir old_td);
bool FollowTrackRailNo90 (FollowTrack_t *This, TileIndex old_tile, Trackdir old_td);

/** Base tile length units */
enum {
	YAPF_TILE_LENGTH = 100,
	YAPF_TILE_CORNER_LENGTH = 71
};

#endif /* YAPF_H */
