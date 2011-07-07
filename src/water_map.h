/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file water_map.h Map accessors for water tiles. */

#ifndef WATER_MAP_H
#define WATER_MAP_H

#include "depot_type.h"
#include "tile_map.h"

/** Available water tile types. */
enum WaterTileType {
	WATER_TILE_CLEAR, ///< Plain water.
	WATER_TILE_COAST, ///< Coast.
	WATER_TILE_LOCK,  ///< Water lock.
	WATER_TILE_DEPOT, ///< Water Depot.
};

/** classes of water (for #WATER_TILE_CLEAR water tile type). */
enum WaterClass {
	WATER_CLASS_SEA,     ///< Sea.
	WATER_CLASS_CANAL,   ///< Canal.
	WATER_CLASS_RIVER,   ///< River.
	WATER_CLASS_INVALID, ///< Used for industry tiles on land (also for oilrig if newgrf says so).
};
/** Helper information for extract tool. */
template <> struct EnumPropsT<WaterClass> : MakeEnumPropsT<WaterClass, byte, WATER_CLASS_SEA, WATER_CLASS_INVALID, WATER_CLASS_INVALID, 2> {};

/** Sections of the water depot. */
enum DepotPart {
	DEPOT_NORTH = 0x80,
	DEPOT_SOUTH = 0x81,
	DEPOT_END   = 0x84,
};

/** Sections of the water lock. */
enum LockPart {
	LOCK_MIDDLE = 0x10,
	LOCK_LOWER  = 0x14,
	LOCK_UPPER  = 0x18,
	LOCK_END    = 0x1C
};

/**
 * Get the water tile type at a tile.
 * @param t Water tile to query.
 * @return Water tile type at the tile.
 */
static inline WaterTileType GetWaterTileType(TileIndex t)
{
	assert(IsTileType(t, MP_WATER));

	if (_m[t].m5 == 0) return WATER_TILE_CLEAR;
	if (_m[t].m5 == 1) return WATER_TILE_COAST;
	if (IsInsideMM(_m[t].m5, LOCK_MIDDLE, LOCK_END)) return WATER_TILE_LOCK;

	assert(IsInsideMM(_m[t].m5, DEPOT_NORTH, DEPOT_END));
	return WATER_TILE_DEPOT;
}

/**
 * Checks whether the tile has an waterclass associated.
 * You can then subsequently call GetWaterClass().
 * @param t Tile to query.
 * @return True if the tiletype has a waterclass.
 */
static inline bool HasTileWaterClass(TileIndex t)
{
	return IsTileType(t, MP_WATER) || IsTileType(t, MP_STATION) || IsTileType(t, MP_INDUSTRY) || IsTileType(t, MP_OBJECT);
}

/**
 * Get the water class at a tile.
 * @param t Water tile to query.
 * @pre IsTileType(t, MP_WATER) || IsTileType(t, MP_STATION) || IsTileType(t, MP_INDUSTRY) || IsTileType(t, MP_OBJECT)
 * @return Water class at the tile.
 */
static inline WaterClass GetWaterClass(TileIndex t)
{
	assert(HasTileWaterClass(t));
	return (WaterClass)GB(_m[t].m1, 5, 2);
}

/**
 * Set the water class at a tile.
 * @param t  Water tile to change.
 * @param wc New water class.
 * @pre IsTileType(t, MP_WATER) || IsTileType(t, MP_STATION) || IsTileType(t, MP_INDUSTRY) || IsTileType(t, MP_OBJECT)
 */
static inline void SetWaterClass(TileIndex t, WaterClass wc)
{
	assert(HasTileWaterClass(t));
	SB(_m[t].m1, 5, 2, wc);
}

/**
 * Tests if the tile was built on water.
 * @param t the tile to check
 * @pre IsTileType(t, MP_WATER) || IsTileType(t, MP_STATION) || IsTileType(t, MP_INDUSTRY) || IsTileType(t, MP_OBJECT)
 * @return true iff on water
 */
static inline bool IsTileOnWater(TileIndex t)
{
	return (GetWaterClass(t) != WATER_CLASS_INVALID);
}

/**
 * Is it a plain water tile?
 * @param t Water tile to query.
 * @return \c true if any type of clear water like ocean, river, or canal.
 */
static inline bool IsWater(TileIndex t)
{
	return GetWaterTileType(t) == WATER_TILE_CLEAR;
}

/**
 * Is it a sea water tile?
 * @param t Water tile to query.
 * @return \c true if it is a sea water tile.
 */
static inline bool IsSea(TileIndex t)
{
	return IsWater(t) && GetWaterClass(t) == WATER_CLASS_SEA;
}

