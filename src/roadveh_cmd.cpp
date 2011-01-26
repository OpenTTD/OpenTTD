/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file roadveh_cmd.cpp Handling of road vehicles. */

#include "stdafx.h"
#include "roadveh.h"
#include "command_func.h"
#include "news_func.h"
#include "pathfinder/npf/npf_func.h"
#include "station_base.h"
#include "company_func.h"
#include "vehicle_gui.h"
#include "articulated_vehicles.h"
#include "newgrf_sound.h"
#include "pathfinder/yapf/yapf.h"
#include "strings_func.h"
#include "tunnelbridge_map.h"
#include "functions.h"
#include "window_func.h"
#include "date_func.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "ai/ai.hpp"
#include "depot_map.h"
#include "effectvehicle_func.h"
#include "roadstop_base.h"
#include "spritecache.h"
#include "core/random_func.hpp"
#include "company_base.h"
#include "core/backup_type.hpp"

#include "table/strings.h"

static const uint16 _roadveh_images[63] = {
	0xCD4, 0xCDC, 0xCE4, 0xCEC, 0xCF4, 0xCFC, 0xD0C, 0xD14,
	0xD24, 0xD1C, 0xD2C, 0xD04, 0xD1C, 0xD24, 0xD6C, 0xD74,
	0xD7C, 0xC14, 0xC1C, 0xC24, 0xC2C, 0xC34, 0xC3C, 0xC4C,
	0xC54, 0xC64, 0xC5C, 0xC6C, 0xC44, 0xC5C, 0xC64, 0xCAC,
	0xCB4, 0xCBC, 0xD94, 0xD9C, 0xDA4, 0xDAC, 0xDB4, 0xDBC,
	0xDCC, 0xDD4, 0xDE4, 0xDDC, 0xDEC, 0xDC4, 0xDDC, 0xDE4,
	0xE2C, 0xE34, 0xE3C, 0xC14, 0xC1C, 0xC2C, 0xC3C, 0xC4C,
	0xC5C, 0xC64, 0xC6C, 0xC74, 0xC84, 0xC94, 0xCA4
};

static const uint16 _roadveh_full_adder[63] = {
	 0,  88,   0,   0,   0,   0,  48,  48,
	48,  48,   0,   0,  64,  64,   0,  16,
	16,   0,  88,   0,   0,   0,   0,  48,
	48,  48,  48,   0,   0,  64,  64,   0,
	16,  16,   0,  88,   0,   0,   0,   0,
	48,  48,  48,  48,   0,   0,  64,  64,
	 0,  16,  16,   0,   8,   8,   8,   8,
	 0,   0,   0,   8,   8,   8,   8
};

/** 'Convert' the DiagDirection where a road vehicle enters to the trackdirs it can drive onto */
static const TrackdirBits _road_enter_dir_to_reachable_trackdirs[DIAGDIR_END] = {
	TRACKDIR_BIT_LEFT_N  | TRACKDIR_BIT_LOWER_E | TRACKDIR_BIT_X_NE,    // Enter from north east
	TRACKDIR_BIT_LEFT_S  | TRACKDIR_BIT_UPPER_E | TRACKDIR_BIT_Y_SE,    // Enter from south east
	TRACKDIR_BIT_UPPER_W | TRACKDIR_BIT_X_SW    | TRACKDIR_BIT_RIGHT_S, // Enter from south west
	TRACKDIR_BIT_RIGHT_N | TRACKDIR_BIT_LOWER_W | TRACKDIR_BIT_Y_NW     // Enter from north west
};

static const Trackdir _road_reverse_table[DIAGDIR_END] = {
	TRACKDIR_RVREV_NE, TRACKDIR_RVREV_SE, TRACKDIR_RVREV_SW, TRACKDIR_RVREV_NW
};

/** Converts the exit direction of a depot to trackdir the vehicle is going to drive to */
static const Trackdir _roadveh_depot_exit_trackdir[DIAGDIR_END] = {
	TRACKDIR_X_NE, TRACKDIR_Y_SE, TRACKDIR_X_SW, TRACKDIR_Y_NW
};


/**
 * Check whether a roadvehicle is a bus
 * @return true if bus
 */
bool RoadVehicle::IsBus() const
{
	assert(this->IsFrontEngine());
	return IsCargoInClass(this->cargo_type, CC_PASSENGERS);
}

/**
 * Get the width of a road vehicle image in the GUI.
 * @param offset Additional offset for positioning the sprite; set to NULL if not needed
 * @return Width in pixels
 */
int RoadVehicle::GetDisplayImageWidth(Point *offset) const
{
	int reference_width = ROADVEHINFO_DEFAULT_VEHICLE_WIDTH;

	if (offset != NULL) {
		offset->x = reference_width / 2;
		offset->y = 0;
	}
	return this->gcache.cached_veh_length * reference_width / 8;
}

static SpriteID GetRoadVehIcon(EngineID engine)
{
	const Engine *e = Engine::Get(engine);
	uint8 spritenum = e->u.road.image_index;

	if (is_custom_sprite(spritenum)) {
		SpriteID sprite = GetCustomVehicleIcon(engine, DIR_W);
		if (sprite != 0) return sprite;

		spritenum = e->original_image_index;
	}

	return DIR_W + _roadveh_images[spritenum];
}

SpriteID RoadVehicle::GetImage(Direction direction) const
{
	uint8 spritenum = this->spritenum;
	SpriteID sprite;

	if (is_custom_sprite(spritenum)) {
		sprite = GetCustomVehicleSprite(this, (Direction)(direction + 4 * IS_CUSTOM_SECONDHEAD_SPRITE(spritenum)));
		if (sprite != 0) return sprite;

		spritenum = Engine::Get(this->engine_type)->original_image_index;
	}

	sprite = direction + _roadveh_images[spritenum];

	if (this->cargo.Count() >= this->cargo_cap / 2U) sprite += _roadveh_full_adder[spritenum];

	return sprite;
}

/**
 * Draw a road vehicle engine.
 * @param left Left edge to draw within.
 * @param right Right edge to draw within.
 * @param preferred_x Preferred position of the engine.
 * @param y Vertical position of the engine.
 * @param engine Engine to draw
 * @param pal Palette to use.
 */
void DrawRoadVehEngine(int left, int right, int preferred_x, int y, EngineID engine, PaletteID pal)
{
	SpriteID sprite = GetRoadVehIcon(engine);
	const Sprite *real_sprite = GetSprite(sprite, ST_NORMAL);
	preferred_x = Clamp(preferred_x, left - real_sprite->x_offs, right - real_sprite->width - real_sprite->x_offs);
	DrawSprite(sprite, pal, preferred_x, y);
}

static uint GetRoadVehLength(const RoadVehicle *v)
{
	uint length = 8;

	uint16 veh_len = GetVehicleCallback(CBID_VEHICLE_LENGTH, 0, 0, v->engine_type, v);
	if (veh_len != CALLBACK_FAILED) {
		length -= Clamp(veh_len, 0, 7);
	}

	return length;
}

void RoadVehUpdateCache(RoadVehicle *v)
{
	assert(v->type == VEH_ROAD);
	assert(v->IsFrontEngine());

	v->InvalidateNewGRFCacheOfChain();

	v->gcache.cached_total_length = 0;

	for (RoadVehicle *u = v; u != NULL; u = u->Next()) {
		/* Check the v->first cache. */
		assert(u->First() == v);

		/* Update the 'first engine' */
		u->gcache.first_engine = (v == u) ? INVALID_ENGINE : v->engine_type;

		/* Update the length of the vehicle. */
		u->gcache.cached_veh_length = GetRoadVehLength(u);
		v->gcache.cached_total_length += u->gcache.cached_veh_length;

		/* Update visual effect */
		v->UpdateVisualEffect();

		/* Invalidate the vehicle colour map */
		u->colourmap = PAL_NONE;
	}

	uint max_speed = GetVehicleProperty(v, PROP_ROADVEH_SPEED, 0);
	v->vcache.cached_max_speed = (max_speed != 0) ? max_speed * 4 : RoadVehInfo(v->engine_type)->max_speed;
}

/**
 * Build a road vehicle.
 * @param tile     tile of the depot where road vehicle is built.
 * @param flags    type of operation.
 * @param e        the engine to build.
 * @param data     unused.
 * @param ret[out] the vehicle that has been built.
 * @return the cost of this operation or an error.
 */
