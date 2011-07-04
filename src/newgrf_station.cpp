/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_station.cpp Functions for dealing with station classes and custom stations. */

#include "stdafx.h"
#include "debug.h"
#include "station_base.h"
#include "waypoint_base.h"
#include "roadstop_base.h"
#include "newgrf_cargo.h"
#include "newgrf_station.h"
#include "newgrf_spritegroup.h"
#include "newgrf_sound.h"
#include "newgrf_railtype.h"
#include "town.h"
#include "newgrf_town.h"
#include "company_func.h"
#include "tunnelbridge_map.h"
#include "newgrf.h"
#include "newgrf_animation_base.h"
#include "newgrf_class_func.h"


template <typename Tspec, typename Tid, Tid Tmax>
/* static */ void NewGRFClass<Tspec, Tid, Tmax>::InsertDefaults()
{
	/* Set up initial data */
	classes[0].global_id = 'DFLT';
	classes[0].name = STR_STATION_CLASS_DFLT;
	classes[0].count = 1;
	classes[0].spec = MallocT<StationSpec*>(1);
	classes[0].spec[0] = NULL;

	classes[1].global_id = 'WAYP';
	classes[1].name = STR_STATION_CLASS_WAYP;
	classes[1].count = 1;
	classes[1].spec = MallocT<StationSpec*>(1);
	classes[1].spec[0] = NULL;
}

INSTANTIATE_NEWGRF_CLASS_METHODS(StationClass, StationSpec, StationClassID, STAT_CLASS_MAX)

static const uint MAX_SPECLIST = 255;

enum TriggerArea {
	TA_TILE,
	TA_PLATFORM,
	TA_WHOLE,
};

struct ETileArea : TileArea {
	ETileArea(const BaseStation *st, TileIndex tile, TriggerArea ta)
	{
		switch (ta) {
			default: NOT_REACHED();

			case TA_TILE:
				this->tile = tile;
				this->w    = 1;
				this->h    = 1;
				break;

			case TA_PLATFORM: {
				TileIndex start, end;
				Axis axis = GetRailStationAxis(tile);
				TileIndexDiff delta = TileOffsByDiagDir(AxisToDiagDir(axis));

				for (end = tile; IsRailStationTile(end + delta) && IsCompatibleTrainStationTile(tile, end + delta); end += delta) { /* Nothing */ }
				for (start = tile; IsRailStationTile(start - delta) && IsCompatibleTrainStationTile(tile, start - delta); start -= delta) { /* Nothing */ }

				this->tile = start;
				this->w = TileX(end) - TileX(start) + 1;
				this->h = TileY(end) - TileY(start) + 1;
				break;
			}

			case TA_WHOLE:
				st->GetTileArea(this, Station::IsExpected(st) ? STATION_RAIL : STATION_WAYPOINT);
				break;
		}
	}
};


/**
 * Evaluate a tile's position within a station, and return the result in a bit-stuffed format.
 * if not centered: .TNLcCpP, if centered: .TNL..CP
 * - T = Tile layout number (#GetStationGfx)
 * - N = Number of platforms
 * - L = Length of platforms
 * - C = Current platform number from start, c = from end
 * - P = Position along platform from start, p = from end
 * .
 * if centered, C/P start from the centre and c/p are not available.
 * @return Platform information in bit-stuffed format.
 */
uint32 GetPlatformInfo(Axis axis, byte tile, int platforms, int length, int x, int y, bool centred)
{
	uint32 retval = 0;

	if (axis == AXIS_X) {
		Swap(platforms, length);
		Swap(x, y);
	}

	if (centred) {
		x -= platforms / 2;
		y -= length / 2;
		x = Clamp(x, -8, 7);
		y = Clamp(y, -8, 7);
		SB(retval,  0, 4, y & 0xF);
		SB(retval,  4, 4, x & 0xF);
	} else {
		SB(retval,  0, 4, min(15, y));
		SB(retval,  4, 4, min(15, length - y - 1));
		SB(retval,  8, 4, min(15, x));
		SB(retval, 12, 4, min(15, platforms - x - 1));
	}
	SB(retval, 16, 4, min(15, length));
	SB(retval, 20, 4, min(15, platforms));
	SB(retval, 24, 4, tile);

	return retval;
}


