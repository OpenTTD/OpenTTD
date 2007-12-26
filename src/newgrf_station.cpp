/* $Id$ */

/** @file newgrf_station.cpp Functions for dealing with station classes and custom stations. */

#include "stdafx.h"
#include "openttd.h"
#include "variables.h"
#include "tile_cmd.h"
#include "landscape.h"
#include "debug.h"
#include "sprite.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "station.h"
#include "station_map.h"
#include "newgrf.h"
#include "newgrf_callbacks.h"
#include "newgrf_commons.h"
#include "newgrf_station.h"
#include "newgrf_spritegroup.h"
#include "cargotype.h"
#include "town_map.h"
#include "newgrf_town.h"
#include "gfx_func.h"
#include "date_func.h"

static StationClass station_classes[STAT_CLASS_MAX];

enum {
	MAX_SPECLIST = 255,
};

/**
 * Reset station classes to their default state.
 * This includes initialising the Default and Waypoint classes with an empty
 * entry, for standard stations and waypoints.
 */
void ResetStationClasses()
{
	for (StationClassID i = STAT_CLASS_BEGIN; i < STAT_CLASS_MAX; i++) {
		station_classes[i].id = 0;
		station_classes[i].name = STR_EMPTY;
		station_classes[i].stations = 0;

		free(station_classes[i].spec);
		station_classes[i].spec = NULL;
	}

	/* Set up initial data */
	station_classes[0].id = 'DFLT';
	station_classes[0].name = STR_STAT_CLASS_DFLT;
	station_classes[0].stations = 1;
	station_classes[0].spec = MallocT<StationSpec*>(1);
	station_classes[0].spec[0] = NULL;

	station_classes[1].id = 'WAYP';
	station_classes[1].name = STR_STAT_CLASS_WAYP;
	station_classes[1].stations = 1;
	station_classes[1].spec = MallocT<StationSpec*>(1);
	station_classes[1].spec[0] = NULL;
}

/**
 * Allocate a station class for the given class id.
 * @param cls A 32 bit value identifying the class.
 * @return Index into station_classes of allocated class.
 */
StationClassID AllocateStationClass(uint32 cls)
{
	for (StationClassID i = STAT_CLASS_BEGIN; i < STAT_CLASS_MAX; i++) {
		if (station_classes[i].id == cls) {
			/* ClassID is already allocated, so reuse it. */
			return i;
		} else if (station_classes[i].id == 0) {
			/* This class is empty, so allocate it to the ClassID. */
			station_classes[i].id = cls;
			return i;
		}
	}

	grfmsg(2, "StationClassAllocate: already allocated %d classes, using default", STAT_CLASS_MAX);
	return STAT_CLASS_DFLT;
}

/** Set the name of a custom station class */
void SetStationClassName(StationClassID sclass, StringID name)
{
	assert(sclass < STAT_CLASS_MAX);
	station_classes[sclass].name = name;
}

/** Retrieve the name of a custom station class */
StringID GetStationClassName(StationClassID sclass)
{
	assert(sclass < STAT_CLASS_MAX);
	return station_classes[sclass].name;
}

/** Build a list of station class name StringIDs to use in a dropdown list
 * @return Pointer to a (static) array of StringIDs
 */
StringID *BuildStationClassDropdown()
{
	/* Allow room for all station classes, plus a terminator entry */
	static StringID names[STAT_CLASS_MAX + 1];
	uint i;

	/* Add each name */
	for (i = 0; i < STAT_CLASS_MAX && station_classes[i].id != 0; i++) {
		names[i] = station_classes[i].name;
	}
	/* Terminate the list */
	names[i] = INVALID_STRING_ID;

	return names;
}

/**
 * Get the number of station classes in use.
 * @return Number of station classes.
 */
uint GetNumStationClasses()
{
	uint i;
	for (i = 0; i < STAT_CLASS_MAX && station_classes[i].id != 0; i++);
	return i;
}