CommandCost CmdBuildRoadVehicle(TileIndex tile, DoCommandFlag flags, const Engine *e, uint16 data, Vehicle **ret)
{
	if (HasTileRoadType(tile, ROADTYPE_TRAM) != HasBit(e->info.misc_flags, EF_ROAD_TRAM)) return_cmd_error(STR_ERROR_DEPOT_WRONG_DEPOT_TYPE);

	if (flags & DC_EXEC) {
		const RoadVehicleInfo *rvi = &e->u.road;

		RoadVehicle *v = new RoadVehicle();
		*ret = v;
		v->direction = DiagDirToDir(GetRoadDepotDirection(tile));
		v->owner = _current_company;

		v->tile = tile;
		int x = TileX(tile) * TILE_SIZE + TILE_SIZE / 2;
		int y = TileY(tile) * TILE_SIZE + TILE_SIZE / 2;
		v->x_pos = x;
		v->y_pos = y;
		v->z_pos = GetSlopeZ(x, y);

		v->state = RVSB_IN_DEPOT;
		v->vehstatus = VS_HIDDEN | VS_STOPPED | VS_DEFPAL;

		v->spritenum = rvi->image_index;
		v->cargo_type = e->GetDefaultCargoType();
		v->cargo_cap = rvi->capacity;

		v->last_station_visited = INVALID_STATION;
		v->engine_type = e->index;
		v->gcache.first_engine = INVALID_ENGINE; // needs to be set before first callback

		v->reliability = e->reliability;
		v->reliability_spd_dec = e->reliability_spd_dec;
		v->max_age = e->GetLifeLengthInDays();
		_new_vehicle_id = v->index;

		v->service_interval = Company::Get(v->owner)->settings.vehicle.servint_roadveh;

		v->date_of_last_service = _date;
		v->build_year = _cur_year;

		v->cur_image = SPR_IMG_QUERY;
		v->random_bits = VehicleRandomBits();
		v->SetFrontEngine();

		v->roadtype = HasBit(e->info.misc_flags, EF_ROAD_TRAM) ? ROADTYPE_TRAM : ROADTYPE_ROAD;
		v->compatible_roadtypes = RoadTypeToRoadTypes(v->roadtype);
		v->gcache.cached_veh_length = 8;

		if (e->flags & ENGINE_EXCLUSIVE_PREVIEW) SetBit(v->vehicle_flags, VF_BUILT_AS_PROTOTYPE);

		AddArticulatedParts(v);
		v->InvalidateNewGRFCacheOfChain();

		/* Call various callbacks after the whole consist has been constructed */
		for (RoadVehicle *u = v; u != NULL; u = u->Next()) {
			u->cargo_cap = GetVehicleCapacity(u);
			v->InvalidateNewGRFCache();
			u->InvalidateNewGRFCache();
		}
		RoadVehUpdateCache(v);
		/* Initialize cached values for realistic acceleration. */
		if (_settings_game.vehicle.roadveh_acceleration_model != AM_ORIGINAL) v->CargoChanged();

		VehicleMove(v, false);

		CheckConsistencyOfArticulatedVehicle(v);
	}

	return CommandCost();
}

bool RoadVehicle::IsStoppedInDepot() const
{
	TileIndex tile = this->tile;

	if (!IsRoadDepotTile(tile)) return false;
	if (this->IsFrontEngine() && !(this->vehstatus & VS_STOPPED)) return false;

	for (const RoadVehicle *v = this; v != NULL; v = v->Next()) {
		if (v->state != RVSB_IN_DEPOT || v->tile != tile) return false;
	}
	return true;
}

static FindDepotData FindClosestRoadDepot(const RoadVehicle *v, int max_distance)
{
	if (IsRoadDepotTile(v->tile)) return FindDepotData(v->tile, 0);

	switch (_settings_game.pf.pathfinder_for_roadvehs) {
		case VPF_NPF: return NPFRoadVehicleFindNearestDepot(v, max_distance);
		case VPF_YAPF: return YapfRoadVehicleFindNearestDepot(v, max_distance);

		default: NOT_REACHED();
	}
}

bool RoadVehicle::FindClosestDepot(TileIndex *location, DestinationID *destination, bool *reverse)
{
	FindDepotData rfdd = FindClosestRoadDepot(this, 0);
	if (rfdd.best_length == UINT_MAX) return false;

	if (location    != NULL) *location    = rfdd.tile;
	if (destination != NULL) *destination = GetDepotIndex(rfdd.tile);

	return true;
}

/**
 * Turn a roadvehicle around.
 * @param tile unused
 * @param flags operation to perform
 * @param p1 vehicle ID to turn
 * @param p2 unused
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdTurnRoadVeh(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	RoadVehicle *v = RoadVehicle::GetIfValid(p1);
	if (v == NULL) return CMD_ERROR;

	CommandCost ret = CheckOwnership(v->owner);
	if (ret.Failed()) return ret;

	if ((v->vehstatus & VS_STOPPED) ||
			(v->vehstatus & VS_CRASHED) ||
			v->breakdown_ctr != 0 ||
			v->overtaking != 0 ||
			v->state == RVSB_WORMHOLE ||
			v->IsInDepot() ||
			v->current_order.IsType(OT_LOADING)) {
		return CMD_ERROR;
	}

	if (IsNormalRoadTile(v->tile) && GetDisallowedRoadDirections(v->tile) != DRD_NONE) return CMD_ERROR;

	if (IsTileType(v->tile, MP_TUNNELBRIDGE) && DirToDiagDir(v->direction) == GetTunnelBridgeDirection(v->tile)) return CMD_ERROR;

	if (flags & DC_EXEC) v->reverse_ctr = 180;

	return CommandCost();
}


void RoadVehicle::MarkDirty()
{
	for (RoadVehicle *v = this; v != NULL; v = v->Next()) {
		v->UpdateViewport(false, false);
	}
	this->CargoChanged();
}

void RoadVehicle::UpdateDeltaXY(Direction direction)
{
#define MKIT(a, b, c, d) ((a & 0xFF) << 24) | ((b & 0xFF) << 16) | ((c & 0xFF) << 8) | ((d & 0xFF) << 0)
	static const uint32 _delta_xy_table[8] = {
		MKIT(3, 3, -1, -1),
		MKIT(3, 7, -1, -3),
		MKIT(3, 3, -1, -1),
		MKIT(7, 3, -3, -1),
		MKIT(3, 3, -1, -1),
		MKIT(3, 7, -1, -3),
		MKIT(3, 3, -1, -1),
		MKIT(7, 3, -3, -1),
	};
#undef MKIT

	uint32 x = _delta_xy_table[direction];
	this->x_offs        = GB(x,  0, 8);
	this->y_offs        = GB(x,  8, 8);
	this->x_extent      = GB(x, 16, 8);
	this->y_extent      = GB(x, 24, 8);
	this->z_extent      = 6;
}

/**
 * Calculates the maximum speed of the vehicle under its current conditions.
 * @return Maximum speed of the vehicle.
 */
FORCEINLINE int RoadVehicle::GetCurrentMaxSpeed() const
{
	if (_settings_game.vehicle.roadveh_acceleration_model == AM_ORIGINAL) return this->vcache.cached_max_speed;

	int max_speed = this->vcache.cached_max_speed;

	/* Limit speed to 50% while reversing, 75% in curves. */
	for (const RoadVehicle *u = this; u != NULL; u = u->Next()) {
		if (this->state <= RVSB_TRACKDIR_MASK && IsReversingRoadTrackdir((Trackdir)this->state)) {
			max_speed = this->vcache.cached_max_speed / 2;
			break;
		} else if ((u->direction & 1) == 0) {
			max_speed = this->vcache.cached_max_speed * 3 / 4;
		}
	}

	return max_speed;
}

static void DeleteLastRoadVeh(RoadVehicle *v)
{
	Vehicle *u = v;
	for (; v->Next() != NULL; v = v->Next()) u = v;
	u->SetNext(NULL);

	/* Only leave the road stop when we're really gone. */
	if (IsInsideMM(v->state, RVSB_IN_ROAD_STOP, RVSB_IN_ROAD_STOP_END)) RoadStop::GetByTile(v->tile, GetRoadStopType(v->tile))->Leave(v);

	delete v;
}

