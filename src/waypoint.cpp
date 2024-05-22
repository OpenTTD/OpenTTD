/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
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
		StationPickerDrawSprite(x, y, STATION_WAYPOINT, railtype, INVALID_ROADTYPE, AXIS_X);
	}
}

void Waypoint::GetTileArea(TileArea *ta, StationType type) const
{
	switch (type) {
		case STATION_WAYPOINT:
			*ta = this->train_station;
			return;

		case STATION_BUOY:
			ta->tile = this->xy;
			ta->w    = 1;
			ta->h    = 1;
			break;

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
