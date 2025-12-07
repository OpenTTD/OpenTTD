/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file water_map.h Map accessors for water tiles. */

#ifndef WATER_MAP_H
#define WATER_MAP_H

#include "depot_type.h"
#include "tile_map.h"

/**
 * Bit field layout of m5 for water tiles.
 */
static constexpr uint8_t WBL_TYPE_BEGIN = 4; ///< Start of the 'type' bitfield.
static constexpr uint8_t WBL_TYPE_COUNT = 4; ///< Length of the 'type' bitfield.

static constexpr uint8_t WBL_LOCK_ORIENT_BEGIN = 0; ///< Start of lock orientation bitfield.
static constexpr uint8_t WBL_LOCK_ORIENT_COUNT = 2; ///< Length of lock orientation bitfield.
static constexpr uint8_t WBL_LOCK_PART_BEGIN = 2; ///< Start of lock part bitfield.
static constexpr uint8_t WBL_LOCK_PART_COUNT = 2; ///< Length of lock part bitfield.

static constexpr uint8_t WBL_DEPOT_PART = 0; ///< Depot part flag.
static constexpr uint8_t WBL_DEPOT_AXIS = 1; ///< Depot axis flag.

/** Available water tile types. */
enum class WaterTileType : uint8_t {
	Clear = 0, ///< Plain water.
	Coast = 1, ///< Coast.
	Lock = 2, ///< Water lock.
	Depot = 3, ///< Water Depot.
};

/** classes of water (for #WaterTileType::Clear water tile type). */
enum class WaterClass : uint8_t {
	Sea = 0, ///< Sea.
	Canal = 1, ///< Canal.
	River = 2, ///< River.
	Invalid = 3, ///< Used for industry tiles on land (also for oilrig if newgrf says so).
};

/**
 * Checks if a water class is valid.
 *
 * @param wc The value to check
 * @return true if the given value is a valid water class.
 */
inline bool IsValidWaterClass(WaterClass wc)
{
	return wc < WaterClass::Invalid;
}

/** Sections of the water depot. */
enum class DepotPart : uint8_t {
	North = 0, ///< Northern part of a depot.
	South = 1, ///< Southern part of a depot.
	End,
};

/** Sections of the water lock. */
enum class LockPart : uint8_t {
	Middle = 0, ///< Middle part of a lock.
	Lower = 1, ///< Lower part of a lock.
	Upper = 2, ///< Upper part of a lock.
	End,
};
DECLARE_INCREMENT_DECREMENT_OPERATORS(LockPart);

bool IsPossibleDockingTile(Tile t);

/**
 * Get the water tile type of a tile.
 * @param t Water tile to query.
 * @return Water tile type at the tile.
 */
inline WaterTileType GetWaterTileType(Tile t)
{
	assert(IsTileType(t, MP_WATER));
	return static_cast<WaterTileType>(GB(t.m5(), WBL_TYPE_BEGIN, WBL_TYPE_COUNT));
}

/**
 * Set the water tile type of a tile.
 * @param t Water tile to set.
 * @param type Water tile type of the tile.
 */
inline void SetWaterTileType(Tile t, WaterTileType type)
{
	assert(IsTileType(t, MP_WATER));
	SB(t.m5(), WBL_TYPE_BEGIN, WBL_TYPE_COUNT, to_underlying(type));
}

/**
 * Checks whether the tile has an waterclass associated.
 * You can then subsequently call GetWaterClass().
 * @param t Tile to query.
 * @return True if the tiletype has a waterclass.
 */
inline bool HasTileWaterClass(Tile t)
{
	return IsTileType(t, MP_WATER) || IsTileType(t, MP_STATION) || IsTileType(t, MP_INDUSTRY) || IsTileType(t, MP_OBJECT) || IsTileType(t, MP_TREES);
}

/**
 * Get the water class at a tile.
 * @param t Water tile to query.
 * @pre IsTileType(t, MP_WATER) || IsTileType(t, MP_STATION) || IsTileType(t, MP_INDUSTRY) || IsTileType(t, MP_OBJECT)
 * @return Water class at the tile.
 */
inline WaterClass GetWaterClass(Tile t)
{
	assert(HasTileWaterClass(t));
	return static_cast<WaterClass>(GB(t.m1(), 5, 2));
}

/**
 * Set the water class at a tile.
 * @param t  Water tile to change.
 * @param wc New water class.
 * @pre IsTileType(t, MP_WATER) || IsTileType(t, MP_STATION) || IsTileType(t, MP_INDUSTRY) || IsTileType(t, MP_OBJECT)
 */
