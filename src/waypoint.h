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

struct Waypoint : SpecializedStation<Waypoint, true> {
	uint16 town_cn;    ///< The Nth waypoint for this town (consecutive number)

	Waypoint(TileIndex tile = INVALID_TILE) : SpecializedStation<Waypoint, true>(tile) { }
	~Waypoint();

	void UpdateVirtCoord();

	/* virtual */ FORCEINLINE bool TileBelongsToRailStation(TileIndex tile) const
	{
		return IsRailWaypointTile(tile) && GetStationIndex(tile) == this->index;
	}

	/* virtual */ uint32 GetNewGRFVariable(const struct ResolverObject *object, byte variable, byte parameter, bool *available) const;

	/* virtual */ void GetTileArea(TileArea *ta, StationType type) const;
};

#define FOR_ALL_WAYPOINTS(var) FOR_ALL_BASE_STATIONS_OF_TYPE(Waypoint, var)

CommandCost RemoveTrainWaypoint(TileIndex tile, DoCommandFlag flags, bool justremove);
void ShowWaypointWindow(const Waypoint *wp);
void DrawWaypointSprite(int x, int y, int stat_id, RailType railtype);
void MakeDefaultWaypointName(Waypoint *wp);

#endif /* WAYPOINT_H */
