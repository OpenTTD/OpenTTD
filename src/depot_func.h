/* $Id$ */

/** @file depot_func.h Functions related to depots. */

#ifndef DEPOT_FUNC_H
#define DEPOT_FUNC_H

#include "depot_type.h"
#include "tile_type.h"
#include "vehicle_type.h"
#include "direction_type.h"
#include "slope_type.h"

void ShowDepotWindow(TileIndex tile, VehicleType type);
void InitializeDepots();

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
