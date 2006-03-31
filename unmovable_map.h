/* $Id$ */

typedef enum UnmovableType {
	UNMOVABLE_TRANSMITTER = 0,
	UNMOVABLE_LIGHTHOUSE  = 1,
	UNMOVABLE_STATUE      = 2,
	UNMOVABLE_OWNED_LAND  = 3,
	UNMOVABLE_HQ_NORTH    = 0x80,
	UNMOVABLE_HQ_WEST     = 0x81,
	UNMOVABLE_HQ_EAST     = 0x82,
	UNMOVABLE_HQ_SOUTH    = 0x83,
} UnmovableType;


static inline UnmovableType GetUnmovableType(TileIndex t)
{
	assert(IsTileType(t, MP_UNMOVABLE));
	return _m[t].m5;
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


static inline void EnlargeCompanyHQ(TileIndex t, byte size)
{
	if (size <= _m[t].m5 - UNMOVABLE_HQ_NORTH) return;

	_m[t + TileDiffXY(0, 0)].m5 = UNMOVABLE_HQ_NORTH + size * 4;
	_m[t + TileDiffXY(0, 1)].m5 = UNMOVABLE_HQ_WEST  + size * 4;
	_m[t + TileDiffXY(1, 0)].m5 = UNMOVABLE_HQ_EAST  + size * 4;
	_m[t + TileDiffXY(1, 1)].m5 = UNMOVABLE_HQ_SOUTH + size * 4;
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

static inline void MakeStatue(TileIndex t, Owner o)
{
	MakeUnmovable(t, UNMOVABLE_STATUE, o);
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

