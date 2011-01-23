/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file train_cmd.cpp Handling of trains. */

#include "stdafx.h"
#include "gui.h"
#include "articulated_vehicles.h"
#include "command_func.h"
#include "pathfinder/npf/npf_func.h"
#include "pathfinder/yapf/yapf.hpp"
#include "news_func.h"
#include "company_func.h"
#include "vehicle_gui.h"
#include "newgrf_sound.h"
#include "newgrf_text.h"
#include "group.h"
#include "strings_func.h"
#include "functions.h"
#include "window_func.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "ai/ai.hpp"
#include "newgrf_station.h"
#include "effectvehicle_func.h"
#include "gamelog.h"
#include "network/network.h"
#include "spritecache.h"
#include "core/random_func.hpp"
#include "company_base.h"
#include "newgrf.h"
#include "order_backup.h"

#include "table/strings.h"
#include "table/train_cmd.h"

static Track ChooseTrainTrack(Train *v, TileIndex tile, DiagDirection enterdir, TrackBits tracks, bool force_res, bool *got_reservation, bool mark_stuck);
static bool TrainCheckIfLineEnds(Train *v);
static void TrainController(Train *v, Vehicle *nomove);
static TileIndex TrainApproachingCrossingTile(const Train *v);
static void CheckIfTrainNeedsService(Train *v);
static void CheckNextTrainTile(Train *v);

static const byte _vehicle_initial_x_fract[4] = {10, 8, 4,  8};
static const byte _vehicle_initial_y_fract[4] = { 8, 4, 8, 10};


/**
 * Determine the side in which the train will leave the tile
 *
 * @param direction vehicle direction
 * @param track vehicle track bits
 * @return side of tile the train will leave
 */
static inline DiagDirection TrainExitDir(Direction direction, TrackBits track)
{
	static const TrackBits state_dir_table[DIAGDIR_END] = { TRACK_BIT_RIGHT, TRACK_BIT_LOWER, TRACK_BIT_LEFT, TRACK_BIT_UPPER };

	DiagDirection diagdir = DirToDiagDir(direction);

	/* Determine the diagonal direction in which we will exit this tile */
	if (!HasBit(direction, 0) && track != state_dir_table[diagdir]) {
		diagdir = ChangeDiagDir(diagdir, DIAGDIRDIFF_90LEFT);
	}

	return diagdir;
}


/**
 * Return the cargo weight multiplier to use for a rail vehicle
 * @param cargo Cargo type to get multiplier for
 * @return Cargo weight multiplier
 */
byte FreightWagonMult(CargoID cargo)
{
	if (!CargoSpec::Get(cargo)->is_freight) return 1;
	return _settings_game.vehicle.freight_trains;
}

/**
 * Logs a bug in GRF and shows a warning message if this
 * is for the first time this happened.
 * @param u first vehicle of chain
 */
static void RailVehicleLengthChanged(const Train *u)
{
	/* show a warning once for each engine in whole game and once for each GRF after each game load */
	const Engine *engine = Engine::Get(u->engine_type);
	uint32 grfid = engine->grf_prop.grffile->grfid;
	GRFConfig *grfconfig = GetGRFConfig(grfid);
	if (GamelogGRFBugReverse(grfid, engine->grf_prop.local_id) || !HasBit(grfconfig->grf_bugs, GBUG_VEH_LENGTH)) {
		ShowNewGrfVehicleError(u->engine_type, STR_NEWGRF_BROKEN, STR_NEWGRF_BROKEN_VEHICLE_LENGTH, GBUG_VEH_LENGTH, true);
	}
}

/** Checks if lengths of all rail vehicles are valid. If not, shows an error message. */
void CheckTrainsLengths()
{
	const Train *v;
	bool first = true;

	FOR_ALL_TRAINS(v) {
		if (v->First() == v && !(v->vehstatus & VS_CRASHED)) {
			for (const Train *u = v, *w = v->Next(); w != NULL; u = w, w = w->Next()) {
				if (u->track != TRACK_BIT_DEPOT) {
					if ((w->track != TRACK_BIT_DEPOT &&
							max(abs(u->x_pos - w->x_pos), abs(u->y_pos - w->y_pos)) != u->gcache.cached_veh_length) ||
							(w->track == TRACK_BIT_DEPOT && TicksToLeaveDepot(u) <= 0)) {
						SetDParam(0, v->index);
						SetDParam(1, v->owner);
						ShowErrorMessage(STR_BROKEN_VEHICLE_LENGTH, INVALID_STRING_ID, WL_CRITICAL);

						if (!_networking && first) {
							first = false;
							DoCommandP(0, PM_PAUSED_ERROR, 1, CMD_PAUSE);
						}
						/* Break so we warn only once for each train. */
						break;
					}
				}
			}
		}
	}
}

/**
 * Update visual effect, power and acceleration caches.
 * Called when a vehicle in the consist enters a different railtype.
 */
void Train::RailtypeChanged()
{
	for (Train *u = this; u != NULL; u = u->Next()) {
		/* The wagon-is-powered-state should not change, so the weight does not change. */
		u->UpdateVisualEffect(false);
	}
	this->PowerChanged();
	if (this->IsFrontEngine()) this->UpdateAcceleration();
}

/**
 * Recalculates the cached stuff of a train. Should be called each time a vehicle is added
 * to/removed from the chain, and when the game is loaded.
 * Note: this needs to be called too for 'wagon chains' (in the depot, without an engine)
 * @param same_length should length of vehicles stay the same?
 */
void Train::ConsistChanged(bool same_length)
{
	uint16 max_speed = UINT16_MAX;

	assert(this->IsFrontEngine() || this->IsFreeWagon());

	const RailVehicleInfo *rvi_v = RailVehInfo(this->engine_type);
	EngineID first_engine = this->IsFrontEngine() ? this->engine_type : INVALID_ENGINE;
	this->gcache.cached_total_length = 0;
	this->compatible_railtypes = RAILTYPES_NONE;

	bool train_can_tilt = true;

	for (Train *u = this; u != NULL; u = u->Next()) {
		const RailVehicleInfo *rvi_u = RailVehInfo(u->engine_type);

		/* Check the this->first cache. */
		assert(u->First() == this);

		/* update the 'first engine' */
		u->gcache.first_engine = this == u ? INVALID_ENGINE : first_engine;
		u->railtype = rvi_u->railtype;

		if (u->IsEngine()) first_engine = u->engine_type;

		/* Set user defined data to its default value */
		u->tcache.user_def_data = rvi_u->user_def_data;
		this->InvalidateNewGRFCache();
		u->InvalidateNewGRFCache();
	}

	for (Train *u = this; u != NULL; u = u->Next()) {
		/* Update user defined data (must be done before other properties) */
		u->tcache.user_def_data = GetVehicleProperty(u, PROP_TRAIN_USER_DATA, u->tcache.user_def_data);
		this->InvalidateNewGRFCache();
		u->InvalidateNewGRFCache();
	}

	for (Train *u = this; u != NULL; u = u->Next()) {
		const Engine *e_u = Engine::Get(u->engine_type);
		const RailVehicleInfo *rvi_u = &e_u->u.rail;

		if (!HasBit(e_u->info.misc_flags, EF_RAIL_TILTS)) train_can_tilt = false;

		/* Cache wagon override sprite group. NULL is returned if there is none */
		u->tcache.cached_override = GetWagonOverrideSpriteSet(u->engine_type, u->cargo_type, u->gcache.first_engine);

		/* Reset colour map */
		u->colourmap = PAL_NONE;

		/* Update powered-wagon-status and visual effect */
		u->UpdateVisualEffect(true);

		if (rvi_v->pow_wag_power != 0 && rvi_u->railveh_type == RAILVEH_WAGON &&
				UsesWagonOverride(u) && !HasBit(u->vcache.cached_vis_effect, VE_DISABLE_WAGON_POWER)) {
			/* wagon is powered */
			SetBit(u->flags, VRF_POWEREDWAGON); // cache 'powered' status
		} else {
			ClrBit(u->flags, VRF_POWEREDWAGON);
		}

		if (!u->IsArticulatedPart()) {
			/* Do not count powered wagons for the compatible railtypes, as wagons always
			   have railtype normal */
			if (rvi_u->power > 0) {
				this->compatible_railtypes |= GetRailTypeInfo(u->railtype)->powered_railtypes;
			}

			/* Some electric engines can be allowed to run on normal rail. It happens to all
			 * existing electric engines when elrails are disabled and then re-enabled */
			if (HasBit(u->flags, VRF_EL_ENGINE_ALLOWED_NORMAL_RAIL)) {
				u->railtype = RAILTYPE_RAIL;
				u->compatible_railtypes |= RAILTYPES_RAIL;
			}

			/* max speed is the minimum of the speed limits of all vehicles in the consist */
			if ((rvi_u->railveh_type != RAILVEH_WAGON || _settings_game.vehicle.wagon_speed_limits) && !UsesWagonOverride(u)) {
				uint16 speed = GetVehicleProperty(u, PROP_TRAIN_SPEED, rvi_u->max_speed);
				if (speed != 0) max_speed = min(speed, max_speed);
			}
		}

		u->cargo_cap = GetVehicleCapacity(u);

		/* check the vehicle length (callback) */
		uint16 veh_len = CALLBACK_FAILED;
		if (HasBit(e_u->info.callback_mask, CBM_VEHICLE_LENGTH)) {
			veh_len = GetVehicleCallback(CBID_VEHICLE_LENGTH, 0, 0, u->engine_type, u);
		}
		if (veh_len == CALLBACK_FAILED) veh_len = rvi_u->shorten_factor;
		veh_len = 8 - Clamp(veh_len, 0, 7);

		/* verify length hasn't changed */
		if (same_length && veh_len != u->gcache.cached_veh_length) RailVehicleLengthChanged(u);

		/* update vehicle length? */
		if (!same_length) u->gcache.cached_veh_length = veh_len;

		this->gcache.cached_total_length += u->gcache.cached_veh_length;
		this->InvalidateNewGRFCache();
		u->InvalidateNewGRFCache();
	}

	/* store consist weight/max speed in cache */
	this->vcache.cached_max_speed = max_speed;
	this->tcache.cached_tilt = train_can_tilt;
	this->tcache.cached_max_curve_speed = this->GetCurveSpeedLimit();

	/* recalculate cached weights and power too (we do this *after* the rest, so it is known which wagons are powered and need extra weight added) */
	this->CargoChanged();

	if (this->IsFrontEngine()) {
		this->UpdateAcceleration();
		SetWindowDirty(WC_VEHICLE_DETAILS, this->index);
		InvalidateWindowData(WC_VEHICLE_REFIT, this->index);
	}
}

/**
 * Get the stop location of (the center) of the front vehicle of a train at
 * a platform of a station.
 * @param station_id     the ID of the station where we're stopping
 * @param tile           the tile where the vehicle currently is
 * @param v              the vehicle to get the stop location of
 * @param station_ahead  'return' the amount of 1/16th tiles in front of the train
 * @param station_length 'return' the station length in 1/16th tiles
 * @return the location, calculated from the begin of the station to stop at.
 */
int GetTrainStopLocation(StationID station_id, TileIndex tile, const Train *v, int *station_ahead, int *station_length)
{
	const Station *st = Station::Get(station_id);
	*station_ahead  = st->GetPlatformLength(tile, DirToDiagDir(v->direction)) * TILE_SIZE;
	*station_length = st->GetPlatformLength(tile) * TILE_SIZE;

	/* Default to the middle of the station for stations stops that are not in
	 * the order list like intermediate stations when non-stop is disabled */
	OrderStopLocation osl = OSL_PLATFORM_MIDDLE;
	if (v->gcache.cached_total_length >= *station_length) {
		/* The train is longer than the station, make it stop at the far end of the platform */
		osl = OSL_PLATFORM_FAR_END;
	} else if (v->current_order.IsType(OT_GOTO_STATION) && v->current_order.GetDestination() == station_id) {
		osl = v->current_order.GetStopLocation();
	}

	/* The stop location of the FRONT! of the train */
	int stop;
	switch (osl) {
		default: NOT_REACHED();

		case OSL_PLATFORM_NEAR_END:
			stop = v->gcache.cached_total_length;
			break;

		case OSL_PLATFORM_MIDDLE:
			stop = *station_length - (*station_length - v->gcache.cached_total_length) / 2;
			break;

		case OSL_PLATFORM_FAR_END:
			stop = *station_length;
			break;
	}

	/* Subtract half the front vehicle length of the train so we get the real
	 * stop location of the train. */
	return stop - (v->gcache.cached_veh_length + 1) / 2;
}


/**
 * Computes train speed limit caused by curves
 * @return imposed speed limit
 */
int Train::GetCurveSpeedLimit() const
{
	assert(this->First() == this);

	static const int absolute_max_speed = UINT16_MAX;
	int max_speed = absolute_max_speed;

	if (_settings_game.vehicle.train_acceleration_model == AM_ORIGINAL) return max_speed;

	int curvecount[2] = {0, 0};

	/* first find the curve speed limit */
	int numcurve = 0;
	int sum = 0;
	int pos = 0;
	int lastpos = -1;
	for (const Vehicle *u = this; u->Next() != NULL; u = u->Next(), pos++) {
		Direction this_dir = u->direction;
		Direction next_dir = u->Next()->direction;

		DirDiff dirdiff = DirDifference(this_dir, next_dir);
		if (dirdiff == DIRDIFF_SAME) continue;

		if (dirdiff == DIRDIFF_45LEFT) curvecount[0]++;
		if (dirdiff == DIRDIFF_45RIGHT) curvecount[1]++;
		if (dirdiff == DIRDIFF_45LEFT || dirdiff == DIRDIFF_45RIGHT) {
			if (lastpos != -1) {
				numcurve++;
				sum += pos - lastpos;
				if (pos - lastpos == 1) {
					max_speed = 88;
				}
			}
			lastpos = pos;
		}

		/* if we have a 90 degree turn, fix the speed limit to 60 */
		if (dirdiff == DIRDIFF_90LEFT || dirdiff == DIRDIFF_90RIGHT) {
			max_speed = 61;
		}
	}

	if (numcurve > 0 && max_speed > 88) {
		if (curvecount[0] == 1 && curvecount[1] == 1) {
			max_speed = absolute_max_speed;
		} else {
			sum /= numcurve;
			max_speed = 232 - (13 - Clamp(sum, 1, 12)) * (13 - Clamp(sum, 1, 12));
		}
	}

	if (max_speed != absolute_max_speed) {
		/* Apply the engine's rail type curve speed advantage, if it slowed by curves */
		const RailtypeInfo *rti = GetRailTypeInfo(this->railtype);
		max_speed += (max_speed / 2) * rti->curve_speed;

		if (this->tcache.cached_tilt) {
			/* Apply max_speed bonus of 20% for a tilting train */
			max_speed += max_speed / 5;
		}
	}

	return max_speed;
}

/**
 * Calculates the maximum speed of the vehicle under its current conditions.
 * @return Maximum speed of the vehicle.
 */
int Train::GetCurrentMaxSpeed() const
{
	int max_speed = this->tcache.cached_max_curve_speed;
	assert(max_speed == this->GetCurveSpeedLimit());

	if (IsRailStationTile(this->tile)) {
		StationID sid = GetStationIndex(this->tile);
		if (this->current_order.ShouldStopAtStation(this, sid)) {
			int station_ahead;
			int station_length;
			int stop_at = GetTrainStopLocation(sid, this->tile, this, &station_ahead, &station_length);

			/* The distance to go is whatever is still ahead of the train minus the
			 * distance from the train's stop location to the end of the platform */
			int distance_to_go = station_ahead / TILE_SIZE - (station_length - stop_at) / TILE_SIZE;

			if (distance_to_go > 0) {
				int st_max_speed = 120;

				int delta_v = this->cur_speed / (distance_to_go + 1);
				if (max_speed > (this->cur_speed - delta_v)) {
					st_max_speed = this->cur_speed - (delta_v / 10);
				}

				st_max_speed = max(st_max_speed, 25 * distance_to_go);
				max_speed = min(max_speed, st_max_speed);
			}
		}
	}

	for (const Train *u = this; u != NULL; u = u->Next()) {
		if (u->track == TRACK_BIT_DEPOT) {
			max_speed = min(max_speed, 61);
			break;
		}
	}

	return min(max_speed, this->gcache.cached_max_track_speed);
}

void Train::UpdateAcceleration()
{
	assert(this->IsFrontEngine());

	uint power = this->gcache.cached_power;
	uint weight = this->gcache.cached_weight;
	assert(weight != 0);
	this->acceleration = Clamp(power / weight * 4, 1, 255);
}

/**
 * Get the width of a train vehicle image in the GUI.
 * @param offset Additional offset for positioning the sprite; set to NULL if not needed
 * @return Width in pixels
 */
int Train::GetDisplayImageWidth(Point *offset) const
{
	int reference_width = TRAININFO_DEFAULT_VEHICLE_WIDTH;
	int vehicle_pitch = 0;

	const Engine *e = Engine::Get(this->engine_type);
	if (e->grf_prop.grffile != NULL && is_custom_sprite(e->u.rail.image_index)) {
		reference_width = e->grf_prop.grffile->traininfo_vehicle_width;
		vehicle_pitch = e->grf_prop.grffile->traininfo_vehicle_pitch;
	}

	if (offset != NULL) {
		offset->x = reference_width / 2;
		offset->y = vehicle_pitch;
	}
	return this->gcache.cached_veh_length * reference_width / 8;
}

static SpriteID GetDefaultTrainSprite(uint8 spritenum, Direction direction)
{
	return ((direction + _engine_sprite_add[spritenum]) & _engine_sprite_and[spritenum]) + _engine_sprite_base[spritenum];
}

SpriteID Train::GetImage(Direction direction) const
{
	uint8 spritenum = this->spritenum;
	SpriteID sprite;

	if (HasBit(this->flags, VRF_REVERSE_DIRECTION)) direction = ReverseDir(direction);

	if (is_custom_sprite(spritenum)) {
		sprite = GetCustomVehicleSprite(this, (Direction)(direction + 4 * IS_CUSTOM_SECONDHEAD_SPRITE(spritenum)));
		if (sprite != 0) return sprite;

		spritenum = Engine::Get(this->engine_type)->original_image_index;
	}

	sprite = GetDefaultTrainSprite(spritenum, direction);

	if (this->cargo.Count() >= this->cargo_cap / 2U) sprite += _wagon_full_adder[spritenum];

	return sprite;
}

