/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file waypoint.cpp Handling of waypoints. */

#include "stdafx.h"

#include "order_func.h"
#include "window_func.h"
#include "newgrf_station.h"
#include "waypoint_base.h"
#include "viewport_kdtree.h"

#include "safeguards.h"

/**
 * Draw a waypoint
 * @param x coordinate
 * @param y coordinate
 * @param station_class Station class.
 * @param station_type Station type within class.
 * @param railtype RailType to use for
 */
void DrawWaypointSprite(int x, int y, StationClassID station_class, uint16_t station_type, RailType railtype)
{
	if (!DrawStationTile(x, y, railtype, AXIS_X, station_class, station_type)) {
		StationPickerDrawSprite(x, y, StationType::RailWaypoint, railtype, INVALID_ROADTYPE, AXIS_X);
	}
}

TileArea Waypoint::GetTileArea(StationType type) const
{
	switch (type) {
		case StationType::RailWaypoint: return this->train_station;
		case StationType::RoadWaypoint: return this->road_waypoint_area;
		case StationType::Buoy: return {this->xy, 1, 1};
		default: NOT_REACHED();
	}
}

Waypoint::~Waypoint()
{
	if (CleaningPool()) return;
	CloseWindowById(WC_WAYPOINT_VIEW, this->index);
	RemoveOrderFromAllVehicles(OT_GOTO_WAYPOINT, this->index);
	if (this->sign.kdtree_valid) _viewport_sign_kdtree.Remove(ViewportSignKdtreeItem::MakeWaypoint(this->index));
}
