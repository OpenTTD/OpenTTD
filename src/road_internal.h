/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file road_internal.h Functions used internally by the roads. */

#ifndef ROAD_INTERNAL_H
#define ROAD_INTERNAL_H

#include "tile_cmd.h"
#include "road_type.h"

RoadBits CleanUpRoadBits(const TileIndex tile, RoadBits org_rb);

CommandCost CheckAllowRemoveRoad(TileIndex tile, RoadBits remove, Owner owner, RoadType rt, DoCommandFlag flags, bool town_check = true);

void DrawRoadCatenary(const TileInfo *ti);

#endif /* ROAD_INTERNAL_H */