static SpriteID GetRailIcon(EngineID engine, bool rear_head, int &y)
{
	const Engine *e = Engine::Get(engine);
	Direction dir = rear_head ? DIR_E : DIR_W;
	uint8 spritenum = e->u.rail.image_index;

	if (is_custom_sprite(spritenum)) {
		SpriteID sprite = GetCustomVehicleIcon(engine, dir);
		if (sprite != 0) {
			if (e->grf_prop.grffile != NULL) {
				y += e->grf_prop.grffile->traininfo_vehicle_pitch;
			}
			return sprite;
		}

		spritenum = Engine::Get(engine)->original_image_index;
	}

	if (rear_head) spritenum++;

	return GetDefaultTrainSprite(spritenum, DIR_W);
}

void DrawTrainEngine(int left, int right, int preferred_x, int y, EngineID engine, PaletteID pal)
{
	if (RailVehInfo(engine)->railveh_type == RAILVEH_MULTIHEAD) {
		int yf = y;
		int yr = y;

		SpriteID spritef = GetRailIcon(engine, false, yf);
		SpriteID spriter = GetRailIcon(engine, true, yr);
		const Sprite *real_spritef = GetSprite(spritef, ST_NORMAL);
		const Sprite *real_spriter = GetSprite(spriter, ST_NORMAL);

		preferred_x = Clamp(preferred_x, left - real_spritef->x_offs + 14, right - real_spriter->width - real_spriter->x_offs - 15);

		DrawSprite(spritef, pal, preferred_x - 14, yf);
		DrawSprite(spriter, pal, preferred_x + 15, yr);
	} else {
		SpriteID sprite = GetRailIcon(engine, false, y);
		const Sprite *real_sprite = GetSprite(sprite, ST_NORMAL);
		preferred_x = Clamp(preferred_x, left - real_sprite->x_offs, right - real_sprite->width - real_sprite->x_offs);
		DrawSprite(sprite, pal, preferred_x, y);
	}
}

/**
 * Build a railroad wagon.
 * @param tile     tile of the depot where rail-vehicle is built.
 * @param flags    type of operation.
 * @param e        the engine to build.
 * @param ret[out] the vehicle that has been built.
 * @return the cost of this operation or an error.
 */
static CommandCost CmdBuildRailWagon(TileIndex tile, DoCommandFlag flags, const Engine *e, Vehicle **ret)
{
	const RailVehicleInfo *rvi = &e->u.rail;

	/* Check that the wagon can drive on the track in question */
	if (!IsCompatibleRail(rvi->railtype, GetRailType(tile))) return CMD_ERROR;

	if (flags & DC_EXEC) {
		Train *v = new Train();
		*ret = v;
		v->spritenum = rvi->image_index;

		v->engine_type = e->index;
		v->gcache.first_engine = INVALID_ENGINE; // needs to be set before first callback

		DiagDirection dir = GetRailDepotDirection(tile);

		v->direction = DiagDirToDir(dir);
		v->tile = tile;

		int x = TileX(tile) * TILE_SIZE | _vehicle_initial_x_fract[dir];
		int y = TileY(tile) * TILE_SIZE | _vehicle_initial_y_fract[dir];

		v->x_pos = x;
		v->y_pos = y;
		v->z_pos = GetSlopeZ(x, y);
		v->owner = _current_company;
		v->track = TRACK_BIT_DEPOT;
		v->vehstatus = VS_HIDDEN | VS_DEFPAL;

		v->SetWagon();

		v->SetFreeWagon();
		InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);

		v->cargo_type = e->GetDefaultCargoType();
		v->cargo_cap = rvi->capacity;

		v->railtype = rvi->railtype;

		v->build_year = _cur_year;
		v->cur_image = SPR_IMG_QUERY;
		v->random_bits = VehicleRandomBits();

		v->group_id = DEFAULT_GROUP;

		AddArticulatedParts(v);

		_new_vehicle_id = v->index;

		VehicleMove(v, false);
		v->First()->ConsistChanged(false);
		UpdateTrainGroupID(v->First());

		CheckConsistencyOfArticulatedVehicle(v);

		/* Try to connect the vehicle to one of free chains of wagons. */
		Train *w;
		FOR_ALL_TRAINS(w) {
			if (w->tile == tile &&              ///< Same depot
					w->IsFreeWagon() &&             ///< A free wagon chain
					w->engine_type == e->index &&   ///< Same type
					w->First() != v &&              ///< Don't connect to ourself
					!(w->vehstatus & VS_CRASHED)) { ///< Not crashed/flooded
				DoCommand(0, v->index | 1 << 20, w->Last()->index, DC_EXEC, CMD_MOVE_RAIL_VEHICLE);
				break;
			}
		}
	}

	return CommandCost();
}

/** Move all free vehicles in the depot to the train */
static void NormalizeTrainVehInDepot(const Train *u)
{
	const Train *v;
	FOR_ALL_TRAINS(v) {
		if (v->IsFreeWagon() && v->tile == u->tile &&
				v->track == TRACK_BIT_DEPOT) {
			if (DoCommand(0, v->index | 1 << 20, u->index, DC_EXEC,
					CMD_MOVE_RAIL_VEHICLE).Failed())
				break;
		}
	}
}

static void AddRearEngineToMultiheadedTrain(Train *v)
{
	Train *u = new Train();
	v->value >>= 1;
	u->value = v->value;
	u->direction = v->direction;
	u->owner = v->owner;
	u->tile = v->tile;
	u->x_pos = v->x_pos;
	u->y_pos = v->y_pos;
	u->z_pos = v->z_pos;
	u->track = TRACK_BIT_DEPOT;
	u->vehstatus = v->vehstatus & ~VS_STOPPED;
	u->spritenum = v->spritenum + 1;
	u->cargo_type = v->cargo_type;
	u->cargo_subtype = v->cargo_subtype;
	u->cargo_cap = v->cargo_cap;
	u->railtype = v->railtype;
	u->engine_type = v->engine_type;
	u->build_year = v->build_year;
	u->cur_image = SPR_IMG_QUERY;
	u->random_bits = VehicleRandomBits();
	v->SetMultiheaded();
	u->SetMultiheaded();
	v->SetNext(u);
	VehicleMove(u, false);

	/* Now we need to link the front and rear engines together */
	v->other_multiheaded_part = u;
	u->other_multiheaded_part = v;
}

/**
 * Build a railroad vehicle.
 * @param tile     tile of the depot where rail-vehicle is built.
 * @param flags    type of operation.
 * @param e        the engine to build.
 * @param data     bit 0 prevents any free cars from being added to the train.
 * @param ret[out] the vehicle that has been built.
 * @return the cost of this operation or an error.
 */
CommandCost CmdBuildRailVehicle(TileIndex tile, DoCommandFlag flags, const Engine *e, uint16 data, Vehicle **ret)
{
	const RailVehicleInfo *rvi = &e->u.rail;

	if (rvi->railveh_type == RAILVEH_WAGON) return CmdBuildRailWagon(tile, flags, e, ret);

	/* Check if depot and new engine uses the same kind of tracks *
	 * We need to see if the engine got power on the tile to avoid electric engines in non-electric depots */
	if (!HasPowerOnRail(rvi->railtype, GetRailType(tile))) return CMD_ERROR;

	if (flags & DC_EXEC) {
		DiagDirection dir = GetRailDepotDirection(tile);
		int x = TileX(tile) * TILE_SIZE + _vehicle_initial_x_fract[dir];
		int y = TileY(tile) * TILE_SIZE + _vehicle_initial_y_fract[dir];

		Train *v = new Train();
		*ret = v;
		v->direction = DiagDirToDir(dir);
		v->tile = tile;
		v->owner = _current_company;
		v->x_pos = x;
		v->y_pos = y;
		v->z_pos = GetSlopeZ(x, y);
		v->track = TRACK_BIT_DEPOT;
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

		v->railtype = rvi->railtype;
		_new_vehicle_id = v->index;

		v->service_interval = Company::Get(_current_company)->settings.vehicle.servint_trains;
		v->date_of_last_service = _date;
		v->build_year = _cur_year;
		v->cur_image = SPR_IMG_QUERY;
		v->random_bits = VehicleRandomBits();

		if (e->flags & ENGINE_EXCLUSIVE_PREVIEW) SetBit(v->vehicle_flags, VF_BUILT_AS_PROTOTYPE);

		v->group_id = DEFAULT_GROUP;

		v->SetFrontEngine();
		v->SetEngine();

		VehicleMove(v, false);

		if (rvi->railveh_type == RAILVEH_MULTIHEAD) {
			AddRearEngineToMultiheadedTrain(v);
		} else {
			AddArticulatedParts(v);
		}

		v->ConsistChanged(false);
		UpdateTrainGroupID(v);

		if (!HasBit(data, 0) && !(flags & DC_AUTOREPLACE)) { // check if the cars should be added to the new vehicle
			NormalizeTrainVehInDepot(v);
		}

		CheckConsistencyOfArticulatedVehicle(v);
	}

	return CommandCost();
}


bool Train::IsInDepot() const
{
	/* Is the front engine stationary in the depot? */
	if (!IsRailDepotTile(this->tile) || this->cur_speed != 0) return false;

	/* Check whether the rest is also already trying to enter the depot. */
	for (const Train *v = this; v != NULL; v = v->Next()) {
		if (v->track != TRACK_BIT_DEPOT || v->tile != this->tile) return false;
	}

	return true;
}

bool Train::IsStoppedInDepot() const
{
	/* Are we stopped? Of course wagons don't really care... */
	if (this->IsFrontEngine() && !(this->vehstatus & VS_STOPPED)) return false;
	return this->IsInDepot();
}

static Train *FindGoodVehiclePos(const Train *src)
{
	EngineID eng = src->engine_type;
	TileIndex tile = src->tile;

	Train *dst;
	FOR_ALL_TRAINS(dst) {
		if (dst->IsFreeWagon() && dst->tile == tile && !(dst->vehstatus & VS_CRASHED)) {
			/* check so all vehicles in the line have the same engine. */
			Train *t = dst;
			while (t->engine_type == eng) {
				t = t->Next();
				if (t == NULL) return dst;
			}
		}
	}

	return NULL;
}

/** Helper type for lists/vectors of trains */
typedef SmallVector<Train *, 16> TrainList;

/**
 * Make a backup of a train into a train list.
 * @param list to make the backup in
 * @param t    the train to make the backup of
 */
static void MakeTrainBackup(TrainList &list, Train *t)
{
	for (; t != NULL; t = t->Next()) *list.Append() = t;
}

/**
 * Restore the train from the backup list.
 * @param list the train to restore.
 */
static void RestoreTrainBackup(TrainList &list)
{
	/* No train, nothing to do. */
	if (list.Length() == 0) return;

	Train *prev = NULL;
	/* Iterate over the list and rebuild it. */
	for (Train **iter = list.Begin(); iter != list.End(); iter++) {
		Train *t = *iter;
		if (prev != NULL) {
			prev->SetNext(t);
		} else if (t->Previous() != NULL) {
			/* Make sure the head of the train is always the first in the chain. */
			t->Previous()->SetNext(NULL);
		}
		prev = t;
	}
}

/**
 * Remove the given wagon from its consist.
 * @param part the part of the train to remove.
 * @param chain whether to remove the whole chain.
 */
static void RemoveFromConsist(Train *part, bool chain = false)
{
	Train *tail = chain ? part->Last() : part->GetLastEnginePart();

	/* Unlink at the front, but make it point to the next
	 * vehicle after the to be remove part. */
	if (part->Previous() != NULL) part->Previous()->SetNext(tail->Next());

	/* Unlink at the back */
	tail->SetNext(NULL);
}

/**
 * Inserts a chain into the train at dst.
 * @param dst   the place where to append after.
 * @param chain the chain to actually add.
 */
static void InsertInConsist(Train *dst, Train *chain)
{
	/* We do not want to add something in the middle of an articulated part. */
	assert(dst->Next() == NULL || !dst->Next()->IsArticulatedPart());

	chain->Last()->SetNext(dst->Next());
	dst->SetNext(chain);
}

/**
 * Normalise the dual heads in the train, i.e. if one is
 * missing move that one to this train.
 * @param t the train to normalise.
 */
static void NormaliseDualHeads(Train *t)
{
	for (; t != NULL; t = t->GetNextVehicle()) {
		if (!t->IsMultiheaded() || !t->IsEngine()) continue;

		/* Make sure that there are no free cars before next engine */
		Train *u;
		for (u = t; u->Next() != NULL && !u->Next()->IsEngine(); u = u->Next()) {}

		if (u == t->other_multiheaded_part) continue;

		/* Remove the part from the 'wrong' train */
		RemoveFromConsist(t->other_multiheaded_part);
		/* And add it to the 'right' train */
		InsertInConsist(u, t->other_multiheaded_part);
	}
}

/**
 * Normalise the sub types of the parts in this chain.
 * @param chain the chain to normalise.
 */
static void NormaliseSubtypes(Train *chain)
{
	/* Nothing to do */
	if (chain == NULL) return;

	/* We must be the first in the chain. */
	assert(chain->Previous() == NULL);

	/* Set the appropriate bits for the first in the chain. */
	if (chain->IsWagon()) {
		chain->SetFreeWagon();
	} else {
		assert(chain->IsEngine());
		chain->SetFrontEngine();
	}

	/* Now clear the bits for the rest of the chain */
	for (Train *t = chain->Next(); t != NULL; t = t->Next()) {
		t->ClearFreeWagon();
		t->ClearFrontEngine();
	}
}

/**
 * Check/validate whether we may actually build a new train.
 * @note All vehicles are/were 'heads' of their chains.
 * @param original_dst The original destination chain.
 * @param dst          The destination chain after constructing the train.
 * @param original_dst The original source chain.
 * @param dst          The source chain after constructing the train.
 * @return possible error of this command.
 */
static CommandCost CheckNewTrain(Train *original_dst, Train *dst, Train *original_src, Train *src)
{
	/* Just add 'new' engines and subtract the original ones.
	 * If that's less than or equal to 0 we can be sure we did
	 * not add any engines (read: trains) along the way. */
	if ((src          != NULL && src->IsEngine()          ? 1 : 0) +
			(dst          != NULL && dst->IsEngine()          ? 1 : 0) -
			(original_src != NULL && original_src->IsEngine() ? 1 : 0) -
			(original_dst != NULL && original_dst->IsEngine() ? 1 : 0) <= 0) {
		return CommandCost();
	}

	/* Get a free unit number and check whether it's within the bounds.
	 * There will always be a maximum of one new train. */
	if (GetFreeUnitNumber(VEH_TRAIN) <= _settings_game.vehicle.max_trains) return CommandCost();

	return_cmd_error(STR_ERROR_TOO_MANY_VEHICLES_IN_GAME);
}

/**
 * Check whether the train parts can be attached.
 * @param t the train to check
 * @return possible error of this command.
 */
static CommandCost CheckTrainAttachment(Train *t)
{
	/* No multi-part train, no need to check. */
	if (t == NULL || t->Next() == NULL || !t->IsEngine()) return CommandCost();

	/* The maximum length for a train. For each part we decrease this by one
	 * and if the result is negative the train is simply too long. */
	int allowed_len = _settings_game.vehicle.mammoth_trains ? 100 : 10;

	Train *head = t;
	Train *prev = t;

	/* Break the prev -> t link so it always holds within the loop. */
	t = t->Next();
	allowed_len--;
	prev->SetNext(NULL);

	/* Make sure the cache is cleared. */
	head->InvalidateNewGRFCache();

	while (t != NULL) {
		Train *next = t->Next();

		/* Unlink the to-be-added piece; it is already unlinked from the previous
		 * part due to the fact that the prev -> t link is broken. */
		t->SetNext(NULL);

		/* Don't check callback for articulated or rear dual headed parts */
		if (!t->IsArticulatedPart() && !t->IsRearDualheaded()) {
			allowed_len--; // We do not count articulated parts and rear heads either.

			/* Back up and clear the first_engine data to avoid using wagon override group */
			EngineID first_engine = t->gcache.first_engine;
			t->gcache.first_engine = INVALID_ENGINE;

			/* We don't want the cache to interfere. head's cache is cleared before
			 * the loop and after each callback does not need to be cleared here. */
			t->InvalidateNewGRFCache();

			uint16 callback = GetVehicleCallbackParent(CBID_TRAIN_ALLOW_WAGON_ATTACH, 0, 0, head->engine_type, t, head);

			/* Restore original first_engine data */
			t->gcache.first_engine = first_engine;

			/* We do not want to remember any cached variables from the test run */
			t->InvalidateNewGRFCache();
			head->InvalidateNewGRFCache();

			if (callback != CALLBACK_FAILED) {
				/* A failing callback means everything is okay */
				StringID error = STR_NULL;

				if (callback == 0xFD) error = STR_ERROR_INCOMPATIBLE_RAIL_TYPES;
				if (callback  < 0xFD) error = GetGRFStringID(GetEngineGRFID(head->engine_type), 0xD000 + callback);

				if (error != STR_NULL) return_cmd_error(error);
			}
		}

		/* And link it to the new part. */
		prev->SetNext(t);
		prev = t;
		t = next;
	}

	if (allowed_len <= 0) return_cmd_error(STR_ERROR_TRAIN_TOO_LONG);
	return CommandCost();
}

/**
 * Validate whether we are going to create valid trains.
 * @note All vehicles are/were 'heads' of their chains.
 * @param original_dst The original destination chain.
 * @param dst          The destination chain after constructing the train.
 * @param original_dst The original source chain.
 * @param dst          The source chain after constructing the train.
 * @param check_limit  Whether to check the vehicle limit.
 * @return possible error of this command.
 */
static CommandCost ValidateTrains(Train *original_dst, Train *dst, Train *original_src, Train *src, bool check_limit)
{
	/* Check whether we may actually construct the trains. */
	CommandCost ret = CheckTrainAttachment(src);
	if (ret.Failed()) return ret;
	ret = CheckTrainAttachment(dst);
	if (ret.Failed()) return ret;

	/* Check whether we need to build a new train. */
	return check_limit ? CheckNewTrain(original_dst, dst, original_src, src) : CommandCost();
}

/**
 * Arrange the trains in the wanted way.
 * @param dst_head   The destination chain of the to be moved vehicle.
 * @param dst        The destination for the to be moved vehicle.
 * @param src_head   The source chain of the to be moved vehicle.
 * @param src        The to be moved vehicle.
 * @param move_chain Whether to move all vehicles after src or not.
 */
