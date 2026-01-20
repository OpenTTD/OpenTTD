/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file object_map.h Map accessors for object tiles. */

#ifndef OBJECT_MAP_H
#define OBJECT_MAP_H

#include "water_map.h"
#include "object_type.h"

ObjectType GetObjectType(Tile t);

/**
 * Check whether the object on a tile is of a specific type.
 * @param t Tile to test.
 * @param type Type to test.
 * @pre IsTileType(t, TileType::Object)
 * @return True if type matches.
 */
inline bool IsObjectType(Tile t, ObjectType type)
{
	return GetObjectType(t) == type;
}

/**
 * Check whether a tile is a object tile of a specific type.
 * @param t Tile to test.
 * @param type Type to test.
 * @return True if type matches.
 */
inline bool IsObjectTypeTile(Tile t, ObjectType type)
{
	return IsTileType(t, TileType::Object) && GetObjectType(t) == type;
}

/**
 * Get the index of which object this tile is attached to.
 * @param t the tile
 * @pre IsTileType(t, TileType::Object)
 * @return The ObjectID of the object.
 */
inline ObjectID GetObjectIndex(Tile t)
{
	assert(IsTileType(t, TileType::Object));
	auto &extended = t.GetTileExtendedAs<TileType::Object>();
	return ObjectID(extended.index_low_bits | extended.index_high_bits << 16);
}

/**
 * Get the random bits of this tile.
 * @param t The tile to get the bits for.
 * @pre IsTileType(t, TileType::Object)
 * @return The random bits.
 */
inline uint8_t GetObjectRandomBits(Tile t)
{
	assert(IsTileType(t, TileType::Object));
	return t.GetTileBaseAs<TileType::Object>().random_bits;
}


/**
 * Make an Object tile.
 * @param t      The tile to make and object tile.
 * @param o      The new owner of the tile.
 * @param index  Index to the object.
 * @param wc     Water class for this object.
 * @param random Random data to store on the tile
 */
inline void MakeObject(Tile t, Owner o, ObjectID index, WaterClass wc, uint8_t random)
{
	SetTileType(t, TileType::Object);
	t.ResetData();
	SetTileOwner(t, o);
	SetWaterClass(t, wc);
	auto &extended = t.GetTileExtendedAs<TileType::Object>();
	extended.index_low_bits = index.base();
	extended.index_high_bits = index.base() >> 16;
	t.GetTileBaseAs<TileType::Object>().random_bits = random;
}

#endif /* OBJECT_MAP_H */