static void RoadVehSetRandomDirection(RoadVehicle *v)
{
	static const DirDiff delta[] = {
		DIRDIFF_45LEFT, DIRDIFF_SAME, DIRDIFF_SAME, DIRDIFF_45RIGHT
	};

	do {
		uint32 r = Random();

		v->direction = ChangeDir(v->direction, delta[r & 3]);
		v->UpdateInclination(false, true);
	} while ((v = v->Next()) != NULL);
}

static bool RoadVehIsCrashed(RoadVehicle *v)
{
	v->crashed_ctr++;
	if (v->crashed_ctr == 2) {
		CreateEffectVehicleRel(v, 4, 4, 8, EV_EXPLOSION_LARGE);
	} else if (v->crashed_ctr <= 45) {
		if ((v->tick_counter & 7) == 0) RoadVehSetRandomDirection(v);
	} else if (v->crashed_ctr >= 2220 && !(v->tick_counter & 0x1F)) {
		bool ret = v->Next() != NULL;
		DeleteLastRoadVeh(v);
		return ret;
	}

	return true;
}

static Vehicle *EnumCheckRoadVehCrashTrain(Vehicle *v, void *data)
{
	const Vehicle *u = (Vehicle*)data;

	return (v->type == VEH_TRAIN &&
			abs(v->z_pos - u->z_pos) <= 6 &&
			abs(v->x_pos - u->x_pos) <= 4 &&
			abs(v->y_pos - u->y_pos) <= 4) ? v : NULL;
}

uint RoadVehicle::Crash(bool flooded)
{
	uint pass = this->GroundVehicleBase::Crash(flooded);
	if (this->IsFrontEngine()) {
		pass += 1; // driver

		/* If we're in a drive through road stop we ought to leave it */
		if (IsInsideMM(this->state, RVSB_IN_DT_ROAD_STOP, RVSB_IN_DT_ROAD_STOP_END)) {
			RoadStop::GetByTile(this->tile, GetRoadStopType(this->tile))->Leave(this);
		}
	}
	this->crashed_ctr = flooded ? 2000 : 1; // max 2220, disappear pretty fast when flooded
	return pass;
}

static void RoadVehCrash(RoadVehicle *v)
{
	uint pass = v->Crash();

	AI::NewEvent(v->owner, new AIEventVehicleCrashed(v->index, v->tile, AIEventVehicleCrashed::CRASH_RV_LEVEL_CROSSING));

	SetDParam(0, pass);
	AddVehicleNewsItem(
		(pass == 1) ?
			STR_NEWS_ROAD_VEHICLE_CRASH_DRIVER : STR_NEWS_ROAD_VEHICLE_CRASH,
		NS_ACCIDENT,
		v->index
	);

	ModifyStationRatingAround(v->tile, v->owner, -160, 22);
	SndPlayVehicleFx(SND_12_EXPLOSION, v);
}

static bool RoadVehCheckTrainCrash(RoadVehicle *v)
{
	for (RoadVehicle *u = v; u != NULL; u = u->Next()) {
		if (u->state == RVSB_WORMHOLE) continue;

		TileIndex tile = u->tile;

		if (!IsLevelCrossingTile(tile)) continue;

		if (HasVehicleOnPosXY(v->x_pos, v->y_pos, u, EnumCheckRoadVehCrashTrain)) {
			RoadVehCrash(v);
			return true;
		}
	}

	return false;
}

TileIndex RoadVehicle::GetOrderStationLocation(StationID station)
{
	if (station == this->last_station_visited) this->last_station_visited = INVALID_STATION;

	const Station *st = Station::Get(station);
	if (!CanVehicleUseStation(this, st)) {
		/* There is no stop left at the station, so don't even TRY to go there */
		this->IncrementOrderIndex();
		return 0;
	}

	return st->xy;
}

static void StartRoadVehSound(const RoadVehicle *v)
{
	if (!PlayVehicleSound(v, VSE_START)) {
		SoundID s = RoadVehInfo(v->engine_type)->sfx;
		if (s == SND_19_BUS_START_PULL_AWAY && (v->tick_counter & 3) == 0) {
			s = SND_1A_BUS_START_PULL_AWAY_WITH_HORN;
		}
		SndPlayVehicleFx(s, v);
	}
}

struct RoadVehFindData {
	int x;
	int y;
	const Vehicle *veh;
	Vehicle *best;
	uint best_diff;
	Direction dir;
};

static Vehicle *EnumCheckRoadVehClose(Vehicle *v, void *data)
{
	static const int8 dist_x[] = { -4, -8, -4, -1, 4, 8, 4, 1 };
	static const int8 dist_y[] = { -4, -1, 4, 8, 4, 1, -4, -8 };

	RoadVehFindData *rvf = (RoadVehFindData*)data;

	short x_diff = v->x_pos - rvf->x;
	short y_diff = v->y_pos - rvf->y;

	if (v->type == VEH_ROAD &&
			!v->IsInDepot() &&
			abs(v->z_pos - rvf->veh->z_pos) < 6 &&
			v->direction == rvf->dir &&
			rvf->veh->First() != v->First() &&
			(dist_x[v->direction] >= 0 || (x_diff > dist_x[v->direction] && x_diff <= 0)) &&
			(dist_x[v->direction] <= 0 || (x_diff < dist_x[v->direction] && x_diff >= 0)) &&
			(dist_y[v->direction] >= 0 || (y_diff > dist_y[v->direction] && y_diff <= 0)) &&
			(dist_y[v->direction] <= 0 || (y_diff < dist_y[v->direction] && y_diff >= 0))) {
		uint diff = abs(x_diff) + abs(y_diff);

		if (diff < rvf->best_diff || (diff == rvf->best_diff && v->index < rvf->best->index)) {
			rvf->best = v;
			rvf->best_diff = diff;
		}
	}

	return NULL;
}

static RoadVehicle *RoadVehFindCloseTo(RoadVehicle *v, int x, int y, Direction dir, bool update_blocked_ctr = true)
{
	RoadVehFindData rvf;
	RoadVehicle *front = v->First();

	if (front->reverse_ctr != 0) return NULL;

	rvf.x = x;
	rvf.y = y;
	rvf.dir = dir;
	rvf.veh = v;
	rvf.best_diff = UINT_MAX;

	if (front->state == RVSB_WORMHOLE) {
		FindVehicleOnPos(v->tile, &rvf, EnumCheckRoadVehClose);
		FindVehicleOnPos(GetOtherTunnelBridgeEnd(v->tile), &rvf, EnumCheckRoadVehClose);
	} else {
		FindVehicleOnPosXY(x, y, &rvf, EnumCheckRoadVehClose);
	}

	/* This code protects a roadvehicle from being blocked for ever
	 * If more than 1480 / 74 days a road vehicle is blocked, it will
	 * drive just through it. The ultimate backup-code of TTD.
	 * It can be disabled. */
	if (rvf.best_diff == UINT_MAX) {
		front->blocked_ctr = 0;
		return NULL;
	}

	if (update_blocked_ctr && ++front->blocked_ctr > 1480) return NULL;

	return RoadVehicle::From(rvf.best);
}

/**
 * A road vehicle arrives at a station. If it is the first time, create a news item.
 * @param v  Road vehicle that arrived.
 * @param st Station where the road vehicle arrived.
 */
static void RoadVehArrivesAt(const RoadVehicle *v, Station *st)
{
	if (v->IsBus()) {
		/* Check if station was ever visited before */
		if (!(st->had_vehicle_of_type & HVOT_BUS)) {
			st->had_vehicle_of_type |= HVOT_BUS;
			SetDParam(0, st->index);
			AddVehicleNewsItem(
				v->roadtype == ROADTYPE_ROAD ? STR_NEWS_FIRST_BUS_ARRIVAL : STR_NEWS_FIRST_PASSENGER_TRAM_ARRIVAL,
				(v->owner == _local_company) ? NS_ARRIVAL_COMPANY : NS_ARRIVAL_OTHER,
				v->index,
				st->index
			);
			AI::NewEvent(v->owner, new AIEventStationFirstVehicle(st->index, v->index));
		}
	} else {
		/* Check if station was ever visited before */
		if (!(st->had_vehicle_of_type & HVOT_TRUCK)) {
			st->had_vehicle_of_type |= HVOT_TRUCK;
			SetDParam(0, st->index);
			AddVehicleNewsItem(
				v->roadtype == ROADTYPE_ROAD ? STR_NEWS_FIRST_TRUCK_ARRIVAL : STR_NEWS_FIRST_CARGO_TRAM_ARRIVAL,
				(v->owner == _local_company) ? NS_ARRIVAL_COMPANY : NS_ARRIVAL_OTHER,
				v->index,
				st->index
			);
			AI::NewEvent(v->owner, new AIEventStationFirstVehicle(st->index, v->index));
		}
	}
}