/**
 * Return the number of stations for the given station class.
 * @param sclass Index of the station class.
 * @return Number of stations in the class.
 */
uint GetNumCustomStations(StationClassID sclass)
{
	assert(sclass < STAT_CLASS_MAX);
	return station_classes[sclass].stations;
}

/**
 * Tie a station spec to its station class.
 * @param statspec The station spec.
 */
void SetCustomStationSpec(StationSpec *statspec)
{
	StationClass *station_class;
	int i;

	/* If the station has already been allocated, don't reallocate it. */
	if (statspec->allocated) return;

	assert(statspec->sclass < STAT_CLASS_MAX);
	station_class = &station_classes[statspec->sclass];

	i = station_class->stations++;
	station_class->spec = ReallocT(station_class->spec, station_class->stations);

	station_class->spec[i] = statspec;
	statspec->allocated = true;
}

/**
 * Retrieve a station spec from a class.
 * @param sclass Index of the station class.
 * @param station The station index with the class.
 * @return The station spec.
 */
const StationSpec *GetCustomStationSpec(StationClassID sclass, uint station)
{
	assert(sclass < STAT_CLASS_MAX);
	if (station < station_classes[sclass].stations)
		return station_classes[sclass].spec[station];

	/* If the custom station isn't defined any more, then the GRF file
	 * probably was not loaded. */
	return NULL;
}


const StationSpec *GetCustomStationSpecByGrf(uint32 grfid, byte localidx)
{
	uint j;

	for (StationClassID i = STAT_CLASS_BEGIN; i < STAT_CLASS_MAX; i++) {
		for (j = 0; j < station_classes[i].stations; j++) {
			const StationSpec *statspec = station_classes[i].spec[j];
			if (statspec == NULL) continue;
			if (statspec->grffile->grfid == grfid && statspec->localidx == localidx) return statspec;
		}
	}

	return NULL;
}


/* Evaluate a tile's position within a station, and return the result a bitstuffed format.
 * if not centred: .TNLcCpP, if centred: .TNL..CP
 * T = Tile layout number (GetStationGfx), N = Number of platforms, L = Length of platforms
 * C = Current platform number from start, c = from end
 * P = Position along platform from start, p = from end
 * if centred, C/P start from the centre and c/p are not available.
 */
uint32 GetPlatformInfo(Axis axis, byte tile, int platforms, int length, int x, int y, bool centred)
{
	uint32 retval = 0;

	if (axis == AXIS_X) {
		Swap(platforms, length);
		Swap(x, y);
	}

	/* Limit our sizes to 4 bits */
	platforms = min(15, platforms);
	length    = min(15, length);
	x = min(15, x);
	y = min(15, y);
	if (centred) {
		x -= platforms / 2;
		y -= length / 2;
		SB(retval,  0, 4, y & 0xF);
		SB(retval,  4, 4, x & 0xF);
	} else {
		SB(retval,  0, 4, y);
		SB(retval,  4, 4, length - y - 1);
		SB(retval,  8, 4, x);
		SB(retval, 12, 4, platforms - x - 1);
	}
	SB(retval, 16, 4, length);
	SB(retval, 20, 4, platforms);
	SB(retval, 24, 4, tile);

	return retval;
}


/* Find the end of a railway station, from the tile, in the direction of delta.
 * If check_type is set, we stop if the custom station type changes.
 * If check_axis is set, we stop if the station direction changes.
 */
