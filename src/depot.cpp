/* $Id$ */

/** @file depot.cpp Handling of depots. */

#include "stdafx.h"
#include "depot_base.h"
#include "company_type.h"
#include "order_func.h"
#include "window_func.h"
#include "core/bitmath_func.hpp"
#include "tile_map.h"
#include "water_map.h"
#include "core/pool_func.hpp"

DepotPool _depot_pool("Depot");
INSTANTIATE_POOL_METHODS(Depot)

/**
 * Gets a depot from a tile
 * @param tile tile with depot
 * @return Returns the depot if the tile had a depot, else it returns NULL
 */
/* static */ Depot *Depot::GetByTile(TileIndex tile)
{
	/* A ship depot is multiple tiles. The north most tile is
	 * always the ->xy tile, so make sure we always look for
	 * the nothern tile and not the southern one. */
	if (IsShipDepotTile(tile)) {
		tile = min(tile, GetOtherShipDepotTile(tile));
	}

	Depot *depot;

	FOR_ALL_DEPOTS(depot) {
		if (depot->xy == tile) return depot;
	}

	return NULL;
}

/**
 * Clean up a depot
 */
Depot::~Depot()
{
	if (CleaningPool()) return;

	/* Clear the depot from all order-lists */
	RemoveOrderFromAllVehicles(OT_GOTO_DEPOT, this->index);

	/* Delete the depot-window */
	DeleteWindowById(WC_VEHICLE_DEPOT, this->xy);
}

void InitializeDepots()
{
	_depot_pool.CleanPool();
}