/**
 * This function looks at the vehicle and updates its speed (cur_speed
 * and subspeed) variables. Furthermore, it returns the distance that
 * the vehicle can drive this tick. #Vehicle::GetAdvanceDistance() determines
 * the distance to drive before moving a step on the map.
 * @return distance to drive.
 */
int RoadVehicle::UpdateSpeed()
{
	switch (_settings_game.vehicle.roadveh_acceleration_model) {
		default: NOT_REACHED();
		case AM_ORIGINAL:
			return this->DoUpdateSpeed(this->overtaking != 0 ? 512 : 256, 0, this->GetCurrentMaxSpeed());

		case AM_REALISTIC:
			return this->DoUpdateSpeed(this->GetAcceleration() + (this->overtaking != 0 ? 256 : 0), this->GetAccelerationStatus() == AS_BRAKE ? 0 : 4, this->GetCurrentMaxSpeed());
	}
}

static Direction RoadVehGetNewDirection(const RoadVehicle *v, int x, int y)
{
	static const Direction _roadveh_new_dir[] = {
		DIR_N , DIR_NW, DIR_W , INVALID_DIR,
		DIR_NE, DIR_N , DIR_SW, INVALID_DIR,
		DIR_E , DIR_SE, DIR_S
	};

	x = x - v->x_pos + 1;
	y = y - v->y_pos + 1;

	if ((uint)x > 2 || (uint)y > 2) return v->direction;
	return _roadveh_new_dir[y * 4 + x];
}

static Direction RoadVehGetSlidingDirection(const RoadVehicle *v, int x, int y)
{
	Direction new_dir = RoadVehGetNewDirection(v, x, y);
	Direction old_dir = v->direction;
	DirDiff delta;

	if (new_dir == old_dir) return old_dir;
	delta = (DirDifference(new_dir, old_dir) > DIRDIFF_REVERSE ? DIRDIFF_45LEFT : DIRDIFF_45RIGHT);
	return ChangeDir(old_dir, delta);
}

struct OvertakeData {
	const RoadVehicle *u;
	const RoadVehicle *v;
	TileIndex tile;
	Trackdir trackdir;
};

static Vehicle *EnumFindVehBlockingOvertake(Vehicle *v, void *data)
{
	const OvertakeData *od = (OvertakeData*)data;

	return (v->type == VEH_ROAD && v->First() == v && v != od->u && v != od->v) ? v : NULL;
}

/**
 * Check if overtaking is possible on a piece of track
 *
 * @param od Information about the tile and the involved vehicles
 * @return true if we have to abort overtaking
 */
static bool CheckRoadBlockedForOvertaking(OvertakeData *od)
{
	TrackStatus ts = GetTileTrackStatus(od->tile, TRANSPORT_ROAD, od->v->compatible_roadtypes);
	TrackdirBits trackdirbits = TrackStatusToTrackdirBits(ts);
	TrackdirBits red_signals = TrackStatusToRedSignals(ts); // barred level crossing
	TrackBits trackbits = TrackdirBitsToTrackBits(trackdirbits);

	/* Track does not continue along overtaking direction || track has junction || levelcrossing is barred */
	if (!HasBit(trackdirbits, od->trackdir) || (trackbits & ~TRACK_BIT_CROSS) || (red_signals != TRACKDIR_BIT_NONE)) return true;

	/* Are there more vehicles on the tile except the two vehicles involved in overtaking */
	return HasVehicleOnPos(od->tile, od, EnumFindVehBlockingOvertake);
}

static void RoadVehCheckOvertake(RoadVehicle *v, RoadVehicle *u)
{
	OvertakeData od;

	od.v = v;
	od.u = u;

	if (u->vcache.cached_max_speed >= v->vcache.cached_max_speed &&
			!(u->vehstatus & VS_STOPPED) &&
			u->cur_speed != 0) {
		return;
	}

	/* Trams can't overtake other trams */
	if (v->roadtype == ROADTYPE_TRAM) return;

	/* Don't overtake in stations */
	if (IsTileType(v->tile, MP_STATION) || IsTileType(u->tile, MP_STATION)) return;

	/* For now, articulated road vehicles can't overtake anything. */
	if (v->HasArticulatedPart()) return;

	/* Vehicles are not driving in same direction || direction is not a diagonal direction */
	if (v->direction != u->direction || !(v->direction & 1)) return;

	/* Check if vehicle is in a road stop, depot, tunnel or bridge or not on a straight road */
	if (v->state >= RVSB_IN_ROAD_STOP || !IsStraightRoadTrackdir((Trackdir)(v->state & RVSB_TRACKDIR_MASK))) return;

	od.trackdir = DiagDirToDiagTrackdir(DirToDiagDir(v->direction));

	/* Are the current and the next tile suitable for overtaking?
	 *  - Does the track continue along od.trackdir
	 *  - No junctions
	 *  - No barred levelcrossing
	 *  - No other vehicles in the way
	 */
	od.tile = v->tile;
	if (CheckRoadBlockedForOvertaking(&od)) return;

	od.tile = v->tile + TileOffsByDiagDir(DirToDiagDir(v->direction));
	if (CheckRoadBlockedForOvertaking(&od)) return;

	if (od.u->cur_speed == 0 || (od.u->vehstatus & VS_STOPPED)) {
		v->overtaking_ctr = 0x11;
		v->overtaking = 0x10;
	} else {
//		if (CheckRoadBlockedForOvertaking(&od)) return;
		v->overtaking_ctr = 0;
		v->overtaking = 0x10;
	}
}

static void RoadZPosAffectSpeed(RoadVehicle *v, byte old_z)
{
	if (old_z == v->z_pos || _settings_game.vehicle.roadveh_acceleration_model != AM_ORIGINAL) return;

	if (old_z < v->z_pos) {
		v->cur_speed = v->cur_speed * 232 / 256; // slow down by ~10%
	} else {
		uint16 spd = v->cur_speed + 2;
		if (spd <= v->vcache.cached_max_speed) v->cur_speed = spd;
	}
}

static int PickRandomBit(uint bits)
{
	uint i;
	uint num = RandomRange(CountBits(bits));

	for (i = 0; !(bits & 1) || (int)--num >= 0; bits >>= 1, i++) {}
	return i;
}

/**
 * Returns direction to for a road vehicle to take or
 * INVALID_TRACKDIR if the direction is currently blocked
 * @param v        the Vehicle to do the pathfinding for
 * @param tile     the where to start the pathfinding
 * @param enterdir the direction the vehicle enters the tile from
 * @return the Trackdir to take
 */
