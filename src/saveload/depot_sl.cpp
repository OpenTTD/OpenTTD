/* $Id$ */

/** @file depot_sl.cpp Code handling saving and loading of depots */

#include "../stdafx.h"
#include "../depot_base.h"

#include "saveload.h"

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
