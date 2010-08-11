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

#include "tile_map.h"
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
 * Get the town of the given statue tile.
 * @param t the tile of the statue.
 * @pre IsStatueTile(t)
 * @return the town the given statue is in.
 */
static inline TownID GetStatueTownID(TileIndex t)
{
	assert(IsStatueTile(t));
	return _m[t].m2;
}

/**
 * Get animation stage/counter of this tile.
 * @param t The tile to query.
 * @pre IsTileType(t, MP_OBJECT)
 * @return The animation 'stage' of the tile.
 */
static inline byte GetObjectAnimationStage(TileIndex t)
{
	assert(IsTileType(t, MP_OBJECT));
	return GB(_m[t].m6, 2, 4);
}

/**
 * Set animation stage/counter of this tile.
 * @param t     The tile to query.
 * @param stage The stage of this tile.
 * @pre IsTileType(t, MP_OBJECT)
 */
static inline void SetObjectAnimationStage(TileIndex t, uint8 stage)
{
	assert(IsTileType(t, MP_OBJECT));
	SB(_m[t].m6, 2, 4, stage);
}

/**
 * Get offset to the northern most tile.
 * @param t The tile to get the offset from.
 * @return The offset to the northern most tile of this structure.
 * @pre IsTileType(t, MP_OBJECT)
 */
static inline byte GetObjectOffset(TileIndex t)
{
	assert(IsTileType(t, MP_OBJECT));
	return _m[t].m3;
}

/**
 * Set offset to the northern most tile.
 * @param t      The tile to set the offset of.
 * @param offset The offset to the northern most tile of this structure.
 * @pre IsTileType(t, MP_OBJECT)
 */
static inline void SetObjectOffset(TileIndex t, uint8 offset)
{
	assert(IsTileType(t, MP_OBJECT));
	_m[t].m3 = offset;
}


/**
 * Make an Object tile.
 * @note do not use this function directly. Use one of the other Make* functions.
 * @param t      The tile to make and object tile.
 * @param u      The object type of the tile.
 * @param o      The new owner of the tile.
 * @param offset The offset to the northern tile of this object.
 * @param index  Generic index associated with the object type.
 * @param wc     Water class for this obect.
 */
static inline void MakeObject(TileIndex t, ObjectType u, Owner o, uint8 offset, uint index, WaterClass wc)
{
	SetTileType(t, MP_OBJECT);
	SetTileOwner(t, o);
	SetWaterClass(t, wc);
	_m[t].m2 = index;
	_m[t].m3 = offset;
	_m[t].m4 = 0;
	_m[t].m5 = u;
	SB(_m[t].m6, 2, 4, 0);
	_me[t].m7 = 0;
}

#endif /* OBJECT_MAP_H */