static Trackdir RoadFindPathToDest(RoadVehicle *v, TileIndex tile, DiagDirection enterdir)
{
#define return_track(x) { best_track = (Trackdir)x; goto found_best_track; }

	TileIndex desttile;
	Trackdir best_track;
	bool path_found = true;

	TrackStatus ts = GetTileTrackStatus(tile, TRANSPORT_ROAD, v->compatible_roadtypes);
	TrackdirBits red_signals = TrackStatusToRedSignals(ts); // crossing
	TrackdirBits trackdirs = TrackStatusToTrackdirBits(ts);

	if (IsTileType(tile, MP_ROAD)) {
		if (IsRoadDepot(tile) && (!IsTileOwner(tile, v->owner) || GetRoadDepotDirection(tile) == enterdir || (GetRoadTypes(tile) & v->compatible_roadtypes) == 0)) {
			/* Road depot owned by another company or with the wrong orientation */
			trackdirs = TRACKDIR_BIT_NONE;
		}
	} else if (IsTileType(tile, MP_STATION) && IsStandardRoadStopTile(tile)) {
		/* Standard road stop (drive-through stops are treated as normal road) */

		if (!IsTileOwner(tile, v->owner) || GetRoadStopDir(tile) == enterdir || v->HasArticulatedPart()) {
			/* different station owner or wrong orientation or the vehicle has articulated parts */
			trackdirs = TRACKDIR_BIT_NONE;
		} else {
			/* Our station */
			RoadStopType rstype = v->IsBus() ? ROADSTOP_BUS : ROADSTOP_TRUCK;

			if (GetRoadStopType(tile) != rstype) {
				/* Wrong station type */
				trackdirs = TRACKDIR_BIT_NONE;
			} else {
				/* Proper station type, check if there is free loading bay */
				if (!_settings_game.pf.roadveh_queue && IsStandardRoadStopTile(tile) &&
						!RoadStop::GetByTile(tile, rstype)->HasFreeBay()) {
					/* Station is full and RV queuing is off */
					trackdirs = TRACKDIR_BIT_NONE;
				}
			}
		}
	}
	/* The above lookups should be moved to GetTileTrackStatus in the
	 * future, but that requires more changes to the pathfinder and other
	 * stuff, probably even more arguments to GTTS.
	 */

	/* Remove tracks unreachable from the enter dir */
	trackdirs &= _road_enter_dir_to_reachable_trackdirs[enterdir];
	if (trackdirs == TRACKDIR_BIT_NONE) {
		/* No reachable tracks, so we'll reverse */
		return_track(_road_reverse_table[enterdir]);
	}

	if (v->reverse_ctr != 0) {
		bool reverse = true;
		if (v->roadtype == ROADTYPE_TRAM) {
			/* Trams may only reverse on a tile if it contains at least the straight
			 * trackbits or when it is a valid turning tile (i.e. one roadbit) */
			RoadBits rb = GetAnyRoadBits(tile, ROADTYPE_TRAM);
			RoadBits straight = AxisToRoadBits(DiagDirToAxis(enterdir));
			reverse = ((rb & straight) == straight) ||
			          (rb == DiagDirToRoadBits(enterdir));
		}
		if (reverse) {
			v->reverse_ctr = 0;
			if (v->tile != tile) {
				return_track(_road_reverse_table[enterdir]);
			}
		}
	}

	desttile = v->dest_tile;
	if (desttile == 0) {
		/* We've got no destination, pick a random track */
		return_track(PickRandomBit(trackdirs));
	}

	/* Only one track to choose between? */
	if (KillFirstBit(trackdirs) == TRACKDIR_BIT_NONE) {
		return_track(FindFirstBit2x64(trackdirs));
	}

	switch (_settings_game.pf.pathfinder_for_roadvehs) {
		case VPF_NPF:  best_track = NPFRoadVehicleChooseTrack(v, tile, enterdir, trackdirs, path_found); break;
		case VPF_YAPF: best_track = YapfRoadVehicleChooseTrack(v, tile, enterdir, trackdirs, path_found); break;

		default: NOT_REACHED();
	}
	v->HandlePathfindingResult(path_found);

found_best_track:;

	if (HasBit(red_signals, best_track)) return INVALID_TRACKDIR;

	return best_track;
}

struct RoadDriveEntry {
	byte x, y;
};

#include "table/roadveh_movement.h"

static bool RoadVehLeaveDepot(RoadVehicle *v, bool first)
{
	/* Don't leave if not all the wagons are in the depot. */
	for (const RoadVehicle *u = v; u != NULL; u = u->Next()) {
		if (u->state != RVSB_IN_DEPOT || u->tile != v->tile) return false;
	}

	DiagDirection dir = GetRoadDepotDirection(v->tile);
	v->direction = DiagDirToDir(dir);

	Trackdir tdir = _roadveh_depot_exit_trackdir[dir];
	const RoadDriveEntry *rdp = _road_drive_data[v->roadtype][(_settings_game.vehicle.road_side << RVS_DRIVE_SIDE) + tdir];

	int x = TileX(v->tile) * TILE_SIZE + (rdp[RVC_DEPOT_START_FRAME].x & 0xF);
	int y = TileY(v->tile) * TILE_SIZE + (rdp[RVC_DEPOT_START_FRAME].y & 0xF);

	if (first) {
		if (RoadVehFindCloseTo(v, x, y, v->direction, false) != NULL) return true;

		VehicleServiceInDepot(v);

		StartRoadVehSound(v);

		/* Vehicle is about to leave a depot */
		v->cur_speed = 0;
	}

	v->vehstatus &= ~VS_HIDDEN;
	v->state = tdir;
	v->frame = RVC_DEPOT_START_FRAME;

	v->x_pos = x;
	v->y_pos = y;
	v->UpdateInclination(true, true);

	InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);

	return true;
}

static Trackdir FollowPreviousRoadVehicle(const RoadVehicle *v, const RoadVehicle *prev, TileIndex tile, DiagDirection entry_dir, bool already_reversed)
{
	if (prev->tile == v->tile && !already_reversed) {
		/* If the previous vehicle is on the same tile as this vehicle is
		 * then it must have reversed. */
		return _road_reverse_table[entry_dir];
	}

	byte prev_state = prev->state;
	Trackdir dir;

	if (prev_state == RVSB_WORMHOLE || prev_state == RVSB_IN_DEPOT) {
		DiagDirection diag_dir = INVALID_DIAGDIR;

		if (IsTileType(tile, MP_TUNNELBRIDGE)) {
			diag_dir = GetTunnelBridgeDirection(tile);
		} else if (IsRoadDepotTile(tile)) {
			diag_dir = ReverseDiagDir(GetRoadDepotDirection(tile));
		}

		if (diag_dir == INVALID_DIAGDIR) return INVALID_TRACKDIR;
		dir = DiagDirToDiagTrackdir(diag_dir);
	} else {
		if (already_reversed && prev->tile != tile) {
			/*
			 * The vehicle has reversed, but did not go straight back.
			 * It immediately turn onto another tile. This means that
			 * the roadstate of the previous vehicle cannot be used
			 * as the direction we have to go with this vehicle.
			 *
			 * Next table is build in the following way:
			 *  - first row for when the vehicle in front went to the northern or
			 *    western tile, second for southern and eastern.
			 *  - columns represent the entry direction.
			 *  - cell values are determined by the Trackdir one has to take from
			 *    the entry dir (column) to the tile in north or south by only
			 *    going over the trackdirs used for turning 90 degrees, i.e.
			 *    TRACKDIR_{UPPER,RIGHT,LOWER,LEFT}_{N,E,S,W}.
			 */
			static const Trackdir reversed_turn_lookup[2][DIAGDIR_END] = {
				{ TRACKDIR_UPPER_W, TRACKDIR_RIGHT_N, TRACKDIR_LEFT_N,  TRACKDIR_UPPER_E },
				{ TRACKDIR_RIGHT_S, TRACKDIR_LOWER_W, TRACKDIR_LOWER_E, TRACKDIR_LEFT_S  }};
			dir = reversed_turn_lookup[prev->tile < tile ? 0 : 1][ReverseDiagDir(entry_dir)];
		} else if (HasBit(prev_state, RVS_IN_DT_ROAD_STOP)) {
			dir = (Trackdir)(prev_state & RVSB_ROAD_STOP_TRACKDIR_MASK);
		} else if (prev_state < TRACKDIR_END) {
			dir = (Trackdir)prev_state;
		} else {
			return INVALID_TRACKDIR;
		}
	}

	/* Do some sanity checking. */
	static const RoadBits required_roadbits[] = {
		ROAD_X,            ROAD_Y,            ROAD_NW | ROAD_NE, ROAD_SW | ROAD_SE,
		ROAD_NW | ROAD_SW, ROAD_NE | ROAD_SE, ROAD_X,            ROAD_Y
	};
	RoadBits required = required_roadbits[dir & 0x07];

	if ((required & GetAnyRoadBits(tile, v->roadtype, true)) == ROAD_NONE) {
		dir = INVALID_TRACKDIR;
	}

	return dir;
}

/**
 * Can a tram track build without destruction on the given tile?
 * @param c the company that would be building the tram tracks
 * @param t the tile to build on.
 * @param r the road bits needed.
 * @return true when a track track can be build on 't'
 */
static bool CanBuildTramTrackOnTile(CompanyID c, TileIndex t, RoadBits r)
{
	/* The 'current' company is not necessarily the owner of the vehicle. */
	Backup<CompanyByte> cur_company(_current_company, c, FILE_LINE);

	CommandCost ret = DoCommand(t, ROADTYPE_TRAM << 4 | r, 0, DC_NONE, CMD_BUILD_ROAD);

	cur_company.Restore();
	return ret.Succeeded();
}

