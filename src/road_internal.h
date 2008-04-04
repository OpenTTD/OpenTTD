/* $Id$ */

/** @file road_internal.h Functions used internally by the roads. */

#ifndef ROAD_INTERNAL_H
#define ROAD_INTERNAL_H

#include "tile_cmd.h"

/**
 * Clean up unneccesary RoadBits of a planed tile.
 * @param tile current tile
 * @param org_rb planed RoadBits
 * @return optimised RoadBits
 */
RoadBits CleanUpRoadBits(const TileIndex tile, RoadBits org_rb);

/**
 * Is it allowed to remove the given road bits from the given tile?
 * @param tile      the tile to remove the road from
 * @param remove    the roadbits that are going to be removed
 * @param owner     the actual owner of the roadbits of the tile
 * @param edge_road are the removed bits from a town?
 * @param rt        the road type to remove the bits from
 * @return true when it is allowed to remove the road bits
 */
bool CheckAllowRemoveRoad(TileIndex tile, RoadBits remove, Owner owner, bool *edge_road, RoadType rt);

/**
 * Draw the catenary for tram road bits
 * @param ti   information about the tile (position, slope)
 * @param tram the roadbits to draw the catenary for
 */
void DrawTramCatenary(const TileInfo *ti, RoadBits tram);

#endif /* ROAD_INTERNAL_H */
