/* $Id$ */

#ifndef ROAD_H
#define ROAD_H

#include "macros.h"

typedef enum RoadBits {
	ROAD_NW  = 1,
	ROAD_SW  = 2,
	ROAD_SE  = 4,
	ROAD_NE  = 8,
	ROAD_X   = ROAD_SW | ROAD_NE,
	ROAD_Y   = ROAD_NW | ROAD_SE,
	ROAD_ALL = ROAD_X  | ROAD_Y
} RoadBits;

static inline RoadBits GetRoadBits(TileIndex tile)
{
	return GB(_m[tile].m5, 0, 4);
}

static inline RoadBits GetCrossingRoadBits(TileIndex tile)
{
	return _m[tile].m5 & 8 ? ROAD_Y : ROAD_X;
}

typedef enum RoadType {
	ROAD_NORMAL,
	ROAD_CROSSING,
	ROAD_DEPOT
} RoadType;

static inline RoadType GetRoadType(TileIndex tile)
{
	return GB(_m[tile].m5, 4, 4);
}

#endif
