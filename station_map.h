/* $Id$ */

#include "station.h"


static inline StationID GetStationIndex(TileIndex t)
{
	return (StationID)_m[t].m2;
}

static inline Station* GetStationByTile(TileIndex t)
{
	return GetStation(GetStationIndex(t));
}
