/* $Id$ */

#ifndef ROAD_H
#define ROAD_H

#include "macros.h"
#include "rail.h"
#include "tile.h"

typedef enum RoadBits {
	ROAD_NW  = 1,
	ROAD_SW  = 2,
	ROAD_SE  = 4,
	ROAD_NE  = 8,
	ROAD_X   = ROAD_SW | ROAD_NE,
	ROAD_Y   = ROAD_NW | ROAD_SE,
	ROAD_ALL = ROAD_X  | ROAD_Y
} RoadBits;

static inline RoadBits ComplementRoadBits(RoadBits r)
{
	return ROAD_ALL ^ r;
}

static inline RoadBits GetRoadBits(TileIndex tile)
{
	return GB(_m[tile].m5, 0, 4);
}

static inline RoadBits GetCrossingRoadBits(TileIndex tile)
{
	return _m[tile].m5 & 8 ? ROAD_Y : ROAD_X;
}

static inline TrackBits GetCrossingRailBits(TileIndex tile)
{
	return _m[tile].m5 & 8 ? TRACK_BIT_X : TRACK_BIT_Y;
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


static inline void MakeRoadNormal(TileIndex t, Owner owner, RoadBits bits, uint town)
{
	SetTileType(t, MP_STREET);
	SetTileOwner(t, owner);
	_m[t].m2 = town;
	_m[t].m3 = 0;
	_m[t].m4 = 0 << 7 | 0 << 4 | 0;
	_m[t].m5 = ROAD_NORMAL << 4 | bits;
}


static inline void MakeRoadCrossing(TileIndex t, Owner road, Owner rail, Axis roaddir, RailType rt, uint town)
{
	SetTileType(t, MP_STREET);
	SetTileOwner(t, rail);
	_m[t].m2 = town;
	_m[t].m3 = road;
	_m[t].m4 = 0 << 7 | 0 << 4 | rt;
	_m[t].m5 = ROAD_CROSSING << 4 | roaddir << 3 | 0 << 2;
}


static inline void MakeRoadDepot(TileIndex t, Owner owner, DiagDirection dir)
{
	SetTileType(t, MP_STREET);
	SetTileOwner(t, owner);
	_m[t].m2 = 0;
	_m[t].m3 = 0;
	_m[t].m4 = 0;
	_m[t].m5 = ROAD_DEPOT << 4 | dir;
}

#endif
