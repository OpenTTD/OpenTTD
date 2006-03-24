/* $Id$ */

#include "town.h"


static inline uint GetTownIndex(TileIndex t)
{
	return _m[t].m2;
}

static inline Town* GetTownByTile(TileIndex t)
{
	return GetTown(GetTownIndex(t));
}
