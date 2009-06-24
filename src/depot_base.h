/* $Id$ */

/** @file depot_base.h Base for all depots (except hangars) */

#ifndef DEPOT_BASE_H
#define DEPOT_BASE_H

#include "tile_type.h"
#include "depot_type.h"
#include "core/pool_type.hpp"
#include "town_type.h"

typedef Pool<Depot, DepotID, 64, 64000> DepotPool;
extern DepotPool _depot_pool;

struct Depot : DepotPool::PoolItem<&_depot_pool> {
	TileIndex xy;
	TownID town_index;

	Depot(TileIndex xy = INVALID_TILE) : xy(xy) {}
	~Depot();

	static Depot *GetByTile(TileIndex tile);
};

#define FOR_ALL_DEPOTS_FROM(var, start) FOR_ALL_ITEMS_FROM(Depot, depot_index, var, start)
#define FOR_ALL_DEPOTS(var) FOR_ALL_DEPOTS_FROM(var, 0)

#endif /* DEPOT_BASE_H */
