/* $Id$ */

/** @file town_map.h Accessors for towns */

#ifndef TOWN_MAP_H
#define TOWN_MAP_H

#include "town.h"

static inline TownID GetTownIndex(TileIndex t)
{
	assert(IsTileType(t, MP_HOUSE) || IsTileType(t, MP_STREET)); // XXX incomplete
	return _m[t].m2;
}

/**
 * Set the town index for a road or house tile.
 * @param tile the tile
 * @param index the index of the town
 * @pre IsTileType(t, MP_STREET) || IsTileType(t, MP_HOUSE)
 */
static inline void SetTownIndex(TileIndex t, TownID index)
{
	assert(IsTileType(t, MP_STREET) || IsTileType(t, MP_HOUSE));
	_m[t].m2 = index;
}

/**
 * Gets the town associated with the house or road tile
 * @param t the tile to get the town of
 * @return the town
 */
static inline Town* GetTownByTile(TileIndex t)
{
	return GetTown(GetTownIndex(t));
}


static inline int GetHouseType(TileIndex t)
{
	assert(IsTileType(t, MP_HOUSE));
	return _m[t].m4;
}

static inline bool LiftHasDestination(TileIndex t)
{
	return HASBIT(_m[t].m5, 7);
}

static inline void SetLiftDestination(TileIndex t, byte dest)
{
	SB(_m[t].m5, 0, 6, dest);
	SETBIT(_m[t].m1, 7); /* Start moving */
}

static inline byte GetLiftDestination(TileIndex t)
{
	return GB(_m[t].m5, 0, 6);
}

static inline bool IsLiftMoving(TileIndex t)
{
	return HASBIT(_m[t].m1, 7);
}

static inline void BeginLiftMovement(TileIndex t)
{
	SETBIT(_m[t].m5, 7);
}

static inline void HaltLift(TileIndex t)
{
	CLRBIT(_m[t].m1, 7);
	CLRBIT(_m[t].m5, 7);
	SB(_m[t].m5, 0, 6, 0);

	DeleteAnimatedTile(t);
}

static inline byte GetLiftPosition(TileIndex t)
{
	return GB(_m[t].m1, 0, 7);
}

static inline void SetLiftPosition(TileIndex t, byte pos)
{
	SB(_m[t].m1, 0, 7, pos);
}

static inline void MakeHouseTile(TileIndex t, TownID tid, byte counter, byte stage, byte type)
{
	assert(IsTileType(t, MP_CLEAR));

	SetTileType(t, MP_HOUSE);
	_m[t].m1 = 0;
	_m[t].m2 = tid;
	SB(_m[t].m3, 6, 2, stage);
	_m[t].m4 = type;
	SB(_m[t].m5, 0, 2, counter);

	MarkTileDirtyByTile(t);
}

enum {
	TWO_BY_TWO_BIT = 2, ///< House is two tiles in X and Y directions
	ONE_BY_TWO_BIT = 1, ///< House is two tiles in Y direction
	TWO_BY_ONE_BIT = 0, ///< House is two tiles in X direction
};

static inline void MakeTownHouse(TileIndex t, TownID tid, byte counter, byte stage, byte size, byte type)
{
	MakeHouseTile(t, tid, counter, stage, type);
	if (HASBIT(size, TWO_BY_TWO_BIT) || HASBIT(size, ONE_BY_TWO_BIT)) MakeHouseTile(t + TileDiffXY(0, 1), tid, counter, stage, ++type);
	if (HASBIT(size, TWO_BY_TWO_BIT) || HASBIT(size, TWO_BY_ONE_BIT)) MakeHouseTile(t + TileDiffXY(1, 0), tid, counter, stage, ++type);
	if (HASBIT(size, TWO_BY_TWO_BIT)) MakeHouseTile(t + TileDiffXY(1, 1), tid, counter, stage, ++type);
}

/**
 * House Construction Scheme.
 *  Construction counter, for buildings under construction. Incremented on every
 *  periodic tile processing.
 *  On wraparound, the stage of building in is increased.
 *  (Get|Set|Inc)HouseBuildingStage are taking care of the real stages,
 *  (as the sprite for the next phase of house building)
 *  (Get|Set|Inc)HouseConstructionTick is simply a tick counter between the
 *  different stages
 */

/**
 * Gets the building stage of a house
 * @param tile the tile of the house to get the building stage of
 * @pre IsTileType(t, MP_HOUSE)
 * @return the building stage of the house
 */
static inline byte GetHouseBuildingStage(TileIndex t)
{
	assert(IsTileType(t, MP_HOUSE));
	return GB(_m[t].m3, 6, 2);
}

/**
 * Sets the building stage of a house
 * @param tile the tile of the house to set the building stage of
 * @param stage the new stage
 * @pre IsTileType(t, MP_HOUSE)
 */
static inline void SetHouseBuildingStage(TileIndex t, byte stage)
{
	assert(IsTileType(t, MP_HOUSE));
	SB(_m[t].m3, 6, 2, stage);
}

/**
 * Increments the building stage of a house
 * @param tile the tile of the house to increment the building stage of
 * @pre IsTileType(t, MP_HOUSE)
 */
static inline void IncHouseBuildingStage( TileIndex t )
{
	assert(IsTileType(t, MP_HOUSE));
	AB(_m[t].m3, 6, 2, 1);
}

/**
 * Gets the construction stage of a house
 * @param tile the tile of the house to get the construction stage of
 * @pre IsTileType(t, MP_HOUSE)
 * @return the construction stage of the house
 */
static inline byte GetHouseConstructionTick(TileIndex t)
{
	assert(IsTileType(t, MP_HOUSE));
	return GB(_m[t].m5, 0, 3);
}

/**
 * Sets the construction stage of a house
 * @param tile the tile of the house to set the construction stage of
 * @param stage the new stage
 * @pre IsTileType(t, MP_HOUSE)
 */
static inline void SetHouseConstructionTick(TileIndex t, byte stage)
{
	assert(IsTileType(t, MP_HOUSE));
	SB(_m[t].m5, 0, 3, stage);
}

/**
 * Sets the increment stage of a house
 * @param tile the tile of the house to increment the construction stage of
 * @pre IsTileType(t, MP_HOUSE)
 */
static inline void IncHouseConstructionTick(TileIndex t)
{
	assert(IsTileType(t, MP_HOUSE));
	AB(_m[t].m5, 0, 3, 1);
}


#endif /* TOWN_MAP_H */