/**
 * Find the end of a railway station, from the \a tile, in the direction of \a delta.
 * @param tile Start tile.
 * @param delta Movement direction.
 * @param check_type Stop when the custom station type changes.
 * @param check_axis Stop when the station direction changes.
 * @return Found end of the railway station.
 */
static TileIndex FindRailStationEnd(TileIndex tile, TileIndexDiff delta, bool check_type, bool check_axis)
{
	byte orig_type = 0;
	Axis orig_axis = AXIS_X;
	StationID sid = GetStationIndex(tile);

	if (check_type) orig_type = GetCustomStationSpecIndex(tile);
	if (check_axis) orig_axis = GetRailStationAxis(tile);

	for (;;) {
		TileIndex new_tile = TILE_ADD(tile, delta);

		if (!IsTileType(new_tile, MP_STATION) || GetStationIndex(new_tile) != sid) break;
		if (!HasStationRail(new_tile)) break;
		if (check_type && GetCustomStationSpecIndex(new_tile) != orig_type) break;
		if (check_axis && GetRailStationAxis(new_tile) != orig_axis) break;

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

	tx -= sx; ex -= sx;
	ty -= sy; ey -= sy;

	return GetPlatformInfo(GetRailStationAxis(tile), GetStationGfx(tile), ex, ey, tx, ty, centred);
}


static uint32 GetRailContinuationInfo(TileIndex tile)
{
	/* Tile offsets and exit dirs for X axis */
	static const Direction x_dir[8] = { DIR_SW, DIR_NE, DIR_SE, DIR_NW, DIR_S, DIR_E, DIR_W, DIR_N };
	static const DiagDirection x_exits[8] = { DIAGDIR_SW, DIAGDIR_NE, DIAGDIR_SE, DIAGDIR_NW, DIAGDIR_SW, DIAGDIR_NE, DIAGDIR_SW, DIAGDIR_NE };

	/* Tile offsets and exit dirs for Y axis */
	static const Direction y_dir[8] = { DIR_SE, DIR_NW, DIR_SW, DIR_NE, DIR_S, DIR_W, DIR_E, DIR_N };
	static const DiagDirection y_exits[8] = { DIAGDIR_SE, DIAGDIR_NW, DIAGDIR_SW, DIAGDIR_NE, DIAGDIR_SE, DIAGDIR_NW, DIAGDIR_SE, DIAGDIR_NW };

	Axis axis = GetRailStationAxis(tile);

	/* Choose appropriate lookup table to use */
	const Direction *dir = axis == AXIS_X ? x_dir : y_dir;
	const DiagDirection *diagdir = axis == AXIS_X ? x_exits : y_exits;

	uint32 res = 0;
	uint i;

	for (i = 0; i < lengthof(x_dir); i++, dir++, diagdir++) {
		TileIndex neighbour_tile = tile + TileOffsByDir(*dir);
		TrackBits trackbits = TrackStatusToTrackBits(GetTileTrackStatus(neighbour_tile, TRANSPORT_RAIL, 0));
		if (trackbits != TRACK_BIT_NONE) {
			/* If there is any track on the tile, set the bit in the second byte */
			SetBit(res, i + 8);

			/* With tunnels and bridges the tile has tracks, but they are not necessarily connected
			 * with the next tile because the ramp is not going in the right direction. */
			if (IsTileType(neighbour_tile, MP_TUNNELBRIDGE) && GetTunnelBridgeDirection(neighbour_tile) != *diagdir) {
				continue;
			}

			/* If any track reaches our exit direction, set the bit in the lower byte */
			if (trackbits & DiagdirReachesTracks(*diagdir)) SetBit(res, i);
		}
	}

	return res;
}


/* Station Resolver Functions */
static uint32 StationGetRandomBits(const ResolverObject *object)
{
	const BaseStation *st = object->u.station.st;
	const TileIndex tile = object->u.station.tile;
	return (st == NULL ? 0 : st->random_bits) | (tile == INVALID_TILE ? 0 : GetStationTileRandomBits(tile) << 16);
}


static uint32 StationGetTriggers(const ResolverObject *object)
{
	const BaseStation *st = object->u.station.st;
	return st == NULL ? 0 : st->waiting_triggers;
}


static void StationSetTriggers(const ResolverObject *object, int triggers)
{
	BaseStation *st = const_cast<BaseStation *>(object->u.station.st);
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
	uint8 valid; ///< Bits indicating what variable is valid (for each bit, \c 0 is invalid, \c 1 is valid).
} _svc;

static uint32 StationGetVariable(const ResolverObject *object, byte variable, byte parameter, bool *available)
{
	const BaseStation *st = object->u.station.st;
	TileIndex tile = object->u.station.tile;

	if (object->scope == VSG_SCOPE_PARENT) {
		/* Pass the request on to the town of the station */
		Town *t;

		if (st != NULL) {
			t = st->town;
		} else if (tile != INVALID_TILE) {
			t = ClosestTownFromTile(tile, UINT_MAX);
		} else {
			*available = false;
			return UINT_MAX;
		}

		return TownGetVariable(variable, parameter, available, t, object->grffile);
	}

	if (st == NULL) {
		/* Station does not exist, so we're in a purchase list */
		switch (variable) {
			case 0x40:
			case 0x41:
			case 0x46:
			case 0x47:
			case 0x49: return 0x2110000;        // Platforms, tracks & position
			case 0x42: return 0;                // Rail type (XXX Get current type from GUI?)
			case 0x43: return _current_company; // Station owner
			case 0x44: return 2;                // PBS status
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

		case 0x42: return GetTerrainType(tile) | (GetReverseRailTypeTranslation(GetRailType(tile), object->u.station.statspec->grf_prop.grffile) << 8);
		case 0x43: return st->owner; // Station owner
		case 0x44: return HasStationReservation(tile) ? 7 : 4; // PBS status
		case 0x45:
			if (!HasBit(_svc.valid, 2)) { _svc.v45 = GetRailContinuationInfo(tile); SetBit(_svc.valid, 2); }
			return _svc.v45;

		case 0x46:
			if (!HasBit(_svc.valid, 3)) { _svc.v46 = GetPlatformInfoHelper(tile, false, false, true); SetBit(_svc.valid, 3); }
			return _svc.v46;

		case 0x47:
			if (!HasBit(_svc.valid, 4)) { _svc.v47 = GetPlatformInfoHelper(tile, true,  false, true); SetBit(_svc.valid, 4); }
			return _svc.v47;

		case 0x49:
			if (!HasBit(_svc.valid, 5)) { _svc.v49 = GetPlatformInfoHelper(tile, false, true, false); SetBit(_svc.valid, 5); }
			return _svc.v49;

		case 0x4A: // Animation frame of tile
			return GetAnimationFrame(tile);

		/* Variables which use the parameter */
		/* Variables 0x60 to 0x65 and 0x69 are handled separately below */
		case 0x66: // Animation frame of nearby tile
			if (parameter != 0) tile = GetNearbyTile(parameter, tile);
			return st->TileBelongsToRailStation(tile) ? GetAnimationFrame(tile) : UINT_MAX;

		case 0x67: { // Land info of nearby tile
			Axis axis = GetRailStationAxis(tile);

			if (parameter != 0) tile = GetNearbyTile(parameter, tile); // only perform if it is required

			Slope tileh = GetTileSlope(tile, NULL);
			bool swap = (axis == AXIS_Y && HasBit(tileh, CORNER_W) != HasBit(tileh, CORNER_E));

			return GetNearbyTileInformation(tile) ^ (swap ? SLOPE_EW : 0);
		}

		case 0x68: { // Station info of nearby tiles
			TileIndex nearby_tile = GetNearbyTile(parameter, tile);

			if (!HasStationTileRail(nearby_tile)) return 0xFFFFFFFF;

			uint32 grfid = st->speclist[GetCustomStationSpecIndex(tile)].grfid;
			bool perpendicular = GetRailStationAxis(tile) != GetRailStationAxis(nearby_tile);
			bool same_station = st->TileBelongsToRailStation(nearby_tile);
			uint32 res = GB(GetStationGfx(nearby_tile), 1, 2) << 12 | !!perpendicular << 11 | !!same_station << 10;

			if (IsCustomStationSpecIndex(nearby_tile)) {
				const StationSpecList ssl = BaseStation::GetByTile(nearby_tile)->speclist[GetCustomStationSpecIndex(nearby_tile)];
				res |= 1 << (ssl.grfid != grfid ? 9 : 8) | ssl.localidx;
			}
			return res;
		}

		/* General station variables */
		case 0x82: return 50;
		case 0x84: return st->string_id;
		case 0x86: return 0;
		case 0xF0: return st->facilities;
		case 0xFA: return Clamp(st->build_date - DAYS_TILL_ORIGINAL_BASE_YEAR, 0, 65535);
	}

	return st->GetNewGRFVariable(object, variable, parameter, available);
}

uint32 Station::GetNewGRFVariable(const ResolverObject *object, byte variable, byte parameter, bool *available) const
{
	switch (variable) {
		case 0x48: { // Accepted cargo types
			CargoID cargo_type;
			uint32 value = 0;

			for (cargo_type = 0; cargo_type < NUM_CARGO; cargo_type++) {
				if (HasBit(this->goods[cargo_type].acceptance_pickup, GoodsEntry::GES_PICKUP)) SetBit(value, cargo_type);
			}
			return value;
		}

		case 0x8A: return this->had_vehicle_of_type;
		case 0xF1: return (this->airport.tile != INVALID_TILE) ? this->airport.GetSpec()->ttd_airport_type : ATP_TTDP_LARGE;
		case 0xF2: return (this->truck_stops != NULL) ? this->truck_stops->status : 0;
		case 0xF3: return (this->bus_stops != NULL)   ? this->bus_stops->status   : 0;
		case 0xF6: return this->airport.flags;
		case 0xF7: return GB(this->airport.flags, 8, 8);
	}

	/* Handle cargo variables with parameter, 0x60 to 0x65 and 0x69 */
	if ((variable >= 0x60 && variable <= 0x65) || variable == 0x69) {
		CargoID c = GetCargoTranslation(parameter, object->u.station.statspec->grf_prop.grffile);

		if (c == CT_INVALID) return 0;
		const GoodsEntry *ge = &this->goods[c];

		switch (variable) {
			case 0x60: return min(ge->cargo.Count(), 4095);
			case 0x61: return ge->days_since_pickup;
			case 0x62: return ge->rating;
			case 0x63: return ge->cargo.DaysInTransit();
			case 0x64: return ge->last_speed | (ge->last_age << 8);
			case 0x65: return GB(ge->acceptance_pickup, GoodsEntry::GES_ACCEPTANCE, 1) << 3;
			case 0x69: return GB(ge->acceptance_pickup, GoodsEntry::GES_EVER_ACCEPTED, 4);
		}
	}

	/* Handle cargo variables (deprecated) */
	if (variable >= 0x8C && variable <= 0xEC) {
		const GoodsEntry *g = &this->goods[GB(variable - 0x8C, 3, 4)];
		switch (GB(variable - 0x8C, 0, 3)) {
			case 0: return g->cargo.Count();
			case 1: return GB(min(g->cargo.Count(), 4095), 0, 4) | (GB(g->acceptance_pickup, GoodsEntry::GES_ACCEPTANCE, 1) << 7);
			case 2: return g->days_since_pickup;
			case 3: return g->rating;
			case 4: return g->cargo.Source();
			case 5: return g->cargo.DaysInTransit();
			case 6: return g->last_speed;
			case 7: return g->last_age;
		}
	}

	DEBUG(grf, 1, "Unhandled station variable 0x%X", variable);

	*available = false;
	return UINT_MAX;
}

uint32 Waypoint::GetNewGRFVariable(const ResolverObject *object, byte variable, byte parameter, bool *available) const
{
	switch (variable) {
		case 0x48: return 0; // Accepted cargo types
		case 0x8A: return HVOT_WAYPOINT;
		case 0xF1: return 0; // airport type
		case 0xF2: return 0; // truck stop status
		case 0xF3: return 0; // bus stop status
		case 0xF6: return 0; // airport flags
		case 0xF7: return 0; // airport flags cont.
	}

	/* Handle cargo variables with parameter, 0x60 to 0x65 */
	if (variable >= 0x60 && variable <= 0x65) {
		return 0;
	}

	/* Handle cargo variables (deprecated) */
	if (variable >= 0x8C && variable <= 0xEC) {
		switch (GB(variable - 0x8C, 0, 3)) {
			case 3: return INITIAL_STATION_RATING;
			case 4: return INVALID_STATION;
			default: return 0;
		}
	}

	DEBUG(grf, 1, "Unhandled station variable 0x%X", variable);

	*available = false;
	return UINT_MAX;
}

static const SpriteGroup *StationResolveReal(const ResolverObject *object, const RealSpriteGroup *group)
{
	const BaseStation *bst = object->u.station.st;
	const StationSpec *statspec = object->u.station.statspec;
	uint set;

	uint cargo = 0;
	CargoID cargo_type = object->u.station.cargo_type;

	if (bst == NULL || statspec->cls_id == STAT_CLASS_WAYP) {
		return group->loading[0];
	}

	const Station *st = Station::From(bst);

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

	if (HasBit(statspec->flags, SSF_DIV_BY_STATION_SIZE)) cargo /= (st->train_station.w + st->train_station.h);
	cargo = min(0xfff, cargo);

	if (cargo > statspec->cargo_threshold) {
		if (group->num_loading > 0) {
			set = ((cargo - statspec->cargo_threshold) * group->num_loading) / (4096 - statspec->cargo_threshold);
			return group->loading[set];
		}
	} else {
		if (group->num_loaded > 0) {
			set = (cargo * group->num_loaded) / (statspec->cargo_threshold + 1);
			return group->loaded[set];
		}
	}

	return group->loading[0];
}


/**
 * Store a value into the persistent storage of the object's parent.
 * @param object Object that we want to query.
 * @param pos Position in the persistent storage to use.
 * @param value Value to store.
 */
void StationStorePSA(ResolverObject *object, uint pos, int32 value)
{
	/* Stations have no persistent storage. */
	BaseStation *st = object->u.station.st;
	if (object->scope != VSG_SCOPE_PARENT || st == NULL) return;

	TownStorePSA(st->town, object->grffile, pos, value);
}

static void NewStationResolver(ResolverObject *res, const StationSpec *statspec, BaseStation *st, TileIndex tile)
{
	res->GetRandomBits = StationGetRandomBits;
	res->GetTriggers   = StationGetTriggers;
	res->SetTriggers   = StationSetTriggers;
	res->GetVariable   = StationGetVariable;
	res->ResolveReal   = StationResolveReal;
	res->StorePSA      = StationStorePSA;

	res->u.station.st       = st;
	res->u.station.statspec = statspec;
	res->u.station.tile     = tile;

	res->callback        = CBID_NO_CALLBACK;
	res->callback_param1 = 0;
	res->callback_param2 = 0;
	res->ResetState();

	res->grffile         = (statspec != NULL ? statspec->grf_prop.grffile : NULL);

	/* Invalidate all cached vars */
	_svc.valid = 0;
}

static const SpriteGroup *ResolveStation(ResolverObject *object)
{
	const SpriteGroup *group;
	CargoID ctype = CT_DEFAULT_NA;

	if (object->u.station.st == NULL) {
		/* No station, so we are in a purchase list */
		ctype = CT_PURCHASE;
	} else if (Station::IsExpected(object->u.station.st)) {
		const Station *st = Station::From(object->u.station.st);
		/* Pick the first cargo that we have waiting */
		const CargoSpec *cs;
		FOR_ALL_CARGOSPECS(cs) {
			if (object->u.station.statspec->grf_prop.spritegroup[cs->Index()] != NULL &&
					!st->goods[cs->Index()].cargo.Empty()) {
				ctype = cs->Index();
				break;
			}
		}
	}

	group = object->u.station.statspec->grf_prop.spritegroup[ctype];
	if (group == NULL) {
		ctype = CT_DEFAULT;
		group = object->u.station.statspec->grf_prop.spritegroup[ctype];
	}

	if (group == NULL) return NULL;

	/* Remember the cargo type we've picked */
	object->u.station.cargo_type = ctype;

	return SpriteGroup::Resolve(group, object);
}

/**
 * Resolve sprites for drawing a station tile.
 * @param statspec Station spec
 * @param st Station (NULL in GUI)
 * @param tile Station tile being drawn (INVALID_TILE in GUI)
 * @param var10 Value to put in variable 10; normally 0; 1 when resolving the groundsprite and SSF_SEPARATE_GROUND is set.
 * @return First sprite of the Action 1 spriteset ot use, minus an offset of 0x42D to accommodate for weird NewGRF specs.
 */
SpriteID GetCustomStationRelocation(const StationSpec *statspec, BaseStation *st, TileIndex tile, uint32 var10)
{
	const SpriteGroup *group;
	ResolverObject object;

	NewStationResolver(&object, statspec, st, tile);
	object.callback_param1 = var10;

	group = ResolveStation(&object);
	if (group == NULL || group->type != SGT_RESULT) return 0;
	return group->GetResult() - 0x42D;
}

/**
 * Resolve the sprites for custom station foundations.
 * @param statspec Station spec
 * @param st Station
 * @param tile Station tile being drawn
 * @param layout Spritelayout as returned by previous callback
 * @param edge_info Information about northern tile edges; whether they need foundations or merge into adjacent tile's foundations.
 * @return First sprite of a set of foundation sprites for various slopes.
 */
SpriteID GetCustomStationFoundationRelocation(const StationSpec *statspec, BaseStation *st, TileIndex tile, uint layout, uint edge_info)
{
	const SpriteGroup *group;
	ResolverObject object;

	NewStationResolver(&object, statspec, st, tile);
	object.callback_param1 = 2; // Indicate we are resolving the foundation sprites
	object.callback_param2 = layout | (edge_info << 16);

	ClearRegister(0x100);
	group = ResolveStation(&object);
	if (group == NULL || group->type != SGT_RESULT) return 0;
	return group->GetResult() + GetRegister(0x100);
}


uint16 GetStationCallback(CallbackID callback, uint32 param1, uint32 param2, const StationSpec *statspec, BaseStation *st, TileIndex tile)
{
	const SpriteGroup *group;
	ResolverObject object;

	NewStationResolver(&object, statspec, st, tile);

	object.callback        = callback;
	object.callback_param1 = param1;
	object.callback_param2 = param2;

	group = ResolveStation(&object);
	if (group == NULL) return CALLBACK_FAILED;
	return group->GetCallbackResult();
}


/**
 * Allocate a StationSpec to a Station. This is called once per build operation.
 * @param statspec StationSpec to allocate.
 * @param st Station to allocate it to.
 * @param exec Whether to actually allocate the spec.
 * @return Index within the Station's spec list, or -1 if the allocation failed.
 */
int AllocateSpecToStation(const StationSpec *statspec, BaseStation *st, bool exec)
{
	uint i;

	if (statspec == NULL || st == NULL) return 0;

	for (i = 1; i < st->num_specs && i < MAX_SPECLIST; i++) {
		if (st->speclist[i].spec == NULL && st->speclist[i].grfid == 0) break;
	}

	if (i == MAX_SPECLIST) {
		/* As final effort when the spec list is already full...
		 * try to find the same spec and return that one. This might
		 * result in slighty "wrong" (as per specs) looking stations,
		 * but it's fairly unlikely that one reaches the limit anyways.
		 */
		for (i = 1; i < st->num_specs && i < MAX_SPECLIST; i++) {
			if (st->speclist[i].spec == statspec) return i;
		}

		return -1;
	}

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
		st->speclist[i].grfid    = statspec->grf_prop.grffile->grfid;
		st->speclist[i].localidx = statspec->grf_prop.local_id;
	}

	return i;
}


/**
 * Deallocate a StationSpec from a Station. Called when removing a single station tile.
 * @param st Station to work with.
 * @param specindex Index of the custom station within the Station's spec list.
 * @return Indicates whether the StationSpec was deallocated.
 */
void DeallocateSpecFromStation(BaseStation *st, byte specindex)
{
	/* specindex of 0 (default) is never freeable */
	if (specindex == 0) return;

	ETileArea area = ETileArea(st, INVALID_TILE, TA_WHOLE);
	/* Check all tiles over the station to check if the specindex is still in use */
	TILE_AREA_LOOP(tile, area) {
		if (st->TileBelongsToRailStation(tile) && GetCustomStationSpecIndex(tile) == specindex) {
			return;
		}
	}

	/* This specindex is no longer in use, so deallocate it */
	st->speclist[specindex].spec     = NULL;
	st->speclist[specindex].grfid    = 0;
	st->speclist[specindex].localidx = 0;

	/* If this was the highest spec index, reallocate */
	if (specindex == st->num_specs - 1) {
		for (; st->speclist[st->num_specs - 1].grfid == 0 && st->num_specs > 1; st->num_specs--) {}

		if (st->num_specs > 1) {
			st->speclist = ReallocT(st->speclist, st->num_specs);
		} else {
			free(st->speclist);
			st->num_specs = 0;
			st->speclist  = NULL;
			st->cached_anim_triggers = 0;
			return;
		}
	}

	StationUpdateAnimTriggers(st);
}

/**
 * Draw representation of a station tile for GUI purposes.
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
	const DrawTileSprites *sprites = NULL;
	const RailtypeInfo *rti = GetRailTypeInfo(railtype);
	PaletteID palette = COMPANY_SPRITE_COLOUR(_local_company);
	uint tile = 2;

	statspec = StationClass::Get(sclass, station);
	if (statspec == NULL) return false;

	if (HasBit(statspec->callback_mask, CBM_STATION_SPRITE_LAYOUT)) {
		uint16 callback = GetStationCallback(CBID_STATION_SPRITE_LAYOUT, 0x2110000, 0, statspec, NULL, INVALID_TILE);
		if (callback != CALLBACK_FAILED) tile = callback;
	}

	uint32 total_offset = rti->GetRailtypeSpriteOffset();
	uint32 relocation = 0;
	uint32 ground_relocation = 0;
	const NewGRFSpriteLayout *layout = NULL;
	DrawTileSprites tmp_rail_layout;

	if (statspec->renderdata == NULL) {
		sprites = GetStationTileLayout(STATION_RAIL, tile + axis);
	} else {
		layout = &statspec->renderdata[(tile < statspec->tiles) ? tile + axis : (uint)axis];
		if (!layout->NeedsPreprocessing()) {
			sprites = layout;
			layout = NULL;
		}
	}

	if (layout != NULL) {
		/* Sprite layout which needs preprocessing */
		bool separate_ground = HasBit(statspec->flags, SSF_SEPARATE_GROUND);
		uint32 var10_values = layout->PrepareLayout(total_offset, rti->fallback_railtype, 0, separate_ground);
		uint8 var10;
		FOR_EACH_SET_BIT(var10, var10_values) {
			uint32 var10_relocation = GetCustomStationRelocation(statspec, NULL, INVALID_TILE, var10);
			layout->ProcessRegisters(var10, var10_relocation, separate_ground);
		}

		tmp_rail_layout.seq = layout->GetLayout(&tmp_rail_layout.ground);
		sprites = &tmp_rail_layout;
		total_offset = 0;
	} else {
		/* Simple sprite layout */
		ground_relocation = relocation = GetCustomStationRelocation(statspec, NULL, INVALID_TILE, 0);
		if (HasBit(sprites->ground.sprite, SPRITE_MODIFIER_CUSTOM_SPRITE)) {
			ground_relocation = GetCustomStationRelocation(statspec, NULL, INVALID_TILE, 1);
		}
		ground_relocation += rti->fallback_railtype;
	}

	SpriteID image = sprites->ground.sprite;
	PaletteID pal = sprites->ground.pal;
	image += HasBit(image, SPRITE_MODIFIER_CUSTOM_SPRITE) ? ground_relocation : total_offset;
	if (HasBit(pal, SPRITE_MODIFIER_CUSTOM_SPRITE)) pal += ground_relocation;

	DrawSprite(image, GroundSpritePaletteTransform(image, pal, palette), x, y);

	DrawRailTileSeqInGUI(x, y, sprites, total_offset, relocation, palette);

	return true;
}