static TileIndex FindRailStationEnd(TileIndex tile, TileIndexDiff delta, bool check_type, bool check_axis)
{
	bool waypoint;
	byte orig_type = 0;
	Axis orig_axis = AXIS_X;

	waypoint = IsTileType(tile, MP_RAILWAY);

	if (waypoint) {
		if (check_axis) orig_axis = GetWaypointAxis(tile);
	} else {
		if (check_type) orig_type = GetCustomStationSpecIndex(tile);
		if (check_axis) orig_axis = GetRailStationAxis(tile);
	}

	while (true) {
		TileIndex new_tile = TILE_ADD(tile, delta);

		if (waypoint) {
			if (!IsTileType(new_tile, MP_RAILWAY)) break;
			if (!IsRailWaypoint(new_tile)) break;
			if (check_axis && GetWaypointAxis(new_tile) != orig_axis) break;
		} else {
			if (!IsRailwayStationTile(new_tile)) break;
			if (check_type && GetCustomStationSpecIndex(new_tile) != orig_type) break;
			if (check_axis && GetRailStationAxis(new_tile) != orig_axis) break;
		}

		tile = new_tile;
	}
	return tile;
}


static uint32 GetPlatformInfoHelper(TileIndex tile, bool check_type, bool check_axis, bool centred)
{
	int tx = TileX(tile);
	int ty = TileY(tile);
	int sx = TileX(FindRailStationEnd(tile, TileDiffXY(-1,  0), check_type, check_axis));
	int sy = TileY(FindRailStationEnd(tile, TileDiffXY( 0, -1), check_type, check_axis));
	int ex = TileX(FindRailStationEnd(tile, TileDiffXY( 1,  0), check_type, check_axis)) + 1;
	int ey = TileY(FindRailStationEnd(tile, TileDiffXY( 0,  1), check_type, check_axis)) + 1;
	Axis axis = IsTileType(tile, MP_RAILWAY) ? GetWaypointAxis(tile) : GetRailStationAxis(tile);

	tx -= sx; ex -= sx;
	ty -= sy; ey -= sy;

	return GetPlatformInfo(axis, IsTileType(tile, MP_RAILWAY) ? 2 : GetStationGfx(tile), ex, ey, tx, ty, centred);
}


static uint32 GetRailContinuationInfo(TileIndex tile)
{
	/* Tile offsets and exit dirs for X axis */
	static const Direction x_dir[8] = { DIR_SW, DIR_NE, DIR_SE, DIR_NW, DIR_S, DIR_E, DIR_W, DIR_N };
	static const DiagDirection x_exits[8] = { DIAGDIR_SW, DIAGDIR_NE, DIAGDIR_SE, DIAGDIR_NW, DIAGDIR_SW, DIAGDIR_NE, DIAGDIR_SW, DIAGDIR_NE };

	/* Tile offsets and exit dirs for Y axis */
	static const Direction y_dir[8] = { DIR_SE, DIR_NW, DIR_SW, DIR_NE, DIR_S, DIR_W, DIR_E, DIR_N };
	static const DiagDirection y_exits[8] = { DIAGDIR_SE, DIAGDIR_NW, DIAGDIR_SW, DIAGDIR_NE, DIAGDIR_SE, DIAGDIR_NW, DIAGDIR_SE, DIAGDIR_NW };

	Axis axis = IsTileType(tile, MP_RAILWAY) ? GetWaypointAxis(tile) : GetRailStationAxis(tile);

	/* Choose appropriate lookup table to use */
	const Direction *dir = axis == AXIS_X ? x_dir : y_dir;
	const DiagDirection *diagdir = axis == AXIS_X ? x_exits : y_exits;

	uint32 res = 0;
	uint i;

	for (i = 0; i < lengthof(x_dir); i++, dir++, diagdir++) {
		uint32 ts = GetTileTrackStatus(tile + TileOffsByDir(*dir), TRANSPORT_RAIL, 0);
		if (ts != 0) {
			/* If there is any track on the tile, set the bit in the second byte */
			SetBit(res, i + 8);

			/* If any track reaches our exit direction, set the bit in the lower byte */
			if (ts & DiagdirReachesTracks(*diagdir)) SetBit(res, i);
		}
	}

	return res;
}


