/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file road_cmd.h Road related functions. */

#ifndef ROAD_CMD_H
#define ROAD_CMD_H

#include "direction_type.h"
#include "road_type.h"

void DrawRoadDepotSprite(int x, int y, DiagDirection dir, RoadTypeIdentifier rtid);
void UpdateNearestTownForRoadTiles(bool invalidate);

#endif /* ROAD_CMD_H */
