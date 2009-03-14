/* $Id$ */

/** @file road_map.h Map accessors for roads. */

#ifndef ROAD_MAP_H
#define ROAD_MAP_H

#include "track_func.h"
#include "rail_type.h"
#include "town_type.h"
#include "road_func.h"
#include "tile_map.h"


enum RoadTileType {
	ROAD_TILE_NORMAL,
	ROAD_TILE_CROSSING,
	ROAD_TILE_DEPOT
};

static inline RoadTileType GetRoadTileType(TileIndex t)
{
	assert(IsTileType(t, MP_ROAD));
	return (RoadTileType)GB(_m[t].m5, 6, 2);
}

static inline bool IsNormalRoad(TileIndex t)
{
	return GetRoadTileType(t) == ROAD_TILE_NORMAL;
}

static inline bool IsNormalRoadTile(TileIndex t)
{
	return IsTileType(t, MP_ROAD) && IsNormalRoad(t);
}

static inline bool IsLevelCrossing(TileIndex t)
{
	return GetRoadTileType(t) == ROAD_TILE_CROSSING;
}

static inline bool IsLevelCrossingTile(TileIndex t)
{
	return IsTileType(t, MP_ROAD) && IsLevelCrossing(t);
}

static inline bool IsRoadDepot(TileIndex t)
{
	return GetRoadTileType(t) == ROAD_TILE_DEPOT;
}

static inline bool IsRoadDepotTile(TileIndex t)
{
	return IsTileType(t, MP_ROAD) && IsRoadDepot(t);
}

static inline RoadBits GetRoadBits(TileIndex t, RoadType rt)
{
	assert(IsNormalRoad(t));
	switch (rt) {
		default: NOT_REACHED();
		case ROADTYPE_ROAD: return (RoadBits)GB(_m[t].m5, 0, 4);
		case ROADTYPE_TRAM: return (RoadBits)GB(_m[t].m3, 0, 4);
	}
}

/**
 * Get all RoadBits set on a tile except from the given RoadType
 *
 * @param t The tile from which we want to get the RoadBits
 * @param rt The RoadType which we exclude from the querry
 * @return all set RoadBits of the tile which are not from the given RoadType
 */
static inline RoadBits GetOtherRoadBits(TileIndex t, RoadType rt)
{
	return GetRoadBits(t, rt == ROADTYPE_ROAD ? ROADTYPE_TRAM : ROADTYPE_ROAD);
}

/**
 * Get all set RoadBits on the given tile
 *
 * @param tile The tile from which we want to get the RoadBits
 * @return all set RoadBits of the tile
 */
static inline RoadBits GetAllRoadBits(TileIndex tile)
{
	return GetRoadBits(tile, ROADTYPE_ROAD) | GetRoadBits(tile, ROADTYPE_TRAM);
}

static inline void SetRoadBits(TileIndex t, RoadBits r, RoadType rt)
{
	assert(IsNormalRoad(t)); // XXX incomplete
	switch (rt) {
		default: NOT_REACHED();
		case ROADTYPE_ROAD: SB(_m[t].m5, 0, 4, r); break;
		case ROADTYPE_TRAM: SB(_m[t].m3, 0, 4, r); break;
	}
}

static inline RoadTypes GetRoadTypes(TileIndex t)
{
	return (RoadTypes)GB(_me[t].m7, 6, 2);
}

static inline void SetRoadTypes(TileIndex t, RoadTypes rt)
{
	assert(IsTileType(t, MP_ROAD) || IsTileType(t, MP_STATION) || IsTileType(t, MP_TUNNELBRIDGE));
	SB(_me[t].m7, 6, 2, rt);
}

static inline bool HasTileRoadType(TileIndex t, RoadType rt)
{
	return HasBit(GetRoadTypes(t), rt);
}

static inline Owner GetRoadOwner(TileIndex t, RoadType rt)
{
	switch (rt) {
		default: NOT_REACHED();
		case ROADTYPE_ROAD: return (Owner)GB(IsNormalRoadTile(t) ? _m[t].m1 : _me[t].m7, 0, 5);
		case ROADTYPE_TRAM: {
			/* Trams don't need OWNER_TOWN, and remapping OWNER_NONE
			 * to OWNER_TOWN makes it use one bit less */
			Owner o = (Owner)GB(_m[t].m3, 4, 4);
			return o == OWNER_TOWN ? OWNER_NONE : o;
		}
	}
}

static inline void SetRoadOwner(TileIndex t, RoadType rt, Owner o)
{
	switch (rt) {
		default: NOT_REACHED();
		case ROADTYPE_ROAD: SB(IsNormalRoadTile(t) ? _m[t].m1 : _me[t].m7, 0, 5, o); break;
		case ROADTYPE_TRAM: SB(_m[t].m3, 4, 4, o == OWNER_NONE ? OWNER_TOWN : o); break;
	}
}

