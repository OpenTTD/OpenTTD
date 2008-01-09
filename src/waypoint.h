/* $Id$ */

/** @file waypoint.h */

#ifndef WAYPOINT_H
#define WAYPOINT_H

#include "oldpool.h"
#include "rail_map.h"
#include "command_type.h"

struct Waypoint;
DECLARE_OLD_POOL(Waypoint, Waypoint, 3, 8000)

struct Waypoint : PoolItem<Waypoint, WaypointID, &_Waypoint_pool> {
	TileIndex xy;      ///< Tile of waypoint

	TownID town_index; ///< Town associated with the waypoint
	byte town_cn;      ///< The Nth waypoint for this town (consecutive number)
	StringID string;   ///< If this is zero (i.e. no custom name), town + town_cn is used for naming

	ViewportSign sign; ///< Dimensions of sign (not saved)
	Date build_date;   ///< Date of construction

	byte stat_id;      ///< ID of waypoint within the waypoint class (not saved)
	uint32 grfid;      ///< ID of GRF file
	byte localidx;     ///< Index of station within GRF file

	byte deleted;      ///< Delete counter. If greater than 0 then it is decremented until it reaches 0; the waypoint is then is deleted.

	Waypoint(TileIndex tile = 0);
	~Waypoint();

	inline bool IsValid() const { return this->xy != 0; }
};

static inline bool IsValidWaypointID(WaypointID index)
{
	return index < GetWaypointPoolSize() && GetWaypoint(index)->IsValid();
}

static inline void DeleteWaypoint(Waypoint *wp)
{
	wp->~Waypoint();
}

#define FOR_ALL_WAYPOINTS_FROM(wp, start) for (wp = GetWaypoint(start); wp != NULL; wp = (wp->index + 1U < GetWaypointPoolSize()) ? GetWaypoint(wp->index + 1U) : NULL) if (wp->IsValid())
#define FOR_ALL_WAYPOINTS(wp) FOR_ALL_WAYPOINTS_FROM(wp, 0)


/**
 * Fetch a waypoint by tile
 * @param tile Tile of waypoint
 * @return Waypoint
 */
static inline Waypoint *GetWaypointByTile(TileIndex tile)
{
	assert(IsTileType(tile, MP_RAILWAY) && IsRailWaypoint(tile));
	return GetWaypoint(GetWaypointIndex(tile));
}

CommandCost RemoveTrainWaypoint(TileIndex tile, uint32 flags, bool justremove);
Station *ComposeWaypointStation(TileIndex tile);
void ShowRenameWaypointWindow(const Waypoint *cp);
void DrawWaypointSprite(int x, int y, int stat_id, RailType railtype);
void FixOldWaypoints();
void UpdateAllWaypointSigns();
void AfterLoadWaypoints();

#endif /* WAYPOINT_H */