/* Station Resolver Functions */
static uint32 StationGetRandomBits(const ResolverObject *object)
{
	const Station *st = object->u.station.st;
	const TileIndex tile = object->u.station.tile;
	return (st == NULL ? 0 : st->random_bits) | (tile == INVALID_TILE ? 0 : GetStationTileRandomBits(tile) << 16);
}


static uint32 StationGetTriggers(const ResolverObject *object)
{
	const Station *st = object->u.station.st;
	return st == NULL ? 0 : st->waiting_triggers;
}


static void StationSetTriggers(const ResolverObject *object, int triggers)
{
	Station *st = (Station*)object->u.station.st;
	assert(st != NULL);
	st->waiting_triggers = triggers;
}

/**
 * Station variable cache
 * This caches 'expensive' station variable lookups which iterate over
 * several tiles that may be called multiple times per Resolve().
 */
static struct {
	uint32 v40;
	uint32 v41;
	uint32 v45;
	uint32 v46;
	uint32 v47;
	uint32 v49;
	uint8 valid;
} _svc;

static uint32 StationGetVariable(const ResolverObject *object, byte variable, byte parameter, bool *available)
{
	const Station *st = object->u.station.st;
	TileIndex tile = object->u.station.tile;

	if (object->scope == VSG_SCOPE_PARENT) {
		/* Pass the request on to the town of the station */
		Town *t;

		if (st != NULL) {
			t = st->town;
		} else if (tile != INVALID_TILE) {
			t = GetTownByTile(tile);
		} else {
			*available = false;
			return UINT_MAX;
		}

		return TownGetVariable(variable, parameter, available, t);
	}

	if (st == NULL) {
		/* Station does not exist, so we're in a purchase list */
		switch (variable) {
			case 0x40:
			case 0x41:
			case 0x46:
			case 0x47:
			case 0x49: return 0x2110000;       // Platforms, tracks & position
			case 0x42: return 0;               // Rail type (XXX Get current type from GUI?)
			case 0x43: return _current_player; // Station owner
			case 0x44: return 2;               // PBS status
			case 0xFA: return Clamp(_date - DAYS_TILL_ORIGINAL_BASE_YEAR, 0, 65535); // Build date, clamped to a 16 bit value
		}

		*available = false;
		return UINT_MAX;
	}

	switch (variable) {
		/* Calculated station variables */
		case 0x40:
			if (!HasBit(_svc.valid, 0)) { _svc.v40 = GetPlatformInfoHelper(tile, false, false, false); SetBit(_svc.valid, 0); }
			return _svc.v40;

		case 0x41:
			if (!HasBit(_svc.valid, 1)) { _svc.v41 = GetPlatformInfoHelper(tile, true,  false, false); SetBit(_svc.valid, 1); }
			return _svc.v41;

		case 0x42: return GetTerrainType(tile) | (GetRailType(tile) << 8);
		case 0x43: return st->owner; // Station owner
		case 0x44: return 2;         // PBS status
		case 0x45:
			if (!HasBit(_svc.valid, 2)) { _svc.v45 = GetRailContinuationInfo(tile); SetBit(_svc.valid, 2); }
			return _svc.v45;

		case 0x46:
			if (!HasBit(_svc.valid, 3)) { _svc.v46 = GetPlatformInfoHelper(tile, false, false, true); SetBit(_svc.valid, 3); }
			return _svc.v46;

		case 0x47:
			if (!HasBit(_svc.valid, 4)) { _svc.v47 = GetPlatformInfoHelper(tile, true,  false, true); SetBit(_svc.valid, 4); }
			return _svc.v47;

		case 0x48: { // Accepted cargo types
			CargoID cargo_type;
			uint32 value = 0;

			for (cargo_type = 0; cargo_type < NUM_CARGO; cargo_type++) {
				if (HasBit(st->goods[cargo_type].acceptance_pickup, GoodsEntry::PICKUP)) SetBit(value, cargo_type);
			}
			return value;
		}
		case 0x49:
			if (!HasBit(_svc.valid, 5)) { _svc.v49 = GetPlatformInfoHelper(tile, false, true, false); SetBit(_svc.valid, 5); }
			return _svc.v49;

		/* Variables which use the parameter */
		/* Variables 0x60 to 0x65 are handled separately below */
		case 0x67: { // Land info of nearby tiles
			Axis axis = GetRailStationAxis(tile);

			if (parameter != 0) tile = GetNearbyTile(parameter, tile); // only perform if it is required
			byte tile_type = GetTerrainType(tile) << 2 | (IsTileType(tile, MP_WATER) ? 1 : 0) << 1;

			uint z;
			Slope tileh = GetTileSlope(tile, &z);
			bool swap = (axis == AXIS_Y && HasBit(tileh, 0) != HasBit(tileh, 2));
			return GetTileType(tile) << 24 | z << 16 | tile_type << 8 | (tileh ^ (swap ? 5 : 0));
		}

		case 0x68: { // Station info of nearby tiles
			TileIndex nearby_tile = GetNearbyTile(parameter, tile);

			if (!IsRailwayStationTile(nearby_tile)) return 0xFFFFFFFF;

			uint32 grfid = st->speclist[GetCustomStationSpecIndex(tile)].grfid;
			bool perpendicular = GetRailStationAxis(tile) != GetRailStationAxis(nearby_tile);
			bool same_station = st->TileBelongsToRailStation(nearby_tile);
			uint32 res = GB(GetStationGfx(nearby_tile), 1, 2) << 12 | !!perpendicular << 11 | !!same_station << 10;

			if (IsCustomStationSpecIndex(nearby_tile)) {
				const StationSpecList ssl = GetStationByTile(nearby_tile)->speclist[GetCustomStationSpecIndex(nearby_tile)];
				res |= 1 << (ssl.grfid != grfid ? 9 : 8) | ssl.localidx;
			}
			return res;
		}

		/* General station properties */
		case 0x82: return 50;
		case 0x84: return st->string_id;
		case 0x86: return 0;
		case 0x8A: return st->had_vehicle_of_type;
		case 0xF0: return st->facilities;
		case 0xF1: return st->airport_type;
		case 0xF2: return st->truck_stops->status;
		case 0xF3: return st->bus_stops->status;
		case 0xF6: return st->airport_flags;
		case 0xF7: return GB(st->airport_flags, 8, 8);
		case 0xFA: return Clamp(st->build_date - DAYS_TILL_ORIGINAL_BASE_YEAR, 0, 65535);
	}

	/* Handle cargo variables with parameter, 0x60 to 0x65 */
	if (variable >= 0x60 && variable <= 0x65) {
		CargoID c = GetCargoTranslation(parameter, object->u.station.statspec->grffile);

		if (c == CT_INVALID) return 0;
		const GoodsEntry *ge = &st->goods[c];

		switch (variable) {
			case 0x60: return min(ge->cargo.Count(), 4095);
			case 0x61: return ge->days_since_pickup;
			case 0x62: return ge->rating;
			case 0x63: return ge->cargo.DaysInTransit();
			case 0x64: return ge->last_speed | (ge->last_age << 8);
			case 0x65: return GB(ge->acceptance_pickup, GoodsEntry::ACCEPTANCE, 1) << 3;
		}
	}

	/* Handle cargo variables (deprecated) */
	if (variable >= 0x8C && variable <= 0xEC) {
		const GoodsEntry *g = &st->goods[GB(variable - 0x8C, 3, 4)];
		switch (GB(variable - 0x8C, 0, 3)) {
			case 0: return g->cargo.Count();
			case 1: return GB(min(g->cargo.Count(), 4095), 0, 4) | (GB(g->acceptance_pickup, GoodsEntry::ACCEPTANCE, 1) << 7);
			case 2: return g->days_since_pickup;
			case 3: return g->rating;
			case 4: return g->cargo.Source();
			case 5: return g->cargo.DaysInTransit();
			case 6: return g->last_speed;
			case 7: return g->last_age;
		}
	}

	DEBUG(grf, 1, "Unhandled station property 0x%X", variable);

	*available = false;
	return UINT_MAX;
}


