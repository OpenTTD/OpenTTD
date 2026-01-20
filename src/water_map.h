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

/** Available water tile types. */
enum class WaterTileType : uint8_t {
	Clear = 0, ///< Plain water.
	Coast = 1, ///< Coast.
	Lock = 2, ///< Water lock.
	Depot = 3, ///< Water Depot.
};

/**
 * Get the internall TileExtended structure for ship depot.
 * @return The appropriate structure from TileExtended union.
 */
template<>
[[debug_inline]] inline auto &Tile::GetTileExtendedAs<TileType::Water, WaterTileType::Depot>()
{
	return extended_tiles[this->tile.base()].ship_depot;
}

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
	End, ///< End marker.
};

/** Sections of the water lock. */
enum class LockPart : uint8_t {
	Middle = 0, ///< Middle part of a lock.
	Lower = 1, ///< Lower part of a lock.
	Upper = 2, ///< Upper part of a lock.
	End, ///< End marker.
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
	assert(IsTileType(t, TileType::Water));
	return static_cast<WaterTileType>(t.GetTileExtendedAs<TileType::Water>().water_type);
}

/**
 * Set the water tile type of a tile.
 * @param t Water tile to set.
 * @param type Water tile type of the tile.
 */
inline void SetWaterTileType(Tile t, WaterTileType type)
{
	assert(IsTileType(t, TileType::Water));
	t.GetTileExtendedAs<TileType::Water>().water_type = to_underlying(type);
}

/**
 * Checks whether the tile has an waterclass associated.
 * You can then subsequently call GetWaterClass().
 * @param t Tile to query.
 * @return True if the tiletype has a waterclass.
 */
inline bool HasTileWaterClass(Tile t)
{
	return IsTileType(t, TileType::Water) || IsTileType(t, TileType::Station) || IsTileType(t, TileType::Industry) || IsTileType(t, TileType::Object) || IsTileType(t, TileType::Trees);
}

/**
 * Get the water class at a tile.
 * @param t Water tile to query.
 * @pre IsTileType(t, TileType::Water) || IsTileType(t, TileType::Station) || IsTileType(t, TileType::Industry) || IsTileType(t, TileType::Object)
 * @return Water class at the tile.
 */
inline WaterClass GetWaterClass(Tile t)
{
	assert(HasTileWaterClass(t));
	return static_cast<WaterClass>(t.GetTileExtendedAs<>().water_class);
}

/**
 * Set the water class at a tile.
 * @param t  Water tile to change.
 * @param wc New water class.
 * @pre IsTileType(t, TileType::Water) || IsTileType(t, TileType::Station) || IsTileType(t, TileType::Industry) || IsTileType(t, TileType::Object)
 */
inline void SetWaterClass(Tile t, WaterClass wc)
{
	assert(HasTileWaterClass(t));
	t.GetTileExtendedAs<>().water_class = to_underlying(wc);
}

/**
 * Tests if the tile was built on water.
 * @param t the tile to check
 * @pre IsTileType(t, TileType::Water) || IsTileType(t, TileType::Station) || IsTileType(t, TileType::Industry) || IsTileType(t, TileType::Object)
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
 * @pre IsTileType(t, TileType::Water)
 */
inline bool IsWater(Tile t)
{
	return GetWaterTileType(t) == WaterTileType::Clear;
}

/**
 * Is it a sea water tile?
 * @param t Water tile to query.
 * @return \c true if it is a sea water tile.
 * @pre IsTileType(t, TileType::Water)
 */
inline bool IsSea(Tile t)
{
	return IsWater(t) && GetWaterClass(t) == WaterClass::Sea;
}

/**
 * Is it a canal tile?
 * @param t Water tile to query.
 * @return \c true if it is a canal tile.
 * @pre IsTileType(t, TileType::Water)
 */
inline bool IsCanal(Tile t)
{
	return IsWater(t) && GetWaterClass(t) == WaterClass::Canal;
}

/**
 * Is it a river water tile?
 * @param t Water tile to query.
 * @return \c true if it is a river water tile.
 * @pre IsTileType(t, TileType::Water)
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
	return IsTileType(t, TileType::Water) && IsWater(t);
}

/**
 * Is it a coast tile?
 * @param t Water tile to query.
 * @return \c true if it is a sea water tile.
 * @pre IsTileType(t, TileType::Water)
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
	return (IsTileType(t, TileType::Water) && IsCoast(t)) || (IsTileType(t, TileType::Trees) && GetWaterClass(t) != WaterClass::Invalid);
}

/**
 * Is it a water tile with a ship depot on it?
 * @param t Water tile to query.
 * @return \c true if it is a ship depot tile.
 * @pre IsTileType(t, TileType::Water)
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
	return IsTileType(t, TileType::Water) && IsShipDepot(t);
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
	return Axis(t.GetTileExtendedAs<TileType::Water, WaterTileType::Depot>().axis);
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
	return static_cast<DepotPart>(t.GetTileExtendedAs<TileType::Water, WaterTileType::Depot>().part);
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
 * @pre IsTileType(t, TileType::Water)
 */