const StationSpec *GetStationSpec(TileIndex t)
{
	if (!IsCustomStationSpecIndex(t)) return NULL;

	const BaseStation *st = BaseStation::GetByTile(t);
	uint specindex = GetCustomStationSpecIndex(t);
	return specindex < st->num_specs ? st->speclist[specindex].spec : NULL;
}


/**
 * Check whether a rail station tile is NOT traversable.
 * @param tile %Tile to test.
 * @return Station tile is blocked.
 * @note This could be cached (during build) in the map array to save on all the dereferencing.
 */
bool IsStationTileBlocked(TileIndex tile)
{
	const StationSpec *statspec = GetStationSpec(tile);

	return statspec != NULL && HasBit(statspec->blocked, GetStationGfx(tile));
}

/**
 * Check if a rail station tile can be electrified.
 * @param tile %Tile to test.
 * @return Tile can be electrified.
 * @note This could be cached (during build) in the map array to save on all the dereferencing.
 */
bool IsStationTileElectrifiable(TileIndex tile)
{
	const StationSpec *statspec = GetStationSpec(tile);

	return statspec == NULL ||
			HasBit(statspec->pylons, GetStationGfx(tile)) ||
			!HasBit(statspec->wires, GetStationGfx(tile));
}

/** Helper class for animation control. */
struct StationAnimationBase : public AnimationBase<StationAnimationBase, StationSpec, BaseStation, GetStationCallback> {
	static const CallbackID cb_animation_speed      = CBID_STATION_ANIMATION_SPEED;
	static const CallbackID cb_animation_next_frame = CBID_STATION_ANIM_NEXT_FRAME;

