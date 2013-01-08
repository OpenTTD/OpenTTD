/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file object_map.h Map accessors for object tiles. */

#ifndef OBJECT_MAP_H
#define OBJECT_MAP_H

#include "water_map.h"
#include "object_type.h"

/**
 * Gets the ObjectType of the given object tile
 * @param t the tile to get the type from.
 * @pre IsTileType(t, MP_OBJECT)
 * @return the type.
 */
static inline ObjectType GetObjectType(TileIndex t)
{
	assert(IsTileType(t, MP_OBJECT));
	return (ObjectType)_m[t].m5;
}

/**
 * Get the index of which object this tile is attached to.
 * @param t the tile
 * @pre IsTileType(t, MP_OBJECT)
 * @return The ObjectID of the object.
 */
static inline ObjectID GetObjectIndex(TileIndex t)
{
	assert(IsTileType(t, MP_OBJECT));
	return _m[t].m2;
}

/**
 * Does the given tile have a transmitter?
 * @param t the tile to inspect.
 * @return true if and only if the tile has a transmitter.
 */
static inline bool IsTransmitterTile(TileIndex t)
{
	return IsTileType(t, MP_OBJECT) && GetObjectType(t) == OBJECT_TRANSMITTER;
}

/**
 * Is this object tile an 'owned land' tile?
 * @param t the tile to inspect.
 * @pre IsTileType(t, MP_OBJECT)
 * @return true if and only if the tile is an 'owned land' tile.
 */
static inline bool IsOwnedLand(TileIndex t)
{
	assert(IsTileType(t, MP_OBJECT));
	return GetObjectType(t) == OBJECT_OWNED_LAND;
}

/**
 * Is the given tile (pre-)owned by someone (the little flags)?
 * @param t the tile to inspect.
 * @return true if and only if the tile is an 'owned land' tile.
 */
static inline bool IsOwnedLandTile(TileIndex t)
{
	return IsTileType(t, MP_OBJECT) && IsOwnedLand(t);
}

/**
 * Is this object tile a HQ tile?
 * @param t the tile to inspect.
 * @pre IsTileType(t, MP_OBJECT)
 * @return true if and only if the tile is a HQ tile.
 */
static inline bool IsCompanyHQ(TileIndex t)
{
	assert(IsTileType(t, MP_OBJECT));
	return _m[t].m5 == OBJECT_HQ;
}

/**
 * Is this object tile a statue?
 * @param t the tile to inspect.
 * @pre IsTileType(t, MP_OBJECT)
 * @return true if and only if the tile is a statue.
 */
static inline bool IsStatue(TileIndex t)
{
	assert(IsTileType(t, MP_OBJECT));
	return GetObjectType(t) == OBJECT_STATUE;
}

/**
 * Is the given tile a statue?
 * @param t the tile to inspect.
 * @return true if and only if the tile is a statue.
 */
static inline bool IsStatueTile(TileIndex t)
{
	return IsTileType(t, MP_OBJECT) && IsStatue(t);
}

/**
 * Get the random bits of this tile.
 * @param t The tile to get the bits for.
 * @pre IsTileType(t, MP_OBJECT)
 * @return The random bits.
 */
static inline byte GetObjectRandomBits(TileIndex t)
{
	assert(IsTileType(t, MP_OBJECT));
	return _m[t].m3;
}


/**
 * Make an Object tile.
 * @note do not use this function directly. Use one of the other Make* functions.
 * @param t      The tile to make and object tile.
 * @param u      The object type of the tile.
 * @param o      The new owner of the tile.
 * @param index  Index to the object.
 * @param wc     Water class for this object.
 * @param random Random data to store on the tile
 */
static inline void MakeObject(TileIndex t, ObjectType u, Owner o, ObjectID index, WaterClass wc, byte random)
{
	SetTileType(t, MP_OBJECT);
	SetTileOwner(t, o);
	SetWaterClass(t, wc);
	_m[t].m2 = index;
	_m[t].m3 = random;
	_m[t].m4 = 0;
	_m[t].m5 = u;
	SB(_m[t].m6, 2, 4, 0);
	_me[t].m7 = 0;
}

#endif /* OBJECT_MAP_H */