inline void SetWaterClass(Tile t, WaterClass wc)
{
	assert(HasTileWaterClass(t));
	SB(t.m1(), 5, 2, to_underlying(wc));
}

/**
 * Tests if the tile was built on water.
 * @param t the tile to check
 * @pre IsTileType(t, MP_WATER) || IsTileType(t, MP_STATION) || IsTileType(t, MP_INDUSTRY) || IsTileType(t, MP_OBJECT)
 * @return true iff on water
 */
inline bool IsTileOnWater(Tile t)
{
	return (GetWaterClass(t) != WaterClass::Invalid);
}

/**
 * Is it a plain water tile?
 * @param t Water tile to query.
 * @return \c true if any type of clear water like ocean, river, or canal.
 * @pre IsTileType(t, MP_WATER)
 */
inline bool IsWater(Tile t)
{
	return GetWaterTileType(t) == WaterTileType::Clear;
}

/**
 * Is it a sea water tile?
 * @param t Water tile to query.
 * @return \c true if it is a sea water tile.
 * @pre IsTileType(t, MP_WATER)
 */
inline bool IsSea(Tile t)
{
	return IsWater(t) && GetWaterClass(t) == WaterClass::Sea;
}

/**
 * Is it a canal tile?
 * @param t Water tile to query.
 * @return \c true if it is a canal tile.
 * @pre IsTileType(t, MP_WATER)
 */
inline bool IsCanal(Tile t)
{
	return IsWater(t) && GetWaterClass(t) == WaterClass::Canal;
}

/**
 * Is it a river water tile?
 * @param t Water tile to query.
 * @return \c true if it is a river water tile.
 * @pre IsTileType(t, MP_WATER)
 */
inline bool IsRiver(Tile t)
{
	return IsWater(t) && GetWaterClass(t) == WaterClass::River;
}

/**
 * Is it a water tile with plain water?
 * @param t Tile to query.
 * @return \c true if it is a plain water tile.
 */
inline bool IsWaterTile(Tile t)
{
	return IsTileType(t, MP_WATER) && IsWater(t);
}

/**
 * Is it a coast tile?
 * @param t Water tile to query.
 * @return \c true if it is a sea water tile.
 * @pre IsTileType(t, MP_WATER)
 */
inline bool IsCoast(Tile t)
{
	return GetWaterTileType(t) == WaterTileType::Coast;
}

/**
 * Is it a coast tile
 * @param t Tile to query.
 * @return \c true if it is a coast.
 */
inline bool IsCoastTile(Tile t)
{
	return (IsTileType(t, MP_WATER) && IsCoast(t)) || (IsTileType(t, MP_TREES) && GetWaterClass(t) != WaterClass::Invalid);
}

/**
 * Is it a water tile with a ship depot on it?
 * @param t Water tile to query.
 * @return \c true if it is a ship depot tile.
 * @pre IsTileType(t, MP_WATER)
 */
inline bool IsShipDepot(Tile t)
{
	return GetWaterTileType(t) == WaterTileType::Depot;
}

/**
 * Is it a ship depot tile?
 * @param t Tile to query.
 * @return \c true if it is a ship depot tile.
 */
inline bool IsShipDepotTile(Tile t)
{
	return IsTileType(t, MP_WATER) && IsShipDepot(t);
}

/**
 * Get the axis of the ship depot.
 * @param t Water tile to query.
 * @return Axis of the depot.
 * @pre IsShipDepotTile(t)
 */
inline Axis GetShipDepotAxis(Tile t)
{
	assert(IsShipDepotTile(t));
	return (Axis)GB(t.m5(), WBL_DEPOT_AXIS, 1);
}

/**
 * Get the part of a ship depot.
 * @param t Water tile to query.
 * @return Part of the depot.
 * @pre IsShipDepotTile(t)
 */
inline DepotPart GetShipDepotPart(Tile t)
{
	assert(IsShipDepotTile(t));
	return static_cast<DepotPart>(GB(t.m5(), WBL_DEPOT_PART, 1));
}

/**
 * Get the direction of the ship depot.
 * @param t Water tile to query.
 * @return Direction of the depot.
 * @pre IsShipDepotTile(t)
 */
inline DiagDirection GetShipDepotDirection(Tile t)
{
	return XYNSToDiagDir(GetShipDepotAxis(t), to_underlying(GetShipDepotPart(t)));
}

/**
 * Get the other tile of the ship depot.
 * @param t Tile to query, containing one section of a ship depot.
 * @return Tile containing the other section of the depot.
 * @pre IsShipDepotTile(t)
 */
inline TileIndex GetOtherShipDepotTile(Tile t)
{
	return TileIndex(t) + (GetShipDepotPart(t) != DepotPart::North ? -1 : 1) * TileOffsByAxis(GetShipDepotAxis(t));
}