static void ArrangeTrains(Train **dst_head, Train *dst, Train **src_head, Train *src, bool move_chain)
{
	/* First determine the front of the two resulting trains */
	if (*src_head == *dst_head) {
		/* If we aren't moving part(s) to a new train, we are just moving the
		 * front back and there is not destination head. */
		*dst_head = NULL;
	} else if (*dst_head == NULL) {
		/* If we are moving to a new train the head of the move train would become
		 * the head of the new vehicle. */
		*dst_head = src;
	}

	if (src == *src_head) {
		/* If we are moving the front of a train then we are, in effect, creating
		 * a new head for the train. Point to that. Unless we are moving the whole
		 * train in which case there is not 'source' train anymore.
		 * In case we are a multiheaded part we want the complete thing to come
		 * with us, so src->GetNextUnit(), however... when we are e.g. a wagon
		 * that is followed by a rear multihead we do not want to include that. */
		*src_head = move_chain ? NULL :
				(src->IsMultiheaded() ? src->GetNextUnit() : src->GetNextVehicle());
	}

	/* Now it's just simply removing the part that we are going to move from the
	 * source train and *if* the destination is a not a new train add the chain
	 * at the destination location. */
	RemoveFromConsist(src, move_chain);
	if (*dst_head != src) InsertInConsist(dst, src);

	/* Now normalise the dual heads, that is move the dual heads around in such
	 * a way that the head and rear of a dual head are in the same train */
	NormaliseDualHeads(*src_head);
	NormaliseDualHeads(*dst_head);
}

/**
 * Normalise the head of the train again, i.e. that is tell the world that
 * we have changed and update all kinds of variables.
 * @param head the train to update.
 */
static void NormaliseTrainHead(Train *head)
{
	/* Not much to do! */
	if (head == NULL) return;

	/* Tell the 'world' the train changed. */
	head->ConsistChanged(false);
	UpdateTrainGroupID(head);

	/* Not a front engine, i.e. a free wagon chain. No need to do more. */
	if (!head->IsFrontEngine()) return;

	/* Update the refit button and window */
	InvalidateWindowData(WC_VEHICLE_REFIT, head->index);
	SetWindowWidgetDirty(WC_VEHICLE_VIEW, head->index, VVW_WIDGET_REFIT_VEH);

	/* If we don't have a unit number yet, set one. */
	if (head->unitnumber != 0) return;
	head->unitnumber = GetFreeUnitNumber(VEH_TRAIN);
}

/**
 * Move a rail vehicle around inside the depot.
 * @param tile unused
 * @param flags type of operation
 *              Note: DC_AUTOREPLACE is set when autoreplace tries to undo its modifications or moves vehicles to temporary locations inside the depot.
 * @param p1 various bitstuffed elements
 * - p1 (bit  0 - 19) source vehicle index
 * - p1 (bit      20) move all vehicles following the source vehicle
 * @param p2 what wagon to put the source wagon AFTER, XXX - INVALID_VEHICLE to make a new line
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdMoveRailVehicle(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	VehicleID s = GB(p1, 0, 20);
	VehicleID d = GB(p2, 0, 20);
	bool move_chain = HasBit(p1, 20);

	Train *src = Train::GetIfValid(s);
	if (src == NULL) return CMD_ERROR;

	CommandCost ret = CheckOwnership(src->owner);
	if (ret.Failed()) return ret;

	/* Do not allow moving crashed vehicles inside the depot, it is likely to cause asserts later */
	if (src->vehstatus & VS_CRASHED) return CMD_ERROR;

	/* if nothing is selected as destination, try and find a matching vehicle to drag to. */
	Train *dst;
	if (d == INVALID_VEHICLE) {
		dst = src->IsEngine() ? NULL : FindGoodVehiclePos(src);
	} else {
		dst = Train::GetIfValid(d);
		if (dst == NULL) return CMD_ERROR;

		CommandCost ret = CheckOwnership(dst->owner);
		if (ret.Failed()) return ret;

		/* Do not allow appending to crashed vehicles, too */
		if (dst->vehstatus & VS_CRASHED) return CMD_ERROR;
	}

	/* if an articulated part is being handled, deal with its parent vehicle */
	src = src->GetFirstEnginePart();
	if (dst != NULL) {
		dst = dst->GetFirstEnginePart();
	}

	/* don't move the same vehicle.. */
	if (src == dst) return CommandCost();

	/* locate the head of the two chains */
	Train *src_head = src->First();
	Train *dst_head;
	if (dst != NULL) {
		dst_head = dst->First();
		if (dst_head->tile != src_head->tile) return CMD_ERROR;
		/* Now deal with articulated part of destination wagon */
		dst = dst->GetLastEnginePart();
	} else {
		dst_head = NULL;
	}

	if (src->IsRearDualheaded()) return_cmd_error(STR_ERROR_REAR_ENGINE_FOLLOW_FRONT);

	/* When moving all wagons, we can't have the same src_head and dst_head */
	if (move_chain && src_head == dst_head) return CommandCost();

	/* When moving a multiheaded part to be place after itself, bail out. */
	if (!move_chain && dst != NULL && dst->IsRearDualheaded() && src == dst->other_multiheaded_part) return CommandCost();

	/* Check if all vehicles in the source train are stopped inside a depot. */
	if (!src_head->IsStoppedInDepot()) return_cmd_error(STR_ERROR_TRAINS_CAN_ONLY_BE_ALTERED_INSIDE_A_DEPOT);

	/* Check if all vehicles in the destination train are stopped inside a depot. */
	if (dst_head != NULL && !dst_head->IsStoppedInDepot()) return_cmd_error(STR_ERROR_TRAINS_CAN_ONLY_BE_ALTERED_INSIDE_A_DEPOT);

	/* First make a backup of the order of the trains. That way we can do
	 * whatever we want with the order and later on easily revert. */
	TrainList original_src;
	TrainList original_dst;

	MakeTrainBackup(original_src, src_head);
	MakeTrainBackup(original_dst, dst_head);

	/* Also make backup of the original heads as ArrangeTrains can change them.
	 * For the destination head we do not care if it is the same as the source
	 * head because in that case it's just a copy. */
	Train *original_src_head = src_head;
	Train *original_dst_head = (dst_head == src_head ? NULL : dst_head);

	/* (Re)arrange the trains in the wanted arrangement. */
	ArrangeTrains(&dst_head, dst, &src_head, src, move_chain);

	if ((flags & DC_AUTOREPLACE) == 0) {
		/* If the autoreplace flag is set we do not need to test for the validity
		 * because we are going to revert the train to its original state. As we
		 * assume the original state was correct autoreplace can skip this. */
		CommandCost ret = ValidateTrains(original_dst_head, dst_head, original_src_head, src_head, true);
		if (ret.Failed()) {
			/* Restore the train we had. */
			RestoreTrainBackup(original_src);
			RestoreTrainBackup(original_dst);
			return ret;
		}
	}

	/* do it? */
	if (flags & DC_EXEC) {
		/* First normalise the sub types of the chains. */
		NormaliseSubtypes(src_head);
		NormaliseSubtypes(dst_head);

		/* There are 14 different cases:
		 *  1) front engine gets moved to a new train, it stays a front engine.
		 *     a) the 'next' part is a wagon that becomes a free wagon chain.
		 *     b) the 'next' part is an engine that becomes a front engine.
		 *     c) there is no 'next' part, nothing else happens
		 *  2) front engine gets moved to another train, it is not a front engine anymore
		 *     a) the 'next' part is a wagon that becomes a free wagon chain.
		 *     b) the 'next' part is an engine that becomes a front engine.
		 *     c) there is no 'next' part, nothing else happens
		 *  3) front engine gets moved to later in the current train, it is not a front engine anymore.
		 *     a) the 'next' part is a wagon that becomes a free wagon chain.
		 *     b) the 'next' part is an engine that becomes a front engine.
		 *  4) free wagon gets moved
		 *     a) the 'next' part is a wagon that becomes a free wagon chain.
		 *     b) the 'next' part is an engine that becomes a front engine.
		 *     c) there is no 'next' part, nothing else happens
		 *  5) non front engine gets moved and becomes a new train, nothing else happens
		 *  6) non front engine gets moved within a train / to another train, nothing hapens
		 *  7) wagon gets moved, nothing happens
		 */
		if (src == original_src_head && src->IsEngine() && !src->IsFrontEngine()) {
			/* Cases #2 and #3: the front engine gets trashed. */
			DeleteWindowById(WC_VEHICLE_VIEW, src->index);
			DeleteWindowById(WC_VEHICLE_ORDERS, src->index);
			DeleteWindowById(WC_VEHICLE_REFIT, src->index);
			DeleteWindowById(WC_VEHICLE_DETAILS, src->index);
			DeleteWindowById(WC_VEHICLE_TIMETABLE, src->index);

			/* We are going to be moved to a different train, and
			 * we were the front engine of the original train. */
			if (dst_head != NULL && dst_head != src && (src_head == NULL || !src_head->IsFrontEngine())) {
				DecreaseGroupNumVehicle(src->group_id);
			}

			/* The front engine is going to be moved later in the
			 * current train, and it will not be a train anymore. */
			if (dst_head == NULL && !src_head->IsFrontEngine()) {
				DecreaseGroupNumVehicle(src->group_id);
			}

			/* Delete orders, group stuff and the unit number as we're not the
			 * front of any vehicle anymore. */
			DeleteVehicleOrders(src);
			RemoveVehicleFromGroup(src);
			src->unitnumber = 0;
		}

		/* We were a front engine and we are becoming one for a different train.
		 * Increase the group counter accordingly. */
		if (original_src_head == src && dst_head == src) {
			IncreaseGroupNumVehicle(src->group_id);
		}

		/* We weren't a front engine but are becoming one. So
		 * we should be put in the default group. */
		if (original_src_head != src && dst_head == src) {
			SetTrainGroupID(src, DEFAULT_GROUP);
		}

		/* Handle 'new engine' part of cases #1b, #2b, #3b, #4b and #5 in NormaliseTrainHead. */
		NormaliseTrainHead(src_head);
		NormaliseTrainHead(dst_head);

		/* We are undoubtedly changing something in the depot and train list. */
		InvalidateWindowData(WC_VEHICLE_DEPOT, src->tile);
		InvalidateWindowClassesData(WC_TRAINS_LIST, 0);
	} else {
		/* We don't want to execute what we're just tried. */
		RestoreTrainBackup(original_src);
		RestoreTrainBackup(original_dst);
	}

	return CommandCost();
}

/**
 * Sell a (single) train wagon/engine.
 * @param flags type of operation
 * @param t     the train wagon to sell
 * @param data  the selling mode
 * - data = 0: only sell the single dragged wagon/engine (and any belonging rear-engines)
 * - data = 1: sell the vehicle and all vehicles following it in the chain
 *             if the wagon is dragged, don't delete the possibly belonging rear-engine to some front
 * @param user  the user for the order backup.
 * @return the cost of this operation or an error
 */
CommandCost CmdSellRailWagon(DoCommandFlag flags, Vehicle *t, uint16 data, uint32 user)
{
	/* Check if we deleted a vehicle window */
	Window *w = NULL;

	/* Sell a chain of vehicles or not? */
	bool sell_chain = HasBit(data, 0);

	Train *v = Train::From(t)->GetFirstEnginePart();
	Train *first = v->First();

	if (v->IsRearDualheaded()) return_cmd_error(STR_ERROR_REAR_ENGINE_FOLLOW_FRONT);

	/* First make a backup of the order of the train. That way we can do
	 * whatever we want with the order and later on easily revert. */
	TrainList original;
	MakeTrainBackup(original, first);

	/* We need to keep track of the new head and the head of what we're going to sell. */
	Train *new_head = first;
	Train *sell_head = NULL;

	/* Split the train in the wanted way. */
	ArrangeTrains(&sell_head, NULL, &new_head, v, sell_chain);

	/* We don't need to validate the second train; it's going to be sold. */
	CommandCost ret = ValidateTrains(NULL, NULL, first, new_head, (flags & DC_AUTOREPLACE) == 0);
	if (ret.Failed()) {
		/* Restore the train we had. */
		RestoreTrainBackup(original);
		return ret;
	}

	CommandCost cost(EXPENSES_NEW_VEHICLES);
	for (Train *t = sell_head; t != NULL; t = t->Next()) cost.AddCost(-t->value);

	/* do it? */
	if (flags & DC_EXEC) {
		/* First normalise the sub types of the chain. */
		NormaliseSubtypes(new_head);

		if (v == first && v->IsEngine() && !sell_chain && new_head != NULL && new_head->IsFrontEngine()) {
			/* We are selling the front engine. In this case we want to
			 * 'give' the order, unit number and such to the new head. */
			new_head->orders.list = first->orders.list;
			new_head->AddToShared(first);
			DeleteVehicleOrders(first);

			/* Copy other important data from the front engine */
			new_head->CopyVehicleConfigAndStatistics(first);
			IncreaseGroupNumVehicle(new_head->group_id);

			/* If we deleted a window then open a new one for the 'new' train */
			if (IsLocalCompany() && w != NULL) ShowVehicleViewWindow(new_head);
		} else if (v->IsPrimaryVehicle() && data & (MAKE_ORDER_BACKUP_FLAG >> 20)) {
			OrderBackup::Backup(v, user);
		}

		/* We need to update the information about the train. */
		NormaliseTrainHead(new_head);

		/* We are undoubtedly changing something in the depot and train list. */
		InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
		InvalidateWindowClassesData(WC_TRAINS_LIST, 0);

		/* Actually delete the sold 'goods' */
		delete sell_head;
	} else {
		/* We don't want to execute what we're just tried. */
		RestoreTrainBackup(original);
	}

	return cost;
}

void Train::UpdateDeltaXY(Direction direction)
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

static inline void SetLastSpeed(Train *v, int spd)
{
	int old = v->tcache.last_speed;
	if (spd != old) {
		v->tcache.last_speed = spd;
		if (_settings_client.gui.vehicle_speed || (old == 0) != (spd == 0)) {
			SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
		}
	}
}

/** Mark a train as stuck and stop it if it isn't stopped right now. */
static void MarkTrainAsStuck(Train *v)
{
	if (!HasBit(v->flags, VRF_TRAIN_STUCK)) {
		/* It is the first time the problem occurred, set the "train stuck" flag. */
		SetBit(v->flags, VRF_TRAIN_STUCK);

		v->wait_counter = 0;

		/* Stop train */
		v->cur_speed = 0;
		v->subspeed = 0;
		SetLastSpeed(v, 0);

		SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
	}
}

static void SwapTrainFlags(uint16 *swap_flag1, uint16 *swap_flag2)
{
	uint16 flag1 = *swap_flag1;
	uint16 flag2 = *swap_flag2;

	/* Clear the flags */
	ClrBit(*swap_flag1, GVF_GOINGUP_BIT);
	ClrBit(*swap_flag1, GVF_GOINGDOWN_BIT);
	ClrBit(*swap_flag2, GVF_GOINGUP_BIT);
	ClrBit(*swap_flag2, GVF_GOINGDOWN_BIT);

	/* Reverse the rail-flags (if needed) */
	if (HasBit(flag1, GVF_GOINGUP_BIT)) {
		SetBit(*swap_flag2, GVF_GOINGDOWN_BIT);
	} else if (HasBit(flag1, GVF_GOINGDOWN_BIT)) {
		SetBit(*swap_flag2, GVF_GOINGUP_BIT);
	}
	if (HasBit(flag2, GVF_GOINGUP_BIT)) {
		SetBit(*swap_flag1, GVF_GOINGDOWN_BIT);
	} else if (HasBit(flag2, GVF_GOINGDOWN_BIT)) {
		SetBit(*swap_flag1, GVF_GOINGUP_BIT);
	}
}

/**
 * Updates some variables after swapping the vehicle.
 * @param v swapped vehicle
 */
static void UpdateStatusAfterSwap(Train *v)
{
	/* Reverse the direction. */
	if (v->track != TRACK_BIT_DEPOT) v->direction = ReverseDir(v->direction);

	/* Call the proper EnterTile function unless we are in a wormhole. */
	if (v->track != TRACK_BIT_WORMHOLE) {
		VehicleEnterTile(v, v->tile, v->x_pos, v->y_pos);
	} else {
		/* VehicleEnter_TunnelBridge() sets TRACK_BIT_WORMHOLE when the vehicle
		 * is on the last bit of the bridge head (frame == TILE_SIZE - 1).
		 * If we were swapped with such a vehicle, we have set TRACK_BIT_WORMHOLE,
		 * when we shouldn't have. Check if this is the case. */
		TileIndex vt = TileVirtXY(v->x_pos, v->y_pos);
		if (IsTileType(vt, MP_TUNNELBRIDGE)) {
			VehicleEnterTile(v, vt, v->x_pos, v->y_pos);
			if (v->track != TRACK_BIT_WORMHOLE && IsBridgeTile(v->tile)) {
				/* We have just left the wormhole, possibly set the
				 * "goingdown" bit. UpdateInclination() can be used
				 * because we are at the border of the tile. */
				v->UpdateInclination(true, true);
				return;
			}
		}
	}

	v->UpdateViewport(true, true);
}

static void ReverseTrainSwapVeh(Train *v, int l, int r)
{
	Train *a, *b;

	/* locate vehicles to swap */
	for (a = v; l != 0; l--) a = a->Next();
	for (b = v; r != 0; r--) b = b->Next();

	if (a != b) {
		/* swap the hidden bits */
		{
			uint16 tmp = (a->vehstatus & ~VS_HIDDEN) | (b->vehstatus & VS_HIDDEN);
			b->vehstatus = (b->vehstatus & ~VS_HIDDEN) | (a->vehstatus & VS_HIDDEN);
			a->vehstatus = tmp;
		}

		Swap(a->track, b->track);
		Swap(a->direction, b->direction);
		Swap(a->x_pos, b->x_pos);
		Swap(a->y_pos, b->y_pos);
		Swap(a->tile,  b->tile);
		Swap(a->z_pos, b->z_pos);

		SwapTrainFlags(&a->gv_flags, &b->gv_flags);

		UpdateStatusAfterSwap(a);
		UpdateStatusAfterSwap(b);
	} else {
		/* Swap GVF_GOINGUP_BIT/GVF_GOINGDOWN_BIT.
		 * This is a little bit redundant way, a->gv_flags will
		 * be (re)set twice, but it reduces code duplication */
		SwapTrainFlags(&a->gv_flags, &a->gv_flags);
		UpdateStatusAfterSwap(a);
	}

	/* Update power of the train in case tiles were different rail type. */
	v->RailtypeChanged();
}


/**
 * Check if the vehicle is a train
 * @param v vehicle on tile
 * @return v if it is a train, NULL otherwise
 */
