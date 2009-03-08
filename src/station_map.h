/* $Id$ */

/** @file station_map.h Maps accessors for stations. */

#ifndef STATION_MAP_H
#define STATION_MAP_H

#include "rail_map.h"
#include "road_map.h"
#include "water_map.h"
#include "station_func.h"
#include "station_base.h"
#include "rail.h"

typedef byte StationGfx;

/** Get Station ID from a tile
 * @pre Tile \t must be part of the station
 * @param t Tile to query station ID from
 * @return Station ID of the station at \a t */
static inline StationID GetStationIndex(TileIndex t)
{
	assert(IsTileType(t, MP_STATION));
	return (StationID)_m[t].m2;
}

static inline Station *GetStationByTile(TileIndex t)
{
	return GetStation(GetStationIndex(t));
}


enum {
	GFX_RADAR_LARGE_FIRST             =  31,
	GFX_RADAR_LARGE_LAST              =  42,
	GFX_WINDSACK_FIRST                =  50,
	GFX_WINDSACK_LAST                 =  53,

	GFX_DOCK_BASE_WATER_PART          =  4,
	GFX_TRUCK_BUS_DRIVETHROUGH_OFFSET =  4,

	GFX_RADAR_INTERNATIONAL_FIRST     =  66,
	GFX_RADAR_INTERNATIONAL_LAST      =  77,
	GFX_RADAR_METROPOLITAN_FIRST      =  78,
	GFX_RADAR_METROPOLITAN_LAST       =  89,
	GFX_RADAR_DISTRICTWE_FIRST        = 121,
	GFX_RADAR_DISTRICTWE_LAST         = 132,
	GFX_WINDSACK_INTERCON_FIRST       = 140,
	GFX_WINDSACK_INTERCON_LAST        = 143,
};

static inline StationType GetStationType(TileIndex t)
{
	return (StationType)GB(_m[t].m6, 3, 3);
}

