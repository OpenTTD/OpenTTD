/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file smallmap_gui.h Smallmap GUI functions. */

#ifndef SMALLMAP_GUI_H
#define SMALLMAP_GUI_H

#include "core/geometry_type.hpp"
#include "station_type.h"
#include "tile_type.h"
#include "window_type.h"

/* set up the cargos to be displayed in the smallmap's route legend */
void BuildLinkStatsLegend();

void BuildIndustriesLegend();
void ShowSmallMap();
void BuildLandLegend();
void BuildOwnerLegend();

/** Enum for how to include the heightmap pixels/colours in small map related functions */
enum class IncludeHeightmap {
	Never,      ///< Never include the heightmap
	IfEnabled,  ///< Only include the heightmap if its enabled in the gui by the player
	Always      ///< Always include the heightmap
};

uint32_t GetSmallMapOwnerPixels(TileIndex tile, TileType t, IncludeHeightmap include_heightmap);

Point GetSmallMapStationMiddle(const Window *w, const Station *st);

#endif /* SMALLMAP_GUI_H */
