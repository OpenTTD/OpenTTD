/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file waypoint_func.h Functions related to waypoints. */

#ifndef WAYPOINT_FUNC_H
#define WAYPOINT_FUNC_H

#include "rail_type.h"
#include "command_type.h"
#include "station_type.h"

CommandCost RemoveBuoy(TileIndex tile, DoCommandFlag flags);

Axis GetAxisForNewWaypoint(TileIndex tile);
void ShowWaypointWindow(const Waypoint *wp);
void DrawWaypointSprite(int x, int y, int stat_id, RailType railtype);

#endif /* WAYPOINT_FUNC_H */