static Vehicle *TrainOnTileEnum(Vehicle *v, void *)
{
	return (v->type == VEH_TRAIN) ? v : NULL;
}


/**
 * Checks if a train is approaching a rail-road crossing
 * @param v vehicle on tile
 * @param data tile with crossing we are testing
 * @return v if it is approaching a crossing, NULL otherwise
 */
static Vehicle *TrainApproachingCrossingEnum(Vehicle *v, void *data)
{
	if (v->type != VEH_TRAIN || (v->vehstatus & VS_CRASHED)) return NULL;

	Train *t = Train::From(v);
	if (!t->IsFrontEngine()) return NULL;

	TileIndex tile = *(TileIndex *)data;

	if (TrainApproachingCrossingTile(t) != tile) return NULL;

	return t;
}


/**
 * Finds a vehicle approaching rail-road crossing
 * @param tile tile to test
 * @return true if a vehicle is approaching the crossing
 * @pre tile is a rail-road crossing
 */
static bool TrainApproachingCrossing(TileIndex tile)
{
	assert(IsLevelCrossingTile(tile));

	DiagDirection dir = AxisToDiagDir(GetCrossingRailAxis(tile));
	TileIndex tile_from = tile + TileOffsByDiagDir(dir);

	if (HasVehicleOnPos(tile_from, &tile, &TrainApproachingCrossingEnum)) return true;

	dir = ReverseDiagDir(dir);
	tile_from = tile + TileOffsByDiagDir(dir);

	return HasVehicleOnPos(tile_from, &tile, &TrainApproachingCrossingEnum);
}


/**
 * Sets correct crossing state
 * @param tile tile to update
 * @param sound should we play sound?
 * @pre tile is a rail-road crossing
 */
void UpdateLevelCrossing(TileIndex tile, bool sound)
{
	assert(IsLevelCrossingTile(tile));

	/* train on crossing || train approaching crossing || reserved */
	bool new_state = HasVehicleOnPos(tile, NULL, &TrainOnTileEnum) || TrainApproachingCrossing(tile) || HasCrossingReservation(tile);

	if (new_state != IsCrossingBarred(tile)) {
		if (new_state && sound) {
			SndPlayTileFx(SND_0E_LEVEL_CROSSING, tile);
		}
		SetCrossingBarred(tile, new_state);
		MarkTileDirtyByTile(tile);
	}
}


/**
 * Bars crossing and plays ding-ding sound if not barred already
 * @param tile tile with crossing
 * @pre tile is a rail-road crossing
 */
static inline void MaybeBarCrossingWithSound(TileIndex tile)
{
	if (!IsCrossingBarred(tile)) {
		BarCrossing(tile);
		SndPlayTileFx(SND_0E_LEVEL_CROSSING, tile);
		MarkTileDirtyByTile(tile);
	}
}


/**
 * Advances wagons for train reversing, needed for variable length wagons.
 * This one is called before the train is reversed.
 * @param v First vehicle in chain
 */
static void AdvanceWagonsBeforeSwap(Train *v)
{
	Train *base = v;
	Train *first = base; // first vehicle to move
	Train *last = v->Last(); // last vehicle to move
	uint length = CountVehiclesInChain(v);

	while (length > 2) {
		last = last->Previous();
		first = first->Next();

		int differential = base->gcache.cached_veh_length - last->gcache.cached_veh_length;

		/* do not update images now
		 * negative differential will be handled in AdvanceWagonsAfterSwap() */
		for (int i = 0; i < differential; i++) TrainController(first, last->Next());

		base = first; // == base->Next()
		length -= 2;
	}
}


/**
 * Advances wagons for train reversing, needed for variable length wagons.
 * This one is called after the train is reversed.
 * @param v First vehicle in chain
 */
static void AdvanceWagonsAfterSwap(Train *v)
{
	/* first of all, fix the situation when the train was entering a depot */
	Train *dep = v; // last vehicle in front of just left depot
	while (dep->Next() != NULL && (dep->track == TRACK_BIT_DEPOT || dep->Next()->track != TRACK_BIT_DEPOT)) {
		dep = dep->Next(); // find first vehicle outside of a depot, with next vehicle inside a depot
	}

	Train *leave = dep->Next(); // first vehicle in a depot we are leaving now

	if (leave != NULL) {
		/* 'pull' next wagon out of the depot, so we won't miss it (it could stay in depot forever) */
		int d = TicksToLeaveDepot(dep);

		if (d <= 0) {
			leave->vehstatus &= ~VS_HIDDEN; // move it out of the depot
			leave->track = TrackToTrackBits(GetRailDepotTrack(leave->tile));
			for (int i = 0; i >= d; i--) TrainController(leave, NULL); // maybe move it, and maybe let another wagon leave
		}
	} else {
		dep = NULL; // no vehicle in a depot, so no vehicle leaving a depot
	}

	Train *base = v;
	Train *first = base; // first vehicle to move
	Train *last = v->Last(); // last vehicle to move
	uint length = CountVehiclesInChain(v);

	/* We have to make sure all wagons that leave a depot because of train reversing are moved correctly
	 * they have already correct spacing, so we have to make sure they are moved how they should */
	bool nomove = (dep == NULL); // If there is no vehicle leaving a depot, limit the number of wagons moved immediately.

	while (length > 2) {
		/* we reached vehicle (originally) in front of a depot, stop now
		 * (we would move wagons that are already moved with new wagon length). */
		if (base == dep) break;

		/* the last wagon was that one leaving a depot, so do not move it anymore */
		if (last == dep) nomove = true;

		last = last->Previous();
		first = first->Next();

		int differential = last->gcache.cached_veh_length - base->gcache.cached_veh_length;

		/* do not update images now */
		for (int i = 0; i < differential; i++) TrainController(first, (nomove ? last->Next() : NULL));

		base = first; // == base->Next()
		length -= 2;
	}
}


static void ReverseTrainDirection(Train *v)
{
	if (IsRailDepotTile(v->tile)) {
		InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
	}

	/* Clear path reservation in front if train is not stuck. */
	if (!HasBit(v->flags, VRF_TRAIN_STUCK)) FreeTrainTrackReservation(v);

	/* Check if we were approaching a rail/road-crossing */
	TileIndex crossing = TrainApproachingCrossingTile(v);

	/* count number of vehicles */
	int r = CountVehiclesInChain(v) - 1;  // number of vehicles - 1

	AdvanceWagonsBeforeSwap(v);

	/* swap start<>end, start+1<>end-1, ... */
	int l = 0;
	do {
		ReverseTrainSwapVeh(v, l++, r--);
	} while (l <= r);

	AdvanceWagonsAfterSwap(v);

	if (IsRailDepotTile(v->tile)) {
		InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
	}

	ToggleBit(v->flags, VRF_TOGGLE_REVERSE);

	ClrBit(v->flags, VRF_REVERSING);

	/* recalculate cached data */
	v->ConsistChanged(true);

	/* update all images */
	for (Train *u = v; u != NULL; u = u->Next()) u->UpdateViewport(false, false);

	/* update crossing we were approaching */
	if (crossing != INVALID_TILE) UpdateLevelCrossing(crossing);

	/* maybe we are approaching crossing now, after reversal */
	crossing = TrainApproachingCrossingTile(v);
	if (crossing != INVALID_TILE) MaybeBarCrossingWithSound(crossing);

	/* If we are inside a depot after reversing, don't bother with path reserving. */
	if (v->track == TRACK_BIT_DEPOT) {
		/* Can't be stuck here as inside a depot is always a safe tile. */
		if (HasBit(v->flags, VRF_TRAIN_STUCK)) SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
		ClrBit(v->flags, VRF_TRAIN_STUCK);
		return;
	}

	/* TrainExitDir does not always produce the desired dir for depots and
	 * tunnels/bridges that is needed for UpdateSignalsOnSegment. */
	DiagDirection dir = TrainExitDir(v->direction, v->track);
	if (IsRailDepotTile(v->tile) || IsTileType(v->tile, MP_TUNNELBRIDGE)) dir = INVALID_DIAGDIR;

	if (UpdateSignalsOnSegment(v->tile, dir, v->owner) == SIGSEG_PBS || _settings_game.pf.reserve_paths) {
		/* If we are currently on a tile with conventional signals, we can't treat the
		 * current tile as a safe tile or we would enter a PBS block without a reservation. */
		bool first_tile_okay = !(IsTileType(v->tile, MP_RAILWAY) &&
			HasSignalOnTrackdir(v->tile, v->GetVehicleTrackdir()) &&
			!IsPbsSignal(GetSignalType(v->tile, FindFirstTrack(v->track))));

		if (IsRailStationTile(v->tile)) SetRailStationPlatformReservation(v->tile, TrackdirToExitdir(v->GetVehicleTrackdir()), true);
		if (TryPathReserve(v, false, first_tile_okay)) {
			/* Do a look-ahead now in case our current tile was already a safe tile. */
			CheckNextTrainTile(v);
		} else if (v->current_order.GetType() != OT_LOADING) {
			/* Do not wait for a way out when we're still loading */
			MarkTrainAsStuck(v);
		}
	} else if (HasBit(v->flags, VRF_TRAIN_STUCK)) {
		/* A train not inside a PBS block can't be stuck. */
		ClrBit(v->flags, VRF_TRAIN_STUCK);
		v->wait_counter = 0;
	}
}

/**
 * Reverse train.
 * @param tile unused
 * @param flags type of operation
 * @param p1 train to reverse
 * @param p2 if true, reverse a unit in a train (needs to be in a depot)
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdReverseTrainDirection(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Train *v = Train::GetIfValid(p1);
	if (v == NULL) return CMD_ERROR;

	CommandCost ret = CheckOwnership(v->owner);
	if (ret.Failed()) return ret;

	if (p2 != 0) {
		/* turn a single unit around */

		if (v->IsMultiheaded() || HasBit(EngInfo(v->engine_type)->callback_mask, CBM_VEHICLE_ARTIC_ENGINE)) {
			return_cmd_error(STR_ERROR_CAN_T_REVERSE_DIRECTION_RAIL_VEHICLE_MULTIPLE_UNITS);
		}

		Train *front = v->First();
		/* make sure the vehicle is stopped in the depot */
		if (!front->IsStoppedInDepot()) {
			return_cmd_error(STR_ERROR_TRAINS_CAN_ONLY_BE_ALTERED_INSIDE_A_DEPOT);
		}

		if (flags & DC_EXEC) {
			ToggleBit(v->flags, VRF_REVERSE_DIRECTION);
			SetWindowDirty(WC_VEHICLE_DEPOT, v->tile);
			SetWindowDirty(WC_VEHICLE_DETAILS, v->index);
			/* We cancel any 'skip signal at dangers' here */
			v->force_proceed = TFP_NONE;
			SetWindowDirty(WC_VEHICLE_VIEW, v->index);
		}
	} else {
		/* turn the whole train around */
		if ((v->vehstatus & VS_CRASHED) || v->breakdown_ctr != 0) return CMD_ERROR;

		if (flags & DC_EXEC) {
			/* Properly leave the station if we are loading and won't be loading anymore */
			if (v->current_order.IsType(OT_LOADING)) {
				const Vehicle *last = v;
				while (last->Next() != NULL) last = last->Next();

				/* not a station || different station --> leave the station */
				if (!IsTileType(last->tile, MP_STATION) || GetStationIndex(last->tile) != GetStationIndex(v->tile)) {
					v->LeaveStation();
				}
			}

			/* We cancel any 'skip signal at dangers' here */
			v->force_proceed = TFP_NONE;
			SetWindowDirty(WC_VEHICLE_VIEW, v->index);

			if (_settings_game.vehicle.train_acceleration_model != AM_ORIGINAL && v->cur_speed != 0) {
				ToggleBit(v->flags, VRF_REVERSING);
			} else {
				v->cur_speed = 0;
				SetLastSpeed(v, 0);
				HideFillingPercent(&v->fill_percent_te_id);
				ReverseTrainDirection(v);
			}
		}
	}
	return CommandCost();
}

/**
 * Force a train through a red signal
 * @param tile unused
 * @param flags type of operation
 * @param p1 train to ignore the red signal
 * @param p2 unused
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdForceTrainProceed(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Train *t = Train::GetIfValid(p1);
	if (t == NULL) return CMD_ERROR;

	CommandCost ret = CheckOwnership(t->owner);
	if (ret.Failed()) return ret;


	if (flags & DC_EXEC) {
		/* If we are forced to proceed, cancel that order.
		 * If we are marked stuck we would want to force the train
		 * to proceed to the next signal. In the other cases we
		 * would like to pass the signal at danger and run till the
		 * next signal we encounter. */
		t->force_proceed = t->force_proceed == TFP_SIGNAL ? TFP_NONE : HasBit(t->flags, VRF_TRAIN_STUCK) || t->IsInDepot() ? TFP_STUCK : TFP_SIGNAL;
		SetWindowDirty(WC_VEHICLE_VIEW, t->index);
	}

	return CommandCost();
}

/**
 * returns the tile of a depot to goto to. The given vehicle must not be
 * crashed!
 */
static FindDepotData FindClosestTrainDepot(Train *v, int max_distance)
{
	assert(!(v->vehstatus & VS_CRASHED));

	if (IsRailDepotTile(v->tile)) return FindDepotData(v->tile, 0);

	PBSTileInfo origin = FollowTrainReservation(v);
	if (IsRailDepotTile(origin.tile)) return FindDepotData(origin.tile, 0);

	switch (_settings_game.pf.pathfinder_for_trains) {
		case VPF_NPF: return NPFTrainFindNearestDepot(v, max_distance);
		case VPF_YAPF: return YapfTrainFindNearestDepot(v, max_distance);

		default: NOT_REACHED();
	}
}

bool Train::FindClosestDepot(TileIndex *location, DestinationID *destination, bool *reverse)
{
	FindDepotData tfdd = FindClosestTrainDepot(this, 0);
	if (tfdd.best_length == UINT_MAX) return false;

	if (location    != NULL) *location    = tfdd.tile;
	if (destination != NULL) *destination = GetDepotIndex(tfdd.tile);
	if (reverse     != NULL) *reverse     = tfdd.reverse;

	return true;
}

void Train::PlayLeaveStationSound() const
{
	static const SoundFx sfx[] = {
		SND_04_TRAIN,
		SND_0A_TRAIN_HORN,
		SND_0A_TRAIN_HORN,
		SND_47_MAGLEV_2,
		SND_41_MAGLEV
	};

	if (PlayVehicleSound(this, VSE_START)) return;

	EngineID engtype = this->engine_type;
	SndPlayVehicleFx(sfx[RailVehInfo(engtype)->engclass], this);
}

/** Check if the train is on the last reserved tile and try to extend the path then. */
static void CheckNextTrainTile(Train *v)
{
	/* Don't do any look-ahead if path_backoff_interval is 255. */
	if (_settings_game.pf.path_backoff_interval == 255) return;

	/* Exit if we are inside a depot. */
	if (v->track == TRACK_BIT_DEPOT) return;

	switch (v->current_order.GetType()) {
		/* Exit if we reached our destination depot. */
		case OT_GOTO_DEPOT:
			if (v->tile == v->dest_tile) return;
			break;

		case OT_GOTO_WAYPOINT:
			/* If we reached our waypoint, make sure we see that. */
			if (IsRailWaypointTile(v->tile) && GetStationIndex(v->tile) == v->current_order.GetDestination()) ProcessOrders(v);
			break;

		case OT_NOTHING:
		case OT_LEAVESTATION:
		case OT_LOADING:
			/* Exit if the current order doesn't have a destination, but the train has orders. */
			if (v->GetNumOrders() > 0) return;
			break;

		default:
			break;
	}
	/* Exit if we are on a station tile and are going to stop. */
	if (IsRailStationTile(v->tile) && v->current_order.ShouldStopAtStation(v, GetStationIndex(v->tile))) return;

	Trackdir td = v->GetVehicleTrackdir();

	/* On a tile with a red non-pbs signal, don't look ahead. */
	if (IsTileType(v->tile, MP_RAILWAY) && HasSignalOnTrackdir(v->tile, td) &&
			!IsPbsSignal(GetSignalType(v->tile, TrackdirToTrack(td))) &&
			GetSignalStateByTrackdir(v->tile, td) == SIGNAL_STATE_RED) return;

	CFollowTrackRail ft(v);
	if (!ft.Follow(v->tile, td)) return;

	if (!HasReservedTracks(ft.m_new_tile, TrackdirBitsToTrackBits(ft.m_new_td_bits))) {
		/* Next tile is not reserved. */
		if (KillFirstBit(ft.m_new_td_bits) == TRACKDIR_BIT_NONE) {
			if (HasPbsSignalOnTrackdir(ft.m_new_tile, FindFirstTrackdir(ft.m_new_td_bits))) {
				/* If the next tile is a PBS signal, try to make a reservation. */
				TrackBits tracks = TrackdirBitsToTrackBits(ft.m_new_td_bits);
				if (_settings_game.pf.forbid_90_deg) {
					tracks &= ~TrackCrossesTracks(TrackdirToTrack(ft.m_old_td));
				}
				ChooseTrainTrack(v, ft.m_new_tile, ft.m_exitdir, tracks, false, NULL, false);
			}
		}
	}
}

