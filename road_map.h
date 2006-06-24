/* $Id$ */

#ifndef ROAD_MAP_H
#define ROAD_MAP_H

#include "macros.h"
#include "tile.h"


typedef enum RoadTileType {
	ROAD_TILE_NORMAL,
	ROAD_TILE_CROSSING,
	ROAD_TILE_DEPOT
} RoadTileType;

static inline RoadTileType GetRoadTileType(TileIndex t)
{
	assert(IsTileType(t, MP_STREET));
	return (RoadTileType)GB(_m[t].m5, 4, 4);
}


typedef enum Roadside {
	ROADSIDE_BARREN           = 0,
	ROADSIDE_GRASS            = 1,
	ROADSIDE_PAVED            = 2,
	ROADSIDE_STREET_LIGHTS    = 3,
	ROADSIDE_TREES            = 5,
	ROADSIDE_GRASS_ROAD_WORKS = 6,
	ROADSIDE_PAVED_ROAD_WORKS = 7
} Roadside;

static inline Roadside GetRoadside(TileIndex tile)
{
	return (Roadside)GB(_m[tile].m4, 4, 3);
}

static inline bool HasRoadWorks(TileIndex t)
{
	return GetRoadside(t) >= ROADSIDE_GRASS_ROAD_WORKS;
}

#endif
