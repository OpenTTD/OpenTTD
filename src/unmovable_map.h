/* $Id$ */

/** @file unmovable_map.h Map accessors for unmovable tiles. */

#ifndef UNMOVABLE_MAP_H
#define UNMOVABLE_MAP_H

#include "core/bitmath_func.hpp"
#include "tile_map.h"

/** Types of unmovable structure */
enum UnmovableType {
	UNMOVABLE_TRANSMITTER = 0,    ///< The large antenna
	UNMOVABLE_LIGHTHOUSE  = 1,    ///< The nice lighthouse
	UNMOVABLE_STATUE      = 2,    ///< Statue in towns
	UNMOVABLE_OWNED_LAND  = 3,    ///< Owned land 'flag'
	UNMOVABLE_HQ          = 4,    ///< HeadQuarter of a player
	UNMOVABLE_MAX,
};

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
 * Get the 'stage' of the HQ.
 * @param t a tile of the HQ.
 * @pre IsTileType(t, MP_UNMOVABLE) && IsCompanyHQ(t)
 * @return the 'stage' of the HQ.
 */
static inline byte GetCompanyHQSize(TileIndex t)
{
	assert(IsTileType(t, MP_UNMOVABLE) && IsCompanyHQ(t));
	return GB(_m[t].m3, 2, 3);
}

/**
 * Set the 'stage' of the HQ.
 * @param t a tile of the HQ.
 * @param size the actual stage of the HQ
 * @pre IsTileType(t, MP_UNMOVABLE) && IsCompanyHQ(t)
 */
static inline void SetCompanyHQSize(TileIndex t, uint8 size)
{
	assert(IsTileType(t, MP_UNMOVABLE) && IsCompanyHQ(t));
	SB(_m[t].m3, 2, 3, size);
}

/**
 * Get the 'section' of the HQ.
 * The scetion is in fact which side of teh HQ the tile represent
 * @param t a tile of the HQ.
 * @pre IsTileType(t, MP_UNMOVABLE) && IsCompanyHQ(t)
 * @return the 'section' of the HQ.
 */
static inline byte GetCompanyHQSection(TileIndex t)
{
	assert(IsTileType(t, MP_UNMOVABLE) && IsCompanyHQ(t));
	return GB(_m[t].m3, 0, 2);
}

/**
 * Set the 'section' of the HQ.
 * @param t a tile of the HQ.
 * param section to be set.
 * @pre IsTileType(t, MP_UNMOVABLE) && IsCompanyHQ(t)
 */
static inline void SetCompanyHQSection(TileIndex t, uint8 section)
{
	assert(IsTileType(t, MP_UNMOVABLE) && IsCompanyHQ(t));
	SB(_m[t].m3, 0, 2, section);
}

/**
 * Enlarge the given HQ to the given size. If the new size
 * is larger than the current size, nothing happens.
 * @param t the tile of the HQ.
 * @param size the new size of the HQ.
 * @pre t is the northern tile of the HQ
 */
static inline void EnlargeCompanyHQ(TileIndex t, byte size)
{
	assert(GetCompanyHQSection(t) == 0);
	assert(size <= 4);
	if (size <= GetCompanyHQSize(t)) return;

	SetCompanyHQSize(t                   , size);
	SetCompanyHQSize(t + TileDiffXY(0, 1), size);
	SetCompanyHQSize(t + TileDiffXY(1, 0), size);
	SetCompanyHQSize(t + TileDiffXY(1, 1), size);
}


/**
 * Make an Unmovable tile.
 * @note do not use this function directly. Use one of the other Make* functions.
 * @param t the tile to make unmovable.
 * @param u the unmovable type of the tile.
 * @param o the new owner of the tile.
 */
static inline void MakeUnmovable(TileIndex t, UnmovableType u, Owner o)
{
	SetTileType(t, MP_UNMOVABLE);
	SetTileOwner(t, o);
	_m[t].m2 = 0;
	_m[t].m3 = 0;
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
	MakeUnmovable(t, UNMOVABLE_HQ, o);
	SetCompanyHQSection(t, section);
}

/**
 * Make an HQ with the give tile as it's northern tile.
 * @param t the tile to make the northern tile of a HQ.
 * @param o the owner of the HQ.
 */
static inline void MakeCompanyHQ(TileIndex t, Owner o)
{
	MakeUnmovableHQHelper(t                   , 0, o);
	MakeUnmovableHQHelper(t + TileDiffXY(0, 1), 1, o);
	MakeUnmovableHQHelper(t + TileDiffXY(1, 0), 2, o);
	MakeUnmovableHQHelper(t + TileDiffXY(1, 1), 3, o);
}

#endif /* UNMOVABLE_MAP_H */