static bool CheckTrainStayInDepot(Train *v)
{
	/* bail out if not all wagons are in the same depot or not in a depot at all */
	for (const Train *u = v; u != NULL; u = u->Next()) {
		if (u->track != TRACK_BIT_DEPOT || u->tile != v->tile) return false;
	}

	/* if the train got no power, then keep it in the depot */
	if (v->gcache.cached_power == 0) {
		v->vehstatus |= VS_STOPPED;
		SetWindowDirty(WC_VEHICLE_DEPOT, v->tile);
		return true;
	}

	SigSegState seg_state;

	if (v->force_proceed == TFP_NONE) {
		/* force proceed was not pressed */
		if (++v->wait_counter < 37) {
			SetWindowClassesDirty(WC_TRAINS_LIST);
			return true;
		}

		v->wait_counter = 0;

		seg_state = _settings_game.pf.reserve_paths ? SIGSEG_PBS : UpdateSignalsOnSegment(v->tile, INVALID_DIAGDIR, v->owner);
		if (seg_state == SIGSEG_FULL || HasDepotReservation(v->tile)) {
			/* Full and no PBS signal in block or depot reserved, can't exit. */
			SetWindowClassesDirty(WC_TRAINS_LIST);
			return true;
		}
	} else {
		seg_state = _settings_game.pf.reserve_paths ? SIGSEG_PBS : UpdateSignalsOnSegment(v->tile, INVALID_DIAGDIR, v->owner);
	}

	/* We are leaving a depot, but have to go to the exact same one; re-enter */
	if (v->current_order.IsType(OT_GOTO_DEPOT) && v->tile == v->dest_tile) {
		/* We need to have a reservation for this to work. */
		if (HasDepotReservation(v->tile)) return true;
		SetDepotReservation(v->tile, true);
		VehicleEnterDepot(v);
		return true;
	}

	/* Only leave when we can reserve a path to our destination. */
	if (seg_state == SIGSEG_PBS && !TryPathReserve(v) && v->force_proceed == TFP_NONE) {
		/* No path and no force proceed. */
		SetWindowClassesDirty(WC_TRAINS_LIST);
		MarkTrainAsStuck(v);
		return true;
	}

	SetDepotReservation(v->tile, true);
	if (_settings_client.gui.show_track_reservation) MarkTileDirtyByTile(v->tile);

	VehicleServiceInDepot(v);
	SetWindowClassesDirty(WC_TRAINS_LIST);
	v->PlayLeaveStationSound();

	v->track = TRACK_BIT_X;
	if (v->direction & 2) v->track = TRACK_BIT_Y;

	v->vehstatus &= ~VS_HIDDEN;
	v->cur_speed = 0;

	v->UpdateDeltaXY(v->direction);
	v->cur_image = v->GetImage(v->direction);
	VehicleMove(v, false);
	UpdateSignalsOnSegment(v->tile, INVALID_DIAGDIR, v->owner);
	v->UpdateAcceleration();
	InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);

	return false;
}

/** Clear the reservation of a tile that was just left by a wagon on track_dir. */
static void ClearPathReservation(const Train *v, TileIndex tile, Trackdir track_dir)
{
	DiagDirection dir = TrackdirToExitdir(track_dir);

	if (IsTileType(tile, MP_TUNNELBRIDGE)) {
		/* Are we just leaving a tunnel/bridge? */
		if (GetTunnelBridgeDirection(tile) == ReverseDiagDir(dir)) {
			TileIndex end = GetOtherTunnelBridgeEnd(tile);

			if (TunnelBridgeIsFree(tile, end, v).Succeeded()) {
				/* Free the reservation only if no other train is on the tiles. */
				SetTunnelBridgeReservation(tile, false);
				SetTunnelBridgeReservation(end, false);

				if (_settings_client.gui.show_track_reservation) {
					MarkTileDirtyByTile(tile);
					MarkTileDirtyByTile(end);
				}
			}
		}
	} else if (IsRailStationTile(tile)) {
		TileIndex new_tile = TileAddByDiagDir(tile, dir);
		/* If the new tile is not a further tile of the same station, we
		 * clear the reservation for the whole platform. */
		if (!IsCompatibleTrainStationTile(new_tile, tile)) {
			SetRailStationPlatformReservation(tile, ReverseDiagDir(dir), false);
		}
	} else {
		/* Any other tile */
		UnreserveRailTrack(tile, TrackdirToTrack(track_dir));
	}
}

/** Free the reserved path in front of a vehicle. */
void FreeTrainTrackReservation(const Train *v, TileIndex origin, Trackdir orig_td)
{
	assert(v->IsFrontEngine());

	TileIndex tile = origin != INVALID_TILE ? origin : v->tile;
	Trackdir  td = orig_td != INVALID_TRACKDIR ? orig_td : v->GetVehicleTrackdir();
	bool      free_tile = tile != v->tile || !(IsRailStationTile(v->tile) || IsTileType(v->tile, MP_TUNNELBRIDGE));
	StationID station_id = IsRailStationTile(v->tile) ? GetStationIndex(v->tile) : INVALID_STATION;

	/* Don't free reservation if it's not ours. */
	if (TracksOverlap(GetReservedTrackbits(tile) | TrackToTrackBits(TrackdirToTrack(td)))) return;

	CFollowTrackRail ft(v, GetRailTypeInfo(v->railtype)->compatible_railtypes);
	while (ft.Follow(tile, td)) {
		tile = ft.m_new_tile;
		TrackdirBits bits = ft.m_new_td_bits & TrackBitsToTrackdirBits(GetReservedTrackbits(tile));
		td = RemoveFirstTrackdir(&bits);
		assert(bits == TRACKDIR_BIT_NONE);

		if (!IsValidTrackdir(td)) break;

		if (IsTileType(tile, MP_RAILWAY)) {
			if (HasSignalOnTrackdir(tile, td) && !IsPbsSignal(GetSignalType(tile, TrackdirToTrack(td)))) {
				/* Conventional signal along trackdir: remove reservation and stop. */
				UnreserveRailTrack(tile, TrackdirToTrack(td));
				break;
			}
			if (HasPbsSignalOnTrackdir(tile, td)) {
				if (GetSignalStateByTrackdir(tile, td) == SIGNAL_STATE_RED) {
					/* Red PBS signal? Can't be our reservation, would be green then. */
					break;
				} else {
					/* Turn the signal back to red. */
					SetSignalStateByTrackdir(tile, td, SIGNAL_STATE_RED);
					MarkTileDirtyByTile(tile);
				}
			} else if (HasSignalOnTrackdir(tile, ReverseTrackdir(td)) && IsOnewaySignal(tile, TrackdirToTrack(td))) {
				break;
			}
		}

		/* Don't free first station/bridge/tunnel if we are on it. */
		if (free_tile || (!(ft.m_is_station && GetStationIndex(ft.m_new_tile) == station_id) && !ft.m_is_tunnel && !ft.m_is_bridge)) ClearPathReservation(v, tile, td);

		free_tile = true;
	}
}

static const byte _initial_tile_subcoord[6][4][3] = {
{{ 15, 8, 1 }, { 0, 0, 0 }, { 0, 8, 5 }, { 0,  0, 0 }},
{{  0, 0, 0 }, { 8, 0, 3 }, { 0, 0, 0 }, { 8, 15, 7 }},
{{  0, 0, 0 }, { 7, 0, 2 }, { 0, 7, 6 }, { 0,  0, 0 }},
{{ 15, 8, 2 }, { 0, 0, 0 }, { 0, 0, 0 }, { 8, 15, 6 }},
{{ 15, 7, 0 }, { 8, 0, 4 }, { 0, 0, 0 }, { 0,  0, 0 }},
{{  0, 0, 0 }, { 0, 0, 0 }, { 0, 8, 4 }, { 7, 15, 0 }},
};

/**
 * Perform pathfinding for a train.
 *
 * @param v The train
 * @param tile The tile the train is about to enter
 * @param enterdir Diagonal direction the train is coming from
 * @param tracks Usable tracks on the new tile
 * @param path_found [out] Whether a path has been found or not.
 * @param do_track_reservation Path reservation is requested
 * @param dest [out] State and destination of the requested path
 * @return The best track the train should follow
 */
static Track DoTrainPathfind(const Train *v, TileIndex tile, DiagDirection enterdir, TrackBits tracks, bool &path_found, bool do_track_reservation, PBSTileInfo *dest)
{
	switch (_settings_game.pf.pathfinder_for_trains) {
		case VPF_NPF: return NPFTrainChooseTrack(v, tile, enterdir, tracks, path_found, do_track_reservation, dest);
		case VPF_YAPF: return YapfTrainChooseTrack(v, tile, enterdir, tracks, path_found, do_track_reservation, dest);

		default: NOT_REACHED();
	}
}

/**
 * Extend a train path as far as possible. Stops on encountering a safe tile,
 * another reservation or a track choice.
 * @return INVALID_TILE indicates that the reservation failed.
 */
static PBSTileInfo ExtendTrainReservation(const Train *v, TrackBits *new_tracks, DiagDirection *enterdir)
{
	PBSTileInfo origin = FollowTrainReservation(v);

	CFollowTrackRail ft(v);

	TileIndex tile = origin.tile;
	Trackdir  cur_td = origin.trackdir;
	while (ft.Follow(tile, cur_td)) {
		if (KillFirstBit(ft.m_new_td_bits) == TRACKDIR_BIT_NONE) {
			/* Possible signal tile. */
			if (HasOnewaySignalBlockingTrackdir(ft.m_new_tile, FindFirstTrackdir(ft.m_new_td_bits))) break;
		}

		if (_settings_game.pf.forbid_90_deg) {
			ft.m_new_td_bits &= ~TrackdirCrossesTrackdirs(ft.m_old_td);
			if (ft.m_new_td_bits == TRACKDIR_BIT_NONE) break;
		}

		/* Station, depot or waypoint are a possible target. */
		bool target_seen = ft.m_is_station || (IsTileType(ft.m_new_tile, MP_RAILWAY) && !IsPlainRail(ft.m_new_tile));
		if (target_seen || KillFirstBit(ft.m_new_td_bits) != TRACKDIR_BIT_NONE) {
			/* Choice found or possible target encountered.
			 * On finding a possible target, we need to stop and let the pathfinder handle the
			 * remaining path. This is because we don't know if this target is in one of our
			 * orders, so we might cause pathfinding to fail later on if we find a choice.
			 * This failure would cause a bogous call to TryReserveSafePath which might reserve
			 * a wrong path not leading to our next destination. */
			if (HasReservedTracks(ft.m_new_tile, TrackdirBitsToTrackBits(TrackdirReachesTrackdirs(ft.m_old_td)))) break;

			/* If we did skip some tiles, backtrack to the first skipped tile so the pathfinder
			 * actually starts its search at the first unreserved tile. */
			if (ft.m_tiles_skipped != 0) ft.m_new_tile -= TileOffsByDiagDir(ft.m_exitdir) * ft.m_tiles_skipped;

			/* Choice found, path valid but not okay. Save info about the choice tile as well. */
			if (new_tracks) *new_tracks = TrackdirBitsToTrackBits(ft.m_new_td_bits);
			if (enterdir) *enterdir = ft.m_exitdir;
			return PBSTileInfo(ft.m_new_tile, ft.m_old_td, false);
		}

		tile = ft.m_new_tile;
		cur_td = FindFirstTrackdir(ft.m_new_td_bits);

		if (IsSafeWaitingPosition(v, tile, cur_td, true, _settings_game.pf.forbid_90_deg)) {
			bool wp_free = IsWaitingPositionFree(v, tile, cur_td, _settings_game.pf.forbid_90_deg);
			if (!(wp_free && TryReserveRailTrack(tile, TrackdirToTrack(cur_td)))) break;
			/* Safe position is all good, path valid and okay. */
			return PBSTileInfo(tile, cur_td, true);
		}

		if (!TryReserveRailTrack(tile, TrackdirToTrack(cur_td))) break;
	}

	if (ft.m_err == CFollowTrackRail::EC_OWNER || ft.m_err == CFollowTrackRail::EC_NO_WAY) {
		/* End of line, path valid and okay. */
		return PBSTileInfo(ft.m_old_tile, ft.m_old_td, true);
	}

	/* Sorry, can't reserve path, back out. */
	tile = origin.tile;
	cur_td = origin.trackdir;
	TileIndex stopped = ft.m_old_tile;
	Trackdir  stopped_td = ft.m_old_td;
	while (tile != stopped || cur_td != stopped_td) {
		if (!ft.Follow(tile, cur_td)) break;

		if (_settings_game.pf.forbid_90_deg) {
			ft.m_new_td_bits &= ~TrackdirCrossesTrackdirs(ft.m_old_td);
			assert(ft.m_new_td_bits != TRACKDIR_BIT_NONE);
		}
		assert(KillFirstBit(ft.m_new_td_bits) == TRACKDIR_BIT_NONE);

		tile = ft.m_new_tile;
		cur_td = FindFirstTrackdir(ft.m_new_td_bits);

		UnreserveRailTrack(tile, TrackdirToTrack(cur_td));
	}

	/* Path invalid. */
	return PBSTileInfo();
}

/**
 * Try to reserve any path to a safe tile, ignoring the vehicle's destination.
 * Safe tiles are tiles in front of a signal, depots and station tiles at end of line.
 *
 * @param v The vehicle.
 * @param tile The tile the search should start from.
 * @param td The trackdir the search should start from.
 * @param override_railtype Whether all physically compatible railtypes should be followed.
 * @return True if a path to a safe stopping tile could be reserved.
 */
static bool TryReserveSafeTrack(const Train *v, TileIndex tile, Trackdir td, bool override_tailtype)
{
	switch (_settings_game.pf.pathfinder_for_trains) {
		case VPF_NPF: return NPFTrainFindNearestSafeTile(v, tile, td, override_tailtype);
		case VPF_YAPF: return YapfTrainFindNearestSafeTile(v, tile, td, override_tailtype);

		default: NOT_REACHED();
	}
}

/** This class will save the current order of a vehicle and restore it on destruction. */
class VehicleOrderSaver
{
private:
	Vehicle        *v;
	Order          old_order;
	TileIndex      old_dest_tile;
	StationID      old_last_station_visited;
	VehicleOrderID index;

public:
	VehicleOrderSaver(Vehicle *_v) :
		v(_v),
		old_order(_v->current_order),
		old_dest_tile(_v->dest_tile),
		old_last_station_visited(_v->last_station_visited),
		index(_v->cur_order_index)
	{
	}

	~VehicleOrderSaver()
	{
		this->v->current_order = this->old_order;
		this->v->dest_tile = this->old_dest_tile;
		this->v->last_station_visited = this->old_last_station_visited;
	}

	/**
	 * Set the current vehicle order to the next order in the order list.
	 * @param skip_first Shall the first (i.e. active) order be skipped?
	 * @return True if a suitable next order could be found.
	 */
	bool SwitchToNextOrder(bool skip_first)
	{
		if (this->v->GetNumOrders() == 0) return false;

		if (skip_first) ++this->index;

		int conditional_depth = 0;

		do {
			/* Wrap around. */
			if (this->index >= this->v->GetNumOrders()) this->index = 0;

			Order *order = this->v->GetOrder(this->index);
			assert(order != NULL);

			switch (order->GetType()) {
				case OT_GOTO_DEPOT:
					/* Skip service in depot orders when the train doesn't need service. */
					if ((order->GetDepotOrderType() & ODTFB_SERVICE) && !this->v->NeedsServicing()) break;
				case OT_GOTO_STATION:
				case OT_GOTO_WAYPOINT:
					this->v->current_order = *order;
					UpdateOrderDest(this->v, order);
					return true;
				case OT_CONDITIONAL: {
					if (conditional_depth > this->v->GetNumOrders()) return false;
					VehicleOrderID next = ProcessConditionalOrder(order, this->v);
					if (next != INVALID_VEH_ORDER_ID) {
						conditional_depth++;
						this->index = next;
						/* Don't increment next, so no break here. */
						continue;
					}
					break;
				}
				default:
					break;
			}
			/* Don't increment inside the while because otherwise conditional
			 * orders can lead to an infinite loop. */
			++this->index;
		} while (this->index != this->v->cur_order_index);

		return false;
	}
};

/* choose a track */
static Track ChooseTrainTrack(Train *v, TileIndex tile, DiagDirection enterdir, TrackBits tracks, bool force_res, bool *got_reservation, bool mark_stuck)
{
	Track best_track = INVALID_TRACK;
	bool do_track_reservation = _settings_game.pf.reserve_paths || force_res;
	bool changed_signal = false;

	assert((tracks & ~TRACK_BIT_MASK) == 0);

	if (got_reservation != NULL) *got_reservation = false;

	/* Don't use tracks here as the setting to forbid 90 deg turns might have been switched between reservation and now. */
	TrackBits res_tracks = (TrackBits)(GetReservedTrackbits(tile) & DiagdirReachesTracks(enterdir));
	/* Do we have a suitable reserved track? */
	if (res_tracks != TRACK_BIT_NONE) return FindFirstTrack(res_tracks);

	/* Quick return in case only one possible track is available */
	if (KillFirstBit(tracks) == TRACK_BIT_NONE) {
		Track track = FindFirstTrack(tracks);
		/* We need to check for signals only here, as a junction tile can't have signals. */
		if (track != INVALID_TRACK && HasPbsSignalOnTrackdir(tile, TrackEnterdirToTrackdir(track, enterdir))) {
			do_track_reservation = true;
			changed_signal = true;
			SetSignalStateByTrackdir(tile, TrackEnterdirToTrackdir(track, enterdir), SIGNAL_STATE_GREEN);
		} else if (!do_track_reservation) {
			return track;
		}
		best_track = track;
	}

	PBSTileInfo   res_dest(tile, INVALID_TRACKDIR, false);
	DiagDirection dest_enterdir = enterdir;
	if (do_track_reservation) {
		/* Check if the train needs service here, so it has a chance to always find a depot.
		 * Also check if the current order is a service order so we don't reserve a path to
		 * the destination but instead to the next one if service isn't needed. */
		CheckIfTrainNeedsService(v);
		if (v->current_order.IsType(OT_DUMMY) || v->current_order.IsType(OT_CONDITIONAL) || v->current_order.IsType(OT_GOTO_DEPOT)) ProcessOrders(v);

		res_dest = ExtendTrainReservation(v, &tracks, &dest_enterdir);
		if (res_dest.tile == INVALID_TILE) {
			/* Reservation failed? */
			if (mark_stuck) MarkTrainAsStuck(v);
			if (changed_signal) SetSignalStateByTrackdir(tile, TrackEnterdirToTrackdir(best_track, enterdir), SIGNAL_STATE_RED);
			return FindFirstTrack(tracks);
		}
	}

	/* Save the current train order. The destructor will restore the old order on function exit. */
	VehicleOrderSaver orders(v);

	/* If the current tile is the destination of the current order and
	 * a reservation was requested, advance to the next order.
	 * Don't advance on a depot order as depots are always safe end points
	 * for a path and no look-ahead is necessary. This also avoids a
	 * problem with depot orders not part of the order list when the
	 * order list itself is empty. */
	if (v->current_order.IsType(OT_LEAVESTATION)) {
		orders.SwitchToNextOrder(false);
	} else if (v->current_order.IsType(OT_LOADING) || (!v->current_order.IsType(OT_GOTO_DEPOT) && (
			v->current_order.IsType(OT_GOTO_STATION) ?
			IsRailStationTile(v->tile) && v->current_order.GetDestination() == GetStationIndex(v->tile) :
			v->tile == v->dest_tile))) {
		orders.SwitchToNextOrder(true);
	}

	if (res_dest.tile != INVALID_TILE && !res_dest.okay) {
		/* Pathfinders are able to tell that route was only 'guessed'. */
		bool      path_found = true;
		TileIndex new_tile = res_dest.tile;

		Track next_track = DoTrainPathfind(v, new_tile, dest_enterdir, tracks, path_found, do_track_reservation, &res_dest);
		if (new_tile == tile) best_track = next_track;
		v->HandlePathfindingResult(path_found);
	}

	/* No track reservation requested -> finished. */
	if (!do_track_reservation) return best_track;

	/* A path was found, but could not be reserved. */
	if (res_dest.tile != INVALID_TILE && !res_dest.okay) {
		if (mark_stuck) MarkTrainAsStuck(v);
		FreeTrainTrackReservation(v);
		return best_track;
	}

	/* No possible reservation target found, we are probably lost. */
	if (res_dest.tile == INVALID_TILE) {
		/* Try to find any safe destination. */
		PBSTileInfo origin = FollowTrainReservation(v);
		if (TryReserveSafeTrack(v, origin.tile, origin.trackdir, false)) {
			TrackBits res = GetReservedTrackbits(tile) & DiagdirReachesTracks(enterdir);
			best_track = FindFirstTrack(res);
			TryReserveRailTrack(v->tile, TrackdirToTrack(v->GetVehicleTrackdir()));
			if (got_reservation != NULL) *got_reservation = true;
			if (changed_signal) MarkTileDirtyByTile(tile);
		} else {
			FreeTrainTrackReservation(v);
			if (mark_stuck) MarkTrainAsStuck(v);
		}
		return best_track;
	}

	if (got_reservation != NULL) *got_reservation = true;

	/* Reservation target found and free, check if it is safe. */
	while (!IsSafeWaitingPosition(v, res_dest.tile, res_dest.trackdir, true, _settings_game.pf.forbid_90_deg)) {
		/* Extend reservation until we have found a safe position. */
		DiagDirection exitdir = TrackdirToExitdir(res_dest.trackdir);
		TileIndex     next_tile = TileAddByDiagDir(res_dest.tile, exitdir);
		TrackBits     reachable = TrackdirBitsToTrackBits((TrackdirBits)(GetTileTrackStatus(next_tile, TRANSPORT_RAIL, 0))) & DiagdirReachesTracks(exitdir);
		if (_settings_game.pf.forbid_90_deg) {
			reachable &= ~TrackCrossesTracks(TrackdirToTrack(res_dest.trackdir));
		}

		/* Get next order with destination. */
		if (orders.SwitchToNextOrder(true)) {
			PBSTileInfo cur_dest;
			bool path_found;
			DoTrainPathfind(v, next_tile, exitdir, reachable, path_found, true, &cur_dest);
			if (cur_dest.tile != INVALID_TILE) {
				res_dest = cur_dest;
				if (res_dest.okay) continue;
				/* Path found, but could not be reserved. */
				FreeTrainTrackReservation(v);
				if (mark_stuck) MarkTrainAsStuck(v);
				if (got_reservation != NULL) *got_reservation = false;
				changed_signal = false;
				break;
			}
		}
		/* No order or no safe position found, try any position. */
		if (!TryReserveSafeTrack(v, res_dest.tile, res_dest.trackdir, true)) {
			FreeTrainTrackReservation(v);
			if (mark_stuck) MarkTrainAsStuck(v);
			if (got_reservation != NULL) *got_reservation = false;
			changed_signal = false;
		}
		break;
	}

	TryReserveRailTrack(v->tile, TrackdirToTrack(v->GetVehicleTrackdir()));

	if (changed_signal) MarkTileDirtyByTile(tile);

	return best_track;
}

