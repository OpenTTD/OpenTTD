/* $Id$ */

#ifndef UNMOVABLE_MAP_H
#define UNMOVABLE_MAP_H

enum {
	HQ_NUM_TILE = 4,
	HQ_NUM_SIZE = 5
};

enum UnmovableType {
	UNMOVABLE_TRANSMITTER = 0,
	UNMOVABLE_LIGHTHOUSE  = 1,
	UNMOVABLE_STATUE      = 2,
	UNMOVABLE_OWNED_LAND  = 3,
	UNMOVABLE_HQ_NORTH    = 0x80,
	UNMOVABLE_HQ_WEST     = 0x81,
	UNMOVABLE_HQ_EAST     = 0x82,
	UNMOVABLE_HQ_SOUTH    = 0x83,

	UNMOVABLE_HQ_END      = UNMOVABLE_HQ_NORTH + HQ_NUM_SIZE * HQ_NUM_TILE
};



static inline UnmovableType GetUnmovableType(TileIndex t)
{
	assert(IsTileType(t, MP_UNMOVABLE));
	return (UnmovableType)_m[t].m5;
}


static inline bool IsTransmitterTile(TileIndex t)
{
	return
		IsTileType(t, MP_UNMOVABLE) &&
		GetUnmovableType(t) == UNMOVABLE_TRANSMITTER;
}


static inline bool IsOwnedLand(TileIndex t)
{
	assert(IsTileType(t, MP_UNMOVABLE));
	return GetUnmovableType(t) == UNMOVABLE_OWNED_LAND;
}

static inline bool IsOwnedLandTile(TileIndex t)
{
	return IsTileType(t, MP_UNMOVABLE) && IsOwnedLand(t);
}

static inline bool IsCompanyHQ(TileIndex t)
{
	return IS_INT_INSIDE(GetUnmovableType(t), UNMOVABLE_HQ_NORTH, UNMOVABLE_HQ_END);
}

static inline bool IsStatue(TileIndex t)
{
	assert(IsTileType(t, MP_UNMOVABLE));
	return GetUnmovableType(t) == UNMOVABLE_STATUE;
}

static inline bool IsStatueTile(TileIndex t)
{
	return IsTileType(t, MP_UNMOVABLE) && IsStatue(t);
}

static inline TownID GetStatueTownID(TileIndex t)
{
	assert(IsStatue(t));
	return _m[t].m2;
}

static inline byte GetCompanyHQSize(TileIndex t)
{
	assert(IsTileType(t, MP_UNMOVABLE) && IsCompanyHQ(t));
	return GB(_m[t].m5, 2, 3);
}

static inline byte GetCompanyHQSection(TileIndex t)
{
	assert(IsTileType(t, MP_UNMOVABLE) && IsCompanyHQ(t));
	return GB(_m[t].m5, 0, 5);
}


static inline void EnlargeCompanyHQ(TileIndex t, byte size)
{
	size *= 4;
	if (size <= _m[t].m5 - UNMOVABLE_HQ_NORTH) return;

	_m[t + TileDiffXY(0, 0)].m5 = UNMOVABLE_HQ_NORTH + size;
	_m[t + TileDiffXY(0, 1)].m5 = UNMOVABLE_HQ_WEST  + size;
	_m[t + TileDiffXY(1, 0)].m5 = UNMOVABLE_HQ_EAST  + size;
	_m[t + TileDiffXY(1, 1)].m5 = UNMOVABLE_HQ_SOUTH + size;
}


static inline void MakeUnmovable(TileIndex t, UnmovableType u, Owner o)
{
	SetTileType(t, MP_UNMOVABLE);
	SetTileOwner(t, o);
	_m[t].m2 = 0;
	_m[t].m3 = 0;
	_m[t].m4 = 0;
	_m[t].m5 = u;
}


static inline void MakeTransmitter(TileIndex t)
{
	MakeUnmovable(t, UNMOVABLE_TRANSMITTER, OWNER_NONE);
}

static inline void MakeLighthouse(TileIndex t)
{
	MakeUnmovable(t, UNMOVABLE_LIGHTHOUSE, OWNER_NONE);
}

static inline void MakeStatue(TileIndex t, Owner o, TownID town_id)
{
	MakeUnmovable(t, UNMOVABLE_STATUE, o);
	_m[t].m2 = town_id;
}

static inline void MakeOwnedLand(TileIndex t, Owner o)
{
	MakeUnmovable(t, UNMOVABLE_OWNED_LAND, o);
}

static inline void MakeCompanyHQ(TileIndex t, Owner o)
{
	MakeUnmovable(t + TileDiffXY(0, 0), UNMOVABLE_HQ_NORTH, o);
	MakeUnmovable(t + TileDiffXY(0, 1), UNMOVABLE_HQ_WEST, o);
	MakeUnmovable(t + TileDiffXY(1, 0), UNMOVABLE_HQ_EAST, o);
	MakeUnmovable(t + TileDiffXY(1, 1), UNMOVABLE_HQ_SOUTH, o);
}

#endif /* UNMOVABLE_MAP_H */