static const SpriteGroup *StationResolveReal(const ResolverObject *object, const SpriteGroup *group)
{
	const Station *st = object->u.station.st;
	const StationSpec *statspec = object->u.station.statspec;
	uint set;

	uint cargo = 0;
	CargoID cargo_type = object->u.station.cargo_type;

	if (st == NULL || statspec->sclass == STAT_CLASS_WAYP) {
		return group->g.real.loading[0];
	}

	switch (cargo_type) {
		case CT_INVALID:
		case CT_DEFAULT_NA:
		case CT_PURCHASE:
			cargo = 0;
			break;

		case CT_DEFAULT:
			for (cargo_type = 0; cargo_type < NUM_CARGO; cargo_type++) {
				cargo += st->goods[cargo_type].cargo.Count();
			}
			break;

		default:
			cargo = st->goods[cargo_type].cargo.Count();
			break;
	}

	if (HasBit(statspec->flags, 1)) cargo /= (st->trainst_w + st->trainst_h);
	cargo = min(0xfff, cargo);

	if (cargo > statspec->cargo_threshold) {
		if (group->g.real.num_loading > 0) {
			set = ((cargo - statspec->cargo_threshold) * group->g.real.num_loading) / (4096 - statspec->cargo_threshold);
			return group->g.real.loading[set];
		}
	} else {
		if (group->g.real.num_loaded > 0) {
			set = (cargo * group->g.real.num_loaded) / (statspec->cargo_threshold + 1);
			return group->g.real.loaded[set];
		}
	}

	return group->g.real.loading[0];
}


