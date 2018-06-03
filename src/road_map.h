/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file road_map.h Map accessors for roads. */

#ifndef ROAD_MAP_H
#define ROAD_MAP_H

#include "track_func.h"
#include "depot_type.h"
#include "rail_type.h"
#include "road_func.h"
#include "tile_map.h"


/** The different types of road tiles. */
enum RoadTileType {
	ROAD_TILE_NORMAL,   ///< Normal road
	ROAD_TILE_CROSSING, ///< Level crossing
	ROAD_TILE_DEPOT,    ///< Depot (one entrance)
};

/**
 * Test whether a tile can have road/tram subtypes.
 * @param t Tile to query.
 * @return true if tile can be queried about road/tram subtypes.
 */
static inline bool MayHaveRoad(TileIndex t)
{
	switch (GetTileType(t)) {
		case MP_ROAD:
		case MP_STATION:
		case MP_TUNNELBRIDGE:
			return true;

		default:
			return false;
	}
}

/**
 * Get the type of the road tile.
 * @param t Tile to query.
 * @pre IsTileType(t, MP_ROAD)
 * @return The road tile type.
 */
static inline RoadTileType GetRoadTileType(TileIndex t)
{
	assert(IsTileType(t, MP_ROAD));
	return (RoadTileType)GB(_m[t].m5, 6, 2);
}

/**
 * Return whether a tile is a normal road.
 * @param t Tile to query.
 * @pre IsTileType(t, MP_ROAD)
 * @return True if normal road.
 */
static inline bool IsNormalRoad(TileIndex t)
{
	return GetRoadTileType(t) == ROAD_TILE_NORMAL;
}

/**
 * Return whether a tile is a normal road tile.
 * @param t Tile to query.
 * @return True if normal road tile.
 */
static inline bool IsNormalRoadTile(TileIndex t)
{
	return IsTileType(t, MP_ROAD) && IsNormalRoad(t);
}

/**
 * Return whether a tile is a level crossing.
 * @param t Tile to query.
 * @pre IsTileType(t, MP_ROAD)
 * @return True if level crossing.
 */
static inline bool IsLevelCrossing(TileIndex t)
{
	return GetRoadTileType(t) == ROAD_TILE_CROSSING;
}

/**
 * Return whether a tile is a level crossing tile.
 * @param t Tile to query.
 * @return True if level crossing tile.
 */
static inline bool IsLevelCrossingTile(TileIndex t)
{
	return IsTileType(t, MP_ROAD) && IsLevelCrossing(t);
}

/**
 * Return whether a tile is a road depot.
 * @param t Tile to query.
 * @pre IsTileType(t, MP_ROAD)
 * @return True if road depot.
 */
static inline bool IsRoadDepot(TileIndex t)
{
	return GetRoadTileType(t) == ROAD_TILE_DEPOT;
}

/**
 * Return whether a tile is a road depot tile.
 * @param t Tile to query.
 * @return True if road depot tile.
 */
static inline bool IsRoadDepotTile(TileIndex t)
{
	return IsTileType(t, MP_ROAD) && IsRoadDepot(t);
}

/**
 * Get the present road bits for a specific road type.
 * @param t  The tile to query.
 * @param rt Road type.
 * @pre IsNormalRoad(t)
 * @return The present road bits for the road type.
 */
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

/**
 * Set the present road bits for a specific road type.
 * @param t  The tile to change.
 * @param r  The new road bits.
 * @param rt Road type.
 * @pre IsNormalRoad(t)
 */
static inline void SetRoadBits(TileIndex t, RoadBits r, RoadType rt)
{
	assert(IsNormalRoad(t)); // XXX incomplete
	switch (rt) {
		default: NOT_REACHED();
		case ROADTYPE_ROAD: SB(_m[t].m5, 0, 4, r); break;
		case ROADTYPE_TRAM: SB(_m[t].m3, 0, 4, r); break;
	}
}

static inline RoadSubType GetRoadSubTypeRoad(TileIndex t)
{
	assert(MayHaveRoad(t));
	return (RoadSubType)GB(_m[t].m4, 0, 4);
}

