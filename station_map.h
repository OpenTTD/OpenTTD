/* $Id$ */

#ifndef STATION_MAP_H
#define STATION_MAP_H

#include "station.h"
#include "water_map.h" /* for IsClearWaterTile */


static inline StationID GetStationIndex(TileIndex t)
{
	return (StationID)_m[t].m2;
}

static inline Station* GetStationByTile(TileIndex t)
{
	return GetStation(GetStationIndex(t));
}


enum {
	RAILWAY_BASE = 0x0,
	AIRPORT_BASE = 0x8,
	TRUCK_BASE = 0x43,
	BUS_BASE = 0x47,
	OILRIG_BASE = 0x4B,
	DOCK_BASE = 0x4C,
	DOCK_BASE_WATER_PART = 0x50,
	BUOY_BASE = 0x52,
	AIRPORT_BASE_EXTENDED = 0x53,

	BASE_END = 0x73
};

enum {
	RAILWAY_SIZE = AIRPORT_BASE - RAILWAY_BASE,
	AIRPORT_SIZE = TRUCK_BASE - AIRPORT_BASE,
	TRUCK_SIZE = BUS_BASE - TRUCK_BASE,
	BUS_SIZE = OILRIG_BASE - BUS_BASE,
	DOCK_SIZE_TOTAL = BUOY_BASE - DOCK_BASE,
	AIRPORT_SIZE_EXTENDED = BASE_END - AIRPORT_BASE_EXTENDED
};

typedef enum HangarTiles {
	HANGAR_TILE_0 = 32,
	HANGAR_TILE_1 = 65,
	HANGAR_TILE_2 = 86
} HangarTiles;

typedef enum StationType {
	STATION_RAIL,
	STATION_HANGAR,
	STATION_AIRPORT,
	STATION_TRUCK,
	STATION_BUS,
	STATION_OILRIG,
	STATION_DOCK,
	STATION_BUOY
} StationType;

StationType GetStationType(TileIndex);

static inline RoadStopType GetRoadStopType(TileIndex t)
{
	assert(GetStationType(t) == STATION_TRUCK || GetStationType(t) == STATION_BUS);
	return GetStationType(t) == STATION_TRUCK ? RS_TRUCK : RS_BUS;
}

static inline bool IsRailwayStation(TileIndex t)
{
	return _m[t].m5 < RAILWAY_BASE + RAILWAY_SIZE;
}

static inline bool IsHangar(TileIndex t)
{
	return
		_m[t].m5 == HANGAR_TILE_0 ||
		_m[t].m5 == HANGAR_TILE_1 ||
		_m[t].m5 == HANGAR_TILE_2;
}

static inline bool IsAirport(TileIndex t)
{
	return
		IS_INT_INSIDE(_m[t].m5, AIRPORT_BASE, AIRPORT_BASE + AIRPORT_SIZE) ||
		IS_INT_INSIDE(_m[t].m5, AIRPORT_BASE_EXTENDED, AIRPORT_BASE_EXTENDED + AIRPORT_SIZE_EXTENDED);
}

static inline bool IsTruckStop(TileIndex t)
{
	return IS_INT_INSIDE(_m[t].m5, TRUCK_BASE, TRUCK_BASE + TRUCK_SIZE);
}

static inline bool IsBusStop(TileIndex t)
{
	return IS_INT_INSIDE(_m[t].m5, BUS_BASE, BUS_BASE + BUS_SIZE);
}

static inline bool IsRoadStop(TileIndex t)
{
	return IsTruckStop(t) || IsBusStop(t);
}

static inline bool IsOilRig(TileIndex t)
{
	return _m[t].m5 == OILRIG_BASE;
}

static inline bool IsDock(TileIndex t)
{
	return IS_INT_INSIDE(_m[t].m5, DOCK_BASE, DOCK_BASE + DOCK_SIZE_TOTAL);
}

static inline bool IsBuoy_(TileIndex t) // XXX _ due to naming conflict
{
	return _m[t].m5 == BUOY_BASE;
}


static inline bool IsHangarTile(TileIndex t)
{
	return IsTileType(t, MP_STATION) && IsHangar(t);
}


static inline Axis GetRailStationAxis(TileIndex t)
{
	assert(IsRailwayStation(t));
	return HASBIT(_m[t].m5, 0) ? AXIS_Y : AXIS_X;
}


static inline Track GetRailStationTrack(TileIndex t)
{
	return GetRailStationAxis(t) == AXIS_X ? TRACK_X : TRACK_Y;
}


static inline DiagDirection GetDockDirection(TileIndex t)
{
	assert(IsTileType(t, MP_STATION));
	assert(_m[t].m5 > DOCK_BASE_WATER_PART);

	return (DiagDirection)(_m[t].m5 - DOCK_BASE);
}


static inline bool IsCustomStationSprite(TileIndex t)
{
	return HASBIT(_m[t].m3, 4);
}

static inline void SetCustomStationSprite(TileIndex t, byte sprite)
{
	SETBIT(_m[t].m3, 4);
	_m[t].m4 = sprite;
}

static inline uint GetCustomStationSprite(TileIndex t)
{
	return _m[t].m4;
}


static inline void MakeStation(TileIndex t, Owner o, StationID sid, byte m5)
{
	SetTileType(t, MP_STATION);
	SetTileOwner(t, o);
	_m[t].m2 = sid;
	_m[t].m3 = 0;
	_m[t].m4 = 0;
	_m[t].m5 = m5;
}

static inline void MakeRailStation(TileIndex t, Owner o, StationID sid, Axis a, byte section, RailType rt)
{
	MakeStation(t, o, sid, section + a);
	SetRailType(t, rt);
}

static inline void MakeRoadStop(TileIndex t, Owner o, StationID sid, RoadStopType rst, DiagDirection d)
{
	MakeStation(t, o, sid, (rst == RS_BUS ? BUS_BASE : TRUCK_BASE) + d);
}

static inline void MakeAirport(TileIndex t, Owner o, StationID sid, byte section)
{
	MakeStation(t, o, sid, section);
}

static inline void MakeBuoy(TileIndex t, StationID sid)
{
	MakeStation(t, OWNER_NONE, sid, BUOY_BASE);
}

static inline void MakeDock(TileIndex t, Owner o, StationID sid, DiagDirection d)
{
	MakeStation(t, o, sid, DOCK_BASE + d);
	MakeStation(t + TileOffsByDir(d), o, sid, DOCK_BASE_WATER_PART + DiagDirToAxis(d));
}

static inline void MakeOilrig(TileIndex t, StationID sid)
{
	MakeStation(t, OWNER_NONE, sid, OILRIG_BASE);
}

#endif
