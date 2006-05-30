/* $Id$ */

#ifndef  YAPF_H
#define  YAPF_H

#include "../debug.h"

Trackdir YapfChooseShipTrack(Vehicle *v, TileIndex tile, DiagDirection enterdir, TrackBits tracks);
Trackdir YapfChooseRoadTrack(Vehicle *v, TileIndex tile, DiagDirection enterdir);
Trackdir YapfChooseRailTrack(Vehicle *v, TileIndex tile, DiagDirection enterdir, TrackdirBits trackdirs);

uint YapfRoadVehDistanceToTile(const Vehicle* v, TileIndex tile);

Depot* YapfFindNearestRoadDepot(const Vehicle *v);
bool YapfFindNearestRailDepotTwoWay(Vehicle *v, int max_distance, int reverse_penalty, TileIndex* depot_tile, bool* reversed);

bool YapfCheckReverseTrain(Vehicle* v);

void YapfNotifyTrackLayoutChange(TileIndex tile, Track track);


void* NpfBeginInterval(void);
int NpfEndInterval(void* perf);

extern int _aystar_stats_open_size;
extern int _aystar_stats_closed_size;


/** Base struct for track followers. */
typedef struct FollowTrack_t
{
	const Vehicle*      m_veh;
	TileIndex     m_old_tile;
	Trackdir      m_old_td;
	TileIndex     m_new_tile;
	TrackdirBits  m_new_td_bits;
//	TrackdirBits  m_red_td_bits;
	DiagDirection m_exitdir;
	bool          m_is_tunnel;
	int           m_tunnel_tiles_skipped;
} FollowTrack_t;

/** track followers */
bool FollowTrackWater    (FollowTrack_t *This, TileIndex old_tile, Trackdir old_td);
bool FollowTrackRoad     (FollowTrack_t *This, TileIndex old_tile, Trackdir old_td);
bool FollowTrackRail     (FollowTrack_t *This, TileIndex old_tile, Trackdir old_td);
bool FollowTrackWaterNo90(FollowTrack_t *This, TileIndex old_tile, Trackdir old_td);
bool FollowTrackRoadNo90 (FollowTrack_t *This, TileIndex old_tile, Trackdir old_td);
bool FollowTrackRailNo90 (FollowTrack_t *This, TileIndex old_tile, Trackdir old_td);

enum {
	YAPF_TILE_LENGTH = 100,
	YAPF_TILE_CORNER_LENGTH = 71
};

#endif /* YAPF_H */
