/* $Id$ */

#ifndef STATION_MAP_H
#define STATION_MAP_H

#include "rail_map.h"
#include "station.h"

typedef byte StationGfx;

static inline StationID GetStationIndex(TileIndex t)
{
	assert(IsTileType(t, MP_STATION));
	return (StationID)_m[t].m2;
}

static inline Station* GetStationByTile(TileIndex t)
{
	return GetStation(GetStationIndex(t));
}


enum {
	GFX_RAILWAY_BASE              =   0,
	GFX_AIRPORT_BASE              =   8,
	GFX_RADAR_LARGE_FIRST         =  39,
	GFX_RADAR_LARGE_LAST          =  50,
	GFX_WINDSACK_FIRST            =  58,
	GFX_WINDSACK_LAST             =  61,
	GFX_TRUCK_BASE                =  67,
	GFX_BUS_BASE                  =  71,
	GFX_OILRIG_BASE               =  75,
	GFX_DOCK_BASE                 =  76,
	GFX_DOCK_BASE_WATER_PART      =  80,
	GFX_BUOY_BASE                 =  82,
	GFX_AIRPORT_BASE_EXTENDED     =  83,
	GFX_RADAR_INTERNATIONAL_FIRST =  90,
	GFX_RADAR_INTERNATIONAL_LAST  = 101,
	GFX_RADAR_METROPOLITAN_FIRST  = 102,
	GFX_RADAR_METROPOLITAN_LAST   = 113,
	GFX_RADAR_DISTRICTWE_FIRST    = 145,
	GFX_RADAR_DISTRICTWE_LAST     = 156,
	GFX_WINDSACK_INTERCON_FIRST   = 164,
	GFX_WINDSACK_INTERCON_LAST    = 167,
	GFX_BASE_END                  = 168
};

enum {
	RAILWAY_SIZE = GFX_AIRPORT_BASE - GFX_RAILWAY_BASE,
	AIRPORT_SIZE = GFX_TRUCK_BASE - GFX_AIRPORT_BASE,
	TRUCK_SIZE = GFX_BUS_BASE - GFX_TRUCK_BASE,
	BUS_SIZE = GFX_OILRIG_BASE - GFX_BUS_BASE,
	DOCK_SIZE_TOTAL = GFX_BUOY_BASE - GFX_DOCK_BASE,
	AIRPORT_SIZE_EXTENDED = GFX_BASE_END - GFX_AIRPORT_BASE_EXTENDED
};

typedef enum HangarTiles {
	HANGAR_TILE_0 = 32,
	HANGAR_TILE_1 = 65,
	HANGAR_TILE_2 = 86,
	HANGAR_TILE_3 = 129, // added for west facing hangar
	HANGAR_TILE_4 = 130, // added for north facing hangar
	HANGAR_TILE_5 = 131 // added for east facing hangar
} HangarTiles;

