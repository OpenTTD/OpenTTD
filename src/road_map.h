/* $Id$ */

#ifndef ROAD_MAP_H
#define ROAD_MAP_H

#include "macros.h"
#include "rail.h"
#include "road.h"
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

static inline bool IsLevelCrossing(TileIndex t)
{
	return GetRoadTileType(t) == ROAD_TILE_CROSSING;
}

static inline bool IsLevelCrossingTile(TileIndex t)
{
	return IsTileType(t, MP_STREET) && IsLevelCrossing(t);
}

static inline RoadBits GetRoadBits(TileIndex t)
{
	assert(GetRoadTileType(t) == ROAD_TILE_NORMAL);
	return (RoadBits)GB(_m[t].m5, 0, 4);
}

static inline void SetRoadBits(TileIndex t, RoadBits r)
{
	assert(GetRoadTileType(t) == ROAD_TILE_NORMAL); // XXX incomplete
	SB(_m[t].m5, 0, 4, r);
}


static inline Axis GetCrossingRoadAxis(TileIndex t)
{
	assert(GetRoadTileType(t) == ROAD_TILE_CROSSING);
	return (Axis)GB(_m[t].m5, 3, 1);
}

static inline RoadBits GetCrossingRoadBits(TileIndex tile)
{
	return GetCrossingRoadAxis(tile) == AXIS_X ? ROAD_X : ROAD_Y;
}

static inline TrackBits GetCrossingRailBits(TileIndex tile)
{
	return AxisToTrackBits(OtherAxis(GetCrossingRoadAxis(tile)));
}


// TODO swap owner of road and rail
static inline Owner GetCrossingRoadOwner(TileIndex t)
{
	assert(GetRoadTileType(t) == ROAD_TILE_CROSSING);
	return (Owner)_m[t].m3;
}

static inline void SetCrossingRoadOwner(TileIndex t, Owner o)
{
	assert(GetRoadTileType(t) == ROAD_TILE_CROSSING);
	_m[t].m3 = o;
}

static inline void UnbarCrossing(TileIndex t)
{
	assert(GetRoadTileType(t) == ROAD_TILE_CROSSING);
	CLRBIT(_m[t].m5, 2);
}

static inline void BarCrossing(TileIndex t)
{
	assert(GetRoadTileType(t) == ROAD_TILE_CROSSING);
	SETBIT(_m[t].m5, 2);
}

static inline bool IsCrossingBarred(TileIndex t)
{
	assert(GetRoadTileType(t) == ROAD_TILE_CROSSING);
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

static inline void SetRoadside(TileIndex tile, Roadside s)
{
	SB(_m[tile].m4, 4, 3, s);
}

static inline bool HasRoadWorks(TileIndex t)
{
	return GetRoadside(t) >= ROADSIDE_GRASS_ROAD_WORKS;
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
	switch (GetRoadside(t)) {
		case ROADSIDE_BARREN:
		case ROADSIDE_GRASS:  SetRoadside(t, ROADSIDE_GRASS_ROAD_WORKS); break;
		default:              SetRoadside(t, ROADSIDE_PAVED_ROAD_WORKS); break;
	}
}

static inline void TerminateRoadWorks(TileIndex t)
{
	assert(HasRoadWorks(t));
	SetRoadside(t, (Roadside)(GetRoadside(t) - ROADSIDE_GRASS_ROAD_WORKS + ROADSIDE_GRASS));
	/* Stop the counter */
	SB(_m[t].m4, 0, 4, 0);
}


static inline DiagDirection GetRoadDepotDirection(TileIndex t)
{
	assert(GetRoadTileType(t) == ROAD_TILE_DEPOT);
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


static inline void MakeRoadNormal(TileIndex t, Owner owner, RoadBits bits, TownID town)
{
	SetTileType(t, MP_STREET);
	SetTileOwner(t, owner);
	_m[t].m2 = town;
	_m[t].m3 = 0;
	_m[t].m4 = 0 << 7 | 0 << 4 | 0;
	_m[t].m5 = ROAD_TILE_NORMAL << 4 | bits;
}


static inline void MakeRoadCrossing(TileIndex t, Owner road, Owner rail, Axis roaddir, RailType rt, uint town)
{
	SetTileType(t, MP_STREET);
	SetTileOwner(t, rail);
	_m[t].m2 = town;
	_m[t].m3 = road;
	_m[t].m4 = 0 << 7 | 0 << 4 | rt;
	_m[t].m5 = ROAD_TILE_CROSSING << 4 | roaddir << 3 | 0 << 2;
}


static inline void MakeRoadDepot(TileIndex t, Owner owner, DiagDirection dir)
{
	SetTileType(t, MP_STREET);
	SetTileOwner(t, owner);
	_m[t].m2 = 0;
	_m[t].m3 = 0;
	_m[t].m4 = 0;
	_m[t].m5 = ROAD_TILE_DEPOT << 4 | dir;
}

#endif /* ROAD_MAP_H */