static void NewStationResolver(ResolverObject *res, const StationSpec *statspec, const Station *st, TileIndex tile)
{
	res->GetRandomBits = StationGetRandomBits;
	res->GetTriggers   = StationGetTriggers;
	res->SetTriggers   = StationSetTriggers;
	res->GetVariable   = StationGetVariable;
	res->ResolveReal   = StationResolveReal;

	res->u.station.st       = st;
	res->u.station.statspec = statspec;
	res->u.station.tile     = tile;

	res->callback        = CBID_NO_CALLBACK;
	res->callback_param1 = 0;
	res->callback_param2 = 0;
	res->last_value      = 0;
	res->trigger         = 0;
	res->reseed          = 0;
}

static const SpriteGroup *ResolveStation(ResolverObject *object)
{
	const SpriteGroup *group;
	CargoID ctype = CT_DEFAULT_NA;

	if (object->u.station.st == NULL) {
		/* No station, so we are in a purchase list */
		ctype = CT_PURCHASE;
	} else {
		/* Pick the first cargo that we have waiting */
		for (CargoID cargo = 0; cargo < NUM_CARGO; cargo++) {
			const CargoSpec *cs = GetCargo(cargo);
			if (cs->IsValid() && object->u.station.statspec->spritegroup[cargo] != NULL &&
					!object->u.station.st->goods[cargo].cargo.Empty()) {
				ctype = cargo;
				break;
			}
		}
	}

	group = object->u.station.statspec->spritegroup[ctype];
	if (group == NULL) {
		ctype = CT_DEFAULT;
		group = object->u.station.statspec->spritegroup[ctype];
	}

	if (group == NULL) return NULL;

	/* Remember the cargo type we've picked */
	object->u.station.cargo_type = ctype;

	/* Invalidate all cached vars */
	_svc.valid = 0;

	return Resolve(group, object);
}

