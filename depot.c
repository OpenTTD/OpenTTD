/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "depot.h"
#include "functions.h"
#include "tile.h"
#include "map.h"
#include "table/strings.h"
#include "saveload.h"
#include "order.h"

enum {
	/* Max depots: 64000 (8 * 8000) */
	DEPOT_POOL_BLOCK_SIZE_BITS = 3,       /* In bits, so (1 << 3) == 8 */
	DEPOT_POOL_MAX_BLOCKS      = 8000,
};

/**
 * Called if a new block is added to the depot-pool
 */
static void DepotPoolNewBlock(uint start_item)
{
	Depot *d;

	/* We don't use FOR_ALL here, because FOR_ALL skips invalid items.
	 * TODO - This is just a temporary stage, this will be removed. */
	for (d = GetDepot(start_item); d != NULL; d = (d->index + 1 < GetDepotPoolSize()) ? GetDepot(d->index + 1) : NULL) d->index = start_item++;
}

/* Initialize the town-pool */
MemoryPool _depot_pool = { "Depots", DEPOT_POOL_MAX_BLOCKS, DEPOT_POOL_BLOCK_SIZE_BITS, sizeof(Depot), &DepotPoolNewBlock, NULL, 0, 0, NULL };


/**
 * Gets a depot from a tile
 *
 * @return Returns the depot if the tile had a depot, else it returns NULL
 */
Depot *GetDepotByTile(TileIndex tile)
{
	Depot *depot;

	FOR_ALL_DEPOTS(depot) {
		if (depot->xy == tile) return depot;
	}

	return NULL;
}

/**
 * Allocate a new depot
 */
Depot *AllocateDepot(void)
{
	Depot *d;

	/* We don't use FOR_ALL here, because FOR_ALL skips invalid items.
	 * TODO - This is just a temporary stage, this will be removed. */
	for (d = GetDepot(0); d != NULL; d = (d->index + 1 < GetDepotPoolSize()) ? GetDepot(d->index + 1) : NULL) {
		if (!IsValidDepot(d)) {
			DepotID index = d->index;

			memset(d, 0, sizeof(Depot));
			d->index = index;

			return d;
		}
	}

	/* Check if we can add a block to the pool */
	if (AddBlockToPool(&_depot_pool)) return AllocateDepot();

	return NULL;
}

/**
 * Clean up a depot
 */
void DestroyDepot(Depot *depot)
{
	/* Clear the tile */
	DoClearSquare(depot->xy);

	/* Clear the depot from all order-lists */
	RemoveOrderFromAllVehicles(OT_GOTO_DEPOT, depot->index);

	/* Delete the depot-window */
	DeleteWindowById(WC_VEHICLE_DEPOT, depot->xy);
}

void InitializeDepots(void)
{
	CleanPool(&_depot_pool);
	AddBlockToPool(&_depot_pool);
}


static const SaveLoad _depot_desc[] = {
	SLE_CONDVAR(Depot, xy,         SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(Depot, xy,         SLE_UINT32,                 6, SL_MAX_VERSION),
	    SLE_VAR(Depot, town_index, SLE_UINT16),
	SLE_END()
};

static void Save_DEPT(void)
{
	Depot *depot;

	FOR_ALL_DEPOTS(depot) {
		SlSetArrayIndex(depot->index);
		SlObject(depot, _depot_desc);
	}
}

static void Load_DEPT(void)
{
	int index;

	while ((index = SlIterateArray()) != -1) {
		Depot *depot;

		if (!AddBlockIfNeeded(&_depot_pool, index))
			error("Depots: failed loading savegame: too many depots");

		depot = GetDepot(index);
		SlObject(depot, _depot_desc);
	}
}

const ChunkHandler _depot_chunk_handlers[] = {
	{ 'DEPT', Save_DEPT, Load_DEPT, CH_ARRAY | CH_LAST},
};