/**
 * Is it a canal tile?
 * @param t Water tile to query.
 * @return \c true if it is a canal tile.
 */
static inline bool IsCanal(TileIndex t)
{
	return IsWater(t) && GetWaterClass(t) == WATER_CLASS_CANAL;
}

/**
 * Is it a river water tile?
 * @param t Water tile to query.
 * @return \c true if it is a river water tile.
 */
static inline bool IsRiver(TileIndex t)
{
	return IsWater(t) && GetWaterClass(t) == WATER_CLASS_RIVER;
}

/**
 * Is it a water tile with plain water?
 * @param t Tile to query.
 * @return \c true if it is a plain water tile.
 */
static inline bool IsWaterTile(TileIndex t)
{
	return IsTileType(t, MP_WATER) && IsWater(t);
}

/**
 * Is it a coast tile?
 * @param t Water tile to query.
 * @return \c true if it is a sea water tile.
 */
static inline bool IsCoast(TileIndex t)
{
	return GetWaterTileType(t) == WATER_TILE_COAST;
}

/**
 * Is it a coast tile
 * @param t Tile to query.
 * @return \c true if it is a coast.
 */
static inline bool IsCoastTile(TileIndex t)
{
	return IsTileType(t, MP_WATER) && IsCoast(t);
}

/**
 * Get the other tile of the ship depot.
 * @param t Tile to query, containing one section of a ship depot.
 * @return Tile containing the other section of the depot.
 */
static inline TileIndex GetOtherShipDepotTile(TileIndex t)
{
	return t + (HasBit(_m[t].m5, 0) ? -1 : 1) * (HasBit(_m[t].m5, 1) ? TileDiffXY(0, 1) : TileDiffXY(1, 0));
}

/**
 * Is it a water tile with a ship depot on it?
 * @param t Water tile to query.
 * @return \c true if it is a ship depot tile.
 */
static inline bool IsShipDepot(TileIndex t)
{
	return IsInsideMM(_m[t].m5, DEPOT_NORTH, DEPOT_END);
}

/**
 * Is it a ship depot tile?
 * @param t Tile to query.
 * @return \c true if it is a ship depot tile.
 */
static inline bool IsShipDepotTile(TileIndex t)
{
	return IsTileType(t, MP_WATER) && IsShipDepot(t);
}

/**
 * Get the axis of the ship depot.
 * @param t Water tile to query.
 * @return Axis of the depot.
 */
static inline Axis GetShipDepotAxis(TileIndex t)
{
	return (Axis)GB(_m[t].m5, 1, 1);
}

/**
 * Get the direction of the ship depot.
 * @param t Water tile to query.
 * @return Direction of the depot.
 */
static inline DiagDirection GetShipDepotDirection(TileIndex t)
{
	return XYNSToDiagDir(GetShipDepotAxis(t), GB(_m[t].m5, 0, 1));
}

/**
 * Get the most northern tile of a ship depot.
 * @param t One of the tiles of the ship depot.
 * @return The northern tile of the depot.
 */
static inline TileIndex GetShipDepotNorthTile(TileIndex t)
{
	assert(IsShipDepot(t));
	TileIndex tile2 = GetOtherShipDepotTile(t);

	return t < tile2 ? t : tile2;
}

/**
 * Is it a water lock tile?
 * @param t Water tile to query.
 * @return \c true if it is a water lock tile.
 */
static inline bool IsLock(TileIndex t)
{
	return IsInsideMM(_m[t].m5, LOCK_MIDDLE, LOCK_END);
}

/**
 * Get the direction of the water lock.
 * @param t Water tile to query.
 * @return Direction of the lock.
 */
static inline DiagDirection GetLockDirection(TileIndex t)
{
	return (DiagDirection)GB(_m[t].m5, 0, 2);
}

/**
 * Get a section of a depot or a lock.
 * @param t Water tile to query.
 * @return The section.
 */
static inline byte GetSection(TileIndex t)
{
	assert(GetWaterTileType(t) == WATER_TILE_LOCK || GetWaterTileType(t) == WATER_TILE_DEPOT);
	return GB(_m[t].m5, 0, 4);
}

/**
 * Get the random bits of the water tile.
 * @param t Water tile to query.
 * @return Random bits of the tile.
 */
static inline byte GetWaterTileRandomBits(TileIndex t)
{
	return _m[t].m4;
}

/**
 * Checks whether the tile has water at the ground.
 * That is, it is either some plain water tile, or a object/industry/station/... with water under it.
 * @return true iff the tile has water at the ground.
 * @note Coast tiles are not considered waterish, even if there is water on a halftile.
 */
static inline bool HasTileWaterGround(TileIndex t)
{
	return HasTileWaterClass(t) && IsTileOnWater(t) && !IsCoastTile(t);
}