SpriteID GetCustomStationRelocation(const StationSpec *statspec, const Station *st, TileIndex tile)
{
	const SpriteGroup *group;
	ResolverObject object;

	NewStationResolver(&object, statspec, st, tile);

	group = ResolveStation(&object);
	if (group == NULL || group->type != SGT_RESULT) return 0;
	return group->g.result.sprite - 0x42D;
}


SpriteID GetCustomStationGroundRelocation(const StationSpec *statspec, const Station *st, TileIndex tile)
{
	const SpriteGroup *group;
	ResolverObject object;

	NewStationResolver(&object, statspec, st, tile);
	object.callback_param1 = 1; // Indicate we are resolving the ground sprite

	group = ResolveStation(&object);
	if (group == NULL || group->type != SGT_RESULT) return 0;
	return group->g.result.sprite - 0x42D;
}


uint16 GetStationCallback(CallbackID callback, uint32 param1, uint32 param2, const StationSpec *statspec, const Station *st, TileIndex tile)
{
	const SpriteGroup *group;
	ResolverObject object;

	NewStationResolver(&object, statspec, st, tile);

	object.callback        = callback;
	object.callback_param1 = param1;
	object.callback_param2 = param2;

	group = ResolveStation(&object);
	if (group == NULL || group->type != SGT_CALLBACK) return CALLBACK_FAILED;
	return group->g.callback.result;
}


/**
 * Allocate a StationSpec to a Station. This is called once per build operation.
 * @param statspec StationSpec to allocate.
 * @param st Station to allocate it to.
 * @param exec Whether to actually allocate the spec.
 * @return Index within the Station's spec list, or -1 if the allocation failed.
 */
int AllocateSpecToStation(const StationSpec *statspec, Station *st, bool exec)
{
	uint i;

	if (statspec == NULL) return 0;

	/* Check if this spec has already been allocated */
	for (i = 1; i < st->num_specs && i < MAX_SPECLIST; i++) {
		if (st->speclist[i].spec == statspec) return i;
	}

	for (i = 1; i < st->num_specs && i < MAX_SPECLIST; i++) {
		if (st->speclist[i].spec == NULL && st->speclist[i].grfid == 0) break;
	}

	if (i == MAX_SPECLIST) return -1;

	if (exec) {
		if (i >= st->num_specs) {
			st->num_specs = i + 1;
			st->speclist = ReallocT(st->speclist, st->num_specs);

			if (st->num_specs == 2) {
				/* Initial allocation */
				st->speclist[0].spec     = NULL;
				st->speclist[0].grfid    = 0;
				st->speclist[0].localidx = 0;
			}
		}

		st->speclist[i].spec     = statspec;
		st->speclist[i].grfid    = statspec->grffile->grfid;
		st->speclist[i].localidx = statspec->localidx;
	}

	return i;
}


/** Deallocate a StationSpec from a Station. Called when removing a single station tile.
 * @param st Station to work with.
 * @param specindex Index of the custom station within the Station's spec list.
 * @return Indicates whether the StationSpec was deallocated.
 */
void DeallocateSpecFromStation(Station* st, byte specindex)
{
	/* specindex of 0 (default) is never freeable */
	if (specindex == 0) return;

	/* Check all tiles over the station to check if the specindex is still in use */
	BEGIN_TILE_LOOP(tile, st->trainst_w, st->trainst_h, st->train_tile) {
		if (IsTileType(tile, MP_STATION) && GetStationIndex(tile) == st->index && IsRailwayStation(tile) && GetCustomStationSpecIndex(tile) == specindex) {
			return;
		}
	} END_TILE_LOOP(tile, st->trainst_w, st->trainst_h, st->train_tile)

	/* This specindex is no longer in use, so deallocate it */
	st->speclist[specindex].spec     = NULL;
	st->speclist[specindex].grfid    = 0;
	st->speclist[specindex].localidx = 0;

	/* If this was the highest spec index, reallocate */
	if (specindex == st->num_specs - 1) {
		for (; st->speclist[st->num_specs - 1].grfid == 0 && st->num_specs > 1; st->num_specs--);

		if (st->num_specs > 1) {
			st->speclist = ReallocT(st->speclist, st->num_specs);
		} else {
			free(st->speclist);
			st->num_specs = 0;
			st->speclist  = NULL;
		}
	}
}

