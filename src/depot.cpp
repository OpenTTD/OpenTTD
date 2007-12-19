/* $Id$ */

/** @file depot.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "vehicle.h"
#include "depot.h"
#include "functions.h"
#include "landscape.h"
#include "map.h"
#include "table/strings.h"
#include "saveload.h"
#include "order.h"

DEFINE_OLD_POOL_GENERIC(Depot, Depot)

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
 * Clean up a depot
 */
Depot::~Depot()
{
	if (CleaningPool()) return;

	/* Clear the depot from all order-lists */
	RemoveOrderFromAllVehicles(OT_GOTO_DEPOT, this->index);

	/* Delete the depot-window */
	DeleteWindowById(WC_VEHICLE_DEPOT, this->xy);
	this->xy = 0;
}

void InitializeDepots()
{
	_Depot_pool.CleanPool();
	_Depot_pool.AddBlockToPool();
}


static const SaveLoad _depot_desc[] = {
	SLE_CONDVAR(Depot, xy,         SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(Depot, xy,         SLE_UINT32,                 6, SL_MAX_VERSION),
	    SLE_VAR(Depot, town_index, SLE_UINT16),
	SLE_END()
};

static void Save_DEPT()
{
	Depot *depot;

	FOR_ALL_DEPOTS(depot) {
		SlSetArrayIndex(depot->index);
		SlObject(depot, _depot_desc);
	}
}

static void Load_DEPT()
{
	int index;

	while ((index = SlIterateArray()) != -1) {
		Depot *depot = new (index) Depot();
		SlObject(depot, _depot_desc);
	}
}

extern const ChunkHandler _depot_chunk_handlers[] = {
	{ 'DEPT', Save_DEPT, Load_DEPT, CH_ARRAY | CH_LAST},
};
