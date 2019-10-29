/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file road_gui.h Functions/types related to the road GUIs. */

#ifndef ROAD_GUI_H
#define ROAD_GUI_H

#include "road_type.h"
#include "tile_type.h"
#include "direction_type.h"
#include "widgets/dropdown_type.h"

struct Window *ShowBuildRoadToolbar(RoadType roadtype);
struct Window *ShowBuildRoadScenToolbar(RoadType roadtype);
void ConnectRoadToStructure(TileIndex tile, DiagDirection direction);
DropDownList GetRoadTypeDropDownList(RoadTramTypes rtts, bool for_replacement = false, bool all_option = false);
DropDownList GetScenRoadTypeDropDownList(RoadTramTypes rtts);
void InitializeRoadGUI();

#endif /* ROAD_GUI_H */