inline bool IsLock(Tile t)
{
	return GetWaterTileType(t) == WaterTileType::Lock;
}

/**
 * Get the direction of the water lock.
 * @param t Water tile to query.
 * @return Direction of the lock.
 * @pre IsTileType(t, TileType::Water) && IsLock(t)
 */
inline DiagDirection GetLockDirection(Tile t)
{
	assert(IsLock(t));
	return DiagDirection(t.GetTileExtendedAs<TileType::Water>().lock_direction);
}

/**
 * Get the part of a lock.
 * @param t Water tile to query.
 * @return The part.
 * @pre IsTileType(t, TileType::Water) && IsLock(t)
 */
inline LockPart GetLockPart(Tile t)
{
	assert(IsLock(t));
	return static_cast<LockPart>(t.GetTileExtendedAs<TileType::Water>().lock_part);
}

/**
 * Get the random bits of the water tile.
 * @param t Water tile to query.
 * @return Random bits of the tile.
 * @pre IsTileType(t, TileType::Water)
 */
inline uint8_t GetWaterTileRandomBits(Tile t)
{
	assert(IsTileType(t, TileType::Water));
	return t.GetTileBaseAs<TileType::Water>().random_bits;
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
	assert(IsTileType(t, TileType::Water) || IsTileType(t, TileType::Railway) || IsTileType(t, TileType::Station) || IsTileType(t, TileType::TunnelBridge));
	t.GetTileExtendedAs<>().ship_docking = b;
}

/**
 * Checks whether the tile is marked as a dockling tile.
 * @return true iff the tile is marked as a docking tile.
 */
inline bool IsDockingTile(Tile t)
{
	return (IsTileType(t, TileType::Water) || IsTileType(t, TileType::Railway) || IsTileType(t, TileType::Station) || IsTileType(t, TileType::TunnelBridge)) && t.GetTileExtendedAs<>().ship_docking;
}


/**
 * Helper function to make a coast tile.
 * @param t The tile to change into water
 */
inline void MakeShore(Tile t)
{
	SetTileType(t, TileType::Water);
	t.ResetData();
	SetTileOwner(t, OWNER_WATER);
	SetWaterClass(t, WaterClass::Sea);
	SetDockingTile(t, false);
	SetWaterTileType(t, WaterTileType::Coast);
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
	SetTileType(t, TileType::Water);
	t.ResetData();
	SetTileOwner(t, o);
	SetWaterClass(t, wc);
	SetDockingTile(t, false);
	t.GetTileBaseAs<TileType::Water>().random_bits = random_bits;
	SetWaterTileType(t, WaterTileType::Clear);
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
	SetTileType(t, TileType::Water);
	t.ResetData();
	SetTileOwner(t, o);
	SetWaterClass(t, original_water_class);
	SetDockingTile(t, false);
	auto &extended = t.GetTileExtendedAs<TileType::Water, WaterTileType::Depot>();
	extended.index = did.base();
	extended.part = to_underlying(part);
	extended.axis = a;
	SetWaterTileType(t, WaterTileType::Depot);
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
	SetTileType(t, TileType::Water);
	t.ResetData();
	SetTileOwner(t, o);
	SetWaterClass(t, original_water_class);
	SetDockingTile(t, false);
	auto &extended = t.GetTileExtendedAs<TileType::Water>();
	extended.lock_part = to_underlying(part);
	extended.lock_direction = dir;
	SetWaterTileType(t, WaterTileType::Lock);
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
	assert(IsTileType(t, TileType::Water));
	t.GetTileBaseAs<TileType::Water>().flood = b;
}
/**
 * Checks whether the tile is marked as a non-flooding water tile.
 * @return true iff the tile is marked as a non-flooding water tile.
 */
inline bool IsNonFloodingWaterTile(Tile t)
{
	assert(IsTileType(t, TileType::Water));
	return t.GetTileBaseAs<TileType::Water>().flood;
}

#endif /* WATER_MAP_H */
