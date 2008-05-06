/* $Id$ */

/** @file depot_base.h Base for all depots (except hangars) */

#ifndef DEPOT_BASE_H
#define DEPOT_BASE_H

#include "tile_type.h"
#include "depot_type.h"
#include "oldpool.h"
#include "town_type.h"

DECLARE_OLD_POOL(Depot, Depot, 3, 8000)

struct Depot : PoolItem<Depot, DepotID, &_Depot_pool> {
	TileIndex xy;
	TownID town_index;

	Depot(TileIndex xy = 0) : xy(xy) {}
	~Depot();

	inline bool IsValid() const { return this->xy != 0; }
};

static inline bool IsValidDepotID(DepotID index)
{
	return index < GetDepotPoolSize() && GetDepot(index)->IsValid();
}

Depot *GetDepotByTile(TileIndex tile);

#define FOR_ALL_DEPOTS_FROM(d, start) for (d = GetDepot(start); d != NULL; d = (d->index + 1U < GetDepotPoolSize()) ? GetDepot(d->index + 1U) : NULL) if (d->IsValid())
#define FOR_ALL_DEPOTS(d) FOR_ALL_DEPOTS_FROM(d, 0)

#endif /* DEPOT_BASE_H */