static inline RoadSubType GetRoadSubTypeTram(TileIndex t)
{
	assert(MayHaveRoad(t));
	return (RoadSubType)GB(_m[t].m4, 4, 4);
}

static inline RoadSubType GetRoadSubType(TileIndex t, RoadType rt)
{
	switch (rt) {
		default: NOT_REACHED();
		case ROADTYPE_ROAD: return GetRoadSubTypeRoad(t);
		case ROADTYPE_TRAM: return GetRoadSubTypeTram(t);
	}
}

static inline RoadTypeIdentifier GetRoadTypeRoad(TileIndex t)
{
	return RoadTypeIdentifier(ROADTYPE_ROAD, GetRoadSubTypeRoad(t));
}

static inline RoadTypeIdentifier GetRoadTypeTram(TileIndex t)
{
	return RoadTypeIdentifier(ROADTYPE_TRAM, GetRoadSubTypeTram(t));
}

static inline RoadTypeIdentifier GetRoadType(TileIndex t, RoadType rt)
{
	return RoadTypeIdentifier(rt, GetRoadSubType(t, rt));
}

/**
 * Get the present road types of a tile.
 * @param t The tile to query.
 * @return Present road types.
 */
static inline RoadTypes GetRoadTypes(TileIndex t)
{
	RoadTypes result = ROADTYPES_NONE;
	if (MayHaveRoad(t)) {
		if (GetRoadSubTypeRoad(t) != INVALID_ROADSUBTYPE) result |= ROADTYPES_ROAD;
		if (GetRoadSubTypeTram(t) != INVALID_ROADSUBTYPE) result |= ROADTYPES_TRAM;
	}
	return result;
}

static inline bool HasRoadTypeRoad(TileIndex t)
{
	return GetRoadSubTypeRoad(t) != INVALID_ROADSUBTYPE;
}

static inline bool HasRoadTypeTram(TileIndex t)
{
	return GetRoadSubTypeTram(t) != INVALID_ROADSUBTYPE;
}

/**
 * Check if a tile has a specific road type.
 * @param t  The tile to check.
 * @param rt Road type to check.
 * @return True if the tile has the specified road type.
 */
static inline bool HasTileRoadType(TileIndex t, RoadType rt)
{
	return GetRoadSubType(t, rt) != INVALID_ROADSUBTYPE;
}

/**
 * Check if a tile has one of the specified road subtypes.
 * @param t  The tile to check.
 * @param rt Road type to check.
 * @param rst Allowed road subtypes.
 * @return True if the tile has one of the specified road subtypes.
 */
static inline bool HasTileAnyRoadSubType(TileIndex t, RoadType rt, RoadSubTypes rst)
{
	if (!MayHaveRoad(t)) return false;
	RoadSubType st = GetRoadSubType(t, rt) ;
	return st != INVALID_ROADSUBTYPE && HasBit(rst, st);
}

/**
 * Get the owner of a specific road type.
 * @param t  The tile to query.
 * @param rt The road type to get the owner of.
 * @return Owner of the given road type.
 */