/**
 * Try to reserve a path to a safe position.
 *
 * @param v The vehicle
 * @param mark_as_stuck Should the train be marked as stuck on a failed reservation?
 * @param first_tile_okay True if no path should be reserved if the current tile is a safe position.
 * @return True if a path could be reserved.
 */
bool TryPathReserve(Train *v, bool mark_as_stuck, bool first_tile_okay)
{
	assert(v->IsFrontEngine());

	/* We have to handle depots specially as the track follower won't look
	 * at the depot tile itself but starts from the next tile. If we are still
	 * inside the depot, a depot reservation can never be ours. */
	if (v->track == TRACK_BIT_DEPOT) {
		if (HasDepotReservation(v->tile)) {
			if (mark_as_stuck) MarkTrainAsStuck(v);
			return false;
		} else {
			/* Depot not reserved, but the next tile might be. */
			TileIndex next_tile = TileAddByDiagDir(v->tile, GetRailDepotDirection(v->tile));
			if (HasReservedTracks(next_tile, DiagdirReachesTracks(GetRailDepotDirection(v->tile)))) return false;
		}
	}

	Vehicle *other_train = NULL;
	PBSTileInfo origin = FollowTrainReservation(v, &other_train);
	/* The path we are driving on is already blocked by some other train.
	 * This can only happen in certain situations when mixing path and
	 * block signals or when changing tracks and/or signals.
	 * Exit here as doing any further reservations will probably just
	 * make matters worse. */
	if (other_train != NULL && other_train->index != v->index) {
		if (mark_as_stuck) MarkTrainAsStuck(v);
		return false;
	}
	/* If we have a reserved path and the path ends at a safe tile, we are finished already. */
	if (origin.okay && (v->tile != origin.tile || first_tile_okay)) {
		/* Can't be stuck then. */
		if (HasBit(v->flags, VRF_TRAIN_STUCK)) SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
		ClrBit(v->flags, VRF_TRAIN_STUCK);
		return true;
	}

	/* If we are in a depot, tentatively reserve the depot. */
	if (v->track == TRACK_BIT_DEPOT) {
		SetDepotReservation(v->tile, true);
		if (_settings_client.gui.show_track_reservation) MarkTileDirtyByTile(v->tile);
	}

	DiagDirection exitdir = TrackdirToExitdir(origin.trackdir);
	TileIndex     new_tile = TileAddByDiagDir(origin.tile, exitdir);
	TrackBits     reachable = TrackdirBitsToTrackBits(TrackStatusToTrackdirBits(GetTileTrackStatus(new_tile, TRANSPORT_RAIL, 0)) & DiagdirReachesTrackdirs(exitdir));

	if (_settings_game.pf.forbid_90_deg) reachable &= ~TrackCrossesTracks(TrackdirToTrack(origin.trackdir));

	bool res_made = false;
	ChooseTrainTrack(v, new_tile, exitdir, reachable, true, &res_made, mark_as_stuck);

	if (!res_made) {
		/* Free the depot reservation as well. */
		if (v->track == TRACK_BIT_DEPOT) SetDepotReservation(v->tile, false);
		return false;
	}

	if (HasBit(v->flags, VRF_TRAIN_STUCK)) {
		v->wait_counter = 0;
		SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
	}
	ClrBit(v->flags, VRF_TRAIN_STUCK);
	return true;
}


static bool CheckReverseTrain(const Train *v)
{
	if (_settings_game.difficulty.line_reverse_mode != 0 ||
			v->track == TRACK_BIT_DEPOT || v->track == TRACK_BIT_WORMHOLE ||
			!(v->direction & 1)) {
		return false;
	}

	assert(v->track != TRACK_BIT_NONE);

	switch (_settings_game.pf.pathfinder_for_trains) {
		case VPF_NPF: return NPFTrainCheckReverse(v);
		case VPF_YAPF: return YapfTrainCheckReverse(v);

		default: NOT_REACHED();
	}
}

TileIndex Train::GetOrderStationLocation(StationID station)
{
	if (station == this->last_station_visited) this->last_station_visited = INVALID_STATION;

	const Station *st = Station::Get(station);
	if (!(st->facilities & FACIL_TRAIN)) {
		/* The destination station has no trainstation tiles. */
		this->IncrementOrderIndex();
		return 0;
	}

	return st->xy;
}

void Train::MarkDirty()
{
	Train *v = this;
	do {
		v->UpdateViewport(false, false);
	} while ((v = v->Next()) != NULL);

	/* need to update acceleration and cached values since the goods on the train changed. */
	this->CargoChanged();
	this->UpdateAcceleration();
}

/**
 * This function looks at the vehicle and updates its speed (cur_speed
 * and subspeed) variables. Furthermore, it returns the distance that
 * the train can drive this tick. #Vehicle::GetAdvanceDistance() determines
 * the distance to drive before moving a step on the map.
 * @return distance to drive.
 */
int Train::UpdateSpeed()
{
	uint accel;
	uint16 max_speed;

	switch (_settings_game.vehicle.train_acceleration_model) {
		default: NOT_REACHED();
		case AM_ORIGINAL:
			max_speed = this->gcache.cached_max_track_speed;
			accel = this->acceleration * (this->GetAccelerationStatus() == AS_BRAKE ? -4 : 2);
			break;
		case AM_REALISTIC:
			max_speed = this->GetCurrentMaxSpeed();
			accel = this->GetAcceleration();
			break;
	}

	uint spd = this->subspeed + accel;
	this->subspeed = (byte)spd;
	{
		int tempmax = max_speed;
		if (this->cur_speed > max_speed) {
			tempmax = max(this->cur_speed - (this->cur_speed / 10) - 1, tempmax);
		}
		/* Force a minimum speed of 1 km/h when realistic acceleration is on and the train is not braking. */
		int min_speed = (_settings_game.vehicle.train_acceleration_model == AM_ORIGINAL || this->GetAccelerationStatus() == AS_BRAKE) ? 0 : 2;
		this->cur_speed = spd = Clamp(this->cur_speed + ((int)spd >> 8), min_speed, tempmax);
	}

	int scaled_spd = this->GetAdvanceSpeed(spd);

	scaled_spd += this->progress;
	this->progress = 0; // set later in TrainLocoHandler or TrainController
	return scaled_spd;
}

/**
 * Trains enters a station, send out a news item if it is the first train, and start loading.
 * @param v Train that entered the station.
 * @param station Station visited.
 */
static void TrainEnterStation(Train *v, StationID station)
{
	v->last_station_visited = station;

	/* check if a train ever visited this station before */
	Station *st = Station::Get(station);
	if (!(st->had_vehicle_of_type & HVOT_TRAIN)) {
		st->had_vehicle_of_type |= HVOT_TRAIN;
		SetDParam(0, st->index);
		AddVehicleNewsItem(
			STR_NEWS_FIRST_TRAIN_ARRIVAL,
			v->owner == _local_company ? NS_ARRIVAL_COMPANY : NS_ARRIVAL_OTHER,
			v->index,
			st->index
		);
		AI::NewEvent(v->owner, new AIEventStationFirstVehicle(st->index, v->index));
	}

	v->force_proceed = TFP_NONE;
	SetWindowDirty(WC_VEHICLE_VIEW, v->index);

	v->BeginLoading();

	TriggerStationAnimation(st, v->tile, SAT_TRAIN_ARRIVES);
}

/* Check if the vehicle is compatible with the specified tile */
static inline bool CheckCompatibleRail(const Train *v, TileIndex tile)
{
	return IsTileOwner(tile, v->owner) &&
			(!v->IsFrontEngine() || HasBit(v->compatible_railtypes, GetRailType(tile)));
}

struct RailtypeSlowdownParams {
	byte small_turn, large_turn;
	byte z_up; // fraction to remove when moving up
	byte z_down; // fraction to remove when moving down
};

static const RailtypeSlowdownParams _railtype_slowdown[] = {
	/* normal accel */
	{256 / 4, 256 / 2, 256 / 4, 2}, ///< normal
	{256 / 4, 256 / 2, 256 / 4, 2}, ///< electrified
	{256 / 4, 256 / 2, 256 / 4, 2}, ///< monorail
	{0,       256 / 2, 256 / 4, 2}, ///< maglev
};

/** Modify the speed of the vehicle due to a change in altitude */
static inline void AffectSpeedByZChange(Train *v, byte old_z)
{
	if (old_z == v->z_pos || _settings_game.vehicle.train_acceleration_model != AM_ORIGINAL) return;

	const RailtypeSlowdownParams *rsp = &_railtype_slowdown[v->railtype];

	if (old_z < v->z_pos) {
		v->cur_speed -= (v->cur_speed * rsp->z_up >> 8);
	} else {
		uint16 spd = v->cur_speed + rsp->z_down;
		if (spd <= v->gcache.cached_max_track_speed) v->cur_speed = spd;
	}
}

static bool TrainMovedChangeSignals(TileIndex tile, DiagDirection dir)
{
	if (IsTileType(tile, MP_RAILWAY) &&
			GetRailTileType(tile) == RAIL_TILE_SIGNALS) {
		TrackdirBits tracks = TrackBitsToTrackdirBits(GetTrackBits(tile)) & DiagdirReachesTrackdirs(dir);
		Trackdir trackdir = FindFirstTrackdir(tracks);
		if (UpdateSignalsOnSegment(tile,  TrackdirToExitdir(trackdir), GetTileOwner(tile)) == SIGSEG_PBS && HasSignalOnTrackdir(tile, trackdir)) {
			/* A PBS block with a non-PBS signal facing us? */
			if (!IsPbsSignal(GetSignalType(tile, TrackdirToTrack(trackdir)))) return true;
		}
	}
	return false;
}

/**
 * Tries to reserve track under whole train consist
 */
void Train::ReserveTrackUnderConsist() const
{
	for (const Train *u = this; u != NULL; u = u->Next()) {
		switch (u->track) {
			case TRACK_BIT_WORMHOLE:
				TryReserveRailTrack(u->tile, DiagDirToDiagTrack(GetTunnelBridgeDirection(u->tile)));
				break;
			case TRACK_BIT_DEPOT:
				break;
			default:
				TryReserveRailTrack(u->tile, TrackBitsToTrack(u->track));
				break;
		}
	}
}

uint Train::Crash(bool flooded)
{
	uint pass = 0;
	if (this->IsFrontEngine()) {
		pass += 2; // driver

		/* Remove the reserved path in front of the train if it is not stuck.
		 * Also clear all reserved tracks the train is currently on. */
		if (!HasBit(this->flags, VRF_TRAIN_STUCK)) FreeTrainTrackReservation(this);
		for (const Train *v = this; v != NULL; v = v->Next()) {
			ClearPathReservation(v, v->tile, v->GetVehicleTrackdir());
			if (IsTileType(v->tile, MP_TUNNELBRIDGE)) {
				/* ClearPathReservation will not free the wormhole exit
				 * if the train has just entered the wormhole. */
				SetTunnelBridgeReservation(GetOtherTunnelBridgeEnd(v->tile), false);
			}
		}

		/* we may need to update crossing we were approaching,
		 * but must be updated after the train has been marked crashed */
		TileIndex crossing = TrainApproachingCrossingTile(this);
		if (crossing != INVALID_TILE) UpdateLevelCrossing(crossing);

		/* Remove the loading indicators (if any) */
		HideFillingPercent(&this->fill_percent_te_id);
	}

	pass += this->GroundVehicleBase::Crash(flooded);

	this->crash_anim_pos = flooded ? 4000 : 1; // max 4440, disappear pretty fast when flooded
	return pass;
}

/**
 * Marks train as crashed and creates an AI event.
 * Doesn't do anything if the train is crashed already.
 * @param v first vehicle of chain
 * @return number of victims (including 2 drivers; zero if train was already crashed)
 */
static uint TrainCrashed(Train *v)
{
	uint num = 0;

	/* do not crash train twice */
	if (!(v->vehstatus & VS_CRASHED)) {
		num = v->Crash();
		AI::NewEvent(v->owner, new AIEventVehicleCrashed(v->index, v->tile, AIEventVehicleCrashed::CRASH_TRAIN));
	}

	/* Try to re-reserve track under already crashed train too.
	 * Crash() clears the reservation! */
	v->ReserveTrackUnderConsist();

	return num;
}

struct TrainCollideChecker {
	Train *v; ///< vehicle we are testing for collision
	uint num; ///< number of victims if train collided
};

static Vehicle *FindTrainCollideEnum(Vehicle *v, void *data)
{
	TrainCollideChecker *tcc = (TrainCollideChecker*)data;

	/* not a train or in depot */
	if (v->type != VEH_TRAIN || Train::From(v)->track == TRACK_BIT_DEPOT) return NULL;

	/* do not crash into trains of another company. */
	if (v->owner != tcc->v->owner) return NULL;

	/* get first vehicle now to make most usual checks faster */
	Train *coll = Train::From(v)->First();

	/* can't collide with own wagons */
	if (coll == tcc->v) return NULL;

	int x_diff = v->x_pos - tcc->v->x_pos;
	int y_diff = v->y_pos - tcc->v->y_pos;

	/* Do fast calculation to check whether trains are not in close vicinity
	 * and quickly reject trains distant enough for any collision.
	 * Differences are shifted by 7, mapping range [-7 .. 8] into [0 .. 15]
	 * Differences are then ORed and then we check for any higher bits */
	uint hash = (y_diff + 7) | (x_diff + 7);
	if (hash & ~15) return NULL;

	/* Slower check using multiplication */
	if (x_diff * x_diff + y_diff * y_diff > 25) return NULL;

	/* Happens when there is a train under bridge next to bridge head */
	if (abs(v->z_pos - tcc->v->z_pos) > 5) return NULL;

	/* crash both trains */
	tcc->num += TrainCrashed(tcc->v);
	tcc->num += TrainCrashed(coll);

	return NULL; // continue searching
}

/**
 * Checks whether the specified train has a collision with another vehicle. If
 * so, destroys this vehicle, and the other vehicle if its subtype has TS_Front.
 * Reports the incident in a flashy news item, modifies station ratings and
 * plays a sound.
 */
static bool CheckTrainCollision(Train *v)
{
	/* can't collide in depot */
	if (v->track == TRACK_BIT_DEPOT) return false;

	assert(v->track == TRACK_BIT_WORMHOLE || TileVirtXY(v->x_pos, v->y_pos) == v->tile);

	TrainCollideChecker tcc;
	tcc.v = v;
	tcc.num = 0;

	/* find colliding vehicles */
	if (v->track == TRACK_BIT_WORMHOLE) {
		FindVehicleOnPos(v->tile, &tcc, FindTrainCollideEnum);
		FindVehicleOnPos(GetOtherTunnelBridgeEnd(v->tile), &tcc, FindTrainCollideEnum);
	} else {
		FindVehicleOnPosXY(v->x_pos, v->y_pos, &tcc, FindTrainCollideEnum);
	}

	/* any dead -> no crash */
	if (tcc.num == 0) return false;

	SetDParam(0, tcc.num);
	AddVehicleNewsItem(STR_NEWS_TRAIN_CRASH,
		NS_ACCIDENT,
		v->index
	);

	ModifyStationRatingAround(v->tile, v->owner, -160, 30);
	SndPlayVehicleFx(SND_13_BIG_CRASH, v);
	return true;
}

