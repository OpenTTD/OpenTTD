/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file unmovable_map.h Map accessors for unmovable tiles. */

#ifndef UNMOVABLE_MAP_H
#define UNMOVABLE_MAP_H

#include "tile_map.h"
#include "unmovable_type.h"

/**
 * Gets the UnmovableType of the given unmovable tile
 * @param t the tile to get the type from.
 * @pre IsTileType(t, MP_UNMOVABLE)
 * @return the type.
 */
static inline UnmovableType GetUnmovableType(TileIndex t)
{
	assert(IsTileType(t, MP_UNMOVABLE));
	return (UnmovableType)_m[t].m5;
}

/**
 * Does the given tile have a transmitter?
 * @param t the tile to inspect.
 * @return true if and only if the tile has a transmitter.
 */
static inline bool IsTransmitterTile(TileIndex t)
{
	return IsTileType(t, MP_UNMOVABLE) && GetUnmovableType(t) == UNMOVABLE_TRANSMITTER;
}

/**
 * Is this unmovable tile an 'owned land' tile?
 * @param t the tile to inspect.
 * @pre IsTileType(t, MP_UNMOVABLE)
 * @return true if and only if the tile is an 'owned land' tile.
 */
static inline bool IsOwnedLand(TileIndex t)
{
	assert(IsTileType(t, MP_UNMOVABLE));
	return GetUnmovableType(t) == UNMOVABLE_OWNED_LAND;
}

/**
 * Is the given tile (pre-)owned by someone (the little flags)?
 * @param t the tile to inspect.
 * @return true if and only if the tile is an 'owned land' tile.
 */
static inline bool IsOwnedLandTile(TileIndex t)
{
	return IsTileType(t, MP_UNMOVABLE) && IsOwnedLand(t);
}

/**
 * Is this unmovable tile a HQ tile?
 * @param t the tile to inspect.
 * @pre IsTileType(t, MP_UNMOVABLE)
 * @return true if and only if the tile is a HQ tile.
 */
static inline bool IsCompanyHQ(TileIndex t)
{
	assert(IsTileType(t, MP_UNMOVABLE));
	return _m[t].m5 == UNMOVABLE_HQ;
}

/**
 * Is this unmovable tile a statue?
 * @param t the tile to inspect.
 * @pre IsTileType(t, MP_UNMOVABLE)
 * @return true if and only if the tile is a statue.
 */
static inline bool IsStatue(TileIndex t)
{
	assert(IsTileType(t, MP_UNMOVABLE));
	return GetUnmovableType(t) == UNMOVABLE_STATUE;
}

/**
 * Is the given tile a statue?
 * @param t the tile to inspect.
 * @return true if and only if the tile is a statue.
 */
static inline bool IsStatueTile(TileIndex t)
{
	return IsTileType(t, MP_UNMOVABLE) && IsStatue(t);
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
 * @pre IsTileType(t, MP_UNMOVABLE)
 * @return The animation 'stage' of the tile.
 */
static inline byte GetUnmovableAnimationStage(TileIndex t)
{
	assert(IsTileType(t, MP_UNMOVABLE));
	return GB(_m[t].m6, 2, 4);
}

/**
 * Set animation stage/counter of this tile.
 * @param t     The tile to query.
 * @param stage The stage of this tile.
 * @pre IsTileType(t, MP_UNMOVABLE)
 */
static inline void SetUnmovableAnimationStage(TileIndex t, uint8 stage)
{
	assert(IsTileType(t, MP_UNMOVABLE));
	SB(_m[t].m6, 2, 4, stage);
}

/**
 * Get offset to the northern most tile.
 * @param t The tile to get the offset from.
 * @return The offset to the northern most tile of this structure.
 * @pre IsTileType(t, MP_UNMOVABLE)
 */
static inline byte GetUnmovableOffset(TileIndex t)
{
	assert(IsTileType(t, MP_UNMOVABLE));
	return _m[t].m3;
}

/**
 * Set offset to the northern most tile.
 * @param t      The tile to set the offset of.
 * @param offset The offset to the northern most tile of this structure.
 * @pre IsTileType(t, MP_UNMOVABLE)
 */
static inline void SetUnmovableOffset(TileIndex t, uint8 offset)
{
	assert(IsTileType(t, MP_UNMOVABLE));
	_m[t].m3 = offset;
}


/**
 * Make an Unmovable tile.
 * @note do not use this function directly. Use one of the other Make* functions.
 * @param t      The tile to make unmovable.
 * @param u      The unmovable type of the tile.
 * @param o      The new owner of the tile.
 * @param offset The offset to the northern tile of this object
 */
static inline void MakeUnmovable(TileIndex t, UnmovableType u, Owner o, uint8 offset = 0)
{
	SetTileType(t, MP_UNMOVABLE);
	SetTileOwner(t, o);
	_m[t].m2 = 0;
	_m[t].m3 = offset;
	_m[t].m4 = 0;
	_m[t].m5 = u;
	SB(_m[t].m6, 2, 4, 0);
	_me[t].m7 = 0;
}


/**
 * Make a transmitter tile.
 * @param t the tile to make a transmitter.
 */
static inline void MakeTransmitter(TileIndex t)
{
	MakeUnmovable(t, UNMOVABLE_TRANSMITTER, OWNER_NONE);
}

/**
 * Make a lighthouse tile.
 * @param t the tile to make a transmitter.
 */
static inline void MakeLighthouse(TileIndex t)
{
	MakeUnmovable(t, UNMOVABLE_LIGHTHOUSE, OWNER_NONE);
}

/**
 * Make a statue tile.
 * @param t the tile to make a statue.
 * @param o the owner of the statue.
 * @param town_id the town the statue was built in.
 */
static inline void MakeStatue(TileIndex t, Owner o, TownID town_id)
{
	MakeUnmovable(t, UNMOVABLE_STATUE, o);
	_m[t].m2 = town_id;
}

/**
 * Make an 'owned land' tile.
 * @param t the tile to make an 'owned land' tile.
 * @param o the owner of the land.
 */
static inline void MakeOwnedLand(TileIndex t, Owner o)
{
	MakeUnmovable(t, UNMOVABLE_OWNED_LAND, o);
}

/**
 * Make a HeadQuarter tile after making it an Unmovable
 * @param t the tile to make an HQ.
 * @param section the part of the HQ this one will be.
 * @param o the new owner of the tile.
 */
static inline void MakeUnmovableHQHelper(TileIndex t, uint8 section, Owner o)
{
	MakeUnmovable(t, UNMOVABLE_HQ, o, section);
}

/**
 * Make an HQ with the given tile as its northern tile.
 * @param t the tile to make the northern tile of a HQ.
 * @param o the owner of the HQ.
 */
static inline void MakeCompanyHQ(TileIndex t, Owner o)
{
	MakeUnmovableHQHelper(t,                    0x00, o);
	MakeUnmovableHQHelper(t + TileDiffXY(0, 1), 0x01, o);
	MakeUnmovableHQHelper(t + TileDiffXY(1, 0), 0x10, o);
	MakeUnmovableHQHelper(t + TileDiffXY(1, 1), 0x11, o);
}

#endif /* UNMOVABLE_MAP_H */
