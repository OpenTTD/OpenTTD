/* $Id$ */

#ifndef WAYPOINT_H
#define WAYPOINT_H

#include "pool.h"

struct Waypoint {
	TileIndex xy;
	uint16 index;

	uint16 town_index;
	byte town_cn;          // The Nth waypoint for this town (consecutive number)
	StringID string;       // If this is zero, town + town_cn is used for naming

	ViewportSign sign;
	uint16 build_date;
	byte stat_id;
	byte deleted;          // this is a delete counter. when it reaches 0, the waypoint struct is deleted.
};

enum {
	RAIL_TYPE_WAYPOINT = 0xC4,
	RAIL_WAYPOINT_TRACK_MASK = 1,
};

extern MemoryPool _waypoint_pool;

/**
 * Get the pointer to the waypoint with index 'index'
 */
static inline Waypoint *GetWaypoint(uint index)
{
	return (Waypoint*)GetItemFromPool(&_waypoint_pool, index);
}

/**
 * Get the current size of the WaypointPool
 */
static inline uint16 GetWaypointPoolSize(void)
{
	return _waypoint_pool.total_items;
}

static inline bool IsWaypointIndex(uint index)
{
	return index < GetWaypointPoolSize();
}

#define FOR_ALL_WAYPOINTS_FROM(wp, start) for (wp = GetWaypoint(start); wp != NULL; wp = (wp->index + 1 < GetWaypointPoolSize()) ? GetWaypoint(wp->index + 1) : NULL)
#define FOR_ALL_WAYPOINTS(wp) FOR_ALL_WAYPOINTS_FROM(wp, 0)

static inline bool IsRailWaypoint(byte m5)
{
	return (m5 & 0xFC) == 0xC4;
}

int32 RemoveTrainWaypoint(TileIndex tile, uint32 flags, bool justremove);
Station *ComposeWaypointStation(TileIndex tile);
Waypoint *GetWaypointByTile(TileIndex tile);
void ShowRenameWaypointWindow(const Waypoint *cp);
void DrawWaypointSprite(int x, int y, int image, uint railtype);
void UpdateWaypointSign(Waypoint *cp);
void FixOldWaypoints(void);
void UpdateAllWaypointSigns(void);

#endif /* WAYPOINT_H */
