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
 * Update all signs
 */
void UpdateAllWaypointVirtCoords()
{
	Waypoint *wp;

	FOR_ALL_WAYPOINTS(wp) {
		wp->UpdateVirtCoord();
	}
}

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
 * This hacks together some dummy one-shot Station structure for a waypoint.
 * @param tile on which to work
 * @return pointer to a Station
 */
Station *ComposeWaypointStation(TileIndex tile)
{
	Waypoint *wp = GetWaypointByTile(tile);

	/* instead of 'static Station stat' use byte array to avoid Station's destructor call upon exit. As
	 * a side effect, the station is not constructed now. */
	static byte stat_raw[sizeof(Station)];
	static Station &stat = *(Station*)stat_raw;

	stat.train_tile = stat.xy = wp->xy;
	stat.town = wp->town;
	stat.build_date = wp->build_date;

	return &stat;
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
	x += 33;
	y += 17;

	if (!DrawStationTile(x, y, railtype, AXIS_X, STAT_CLASS_WAYP, stat_id)) {
		DrawDefaultWaypointSprite(x, y, railtype);
	}
}

Waypoint::~Waypoint()
{
	free(this->name);

	if (CleaningPool()) return;
	DeleteWindowById(WC_WAYPOINT_VIEW, this->index);
	RemoveOrderFromAllVehicles(OT_GOTO_WAYPOINT, this->index);

	this->sign.MarkDirty();
}

void InitializeWaypoints()
{
	_waypoint_pool.CleanPool();
}