static bool IndividualRoadVehicleController(RoadVehicle *v, const RoadVehicle *prev)
{
	if (v->overtaking != 0)  {
		if (IsTileType(v->tile, MP_STATION)) {
			/* Force us to be not overtaking! */
			v->overtaking = 0;
		} else if (++v->overtaking_ctr >= 35) {
			/* If overtaking just aborts at a random moment, we can have a out-of-bound problem,
			 *  if the vehicle started a corner. To protect that, only allow an abort of
			 *  overtake if we are on straight roads */
			if (v->state < RVSB_IN_ROAD_STOP && IsStraightRoadTrackdir((Trackdir)v->state)) {
				v->overtaking = 0;
			}
		}
	}

	/* If this vehicle is in a depot and we've reached this point it must be
	 * one of the articulated parts. It will stay in the depot until activated
	 * by the previous vehicle in the chain when it gets to the right place. */
	if (v->IsInDepot()) return true;

	if (v->state == RVSB_WORMHOLE) {
		/* Vehicle is entering a depot or is on a bridge or in a tunnel */
		GetNewVehiclePosResult gp = GetNewVehiclePos(v);

		/* Apply bridge speed limit */
		if (!(v->vehstatus & VS_HIDDEN)) {
			RoadVehicle *first = v->First();
			first->cur_speed = min(first->cur_speed, GetBridgeSpec(GetBridgeType(v->tile))->speed * 2);
		}

		if (v->IsFrontEngine()) {
			const Vehicle *u = RoadVehFindCloseTo(v, gp.x, gp.y, v->direction);
			if (u != NULL) {
				v->cur_speed = u->First()->cur_speed;
				return false;
			}
		}

		if (IsTileType(gp.new_tile, MP_TUNNELBRIDGE) && HasBit(VehicleEnterTile(v, gp.new_tile, gp.x, gp.y), VETS_ENTERED_WORMHOLE)) {
			/* Vehicle has just entered a bridge or tunnel */
			v->x_pos = gp.x;
			v->y_pos = gp.y;
			v->UpdateInclination(true, true);
			return true;
		}

		v->x_pos = gp.x;
		v->y_pos = gp.y;
		VehicleMove(v, !(v->vehstatus & VS_HIDDEN));
		return true;
	}

	/* Get move position data for next frame.
	 * For a drive-through road stop use 'straight road' move data.
	 * In this case v->state is masked to give the road stop entry direction. */
	RoadDriveEntry rd = _road_drive_data[v->roadtype][(
		(HasBit(v->state, RVS_IN_DT_ROAD_STOP) ? v->state & RVSB_ROAD_STOP_TRACKDIR_MASK : v->state) +
		(_settings_game.vehicle.road_side << RVS_DRIVE_SIDE)) ^ v->overtaking][v->frame + 1];

	if (rd.x & RDE_NEXT_TILE) {
		TileIndex tile = v->tile + TileOffsByDiagDir((DiagDirection)(rd.x & 3));
		Trackdir dir;

		if (v->IsFrontEngine()) {
			/* If this is the front engine, look for the right path. */
			dir = RoadFindPathToDest(v, tile, (DiagDirection)(rd.x & 3));
		} else {
			dir = FollowPreviousRoadVehicle(v, prev, tile, (DiagDirection)(rd.x & 3), false);
		}

		if (dir == INVALID_TRACKDIR) {
			if (!v->IsFrontEngine()) error("Disconnecting road vehicle.");
			v->cur_speed = 0;
			return false;
		}

again:
		uint start_frame = RVC_DEFAULT_START_FRAME;
		if (IsReversingRoadTrackdir(dir)) {
			/* Turning around */
			if (v->roadtype == ROADTYPE_TRAM) {
				/* Determine the road bits the tram needs to be able to turn around
				 * using the 'big' corner loop. */
				RoadBits needed;
				switch (dir) {
					default: NOT_REACHED();
					case TRACKDIR_RVREV_NE: needed = ROAD_SW; break;
					case TRACKDIR_RVREV_SE: needed = ROAD_NW; break;
					case TRACKDIR_RVREV_SW: needed = ROAD_NE; break;
					case TRACKDIR_RVREV_NW: needed = ROAD_SE; break;
				}
				if ((v->Previous() != NULL && v->Previous()->tile == tile) ||
						(v->IsFrontEngine() && IsNormalRoadTile(tile) && !HasRoadWorks(tile) &&
							(needed & GetRoadBits(tile, ROADTYPE_TRAM)) != ROAD_NONE)) {
					/*
					 * Taking the 'big' corner for trams only happens when:
					 * - The previous vehicle in this (articulated) tram chain is
					 *   already on the 'next' tile, we just follow them regardless of
					 *   anything. When it is NOT on the 'next' tile, the tram started
					 *   doing a reversing turn when the piece of tram track on the next
					 *   tile did not exist yet. Do not use the big tram loop as that is
					 *   going to cause the tram to split up.
					 * - Or the front of the tram can drive over the next tile.
					 */
				} else if (!v->IsFrontEngine() || !CanBuildTramTrackOnTile(v->owner, tile, needed) || ((~needed & GetAnyRoadBits(v->tile, ROADTYPE_TRAM, false)) == ROAD_NONE)) {
					/*
					 * Taking the 'small' corner for trams only happens when:
					 * - We are not the from vehicle of an articulated tram.
					 * - Or when the company cannot build on the next tile.
					 *
					 * The 'small' corner means that the vehicle is on the end of a
					 * tram track and needs to start turning there. To do this properly
					 * the tram needs to start at an offset in the tram turning 'code'
					 * for 'big' corners. It furthermore does not go to the next tile,
					 * so that needs to be fixed too.
					 */
					tile = v->tile;
					start_frame = RVC_TURN_AROUND_START_FRAME_SHORT_TRAM;
				} else {
					/* The company can build on the next tile, so wait till (s)he does. */
					v->cur_speed = 0;
					return false;
				}
			} else if (IsNormalRoadTile(v->tile) && GetDisallowedRoadDirections(v->tile) != DRD_NONE) {
				v->cur_speed = 0;
				return false;
			} else {
				tile = v->tile;
			}
		}

		/* Get position data for first frame on the new tile */
		const RoadDriveEntry *rdp = _road_drive_data[v->roadtype][(dir + (_settings_game.vehicle.road_side << RVS_DRIVE_SIDE)) ^ v->overtaking];

		int x = TileX(tile) * TILE_SIZE + rdp[start_frame].x;
		int y = TileY(tile) * TILE_SIZE + rdp[start_frame].y;

		Direction new_dir = RoadVehGetSlidingDirection(v, x, y);
		if (v->IsFrontEngine()) {
			Vehicle *u = RoadVehFindCloseTo(v, x, y, new_dir);
			if (u != NULL) {
				v->cur_speed = u->First()->cur_speed;
				return false;
			}
		}

		uint32 r = VehicleEnterTile(v, tile, x, y);
		if (HasBit(r, VETS_CANNOT_ENTER)) {
			if (!IsTileType(tile, MP_TUNNELBRIDGE)) {
				v->cur_speed = 0;
				return false;
			}
			/* Try an about turn to re-enter the previous tile */
			dir = _road_reverse_table[rd.x & 3];
			goto again;
		}

		if (IsInsideMM(v->state, RVSB_IN_ROAD_STOP, RVSB_IN_DT_ROAD_STOP_END) && IsTileType(v->tile, MP_STATION)) {
			if (IsReversingRoadTrackdir(dir) && IsInsideMM(v->state, RVSB_IN_ROAD_STOP, RVSB_IN_ROAD_STOP_END)) {
				/* New direction is trying to turn vehicle around.
				 * We can't turn at the exit of a road stop so wait.*/
				v->cur_speed = 0;
				return false;
			}

			/* If we are a drive through road stop and the next tile is of
			 * the same road stop and the next tile isn't this one (i.e. we
			 * are not reversing), then keep the reservation and state.
			 * This way we will not be shortly unregister from the road
			 * stop. It also makes it possible to load when on the edge of
			 * two road stops; otherwise you could get vehicles that should
			 * be loading but are not actually loading. */
			if (IsDriveThroughStopTile(v->tile) &&
					RoadStop::IsDriveThroughRoadStopContinuation(v->tile, tile) &&
					v->tile != tile) {
				/* So, keep 'our' state */
				dir = (Trackdir)v->state;
			} else if (IsRoadStop(v->tile)) {
				/* We're not continuing our drive through road stop, so leave. */
				RoadStop::GetByTile(v->tile, GetRoadStopType(v->tile))->Leave(v);
			}
		}

		if (!HasBit(r, VETS_ENTERED_WORMHOLE)) {
			v->tile = tile;
			v->state = (byte)dir;
			v->frame = start_frame;
		}
		if (new_dir != v->direction) {
			v->direction = new_dir;
			if (_settings_game.vehicle.roadveh_acceleration_model == AM_ORIGINAL) v->cur_speed -= v->cur_speed >> 2;
		}
		v->x_pos = x;
		v->y_pos = y;
		RoadZPosAffectSpeed(v, v->UpdateInclination(true, true));
		return true;
	}

	if (rd.x & RDE_TURNED) {
		/* Vehicle has finished turning around, it will now head back onto the same tile */
		Trackdir dir;
		uint turn_around_start_frame = RVC_TURN_AROUND_START_FRAME;

		if (v->roadtype == ROADTYPE_TRAM && !IsRoadDepotTile(v->tile) && HasExactlyOneBit(GetAnyRoadBits(v->tile, ROADTYPE_TRAM, true))) {
			/*
			 * The tram is turning around with one tram 'roadbit'. This means that
			 * it is using the 'big' corner 'drive data'. However, to support the
			 * trams to take a small corner, there is a 'turned' marker in the middle
			 * of the turning 'drive data'. When the tram took the long corner, we
			 * will still use the 'big' corner drive data, but we advance it one
			 * frame. We furthermore set the driving direction so the turning is
			 * going to be properly shown.
			 */
			turn_around_start_frame = RVC_START_FRAME_AFTER_LONG_TRAM;
			switch (rd.x & 0x3) {
				default: NOT_REACHED();
				case DIAGDIR_NW: dir = TRACKDIR_RVREV_SE; break;
				case DIAGDIR_NE: dir = TRACKDIR_RVREV_SW; break;
				case DIAGDIR_SE: dir = TRACKDIR_RVREV_NW; break;
				case DIAGDIR_SW: dir = TRACKDIR_RVREV_NE; break;
			}
		} else {
			if (v->IsFrontEngine()) {
				/* If this is the front engine, look for the right path. */
				dir = RoadFindPathToDest(v, v->tile, (DiagDirection)(rd.x & 3));
			} else {
				dir = FollowPreviousRoadVehicle(v, prev, v->tile, (DiagDirection)(rd.x & 3), true);
			}
		}

		if (dir == INVALID_TRACKDIR) {
			v->cur_speed = 0;
			return false;
		}

		const RoadDriveEntry *rdp = _road_drive_data[v->roadtype][(_settings_game.vehicle.road_side << RVS_DRIVE_SIDE) + dir];

		int x = TileX(v->tile) * TILE_SIZE + rdp[turn_around_start_frame].x;
		int y = TileY(v->tile) * TILE_SIZE + rdp[turn_around_start_frame].y;

		Direction new_dir = RoadVehGetSlidingDirection(v, x, y);
		if (v->IsFrontEngine() && RoadVehFindCloseTo(v, x, y, new_dir) != NULL) return false;

		uint32 r = VehicleEnterTile(v, v->tile, x, y);
		if (HasBit(r, VETS_CANNOT_ENTER)) {
			v->cur_speed = 0;
			return false;
		}

		v->state = dir;
		v->frame = turn_around_start_frame;

		if (new_dir != v->direction) {
			v->direction = new_dir;
			if (_settings_game.vehicle.roadveh_acceleration_model == AM_ORIGINAL) v->cur_speed -= v->cur_speed >> 2;
		}

		v->x_pos = x;
		v->y_pos = y;
		RoadZPosAffectSpeed(v, v->UpdateInclination(true, true));
		return true;
	}

	/* This vehicle is not in a wormhole and it hasn't entered a new tile. If
	 * it's on a depot tile, check if it's time to activate the next vehicle in
	 * the chain yet. */
	if (v->Next() != NULL && IsRoadDepotTile(v->tile)) {
		if (v->frame == v->gcache.cached_veh_length + RVC_DEPOT_START_FRAME) {
			RoadVehLeaveDepot(v->Next(), false);
		}
	}

	/* Calculate new position for the vehicle */
	int x = (v->x_pos & ~15) + (rd.x & 15);
	int y = (v->y_pos & ~15) + (rd.y & 15);

	Direction new_dir = RoadVehGetSlidingDirection(v, x, y);

	if (v->IsFrontEngine() && !IsInsideMM(v->state, RVSB_IN_ROAD_STOP, RVSB_IN_ROAD_STOP_END)) {
		/* Vehicle is not in a road stop.
		 * Check for another vehicle to overtake */
		RoadVehicle *u = RoadVehFindCloseTo(v, x, y, new_dir);

		if (u != NULL) {
			u = u->First();
			/* There is a vehicle in front overtake it if possible */
			if (v->overtaking == 0) RoadVehCheckOvertake(v, u);
			if (v->overtaking == 0) v->cur_speed = u->cur_speed;

			/* In case an RV is stopped in a road stop, why not try to load? */
			if (v->cur_speed == 0 && IsInsideMM(v->state, RVSB_IN_DT_ROAD_STOP, RVSB_IN_DT_ROAD_STOP_END) &&
					v->current_order.ShouldStopAtStation(v, GetStationIndex(v->tile)) &&
					v->owner == GetTileOwner(v->tile) && !v->current_order.IsType(OT_LEAVESTATION) &&
					GetRoadStopType(v->tile) == (v->IsBus() ? ROADSTOP_BUS : ROADSTOP_TRUCK)) {
				Station *st = Station::GetByTile(v->tile);
				v->last_station_visited = st->index;
				RoadVehArrivesAt(v, st);
				v->BeginLoading();
			}
			return false;
		}
	}

	Direction old_dir = v->direction;
	if (new_dir != old_dir) {
		v->direction = new_dir;
		if (_settings_game.vehicle.roadveh_acceleration_model == AM_ORIGINAL) v->cur_speed -= v->cur_speed >> 2;
		if (old_dir != v->state) {
			/* The vehicle is in a road stop */
			v->UpdateInclination(false, true);
			/* Note, return here means that the frame counter is not incremented
			 * for vehicles changing direction in a road stop. This causes frames to
			 * be repeated. (XXX) Is this intended? */
			return true;
		}
	}

	/* If the vehicle is in a normal road stop and the frame equals the stop frame OR
	 * if the vehicle is in a drive-through road stop and this is the destination station
	 * and it's the correct type of stop (bus or truck) and the frame equals the stop frame...
	 * (the station test and stop type test ensure that other vehicles, using the road stop as
	 * a through route, do not stop) */
	if (v->IsFrontEngine() && ((IsInsideMM(v->state, RVSB_IN_ROAD_STOP, RVSB_IN_ROAD_STOP_END) &&
			_road_stop_stop_frame[v->state - RVSB_IN_ROAD_STOP + (_settings_game.vehicle.road_side << RVS_DRIVE_SIDE)] == v->frame) ||
			(IsInsideMM(v->state, RVSB_IN_DT_ROAD_STOP, RVSB_IN_DT_ROAD_STOP_END) &&
			v->current_order.ShouldStopAtStation(v, GetStationIndex(v->tile)) &&
			v->owner == GetTileOwner(v->tile) &&
			GetRoadStopType(v->tile) == (v->IsBus() ? ROADSTOP_BUS : ROADSTOP_TRUCK) &&
			v->frame == RVC_DRIVE_THROUGH_STOP_FRAME))) {

		RoadStop *rs = RoadStop::GetByTile(v->tile, GetRoadStopType(v->tile));
		Station *st = Station::GetByTile(v->tile);

		/* Vehicle is at the stop position (at a bay) in a road stop.
		 * Note, if vehicle is loading/unloading it has already been handled,
		 * so if we get here the vehicle has just arrived or is just ready to leave. */
		if (!HasBit(v->state, RVS_ENTERED_STOP)) {
			/* Vehicle has arrived at a bay in a road stop */

			if (IsDriveThroughStopTile(v->tile)) {
				TileIndex next_tile = TILE_ADD(v->tile, TileOffsByDir(v->direction));

				/* Check if next inline bay is free and has compatible road. */
				if (RoadStop::IsDriveThroughRoadStopContinuation(v->tile, next_tile) && (GetRoadTypes(next_tile) & v->compatible_roadtypes) != 0) {
					v->frame++;
					v->x_pos = x;
					v->y_pos = y;
					RoadZPosAffectSpeed(v, v->UpdateInclination(true, false));
					return true;
				}
			}

			rs->SetEntranceBusy(false);
			SetBit(v->state, RVS_ENTERED_STOP);

			v->last_station_visited = st->index;

			if (IsDriveThroughStopTile(v->tile) || (v->current_order.IsType(OT_GOTO_STATION) && v->current_order.GetDestination() == st->index)) {
				RoadVehArrivesAt(v, st);
				v->BeginLoading();
				return false;
			}
		} else {
			/* Vehicle is ready to leave a bay in a road stop */
			if (rs->IsEntranceBusy()) {
				/* Road stop entrance is busy, so wait as there is nowhere else to go */
				v->cur_speed = 0;
				return false;
			}
			if (v->current_order.IsType(OT_LEAVESTATION)) v->current_order.Free();
		}

		if (IsStandardRoadStopTile(v->tile)) rs->SetEntranceBusy(true);

		StartRoadVehSound(v);
		SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
	}

	/* Check tile position conditions - i.e. stop position in depot,
	 * entry onto bridge or into tunnel */
	uint32 r = VehicleEnterTile(v, v->tile, x, y);
	if (HasBit(r, VETS_CANNOT_ENTER)) {
		v->cur_speed = 0;
		return false;
	}

	if (v->current_order.IsType(OT_LEAVESTATION) && IsDriveThroughStopTile(v->tile)) {
		v->current_order.Free();
	}

	/* Move to next frame unless vehicle arrived at a stop position
	 * in a depot or entered a tunnel/bridge */
	if (!HasBit(r, VETS_ENTERED_WORMHOLE)) v->frame++;
	v->x_pos = x;
	v->y_pos = y;
	RoadZPosAffectSpeed(v, v->UpdateInclination(false, true));
	return true;
}