/** Draw representation of a station tile for GUI purposes.
 * @param x Position x of image.
 * @param y Position y of image.
 * @param axis Axis.
 * @param railtype Rail type.
 * @param sclass, station Type of station.
 * @param station station ID
 * @return True if the tile was drawn (allows for fallback to default graphic)
 */
bool DrawStationTile(int x, int y, RailType railtype, Axis axis, StationClassID sclass, uint station)
{
	const StationSpec *statspec;
	const DrawTileSprites *sprites;
	const DrawTileSeqStruct *seq;
	const RailtypeInfo *rti = GetRailTypeInfo(railtype);
	SpriteID relocation;
	SpriteID image;
	SpriteID pal = PLAYER_SPRITE_COLOR(_local_player);
	uint tile = 2;

	statspec = GetCustomStationSpec(sclass, station);
	if (statspec == NULL) return false;

	relocation = GetCustomStationRelocation(statspec, NULL, INVALID_TILE);

	if (HasBit(statspec->callbackmask, CBM_STATION_SPRITE_LAYOUT)) {
		uint16 callback = GetStationCallback(CBID_STATION_SPRITE_LAYOUT, 0x2110000, 0, statspec, NULL, INVALID_TILE);
		if (callback != CALLBACK_FAILED) tile = callback;
	}

	if (statspec->renderdata == NULL) {
		sprites = GetStationTileLayout(STATION_RAIL, tile + axis);
	} else {
		sprites = &statspec->renderdata[(tile < statspec->tiles) ? tile + axis : (uint)axis];
	}

	image = sprites->ground_sprite;
	if (HasBit(image, SPRITE_MODIFIER_USE_OFFSET)) {
		image += GetCustomStationGroundRelocation(statspec, NULL, INVALID_TILE);
		image += rti->custom_ground_offset;
	} else {
		image += rti->total_offset;
	}

	DrawSprite(image, PAL_NONE, x, y);

	foreach_draw_tile_seq(seq, sprites->seq) {
		Point pt;
		image = seq->image;
		if (HasBit(image, SPRITE_MODIFIER_USE_OFFSET)) {
			image += rti->total_offset;
		} else {
			image += relocation;
		}

		if ((byte)seq->delta_z != 0x80) {
			pt = RemapCoords(seq->delta_x, seq->delta_y, seq->delta_z);
			DrawSprite(image, pal, x + pt.x, y + pt.y);
		}
	}

	return true;
}


static const StationSpec* GetStationSpec(TileIndex t)
{
	const Station* st;
	uint specindex;

	if (!IsCustomStationSpecIndex(t)) return NULL;

	st = GetStationByTile(t);
	specindex = GetCustomStationSpecIndex(t);
	return specindex < st->num_specs ? st->speclist[specindex].spec : NULL;
}


/* Check if a rail station tile is traversable.
 * XXX This could be cached (during build) in the map array to save on all the dereferencing */
bool IsStationTileBlocked(TileIndex tile)
{
	const StationSpec* statspec = GetStationSpec(tile);

	return statspec != NULL && HasBit(statspec->blocked, GetStationGfx(tile));
}

/* Check if a rail station tile is electrifiable.
 * XXX This could be cached (during build) in the map array to save on all the dereferencing */
bool IsStationTileElectrifiable(TileIndex tile)
{
	const StationSpec* statspec = GetStationSpec(tile);

	return
		statspec == NULL ||
		HasBit(statspec->pylons, GetStationGfx(tile)) ||
		!HasBit(statspec->wires, GetStationGfx(tile));
}