/**
 * Helper function to make a coast tile.
 * @param t The tile to change into water
 */
static inline void MakeShore(TileIndex t)
{
	SetTileType(t, MP_WATER);
	SetTileOwner(t, OWNER_WATER);
	SetWaterClass(t, WATER_CLASS_SEA);
	_m[t].m2 = 0;
	_m[t].m3 = 0;
	_m[t].m4 = 0;
	_m[t].m5 = 1;
	SB(_m[t].m6, 2, 4, 0);
	_me[t].m7 = 0;
}

/**
 * Helper function for making a watery tile.
 * @param t The tile to change into water
 * @param o The owner of the water
 * @param wc The class of water the tile has to be
 * @param random_bits Eventual random bits to be set for this tile
 */
static inline void MakeWater(TileIndex t, Owner o, WaterClass wc, uint8 random_bits)
{
	SetTileType(t, MP_WATER);
	SetTileOwner(t, o);
	SetWaterClass(t, wc);
	_m[t].m2 = 0;
	_m[t].m3 = 0;
	_m[t].m4 = random_bits;
	_m[t].m5 = 0;
	SB(_m[t].m6, 2, 4, 0);
	_me[t].m7 = 0;
}

/**
 * Make a sea tile.
 * @param t The tile to change into sea
 */
static inline void MakeSea(TileIndex t)
{
	MakeWater(t, OWNER_WATER, WATER_CLASS_SEA, 0);
}

/**
 * Make a river tile
 * @param t The tile to change into river
 * @param random_bits Random bits to be set for this tile
 */
static inline void MakeRiver(TileIndex t, uint8 random_bits)
{
	MakeWater(t, OWNER_WATER, WATER_CLASS_RIVER, random_bits);
}

/**
 * Make a canal tile
 * @param t The tile to change into canal
 * @param o The owner of the canal
 * @param random_bits Random bits to be set for this tile
 */
static inline void MakeCanal(TileIndex t, Owner o, uint8 random_bits)
{
	assert(o != OWNER_WATER);
	MakeWater(t, o, WATER_CLASS_CANAL, random_bits);
}

/**
 * Make a ship depot section.
 * @param t    Tile to place the ship depot section.
 * @param o    Owner of the depot.
 * @param did  Depot ID.
 * @param base Depot base (either #DEPOT_NORTH or #DEPOT_SOUTH).
 * @param a    Axis of the depot.
 * @param original_water_class Original water class.
 */
static inline void MakeShipDepot(TileIndex t, Owner o, DepotID did, DepotPart base, Axis a, WaterClass original_water_class)
{
	SetTileType(t, MP_WATER);
	SetTileOwner(t, o);
	SetWaterClass(t, original_water_class);
	_m[t].m2 = did;
	_m[t].m3 = 0;
	_m[t].m4 = 0;
	_m[t].m5 = base + a * 2;
	SB(_m[t].m6, 2, 4, 0);
	_me[t].m7 = 0;
}

/**
 * Make a lock section.
 * @param t Tile to place the water lock section.
 * @param o Owner of the lock.
 * @param section Section to place.
 * @param original_water_class Original water class.
 * @see MakeLock
 */
static inline void MakeLockTile(TileIndex t, Owner o, byte section, WaterClass original_water_class)
{
	SetTileType(t, MP_WATER);
	SetTileOwner(t, o);
	SetWaterClass(t, original_water_class);
	_m[t].m2 = 0;
	_m[t].m3 = 0;
	_m[t].m4 = 0;
	_m[t].m5 = section;
	SB(_m[t].m6, 2, 4, 0);
	_me[t].m7 = 0;
}

/**
 * Make a water lock.
 * @param t Tile to place the water lock section.
 * @param o Owner of the lock.
 * @param d Direction of the water lock.
 * @param wc_lower Original water class of the lower part.
 * @param wc_upper Original water class of the upper part.
 */
static inline void MakeLock(TileIndex t, Owner o, DiagDirection d, WaterClass wc_lower, WaterClass wc_upper)
{
	TileIndexDiff delta = TileOffsByDiagDir(d);

	MakeLockTile(t, o, LOCK_MIDDLE + d, WATER_CLASS_CANAL);
	/* Keep the current owner for the upper and lower part if it is a
	 * water tile so we can restore the owner after deleting the lock. */
	MakeLockTile(t - delta, IsWaterTile(t - delta) ? GetTileOwner(t - delta) : o, LOCK_LOWER + d, wc_lower);
	MakeLockTile(t + delta, IsWaterTile(t + delta) ? GetTileOwner(t + delta) : o, LOCK_UPPER + d, wc_upper);
}

#endif /* WATER_MAP_H */
