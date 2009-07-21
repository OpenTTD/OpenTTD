/* $Id$ */

/** @file waypoint.cpp Handling of waypoints. */

#include "stdafx.h"

#include "strings_type.h"
#include "rail.h"
#include "station_base.h"
#include "town.h"
#include "waypoint.h"
#include "window_func.h"
#include "newgrf_station.h"
#include "order_func.h"
#include "core/pool_func.hpp"

WaypointPool _waypoint_pool("Waypoint");
INSTANTIATE_POOL_METHODS(Waypoint)

/**
 * Daily loop for waypoints
 */
void WaypointsDailyLoop()
{
	Waypoint *wp;

	/* Check if we need to delete a waypoint */
	FOR_ALL_WAYPOINTS(wp) {
		if (wp->delete_ctr != 0 && --wp->delete_ctr == 0) delete wp;
	}
}

/**
 * Draw a waypoint
 * @param x coordinate
 * @param y coordinate
 * @param stat_id station id
 * @param railtype RailType to use for
 */
void DrawWaypointSprite(int x, int y, int stat_id, RailType railtype)
{
	if (!DrawStationTile(x, y, railtype, AXIS_X, STAT_CLASS_WAYP, stat_id)) {
		StationPickerDrawSprite(x, y, STATION_WAYPOINT, railtype, INVALID_ROADTYPE, AXIS_X);
	}
}

void Waypoint::GetTileArea(TileArea *ta, StationType type) const
{
	switch (type) {
		case STATION_BUOY:
		case STATION_WAYPOINT:
			break;

		default: NOT_REACHED();
	}

	ta->tile = this->xy;
	ta->w    = 1;
	ta->h    = 1;
}

Waypoint::~Waypoint()
{
	if (CleaningPool()) return;
	DeleteWindowById(WC_WAYPOINT_VIEW, this->index);
	RemoveOrderFromAllVehicles(OT_GOTO_WAYPOINT, this->index);

	this->sign.MarkDirty();
}

void InitializeWaypoints()
{
	_waypoint_pool.CleanPool();
}