static bool RoadVehController(RoadVehicle *v)
{
	/* decrease counters */
	v->tick_counter++;
	v->current_order_time++;
	if (v->reverse_ctr != 0) v->reverse_ctr--;

	/* handle crashed */
	if (v->vehstatus & VS_CRASHED || RoadVehCheckTrainCrash(v)) {
		return RoadVehIsCrashed(v);
	}

	/* road vehicle has broken down? */
	if (v->HandleBreakdown()) return true;
	if (v->vehstatus & VS_STOPPED) return true;

	ProcessOrders(v);
	v->HandleLoading();

	if (v->current_order.IsType(OT_LOADING)) return true;

	if (v->IsInDepot() && RoadVehLeaveDepot(v, true)) return true;

	v->ShowVisualEffect();

	/* Check how far the vehicle needs to proceed */
	int j = v->UpdateSpeed();

	int adv_spd = v->GetAdvanceDistance();
	bool blocked = false;
	while (j >= adv_spd) {
		j -= adv_spd;

		RoadVehicle *u = v;
		for (RoadVehicle *prev = NULL; u != NULL; prev = u, u = u->Next()) {
			if (!IndividualRoadVehicleController(u, prev)) {
				blocked = true;
				break;
			}
		}
		if (blocked) break;

		/* Determine distance to next map position */
		adv_spd = v->GetAdvanceDistance();

		/* Test for a collision, but only if another movement will occur. */
		if (j >= adv_spd && RoadVehCheckTrainCrash(v)) break;
	}

	v->SetLastSpeed();

	for (RoadVehicle *u = v; u != NULL; u = u->Next()) {
		if ((u->vehstatus & VS_HIDDEN) != 0) continue;

		u->UpdateViewport(false, false);
	}

	/* If movement is blocked, set 'progress' to its maximum, so the roadvehicle does
	 * not accelerate again before it can actually move. I.e. make sure it tries to advance again
	 * on next tick to discover whether it is still blocked. */
	if (v->progress == 0) v->progress = blocked ? adv_spd - 1 : j;

	return true;
}