static inline bool IsRoadOwner(TileIndex t, RoadType rt, Owner o)
{
	assert(HasTileRoadType(t, rt));
	return (GetRoadOwner(t, rt) == o);
}

/** Checks if given tile has town owned road
 * @param t tile to check
 * @return true iff tile has road and the road is owned by a town
 * @pre IsTileType(t, MP_ROAD)
 */
static inline bool HasTownOwnedRoad(TileIndex t)
{
	return HasTileRoadType(t, ROADTYPE_ROAD) && IsRoadOwner(t, ROADTYPE_ROAD, OWNER_TOWN);
}

/** Which directions are disallowed ? */
enum DisallowedRoadDirections {
	DRD_NONE,       ///< None of the directions are disallowed
	DRD_SOUTHBOUND, ///< All southbound traffic is disallowed
	DRD_NORTHBOUND, ///< All northbound traffic is disallowed
	DRD_BOTH,       ///< All directions are disallowed
	DRD_END
};
DECLARE_ENUM_AS_BIT_SET(DisallowedRoadDirections);

/**
 * Gets the disallowed directions
 * @param t the tile to get the directions from
 * @return the disallowed directions
 */
static inline DisallowedRoadDirections GetDisallowedRoadDirections(TileIndex t)
{
	assert(IsNormalRoad(t));
	return (DisallowedRoadDirections)GB(_m[t].m5, 4, 2);
}

/**
 * Sets the disallowed directions
 * @param t   the tile to set the directions for
 * @param drd the disallowed directions
 */
static inline void SetDisallowedRoadDirections(TileIndex t, DisallowedRoadDirections drd)
{
	assert(IsNormalRoad(t));
	assert(drd < DRD_END);
	SB(_m[t].m5, 4, 2, drd);
}

static inline Axis GetCrossingRoadAxis(TileIndex t)
{
	assert(IsLevelCrossing(t));
	return (Axis)GB(_m[t].m5, 0, 1);
}

static inline Axis GetCrossingRailAxis(TileIndex t)
{
	assert(IsLevelCrossing(t));
	return OtherAxis((Axis)GetCrossingRoadAxis(t));
}

static inline RoadBits GetCrossingRoadBits(TileIndex tile)
{
	return GetCrossingRoadAxis(tile) == AXIS_X ? ROAD_X : ROAD_Y;
}

static inline Track GetCrossingRailTrack(TileIndex tile)
{
	return AxisToTrack(GetCrossingRailAxis(tile));
}

static inline TrackBits GetCrossingRailBits(TileIndex tile)
{
	return AxisToTrackBits(GetCrossingRailAxis(tile));
}


/**
 * Get the reservation state of the rail crossing
 * @pre IsLevelCrossingTile(t)
 * @param t the crossing tile
 * @return reservation state
 */
static inline bool GetCrossingReservation(TileIndex t)
{
	assert(IsLevelCrossingTile(t));
	return HasBit(_m[t].m5, 4);
}

/**
 * Set the reservation state of the rail crossing
 * @note Works for both waypoints and rail depots
 * @pre IsLevelCrossingTile(t)
 * @param t the crossing tile
 * @param b the reservation state
 */
static inline void SetCrossingReservation(TileIndex t, bool b)
{
	assert(IsLevelCrossingTile(t));
	SB(_m[t].m5, 4, 1, b ? 1 : 0);
}

/**
 * Get the reserved track bits for a rail crossing
 * @pre IsLevelCrossingTile(t)
 * @param t the tile
 * @return reserved track bits
 */
static inline TrackBits GetRailCrossingReservation(TileIndex t)
{
	return GetCrossingReservation(t) ? GetCrossingRailBits(t) : TRACK_BIT_NONE;
}

static inline bool IsCrossingBarred(TileIndex t)
{
	assert(IsLevelCrossing(t));
	return HasBit(_m[t].m5, 5);
}

static inline void SetCrossingBarred(TileIndex t, bool barred)
{
	assert(IsLevelCrossing(t));
	SB(_m[t].m5, 5, 1, barred ? 1 : 0);
}

static inline void UnbarCrossing(TileIndex t)
{
	SetCrossingBarred(t, false);
}

static inline void BarCrossing(TileIndex t)
{
	SetCrossingBarred(t, true);
}

#define IsOnDesert IsOnSnow
static inline bool IsOnSnow(TileIndex t)
{
	return HasBit(_me[t].m7, 5);
}

#define ToggleDesert ToggleSnow
static inline void ToggleSnow(TileIndex t)
{
	ToggleBit(_me[t].m7, 5);
}