static inline Owner GetRoadOwner(TileIndex t, RoadType rt)
{
	assert(MayHaveRoad(t));
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

/**
 * Set the owner of a specific road type.
 * @param t  The tile to change.
 * @param rt The road type to change the owner of.
 * @param o  New owner of the given road type.
 */
static inline void SetRoadOwner(TileIndex t, RoadType rt, Owner o)
{
	switch (rt) {
		default: NOT_REACHED();
		case ROADTYPE_ROAD: SB(IsNormalRoadTile(t) ? _m[t].m1 : _me[t].m7, 0, 5, o); break;
		case ROADTYPE_TRAM: SB(_m[t].m3, 4, 4, o == OWNER_NONE ? OWNER_TOWN : o); break;
	}
}

/**
 * Check if a specific road type is owned by an owner.
 * @param t  The tile to query.
 * @param rt The road type to compare the owner of.
 * @param o  Owner to compare with.
 * @pre HasTileRoadType(t, rt)
 * @return True if the road type is owned by the given owner.
 */
static inline bool IsRoadOwner(TileIndex t, RoadType rt, Owner o)
{
	assert(HasTileRoadType(t, rt));
	return (GetRoadOwner(t, rt) == o);
}

/**
 * Checks if given tile has town owned road
 * @param t tile to check
 * @pre IsTileType(t, MP_ROAD)
 * @return true iff tile has road and the road is owned by a town
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
	DRD_END,        ///< Sentinel
};
DECLARE_ENUM_AS_BIT_SET(DisallowedRoadDirections)
/** Helper information for extract tool. */
template <> struct EnumPropsT<DisallowedRoadDirections> : MakeEnumPropsT<DisallowedRoadDirections, byte, DRD_NONE, DRD_END, DRD_END, 2> {};

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

/**
 * Get the road axis of a level crossing.
 * @param t The tile to query.
 * @pre IsLevelCrossing(t)
 * @return The axis of the road.
 */
static inline Axis GetCrossingRoadAxis(TileIndex t)
{
	assert(IsLevelCrossing(t));
	return (Axis)GB(_m[t].m5, 0, 1);
}

/**
 * Get the rail axis of a level crossing.
 * @param t The tile to query.
 * @pre IsLevelCrossing(t)
 * @return The axis of the rail.
 */
static inline Axis GetCrossingRailAxis(TileIndex t)
{
	assert(IsLevelCrossing(t));
	return OtherAxis((Axis)GetCrossingRoadAxis(t));
}

/**
 * Get the road bits of a level crossing.
 * @param tile The tile to query.
 * @return The present road bits.
 */
static inline RoadBits GetCrossingRoadBits(TileIndex tile)
{
	return GetCrossingRoadAxis(tile) == AXIS_X ? ROAD_X : ROAD_Y;
}

/**
 * Get the rail track of a level crossing.
 * @param tile The tile to query.
 * @return The rail track.
 */
static inline Track GetCrossingRailTrack(TileIndex tile)
{
	return AxisToTrack(GetCrossingRailAxis(tile));
}

/**
 * Get the rail track bits of a level crossing.
 * @param tile The tile to query.
 * @return The rail track bits.
 */
static inline TrackBits GetCrossingRailBits(TileIndex tile)
{
	return AxisToTrackBits(GetCrossingRailAxis(tile));
}


/**
 * Get the reservation state of the rail crossing
 * @param t the crossing tile
 * @return reservation state
 * @pre IsLevelCrossingTile(t)
 */
static inline bool HasCrossingReservation(TileIndex t)
{
	assert(IsLevelCrossingTile(t));
	return HasBit(_m[t].m5, 4);
}

/**
 * Set the reservation state of the rail crossing
 * @note Works for both waypoints and rail depots
 * @param t the crossing tile
 * @param b the reservation state
 * @pre IsLevelCrossingTile(t)
 */
static inline void SetCrossingReservation(TileIndex t, bool b)
{
	assert(IsLevelCrossingTile(t));
	SB(_m[t].m5, 4, 1, b ? 1 : 0);
}

/**
 * Get the reserved track bits for a rail crossing
 * @param t the tile
 * @pre IsLevelCrossingTile(t)
 * @return reserved track bits
 */
static inline TrackBits GetCrossingReservationTrackBits(TileIndex t)
{
	return HasCrossingReservation(t) ? GetCrossingRailBits(t) : TRACK_BIT_NONE;
}

/**
 * Check if the level crossing is barred.
 * @param t The tile to query.
 * @pre IsLevelCrossing(t)
 * @return True if the level crossing is barred.
 */
static inline bool IsCrossingBarred(TileIndex t)
{
	assert(IsLevelCrossing(t));
	return HasBit(_m[t].m5, 5);
}

/**
 * Set the bar state of a level crossing.
 * @param t The tile to modify.
 * @param barred True if the crossing should be barred, false otherwise.
 * @pre IsLevelCrossing(t)
 */
static inline void SetCrossingBarred(TileIndex t, bool barred)
{
	assert(IsLevelCrossing(t));
	SB(_m[t].m5, 5, 1, barred ? 1 : 0);
}

/**
 * Unbar a level crossing.
 * @param t The tile to change.
 */
static inline void UnbarCrossing(TileIndex t)
{
	SetCrossingBarred(t, false);
}

/**
 * Bar a level crossing.
 * @param t The tile to change.
 */
static inline void BarCrossing(TileIndex t)
{
	SetCrossingBarred(t, true);
}

/** Check if a road tile has snow/desert. */
#define IsOnDesert IsOnSnow
/**
 * Check if a road tile has snow/desert.
 * @param t The tile to query.
 * @return True if the tile has snow/desert.
 */
static inline bool IsOnSnow(TileIndex t)
{
	return HasBit(_me[t].m7, 5);
}

/** Toggle the snow/desert state of a road tile. */
#define ToggleDesert ToggleSnow
/**
 * Toggle the snow/desert state of a road tile.
 * @param t The tile to change.
 */
static inline void ToggleSnow(TileIndex t)
{
	ToggleBit(_me[t].m7, 5);
}


/** The possible road side decorations. */
enum Roadside {
	ROADSIDE_BARREN           = 0, ///< Road on barren land
	ROADSIDE_GRASS            = 1, ///< Road on grass
	ROADSIDE_PAVED            = 2, ///< Road with paved sidewalks
	ROADSIDE_STREET_LIGHTS    = 3, ///< Road with street lights on paved sidewalks
	ROADSIDE_TREES            = 5, ///< Road with trees on paved sidewalks
	ROADSIDE_GRASS_ROAD_WORKS = 6, ///< Road on grass with road works
	ROADSIDE_PAVED_ROAD_WORKS = 7, ///< Road with sidewalks and road works
};

/**
 * Get the decorations of a road.
 * @param tile The tile to query.
 * @return The road decoration of the tile.
 */
static inline Roadside GetRoadside(TileIndex tile)
{
	return (Roadside)GB(_me[tile].m6, 3, 3);
}

/**
 * Set the decorations of a road.
 * @param tile The tile to change.
 * @param s    The new road decoration of the tile.
 */
static inline void SetRoadside(TileIndex tile, Roadside s)
{
	SB(_me[tile].m6, 3, 3, s);
}

/**
 * Check if a tile has road works.
 * @param t The tile to check.
 * @return True if the tile has road works in progress.
 */
static inline bool HasRoadWorks(TileIndex t)
{
	return GetRoadside(t) >= ROADSIDE_GRASS_ROAD_WORKS;
}

/**
 * Increase the progress counter of road works.
 * @param t The tile to modify.
 * @return True if the road works are in the last stage.
 */
static inline bool IncreaseRoadWorksCounter(TileIndex t)
{
	AB(_me[t].m7, 0, 4, 1);

	return GB(_me[t].m7, 0, 4) == 15;
}

/**
 * Start road works on a tile.
 * @param t The tile to start the work on.
 * @pre !HasRoadWorks(t)
 */
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

/**
 * Terminate road works on a tile.
 * @param t Tile to stop the road works on.
 * @pre HasRoadWorks(t)
 */
static inline void TerminateRoadWorks(TileIndex t)
{
	assert(HasRoadWorks(t));
	SetRoadside(t, (Roadside)(GetRoadside(t) - ROADSIDE_GRASS_ROAD_WORKS + ROADSIDE_GRASS));
	/* Stop the counter */
	SB(_me[t].m7, 0, 4, 0);
}


/**
 * Get the direction of the exit of a road depot.
 * @param t The tile to query.
 * @return Diagonal direction of the depot exit.
 */
static inline DiagDirection GetRoadDepotDirection(TileIndex t)
{
	assert(IsRoadDepot(t));
	return (DiagDirection)GB(_m[t].m5, 0, 2);
}


RoadBits GetAnyRoadBits(TileIndex tile, RoadType rt, bool straight_tunnel_bridge_entrance = false);

struct RoadTypeIdentifiers {
	RoadTypeIdentifier road_identifier;
	RoadTypeIdentifier tram_identifier;

	/** Creates an INVALID RoadTypeIdentifiers */
	RoadTypeIdentifiers() {}

	/**
	 * Extracts the RoadTypeIdentifiers from the given tile
	 * @param tile The tile to read the informations from
	 */
	static RoadTypeIdentifiers FromTile(TileIndex t)
	{
		assert(MayHaveRoad(t));

		RoadTypeIdentifiers rtids;
		rtids.road_identifier = GetRoadTypeRoad(t);
		rtids.tram_identifier = GetRoadTypeTram(t);

		return rtids;
	}

	/**
	 * Converts a RoadTypeIdentifier into a RoadTypeIdentifiers
	 * @param rtid The RoadTypeIdentifier to convert
	 */
	static RoadTypeIdentifiers FromRoadTypeIdentifier(RoadTypeIdentifier rtid)
	{
		RoadTypeIdentifiers rtids;

		switch (rtid.basetype) {
			default: NOT_REACHED();
			case ROADTYPE_ROAD: rtids.road_identifier = rtid; break;
			case ROADTYPE_TRAM: rtids.tram_identifier = rtid; break;
		}

		return rtids;
	}

	/**
	 * Creates a RoadTypeIdentifiers from two RoadTypeIdentifier
	 * The order of the identifier doesn't matter, but they must be of different
	 * base type (no ROAD + ROAD or TRAM + TRAM)
	 * @param rtid1 The first RoadTypeIdentifier
	 * @param rtid2 The second RoadTypeIdentifier
	 */
	static RoadTypeIdentifiers FromRoadTypeIdentifier(RoadTypeIdentifier rtid1, RoadTypeIdentifier rtid2)
	{
		assert(rtid1.basetype < ROADTYPE_END);
		assert(rtid2.basetype < ROADTYPE_END);

		/* We can't merge 2 road types of the same base type, not yet */
		assert(rtid1.basetype != rtid2.basetype);

		RoadTypeIdentifiers rtids;

		/* As we are sure about the road types being different we can just check the first one */
		switch (rtid1.basetype) {
			default: NOT_REACHED();
			case ROADTYPE_ROAD:
				rtids.road_identifier = rtid1;
				rtids.tram_identifier = rtid2;
				break;
			case ROADTYPE_TRAM:
				rtids.road_identifier = rtid2;
				rtids.tram_identifier = rtid1;
				break;
		}

		return rtids;
	}

	/**
	 * Returns the RoadTypes contained in the RoadTypeIdentifiers
	 * @return RoadTypes flags
	 */
	RoadTypes PresentRoadTypes() const
	{
		RoadTypes rot = ROADTYPES_NONE;

		if (this->road_identifier.IsValid()) {
			rot |= ROADTYPES_ROAD;
		}

		if (this->tram_identifier.IsValid()) {
			rot |= ROADTYPES_TRAM;
		}

		return rot;
	};

	RoadTypeIdentifier GetType(RoadType rt) const
	{
		switch (rt) {
			default: NOT_REACHED();
			case ROADTYPE_ROAD: return this->road_identifier;
			case ROADTYPE_TRAM: return this->tram_identifier;
		}
	}

	bool HasType(RoadType rt) const
	{
		switch (rt) {
			default: NOT_REACHED();
			case ROADTYPE_ROAD: return this->road_identifier.IsValid();
			case ROADTYPE_TRAM: return this->tram_identifier.IsValid();
		}
	}

	bool HasRoad() const
	{
		return this->road_identifier.IsValid();
	}

	bool HasTram() const
	{
		return this->tram_identifier.IsValid();
	}

	/**
	 * Merge a new road type identifier into the current one.
	 * If the roadtype is already present, the new subtype replaces the old one.
	 * @param rtid The new RoadTypeIdentifier to merge into the current one
	 */
	void MergeRoadType(RoadTypeIdentifier rtid)
	{
		switch (rtid.basetype) {
			default: NOT_REACHED();
			case ROADTYPE_ROAD: this->road_identifier = rtid; break;
			case ROADTYPE_TRAM: this->tram_identifier = rtid; break;
		}
	};

	/**
	 * Remove a roadtype.
	 */
	void ClearRoadType(RoadType rt)
	{
		switch (rt) {
			default: NOT_REACHED();
			case ROADTYPE_ROAD: this->road_identifier = RoadTypeIdentifier(); break;
			case ROADTYPE_TRAM: this->tram_identifier = RoadTypeIdentifier(); break;
		}
	}
};

#define FOR_EACH_SET_ROADTYPEIDENTIFIER(var, rtids) \
	for (RoadType ___FESRTID = ROADTYPE_BEGIN; ___FESRTID < ROADTYPE_END; ___FESRTID++) \
		if ((var = rtids.GetType(___FESRTID)).IsValid())

/**
 * Set the present road types of a tile.
 * @param t  The tile to change.
 * @param rtids The new road types identifiers to set for the tile.
 */
static inline void SetRoadTypes(TileIndex t, RoadTypeIdentifiers rtids)
{
	assert(MayHaveRoad(t));
	SB(_m[t].m4, 0, 4, rtids.road_identifier.subtype);
	SB(_m[t].m4, 4, 4, rtids.tram_identifier.subtype);
}

/**
 * Make a normal road tile.
 * @param t     Tile to make a normal road.
 * @param bits  Road bits to set for all present road types.
 * @param rtids New present road types.
 * @param town  Town ID if the road is a town-owned road.
 * @param road  New owner of road.
 * @param tram  New owner of tram tracks.
 */
static inline void MakeRoadNormal(TileIndex t, RoadBits bits, RoadTypeIdentifiers rtids, TownID town, Owner road, Owner tram)
{
	SetTileType(t, MP_ROAD);
	SetTileOwner(t, road);
	_m[t].m2 = town;
	_m[t].m3 = (rtids.HasTram() ? bits : 0);
	_m[t].m5 = (rtids.HasRoad() ? bits : 0) | ROAD_TILE_NORMAL << 6;
	SB(_me[t].m6, 2, 4, 0);
	_me[t].m7 = 0;
	SetRoadTypes(t, rtids);
	SetRoadOwner(t, ROADTYPE_TRAM, tram);
}

/**
 * Make a level crossing.
 * @param t       Tile to make a level crossing.
 * @param road    New owner of road.
 * @param tram    New owner of tram tracks.
 * @param rail    New owner of the rail track.
 * @param roaddir Axis of the road.
 * @param rat     New rail type.
 * @param rtids   New present road types.
 * @param town    Town ID if the road is a town-owned road.
 */
static inline void MakeRoadCrossing(TileIndex t, Owner road, Owner tram, Owner rail, Axis roaddir, RailType rat, RoadTypeIdentifiers rtids, uint town)
{
	SetTileType(t, MP_ROAD);
	SetTileOwner(t, rail);
	_m[t].m2 = town;
	_m[t].m3 = rat;
	_m[t].m4 = INVALID_ROADTYPES;
	_m[t].m5 = ROAD_TILE_CROSSING << 6 | roaddir;
	SB(_me[t].m6, 2, 4, 0);
	_me[t].m7 = road;
	SetRoadTypes(t, rtids);
	SetRoadOwner(t, ROADTYPE_TRAM, tram);
}

/**
 * Make a road depot.
 * @param t     Tile to make a level crossing.
 * @param owner New owner of the depot.
 * @param did   New depot ID.
 * @param dir   Direction of the depot exit.
 * @param rt    Road type of the depot.
 */
static inline void MakeRoadDepot(TileIndex t, Owner owner, DepotID did, DiagDirection dir, RoadTypeIdentifier rtid)
{
	SetTileType(t, MP_ROAD);
	SetTileOwner(t, owner);
	_m[t].m2 = did;
	_m[t].m3 = 0;
	_m[t].m4 = INVALID_ROADTYPES;
	_m[t].m5 = ROAD_TILE_DEPOT << 6 | dir;
	SB(_me[t].m6, 2, 4, 0);
	_me[t].m7 = owner;
	SetRoadTypes(t, RoadTypeIdentifiers::FromRoadTypeIdentifier(rtid));
	SetRoadOwner(t, ROADTYPE_TRAM, owner);
}

#endif /* ROAD_MAP_H */
