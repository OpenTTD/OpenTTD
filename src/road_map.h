/* $Id$ */

/** @file road_map.h */

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

static inline bool IsLevelCrossing(TileIndex t)
{
	return GetRoadTileType(t) == ROAD_TILE_CROSSING;
}

static inline bool IsLevelCrossingTile(TileIndex t)
{
	return IsTileType(t, MP_ROAD) && IsLevelCrossing(t);
}

static inline RoadBits GetRoadBits(TileIndex t, RoadType rt)
{
	assert(GetRoadTileType(t) == ROAD_TILE_NORMAL);
	switch (rt) {
		default: NOT_REACHED();
		case ROADTYPE_ROAD: return (RoadBits)GB(_m[t].m4, 0, 4);
		case ROADTYPE_TRAM: return (RoadBits)GB(_m[t].m4, 4, 4);
		case ROADTYPE_HWAY: return (RoadBits)GB(_m[t].m6, 2, 4);
	}
}

static inline RoadBits GetAllRoadBits(TileIndex tile)
{
	return GetRoadBits(tile, ROADTYPE_ROAD) | GetRoadBits(tile, ROADTYPE_TRAM) | GetRoadBits(tile, ROADTYPE_HWAY);
}

static inline void SetRoadBits(TileIndex t, RoadBits r, RoadType rt)
{
	assert(GetRoadTileType(t) == ROAD_TILE_NORMAL); // XXX incomplete
	switch (rt) {
		default: NOT_REACHED();
		case ROADTYPE_ROAD: SB(_m[t].m4, 0, 4, r); break;
		case ROADTYPE_TRAM: SB(_m[t].m4, 4, 4, r); break;
		case ROADTYPE_HWAY: SB(_m[t].m6, 2, 4, r); break;
	}
}

static inline RoadTypes GetRoadTypes(TileIndex t)
{
	if (IsTileType(t, MP_ROAD)) {
		return (RoadTypes)GB(_me[t].m7, 5, 3);
	} else {
		return (RoadTypes)GB(_m[t].m3, 0, 3);
	}
}

static inline void SetRoadTypes(TileIndex t, RoadTypes rt)
{
	if (IsTileType(t, MP_ROAD)) {
		SB(_me[t].m7, 5, 3, rt);
	} else {
		assert(IsTileType(t, MP_STATION) || IsTileType(t, MP_TUNNELBRIDGE));
		SB(_m[t].m3, 0, 2, rt);
	}
}

static inline Owner GetRoadOwner(TileIndex t, RoadType rt)
{
	if (!IsTileType(t, MP_ROAD)) return GetTileOwner(t);

	switch (GetRoadTileType(t)) {
		default: NOT_REACHED();
		case ROAD_TILE_NORMAL:
			switch (rt) {
				default: NOT_REACHED();
				case ROADTYPE_ROAD: return (Owner)GB( _m[t].m1, 0, 5);
				case ROADTYPE_TRAM: {
					/* Trams don't need OWNER_TOWN, and remapping OWNER_NONE
					 * to OWNER_TOWN makes it use one bit less */
					Owner o = (Owner)GB( _m[t].m5, 0, 4);
					return o == OWNER_TOWN ? OWNER_NONE : o;
				}
				case ROADTYPE_HWAY: return (Owner)GB(_me[t].m7, 0, 5);
			}
		case ROAD_TILE_CROSSING:
			switch (rt) {
				default: NOT_REACHED();
				case ROADTYPE_ROAD: return (Owner)GB( _m[t].m4, 0, 5);
				case ROADTYPE_TRAM: {
					/* Trams don't need OWNER_TOWN, and remapping OWNER_NONE
					 * to OWNER_TOWN makes it use one bit less */
					Owner o = (Owner)GB( _m[t].m5, 0, 4);
					return o == OWNER_TOWN ? OWNER_NONE : o;
				}
				case ROADTYPE_HWAY: return (Owner)GB(_me[t].m7, 0, 5);
			}
		case ROAD_TILE_DEPOT: return GetTileOwner(t);
	}
}

static inline void SetRoadOwner(TileIndex t, RoadType rt, Owner o)
{
	if (!IsTileType(t, MP_ROAD)) return SetTileOwner(t, o);

	switch (GetRoadTileType(t)) {
		default: NOT_REACHED();
		case ROAD_TILE_NORMAL:
			switch (rt) {
				default: NOT_REACHED();
				case ROADTYPE_ROAD: SB( _m[t].m1, 0, 5, o); break;
				case ROADTYPE_TRAM: SB( _m[t].m5, 0, 4, o == OWNER_NONE ? OWNER_TOWN : o); break;
				case ROADTYPE_HWAY: SB(_me[t].m7, 0, 5, o); break;
			}
			break;
		case ROAD_TILE_CROSSING:
			switch (rt) {
				default: NOT_REACHED();
				case ROADTYPE_ROAD: SB( _m[t].m4, 0, 5, o); break;
				/* Trams don't need OWNER_TOWN, and remapping OWNER_NONE
				 * to OWNER_TOWN makes it use one bit less */
				case ROADTYPE_TRAM: SB( _m[t].m5, 0, 4, o == OWNER_NONE ? OWNER_TOWN : o); break;
				case ROADTYPE_HWAY: SB(_me[t].m7, 0, 5, o); break;
			}
			break;
		case ROAD_TILE_DEPOT: return SetTileOwner(t, o);
	}
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
	assert(GetRoadTileType(t) == ROAD_TILE_NORMAL);
	return (DisallowedRoadDirections)GB(_m[t].m5, 4, 2);
}

