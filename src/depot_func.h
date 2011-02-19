/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file depot_func.h Functions related to depots. */

#ifndef DEPOT_FUNC_H
#define DEPOT_FUNC_H

#include "tile_type.h"
#include "vehicle_type.h"
#include "direction_type.h"
#include "slope_type.h"

void ShowDepotWindow(TileIndex tile, VehicleType type);

void DeleteDepotHighlightOfVehicle(const Vehicle *v);

/**
 * Find out if the slope of the tile is suitable to build a depot of given direction
 * @param direction The direction in which the depot's exit points
 * @param tileh The slope of the tile in question
 * @return true if the construction is possible

 * This is checked by the ugly 0x4C >> direction magic, which does the following:
 * 0x4C is 0100 1100 and tileh has only bits 0..3 set (steep tiles are ruled out)
 * So: for direction (only the significant bits are shown)<p>
 * 00 (exit towards NE) we need either bit 2 or 3 set in tileh: 0x4C >> 0 = 1100<p>
 * 01 (exit towards SE) we need either bit 1 or 2 set in tileh: 0x4C >> 1 = 0110<p>
 * 02 (exit towards SW) we need either bit 0 or 1 set in tileh: 0x4C >> 2 = 0011<p>
 * 03 (exit towards NW) we need either bit 0 or 4 set in tileh: 0x4C >> 3 = 1001<p>
 * So ((0x4C >> direction) & tileh) determines whether the depot can be built on the current tileh
 */
static inline bool CanBuildDepotByTileh(DiagDirection direction, Slope tileh)
{
	return ((0x4C >> direction) & tileh) != 0;
}

#endif /* DEPOT_FUNC_H */