static Vehicle *CheckTrainAtSignal(Vehicle *v, void *data)
{
	if (v->type != VEH_TRAIN || (v->vehstatus & VS_CRASHED)) return NULL;

	Train *t = Train::From(v);
	DiagDirection exitdir = *(DiagDirection *)data;

	/* not front engine of a train, inside wormhole or depot, crashed */
	if (!t->IsFrontEngine() || !(t->track & TRACK_BIT_MASK)) return NULL;

	if (t->cur_speed > 5 || TrainExitDir(t->direction, t->track) != exitdir) return NULL;

	return t;
}

static void TrainController(Train *v, Vehicle *nomove)
{
	Train *first = v->First();
	Train *prev;
	bool direction_changed = false; // has direction of any part changed?

	/* For every vehicle after and including the given vehicle */
	for (prev = v->Previous(); v != nomove; prev = v, v = v->Next()) {
		DiagDirection enterdir = DIAGDIR_BEGIN;
		bool update_signals_crossing = false; // will we update signals or crossing state?

		GetNewVehiclePosResult gp = GetNewVehiclePos(v);
		if (v->track != TRACK_BIT_WORMHOLE) {
			/* Not inside tunnel */
			if (gp.old_tile == gp.new_tile) {
				/* Staying in the old tile */
				if (v->track == TRACK_BIT_DEPOT) {
					/* Inside depot */
					gp.x = v->x_pos;
					gp.y = v->y_pos;
				} else {
					/* Not inside depot */

					/* Reverse when we are at the end of the track already, do not move to the new position */
					if (v->IsFrontEngine() && !TrainCheckIfLineEnds(v)) return;

					uint32 r = VehicleEnterTile(v, gp.new_tile, gp.x, gp.y);
					if (HasBit(r, VETS_CANNOT_ENTER)) {
						goto invalid_rail;
					}
					if (HasBit(r, VETS_ENTERED_STATION)) {
						/* The new position is the end of the platform */
						TrainEnterStation(v, r >> VETS_STATION_ID_OFFSET);
					}
				}
			} else {
				/* A new tile is about to be entered. */

				/* Determine what direction we're entering the new tile from */
				enterdir = DiagdirBetweenTiles(gp.old_tile, gp.new_tile);
				assert(IsValidDiagDirection(enterdir));

				/* Get the status of the tracks in the new tile and mask
				 * away the bits that aren't reachable. */
				TrackStatus ts = GetTileTrackStatus(gp.new_tile, TRANSPORT_RAIL, 0, ReverseDiagDir(enterdir));
				TrackdirBits reachable_trackdirs = DiagdirReachesTrackdirs(enterdir);

				TrackdirBits trackdirbits = TrackStatusToTrackdirBits(ts) & reachable_trackdirs;
				TrackBits red_signals = TrackdirBitsToTrackBits(TrackStatusToRedSignals(ts) & reachable_trackdirs);

				TrackBits bits = TrackdirBitsToTrackBits(trackdirbits);
				if (_settings_game.pf.forbid_90_deg && prev == NULL) {
					/* We allow wagons to make 90 deg turns, because forbid_90_deg
					 * can be switched on halfway a turn */
					bits &= ~TrackCrossesTracks(FindFirstTrack(v->track));
				}

				if (bits == TRACK_BIT_NONE) goto invalid_rail;

				/* Check if the new tile constrains tracks that are compatible
				 * with the current train, if not, bail out. */
				if (!CheckCompatibleRail(v, gp.new_tile)) goto invalid_rail;

				TrackBits chosen_track;
				if (prev == NULL) {
					/* Currently the locomotive is active. Determine which one of the
					 * available tracks to choose */
					chosen_track = TrackToTrackBits(ChooseTrainTrack(v, gp.new_tile, enterdir, bits, false, NULL, true));
					assert(chosen_track & (bits | GetReservedTrackbits(gp.new_tile)));

					if (v->force_proceed != TFP_NONE && IsPlainRailTile(gp.new_tile) && HasSignals(gp.new_tile)) {
						/* For each signal we find decrease the counter by one.
						 * We start at two, so the first signal we pass decreases
						 * this to one, then if we reach the next signal it is
						 * decreased to zero and we won't pass that new signal. */
						Trackdir dir = FindFirstTrackdir(trackdirbits);
						if (GetSignalType(gp.new_tile, TrackdirToTrack(dir)) != SIGTYPE_PBS ||
								!HasSignalOnTrackdir(gp.new_tile, ReverseTrackdir(dir))) {
							/* However, we do not want to be stopped by PBS signals
							 * entered via the back. */
							v->force_proceed = (v->force_proceed == TFP_SIGNAL) ? TFP_STUCK : TFP_NONE;
							SetWindowDirty(WC_VEHICLE_VIEW, v->index);
						}
					}

					/* Check if it's a red signal and that force proceed is not clicked. */
					if ((red_signals & chosen_track) && v->force_proceed == TFP_NONE) {
						/* In front of a red signal */
						Trackdir i = FindFirstTrackdir(trackdirbits);

						/* Don't handle stuck trains here. */
						if (HasBit(v->flags, VRF_TRAIN_STUCK)) return;

						if (!HasSignalOnTrackdir(gp.new_tile, ReverseTrackdir(i))) {
							v->cur_speed = 0;
							v->subspeed = 0;
							v->progress = 255 - 100;
							if (_settings_game.pf.wait_oneway_signal == 255 || ++v->wait_counter < _settings_game.pf.wait_oneway_signal * 20) return;
						} else if (HasSignalOnTrackdir(gp.new_tile, i)) {
							v->cur_speed = 0;
							v->subspeed = 0;
							v->progress = 255 - 10;
							if (_settings_game.pf.wait_twoway_signal == 255 || ++v->wait_counter < _settings_game.pf.wait_twoway_signal * 73) {
								DiagDirection exitdir = TrackdirToExitdir(i);
								TileIndex o_tile = TileAddByDiagDir(gp.new_tile, exitdir);

								exitdir = ReverseDiagDir(exitdir);

								/* check if a train is waiting on the other side */
								if (!HasVehicleOnPos(o_tile, &exitdir, &CheckTrainAtSignal)) return;
							}
						}

						/* If we would reverse but are currently in a PBS block and
						 * reversing of stuck trains is disabled, don't reverse.
						 * This does not apply if the reason for reversing is a one-way
						 * signal blocking us, because a train would then be stuck forever. */
						if (_settings_game.pf.wait_for_pbs_path == 255 && !HasOnewaySignalBlockingTrackdir(gp.new_tile, i) &&
								UpdateSignalsOnSegment(v->tile, enterdir, v->owner) == SIGSEG_PBS) {
							v->wait_counter = 0;
							return;
						}
						goto reverse_train_direction;
					} else {
						TryReserveRailTrack(gp.new_tile, TrackBitsToTrack(chosen_track));
					}
				} else {
					/* The wagon is active, simply follow the prev vehicle. */
					if (prev->tile == gp.new_tile) {
						/* Choose the same track as prev */
						if (prev->track == TRACK_BIT_WORMHOLE) {
							/* Vehicles entering tunnels enter the wormhole earlier than for bridges.
							 * However, just choose the track into the wormhole. */
							assert(IsTunnel(prev->tile));
							chosen_track = bits;
						} else {
							chosen_track = prev->track;
						}
					} else {
						/* Choose the track that leads to the tile where prev is.
						 * This case is active if 'prev' is already on the second next tile, when 'v' just enters the next tile.
						 * I.e. when the tile between them has only space for a single vehicle like
						 *  1) horizontal/vertical track tiles and
						 *  2) some orientations of tunnel entries, where the vehicle is already inside the wormhole at 8/16 from the tile edge.
						 *     Is also the train just reversing, the wagon inside the tunnel is 'on' the tile of the opposite tunnel entry.
						 */
						static const TrackBits _connecting_track[DIAGDIR_END][DIAGDIR_END] = {
							{TRACK_BIT_X,     TRACK_BIT_LOWER, TRACK_BIT_NONE,  TRACK_BIT_LEFT },
							{TRACK_BIT_UPPER, TRACK_BIT_Y,     TRACK_BIT_LEFT,  TRACK_BIT_NONE },
							{TRACK_BIT_NONE,  TRACK_BIT_RIGHT, TRACK_BIT_X,     TRACK_BIT_UPPER},
							{TRACK_BIT_RIGHT, TRACK_BIT_NONE,  TRACK_BIT_LOWER, TRACK_BIT_Y    }
						};
						DiagDirection exitdir = DiagdirBetweenTiles(gp.new_tile, prev->tile);
						assert(IsValidDiagDirection(exitdir));
						chosen_track = _connecting_track[enterdir][exitdir];
					}
					chosen_track &= bits;
				}

				/* Make sure chosen track is a valid track */
				assert(
						chosen_track == TRACK_BIT_X     || chosen_track == TRACK_BIT_Y ||
						chosen_track == TRACK_BIT_UPPER || chosen_track == TRACK_BIT_LOWER ||
						chosen_track == TRACK_BIT_LEFT  || chosen_track == TRACK_BIT_RIGHT);

				/* Update XY to reflect the entrance to the new tile, and select the direction to use */
				const byte *b = _initial_tile_subcoord[FIND_FIRST_BIT(chosen_track)][enterdir];
				gp.x = (gp.x & ~0xF) | b[0];
				gp.y = (gp.y & ~0xF) | b[1];
				Direction chosen_dir = (Direction)b[2];

				/* Call the landscape function and tell it that the vehicle entered the tile */
				uint32 r = VehicleEnterTile(v, gp.new_tile, gp.x, gp.y);
				if (HasBit(r, VETS_CANNOT_ENTER)) {
					goto invalid_rail;
				}

				if (!HasBit(r, VETS_ENTERED_WORMHOLE)) {
					Track track = FindFirstTrack(chosen_track);
					Trackdir tdir = TrackDirectionToTrackdir(track, chosen_dir);
					if (v->IsFrontEngine() && HasPbsSignalOnTrackdir(gp.new_tile, tdir)) {
						SetSignalStateByTrackdir(gp.new_tile, tdir, SIGNAL_STATE_RED);
						MarkTileDirtyByTile(gp.new_tile);
					}

					/* Clear any track reservation when the last vehicle leaves the tile */
					if (v->Next() == NULL) ClearPathReservation(v, v->tile, v->GetVehicleTrackdir());

					v->tile = gp.new_tile;

					if (GetTileRailType(gp.new_tile) != GetTileRailType(gp.old_tile)) {
						v->First()->RailtypeChanged();
					}

					v->track = chosen_track;
					assert(v->track);
				}

				/* We need to update signal status, but after the vehicle position hash
				 * has been updated by UpdateInclination() */
				update_signals_crossing = true;

				if (chosen_dir != v->direction) {
					if (prev == NULL && _settings_game.vehicle.train_acceleration_model == AM_ORIGINAL) {
						const RailtypeSlowdownParams *rsp = &_railtype_slowdown[v->railtype];
						DirDiff diff = DirDifference(v->direction, chosen_dir);
						v->cur_speed -= (diff == DIRDIFF_45RIGHT || diff == DIRDIFF_45LEFT ? rsp->small_turn : rsp->large_turn) * v->cur_speed >> 8;
					}
					direction_changed = true;
					v->direction = chosen_dir;
				}

				if (v->IsFrontEngine()) {
					v->wait_counter = 0;

					/* If we are approaching a crossing that is reserved, play the sound now. */
					TileIndex crossing = TrainApproachingCrossingTile(v);
					if (crossing != INVALID_TILE && HasCrossingReservation(crossing)) SndPlayTileFx(SND_0E_LEVEL_CROSSING, crossing);

					/* Always try to extend the reservation when entering a tile. */
					CheckNextTrainTile(v);
				}

				if (HasBit(r, VETS_ENTERED_STATION)) {
					/* The new position is the location where we want to stop */
					TrainEnterStation(v, r >> VETS_STATION_ID_OFFSET);
				}
			}
		} else {
			/* In a tunnel or on a bridge
			 * - for tunnels, only the part when the vehicle is not visible (part of enter/exit tile too)
			 * - for bridges, only the middle part - without the bridge heads */
			if (!(v->vehstatus & VS_HIDDEN)) {
				Train *first = v->First();
				first->cur_speed = min(first->cur_speed, GetBridgeSpec(GetBridgeType(v->tile))->speed);
			}

			if (IsTileType(gp.new_tile, MP_TUNNELBRIDGE) && HasBit(VehicleEnterTile(v, gp.new_tile, gp.x, gp.y), VETS_ENTERED_WORMHOLE)) {
				/* Perform look-ahead on tunnel exit. */
				if (v->IsFrontEngine()) {
					TryReserveRailTrack(gp.new_tile, DiagDirToDiagTrack(GetTunnelBridgeDirection(gp.new_tile)));
					CheckNextTrainTile(v);
				}
				/* Prevent v->UpdateInclination() being called with wrong parameters.
				 * This could happen if the train was reversed inside the tunnel/bridge. */
				if (gp.old_tile == gp.new_tile) {
					gp.old_tile = GetOtherTunnelBridgeEnd(gp.old_tile);
				}
			} else {
				v->x_pos = gp.x;
				v->y_pos = gp.y;
				VehicleMove(v, !(v->vehstatus & VS_HIDDEN));
				continue;
			}
		}

		/* update image of train, as well as delta XY */
		v->UpdateDeltaXY(v->direction);

		v->x_pos = gp.x;
		v->y_pos = gp.y;

		/* update the Z position of the vehicle */
		byte old_z = v->UpdateInclination(gp.new_tile != gp.old_tile, false);

		if (prev == NULL) {
			/* This is the first vehicle in the train */
			AffectSpeedByZChange(v, old_z);
		}

		if (update_signals_crossing) {
			if (v->IsFrontEngine()) {
				if (TrainMovedChangeSignals(gp.new_tile, enterdir)) {
					/* We are entering a block with PBS signals right now, but
					 * not through a PBS signal. This means we don't have a
					 * reservation right now. As a conventional signal will only
					 * ever be green if no other train is in the block, getting
					 * a path should always be possible. If the player built
					 * such a strange network that it is not possible, the train
					 * will be marked as stuck and the player has to deal with
					 * the problem. */
					if ((!HasReservedTracks(gp.new_tile, v->track) &&
							!TryReserveRailTrack(gp.new_tile, FindFirstTrack(v->track))) ||
							!TryPathReserve(v)) {
						MarkTrainAsStuck(v);
					}
				}
			}

			/* Signals can only change when the first
			 * (above) or the last vehicle moves. */
			if (v->Next() == NULL) {
				TrainMovedChangeSignals(gp.old_tile, ReverseDiagDir(enterdir));
				if (IsLevelCrossingTile(gp.old_tile)) UpdateLevelCrossing(gp.old_tile);
			}
		}

		/* Do not check on every tick to save some computing time. */
		if (v->IsFrontEngine() && v->tick_counter % _settings_game.pf.path_backoff_interval == 0) CheckNextTrainTile(v);
	}

	if (direction_changed) first->tcache.cached_max_curve_speed = first->GetCurveSpeedLimit();

	return;

invalid_rail:
	/* We've reached end of line?? */
	if (prev != NULL) error("Disconnecting train");

reverse_train_direction:
	v->wait_counter = 0;
	v->cur_speed = 0;
	v->subspeed = 0;
	ReverseTrainDirection(v);
}

/**
 * Collect trackbits of all crashed train vehicles on a tile
 * @param v Vehicle passed from Find/HasVehicleOnPos()
 * @param data trackdirbits for the result
 * @return NULL to iterate over all vehicles on the tile.
 */
static Vehicle *CollectTrackbitsFromCrashedVehiclesEnum(Vehicle *v, void *data)
{
	TrackBits *trackbits = (TrackBits *)data;

	if (v->type == VEH_TRAIN && (v->vehstatus & VS_CRASHED) != 0) {
		TrackBits train_tbits = Train::From(v)->track;
		if (train_tbits == TRACK_BIT_WORMHOLE) {
			/* Vehicle is inside a wormhole, v->track contains no useful value then. */
			*trackbits |= DiagDirToDiagTrackBits(GetTunnelBridgeDirection(v->tile));
		} else if (train_tbits != TRACK_BIT_DEPOT) {
			*trackbits |= train_tbits;
		}
	}

	return NULL;
}

/**
 * Deletes/Clears the last wagon of a crashed train. It takes the engine of the
 * train, then goes to the last wagon and deletes that. Each call to this function
 * will remove the last wagon of a crashed train. If this wagon was on a crossing,
 * or inside a tunnel/bridge, recalculate the signals as they might need updating
 * @param v the Vehicle of which last wagon is to be removed
 */
static void DeleteLastWagon(Train *v)
{
	Train *first = v->First();

	/* Go to the last wagon and delete the link pointing there
	 * *u is then the one-before-last wagon, and *v the last
	 * one which will physically be removed */
	Train *u = v;
	for (; v->Next() != NULL; v = v->Next()) u = v;
	u->SetNext(NULL);

	if (first != v) {
		/* Recalculate cached train properties */
		first->ConsistChanged(false);
		/* Update the depot window if the first vehicle is in depot -
		 * if v == first, then it is updated in PreDestructor() */
		if (first->track == TRACK_BIT_DEPOT) {
			SetWindowDirty(WC_VEHICLE_DEPOT, first->tile);
		}
	}

	/* 'v' shouldn't be accessed after it has been deleted */
	TrackBits trackbits = v->track;
	TileIndex tile = v->tile;
	Owner owner = v->owner;

	delete v;
	v = NULL; // make sure nobody will try to read 'v' anymore

	if (trackbits == TRACK_BIT_WORMHOLE) {
		/* Vehicle is inside a wormhole, v->track contains no useful value then. */
		trackbits = DiagDirToDiagTrackBits(GetTunnelBridgeDirection(tile));
	}

	Track track = TrackBitsToTrack(trackbits);
	if (HasReservedTracks(tile, trackbits)) {
		UnreserveRailTrack(tile, track);

		/* If there are still crashed vehicles on the tile, give the track reservation to them */
		TrackBits remaining_trackbits = TRACK_BIT_NONE;
		FindVehicleOnPos(tile, &remaining_trackbits, CollectTrackbitsFromCrashedVehiclesEnum);

		/* It is important that these two are the first in the loop, as reservation cannot deal with every trackbit combination */
		assert(TRACK_BEGIN == TRACK_X && TRACK_Y == TRACK_BEGIN + 1);
		Track t;
		FOR_EACH_SET_TRACK(t, remaining_trackbits) TryReserveRailTrack(tile, t);
	}

	/* check if the wagon was on a road/rail-crossing */
	if (IsLevelCrossingTile(tile)) UpdateLevelCrossing(tile);

	/* Update signals */
	if (IsTileType(tile, MP_TUNNELBRIDGE) || IsRailDepotTile(tile)) {
		UpdateSignalsOnSegment(tile, INVALID_DIAGDIR, owner);
	} else {
		SetSignalsOnBothDir(tile, track, owner);
	}
}

