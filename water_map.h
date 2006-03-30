/* $Id$ */

#ifndef WATER_MAP_H
#define WATER_MAP_H

typedef enum DepotPart {
	DEPOT_NORTH = 0x80,
	DEPOT_SOUTH = 0x81
} DepotPart;

typedef enum LockPart {
	LOCK_MIDDLE = 0x10,
	LOCK_LOWER  = 0x14,
	LOCK_UPPER  = 0x18
} LockPart;

static inline void MakeWater(TileIndex t)
{
	SetTileType(t, MP_WATER);
	SetTileOwner(t, OWNER_WATER);
	_m[t].m2 = 0;
	_m[t].m3 = 0;
	_m[t].m4 = 0;
	_m[t].m5 = 0;
}


static inline void MakeShore(TileIndex t)
{
	SetTileType(t, MP_WATER);
	SetTileOwner(t, OWNER_WATER);
	_m[t].m2 = 0;
	_m[t].m3 = 0;
	_m[t].m4 = 0;
	_m[t].m5 = 1;
}

static inline void MakeShipDepot(TileIndex t, Owner o, DepotPart base, Axis a)
{
	SetTileType(t, MP_WATER);
	SetTileOwner(t, o);
	_m[t].m2 = 0;
	_m[t].m3 = 0;
	_m[t].m4 = 0;
	_m[t].m5 = base + a * 2;
}

static inline void MakeLockTile(TileIndex t, byte section)
{
	SetTileType(t, MP_WATER);
	SetTileOwner(t, OWNER_WATER);
	_m[t].m2 = 0;
	_m[t].m3 = 0;
	_m[t].m4 = 0;
	_m[t].m5 = section;
}

static inline void MakeLock(TileIndex t, DiagDirection d)
{
	TileIndexDiff delta = TileOffsByDir(d);

	MakeLockTile(t, LOCK_MIDDLE + d);
	MakeLockTile(t - delta, LOCK_LOWER + d);
	MakeLockTile(t + delta, LOCK_UPPER + d);
}

#endif
