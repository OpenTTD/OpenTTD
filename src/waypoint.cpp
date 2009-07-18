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

Waypoint::~Waypoint()
{
	if (CleaningPool()) return;
	DeleteWindowById(WC_WAYPOINT_VIEW, this->index);
	RemoveOrderFromAllVehicles(OT_GOTO_WAYPOINT, this->index);

	this->sign.MarkDirty();
}

/**
 * Assign a station spec to this waypoint.
 * @param index the index of the spec from the waypoint specs
 */
void Waypoint::AssignStationSpec(uint index)
{
	free(this->speclist);

	const StationSpec *statspec = GetCustomStationSpec(STAT_CLASS_WAYP, index);

	if (statspec != NULL) {
		this->speclist = MallocT<StationSpecList>(1);
		this->speclist->spec = statspec;
		this->speclist->grfid = statspec->grffile->grfid;
		this->speclist->localidx = statspec->localidx;
		this->num_specs = 1;
	} else {
		this->speclist = NULL;
		this->num_specs = 0;
	}
}

void InitializeWaypoints()
{
	_waypoint_pool.CleanPool();
}
