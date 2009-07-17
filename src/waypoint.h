/* $Id$ */

/** @file waypoint.h Base of waypoints. */

#ifndef WAYPOINT_H
#define WAYPOINT_H

#include "waypoint_type.h"
#include "rail_map.h"
#include "command_type.h"
#include "station_base.h"
#include "town_type.h"
#include "viewport_type.h"
#include "date_type.h"
#include "core/pool_type.hpp"

typedef Pool<Waypoint, WaypointID, 32, 64000> WaypointPool;
extern WaypointPool _waypoint_pool;

struct Waypoint : WaypointPool::PoolItem<&_waypoint_pool>, BaseStation {
	uint16 town_cn;    ///< The Nth waypoint for this town (consecutive number)

	Waypoint(TileIndex tile = INVALID_TILE) : BaseStation(tile) { }
	~Waypoint();

	void UpdateVirtCoord();

	/* virtual */ FORCEINLINE bool TileBelongsToRailStation(TileIndex tile) const
	{
		return this->delete_ctr == 0 && this->xy == tile;
	}

	/* virtual */ uint32 GetNewGRFVariable(const struct ResolverObject *object, byte variable, byte parameter, bool *available) const;

	void AssignStationSpec(uint index);

	/**
	 * Fetch a waypoint by tile
	 * @param tile Tile of waypoint
	 * @return Waypoint
	 */
	static FORCEINLINE Waypoint *GetByTile(TileIndex tile)
	{
		return Waypoint::Get(GetWaypointIndex(tile));
	}
};

#define FOR_ALL_WAYPOINTS_FROM(var, start) FOR_ALL_ITEMS_FROM(Waypoint, waypoint_index, var, start)
#define FOR_ALL_WAYPOINTS(var) FOR_ALL_WAYPOINTS_FROM(var, 0)

CommandCost RemoveTrainWaypoint(TileIndex tile, DoCommandFlag flags, bool justremove);
void ShowWaypointWindow(const Waypoint *wp);
void DrawWaypointSprite(int x, int y, int stat_id, RailType railtype);
void UpdateAllWaypointVirtCoords();

#endif /* WAYPOINT_H */
