/* $Id$ */

/** @file unmovable_map.h */

#ifndef UNMOVABLE_MAP_H
#define UNMOVABLE_MAP_H

enum {
	HQ_NUM_TILE = 4, ///< Number of HQ tiles
	HQ_NUM_SIZE = 5  ///< Number of stages of an HQ
};

/** Types of unmovable structure */
enum UnmovableType {
	UNMOVABLE_TRANSMITTER = 0,    ///< The large antenna
	UNMOVABLE_LIGHTHOUSE  = 1,    ///< The nice lighthouse
	UNMOVABLE_STATUE      = 2,    ///< Statue in towns
	UNMOVABLE_OWNED_LAND  = 3,    ///< Owned land 'flag'
	UNMOVABLE_HQ_NORTH    = 0x80, ///< Offset for the northern HQ tile
	UNMOVABLE_HQ_WEST     = 0x81, ///< Offset for the western HQ tile
	UNMOVABLE_HQ_EAST     = 0x82, ///< Offset for the eastern HQ tile
	UNMOVABLE_HQ_SOUTH    = 0x83, ///< Offset for the southern HQ tile

	/** End of the HQ (rather end + 1 for IS_INT_INSIDE) */
	UNMOVABLE_HQ_END      = UNMOVABLE_HQ_NORTH + HQ_NUM_SIZE * HQ_NUM_TILE
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
	return IS_INT_INSIDE(GetUnmovableType(t), UNMOVABLE_HQ_NORTH, UNMOVABLE_HQ_END);
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
	return GB(_m[t].m5, 2, 3);
}

/**
 * Get the 'section' (including stage) of the HQ.
 * @param t a tile of the HQ.
 * @pre IsTileType(t, MP_UNMOVABLE) && IsCompanyHQ(t)
 * @return the 'section' of the HQ.
 */
static inline byte GetCompanyHQSection(TileIndex t)
{
	assert(IsTileType(t, MP_UNMOVABLE) && IsCompanyHQ(t));
	return GB(_m[t].m5, 0, 5);
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
	assert(GB(GetCompanyHQSection(t), 0, 2) == 0);

	size *= 4;
	if (size <= _m[t].m5 - UNMOVABLE_HQ_NORTH) return;

	_m[t + TileDiffXY(0, 0)].m5 = UNMOVABLE_HQ_NORTH + size;
	_m[t + TileDiffXY(0, 1)].m5 = UNMOVABLE_HQ_WEST  + size;
	_m[t + TileDiffXY(1, 0)].m5 = UNMOVABLE_HQ_EAST  + size;
	_m[t + TileDiffXY(1, 1)].m5 = UNMOVABLE_HQ_SOUTH + size;
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
 * Make an HQ with the give tile as it's northern tile.
 * @param t the tile to make the northern tile of a HQ.
 * @param o the owner of the HQ.
 */
static inline void MakeCompanyHQ(TileIndex t, Owner o)
{
	MakeUnmovable(t + TileDiffXY(0, 0), UNMOVABLE_HQ_NORTH, o);
	MakeUnmovable(t + TileDiffXY(0, 1), UNMOVABLE_HQ_WEST, o);
	MakeUnmovable(t + TileDiffXY(1, 0), UNMOVABLE_HQ_EAST, o);
	MakeUnmovable(t + TileDiffXY(1, 1), UNMOVABLE_HQ_SOUTH, o);
}

#endif /* UNMOVABLE_MAP_H */