typedef enum StationType {
	STATION_RAIL,
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

static inline StationGfx GetStationGfx(TileIndex t)
{
	assert(IsTileType(t, MP_STATION));
	return _m[t].m5;
}

static inline void SetStationGfx(TileIndex t, StationGfx gfx)
{
	assert(IsTileType(t, MP_STATION));
	_m[t].m5 = gfx;
}

static inline bool IsRailwayStation(TileIndex t)
{
	return GetStationGfx(t) < GFX_RAILWAY_BASE + RAILWAY_SIZE;
}

static inline bool IsRailwayStationTile(TileIndex t)
{
	return IsTileType(t, MP_STATION) && IsRailwayStation(t);
}

static inline bool IsHangar(TileIndex t)
{
	StationGfx gfx = GetStationGfx(t);
	return
		gfx == HANGAR_TILE_0 ||
		gfx == HANGAR_TILE_1 ||
		gfx == HANGAR_TILE_2 ||
		gfx == HANGAR_TILE_3 ||
		gfx == HANGAR_TILE_4 ||
		gfx == HANGAR_TILE_5;
}

static inline bool IsAirport(TileIndex t)
{
	StationGfx gfx = GetStationGfx(t);
	return
		(IS_BYTE_INSIDE(gfx, GFX_AIRPORT_BASE, GFX_AIRPORT_BASE + AIRPORT_SIZE)) ||
		(IS_BYTE_INSIDE(gfx, GFX_AIRPORT_BASE_EXTENDED, GFX_AIRPORT_BASE_EXTENDED + AIRPORT_SIZE_EXTENDED));
}

static inline bool IsTruckStop(TileIndex t)
{
	return IS_BYTE_INSIDE(GetStationGfx(t), GFX_TRUCK_BASE, GFX_TRUCK_BASE + TRUCK_SIZE);
}

static inline bool IsBusStop(TileIndex t)
{
	return IS_BYTE_INSIDE(GetStationGfx(t), GFX_BUS_BASE, GFX_BUS_BASE + BUS_SIZE);
}

static inline bool IsRoadStop(TileIndex t)
{
	return IsTruckStop(t) || IsBusStop(t);
}

static inline bool IsRoadStopTile(TileIndex t)
{
	return IsTileType(t, MP_STATION) && IsRoadStop(t);
}

/**
 * Gets the direction the road stop entrance points towards.
 */
static inline DiagDirection GetRoadStopDir(TileIndex t)
{
	assert(IsRoadStopTile(t));
	return (DiagDirection)((GetStationGfx(t) - GFX_TRUCK_BASE) & 3);
}

static inline bool IsOilRig(TileIndex t)
{
	return GetStationGfx(t) == GFX_OILRIG_BASE;
}

static inline bool IsDock(TileIndex t)
{
	return IS_BYTE_INSIDE(GetStationGfx(t), GFX_DOCK_BASE, GFX_DOCK_BASE + DOCK_SIZE_TOTAL);
}

static inline bool IsBuoy_(TileIndex t) // XXX _ due to naming conflict
{
	return GetStationGfx(t) == GFX_BUOY_BASE;
}

static inline bool IsBuoyTile(TileIndex t)
{
	return IsTileType(t, MP_STATION) && IsBuoy_(t);
}


static inline bool IsHangarTile(TileIndex t)
{
	return IsTileType(t, MP_STATION) && IsHangar(t);
}


static inline Axis GetRailStationAxis(TileIndex t)
{
	assert(IsRailwayStation(t));
	return HASBIT(GetStationGfx(t), 0) ? AXIS_Y : AXIS_X;
}


static inline Track GetRailStationTrack(TileIndex t)
{
	return AxisToTrack(GetRailStationAxis(t));
}

static inline bool IsCompatibleTrainStationTile(TileIndex t1, TileIndex t2)
{
	assert(IsRailwayStationTile(t2));
	return
		IsRailwayStationTile(t1) &&
		IsCompatibleRail(GetRailType(t1), GetRailType(t2)) &&
		GetRailStationAxis(t1) == GetRailStationAxis(t2) &&
		!IsStationTileBlocked(t1);
}


static inline DiagDirection GetDockDirection(TileIndex t)
{
	StationGfx gfx = GetStationGfx(t);
	assert(gfx < GFX_DOCK_BASE_WATER_PART);
	return (DiagDirection)(gfx - GFX_DOCK_BASE);
}

static inline TileIndexDiffC GetDockOffset(TileIndex t)
{
	static const TileIndexDiffC buoy_offset = {0, 0};
	static const TileIndexDiffC oilrig_offset = {2, 0};
	static const TileIndexDiffC dock_offset[DIAGDIR_END] = {
		{-2,  0},
		{ 0,  2},
		{ 2,  0},
		{ 0, -2},
	};
	assert(IsTileType(t, MP_STATION));

	if (IsBuoy_(t)) return buoy_offset;
	if (IsOilRig(t)) return oilrig_offset;

	assert(IsDock(t));

	return dock_offset[GetDockDirection(t)];
}

static inline bool IsCustomStationSpecIndex(TileIndex t)
{
	assert(IsTileType(t, MP_STATION));
	return _m[t].m4 != 0;
}

static inline void SetCustomStationSpecIndex(TileIndex t, byte specindex)
{
	assert(IsTileType(t, MP_STATION));
	_m[t].m4 = specindex;
}

static inline uint GetCustomStationSpecIndex(TileIndex t)
{
	assert(IsTileType(t, MP_STATION));
	return _m[t].m4;
}

static inline void SetStationTileRandomBits(TileIndex t, byte random_bits)
{
	assert(IsTileType(t, MP_STATION));
	SB(_m[t].m3, 4, 4, random_bits);
}

static inline byte GetStationTileRandomBits(TileIndex t)
{
	assert(IsTileType(t, MP_STATION));
	return GB(_m[t].m3, 4, 4);
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
	MakeStation(t, o, sid, (rst == RS_BUS ? GFX_BUS_BASE : GFX_TRUCK_BASE) + d);
}

static inline void MakeAirport(TileIndex t, Owner o, StationID sid, byte section)
{
	MakeStation(t, o, sid, section);
}

static inline void MakeBuoy(TileIndex t, StationID sid)
{
	/* Make the owner of the buoy tile the same as the current owner of the
	 * water tile. In this way, we can reset the owner of the water to its
	 * original state when the buoy gets removed. */
	MakeStation(t, GetTileOwner(t), sid, GFX_BUOY_BASE);
}

static inline void MakeDock(TileIndex t, Owner o, StationID sid, DiagDirection d)
{
	MakeStation(t, o, sid, GFX_DOCK_BASE + d);
	MakeStation(t + TileOffsByDiagDir(d), o, sid, GFX_DOCK_BASE_WATER_PART + DiagDirToAxis(d));
}

static inline void MakeOilrig(TileIndex t, StationID sid)
{
	MakeStation(t, OWNER_NONE, sid, GFX_OILRIG_BASE);
}

#endif /* STATION_MAP_H */