/**
 * Sets the disallowed directions
 * @param t   the tile to set the directions for
 * @param drd the disallowed directions
 */
static inline void SetDisallowedRoadDirections(TileIndex t, DisallowedRoadDirections drd)
{
	assert(GetRoadTileType(t) == ROAD_TILE_NORMAL);
	assert(drd < DRD_END);
	SB(_m[t].m5, 4, 2, drd);
}

static inline Axis GetCrossingRoadAxis(TileIndex t)
{
	assert(GetRoadTileType(t) == ROAD_TILE_CROSSING);
	return (Axis)GB(_m[t].m4, 6, 1);
}

static inline RoadBits GetCrossingRoadBits(TileIndex tile)
{
	return GetCrossingRoadAxis(tile) == AXIS_X ? ROAD_X : ROAD_Y;
}

static inline TrackBits GetCrossingRailBits(TileIndex tile)
{
	return AxisToTrackBits(OtherAxis(GetCrossingRoadAxis(tile)));
}

static inline bool IsCrossingBarred(TileIndex t)
{
	assert(GetRoadTileType(t) == ROAD_TILE_CROSSING);
	return HasBit(_m[t].m4, 5);
}

static inline void SetCrossingBarred(TileIndex t, bool barred)
{
	assert(GetRoadTileType(t) == ROAD_TILE_CROSSING);
	SB(_m[t].m4, 5, 1, barred);
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
	return HasBit(_m[t].m3, 7);
}

#define ToggleDesert ToggleSnow
static inline void ToggleSnow(TileIndex t)
{
	ToggleBit(_m[t].m3, 7);
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
	return (Roadside)GB(_m[tile].m3, 4, 3);
}

static inline void SetRoadside(TileIndex tile, Roadside s)
{
	SB(_m[tile].m3, 4, 3, s);
}

static inline bool HasRoadWorks(TileIndex t)
{
	return GetRoadside(t) >= ROADSIDE_GRASS_ROAD_WORKS;
}

static inline bool IncreaseRoadWorksCounter(TileIndex t)
{
	AB(_m[t].m3, 0, 4, 1);

	return GB(_m[t].m3, 0, 4) == 15;
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
	SB(_m[t].m3, 0, 4, 0);
}


static inline DiagDirection GetRoadDepotDirection(TileIndex t)
{
	assert(GetRoadTileType(t) == ROAD_TILE_DEPOT);
	return (DiagDirection)GB(_m[t].m5, 0, 2);
}


/**
 * Returns the RoadBits on an arbitrary tile
 * Special behaviour:
 * - road depots: entrance is treated as road piece
 * - road tunnels: entrance is treated as road piece
 * - bridge ramps: start of the ramp is treated as road piece
 * - bridge middle parts: bridge itself is ignored
 * @param tile the tile to get the road bits for
 * @param rt   the road type to get the road bits form
 * @return the road bits of the given tile
 */
RoadBits GetAnyRoadBits(TileIndex tile, RoadType rt);

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


static inline void MakeRoadNormal(TileIndex t, RoadBits bits, RoadTypes rot, TownID town, Owner road, Owner tram, Owner hway)
{
	SetTileType(t, MP_ROAD);
	SetTileOwner(t, road);
	_m[t].m2 = town;
	_m[t].m3 = 0;
	_m[t].m4 = (HasBit(rot, ROADTYPE_TRAM) ? bits : 0) << 4 | (HasBit(rot, ROADTYPE_ROAD) ? bits : 0);
	_m[t].m5 = ROAD_TILE_NORMAL << 6;
	SetRoadOwner(t, ROADTYPE_TRAM, tram);
	SB(_m[t].m6, 2, 4, HasBit(rot, ROADTYPE_HWAY) ? bits : 0);
	_me[t].m7 = rot << 5 | hway;
}


static inline void MakeRoadCrossing(TileIndex t, Owner road, Owner tram, Owner hway, Owner rail, Axis roaddir, RailType rat, RoadTypes rot, uint town)
{
	SetTileType(t, MP_ROAD);
	SetTileOwner(t, rail);
	_m[t].m2 = town;
	_m[t].m3 = rat;
	_m[t].m4 = roaddir << 6 | road;
	_m[t].m5 = ROAD_TILE_CROSSING << 6;
	SetRoadOwner(t, ROADTYPE_TRAM, tram);
	SB(_m[t].m6, 2, 4, 0);
	_me[t].m7 = rot << 5 | hway;
}


static inline void MakeRoadDepot(TileIndex t, Owner owner, DiagDirection dir, RoadType rt)
{
	SetTileType(t, MP_ROAD);
	SetTileOwner(t, owner);
	_m[t].m2 = 0;
	_m[t].m3 = 0;
	_m[t].m4 = 0;
	_m[t].m5 = ROAD_TILE_DEPOT << 6 | dir;
	SB(_m[t].m6, 2, 4, 0);
	_me[t].m7 = RoadTypeToRoadTypes(rt) << 5;
}

#endif /* ROAD_MAP_H */
