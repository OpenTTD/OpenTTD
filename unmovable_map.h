/* $Id$ */

typedef enum UnmovableType {
	UNMOVABLE_TRANSMITTER = 0,
	UNMOVABLE_LIGHTHOUSE  = 1,
	UNMOVABLE_STATUE      = 2,
	UNMOVABLE_OWNED_LAND  = 3
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