static void ChangeTrainDirRandomly(Train *v)
{
	static const DirDiff delta[] = {
		DIRDIFF_45LEFT, DIRDIFF_SAME, DIRDIFF_SAME, DIRDIFF_45RIGHT
	};

	do {
		/* We don't need to twist around vehicles if they're not visible */
		if (!(v->vehstatus & VS_HIDDEN)) {
			v->direction = ChangeDir(v->direction, delta[GB(Random(), 0, 2)]);
			v->UpdateDeltaXY(v->direction);
			v->cur_image = v->GetImage(v->direction);
			/* Refrain from updating the z position of the vehicle when on
			 * a bridge, because UpdateInclination() will put the vehicle under
			 * the bridge in that case */
			if (v->track != TRACK_BIT_WORMHOLE) v->UpdateInclination(false, false);
		}
	} while ((v = v->Next()) != NULL);
}

static bool HandleCrashedTrain(Train *v)
{
	int state = ++v->crash_anim_pos;

	if (state == 4 && !(v->vehstatus & VS_HIDDEN)) {
		CreateEffectVehicleRel(v, 4, 4, 8, EV_EXPLOSION_LARGE);
	}

	uint32 r;
	if (state <= 200 && Chance16R(1, 7, r)) {
		int index = (r * 10 >> 16);

		Vehicle *u = v;
		do {
			if (--index < 0) {
				r = Random();

				CreateEffectVehicleRel(u,
					GB(r,  8, 3) + 2,
					GB(r, 16, 3) + 2,
					GB(r,  0, 3) + 5,
					EV_EXPLOSION_SMALL);
				break;
			}
		} while ((u = u->Next()) != NULL);
	}

	if (state <= 240 && !(v->tick_counter & 3)) ChangeTrainDirRandomly(v);

	if (state >= 4440 && !(v->tick_counter & 0x1F)) {
		bool ret = v->Next() != NULL;
		DeleteLastWagon(v);
		return ret;
	}

	return true;
}

/** Maximum speeds for train that is broken down or approaching line end */
static const uint16 _breakdown_speeds[16] = {
	225, 210, 195, 180, 165, 150, 135, 120, 105, 90, 75, 60, 45, 30, 15, 15
};


/**
 * Train is approaching line end, slow down and possibly reverse
 *
 * @param v front train engine
 * @param signal not line end, just a red signal
 * @return true iff we did NOT have to reverse
 */
static bool TrainApproachingLineEnd(Train *v, bool signal)
{
	/* Calc position within the current tile */
	uint x = v->x_pos & 0xF;
	uint y = v->y_pos & 0xF;

	/* for diagonal directions, 'x' will be 0..15 -
	 * for other directions, it will be 1, 3, 5, ..., 15 */
	switch (v->direction) {
		case DIR_N : x = ~x + ~y + 25; break;
		case DIR_NW: x = y;            // FALL THROUGH
		case DIR_NE: x = ~x + 16;      break;
		case DIR_E : x = ~x + y + 9;   break;
		case DIR_SE: x = y;            break;
		case DIR_S : x = x + y - 7;    break;
		case DIR_W : x = ~y + x + 9;   break;
		default: break;
	}

	/* do not reverse when approaching red signal */
	if (!signal && x + (v->gcache.cached_veh_length + 1) / 2 >= TILE_SIZE) {
		/* we are too near the tile end, reverse now */
		v->cur_speed = 0;
		ReverseTrainDirection(v);
		return false;
	}

	/* slow down */
	v->vehstatus |= VS_TRAIN_SLOWING;
	uint16 break_speed = _breakdown_speeds[x & 0xF];
	if (break_speed < v->cur_speed) v->cur_speed = break_speed;

	return true;
}


/**
 * Determines whether train would like to leave the tile
 * @param v train to test
 * @return true iff vehicle is NOT entering or inside a depot or tunnel/bridge
 */
static bool TrainCanLeaveTile(const Train *v)
{
	/* Exit if inside a tunnel/bridge or a depot */
	if (v->track == TRACK_BIT_WORMHOLE || v->track == TRACK_BIT_DEPOT) return false;

	TileIndex tile = v->tile;

	/* entering a tunnel/bridge? */
	if (IsTileType(tile, MP_TUNNELBRIDGE)) {
		DiagDirection dir = GetTunnelBridgeDirection(tile);
		if (DiagDirToDir(dir) == v->direction) return false;
	}

	/* entering a depot? */
	if (IsRailDepotTile(tile)) {
		DiagDirection dir = ReverseDiagDir(GetRailDepotDirection(tile));
		if (DiagDirToDir(dir) == v->direction) return false;
	}

	return true;
}


/**
 * Determines whether train is approaching a rail-road crossing
 *   (thus making it barred)
 * @param v front engine of train
 * @return TileIndex of crossing the train is approaching, else INVALID_TILE
 * @pre v in non-crashed front engine
 */
static TileIndex TrainApproachingCrossingTile(const Train *v)
{
	assert(v->IsFrontEngine());
	assert(!(v->vehstatus & VS_CRASHED));

	if (!TrainCanLeaveTile(v)) return INVALID_TILE;

	DiagDirection dir = TrainExitDir(v->direction, v->track);
	TileIndex tile = v->tile + TileOffsByDiagDir(dir);

	/* not a crossing || wrong axis || unusable rail (wrong type or owner) */
	if (!IsLevelCrossingTile(tile) || DiagDirToAxis(dir) == GetCrossingRoadAxis(tile) ||
			!CheckCompatibleRail(v, tile)) {
		return INVALID_TILE;
	}

	return tile;
}


/**
 * Checks for line end. Also, bars crossing at next tile if needed
 *
 * @param v vehicle we are checking
 * @return true iff we did NOT have to reverse
 */
static bool TrainCheckIfLineEnds(Train *v)
{
	/* First, handle broken down train */

	int t = v->breakdown_ctr;
	if (t > 1) {
		v->vehstatus |= VS_TRAIN_SLOWING;

		uint16 break_speed = _breakdown_speeds[GB(~t, 4, 4)];
		if (break_speed < v->cur_speed) v->cur_speed = break_speed;
	} else {
		v->vehstatus &= ~VS_TRAIN_SLOWING;
	}

	if (!TrainCanLeaveTile(v)) return true;

	/* Determine the non-diagonal direction in which we will exit this tile */
	DiagDirection dir = TrainExitDir(v->direction, v->track);
	/* Calculate next tile */
	TileIndex tile = v->tile + TileOffsByDiagDir(dir);

	/* Determine the track status on the next tile */
	TrackStatus ts = GetTileTrackStatus(tile, TRANSPORT_RAIL, 0, ReverseDiagDir(dir));
	TrackdirBits reachable_trackdirs = DiagdirReachesTrackdirs(dir);

	TrackdirBits trackdirbits = TrackStatusToTrackdirBits(ts) & reachable_trackdirs;
	TrackdirBits red_signals = TrackStatusToRedSignals(ts) & reachable_trackdirs;

	/* We are sure the train is not entering a depot, it is detected above */

	/* mask unreachable track bits if we are forbidden to do 90deg turns */
	TrackBits bits = TrackdirBitsToTrackBits(trackdirbits);
	if (_settings_game.pf.forbid_90_deg) {
		bits &= ~TrackCrossesTracks(FindFirstTrack(v->track));
	}

	/* no suitable trackbits at all || unusable rail (wrong type or owner) */
	if (bits == TRACK_BIT_NONE || !CheckCompatibleRail(v, tile)) {
		return TrainApproachingLineEnd(v, false);
	}

	/* approaching red signal */
	if ((trackdirbits & red_signals) != 0) return TrainApproachingLineEnd(v, true);

	/* approaching a rail/road crossing? then make it red */
	if (IsLevelCrossingTile(tile)) MaybeBarCrossingWithSound(tile);

	return true;
}


static bool TrainLocoHandler(Train *v, bool mode)
{
	/* train has crashed? */
	if (v->vehstatus & VS_CRASHED) {
		return mode ? true : HandleCrashedTrain(v); // 'this' can be deleted here
	}

	if (v->force_proceed != TFP_NONE) {
		ClrBit(v->flags, VRF_TRAIN_STUCK);
		SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
	}

	/* train is broken down? */
	if (v->HandleBreakdown()) return true;

	if (HasBit(v->flags, VRF_REVERSING) && v->cur_speed == 0) {
		ReverseTrainDirection(v);
	}

	/* exit if train is stopped */
	if ((v->vehstatus & VS_STOPPED) && v->cur_speed == 0) return true;

	bool valid_order = !v->current_order.IsType(OT_NOTHING) && v->current_order.GetType() != OT_CONDITIONAL;
	if (ProcessOrders(v) && CheckReverseTrain(v)) {
		v->wait_counter = 0;
		v->cur_speed = 0;
		v->subspeed = 0;
		ClrBit(v->flags, VRF_LEAVING_STATION);
		ReverseTrainDirection(v);
		return true;
	} else if (HasBit(v->flags, VRF_LEAVING_STATION)) {
		/* Try to reserve a path when leaving the station as we
		 * might not be marked as wanting a reservation, e.g.
		 * when an overlength train gets turned around in a station. */
		DiagDirection dir = TrainExitDir(v->direction, v->track);
		if (IsRailDepotTile(v->tile) || IsTileType(v->tile, MP_TUNNELBRIDGE)) dir = INVALID_DIAGDIR;

		if (UpdateSignalsOnSegment(v->tile, dir, v->owner) == SIGSEG_PBS || _settings_game.pf.reserve_paths) {
			TryPathReserve(v, true, true);
		}
		ClrBit(v->flags, VRF_LEAVING_STATION);
	}

	v->HandleLoading(mode);

	if (v->current_order.IsType(OT_LOADING)) return true;

	if (CheckTrainStayInDepot(v)) return true;

	if (!mode) v->ShowVisualEffect();

	/* We had no order but have an order now, do look ahead. */
	if (!valid_order && !v->current_order.IsType(OT_NOTHING)) {
		CheckNextTrainTile(v);
	}

	/* Handle stuck trains. */
	if (!mode && HasBit(v->flags, VRF_TRAIN_STUCK)) {
		++v->wait_counter;

		/* Should we try reversing this tick if still stuck? */
		bool turn_around = v->wait_counter % (_settings_game.pf.wait_for_pbs_path * DAY_TICKS) == 0 && _settings_game.pf.wait_for_pbs_path < 255;

		if (!turn_around && v->wait_counter % _settings_game.pf.path_backoff_interval != 0 && v->force_proceed == TFP_NONE) return true;
		if (!TryPathReserve(v)) {
			/* Still stuck. */
			if (turn_around) ReverseTrainDirection(v);

			if (HasBit(v->flags, VRF_TRAIN_STUCK) && v->wait_counter > 2 * _settings_game.pf.wait_for_pbs_path * DAY_TICKS) {
				/* Show message to player. */
				if (_settings_client.gui.lost_vehicle_warn && v->owner == _local_company) {
					SetDParam(0, v->index);
					AddVehicleNewsItem(
						STR_NEWS_TRAIN_IS_STUCK,
						NS_ADVICE,
						v->index
					);
				}
				v->wait_counter = 0;
			}
			/* Exit if force proceed not pressed, else reset stuck flag anyway. */
			if (v->force_proceed == TFP_NONE) return true;
			ClrBit(v->flags, VRF_TRAIN_STUCK);
			v->wait_counter = 0;
			SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
		}
	}

	if (v->current_order.IsType(OT_LEAVESTATION)) {
		v->current_order.Free();
		SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
		return true;
	}

	int j = v->UpdateSpeed();

	/* we need to invalidate the widget if we are stopping from 'Stopping 0 km/h' to 'Stopped' */
	if (v->cur_speed == 0 && (v->vehstatus & VS_STOPPED)) {
		/* If we manually stopped, we're not force-proceeding anymore. */
		v->force_proceed = TFP_NONE;
		SetWindowDirty(WC_VEHICLE_VIEW, v->index);
	}

	int adv_spd = v->GetAdvanceDistance();
	if (j < adv_spd) {
		/* if the vehicle has speed 0, update the last_speed field. */
		if (v->cur_speed == 0) SetLastSpeed(v, v->cur_speed);
	} else {
		TrainCheckIfLineEnds(v);
		/* Loop until the train has finished moving. */
		for (;;) {
			j -= adv_spd;
			TrainController(v, NULL);
			/* Don't continue to move if the train crashed. */
			if (CheckTrainCollision(v)) break;
			/* Determine distance to next map position */
			adv_spd = v->GetAdvanceDistance();

			/* No more moving this tick */
			if (j < adv_spd || v->cur_speed == 0) break;

			OrderType order_type = v->current_order.GetType();
			/* Do not skip waypoints (incl. 'via' stations) when passing through at full speed. */
			if ((order_type == OT_GOTO_WAYPOINT || order_type == OT_GOTO_STATION) &&
						(v->current_order.GetNonStopType() & ONSF_NO_STOP_AT_DESTINATION_STATION) &&
						IsTileType(v->tile, MP_STATION) &&
						v->current_order.GetDestination() == GetStationIndex(v->tile)) {
				ProcessOrders(v);
			}
		}
		SetLastSpeed(v, v->cur_speed);
	}

	for (Train *u = v; u != NULL; u = u->Next()) {
		if ((u->vehstatus & VS_HIDDEN) != 0) continue;

		u->UpdateViewport(false, false);
	}

	if (v->progress == 0) v->progress = j; // Save unused spd for next time, if TrainController didn't set progress

	return true;
}



Money Train::GetRunningCost() const
{
	Money cost = 0;
	const Train *v = this;

	do {
		const Engine *e = Engine::Get(v->engine_type);
		if (e->u.rail.running_cost_class == INVALID_PRICE) continue;

		uint cost_factor = GetVehicleProperty(v, PROP_TRAIN_RUNNING_COST_FACTOR, e->u.rail.running_cost);
		if (cost_factor == 0) continue;

		/* Halve running cost for multiheaded parts */
		if (v->IsMultiheaded()) cost_factor /= 2;

		cost += GetPrice(e->u.rail.running_cost_class, cost_factor, e->grf_prop.grffile);
	} while ((v = v->GetNextVehicle()) != NULL);

	return cost;
}


bool Train::Tick()
{
	this->tick_counter++;

	if (this->IsFrontEngine()) {
		if (!(this->vehstatus & VS_STOPPED) || this->cur_speed > 0) this->running_ticks++;

		this->current_order_time++;

		if (!TrainLocoHandler(this, false)) return false;

		return TrainLocoHandler(this, true);
	} else if (this->IsFreeWagon() && (this->vehstatus & VS_CRASHED)) {
		/* Delete flooded standalone wagon chain */
		if (++this->crash_anim_pos >= 4400) {
			delete this;
			return false;
		}
	}

	return true;
}

static void CheckIfTrainNeedsService(Train *v)
{
	if (Company::Get(v->owner)->settings.vehicle.servint_trains == 0 || !v->NeedsAutomaticServicing()) return;
	if (v->IsInDepot()) {
		VehicleServiceInDepot(v);
		return;
	}

	uint max_penalty;
	switch (_settings_game.pf.pathfinder_for_trains) {
		case VPF_NPF:  max_penalty = _settings_game.pf.npf.maximum_go_to_depot_penalty;  break;
		case VPF_YAPF: max_penalty = _settings_game.pf.yapf.maximum_go_to_depot_penalty; break;
		default: NOT_REACHED();
	}

	FindDepotData tfdd = FindClosestTrainDepot(v, max_penalty);
	/* Only go to the depot if it is not too far out of our way. */
	if (tfdd.best_length == UINT_MAX || tfdd.best_length > max_penalty) {
		if (v->current_order.IsType(OT_GOTO_DEPOT)) {
			/* If we were already heading for a depot but it has
			 * suddenly moved farther away, we continue our normal
			 * schedule? */
			v->current_order.MakeDummy();
			SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
		}
		return;
	}

	DepotID depot = GetDepotIndex(tfdd.tile);

	if (v->current_order.IsType(OT_GOTO_DEPOT) &&
			v->current_order.GetDestination() != depot &&
			!Chance16(3, 16)) {
		return;
	}

	v->current_order.MakeGoToDepot(depot, ODTFB_SERVICE);
	v->dest_tile = tfdd.tile;
	SetWindowWidgetDirty(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
}

void Train::OnNewDay()
{
	if ((++this->day_counter & 7) == 0) DecreaseVehicleValue(this);

	if (this->IsFrontEngine()) {
		CheckVehicleBreakdown(this);
		AgeVehicle(this);

		CheckIfTrainNeedsService(this);

		CheckOrders(this);

		/* update destination */
		if (this->current_order.IsType(OT_GOTO_STATION)) {
			TileIndex tile = Station::Get(this->current_order.GetDestination())->train_station.tile;
			if (tile != INVALID_TILE) this->dest_tile = tile;
		}

		if (this->running_ticks != 0) {
			/* running costs */
			CommandCost cost(EXPENSES_TRAIN_RUN, this->GetRunningCost() * this->running_ticks / (DAYS_IN_YEAR  * DAY_TICKS));

			this->profit_this_year -= cost.GetCost();
			this->running_ticks = 0;

			SubtractMoneyFromCompanyFract(this->owner, cost);

			SetWindowDirty(WC_VEHICLE_DETAILS, this->index);
			SetWindowClassesDirty(WC_TRAINS_LIST);
		}
	} else if (this->IsEngine()) {
		/* Also age engines that aren't front engines */
		AgeVehicle(this);
	}
}

Trackdir Train::GetVehicleTrackdir() const
{
	if (this->vehstatus & VS_CRASHED) return INVALID_TRACKDIR;

	if (this->track == TRACK_BIT_DEPOT) {
		/* We'll assume the train is facing outwards */
		return DiagDirToDiagTrackdir(GetRailDepotDirection(this->tile)); // Train in depot
	}

	if (this->track == TRACK_BIT_WORMHOLE) {
		/* train in tunnel or on bridge, so just use his direction and assume a diagonal track */
		return DiagDirToDiagTrackdir(DirToDiagDir(this->direction));
	}

	return TrackDirectionToTrackdir(FindFirstTrack(this->track), this->direction);
}