enum Roadside {
	ROADSIDE_BARREN           = 0,
	ROADSIDE_GRASS            = 1,
	ROADSIDE_PAVED            = 2,
	ROADSIDE_STREET_LIGHTS    = 3,
	ROADSIDE_TREES            = 5,
	ROADSIDE_GRASS_ROAD_WORKS = 6,
	ROADSIDE_PAVED_ROAD_WORKS = 7
};

static inline Roadside GetRoadside(TileIndex tile)
{
	return (Roadside)GB(_m[tile].m6, 3, 3);
}

static inline void SetRoadside(TileIndex tile, Roadside s)
{
	SB(_m[tile].m6, 3, 3, s);
}

static inline bool HasRoadWorks(TileIndex t)
{
	return GetRoadside(t) >= ROADSIDE_GRASS_ROAD_WORKS;
}

static inline bool IncreaseRoadWorksCounter(TileIndex t)
{
	AB(_me[t].m7, 0, 4, 1);

	return GB(_me[t].m7, 0, 4) == 15;
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
	SB(_me[t].m7, 0, 4, 0);
}


static inline DiagDirection GetRoadDepotDirection(TileIndex t)
{
	assert(IsRoadDepot(t));
	return (DiagDirection)GB(_m[t].m5, 0, 2);
}


/**
 * Returns the RoadBits on an arbitrary tile
 * Special behaviour:
 * - road depots: entrance is treated as road piece
 * - road tunnels: entrance is treated as road piece
 * - bridge ramps: start of the ramp is treated as road piece
 * - bridge middle parts: bridge itself is ignored
 *
 * If straight_tunnel_bridge_entrance is set a ROAD_X or ROAD_Y
 * for bridge ramps and tunnel entrances is returned depending
 * on the orientation of the tunnel or bridge.
 * @param tile the tile to get the road bits for
 * @param rt   the road type to get the road bits form
 * @param stbe whether to return straight road bits for tunnels/bridges.
 * @return the road bits of the given tile
 */
RoadBits GetAnyRoadBits(TileIndex tile, RoadType rt, bool straight_tunnel_bridge_entrance = false);

/**
 * Get the accessible track bits for the given tile.
 * Special behaviour:
 *  - road depots: no track bits
 *  - non-drive-through stations: no track bits
 * @param tile the tile to get the track bits for
 * @return the track bits for the given tile
 */
TrackBits GetAnyRoadTrackBits(TileIndex tile, RoadType rt);

/**
 * Return if the tile is a valid tile for a crossing.
 *
 * @note function is overloaded
 * @param tile the curent tile
 * @param ax the axis of the road over the rail
 * @return true if it is a valid tile
 */
bool IsPossibleCrossing(const TileIndex tile, Axis ax);


static inline void MakeRoadNormal(TileIndex t, RoadBits bits, RoadTypes rot, TownID town, Owner road, Owner tram)
{
	SetTileType(t, MP_ROAD);
	SetTileOwner(t, road);
	_m[t].m2 = town;
	_m[t].m3 = (HasBit(rot, ROADTYPE_TRAM) ? bits : 0);
	_m[t].m4 = 0;
	_m[t].m5 = (HasBit(rot, ROADTYPE_ROAD) ? bits : 0) | ROAD_TILE_NORMAL << 6;
	SB(_m[t].m6, 2, 4, 0);
	_me[t].m7 = rot << 6;
	SetRoadOwner(t, ROADTYPE_TRAM, tram);
}


static inline void MakeRoadCrossing(TileIndex t, Owner road, Owner tram, Owner rail, Axis roaddir, RailType rat, RoadTypes rot, uint town)
{
	SetTileType(t, MP_ROAD);
	SetTileOwner(t, rail);
	_m[t].m2 = town;
	_m[t].m3 = rat;
	_m[t].m4 = 0;
	_m[t].m5 = ROAD_TILE_CROSSING << 6 | roaddir;
	SB(_m[t].m6, 2, 4, 0);
	_me[t].m7 = rot << 6 | road;
	SetRoadOwner(t, ROADTYPE_TRAM, tram);
}


static inline void MakeRoadDepot(TileIndex t, Owner owner, DiagDirection dir, RoadType rt, TownID town)
{
	SetTileType(t, MP_ROAD);
	SetTileOwner(t, owner);
	_m[t].m2 = town;
	_m[t].m3 = 0;
	_m[t].m4 = 0;
	_m[t].m5 = ROAD_TILE_DEPOT << 6 | dir;
	SB(_m[t].m6, 2, 4, 0);
	_me[t].m7 = RoadTypeToRoadTypes(rt) << 6 | owner;
	SetRoadOwner(t, ROADTYPE_TRAM, owner);
}

#endif /* ROAD_MAP_H */
