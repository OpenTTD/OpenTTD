/* $Id$ */

/** @file waypoint_func.h Functions related to waypoints. */

#ifndef WAYPOINT_FUNC_H
#define WAYPOINT_FUNC_H

#include "rail_type.h"
#include "command_type.h"
#include "station_type.h"

CommandCost RemoveTrainWaypoint(TileIndex tile, DoCommandFlag flags, bool justremove);
CommandCost RemoveBuoy(TileIndex tile, DoCommandFlag flags);

void ShowWaypointWindow(const Waypoint *wp);
void DrawWaypointSprite(int x, int y, int stat_id, RailType railtype);
void MakeDefaultWaypointName(Waypoint *wp);

#endif /* WAYPOINT_FUNC_H */