/**
 * Get the most northern tile of a ship depot.
 * @param t One of the tiles of the ship depot.
 * @return The northern tile of the depot.
 * @pre IsShipDepotTile(t)
 */
inline TileIndex GetShipDepotNorthTile(Tile t)
{
	assert(IsShipDepot(t));
	TileIndex tile2 = GetOtherShipDepotTile(t);

	return t < tile2 ? TileIndex(t) : tile2;
}

/**
 * Is there a lock on a given water tile?
 * @param t Water tile to query.
 * @return \c true if it is a water lock tile.
 * @pre IsTileType(t, MP_WATER)
 */
inline bool IsLock(Tile t)
{
	return GetWaterTileType(t) == WaterTileType::Lock;
}

/**
 * Get the direction of the water lock.
 * @param t Water tile to query.
 * @return Direction of the lock.
 * @pre IsTileType(t, MP_WATER) && IsLock(t)
 */
inline DiagDirection GetLockDirection(Tile t)
{
	assert(IsLock(t));
	return (DiagDirection)GB(t.m5(), WBL_LOCK_ORIENT_BEGIN, WBL_LOCK_ORIENT_COUNT);
}

/**
 * Get the part of a lock.
 * @param t Water tile to query.
 * @return The part.
 * @pre IsTileType(t, MP_WATER) && IsLock(t)
 */
inline LockPart GetLockPart(Tile t)
{
	assert(IsLock(t));
	return static_cast<LockPart>(GB(t.m5(), WBL_LOCK_PART_BEGIN, WBL_LOCK_PART_COUNT));
}

/**
 * Get the random bits of the water tile.
 * @param t Water tile to query.
 * @return Random bits of the tile.
 * @pre IsTileType(t, MP_WATER)
 */
inline uint8_t GetWaterTileRandomBits(Tile t)
{
	assert(IsTileType(t, MP_WATER));
	return t.m4();
}

/**
 * Checks whether the tile has water at the ground.
 * That is, it is either some plain water tile, or a object/industry/station/... with water under it.
 * @return true iff the tile has water at the ground.
 * @note Coast tiles are not considered waterish, even if there is water on a halftile.
 */
inline bool HasTileWaterGround(Tile t)
{
	return HasTileWaterClass(t) && IsTileOnWater(t) && !IsCoastTile(t);
}

/**
 * Set the docking tile state of a tile. This is used by pathfinders to reach their destination.
 * As well as water tiles, half-rail tiles, buoys and aqueduct ends can also be docking tiles.
 * @param t the tile
 * @param b the docking tile state
 */
inline void SetDockingTile(Tile t, bool b)
{
	assert(IsTileType(t, MP_WATER) || IsTileType(t, MP_RAILWAY) || IsTileType(t, MP_STATION) || IsTileType(t, MP_TUNNELBRIDGE));
	AssignBit(t.m1(), 7, b);
}

/**
 * Checks whether the tile is marked as a dockling tile.
 * @return true iff the tile is marked as a docking tile.
 */
inline bool IsDockingTile(Tile t)
{
	return (IsTileType(t, MP_WATER) || IsTileType(t, MP_RAILWAY) || IsTileType(t, MP_STATION) || IsTileType(t, MP_TUNNELBRIDGE)) && HasBit(t.m1(), 7);
}


/**
 * Helper function to make a coast tile.
 * @param t The tile to change into water
 */
inline void MakeShore(Tile t)
{
	SetTileType(t, MP_WATER);
	SetTileOwner(t, OWNER_WATER);
	SetWaterClass(t, WaterClass::Sea);
	SetDockingTile(t, false);
	t.m2() = 0;
	t.m3() = 0;
	t.m4() = 0;
	t.m5() = 0;
	SetWaterTileType(t, WaterTileType::Coast);
	SB(t.m6(), 2, 6, 0);
	t.m7() = 0;
	t.m8() = 0;
}

/**
 * Helper function for making a watery tile.
 * @param t The tile to change into water
 * @param o The owner of the water
 * @param wc The class of water the tile has to be
 * @param random_bits Eventual random bits to be set for this tile
 */
inline void MakeWater(Tile t, Owner o, WaterClass wc, uint8_t random_bits)
{
	SetTileType(t, MP_WATER);
	SetTileOwner(t, o);
	SetWaterClass(t, wc);
	SetDockingTile(t, false);
	t.m2() = 0;
	t.m3() = 0;
	t.m4() = random_bits;
	t.m5() = 0;
	SetWaterTileType(t, WaterTileType::Clear);
	SB(t.m6(), 2, 6, 0);
	t.m7() = 0;
	t.m8() = 0;
}

