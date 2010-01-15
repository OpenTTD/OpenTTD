/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file depot_base.h Base for all depots (except hangars) */

#ifndef DEPOT_BASE_H
#define DEPOT_BASE_H

#include "depot_map.h"
#include "core/pool_type.hpp"

typedef Pool<Depot, DepotID, 64, 64000> DepotPool;
extern DepotPool _depot_pool;

struct Depot : DepotPool::PoolItem<&_depot_pool> {
	TileIndex xy;
	TownID town_index;

	Depot(TileIndex xy = INVALID_TILE) : xy(xy) {}
	~Depot();

	static FORCEINLINE Depot *GetByTile(TileIndex tile)
	{
		return Depot::Get(GetDepotIndex(tile));
	}
};

#define FOR_ALL_DEPOTS_FROM(var, start) FOR_ALL_ITEMS_FROM(Depot, depot_index, var, start)
#define FOR_ALL_DEPOTS(var) FOR_ALL_DEPOTS_FROM(var, 0)

#endif /* DEPOT_BASE_H */
