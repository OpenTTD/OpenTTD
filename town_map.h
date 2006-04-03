/* $Id$ */

#ifndef TOWN_MAP_H
#define TOWN_MAP_H

#include "town.h"

static inline int GetHouseType(TileIndex t)
{
	assert(IsTileType(t, MP_HOUSE));
	return _m[t].m4;
}

static inline uint GetTownIndex(TileIndex t)
{
	assert(IsTileType(t, MP_HOUSE) || IsTileType(t, MP_STREET)); // XXX incomplete
	return _m[t].m2;
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

static inline Town* GetTownByTile(TileIndex t)
{
	return GetTown(GetTownIndex(t));
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
#endif
