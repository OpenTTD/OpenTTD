#ifndef DEPOT_H
#define DEPOT_H

#include "pool.h"

struct Depot {
	TileIndex xy;
	uint16 town_index;
	uint16 index;
};

extern MemoryPool _depot_pool;

/**
 * Get the pointer to the depot with index 'index'
 */
static inline Depot *GetDepot(uint index)
{
	return (Depot*)GetItemFromPool(&_depot_pool, index);
}

/**
 * Get the current size of the DepotPool
 */
static inline uint16 GetDepotPoolSize(void)
{
	return _depot_pool.total_items;
}

#define FOR_ALL_DEPOTS_FROM(d, start) for (d = GetDepot(start); d != NULL; d = (d->index + 1 < GetDepotPoolSize()) ? GetDepot(d->index + 1) : NULL)
#define FOR_ALL_DEPOTS(d) FOR_ALL_DEPOTS_FROM(d, 0)

#define MIN_SERVINT_PERCENT  5
#define MAX_SERVINT_PERCENT 90
#define MIN_SERVINT_DAYS    30
#define MAX_SERVINT_DAYS   800

VARDEF TileIndex _last_built_train_depot_tile;
VARDEF TileIndex _last_built_road_depot_tile;
VARDEF TileIndex _last_built_aircraft_depot_tile;
VARDEF TileIndex _last_built_ship_depot_tile;

bool IsTrainDepotTile(TileIndex tile);
bool IsRoadDepotTile(TileIndex tile);
Depot *GetDepotByTile(uint tile);
void InitializeDepot(void);
Depot *AllocateDepot(void);
bool IsShipDepotTile(TileIndex tile);
void DoDeleteDepot(uint tile);

#endif /* DEPOT_H */