	static const StationCallbackMask cbm_animation_speed      = CBM_STATION_ANIMATION_SPEED;
	static const StationCallbackMask cbm_animation_next_frame = CBM_STATION_ANIMATION_NEXT_FRAME;
};

void AnimateStationTile(TileIndex tile)
{
	const StationSpec *ss = GetStationSpec(tile);
	if (ss == NULL) return;

	StationAnimationBase::AnimateTile(ss, BaseStation::GetByTile(tile), tile, HasBit(ss->flags, SSF_CB141_RANDOM_BITS));
}

void TriggerStationAnimation(BaseStation *st, TileIndex tile, StationAnimationTrigger trigger, CargoID cargo_type)
{
	/* List of coverage areas for each animation trigger */
	static const TriggerArea tas[] = {
		TA_TILE, TA_WHOLE, TA_WHOLE, TA_PLATFORM, TA_PLATFORM, TA_PLATFORM, TA_WHOLE
	};

	/* Get Station if it wasn't supplied */
	if (st == NULL) st = BaseStation::GetByTile(tile);

	/* Check the cached animation trigger bitmask to see if we need
	 * to bother with any further processing. */
	if (!HasBit(st->cached_anim_triggers, trigger)) return;

	uint16 random_bits = Random();
	ETileArea area = ETileArea(st, tile, tas[trigger]);

	/* Check all tiles over the station to check if the specindex is still in use */
	TILE_AREA_LOOP(tile, area) {
		if (st->TileBelongsToRailStation(tile)) {
			const StationSpec *ss = GetStationSpec(tile);
			if (ss != NULL && HasBit(ss->animation.triggers, trigger)) {
				CargoID cargo;
				if (cargo_type == CT_INVALID) {
					cargo = CT_INVALID;
				} else {
					cargo = GetReverseCargoTranslation(cargo_type, ss->grf_prop.grffile);
				}
				StationAnimationBase::ChangeAnimationFrame(CBID_STATION_ANIM_START_STOP, ss, st, tile, (random_bits << 16) | Random(), (uint8)trigger | (cargo << 8));
			}
		}
	}
}

/**
 * Update the cached animation trigger bitmask for a station.
 * @param st Station to update.
 */
void StationUpdateAnimTriggers(BaseStation *st)
{
	st->cached_anim_triggers = 0;

	/* Combine animation trigger bitmask for all station specs
	 * of this station. */
	for (uint i = 0; i < st->num_specs; i++) {
		const StationSpec *ss = st->speclist[i].spec;
		if (ss != NULL) st->cached_anim_triggers |= ss->animation.triggers;
	}
}

/**
 * Resolve a station's spec and such so we can get a variable.
 * @param ro    The resolver object to fill.
 * @param index The station to get the data from.
 */
void GetStationResolver(ResolverObject *ro, uint index)
{
	NewStationResolver(ro, GetStationSpec(index), Station::GetByTile(index), index);
}
