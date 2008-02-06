/* $Id$ */

/** @file water_map.h */

#ifndef WATER_MAP_H
#define WATER_MAP_H

enum WaterTileType {
	WATER_TILE_CLEAR,
	WATER_TILE_COAST,
	WATER_TILE_LOCK,
	WATER_TILE_DEPOT,
};

enum WaterClass {
	WATER_CLASS_SEA,
	WATER_CLASS_CANAL,
	WATER_CLASS_RIVER,
};

enum DepotPart {
	DEPOT_NORTH = 0x80,
	DEPOT_SOUTH = 0x81,
	DEPOT_END   = 0x84,
};

enum LockPart {
	LOCK_MIDDLE = 0x10,
	LOCK_LOWER  = 0x14,
	LOCK_UPPER  = 0x18,
	LOCK_END    = 0x1C
};

static inline WaterTileType GetWaterTileType(TileIndex t)
{
	assert(IsTileType(t, MP_WATER));

	if (_m[t].m5 == 0) return WATER_TILE_CLEAR;
	if (_m[t].m5 == 1) return WATER_TILE_COAST;
	if (IsInsideMM(_m[t].m5, LOCK_MIDDLE, LOCK_END)) return WATER_TILE_LOCK;

	assert(IsInsideMM(_m[t].m5, DEPOT_NORTH, DEPOT_END));
	return WATER_TILE_DEPOT;
}

static inline WaterClass GetWaterClass(TileIndex t)
{
	assert(IsTileType(t, MP_WATER) || IsTileType(t, MP_STATION));
	return (WaterClass)GB(_m[t].m3, 0, 2);
}

static inline void SetWaterClass(TileIndex t, WaterClass wc)
{
	assert(IsTileType(t, MP_WATER) || IsTileType(t, MP_STATION));
	SB(_m[t].m3, 0, 2, wc);
}

/** IsWater return true if any type of clear water like ocean, river, canal */
static inline bool IsWater(TileIndex t)
{
	return GetWaterTileType(t) == WATER_TILE_CLEAR;
}

static inline bool IsSea(TileIndex t)
{
	return IsWater(t) && GetWaterClass(t) == WATER_CLASS_SEA;
}

static inline bool IsCanal(TileIndex t)
{
	return IsWater(t) && GetWaterClass(t) == WATER_CLASS_CANAL;
}

static inline bool IsRiver(TileIndex t)
{
	return IsWater(t) && GetWaterClass(t) == WATER_CLASS_RIVER;
}

static inline bool IsWaterTile(TileIndex t)
{
	return IsTileType(t, MP_WATER) && IsWater(t);
}

static inline bool IsCoast(TileIndex t)
{
	return GetWaterTileType(t) == WATER_TILE_COAST;
}

static inline TileIndex GetOtherShipDepotTile(TileIndex t)
{
	return t + (HasBit(_m[t].m5, 0) ? -1 : 1) * (HasBit(_m[t].m5, 1) ? TileDiffXY(0, 1) : TileDiffXY(1, 0));
}

static inline TileIndex IsShipDepot(TileIndex t)
{
	return IsInsideMM(_m[t].m5, DEPOT_NORTH, DEPOT_END);
}

static inline Axis GetShipDepotAxis(TileIndex t)
{
	return (Axis)GB(_m[t].m5, 1, 1);
}

static inline DiagDirection GetShipDepotDirection(TileIndex t)
{
	return XYNSToDiagDir(GetShipDepotAxis(t), GB(_m[t].m5, 0, 1));
}

static inline bool IsLock(TileIndex t)
{
	return IsInsideMM(_m[t].m5, LOCK_MIDDLE, LOCK_END);
}

static inline DiagDirection GetLockDirection(TileIndex t)
{
	return (DiagDirection)GB(_m[t].m5, 0, 2);
}

static inline byte GetSection(TileIndex t)
{
	assert(GetWaterTileType(t) == WATER_TILE_LOCK || GetWaterTileType(t) == WATER_TILE_DEPOT);
	return GB(_m[t].m5, 0, 4);
}

static inline byte GetWaterTileRandomBits(TileIndex t)
{
	return _m[t].m4;
}


static inline void MakeWater(TileIndex t)
{
	SetTileType(t, MP_WATER);
	SetTileOwner(t, OWNER_WATER);
	_m[t].m2 = 0;
	_m[t].m3 = WATER_CLASS_SEA;
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

static inline void MakeRiver(TileIndex t, uint8 random_bits)
{
	SetTileType(t, MP_WATER);
	SetTileOwner(t, OWNER_WATER);
	_m[t].m2 = 0;
	_m[t].m3 = WATER_CLASS_RIVER;
	_m[t].m4 = random_bits;
	_m[t].m5 = 0;
}

static inline void MakeCanal(TileIndex t, Owner o, uint8 random_bits)
{
	assert(o != OWNER_WATER);
	SetTileType(t, MP_WATER);
	SetTileOwner(t, o);
	_m[t].m2 = 0;
	_m[t].m3 = WATER_CLASS_CANAL;
	_m[t].m4 = random_bits;
	_m[t].m5 = 0;
}

static inline void MakeShipDepot(TileIndex t, Owner o, DepotPart base, Axis a, WaterClass original_water_class)
{
	SetTileType(t, MP_WATER);
	SetTileOwner(t, o);
	_m[t].m2 = 0;
	_m[t].m3 = original_water_class;
	_m[t].m4 = 0;
	_m[t].m5 = base + a * 2;
}

static inline void MakeLockTile(TileIndex t, Owner o, byte section, WaterClass original_water_class)
{
	SetTileType(t, MP_WATER);
	SetTileOwner(t, o);
	_m[t].m2 = 0;
	_m[t].m3 = original_water_class;
	_m[t].m4 = 0;
	_m[t].m5 = section;
}

static inline void MakeLock(TileIndex t, Owner o, DiagDirection d, WaterClass wc_lower, WaterClass wc_upper)
{
	TileIndexDiff delta = TileOffsByDiagDir(d);

	MakeLockTile(t, o, LOCK_MIDDLE + d, WATER_CLASS_CANAL);
	MakeLockTile(t - delta, o, LOCK_LOWER + d, wc_lower);
	MakeLockTile(t + delta, o, LOCK_UPPER + d, wc_upper);
}

#endif /* WATER_MAP_H */
