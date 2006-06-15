/* $Id$ */

#include "tile.h"

#ifndef STATION_MAP_H
#define STATION_MAP_H

typedef byte StationGfx;


typedef enum HangarTiles {
	HANGAR_TILE_0 = 32,
	HANGAR_TILE_1 = 65,
	HANGAR_TILE_2 = 86
} HangarTiles;


static inline StationGfx GetStationGfx(TileIndex t)
{
	assert(IsTileType(t, MP_STATION));
	return _m[t].m5;
}


static inline bool IsHangar(TileIndex t)
{
	StationGfx gfx = GetStationGfx(t);
	return
		gfx == HANGAR_TILE_0 ||
		gfx == HANGAR_TILE_1 ||
		gfx == HANGAR_TILE_2;
}


static inline bool IsHangarTile(TileIndex t)
{
	return IsTileType(t, MP_STATION) && IsHangar(t);
}

#endif
