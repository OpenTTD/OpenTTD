/* $Id$ */

#ifndef ROAD_MAP_H
#define ROAD_MAP_H

#include "macros.h"
#include "rail.h"
#include "road.h"
#include "tile.h"


typedef enum RoadType {
	ROAD_NORMAL,
	ROAD_CROSSING,
	ROAD_DEPOT
} RoadType;

static inline RoadType GetRoadType(TileIndex t)
{
	assert(IsTileType(t, MP_STREET));
	return GB(_m[t].m5, 4, 4);
}

static inline bool IsLevelCrossing(TileIndex t)
{
	return GetRoadType(t) == ROAD_CROSSING;
}


static inline RoadBits GetRoadBits(TileIndex t)
{
	assert(GetRoadType(t) == ROAD_NORMAL);
	return GB(_m[t].m5, 0, 4);
}

static inline void SetRoadBits(TileIndex t, RoadBits r)
{
	assert(GetRoadType(t) == ROAD_NORMAL); // XXX incomplete
	SB(_m[t].m5, 0, 4, r);
}


static inline Axis GetCrossingRoadAxis(TileIndex t)
{
	assert(GetRoadType(t) == ROAD_CROSSING);
	return (Axis)GB(_m[t].m5, 3, 1);
}

static inline RoadBits GetCrossingRoadBits(TileIndex tile)
{
	return GetCrossingRoadAxis(tile) == AXIS_X ? ROAD_X : ROAD_Y;
}

static inline TrackBits GetCrossingRailBits(TileIndex tile)
{
	return GetCrossingRoadAxis(tile) == AXIS_X ? TRACK_BIT_Y : TRACK_BIT_X;
}


// TODO swap owner of road and rail
static inline Owner GetCrossingRoadOwner(TileIndex t)
{
	assert(GetRoadType(t) == ROAD_CROSSING);
	return (Owner)_m[t].m3;
}

static inline void SetCrossingRoadOwner(TileIndex t, Owner o)
{
	assert(GetRoadType(t) == ROAD_CROSSING);
	_m[t].m3 = o;
}

static inline void UnbarCrossing(TileIndex t)
{
	assert(GetRoadType(t) == ROAD_CROSSING);
	CLRBIT(_m[t].m5, 2);
}

static inline void BarCrossing(TileIndex t)
{
	assert(GetRoadType(t) == ROAD_CROSSING);
	SETBIT(_m[t].m5, 2);
}

static inline bool IsCrossingBarred(TileIndex t)
{
	assert(GetRoadType(t) == ROAD_CROSSING);
	return HASBIT(_m[t].m5, 2);
}

#define IsOnDesert IsOnSnow
static inline bool IsOnSnow(TileIndex t)
{
	return HASBIT(_m[t].m4, 7);
}

#define ToggleDesert ToggleSnow
static inline void ToggleSnow(TileIndex t)
{
	TOGGLEBIT(_m[t].m4, 7);
}

typedef enum RoadGroundType {
	RGT_BARREN,
	RGT_GRASS,
	RGT_PAVED,
	RGT_LIGHT,
	RGT_NOT_IN_USE, /* Has something to do with fund buildings */
	RGT_ALLEY,
	RGT_ROADWORK_GRASS,
	RGT_ROADWORK_PAVED,

	RGT_ROADWORK_OFFSET = RGT_ROADWORK_GRASS - RGT_GRASS
} RoadGroundType;

static inline RoadGroundType GetGroundType(TileIndex t)
{
	return (RoadGroundType)GB(_m[t].m4, 4, 3);
}

static inline void SetGroundType(TileIndex t, RoadGroundType rgt)
{
	SB(_m[t].m4, 4, 3, rgt);
}

static inline bool HasRoadWorks(TileIndex t)
{
	return GetGroundType(t) >= RGT_ROADWORK_GRASS;
}

static inline bool IncreaseRoadWorksCounter(TileIndex t)
{
	AB(_m[t].m4, 0, 4, 1);

	return GB(_m[t].m4, 0, 4) == 15;
}

static inline void StartRoadWorks(TileIndex t)
{
	assert(!HasRoadWorks(t));
	/* Remove any trees or lamps in case or roadwork */
	SetGroundType(t, min(GetGroundType(t), RGT_PAVED) + RGT_ROADWORK_OFFSET);
}

static inline void TerminateRoadWorks(TileIndex t)
{
	assert(HasRoadWorks(t));
	SetGroundType(t, GetGroundType(t) - RGT_ROADWORK_OFFSET);
	/* Stop the counter */
	SB(_m[t].m4, 0, 4, 0);
}

static inline bool HasPavement(TileIndex t)
{
	return GetGroundType(t) >= RGT_PAVED && GetGroundType(t) != RGT_ROADWORK_GRASS;
}

static inline DiagDirection GetRoadDepotDirection(TileIndex t)
{
	assert(GetRoadType(t) == ROAD_DEPOT);
	return (DiagDirection)GB(_m[t].m5, 0, 2);
}


/**
 * Returns the RoadBits on an arbitrary tile
 * Special behavior:
 * - road depots: entrance is treated as road piece
 * - road tunnels: entrance is treated as road piece
 * - bridge ramps: start of the ramp is treated as road piece
 * - bridge middle parts: bridge itself is ignored
 */
RoadBits GetAnyRoadBits(TileIndex);


TrackBits GetAnyRoadTrackBits(TileIndex tile);


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