Money RoadVehicle::GetRunningCost() const
{
	const Engine *e = Engine::Get(this->engine_type);
	if (e->u.road.running_cost_class == INVALID_PRICE) return 0;

	uint cost_factor = GetVehicleProperty(this, PROP_ROADVEH_RUNNING_COST_FACTOR, e->u.road.running_cost);
	if (cost_factor == 0) return 0;

	return GetPrice(e->u.road.running_cost_class, cost_factor, e->grf_prop.grffile);
}

bool RoadVehicle::Tick()
{
	if (this->IsFrontEngine()) {
		if (!(this->vehstatus & VS_STOPPED)) this->running_ticks++;
		return RoadVehController(this);
	}

	return true;
}

static void CheckIfRoadVehNeedsService(RoadVehicle *v)
{
	/* If we already got a slot at a stop, use that FIRST, and go to a depot later */
	if (Company::Get(v->owner)->settings.vehicle.servint_roadveh == 0 || !v->NeedsAutomaticServicing()) return;
	if (v->IsInDepot()) {
		VehicleServiceInDepot(v);
		return;
	}

	uint max_penalty;
	switch (_settings_game.pf.pathfinder_for_roadvehs) {
		case VPF_NPF:  max_penalty = _settings_game.pf.npf.maximum_go_to_depot_penalty;  break;
		case VPF_YAPF: max_penalty = _settings_game.pf.yapf.maximum_go_to_depot_penalty; break;
		default: NOT_REACHED();
	}

	FindDepotData rfdd = FindClosestRoadDepot(v, max_penalty);
	/* Only go to the depot if it is not too far out of our way. */
	if (rfdd.best_length == UINT_MAX || rfdd.best_length > max_penalty) {
		if (v->current_order.IsType(OT_GOTO_DEPOT)) {
			/* If we were already heading for a depot but it has
			 * suddenly moved farther away, we continue our normal
			 * schedule? */
			v->current_order.MakeDummy();
			SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
		}
		return;
	}

	DepotID depot = GetDepotIndex(rfdd.tile);

	if (v->current_order.IsType(OT_GOTO_DEPOT) &&
			v->current_order.GetNonStopType() & ONSF_NO_STOP_AT_INTERMEDIATE_STATIONS &&
			!Chance16(1, 20)) {
		return;
	}

	v->current_order.MakeGoToDepot(depot, ODTFB_SERVICE);
	v->dest_tile = rfdd.tile;
	SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
}

void RoadVehicle::OnNewDay()
{
	if (!this->IsFrontEngine()) return;

	if ((++this->day_counter & 7) == 0) DecreaseVehicleValue(this);
	if (this->blocked_ctr == 0) CheckVehicleBreakdown(this);

	AgeVehicle(this);
	CheckIfRoadVehNeedsService(this);

	CheckOrders(this);

	if (this->running_ticks == 0) return;

	CommandCost cost(EXPENSES_ROADVEH_RUN, this->GetRunningCost() * this->running_ticks / (DAYS_IN_YEAR * DAY_TICKS));

	this->profit_this_year -= cost.GetCost();
	this->running_ticks = 0;

	SubtractMoneyFromCompanyFract(this->owner, cost);

	SetWindowDirty(WC_VEHICLE_DETAILS, this->index);
	SetWindowClassesDirty(WC_ROADVEH_LIST);
}

Trackdir RoadVehicle::GetVehicleTrackdir() const
{
	if (this->vehstatus & VS_CRASHED) return INVALID_TRACKDIR;

	if (this->IsInDepot()) {
		/* We'll assume the road vehicle is facing outwards */
		return DiagDirToDiagTrackdir(GetRoadDepotDirection(this->tile));
	}

	if (IsStandardRoadStopTile(this->tile)) {
		/* We'll assume the road vehicle is facing outwards */
		return DiagDirToDiagTrackdir(GetRoadStopDir(this->tile)); // Road vehicle in a station
	}

	/* Drive through road stops / wormholes (tunnels) */
	if (this->state > RVSB_TRACKDIR_MASK) return DiagDirToDiagTrackdir(DirToDiagDir(this->direction));

	/* If vehicle's state is a valid track direction (vehicle is not turning around) return it,
	 * otherwise transform it into a valid track direction */
	return (Trackdir)((IsReversingRoadTrackdir((Trackdir)this->state)) ? (this->state - 6) : this->state);
}
