/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

 /** @file yapf_ship_regions.h Implementation of YAPF for water regions, which are used for finding intermediate ship destinations. */

#ifndef YAPF_SHIP_REGIONS_H
#define YAPF_SHIP_REGIONS_H

#include "../../stdafx.h"
#include "../../tile_type.h"
#include "../water_regions.h"

struct Ship;

std::vector<WaterRegionPatchDesc> YapfShipFindWaterRegionPath(const Ship *v, TileIndex start_tile, int max_returned_path_length);

#endif /* YAPF_SHIP_REGIONS_H */