static inline RoadStopType GetRoadStopType(TileIndex t)
{
	assert(GetStationType(t) == STATION_TRUCK || GetStationType(t) == STATION_BUS);
	return GetStationType(t) == STATION_TRUCK ? ROADSTOP_TRUCK : ROADSTOP_BUS;
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

static inline uint8 GetStationAnimationFrame(TileIndex t)
{
	assert(IsTileType(t, MP_STATION));
	return _me[t].m7;
}

static inline void SetStationAnimationFrame(TileIndex t, uint8 frame)
{
	assert(IsTileType(t, MP_STATION));
	_me[t].m7 = frame;
}

static inline bool IsRailwayStation(TileIndex t)
{
	return GetStationType(t) == STATION_RAIL;
}

static inline bool IsRailwayStationTile(TileIndex t)
{
	return IsTileType(t, MP_STATION) && IsRailwayStation(t);
}

static inline bool IsAirport(TileIndex t)
{
	return GetStationType(t) == STATION_AIRPORT;
}

bool IsHangar(TileIndex t);

/**
 * Is the station at \a t a truck stop?
 * @param t Tile to check
 * @return \c true if station is a truck stop, \c false otherwise */
static inline bool IsTruckStop(TileIndex t)
{
	return GetStationType(t) == STATION_TRUCK;
}

/**
 * Is the station at \a t a bus stop?
 * @param t Tile to check
 * @return \c true if station is a bus stop, \c false otherwise */
static inline bool IsBusStop(TileIndex t)
{
	return GetStationType(t) == STATION_BUS;
}

/**
 * Is the station at \a t a road station?
 * @pre Tile at \a t is a station tile
 * @param t Tile to check
 * @return \c true if station at the tile is a bus top or a truck stop, \c false otherwise */
static inline bool IsRoadStop(TileIndex t)
{
	assert(IsTileType(t, MP_STATION));
	return IsTruckStop(t) || IsBusStop(t);
}

static inline bool IsRoadStopTile(TileIndex t)
{
	return IsTileType(t, MP_STATION) && IsRoadStop(t);
}

static inline bool IsStandardRoadStopTile(TileIndex t)
{
	return IsRoadStopTile(t) && GetStationGfx(t) < GFX_TRUCK_BUS_DRIVETHROUGH_OFFSET;
}

static inline bool IsDriveThroughStopTile(TileIndex t)
{
	return IsRoadStopTile(t) && GetStationGfx(t) >= GFX_TRUCK_BUS_DRIVETHROUGH_OFFSET;
}

/**
 * Gets the direction the road stop entrance points towards.
 */
static inline DiagDirection GetRoadStopDir(TileIndex t)
{
	StationGfx gfx = GetStationGfx(t);
	assert(IsRoadStopTile(t));
	if (gfx < GFX_TRUCK_BUS_DRIVETHROUGH_OFFSET) {
		return (DiagDirection)(gfx);
	} else {
		return (DiagDirection)(gfx - GFX_TRUCK_BUS_DRIVETHROUGH_OFFSET);
	}
}

static inline bool IsOilRig(TileIndex t)
{
	return GetStationType(t) == STATION_OILRIG;
}

static inline bool IsDock(TileIndex t)
{
	return GetStationType(t) == STATION_DOCK;
}

static inline bool IsDockTile(TileIndex t)
{
	return IsTileType(t, MP_STATION) && GetStationType(t) == STATION_DOCK;
}

static inline bool IsBuoy(TileIndex t)
{
	return GetStationType(t) == STATION_BUOY;
}

static inline bool IsBuoyTile(TileIndex t)
{
	return IsTileType(t, MP_STATION) && IsBuoy(t);
}

static inline bool IsHangarTile(TileIndex t)
{
	return IsTileType(t, MP_STATION) && IsHangar(t);
}


static inline Axis GetRailStationAxis(TileIndex t)
{
	assert(IsRailwayStation(t));
	return HasBit(GetStationGfx(t), 0) ? AXIS_Y : AXIS_X;
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
		GetStationIndex(t1) == GetStationIndex(t2) &&
		!IsStationTileBlocked(t1);
}

/**
 * Get the reservation state of the rail station
 * @pre IsRailwayStationTile(t)
 * @param t the station tile
 * @return reservation state
 */
static inline bool GetRailwayStationReservation(TileIndex t)
{
	assert(IsRailwayStationTile(t));
	return HasBit(_m[t].m6, 2);
}

/**
 * Set the reservation state of the rail station
 * @pre IsRailwayStationTile(t)
 * @param t the station tile
 * @param b the reservation state
 */
static inline void SetRailwayStationReservation(TileIndex t, bool b)
{
	assert(IsRailwayStationTile(t));
	SB(_m[t].m6, 2, 1, b ? 1 : 0);
}

/**
 * Get the reserved track bits for a waypoint
 * @pre IsRailwayStationTile(t)
 * @param t the tile
 * @return reserved track bits
 */
static inline TrackBits GetRailStationReservation(TileIndex t)
{
	return GetRailwayStationReservation(t) ? AxisToTrackBits(GetRailStationAxis(t)) : TRACK_BIT_NONE;
}


static inline DiagDirection GetDockDirection(TileIndex t)
{
	StationGfx gfx = GetStationGfx(t);
	assert(IsDock(t) && gfx < GFX_DOCK_BASE_WATER_PART);
	return (DiagDirection)(gfx);
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

	if (IsBuoy(t)) return buoy_offset;
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

static inline void MakeStation(TileIndex t, Owner o, StationID sid, StationType st, byte section)
{
	SetTileType(t, MP_STATION);
	SetTileOwner(t, o);
	_m[t].m2 = sid;
	_m[t].m3 = 0;
	_m[t].m4 = 0;
	_m[t].m5 = section;
	SB(_m[t].m6, 2, 1, 0);
	SB(_m[t].m6, 3, 3, st);
	_me[t].m7 = 0;
}

static inline void MakeRailStation(TileIndex t, Owner o, StationID sid, Axis a, byte section, RailType rt)
{
	MakeStation(t, o, sid, STATION_RAIL, section + a);
	SetRailType(t, rt);
	SetRailwayStationReservation(t, false);
}

static inline void MakeRoadStop(TileIndex t, Owner o, StationID sid, RoadStopType rst, RoadTypes rt, DiagDirection d)
{
	MakeStation(t, o, sid, (rst == ROADSTOP_BUS ? STATION_BUS : STATION_TRUCK), d);
	SetRoadTypes(t, rt);
	SetRoadOwner(t, ROADTYPE_ROAD, o);
	SetRoadOwner(t, ROADTYPE_TRAM, o);
}

static inline void MakeDriveThroughRoadStop(TileIndex t, Owner station, Owner road, Owner tram, StationID sid, RoadStopType rst, RoadTypes rt, Axis a)
{
	MakeStation(t, station, sid, (rst == ROADSTOP_BUS ? STATION_BUS : STATION_TRUCK), GFX_TRUCK_BUS_DRIVETHROUGH_OFFSET + a);
	SetRoadTypes(t, rt);
	SetRoadOwner(t, ROADTYPE_ROAD, road);
	SetRoadOwner(t, ROADTYPE_TRAM, tram);
}

static inline void MakeAirport(TileIndex t, Owner o, StationID sid, byte section)
{
	MakeStation(t, o, sid, STATION_AIRPORT, section);
}

static inline void MakeBuoy(TileIndex t, StationID sid, WaterClass wc)
{
	/* Make the owner of the buoy tile the same as the current owner of the
	 * water tile. In this way, we can reset the owner of the water to its
	 * original state when the buoy gets removed. */
	MakeStation(t, GetTileOwner(t), sid, STATION_BUOY, 0);
	SetWaterClass(t, wc);
}

static inline void MakeDock(TileIndex t, Owner o, StationID sid, DiagDirection d, WaterClass wc)
{
	MakeStation(t, o, sid, STATION_DOCK, d);
	MakeStation(t + TileOffsByDiagDir(d), o, sid, STATION_DOCK, GFX_DOCK_BASE_WATER_PART + DiagDirToAxis(d));
	SetWaterClass(t + TileOffsByDiagDir(d), wc);
}

static inline void MakeOilrig(TileIndex t, StationID sid, WaterClass wc)
{
	MakeStation(t, OWNER_NONE, sid, STATION_OILRIG, 0);
	SetWaterClass(t, wc);
}

#endif /* STATION_MAP_H */