/**
 * Make a sea tile.
 * @param t The tile to change into sea
 */
inline void MakeSea(Tile t)
{
	MakeWater(t, OWNER_WATER, WaterClass::Sea, 0);
}

/**
 * Make a river tile
 * @param t The tile to change into river
 * @param random_bits Random bits to be set for this tile
 */
inline void MakeRiver(Tile t, uint8_t random_bits)
{
	MakeWater(t, OWNER_WATER, WaterClass::River, random_bits);
}

/**
 * Make a canal tile
 * @param t The tile to change into canal
 * @param o The owner of the canal
 * @param random_bits Random bits to be set for this tile
 */
inline void MakeCanal(Tile t, Owner o, uint8_t random_bits)
{
	assert(o != OWNER_WATER);
	MakeWater(t, o, WaterClass::Canal, random_bits);
}

/**
 * Make a ship depot section.
 * @param t    Tile to place the ship depot section.
 * @param o    Owner of the depot.
 * @param did  Depot ID.
 * @param part Depot part (either #DepotPart::North or #DepotPart::South).
 * @param a    Axis of the depot.
 * @param original_water_class Original water class.
 */
inline void MakeShipDepot(Tile t, Owner o, DepotID did, DepotPart part, Axis a, WaterClass original_water_class)
{
	SetTileType(t, MP_WATER);
	SetTileOwner(t, o);
	SetWaterClass(t, original_water_class);
	SetDockingTile(t, false);
	t.m2() = did.base();
	t.m3() = 0;
	t.m4() = 0;
	t.m5() = to_underlying(part) << WBL_DEPOT_PART | a << WBL_DEPOT_AXIS;
	SetWaterTileType(t, WaterTileType::Depot);
	SB(t.m6(), 2, 6, 0);
	t.m7() = 0;
	t.m8() = 0;
}

/**
 * Make a lock section.
 * @param t Tile to place the water lock section.
 * @param o Owner of the lock.
 * @param part Part to place.
 * @param dir Lock orientation
 * @param original_water_class Original water class.
 * @see MakeLock
 */
inline void MakeLockTile(Tile t, Owner o, LockPart part, DiagDirection dir, WaterClass original_water_class)
{
	SetTileType(t, MP_WATER);
	SetTileOwner(t, o);
	SetWaterClass(t, original_water_class);
	SetDockingTile(t, false);
	t.m2() = 0;
	t.m3() = 0;
	t.m4() = 0;
	t.m5() = to_underlying(part) << WBL_LOCK_PART_BEGIN | dir << WBL_LOCK_ORIENT_BEGIN;
	SetWaterTileType(t, WaterTileType::Lock);
	SB(t.m6(), 2, 6, 0);
	t.m7() = 0;
	t.m8() = 0;
}

/**
 * Make a water lock.
 * @param t Tile to place the water lock section.
 * @param o Owner of the lock.
 * @param d Direction of the water lock.
 * @param wc_lower Original water class of the lower part.
 * @param wc_upper Original water class of the upper part.
 * @param wc_middle Original water class of the middle part.
 */
inline void MakeLock(Tile t, Owner o, DiagDirection d, WaterClass wc_lower, WaterClass wc_upper, WaterClass wc_middle)
{
	TileIndexDiff delta = TileOffsByDiagDir(d);
	Tile lower_tile = TileIndex(t) - delta;
	Tile upper_tile = TileIndex(t) + delta;

	/* Keep the current waterclass and owner for the tiles.
	 * It allows to restore them after the lock is deleted */
	MakeLockTile(t, o, LockPart::Middle, d, wc_middle);
	MakeLockTile(lower_tile, IsWaterTile(lower_tile) ? GetTileOwner(lower_tile) : o, LockPart::Lower, d, wc_lower);
	MakeLockTile(upper_tile, IsWaterTile(upper_tile) ? GetTileOwner(upper_tile) : o, LockPart::Upper, d, wc_upper);
}

/**
 * Set the non-flooding water tile state of a tile.
 * @param t the tile
 * @param b the non-flooding water tile state
 */
inline void SetNonFloodingWaterTile(Tile t, bool b)
{
	assert(IsTileType(t, MP_WATER));
	AssignBit(t.m3(), 0, b);
}
/**
 * Checks whether the tile is marked as a non-flooding water tile.
 * @return true iff the tile is marked as a non-flooding water tile.
 */
inline bool IsNonFloodingWaterTile(Tile t)
{
	assert(IsTileType(t, MP_WATER));
	return HasBit(t.m3(), 0);
}

#endif /* WATER_MAP_H */
