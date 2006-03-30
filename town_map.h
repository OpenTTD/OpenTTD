/* $Id$ */

#include "town.h"


static inline uint GetTownIndex(TileIndex t)
{
	assert(IsTileType(t, MP_HOUSE) || IsTileType(t, MP_STREET)); // XXX incomplete
	return _m[t].m2;
}

static inline Town* GetTownByTile(TileIndex t)
{
	return GetTown(GetTownIndex(t));
}
