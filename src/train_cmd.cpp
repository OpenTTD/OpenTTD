/* $Id$ */

/** @file train_cmd.cpp Handling of trains. */

#include "stdafx.h"
#include "gui.h"
#include "articulated_vehicles.h"
#include "command_func.h"
#include "npf.h"
#include "news_func.h"
#include "engine_func.h"
#include "engine_base.h"
#include "company_func.h"
#include "depot_base.h"
#include "vehicle_gui.h"
#include "train.h"
#include "newgrf_engine.h"
#include "newgrf_sound.h"
#include "newgrf_text.h"
#include "yapf/follow_track.hpp"
#include "group.h"
#include "table/sprites.h"
#include "strings_func.h"
#include "functions.h"
#include "window_func.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "variables.h"
#include "autoreplace_gui.h"
#include "gfx_func.h"
#include "ai/ai.hpp"
#include "newgrf_station.h"
#include "effectvehicle_func.h"
#include "gamelog.h"
#include "network/network.h"

#include "table/strings.h"
#include "table/train_cmd.h"

static Track ChooseTrainTrack(Vehicle *v, TileIndex tile, DiagDirection enterdir, TrackBits tracks, bool force_res, bool *got_reservation, bool mark_stuck);
static bool TrainCheckIfLineEnds(Vehicle *v);
static void TrainController(Vehicle *v, Vehicle *nomove);
static TileIndex TrainApproachingCrossingTile(const Vehicle *v);
static void CheckIfTrainNeedsService(Vehicle *v);
static void CheckNextTrainTile(Vehicle *v);

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


/** Return the cargo weight multiplier to use for a rail vehicle
 * @param cargo Cargo type to get multiplier for
 * @return Cargo weight multiplier
 */
byte FreightWagonMult(CargoID cargo)
{
	if (!GetCargo(cargo)->is_freight) return 1;
	return _settings_game.vehicle.freight_trains;
}


/**
 * Recalculates the cached total power of a train. Should be called when the consist is changed
 * @param v First vehicle of the consist.
 */
void TrainPowerChanged(Vehicle *v)
{
	uint32 total_power = 0;
	uint32 max_te = 0;

	for (const Vehicle *u = v; u != NULL; u = u->Next()) {
		RailType railtype = GetRailType(u->tile);

		/* Power is not added for articulated parts */
		if (!IsArticulatedPart(u)) {
			bool engine_has_power = HasPowerOnRail(u->u.rail.railtype, railtype);

			const RailVehicleInfo *rvi_u = RailVehInfo(u->engine_type);

			if (engine_has_power) {
				uint16 power = GetVehicleProperty(u, 0x0B, rvi_u->power);
				if (power != 0) {
					/* Halve power for multiheaded parts */
					if (IsMultiheaded(u)) power /= 2;

					total_power += power;
					/* Tractive effort in (tonnes * 1000 * 10 =) N */
					max_te += (u->u.rail.cached_veh_weight * 10000 * GetVehicleProperty(u, 0x1F, rvi_u->tractive_effort)) / 256;
				}
			}
		}

		if (HasBit(u->u.rail.flags, VRF_POWEREDWAGON) && HasPowerOnRail(v->u.rail.railtype, railtype)) {
			total_power += RailVehInfo(u->u.rail.first_engine)->pow_wag_power;
		}
	}

	if (v->u.rail.cached_power != total_power || v->u.rail.cached_max_te != max_te) {
		/* If it has no power (no catenary), stop the train */
		if (total_power == 0) v->vehstatus |= VS_STOPPED;

		v->u.rail.cached_power = total_power;
		v->u.rail.cached_max_te = max_te;
		InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
	}
}


/**
 * Recalculates the cached weight of a train and its vehicles. Should be called each time the cargo on
 * the consist changes.
 * @param v First vehicle of the consist.
 */
static void TrainCargoChanged(Vehicle *v)
{
	uint32 weight = 0;

	for (Vehicle *u = v; u != NULL; u = u->Next()) {
		uint32 vweight = GetCargo(u->cargo_type)->weight * u->cargo.Count() * FreightWagonMult(u->cargo_type) / 16;

		/* Vehicle weight is not added for articulated parts. */
		if (!IsArticulatedPart(u)) {
			/* vehicle weight is the sum of the weight of the vehicle and the weight of its cargo */
			vweight += GetVehicleProperty(u, 0x16, RailVehInfo(u->engine_type)->weight);
		}

		/* powered wagons have extra weight added */
		if (HasBit(u->u.rail.flags, VRF_POWEREDWAGON)) {
			vweight += RailVehInfo(u->u.rail.first_engine)->pow_wag_weight;
		}

		/* consist weight is the sum of the weight of all vehicles in the consist */
		weight += vweight;

		/* store vehicle weight in cache */
		u->u.rail.cached_veh_weight = vweight;
	}

	/* store consist weight in cache */
	v->u.rail.cached_weight = weight;

	/* Now update train power (tractive effort is dependent on weight) */
	TrainPowerChanged(v);
}


/** Logs a bug in GRF and shows a warning message if this
 * is for the first time this happened.
 * @param u first vehicle of chain
 */
static void RailVehicleLengthChanged(const Vehicle *u)
{
	/* show a warning once for each engine in whole game and once for each GRF after each game load */
	const Engine *engine = GetEngine(u->engine_type);
	uint32 grfid = engine->grffile->grfid;
	GRFConfig *grfconfig = GetGRFConfig(grfid);
	if (GamelogGRFBugReverse(grfid, engine->internal_id) || !HasBit(grfconfig->grf_bugs, GBUG_VEH_LENGTH)) {
		ShowNewGrfVehicleError(u->engine_type, STR_NEWGRF_BROKEN, STR_NEWGRF_BROKEN_VEHICLE_LENGTH, GBUG_VEH_LENGTH, true);
	}
}

/** Checks if lengths of all rail vehicles are valid. If not, shows an error message. */
void CheckTrainsLengths()
{
	const Vehicle *v;

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_TRAIN && v->First() == v && !(v->vehstatus & VS_CRASHED)) {
			for (const Vehicle *u = v, *w = v->Next(); w != NULL; u = w, w = w->Next()) {
				if (u->u.rail.track != TRACK_BIT_DEPOT) {
					if ((w->u.rail.track != TRACK_BIT_DEPOT &&
							max(abs(u->x_pos - w->x_pos), abs(u->y_pos - w->y_pos)) != u->u.rail.cached_veh_length) ||
							(w->u.rail.track == TRACK_BIT_DEPOT && TicksToLeaveDepot(u) <= 0)) {
						SetDParam(0, v->index);
						SetDParam(1, v->owner);
						ShowErrorMessage(INVALID_STRING_ID, STR_BROKEN_VEHICLE_LENGTH, 0, 0);

						if (!_networking) _pause_game = -1;
					}
				}
			}
		}
	}
}

/**
 * Recalculates the cached stuff of a train. Should be called each time a vehicle is added
 * to/removed from the chain, and when the game is loaded.
 * Note: this needs to be called too for 'wagon chains' (in the depot, without an engine)
 * @param v First vehicle of the chain.
 * @param same_length should length of vehicles stay the same?
 */
void TrainConsistChanged(Vehicle *v, bool same_length)
{
	uint16 max_speed = UINT16_MAX;

	assert(v->type == VEH_TRAIN);
	assert(IsFrontEngine(v) || IsFreeWagon(v));

	const RailVehicleInfo *rvi_v = RailVehInfo(v->engine_type);
	EngineID first_engine = IsFrontEngine(v) ? v->engine_type : INVALID_ENGINE;
	v->u.rail.cached_total_length = 0;
	v->u.rail.compatible_railtypes = RAILTYPES_NONE;

	bool train_can_tilt = true;

	for (Vehicle *u = v; u != NULL; u = u->Next()) {
		const RailVehicleInfo *rvi_u = RailVehInfo(u->engine_type);

		/* Check the v->first cache. */
		assert(u->First() == v);

		/* update the 'first engine' */
		u->u.rail.first_engine = v == u ? INVALID_ENGINE : first_engine;
		u->u.rail.railtype = rvi_u->railtype;

		if (IsTrainEngine(u)) first_engine = u->engine_type;

		/* Set user defined data to its default value */
		u->u.rail.user_def_data = rvi_u->user_def_data;
		u->cache_valid = 0;
	}

	for (Vehicle *u = v; u != NULL; u = u->Next()) {
		/* Update user defined data (must be done before other properties) */
		u->u.rail.user_def_data = GetVehicleProperty(u, 0x25, u->u.rail.user_def_data);
		u->cache_valid = 0;
	}

	for (Vehicle *u = v; u != NULL; u = u->Next()) {
		const Engine *e_u = GetEngine(u->engine_type);
		const RailVehicleInfo *rvi_u = &e_u->u.rail;

		if (!HasBit(EngInfo(u->engine_type)->misc_flags, EF_RAIL_TILTS)) train_can_tilt = false;

		/* Cache wagon override sprite group. NULL is returned if there is none */
		u->u.rail.cached_override = GetWagonOverrideSpriteSet(u->engine_type, u->cargo_type, u->u.rail.first_engine);

		/* Reset colour map */
		u->colourmap = PAL_NONE;

		if (rvi_u->visual_effect != 0) {
			u->u.rail.cached_vis_effect = rvi_u->visual_effect;
		} else {
			if (IsTrainWagon(u) || IsArticulatedPart(u)) {
				/* Wagons and articulated parts have no effect by default */
				u->u.rail.cached_vis_effect = 0x40;
			} else if (rvi_u->engclass == 0) {
				/* Steam is offset by -4 units */
				u->u.rail.cached_vis_effect = 4;
			} else {
				/* Diesel fumes and sparks come from the centre */
				u->u.rail.cached_vis_effect = 8;
			}
		}

		/* Check powered wagon / visual effect callback */
		if (HasBit(EngInfo(u->engine_type)->callbackmask, CBM_TRAIN_WAGON_POWER)) {
			uint16 callback = GetVehicleCallback(CBID_TRAIN_WAGON_POWER, 0, 0, u->engine_type, u);

			if (callback != CALLBACK_FAILED) u->u.rail.cached_vis_effect = GB(callback, 0, 8);
		}

		if (rvi_v->pow_wag_power != 0 && rvi_u->railveh_type == RAILVEH_WAGON &&
			UsesWagonOverride(u) && !HasBit(u->u.rail.cached_vis_effect, 7)) {
			/* wagon is powered */
			SetBit(u->u.rail.flags, VRF_POWEREDWAGON); // cache 'powered' status
		} else {
			ClrBit(u->u.rail.flags, VRF_POWEREDWAGON);
		}

		if (!IsArticulatedPart(u)) {
			/* Do not count powered wagons for the compatible railtypes, as wagons always
			   have railtype normal */
			if (rvi_u->power > 0) {
				v->u.rail.compatible_railtypes |= GetRailTypeInfo(u->u.rail.railtype)->powered_railtypes;
			}

			/* Some electric engines can be allowed to run on normal rail. It happens to all
			 * existing electric engines when elrails are disabled and then re-enabled */
			if (HasBit(u->u.rail.flags, VRF_EL_ENGINE_ALLOWED_NORMAL_RAIL)) {
				u->u.rail.railtype = RAILTYPE_RAIL;
				u->u.rail.compatible_railtypes |= RAILTYPES_RAIL;
			}

			/* max speed is the minimum of the speed limits of all vehicles in the consist */
			if ((rvi_u->railveh_type != RAILVEH_WAGON || _settings_game.vehicle.wagon_speed_limits) && !UsesWagonOverride(u)) {
				uint16 speed = GetVehicleProperty(u, 0x09, rvi_u->max_speed);
				if (speed != 0) max_speed = min(speed, max_speed);
			}
		}

		if (e_u->CanCarryCargo() && u->cargo_type == e_u->GetDefaultCargoType() && u->cargo_subtype == 0) {
			/* Set cargo capacity if we've not been refitted */
			u->cargo_cap = GetVehicleProperty(u, 0x14, rvi_u->capacity);
		}

		/* check the vehicle length (callback) */
		uint16 veh_len = CALLBACK_FAILED;
		if (HasBit(EngInfo(u->engine_type)->callbackmask, CBM_VEHICLE_LENGTH)) {
			veh_len = GetVehicleCallback(CBID_VEHICLE_LENGTH, 0, 0, u->engine_type, u);
		}
		if (veh_len == CALLBACK_FAILED) veh_len = rvi_u->shorten_factor;
		veh_len = 8 - Clamp(veh_len, 0, u->Next() == NULL ? 7 : 5); // the clamp on vehicles not the last in chain is stricter, as too short wagons can break the 'follow next vehicle' code

		/* verify length hasn't changed */
		if (same_length && veh_len != u->u.rail.cached_veh_length) RailVehicleLengthChanged(u);

		/* update vehicle length? */
		if (!same_length) u->u.rail.cached_veh_length = veh_len;

		v->u.rail.cached_total_length += u->u.rail.cached_veh_length;
		u->cache_valid = 0;
	}

	/* store consist weight/max speed in cache */
	v->u.rail.cached_max_speed = max_speed;
	v->u.rail.cached_tilt = train_can_tilt;

	/* recalculate cached weights and power too (we do this *after* the rest, so it is known which wagons are powered and need extra weight added) */
	TrainCargoChanged(v);

	if (IsFrontEngine(v)) {
		UpdateTrainAcceleration(v);
		InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
	}
}

enum AccelType {
	AM_ACCEL,
	AM_BRAKE
};

/** new acceleration*/
static int GetTrainAcceleration(Vehicle *v, bool mode)
{
	static const int absolute_max_speed = UINT16_MAX;
	int max_speed = absolute_max_speed;
	int speed = v->cur_speed * 10 / 16; // km-ish/h -> mp/h
	int curvecount[2] = {0, 0};

	/* first find the curve speed limit */
	int numcurve = 0;
	int sum = 0;
	int pos = 0;
	int lastpos = -1;
	for (const Vehicle *u = v; u->Next() != NULL; u = u->Next(), pos++) {
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

	if ((curvecount[0] != 0 || curvecount[1] != 0) && max_speed > 88) {
		int total = curvecount[0] + curvecount[1];

		if (curvecount[0] == 1 && curvecount[1] == 1) {
			max_speed = absolute_max_speed;
		} else if (total > 1) {
			if (numcurve > 0) sum /= numcurve;
			max_speed = 232 - (13 - Clamp(sum, 1, 12)) * (13 - Clamp(sum, 1, 12));
		}
	}

	if (max_speed != absolute_max_speed) {
		/* Apply the engine's rail type curve speed advantage, if it slowed by curves */
		const RailtypeInfo *rti = GetRailTypeInfo(v->u.rail.railtype);
		max_speed += (max_speed / 2) * rti->curve_speed;

		if (v->u.rail.cached_tilt) {
			/* Apply max_speed bonus of 20% for a tilting train */
			max_speed += max_speed / 5;
		}
	}

	if (IsTileType(v->tile, MP_STATION) && IsFrontEngine(v)) {
		if (v->current_order.ShouldStopAtStation(v, GetStationIndex(v->tile))) {
			int station_length = GetStationByTile(v->tile)->GetPlatformLength(v->tile, DirToDiagDir(v->direction));

			int st_max_speed = 120;

			int delta_v = v->cur_speed / (station_length + 1);
			if (v->max_speed > (v->cur_speed - delta_v)) {
				st_max_speed = v->cur_speed - (delta_v / 10);
			}

			st_max_speed = max(st_max_speed, 25 * station_length);
			max_speed = min(max_speed, st_max_speed);
		}
	}

	int mass = v->u.rail.cached_weight;
	int power = v->u.rail.cached_power * 746;
	max_speed = min(max_speed, v->u.rail.cached_max_speed);

	int num = 0; // number of vehicles, change this into the number of axles later
	int incl = 0;
	int drag_coeff = 20; //[1e-4]
	for (const Vehicle *u = v; u != NULL; u = u->Next()) {
		num++;
		drag_coeff += 3;

		if (u->u.rail.track == TRACK_BIT_DEPOT) max_speed = min(max_speed, 61);

		if (HasBit(u->u.rail.flags, VRF_GOINGUP)) {
			incl += u->u.rail.cached_veh_weight * 60; // 3% slope, quite a bit actually
		} else if (HasBit(u->u.rail.flags, VRF_GOINGDOWN)) {
			incl -= u->u.rail.cached_veh_weight * 60;
		}
	}

	v->max_speed = max_speed;

	const int area = 120;
	const int friction = 35; //[1e-3]
	int resistance;
	if (v->u.rail.railtype != RAILTYPE_MAGLEV) {
		resistance = 13 * mass / 10;
		resistance += 60 * num;
		resistance += friction * mass * speed / 1000;
		resistance += (area * drag_coeff * speed * speed) / 10000;
	} else {
		resistance = (area * (drag_coeff / 2) * speed * speed) / 10000;
	}
	resistance += incl;
	resistance *= 4; //[N]

	const int max_te = v->u.rail.cached_max_te; // [N]
	int force;
	if (speed > 0) {
		switch (v->u.rail.railtype) {
			case RAILTYPE_RAIL:
			case RAILTYPE_ELECTRIC:
			case RAILTYPE_MONO:
				force = power / speed; //[N]
				force *= 22;
				force /= 10;
				if (mode == AM_ACCEL && force > max_te) force = max_te;
				break;

			default: NOT_REACHED();
			case RAILTYPE_MAGLEV:
				force = power / 25;
				break;
		}
	} else {
		/* "kickoff" acceleration */
		force = (mode == AM_ACCEL && v->u.rail.railtype != RAILTYPE_MAGLEV) ? min(max_te, power) : power;
		force = max(force, (mass * 8) + resistance);
	}

	if (mode == AM_ACCEL) {
		return (force - resistance) / (mass * 2);
	} else {
		return min(-force - resistance, -10000) / mass;
	}
}

void UpdateTrainAcceleration(Vehicle *v)
{
	assert(IsFrontEngine(v));

	v->max_speed = v->u.rail.cached_max_speed;

	uint power = v->u.rail.cached_power;
	uint weight = v->u.rail.cached_weight;
	assert(weight != 0);
	v->acceleration = Clamp(power / weight * 4, 1, 255);
}

SpriteID Train::GetImage(Direction direction) const
{
	uint8 spritenum = this->spritenum;
	SpriteID sprite;

	if (HasBit(this->u.rail.flags, VRF_REVERSE_DIRECTION)) direction = ReverseDir(direction);

	if (is_custom_sprite(spritenum)) {
		sprite = GetCustomVehicleSprite(this, (Direction)(direction + 4 * IS_CUSTOM_SECONDHEAD_SPRITE(spritenum)));
		if (sprite != 0) return sprite;

		spritenum = GetEngine(this->engine_type)->image_index;
	}

	sprite = _engine_sprite_base[spritenum] + ((direction + _engine_sprite_add[spritenum]) & _engine_sprite_and[spritenum]);

	if (this->cargo.Count() >= this->cargo_cap / 2U) sprite += _wagon_full_adder[spritenum];

	return sprite;
}

static SpriteID GetRailIcon(EngineID engine, bool rear_head, int &y)
{
	Direction dir = rear_head ? DIR_E : DIR_W;
	uint8 spritenum = RailVehInfo(engine)->image_index;

	if (is_custom_sprite(spritenum)) {
		SpriteID sprite = GetCustomVehicleIcon(engine, dir);
		if (sprite != 0) {
			y += _traininfo_vehicle_pitch; // TODO Make this per-GRF
			return sprite;
		}

		spritenum = GetEngine(engine)->image_index;
	}

	if (rear_head) spritenum++;

	return ((6 + _engine_sprite_add[spritenum]) & _engine_sprite_and[spritenum]) + _engine_sprite_base[spritenum];
}

void DrawTrainEngine(int x, int y, EngineID engine, SpriteID pal)
{
	if (RailVehInfo(engine)->railveh_type == RAILVEH_MULTIHEAD) {
		int yf = y;
		int yr = y;

		SpriteID spritef = GetRailIcon(engine, false, yf);
		SpriteID spriter = GetRailIcon(engine, true, yr);
		DrawSprite(spritef, pal, x - 14, yf);
		DrawSprite(spriter, pal, x + 15, yr);
	} else {
		SpriteID sprite = GetRailIcon(engine, false, y);
		DrawSprite(sprite, pal, x, y);
	}
}

static CommandCost CmdBuildRailWagon(EngineID engine, TileIndex tile, DoCommandFlag flags)
{
	const Engine *e = GetEngine(engine);
	const RailVehicleInfo *rvi = &e->u.rail;
	CommandCost value(EXPENSES_NEW_VEHICLES, e->GetCost());

	/* Engines without valid cargo should not be available */
	if (e->GetDefaultCargoType() == CT_INVALID) return CMD_ERROR;

	if (flags & DC_QUERY_COST) return value;

	/* Check that the wagon can drive on the track in question */
	if (!IsCompatibleRail(rvi->railtype, GetRailType(tile))) return CMD_ERROR;

	uint num_vehicles = 1 + CountArticulatedParts(engine, false);

	/* Allow for the wagon and the articulated parts, plus one to "terminate" the list. */
	Vehicle **vl = AllocaM(Vehicle*, num_vehicles + 1);
	memset(vl, 0, sizeof(*vl) * (num_vehicles + 1));

	if (!Vehicle::AllocateList(vl, num_vehicles)) {
		return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);
	}

	if (flags & DC_EXEC) {
		Vehicle *v = vl[0];
		v->spritenum = rvi->image_index;

		Vehicle *u = NULL;

		Vehicle *w;
		FOR_ALL_VEHICLES(w) {
			if (w->type == VEH_TRAIN && w->tile == tile &&
			    IsFreeWagon(w) && w->engine_type == engine &&
			    !HASBITS(w->vehstatus, VS_CRASHED)) {          /// do not connect new wagon with crashed/flooded consists
				u = GetLastVehicleInChain(w);
				break;
			}
		}

		v = new (v) Train();
		v->engine_type = engine;

		DiagDirection dir = GetRailDepotDirection(tile);

		v->direction = DiagDirToDir(dir);
		v->tile = tile;

		int x = TileX(tile) * TILE_SIZE | _vehicle_initial_x_fract[dir];
		int y = TileY(tile) * TILE_SIZE | _vehicle_initial_y_fract[dir];

		v->x_pos = x;
		v->y_pos = y;
		v->z_pos = GetSlopeZ(x, y);
		v->owner = _current_company;
		v->u.rail.track = TRACK_BIT_DEPOT;
		v->vehstatus = VS_HIDDEN | VS_DEFPAL;

//		v->subtype = 0;
		SetTrainWagon(v);

		if (u != NULL) {
			u->SetNext(v);
		} else {
			SetFreeWagon(v);
			InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
		}

		v->cargo_type = e->GetDefaultCargoType();
//		v->cargo_subtype = 0;
		v->cargo_cap = rvi->capacity;
		v->value = value.GetCost();
//		v->day_counter = 0;

		v->u.rail.railtype = rvi->railtype;

		v->build_year = _cur_year;
		v->cur_image = 0xAC2;
		v->random_bits = VehicleRandomBits();

		v->group_id = DEFAULT_GROUP;

		AddArticulatedParts(vl, VEH_TRAIN);

		_new_vehicle_id = v->index;

		VehicleMove(v, false);
		TrainConsistChanged(v->First(), false);
		UpdateTrainGroupID(v->First());

		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
		if (IsLocalCompany()) {
			InvalidateAutoreplaceWindow(v->engine_type, v->group_id); // updates the replace Train window
		}
		GetCompany(_current_company)->num_engines[engine]++;

		CheckConsistencyOfArticulatedVehicle(v);
	}

	return value;
}

/** Move all free vehicles in the depot to the train */
static void NormalizeTrainVehInDepot(const Vehicle *u)
{
	const Vehicle *v;

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_TRAIN && IsFreeWagon(v) &&
				v->tile == u->tile &&
				v->u.rail.track == TRACK_BIT_DEPOT) {
			if (CmdFailed(DoCommand(0, v->index | (u->index << 16), 1, DC_EXEC,
					CMD_MOVE_RAIL_VEHICLE)))
				break;
		}
	}
}

static void AddRearEngineToMultiheadedTrain(Vehicle *v, Vehicle *u, bool building)
{
	u = new (u) Train();
	u->direction = v->direction;
	u->owner = v->owner;
	u->tile = v->tile;
	u->x_pos = v->x_pos;
	u->y_pos = v->y_pos;
	u->z_pos = v->z_pos;
	u->u.rail.track = TRACK_BIT_DEPOT;
	u->vehstatus = v->vehstatus & ~VS_STOPPED;
//	u->subtype = 0;
	SetMultiheaded(u);
	u->spritenum = v->spritenum + 1;
	u->cargo_type = v->cargo_type;
	u->cargo_subtype = v->cargo_subtype;
	u->cargo_cap = v->cargo_cap;
	u->u.rail.railtype = v->u.rail.railtype;
	if (building) v->SetNext(u);
	u->engine_type = v->engine_type;
	u->build_year = v->build_year;
	if (building) v->value >>= 1;
	u->value = v->value;
	u->cur_image = 0xAC2;
	u->random_bits = VehicleRandomBits();
	VehicleMove(u, false);
}

/** Build a railroad vehicle.
 * @param tile tile of the depot where rail-vehicle is built
 * @param flags type of operation
 * @param p1 engine type id
 * @param p2 bit 1 prevents any free cars from being added to the train
 */
CommandCost CmdBuildRailVehicle(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	/* Check if the engine-type is valid (for the company) */
	if (!IsEngineBuildable(p1, VEH_TRAIN, _current_company)) return_cmd_error(STR_RAIL_VEHICLE_NOT_AVAILABLE);

	const Engine *e = GetEngine(p1);
	CommandCost value(EXPENSES_NEW_VEHICLES, e->GetCost());

	/* Engines with CT_INVALID should not be available */
	if (e->GetDefaultCargoType() == CT_INVALID) return CMD_ERROR;

	if (flags & DC_QUERY_COST) return value;

	/* Check if the train is actually being built in a depot belonging
	 * to the company. Doesn't matter if only the cost is queried */
	if (!IsRailDepotTile(tile)) return CMD_ERROR;
	if (!IsTileOwner(tile, _current_company)) return CMD_ERROR;

	const RailVehicleInfo *rvi = RailVehInfo(p1);
	if (rvi->railveh_type == RAILVEH_WAGON) return CmdBuildRailWagon(p1, tile, flags);

	uint num_vehicles =
		(rvi->railveh_type == RAILVEH_MULTIHEAD ? 2 : 1) +
		CountArticulatedParts(p1, false);

	/* Check if depot and new engine uses the same kind of tracks *
	 * We need to see if the engine got power on the tile to avoid eletric engines in non-electric depots */
	if (!HasPowerOnRail(rvi->railtype, GetRailType(tile))) return CMD_ERROR;

	/* Allow for the dual-heads and the articulated parts, plus one to "terminate" the list. */
	Vehicle **vl = AllocaM(Vehicle*, num_vehicles + 1);
	memset(vl, 0, sizeof(*vl) * (num_vehicles + 1));

	if (!Vehicle::AllocateList(vl, num_vehicles)) {
		return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);
	}

	Vehicle *v = vl[0];

	UnitID unit_num = (flags & DC_AUTOREPLACE) ? 0 : GetFreeUnitNumber(VEH_TRAIN);
	if (unit_num > _settings_game.vehicle.max_trains) {
		return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);
	}

	if (flags & DC_EXEC) {
		DiagDirection dir = GetRailDepotDirection(tile);
		int x = TileX(tile) * TILE_SIZE + _vehicle_initial_x_fract[dir];
		int y = TileY(tile) * TILE_SIZE + _vehicle_initial_y_fract[dir];

		v = new (v) Train();
		v->unitnumber = unit_num;
		v->direction = DiagDirToDir(dir);
		v->tile = tile;
		v->owner = _current_company;
		v->x_pos = x;
		v->y_pos = y;
		v->z_pos = GetSlopeZ(x, y);
//		v->running_ticks = 0;
		v->u.rail.track = TRACK_BIT_DEPOT;
		v->vehstatus = VS_HIDDEN | VS_STOPPED | VS_DEFPAL;
		v->spritenum = rvi->image_index;
		v->cargo_type = e->GetDefaultCargoType();
//		v->cargo_subtype = 0;
		v->cargo_cap = rvi->capacity;
		v->max_speed = rvi->max_speed;
		v->value = value.GetCost();
		v->last_station_visited = INVALID_STATION;
//		v->dest_tile = 0;

		v->engine_type = p1;

		v->reliability = e->reliability;
		v->reliability_spd_dec = e->reliability_spd_dec;
		v->max_age = e->lifelength * DAYS_IN_LEAP_YEAR;

		v->name = NULL;
		v->u.rail.railtype = rvi->railtype;
		_new_vehicle_id = v->index;

		v->service_interval = _settings_game.vehicle.servint_trains;
		v->date_of_last_service = _date;
		v->build_year = _cur_year;
		v->cur_image = 0xAC2;
		v->random_bits = VehicleRandomBits();

//		v->vehicle_flags = 0;
		if (e->flags & ENGINE_EXCLUSIVE_PREVIEW) SetBit(v->vehicle_flags, VF_BUILT_AS_PROTOTYPE);

		v->group_id = DEFAULT_GROUP;

//		v->subtype = 0;
		SetFrontEngine(v);
		SetTrainEngine(v);

		VehicleMove(v, false);

		if (rvi->railveh_type == RAILVEH_MULTIHEAD) {
			SetMultiheaded(v);
			AddRearEngineToMultiheadedTrain(vl[0], vl[1], true);
			/* Now we need to link the front and rear engines together
			 * other_multiheaded_part is the pointer that links to the other half of the engine
			 * vl[0] is the front and vl[1] is the rear
			 */
			vl[0]->u.rail.other_multiheaded_part = vl[1];
			vl[1]->u.rail.other_multiheaded_part = vl[0];
		} else {
			AddArticulatedParts(vl, VEH_TRAIN);
		}

		TrainConsistChanged(v, false);
		UpdateTrainGroupID(v);

		if (!HasBit(p2, 1) && !(flags & DC_AUTOREPLACE)) { // check if the cars should be added to the new vehicle
			NormalizeTrainVehInDepot(v);
		}

		InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
		InvalidateWindowClassesData(WC_TRAINS_LIST, 0);
		InvalidateWindow(WC_COMPANY, v->owner);
		if (IsLocalCompany()) {
			InvalidateAutoreplaceWindow(v->engine_type, v->group_id); // updates the replace Train window
		}

		GetCompany(_current_company)->num_engines[p1]++;

		CheckConsistencyOfArticulatedVehicle(v);
	}

	return value;
}


/* Check if all the wagons of the given train are in a depot, returns the
 * number of cars (including loco) then. If not it returns -1 */
int CheckTrainInDepot(const Vehicle *v, bool needs_to_be_stopped)
{
	TileIndex tile = v->tile;

	/* check if stopped in a depot */
	if (!IsRailDepotTile(tile) || v->cur_speed != 0) return -1;

	int count = 0;
	for (; v != NULL; v = v->Next()) {
		/* This count is used by the depot code to determine the number of engines
		 * in the consist. Exclude articulated parts so that autoreplacing to
		 * engines with more articulated parts than before works correctly.
		 *
		 * Also skip counting rear ends of multiheaded engines */
		if (!IsArticulatedPart(v) && !IsRearDualheaded(v)) count++;
		if (v->u.rail.track != TRACK_BIT_DEPOT || v->tile != tile ||
				(IsFrontEngine(v) && needs_to_be_stopped && !(v->vehstatus & VS_STOPPED))) {
			return -1;
		}
	}

	return count;
}

/* Used to check if the train is inside the depot and verifying that the VS_STOPPED flag is set */
int CheckTrainStoppedInDepot(const Vehicle *v)
{
	return CheckTrainInDepot(v, true);
}

/* Used to check if the train is inside the depot, but not checking the VS_STOPPED flag */
inline bool CheckTrainIsInsideDepot(const Vehicle *v)
{
	return CheckTrainInDepot(v, false) > 0;
}

/**
 * Unlink a rail wagon from the consist.
 * @param v Vehicle to remove.
 * @param first The first vehicle of the consist.
 * @return The first vehicle of the consist.
 */
static Vehicle *UnlinkWagon(Vehicle *v, Vehicle *first)
{
	/* unlinking the first vehicle of the chain? */
	if (v == first) {
		v = GetNextVehicle(v);
		if (v == NULL) return NULL;

		if (IsTrainWagon(v)) SetFreeWagon(v);

		/* First can be an articulated engine, meaning GetNextVehicle() isn't
		 * v->Next(). Thus set the next vehicle of the last articulated part
		 * and the last articulated part is just before the next vehicle (v). */
		v->Previous()->SetNext(NULL);

		return v;
	}

	Vehicle *u;
	for (u = first; GetNextVehicle(u) != v; u = GetNextVehicle(u)) {}
	GetLastEnginePart(u)->SetNext(GetNextVehicle(v));
	return first;
}

static Vehicle *FindGoodVehiclePos(const Vehicle *src)
{
	Vehicle *dst;
	EngineID eng = src->engine_type;
	TileIndex tile = src->tile;

	FOR_ALL_VEHICLES(dst) {
		if (dst->type == VEH_TRAIN && IsFreeWagon(dst) && dst->tile == tile && !HASBITS(dst->vehstatus, VS_CRASHED)) {
			/* check so all vehicles in the line have the same engine. */
			Vehicle *v = dst;

			while (v->engine_type == eng) {
				v = v->Next();
				if (v == NULL) return dst;
			}
		}
	}

	return NULL;
}

/*
 * add a vehicle v behind vehicle dest
 * use this function since it sets flags as needed
 */
static void AddWagonToConsist(Vehicle *v, Vehicle *dest)
{
	UnlinkWagon(v, v->First());
	if (dest == NULL) return;

	Vehicle *next = dest->Next();
	v->SetNext(NULL);
	dest->SetNext(v);
	v->SetNext(next);
	ClearFreeWagon(v);
	ClearFrontEngine(v);
}

/*
 * move around on the train so rear engines are placed correctly according to the other engines
 * always call with the front engine
 */
static void NormaliseTrainConsist(Vehicle *v)
{
	if (IsFreeWagon(v)) return;

	assert(IsFrontEngine(v));

	for (; v != NULL; v = GetNextVehicle(v)) {
		if (!IsMultiheaded(v) || !IsTrainEngine(v)) continue;

		/* make sure that there are no free cars before next engine */
		Vehicle *u;
		for (u = v; u->Next() != NULL && !IsTrainEngine(u->Next()); u = u->Next()) {}

		if (u == v->u.rail.other_multiheaded_part) continue;
		AddWagonToConsist(v->u.rail.other_multiheaded_part, u);
	}
}

/** Move a rail vehicle around inside the depot.
 * @param tile unused
 * @param flags type of operation
 *              Note: DC_AUTOREPLACE is set when autoreplace tries to undo its modifications or moves vehicles to temporary locations inside the depot.
 * @param p1 various bitstuffed elements
 * - p1 (bit  0 - 15) source vehicle index
 * - p1 (bit 16 - 31) what wagon to put the source wagon AFTER, XXX - INVALID_VEHICLE to make a new line
 * @param p2 (bit 0) move all vehicles following the source vehicle
 */
CommandCost CmdMoveRailVehicle(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	VehicleID s = GB(p1, 0, 16);
	VehicleID d = GB(p1, 16, 16);

	if (!IsValidVehicleID(s)) return CMD_ERROR;

	Vehicle *src = GetVehicle(s);

	if (src->type != VEH_TRAIN || !CheckOwnership(src->owner)) return CMD_ERROR;

	/* Do not allow moving crashed vehicles inside the depot, it is likely to cause asserts later */
	if (HASBITS(src->vehstatus, VS_CRASHED)) return CMD_ERROR;

	/* if nothing is selected as destination, try and find a matching vehicle to drag to. */
	Vehicle *dst;
	if (d == INVALID_VEHICLE) {
		dst = IsTrainEngine(src) ? NULL : FindGoodVehiclePos(src);
	} else {
		if (!IsValidVehicleID(d)) return CMD_ERROR;
		dst = GetVehicle(d);
		if (dst->type != VEH_TRAIN || !CheckOwnership(dst->owner)) return CMD_ERROR;

		/* Do not allow appending to crashed vehicles, too */
		if (HASBITS(dst->vehstatus, VS_CRASHED)) return CMD_ERROR;
	}

	/* if an articulated part is being handled, deal with its parent vehicle */
	while (IsArticulatedPart(src)) src = src->Previous();
	if (dst != NULL) {
		while (IsArticulatedPart(dst)) dst = dst->Previous();
	}

	/* don't move the same vehicle.. */
	if (src == dst) return CommandCost();

	/* locate the head of the two chains */
	Vehicle *src_head = src->First();
	Vehicle *dst_head;
	if (dst != NULL) {
		dst_head = dst->First();
		if (dst_head->tile != src_head->tile) return CMD_ERROR;
		/* Now deal with articulated part of destination wagon */
		dst = GetLastEnginePart(dst);
	} else {
		dst_head = NULL;
	}

	if (IsRearDualheaded(src)) return_cmd_error(STR_REAR_ENGINE_FOLLOW_FRONT_ERROR);

	/* when moving all wagons, we can't have the same src_head and dst_head */
	if (HasBit(p2, 0) && src_head == dst_head) return CommandCost();

	/* check if all vehicles in the source train are stopped inside a depot. */
	int src_len = CheckTrainStoppedInDepot(src_head);
	if (src_len < 0) return_cmd_error(STR_881A_TRAINS_CAN_ONLY_BE_ALTERED);

	if ((flags & DC_AUTOREPLACE) == 0) {
		/* Check whether there are more than 'max_len' train units (articulated parts and rear heads do not count) in the new chain */
		int max_len = _settings_game.vehicle.mammoth_trains ? 100 : 10;

		/* check the destination row if the source and destination aren't the same. */
		if (src_head != dst_head) {
			int dst_len = 0;

			if (dst_head != NULL) {
				/* check if all vehicles in the dest train are stopped. */
				dst_len = CheckTrainStoppedInDepot(dst_head);
				if (dst_len < 0) return_cmd_error(STR_881A_TRAINS_CAN_ONLY_BE_ALTERED);
			}

			/* We are moving between rows, so only count the wagons from the source
			 * row that are being moved. */
			if (HasBit(p2, 0)) {
				const Vehicle *u;
				for (u = src_head; u != src && u != NULL; u = GetNextVehicle(u))
					src_len--;
			} else {
				/* If moving only one vehicle, just count that. */
				src_len = 1;
			}

			if (src_len + dst_len > max_len) {
				/* Abort if we're adding too many wagons to a train. */
				if (dst_head != NULL && IsFrontEngine(dst_head)) return_cmd_error(STR_8819_TRAIN_TOO_LONG);
				/* Abort if we're making a train on a new row. */
				if (dst_head == NULL && IsTrainEngine(src)) return_cmd_error(STR_8819_TRAIN_TOO_LONG);
			}
		} else {
			/* Abort if we're creating a new train on an existing row. */
			if (src_len > max_len && src == src_head && IsTrainEngine(GetNextVehicle(src_head)))
				return_cmd_error(STR_8819_TRAIN_TOO_LONG);
		}
	}

	/* moving a loco to a new line?, then we need to assign a unitnumber. */
	if (dst == NULL && !IsFrontEngine(src) && IsTrainEngine(src)) {
		UnitID unit_num = ((flags & DC_AUTOREPLACE) != 0 ? 0 : GetFreeUnitNumber(VEH_TRAIN));
		if (unit_num > _settings_game.vehicle.max_trains)
			return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);

		if (flags & DC_EXEC) src->unitnumber = unit_num;
	}

	/* When we move the front vehicle, the second vehicle might need a unitnumber */
	if (!HasBit(p2, 0) && (IsFreeWagon(src) || (IsFrontEngine(src) && dst == NULL)) && (flags & DC_AUTOREPLACE) == 0) {
		Vehicle *second = GetNextUnit(src);
		if (second != NULL && IsTrainEngine(second) && GetFreeUnitNumber(VEH_TRAIN) > _settings_game.vehicle.max_trains) {
			return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);
		}
	}

	/*
	 * Check whether the vehicles in the source chain are in the destination
	 * chain. This can easily be done by checking whether the first vehicle
	 * of the source chain is in the destination chain as the Next/Previous
	 * pointers always make a doubly linked list of it where the assumption
	 * v->Next()->Previous() == v holds (assuming v->Next() != NULL).
	 */
	bool src_in_dst = false;
	for (Vehicle *v = dst_head; !src_in_dst && v != NULL; v = v->Next()) src_in_dst = v == src;

	/*
	 * If the source chain is in the destination chain then the user is
	 * only reordering the vehicles, thus not attaching a new vehicle.
	 * Therefor the 'allow wagon attach' callback does not need to be
	 * called. If it would be called strange things would happen because
	 * one 'attaches' an already 'attached' vehicle causing more trouble
	 * than it actually solves (infinite loops and such).
	 */
	if (dst_head != NULL && !src_in_dst && (flags & DC_AUTOREPLACE) == 0) {
		/*
		 * When performing the 'allow wagon attach' callback, we have to check
		 * that for each and every wagon, not only the first one. This means
		 * that we have to test one wagon, attach it to the train and then test
		 * the next wagon till we have reached the end. We have to restore it
		 * to the state it was before we 'tried' attaching the train when the
		 * attaching fails or succeeds because we are not 'only' doing this
		 * in the DC_EXEC state.
		 */
		Vehicle *dst_tail = dst_head;
		while (dst_tail->Next() != NULL) dst_tail = dst_tail->Next();

		Vehicle *orig_tail = dst_tail;
		Vehicle *next_to_attach = src;
		Vehicle *src_previous = src->Previous();

		while (next_to_attach != NULL) {
			/* Don't check callback for articulated or rear dual headed parts */
			if (!IsArticulatedPart(next_to_attach) && !IsRearDualheaded(next_to_attach)) {
				/* Back up and clear the first_engine data to avoid using wagon override group */
				EngineID first_engine = next_to_attach->u.rail.first_engine;
				next_to_attach->u.rail.first_engine = INVALID_ENGINE;

				uint16 callback = GetVehicleCallbackParent(CBID_TRAIN_ALLOW_WAGON_ATTACH, 0, 0, dst_head->engine_type, next_to_attach, dst_head);

				/* Restore original first_engine data */
				next_to_attach->u.rail.first_engine = first_engine;

				if (callback != CALLBACK_FAILED) {
					StringID error = STR_NULL;

					if (callback == 0xFD) error = STR_INCOMPATIBLE_RAIL_TYPES;
					if (callback < 0xFD) error = GetGRFStringID(GetEngineGRFID(dst_head->engine_type), 0xD000 + callback);

					if (error != STR_NULL) {
						/*
						 * The attaching is not allowed. In this case 'next_to_attach'
						 * can contain some vehicles of the 'source' and the destination
						 * train can have some too. We 'just' add the to-be added wagons
						 * to the chain and then split it where it was previously
						 * separated, i.e. the tail of the original destination train.
						 * Furthermore the 'previous' link of the original source vehicle needs
						 * to be restored, otherwise the train goes missing in the depot.
						 */
						dst_tail->SetNext(next_to_attach);
						orig_tail->SetNext(NULL);
						if (src_previous != NULL) src_previous->SetNext(src);

						return_cmd_error(error);
					}
				}
			}

			/* Only check further wagons if told to move the chain */
			if (!HasBit(p2, 0)) break;

			/*
			 * Adding a next wagon to the chain so we can test the other wagons.
			 * First 'take' the first wagon from 'next_to_attach' and move it
			 * to the next wagon. Then add that to the tail of the destination
			 * train and update the tail with the new vehicle.
			 */
			Vehicle *to_add = next_to_attach;
			next_to_attach = next_to_attach->Next();

			to_add->SetNext(NULL);
			dst_tail->SetNext(to_add);
			dst_tail = dst_tail->Next();
		}

		/*
		 * When we reach this the attaching is allowed. It also means that the
		 * chain of vehicles to attach is empty, so we do not need to merge that.
		 * This means only the splitting needs to be done.
		 * Furthermore the 'previous' link of the original source vehicle needs
		 * to be restored, otherwise the train goes missing in the depot.
		 */
		orig_tail->SetNext(NULL);
		if (src_previous != NULL) src_previous->SetNext(src);
	}

	/* do it? */
	if (flags & DC_EXEC) {
		/* If we move the front Engine and if the second vehicle is not an engine
		   add the whole vehicle to the DEFAULT_GROUP */
		if (IsFrontEngine(src) && !IsDefaultGroupID(src->group_id)) {
			Vehicle *v = GetNextVehicle(src);

			if (v != NULL && IsTrainEngine(v)) {
				v->group_id   = src->group_id;
				src->group_id = DEFAULT_GROUP;
			}
		}

		if (HasBit(p2, 0)) {
			/* unlink ALL wagons */
			if (src != src_head) {
				Vehicle *v = src_head;
				while (GetNextVehicle(v) != src) v = GetNextVehicle(v);
				GetLastEnginePart(v)->SetNext(NULL);
			} else {
				InvalidateWindowData(WC_VEHICLE_DEPOT, src_head->tile); // We removed a line
				src_head = NULL;
			}
		} else {
			/* if moving within the same chain, dont use dst_head as it may get invalidated */
			if (src_head == dst_head) dst_head = NULL;
			/* unlink single wagon from linked list */
			src_head = UnlinkWagon(src, src_head);
			GetLastEnginePart(src)->SetNext(NULL);
		}

		if (dst == NULL) {
			/* We make a new line in the depot, so we know already that we invalidate the window data */
			InvalidateWindowData(WC_VEHICLE_DEPOT, src->tile);

			/* move the train to an empty line. for locomotives, we set the type to TS_Front. for wagons, 4. */
			if (IsTrainEngine(src)) {
				if (!IsFrontEngine(src)) {
					/* setting the type to 0 also involves setting up the orders field. */
					SetFrontEngine(src);
					assert(src->orders.list == NULL);

					/* Decrease the engines number of the src engine_type */
					if (!IsDefaultGroupID(src->group_id) && IsValidGroupID(src->group_id)) {
						GetGroup(src->group_id)->num_engines[src->engine_type]--;
					}

					/* If we move an engine to a new line affect it to the DEFAULT_GROUP */
					src->group_id = DEFAULT_GROUP;
				}
			} else {
				SetFreeWagon(src);
			}
			dst_head = src;
		} else {
			if (IsFrontEngine(src)) {
				/* the vehicle was previously a loco. need to free the order list and delete vehicle windows etc. */
				DeleteWindowById(WC_VEHICLE_VIEW, src->index);
				DeleteWindowById(WC_VEHICLE_ORDERS, src->index);
				DeleteWindowById(WC_VEHICLE_REFIT, src->index);
				DeleteWindowById(WC_VEHICLE_DETAILS, src->index);
				DeleteWindowById(WC_VEHICLE_TIMETABLE, src->index);
				DeleteVehicleOrders(src);
				RemoveVehicleFromGroup(src);
			}

			if (IsFrontEngine(src) || IsFreeWagon(src)) {
				InvalidateWindowData(WC_VEHICLE_DEPOT, src->tile);
				ClearFrontEngine(src);
				ClearFreeWagon(src);
				src->unitnumber = 0; // doesn't occupy a unitnumber anymore.
			}

			/* link in the wagon(s) in the chain. */
			{
				Vehicle *v;

				for (v = src; GetNextVehicle(v) != NULL; v = GetNextVehicle(v)) {}
				GetLastEnginePart(v)->SetNext(dst->Next());
			}
			dst->SetNext(src);
		}

		if (src->u.rail.other_multiheaded_part != NULL) {
			if (src->u.rail.other_multiheaded_part == src_head) {
				src_head = src_head->Next();
			}
			AddWagonToConsist(src->u.rail.other_multiheaded_part, src);
		}

		/* If there is an engine behind first_engine we moved away, it should become new first_engine
		 * To do this, CmdMoveRailVehicle must be called once more
		 * we can't loop forever here because next time we reach this line we will have a front engine */
		if (src_head != NULL && !IsFrontEngine(src_head) && IsTrainEngine(src_head)) {
			/* As in CmdMoveRailVehicle src_head->group_id will be equal to DEFAULT_GROUP
			 * we need to save the group and reaffect it to src_head */
			const GroupID tmp_g = src_head->group_id;
			CmdMoveRailVehicle(0, flags, src_head->index | (INVALID_VEHICLE << 16), 1, text);
			SetTrainGroupID(src_head, tmp_g);
			src_head = NULL; // don't do anything more to this train since the new call will do it
		}

		if (src_head != NULL) {
			NormaliseTrainConsist(src_head);
			TrainConsistChanged(src_head, false);
			UpdateTrainGroupID(src_head);
			if (IsFrontEngine(src_head)) {
				/* Update the refit button and window */
				InvalidateWindow(WC_VEHICLE_REFIT, src_head->index);
				InvalidateWindowWidget(WC_VEHICLE_VIEW, src_head->index, VVW_WIDGET_REFIT_VEH);
			}
			/* Update the depot window */
			InvalidateWindow(WC_VEHICLE_DEPOT, src_head->tile);
		}

		if (dst_head != NULL) {
			NormaliseTrainConsist(dst_head);
			TrainConsistChanged(dst_head, false);
			UpdateTrainGroupID(dst_head);
			if (IsFrontEngine(dst_head)) {
				/* Update the refit button and window */
				InvalidateWindowWidget(WC_VEHICLE_VIEW, dst_head->index, VVW_WIDGET_REFIT_VEH);
				InvalidateWindow(WC_VEHICLE_REFIT, dst_head->index);
			}
			/* Update the depot window */
			InvalidateWindow(WC_VEHICLE_DEPOT, dst_head->tile);
		}

		InvalidateWindowClassesData(WC_TRAINS_LIST, 0);
	}

	return CommandCost();
}

/** Sell a (single) train wagon/engine.
 * @param tile unused
 * @param flags type of operation
 * @param p1 the wagon/engine index
 * @param p2 the selling mode
 * - p2 = 0: only sell the single dragged wagon/engine (and any belonging rear-engines)
 * - p2 = 1: sell the vehicle and all vehicles following it in the chain
 *           if the wagon is dragged, don't delete the possibly belonging rear-engine to some front
 */
CommandCost CmdSellRailWagon(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	/* Check if we deleted a vehicle window */
	Window *w = NULL;

	if (!IsValidVehicleID(p1) || p2 > 1) return CMD_ERROR;

	Vehicle *v = GetVehicle(p1);

	if (v->type != VEH_TRAIN || !CheckOwnership(v->owner)) return CMD_ERROR;

	if (HASBITS(v->vehstatus, VS_CRASHED)) return_cmd_error(STR_CAN_T_SELL_DESTROYED_VEHICLE);

	while (IsArticulatedPart(v)) v = v->Previous();
	Vehicle *first = v->First();

	/* make sure the vehicle is stopped in the depot */
	if (CheckTrainStoppedInDepot(first) < 0) {
		return_cmd_error(STR_881A_TRAINS_CAN_ONLY_BE_ALTERED);
	}

	if (IsRearDualheaded(v)) return_cmd_error(STR_REAR_ENGINE_FOLLOW_FRONT_ERROR);

	if (flags & DC_EXEC) {
		if (v == first && IsFrontEngine(first)) {
			DeleteWindowById(WC_VEHICLE_VIEW, first->index);
			DeleteWindowById(WC_VEHICLE_ORDERS, first->index);
			DeleteWindowById(WC_VEHICLE_REFIT, first->index);
			DeleteWindowById(WC_VEHICLE_DETAILS, first->index);
			DeleteWindowById(WC_VEHICLE_TIMETABLE, first->index);
		}
		InvalidateWindow(WC_VEHICLE_DEPOT, first->tile);
		InvalidateWindowClassesData(WC_TRAINS_LIST, 0);
	}

	CommandCost cost(EXPENSES_NEW_VEHICLES);
	switch (p2) {
		case 0: { // Delete given wagon
			bool switch_engine = false;    // update second wagon to engine?

			/* 1. Delete the engine, if it is dualheaded also delete the matching
			 * rear engine of the loco (from the point of deletion onwards) */
			Vehicle *rear = (IsMultiheaded(v) &&
				IsTrainEngine(v)) ? v->u.rail.other_multiheaded_part : NULL;

			if (rear != NULL) {
				cost.AddCost(-rear->value);
				if (flags & DC_EXEC) {
					UnlinkWagon(rear, first);
					delete rear;
				}
			}

			/* 2. We are selling the front vehicle, some special action might be required
			 * here, so take attention */
			if (v == first) {
				Vehicle *new_f = GetNextVehicle(first);

				/* 2.2 If there are wagons present after the deleted front engine, check
				 * if the second wagon (which will be first) is an engine. If it is one,
				 * promote it as a new train, retaining the unitnumber, orders */
				if (new_f != NULL && IsTrainEngine(new_f)) {
					if (IsTrainEngine(first)) {
						/* Let the new front engine take over the setup of the old engine */
						switch_engine = true;

						if (flags & DC_EXEC) {
							/* Make sure the group counts stay correct. */
							new_f->group_id        = first->group_id;
							first->group_id        = DEFAULT_GROUP;

							/* Copy orders (by sharing) */
							new_f->orders.list     = first->orders.list;
							new_f->AddToShared(first);
							DeleteVehicleOrders(first);

							/* Copy other important data from the front engine */
							new_f->CopyVehicleConfigAndStatistics(first);

							/* If we deleted a window then open a new one for the 'new' train */
							if (IsLocalCompany() && w != NULL) ShowVehicleViewWindow(new_f);
						}
					} else {
						/* We are selling a free wagon, and construct a new train at the same time.
						 * This needs lots of extra checks (e.g. train limit), which are done by first moving
						 * the remaining vehicles to a new row */
						cost.AddCost(DoCommand(0, new_f->index | INVALID_VEHICLE << 16, 1, flags, CMD_MOVE_RAIL_VEHICLE));
						if (cost.Failed()) return cost;
					}
				}
			}

			/* 3. Delete the requested wagon */
			cost.AddCost(-v->value);
			if (flags & DC_EXEC) {
				first = UnlinkWagon(v, first);
				delete v;

				/* 4 If the second wagon was an engine, update it to front_engine
				 * which UnlinkWagon() has changed to TS_Free_Car */
				if (switch_engine) SetFrontEngine(first);

				/* 5. If the train still exists, update its acceleration, window, etc. */
				if (first != NULL) {
					NormaliseTrainConsist(first);
					TrainConsistChanged(first, false);
					UpdateTrainGroupID(first);
					if (IsFrontEngine(first)) InvalidateWindow(WC_VEHICLE_REFIT, first->index);
				}

			}
		} break;
		case 1: { // Delete wagon and all wagons after it given certain criteria
			/* Start deleting every vehicle after the selected one
			 * If we encounter a matching rear-engine to a front-engine
			 * earlier in the chain (before deletion), leave it alone */
			for (Vehicle *tmp; v != NULL; v = tmp) {
				tmp = GetNextVehicle(v);

				if (IsMultiheaded(v)) {
					if (IsTrainEngine(v)) {
						/* We got a front engine of a multiheaded set. Now we will sell the rear end too */
						Vehicle *rear = v->u.rail.other_multiheaded_part;

						if (rear != NULL) {
							cost.AddCost(-rear->value);

							/* If this is a multiheaded vehicle with nothing
							 * between the parts, tmp will be pointing to the
							 * rear part, which is unlinked from the train and
							 * deleted here. However, because tmp has already
							 * been set it needs to be updated now so that the
							 * loop never sees the rear part. */
							if (tmp == rear) tmp = GetNextVehicle(tmp);

							if (flags & DC_EXEC) {
								first = UnlinkWagon(rear, first);
								delete rear;
							}
						}
					} else if (v->u.rail.other_multiheaded_part != NULL) {
						/* The front to this engine is earlier in this train. Do nothing */
						continue;
					}
				}

				cost.AddCost(-v->value);
				if (flags & DC_EXEC) {
					first = UnlinkWagon(v, first);
					delete v;
				}
			}

			/* 3. If it is still a valid train after selling, update its acceleration and cached values */
			if (flags & DC_EXEC && first != NULL) {
				NormaliseTrainConsist(first);
				TrainConsistChanged(first, false);
				UpdateTrainGroupID(first);
				InvalidateWindow(WC_VEHICLE_REFIT, first->index);
			}
		} break;
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

static void UpdateVarsAfterSwap(Vehicle *v)
{
	v->UpdateDeltaXY(v->direction);
	v->cur_image = v->GetImage(v->direction);
	VehicleMove(v, true);
}

static inline void SetLastSpeed(Vehicle *v, int spd)
{
	int old = v->u.rail.last_speed;
	if (spd != old) {
		v->u.rail.last_speed = spd;
		if (_settings_client.gui.vehicle_speed || (old == 0) != (spd == 0)) {
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
		}
	}
}

/** Mark a train as stuck and stop it if it isn't stopped right now. */
static void MarkTrainAsStuck(Vehicle *v)
{
	if (!HasBit(v->u.rail.flags, VRF_TRAIN_STUCK)) {
		/* It is the first time the problem occured, set the "train stuck" flag. */
		SetBit(v->u.rail.flags, VRF_TRAIN_STUCK);
		v->load_unload_time_rem = 0;

		/* Stop train */
		v->cur_speed = 0;
		v->subspeed = 0;
		SetLastSpeed(v, 0);

		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
	}
}

static void SwapTrainFlags(uint16 *swap_flag1, uint16 *swap_flag2)
{
	uint16 flag1 = *swap_flag1;
	uint16 flag2 = *swap_flag2;

	/* Clear the flags */
	ClrBit(*swap_flag1, VRF_GOINGUP);
	ClrBit(*swap_flag1, VRF_GOINGDOWN);
	ClrBit(*swap_flag2, VRF_GOINGUP);
	ClrBit(*swap_flag2, VRF_GOINGDOWN);

	/* Reverse the rail-flags (if needed) */
	if (HasBit(flag1, VRF_GOINGUP)) {
		SetBit(*swap_flag2, VRF_GOINGDOWN);
	} else if (HasBit(flag1, VRF_GOINGDOWN)) {
		SetBit(*swap_flag2, VRF_GOINGUP);
	}
	if (HasBit(flag2, VRF_GOINGUP)) {
		SetBit(*swap_flag1, VRF_GOINGDOWN);
	} else if (HasBit(flag2, VRF_GOINGDOWN)) {
		SetBit(*swap_flag1, VRF_GOINGUP);
	}
}

static void ReverseTrainSwapVeh(Vehicle *v, int l, int r)
{
	Vehicle *a, *b;

	/* locate vehicles to swap */
	for (a = v; l != 0; l--) a = a->Next();
	for (b = v; r != 0; r--) b = b->Next();

	if (a != b) {
		/* swap the hidden bits */
		{
			uint16 tmp = (a->vehstatus & ~VS_HIDDEN) | (b->vehstatus&VS_HIDDEN);
			b->vehstatus = (b->vehstatus & ~VS_HIDDEN) | (a->vehstatus&VS_HIDDEN);
			a->vehstatus = tmp;
		}

		Swap(a->u.rail.track, b->u.rail.track);
		Swap(a->direction,    b->direction);

		/* toggle direction */
		if (a->u.rail.track != TRACK_BIT_DEPOT) a->direction = ReverseDir(a->direction);
		if (b->u.rail.track != TRACK_BIT_DEPOT) b->direction = ReverseDir(b->direction);

		Swap(a->x_pos, b->x_pos);
		Swap(a->y_pos, b->y_pos);
		Swap(a->tile,  b->tile);
		Swap(a->z_pos, b->z_pos);

		SwapTrainFlags(&a->u.rail.flags, &b->u.rail.flags);

		/* update other vars */
		UpdateVarsAfterSwap(a);
		UpdateVarsAfterSwap(b);

		/* call the proper EnterTile function unless we are in a wormhole */
		if (a->u.rail.track != TRACK_BIT_WORMHOLE) VehicleEnterTile(a, a->tile, a->x_pos, a->y_pos);
		if (b->u.rail.track != TRACK_BIT_WORMHOLE) VehicleEnterTile(b, b->tile, b->x_pos, b->y_pos);
	} else {
		if (a->u.rail.track != TRACK_BIT_DEPOT) a->direction = ReverseDir(a->direction);
		UpdateVarsAfterSwap(a);

		if (a->u.rail.track != TRACK_BIT_WORMHOLE) VehicleEnterTile(a, a->tile, a->x_pos, a->y_pos);
	}

	/* Update train's power incase tiles were different rail type */
	TrainPowerChanged(v);
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
	/* not a train || not front engine || crashed */
	if (v->type != VEH_TRAIN || !IsFrontEngine(v) || v->vehstatus & VS_CRASHED) return NULL;

	TileIndex tile = *(TileIndex*)data;

	if (TrainApproachingCrossingTile(v) != tile) return NULL;

	return v;
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
	bool new_state = HasVehicleOnPos(tile, NULL, &TrainOnTileEnum) || TrainApproachingCrossing(tile) || GetCrossingReservation(tile);

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
static void AdvanceWagonsBeforeSwap(Vehicle *v)
{
	Vehicle *base = v;
	Vehicle *first = base;                    // first vehicle to move
	Vehicle *last = GetLastVehicleInChain(v); // last vehicle to move
	uint length = CountVehiclesInChain(v);

	while (length > 2) {
		last = last->Previous();
		first = first->Next();

		int differential = base->u.rail.cached_veh_length - last->u.rail.cached_veh_length;

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
static void AdvanceWagonsAfterSwap(Vehicle *v)
{
	/* first of all, fix the situation when the train was entering a depot */
	Vehicle *dep = v; // last vehicle in front of just left depot
	while (dep->Next() != NULL && (dep->u.rail.track == TRACK_BIT_DEPOT || dep->Next()->u.rail.track != TRACK_BIT_DEPOT)) {
		dep = dep->Next(); // find first vehicle outside of a depot, with next vehicle inside a depot
	}

	Vehicle *leave = dep->Next(); // first vehicle in a depot we are leaving now

	if (leave != NULL) {
		/* 'pull' next wagon out of the depot, so we won't miss it (it could stay in depot forever) */
		int d = TicksToLeaveDepot(dep);

		if (d <= 0) {
			leave->vehstatus &= ~VS_HIDDEN; // move it out of the depot
			leave->u.rail.track = TrackToTrackBits(GetRailDepotTrack(leave->tile));
			for (int i = 0; i >= d; i--) TrainController(leave, NULL); // maybe move it, and maybe let another wagon leave
		}
	} else {
		dep = NULL; // no vehicle in a depot, so no vehicle leaving a depot
	}

	Vehicle *base = v;
	Vehicle *first = base;                    // first vehicle to move
	Vehicle *last = GetLastVehicleInChain(v); // last vehicle to move
	uint length = CountVehiclesInChain(v);

	/* we have to make sure all wagons that leave a depot because of train reversing are moved coorectly
	 * they have already correct spacing, so we have to make sure they are moved how they should */
	bool nomove = (dep == NULL); // if there is no vehicle leaving a depot, limit the number of wagons moved immediatelly

	while (length > 2) {
		/* we reached vehicle (originally) in front of a depot, stop now
		 * (we would move wagons that are alredy moved with new wagon length) */
		if (base == dep) break;

		/* the last wagon was that one leaving a depot, so do not move it anymore */
		if (last == dep) nomove = true;

		last = last->Previous();
		first = first->Next();

		int differential = last->u.rail.cached_veh_length - base->u.rail.cached_veh_length;

		/* do not update images now */
		for (int i = 0; i < differential; i++) TrainController(first, (nomove ? last->Next() : NULL));

		base = first; // == base->Next()
		length -= 2;
	}
}


static void ReverseTrainDirection(Vehicle *v)
{
	if (IsRailDepotTile(v->tile)) {
		InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
	}

	/* Clear path reservation in front. */
	FreeTrainTrackReservation(v);

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

	ToggleBit(v->u.rail.flags, VRF_TOGGLE_REVERSE);

	ClrBit(v->u.rail.flags, VRF_REVERSING);

	/* recalculate cached data */
	TrainConsistChanged(v, true);

	/* update all images */
	for (Vehicle *u = v; u != NULL; u = u->Next()) u->cur_image = u->GetImage(u->direction);

	/* update crossing we were approaching */
	if (crossing != INVALID_TILE) UpdateLevelCrossing(crossing);

	/* maybe we are approaching crossing now, after reversal */
	crossing = TrainApproachingCrossingTile(v);
	if (crossing != INVALID_TILE) MaybeBarCrossingWithSound(crossing);

	/* If we are inside a depot after reversing, don't bother with path reserving. */
	if (v->u.rail.track & TRACK_BIT_DEPOT) {
		/* Can't be stuck here as inside a depot is always a safe tile. */
		if (HasBit(v->u.rail.flags, VRF_TRAIN_STUCK)) InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
		ClrBit(v->u.rail.flags, VRF_TRAIN_STUCK);
		return;
	}

	/* TrainExitDir does not always produce the desired dir for depots and
	 * tunnels/bridges that is needed for UpdateSignalsOnSegment. */
	DiagDirection dir = TrainExitDir(v->direction, v->u.rail.track);
	if (IsRailDepotTile(v->tile) || IsTileType(v->tile, MP_TUNNELBRIDGE)) dir = INVALID_DIAGDIR;

	if (UpdateSignalsOnSegment(v->tile, dir, v->owner) == SIGSEG_PBS || _settings_game.pf.reserve_paths) {
		/* If we are currently on a tile with conventional signals, we can't treat the
		 * current tile as a safe tile or we would enter a PBS block without a reservation. */
		bool first_tile_okay = !(IsTileType(v->tile, MP_RAILWAY) &&
			HasSignalOnTrackdir(v->tile, GetVehicleTrackdir(v)) &&
			!IsPbsSignal(GetSignalType(v->tile, FindFirstTrack(v->u.rail.track))));

		if (IsRailwayStationTile(v->tile)) SetRailwayStationPlatformReservation(v->tile, TrackdirToExitdir(GetVehicleTrackdir(v)), true);
		if (TryPathReserve(v, false, first_tile_okay)) {
			/* Do a look-ahead now in case our current tile was already a safe tile. */
			CheckNextTrainTile(v);
		} else if (v->current_order.GetType() != OT_LOADING) {
			/* Do not wait for a way out when we're still loading */
			MarkTrainAsStuck(v);
		}
	} else if (HasBit(v->u.rail.flags, VRF_TRAIN_STUCK)) {
		/* A train not inside a PBS block can't be stuck. */
		ClrBit(v->u.rail.flags, VRF_TRAIN_STUCK);
		v->load_unload_time_rem = 0;
	}
}

/** Reverse train.
 * @param tile unused
 * @param flags type of operation
 * @param p1 train to reverse
 * @param p2 if true, reverse a unit in a train (needs to be in a depot)
 */
CommandCost CmdReverseTrainDirection(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	Vehicle *v = GetVehicle(p1);

	if (v->type != VEH_TRAIN || !CheckOwnership(v->owner)) return CMD_ERROR;

	if (p2 != 0) {
		/* turn a single unit around */

		if (IsMultiheaded(v) || HasBit(EngInfo(v->engine_type)->callbackmask, CBM_VEHICLE_ARTIC_ENGINE)) {
			return_cmd_error(STR_ONLY_TURN_SINGLE_UNIT);
		}

		Vehicle *front = v->First();
		/* make sure the vehicle is stopped in the depot */
		if (CheckTrainStoppedInDepot(front) < 0) {
			return_cmd_error(STR_881A_TRAINS_CAN_ONLY_BE_ALTERED);
		}

		if (flags & DC_EXEC) {
			ToggleBit(v->u.rail.flags, VRF_REVERSE_DIRECTION);
			InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
			InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
		}
	} else {
		/* turn the whole train around */
		if (v->vehstatus & VS_CRASHED || v->breakdown_ctr != 0) return CMD_ERROR;

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

			if (_settings_game.vehicle.train_acceleration_model != TAM_ORIGINAL && v->cur_speed != 0) {
				ToggleBit(v->u.rail.flags, VRF_REVERSING);
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

/** Force a train through a red signal
 * @param tile unused
 * @param flags type of operation
 * @param p1 train to ignore the red signal
 * @param p2 unused
 */
CommandCost CmdForceTrainProceed(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	Vehicle *v = GetVehicle(p1);

	if (v->type != VEH_TRAIN || !CheckOwnership(v->owner)) return CMD_ERROR;

	if (flags & DC_EXEC) v->u.rail.force_proceed = 0x50;

	return CommandCost();
}

/** Refits a train to the specified cargo type.
 * @param tile unused
 * @param flags type of operation
 * @param p1 vehicle ID of the train to refit
 * param p2 various bitstuffed elements
 * - p2 = (bit 0-7) - the new cargo type to refit to
 * - p2 = (bit 8-15) - the new cargo subtype to refit to
 * - p2 = (bit 16) - refit only this vehicle
 * @return cost of refit or error
 */
CommandCost CmdRefitRailVehicle(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	CargoID new_cid = GB(p2, 0, 8);
	byte new_subtype = GB(p2, 8, 8);
	bool only_this = HasBit(p2, 16);

	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	Vehicle *v = GetVehicle(p1);

	if (v->type != VEH_TRAIN || !CheckOwnership(v->owner)) return CMD_ERROR;
	if (CheckTrainStoppedInDepot(v) < 0) return_cmd_error(STR_TRAIN_MUST_BE_STOPPED);
	if (v->vehstatus & VS_CRASHED) return_cmd_error(STR_CAN_T_REFIT_DESTROYED_VEHICLE);

	/* Check cargo */
	if (new_cid >= NUM_CARGO) return CMD_ERROR;

	CommandCost cost(EXPENSES_TRAIN_RUN);
	uint num = 0;

	do {
		/* XXX: We also refit all the attached wagons en-masse if they
		 * can be refitted. This is how TTDPatch does it.  TODO: Have
		 * some nice [Refit] button near each wagon. --pasky */
		if (!CanRefitTo(v->engine_type, new_cid)) continue;

		const Engine *e = GetEngine(v->engine_type);
		if (e->CanCarryCargo()) {
			uint16 amount = CALLBACK_FAILED;

			if (HasBit(EngInfo(v->engine_type)->callbackmask, CBM_VEHICLE_REFIT_CAPACITY)) {
				/* Back up the vehicle's cargo type */
				CargoID temp_cid = v->cargo_type;
				byte temp_subtype = v->cargo_subtype;
				v->cargo_type = new_cid;
				v->cargo_subtype = new_subtype;
				/* Check the refit capacity callback */
				amount = GetVehicleCallback(CBID_VEHICLE_REFIT_CAPACITY, 0, 0, v->engine_type, v);
				/* Restore the original cargo type */
				v->cargo_type = temp_cid;
				v->cargo_subtype = temp_subtype;
			}

			if (amount == CALLBACK_FAILED) { // callback failed or not used, use default
				CargoID old_cid = e->GetDefaultCargoType();
				/* normally, the capacity depends on the cargo type, a rail vehicle can
				 * carry twice as much mail/goods as normal cargo, and four times as
				 * many passengers
				 */
				amount = e->u.rail.capacity;
				switch (old_cid) {
					case CT_PASSENGERS: break;
					case CT_MAIL:
					case CT_GOODS: amount *= 2; break;
					default:       amount *= 4; break;
				}
				switch (new_cid) {
					case CT_PASSENGERS: break;
					case CT_MAIL:
					case CT_GOODS: amount /= 2; break;
					default:       amount /= 4; break;
				}
			}

			if (new_cid != v->cargo_type) {
				cost.AddCost(GetRefitCost(v->engine_type));
			}

			num += amount;
			if (flags & DC_EXEC) {
				v->cargo.Truncate((v->cargo_type == new_cid) ? amount : 0);
				v->cargo_type = new_cid;
				v->cargo_cap = amount;
				v->cargo_subtype = new_subtype;
				InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
				InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
				InvalidateWindowClassesData(WC_TRAINS_LIST, 0);
			}
		}
	} while ((v = v->Next()) != NULL && !only_this);

	_returned_refit_capacity = num;

	/* Update the train's cached variables */
	if (flags & DC_EXEC) TrainConsistChanged(GetVehicle(p1)->First(), false);

	return cost;
}

struct TrainFindDepotData {
	uint best_length;
	TileIndex tile;
	Owner owner;
	/**
	 * true if reversing is necessary for the train to get to this depot
	 * This value is unused when new depot finding and NPF are both disabled
	 */
	bool reverse;
};

static bool NtpCallbFindDepot(TileIndex tile, TrainFindDepotData *tfdd, int track, uint length)
{
	if (IsTileType(tile, MP_RAILWAY) &&
			IsTileOwner(tile, tfdd->owner) &&
			IsRailDepot(tile)) {
		/* approximate number of tiles by dividing by DIAG_FACTOR */
		tfdd->best_length = length / DIAG_FACTOR;
		tfdd->tile = tile;
		return true;
	}

	return false;
}

/** returns the tile of a depot to goto to. The given vehicle must not be
 * crashed! */
static TrainFindDepotData FindClosestTrainDepot(Vehicle *v, int max_distance)
{
	assert(!(v->vehstatus & VS_CRASHED));

	TrainFindDepotData tfdd;
	tfdd.owner = v->owner;
	tfdd.reverse = false;

	if (IsRailDepotTile(v->tile)) {
		tfdd.tile = v->tile;
		tfdd.best_length = 0;
		return tfdd;
	}

	PBSTileInfo origin = FollowTrainReservation(v);
	if (IsRailDepotTile(origin.tile)) {
		tfdd.tile = origin.tile;
		tfdd.best_length = 0;
		return tfdd;
	}

	tfdd.best_length = UINT_MAX;

	uint8 pathfinder = _settings_game.pf.pathfinder_for_trains;
	if ((_settings_game.pf.reserve_paths || HasReservedTracks(v->tile, v->u.rail.track)) && pathfinder == VPF_NTP) pathfinder = VPF_NPF;

	switch (pathfinder) {
		case VPF_YAPF: { // YAPF
			bool found = YapfFindNearestRailDepotTwoWay(v, max_distance, NPF_INFINITE_PENALTY, &tfdd.tile, &tfdd.reverse);
			tfdd.best_length = found ? max_distance / 2 : UINT_MAX; // some fake distance or NOT_FOUND
		} break;

		case VPF_NPF: { // NPF
			const Vehicle *last = GetLastVehicleInChain(v);
			Trackdir trackdir = GetVehicleTrackdir(v);
			Trackdir trackdir_rev = ReverseTrackdir(GetVehicleTrackdir(last));

			assert(trackdir != INVALID_TRACKDIR);
			NPFFoundTargetData ftd = NPFRouteToDepotBreadthFirstTwoWay(v->tile, trackdir, false, last->tile, trackdir_rev, false, TRANSPORT_RAIL, 0, v->owner, v->u.rail.compatible_railtypes, NPF_INFINITE_PENALTY);
			if (ftd.best_bird_dist == 0) {
				/* Found target */
				tfdd.tile = ftd.node.tile;
				/* Our caller expects a number of tiles, so we just approximate that
				 * number by this. It might not be completely what we want, but it will
				 * work for now :-) We can possibly change this when the old pathfinder
				 * is removed. */
				tfdd.best_length = ftd.best_path_dist / NPF_TILE_LENGTH;
				if (NPFGetFlag(&ftd.node, NPF_FLAG_REVERSE)) tfdd.reverse = true;
			}
		} break;

		default:
		case VPF_NTP: { // NTP
			/* search in the forward direction first. */
			DiagDirection i = TrainExitDir(v->direction, v->u.rail.track);
			NewTrainPathfind(v->tile, 0, v->u.rail.compatible_railtypes, i, (NTPEnumProc*)NtpCallbFindDepot, &tfdd);
			if (tfdd.best_length == UINT_MAX){
				tfdd.reverse = true;
				/* search in backwards direction */
				i = TrainExitDir(ReverseDir(v->direction), v->u.rail.track);
				NewTrainPathfind(v->tile, 0, v->u.rail.compatible_railtypes, i, (NTPEnumProc*)NtpCallbFindDepot, &tfdd);
			}
		} break;
	}

	return tfdd;
}

bool Train::FindClosestDepot(TileIndex *location, DestinationID *destination, bool *reverse)
{
	TrainFindDepotData tfdd = FindClosestTrainDepot(this, 0);
	if (tfdd.best_length == UINT_MAX) return false;

	if (location    != NULL) *location    = tfdd.tile;
	if (destination != NULL) *destination = GetDepotByTile(tfdd.tile)->index;
	if (reverse     != NULL) *reverse     = tfdd.reverse;

	return true;
}

/** Send a train to a depot
 * @param tile unused
 * @param flags type of operation
 * @param p1 train to send to the depot
 * @param p2 various bitmasked elements
 * - p2 bit 0-3 - DEPOT_ flags (see vehicle.h)
 * - p2 bit 8-10 - VLW flag (for mass goto depot)
 */
CommandCost CmdSendTrainToDepot(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (p2 & DEPOT_MASS_SEND) {
		/* Mass goto depot requested */
		if (!ValidVLWFlags(p2 & VLW_MASK)) return CMD_ERROR;
		return SendAllVehiclesToDepot(VEH_TRAIN, flags, p2 & DEPOT_SERVICE, _current_company, (p2 & VLW_MASK), p1);
	}

	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	Vehicle *v = GetVehicle(p1);

	if (v->type != VEH_TRAIN) return CMD_ERROR;

	return v->SendToDepot(flags, (DepotCommand)(p2 & DEPOT_COMMAND_MASK));
}


void OnTick_Train()
{
	_age_cargo_skip_counter = (_age_cargo_skip_counter == 0) ? 184 : (_age_cargo_skip_counter - 1);
}

static const int8 _vehicle_smoke_pos[8] = {
	1, 1, 1, 0, -1, -1, -1, 0
};

static void HandleLocomotiveSmokeCloud(const Vehicle *v)
{
	bool sound = false;

	if (v->vehstatus & VS_TRAIN_SLOWING || v->load_unload_time_rem != 0 || v->cur_speed < 2) {
		return;
	}

	const Vehicle *u = v;

	do {
		const RailVehicleInfo *rvi = RailVehInfo(v->engine_type);
		int effect_offset = GB(v->u.rail.cached_vis_effect, 0, 4) - 8;
		byte effect_type = GB(v->u.rail.cached_vis_effect, 4, 2);
		bool disable_effect = HasBit(v->u.rail.cached_vis_effect, 6);

		/* no smoke? */
		if ((rvi->railveh_type == RAILVEH_WAGON && effect_type == 0) ||
				disable_effect ||
				v->vehstatus & VS_HIDDEN) {
			continue;
		}

		/* No smoke in depots or tunnels */
		if (IsRailDepotTile(v->tile) || IsTunnelTile(v->tile)) continue;

		/* No sparks for electric vehicles on nonelectrified tracks */
		if (!HasPowerOnRail(v->u.rail.railtype, GetTileRailType(v->tile))) continue;

		if (effect_type == 0) {
			/* Use default effect type for engine class. */
			effect_type = rvi->engclass;
		} else {
			effect_type--;
		}

		int x = _vehicle_smoke_pos[v->direction] * effect_offset;
		int y = _vehicle_smoke_pos[(v->direction + 2) % 8] * effect_offset;

		if (HasBit(v->u.rail.flags, VRF_REVERSE_DIRECTION)) {
			x = -x;
			y = -y;
		}

		switch (effect_type) {
			case 0:
				/* steam smoke. */
				if (GB(v->tick_counter, 0, 4) == 0) {
					CreateEffectVehicleRel(v, x, y, 10, EV_STEAM_SMOKE);
					sound = true;
				}
				break;

			case 1:
				/* diesel smoke */
				if (u->cur_speed <= 40 && Chance16(15, 128)) {
					CreateEffectVehicleRel(v, 0, 0, 10, EV_DIESEL_SMOKE);
					sound = true;
				}
				break;

			case 2:
				/* blue spark */
				if (GB(v->tick_counter, 0, 2) == 0 && Chance16(1, 45)) {
					CreateEffectVehicleRel(v, 0, 0, 10, EV_ELECTRIC_SPARK);
					sound = true;
				}
				break;

			default:
				break;
		}
	} while ((v = v->Next()) != NULL);

	if (sound) PlayVehicleSound(u, VSE_TRAIN_EFFECT);
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
static void CheckNextTrainTile(Vehicle *v)
{
	/* Don't do any look-ahead if path_backoff_interval is 255. */
	if (_settings_game.pf.path_backoff_interval == 255) return;

	/* Exit if we reached our destination depot or are inside a depot. */
	if ((v->tile == v->dest_tile && v->current_order.IsType(OT_GOTO_DEPOT)) || v->u.rail.track & TRACK_BIT_DEPOT) return;
	/* Exit if we are on a station tile and are going to stop. */
	if (IsRailwayStationTile(v->tile) && v->current_order.ShouldStopAtStation(v, GetStationIndex(v->tile))) return;
	/* Exit if the current order doesn't have a destination, but the train has orders. */
	if ((v->current_order.IsType(OT_NOTHING) || v->current_order.IsType(OT_LEAVESTATION) || v->current_order.IsType(OT_LOADING)) && v->GetNumOrders() > 0) return;

	Trackdir td = GetVehicleTrackdir(v);

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
				if (_settings_game.pf.pathfinder_for_trains != VPF_NTP && _settings_game.pf.forbid_90_deg) {
					tracks &= ~TrackCrossesTracks(TrackdirToTrack(ft.m_old_td));
				}
				ChooseTrainTrack(v, ft.m_new_tile, ft.m_exitdir, tracks, false, NULL, false);
			}
		}
	}
}

static bool CheckTrainStayInDepot(Vehicle *v)
{
	/* bail out if not all wagons are in the same depot or not in a depot at all */
	for (const Vehicle *u = v; u != NULL; u = u->Next()) {
		if (u->u.rail.track != TRACK_BIT_DEPOT || u->tile != v->tile) return false;
	}

	/* if the train got no power, then keep it in the depot */
	if (v->u.rail.cached_power == 0) {
		v->vehstatus |= VS_STOPPED;
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
		return true;
	}

	SigSegState seg_state;

	if (v->u.rail.force_proceed == 0) {
		/* force proceed was not pressed */
		if (++v->load_unload_time_rem < 37) {
			InvalidateWindowClasses(WC_TRAINS_LIST);
			return true;
		}

		v->load_unload_time_rem = 0;

		seg_state = _settings_game.pf.reserve_paths ? SIGSEG_PBS : UpdateSignalsOnSegment(v->tile, INVALID_DIAGDIR, v->owner);
		if (seg_state == SIGSEG_FULL || GetDepotWaypointReservation(v->tile)) {
			/* Full and no PBS signal in block or depot reserved, can't exit. */
			InvalidateWindowClasses(WC_TRAINS_LIST);
			return true;
		}
	} else {
		seg_state = _settings_game.pf.reserve_paths ? SIGSEG_PBS : UpdateSignalsOnSegment(v->tile, INVALID_DIAGDIR, v->owner);
	}

	/* Only leave when we can reserve a path to our destination. */
	if (seg_state == SIGSEG_PBS && !TryPathReserve(v) && v->u.rail.force_proceed == 0) {
		/* No path and no force proceed. */
		InvalidateWindowClasses(WC_TRAINS_LIST);
		MarkTrainAsStuck(v);
		return true;
	}

	SetDepotWaypointReservation(v->tile, true);
	if (_settings_client.gui.show_track_reservation) MarkTileDirtyByTile(v->tile);

	VehicleServiceInDepot(v);
	InvalidateWindowClasses(WC_TRAINS_LIST);
	v->PlayLeaveStationSound();

	v->u.rail.track = TRACK_BIT_X;
	if (v->direction & 2) v->u.rail.track = TRACK_BIT_Y;

	v->vehstatus &= ~VS_HIDDEN;
	v->cur_speed = 0;

	v->UpdateDeltaXY(v->direction);
	v->cur_image = v->GetImage(v->direction);
	VehicleMove(v, false);
	UpdateSignalsOnSegment(v->tile, INVALID_DIAGDIR, v->owner);
	UpdateTrainAcceleration(v);
	InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);

	return false;
}

/** Clear the reservation of a tile that was just left by a wagon on track_dir. */
static void ClearPathReservation(const Vehicle *v, TileIndex tile, Trackdir track_dir)
{
	DiagDirection dir = TrackdirToExitdir(track_dir);

	if (IsTileType(tile, MP_TUNNELBRIDGE)) {
		/* Are we just leaving a tunnel/bridge? */
		if (GetTunnelBridgeDirection(tile) == ReverseDiagDir(dir)) {
			TileIndex end = GetOtherTunnelBridgeEnd(tile);

			if (!HasVehicleOnTunnelBridge(tile, end, v)) {
				/* Free the reservation only if no other train is on the tiles. */
				SetTunnelBridgeReservation(tile, false);
				SetTunnelBridgeReservation(end, false);

				if (_settings_client.gui.show_track_reservation) {
					MarkTileDirtyByTile(tile);
					MarkTileDirtyByTile(end);
				}
			}
		}
	} else if (IsRailwayStationTile(tile)) {
		TileIndex new_tile = TileAddByDiagDir(tile, dir);
		/* If the new tile is not a further tile of the same station, we
		 * clear the reservation for the whole platform. */
		if (!IsCompatibleTrainStationTile(new_tile, tile)) {
			SetRailwayStationPlatformReservation(tile, ReverseDiagDir(dir), false);
		}
	} else {
		/* Any other tile */
		UnreserveRailTrack(tile, TrackdirToTrack(track_dir));
	}
}

/** Free the reserved path in front of a vehicle. */
void FreeTrainTrackReservation(const Vehicle *v, TileIndex origin, Trackdir orig_td)
{
	assert(IsFrontEngine(v));

	TileIndex tile = origin != INVALID_TILE ? origin : v->tile;
	Trackdir  td = orig_td != INVALID_TRACKDIR ? orig_td : GetVehicleTrackdir(v);
	bool      free_tile = tile != v->tile || !(IsRailwayStationTile(v->tile) || IsTileType(v->tile, MP_TUNNELBRIDGE));
	StationID station_id = IsRailwayStationTile(v->tile) ? GetStationIndex(v->tile) : INVALID_STATION;

	/* Don't free reservation if it's not ours. */
	if (TracksOverlap(GetReservedTrackbits(tile) | TrackToTrackBits(TrackdirToTrack(td)))) return;

	CFollowTrackRail ft(v, GetRailTypeInfo(v->u.rail.railtype)->compatible_railtypes);
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

/** Check for station tiles */
struct TrainTrackFollowerData {
	TileIndex dest_coords;
	StationID station_index; ///< station index we're heading for
	uint best_bird_dist;
	uint best_track_dist;
	TrackdirByte best_track;
};

static bool NtpCallbFindStation(TileIndex tile, TrainTrackFollowerData *ttfd, Trackdir track, uint length)
{
	/* heading for nowhere? */
	if (ttfd->dest_coords == 0) return false;

	/* did we reach the final station? */
	if ((ttfd->station_index == INVALID_STATION && tile == ttfd->dest_coords) || (
				IsTileType(tile, MP_STATION) &&
				IsRailwayStation(tile) &&
				GetStationIndex(tile) == ttfd->station_index
			)) {
		/* We do not check for dest_coords if we have a station_index,
		 * because in that case the dest_coords are just an
		 * approximation of where the station is */

		/* found station */
		ttfd->best_track = track;
		ttfd->best_bird_dist = 0;
		return true;
	} else {
		/* didn't find station, keep track of the best path so far. */
		uint dist = DistanceManhattan(tile, ttfd->dest_coords);
		if (dist < ttfd->best_bird_dist) {
			ttfd->best_bird_dist = dist;
			ttfd->best_track = track;
		}
		return false;
	}
}

static void FillWithStationData(TrainTrackFollowerData *fd, const Vehicle *v)
{
	fd->dest_coords = v->dest_tile;
	fd->station_index = v->current_order.IsType(OT_GOTO_STATION) ? v->current_order.GetDestination() : INVALID_STATION;
}

static const byte _initial_tile_subcoord[6][4][3] = {
{{ 15, 8, 1 }, { 0, 0, 0 }, { 0, 8, 5 }, { 0,  0, 0 }},
{{  0, 0, 0 }, { 8, 0, 3 }, { 0, 0, 0 }, { 8, 15, 7 }},
{{  0, 0, 0 }, { 7, 0, 2 }, { 0, 7, 6 }, { 0,  0, 0 }},
{{ 15, 8, 2 }, { 0, 0, 0 }, { 0, 0, 0 }, { 8, 15, 6 }},
{{ 15, 7, 0 }, { 8, 0, 4 }, { 0, 0, 0 }, { 0,  0, 0 }},
{{  0, 0, 0 }, { 0, 0, 0 }, { 0, 8, 4 }, { 7, 15, 0 }},
};

static const byte _search_directions[6][4] = {
	{ 0, 9, 2, 9 }, ///< track 1
	{ 9, 1, 9, 3 }, ///< track 2
	{ 9, 0, 3, 9 }, ///< track upper
	{ 1, 9, 9, 2 }, ///< track lower
	{ 3, 2, 9, 9 }, ///< track left
	{ 9, 9, 1, 0 }, ///< track right
};

static const byte _pick_track_table[6] = {1, 3, 2, 2, 0, 0};

/**
 * Perform pathfinding for a train.
 *
 * @param v The train
 * @param tile The tile the train is about to enter
 * @param enterdir Diagonal direction the train is coming from
 * @param tracks Usable tracks on the new tile
 * @param path_not_found [out] Set to false if the pathfinder couldn't find a way to the destination
 * @param do_track_reservation Path reservation is requested
 * @param dest [out] State and destination of the requested path
 * @return The best track the train should follow
 */
static Track DoTrainPathfind(Vehicle *v, TileIndex tile, DiagDirection enterdir, TrackBits tracks, bool *path_not_found, bool do_track_reservation, PBSTileInfo *dest)
{
	Track best_track;

#ifdef PF_BENCHMARK
	TIC()
#endif

	if (path_not_found) *path_not_found = false;

	uint8 pathfinder = _settings_game.pf.pathfinder_for_trains;
	if (do_track_reservation && pathfinder == VPF_NTP) pathfinder = VPF_NPF;

	switch (pathfinder) {
		case VPF_YAPF: { // YAPF
			Trackdir trackdir = YapfChooseRailTrack(v, tile, enterdir, tracks, path_not_found, do_track_reservation, dest);
			if (trackdir != INVALID_TRACKDIR) {
				best_track = TrackdirToTrack(trackdir);
			} else {
				best_track = FindFirstTrack(tracks);
			}
		} break;

		case VPF_NPF: { // NPF
			void *perf = NpfBeginInterval();

			NPFFindStationOrTileData fstd;
			NPFFillWithOrderData(&fstd, v, do_track_reservation);

			PBSTileInfo origin = FollowTrainReservation(v);
			assert(IsValidTrackdir(origin.trackdir));

			NPFFoundTargetData ftd = NPFRouteToStationOrTile(origin.tile, origin.trackdir, true, &fstd, TRANSPORT_RAIL, 0, v->owner, v->u.rail.compatible_railtypes);

			if (dest != NULL) {
				dest->tile = ftd.node.tile;
				dest->trackdir = (Trackdir)ftd.node.direction;
				dest->okay = ftd.res_okay;
			}

			if (ftd.best_trackdir == INVALID_TRACKDIR) {
				/* We are already at our target. Just do something
				 * @todo maybe display error?
				 * @todo: go straight ahead if possible? */
				best_track = FindFirstTrack(tracks);
			} else {
				/* If ftd.best_bird_dist is 0, we found our target and ftd.best_trackdir contains
				 * the direction we need to take to get there, if ftd.best_bird_dist is not 0,
				 * we did not find our target, but ftd.best_trackdir contains the direction leading
				 * to the tile closest to our target. */
				if (ftd.best_bird_dist != 0 && path_not_found != NULL) *path_not_found = true;
				/* Discard enterdir information, making it a normal track */
				best_track = TrackdirToTrack(ftd.best_trackdir);
			}

			int time = NpfEndInterval(perf);
			DEBUG(yapf, 4, "[NPFT] %d us - %d rounds - %d open - %d closed -- ", time, 0, _aystar_stats_open_size, _aystar_stats_closed_size);
		} break;

		default:
		case VPF_NTP: { // NTP
			void *perf = NpfBeginInterval();

			TrainTrackFollowerData fd;
			FillWithStationData(&fd, v);

			/* New train pathfinding */
			fd.best_bird_dist = UINT_MAX;
			fd.best_track_dist = UINT_MAX;
			fd.best_track = INVALID_TRACKDIR;

			NewTrainPathfind(tile - TileOffsByDiagDir(enterdir), v->dest_tile,
				v->u.rail.compatible_railtypes, enterdir, (NTPEnumProc*)NtpCallbFindStation, &fd);

			/* check whether the path was found or only 'guessed' */
			if (fd.best_bird_dist != 0 && path_not_found != NULL) *path_not_found = true;

			if (fd.best_track == INVALID_TRACKDIR) {
				/* blaha */
				best_track = FindFirstTrack(tracks);
			} else {
				best_track = TrackdirToTrack(fd.best_track);
			}

			int time = NpfEndInterval(perf);
			DEBUG(yapf, 4, "[NTPT] %d us - %d rounds - %d open - %d closed -- ", time, 0, 0, 0);
		} break;
	}

#ifdef PF_BENCHMARK
	TOC("PF time = ", 1)
#endif

	return best_track;
}

/**
 * Extend a train path as far as possible. Stops on encountering a safe tile,
 * another reservation or a track choice.
 * @return INVALID_TILE indicates that the reservation failed.
 */
static PBSTileInfo ExtendTrainReservation(const Vehicle *v, TrackBits *new_tracks, DiagDirection *enterdir)
{
	bool no_90deg_turns = _settings_game.pf.pathfinder_for_trains != VPF_NTP && _settings_game.pf.forbid_90_deg;
	PBSTileInfo origin = FollowTrainReservation(v);

	CFollowTrackRail ft(v);

	TileIndex tile = origin.tile;
	Trackdir  cur_td = origin.trackdir;
	while (ft.Follow(tile, cur_td)) {
		if (KillFirstBit(ft.m_new_td_bits) == TRACKDIR_BIT_NONE) {
			/* Possible signal tile. */
			if (HasOnewaySignalBlockingTrackdir(ft.m_new_tile, FindFirstTrackdir(ft.m_new_td_bits))) break;
		}

		if (no_90deg_turns) {
			ft.m_new_td_bits &= ~TrackdirCrossesTrackdirs(ft.m_old_td);
			if (ft.m_new_td_bits == TRACKDIR_BIT_NONE) break;
		}

		/* Station, depot or waypoint are a possible target. */
		bool target_seen = ft.m_is_station || (IsTileType(ft.m_new_tile, MP_RAILWAY) && !IsPlainRailTile(ft.m_new_tile));
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

		if (IsSafeWaitingPosition(v, tile, cur_td, true, no_90deg_turns)) {
			bool wp_free = IsWaitingPositionFree(v, tile, cur_td, no_90deg_turns);
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

		if (no_90deg_turns) {
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
 * @param override_tailtype Whether all physically compatible railtypes should be followed.
 * @return True if a path to a safe stopping tile could be reserved.
 */
static bool TryReserveSafeTrack(const Vehicle *v, TileIndex tile, Trackdir td, bool override_tailtype)
{
	if (_settings_game.pf.pathfinder_for_trains == VPF_YAPF) {
		return YapfRailFindNearestSafeTile(v, tile, td, override_tailtype);
	} else {
		return NPFRouteToSafeTile(v, tile, td, override_tailtype).res_okay;
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

			Order *order = GetVehicleOrder(this->v, this->index);
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
static Track ChooseTrainTrack(Vehicle *v, TileIndex tile, DiagDirection enterdir, TrackBits tracks, bool force_res, bool *got_reservation, bool mark_stuck)
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
			IsRailwayStationTile(v->tile) && v->current_order.GetDestination() == GetStationIndex(v->tile) :
			v->tile == v->dest_tile))) {
		orders.SwitchToNextOrder(true);
	}

	if (res_dest.tile != INVALID_TILE && !res_dest.okay) {
		/* Pathfinders are able to tell that route was only 'guessed'. */
		bool      path_not_found = false;
		TileIndex new_tile = res_dest.tile;

		Track next_track = DoTrainPathfind(v, new_tile, dest_enterdir, tracks, &path_not_found, do_track_reservation, &res_dest);
		if (new_tile == tile) best_track = next_track;

		/* handle "path not found" state */
		if (path_not_found) {
			/* PF didn't find the route */
			if (!HasBit(v->u.rail.flags, VRF_NO_PATH_TO_DESTINATION)) {
				/* it is first time the problem occurred, set the "path not found" flag */
				SetBit(v->u.rail.flags, VRF_NO_PATH_TO_DESTINATION);
				/* and notify user about the event */
				AI::NewEvent(v->owner, new AIEventVehicleLost(v->index));
				if (_settings_client.gui.lost_train_warn && v->owner == _local_company) {
					SetDParam(0, v->index);
					AddNewsItem(
						STR_TRAIN_IS_LOST,
						NS_ADVICE,
						v->index,
						0);
				}
			}
		} else {
			/* route found, is the train marked with "path not found" flag? */
			if (HasBit(v->u.rail.flags, VRF_NO_PATH_TO_DESTINATION)) {
				/* clear the flag as the PF's problem was solved */
				ClrBit(v->u.rail.flags, VRF_NO_PATH_TO_DESTINATION);
				/* can we also delete the "News" item somehow? */
			}
		}
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
			TryReserveRailTrack(v->tile, TrackdirToTrack(GetVehicleTrackdir(v)));
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
		if (_settings_game.pf.pathfinder_for_trains != VPF_NTP && _settings_game.pf.forbid_90_deg) {
			reachable &= ~TrackCrossesTracks(TrackdirToTrack(res_dest.trackdir));
		}

		/* Get next order with destination. */
		if (orders.SwitchToNextOrder(true)) {
			PBSTileInfo cur_dest;
			DoTrainPathfind(v, next_tile, exitdir, reachable, NULL, true, &cur_dest);
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

	TryReserveRailTrack(v->tile, TrackdirToTrack(GetVehicleTrackdir(v)));

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
bool TryPathReserve(Vehicle *v, bool mark_as_stuck, bool first_tile_okay)
{
	assert(v->type == VEH_TRAIN && IsFrontEngine(v));

	/* We have to handle depots specially as the track follower won't look
	 * at the depot tile itself but starts from the next tile. If we are still
	 * inside the depot, a depot reservation can never be ours. */
	if (v->u.rail.track & TRACK_BIT_DEPOT) {
		if (GetDepotWaypointReservation(v->tile)) {
			if (mark_as_stuck) MarkTrainAsStuck(v);
			return false;
		} else {
			/* Depot not reserved, but the next tile might be. */
			TileIndex next_tile = TileAddByDiagDir(v->tile, GetRailDepotDirection(v->tile));
			if (HasReservedTracks(next_tile, DiagdirReachesTracks(GetRailDepotDirection(v->tile)))) return false;
		}
	}

	/* Special check if we are in front of a two-sided conventional signal. */
	DiagDirection dir = TrainExitDir(v->direction, v->u.rail.track);
	TileIndex next_tile = TileAddByDiagDir(v->tile, dir);
	if (IsTileType(next_tile, MP_RAILWAY) && HasReservedTracks(next_tile, DiagdirReachesTracks(dir))) {
		/* Can have only one reserved trackdir. */
		Trackdir td = FindFirstTrackdir(TrackBitsToTrackdirBits(GetReservedTrackbits(next_tile)) & DiagdirReachesTrackdirs(dir));
		if (HasSignalOnTrackdir(next_tile, td) && HasSignalOnTrackdir(next_tile, ReverseTrackdir(td)) &&
				!IsPbsSignal(GetSignalType(next_tile, TrackdirToTrack(td)))) {
			/* Signal already reserved, is not ours. */
			if (mark_as_stuck) MarkTrainAsStuck(v);
			return false;
		}
	}

	bool other_train = false;
	PBSTileInfo origin = FollowTrainReservation(v, &other_train);
	/* If we have a reserved path and the path ends at a safe tile, we are finished already. */
	if (origin.okay && (v->tile != origin.tile || first_tile_okay)) {
		/* Can't be stuck then. */
		if (HasBit(v->u.rail.flags, VRF_TRAIN_STUCK)) InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
		ClrBit(v->u.rail.flags, VRF_TRAIN_STUCK);
		return true;
	}
	/* The path we are driving on is alread blocked by some other train.
	 * This can only happen when tracks and signals are changed. A crash
	 * is probably imminent, don't do any further reservation because
	 * it might cause stale reservations. */
	if (other_train && v->tile != origin.tile) {
		if (mark_as_stuck) MarkTrainAsStuck(v);
		return false;
	}

	/* If we are in a depot, tentativly reserve the depot. */
	if (v->u.rail.track & TRACK_BIT_DEPOT) {
		SetDepotWaypointReservation(v->tile, true);
		if (_settings_client.gui.show_track_reservation) MarkTileDirtyByTile(v->tile);
	}

	DiagDirection exitdir = TrackdirToExitdir(origin.trackdir);
	TileIndex     new_tile = TileAddByDiagDir(origin.tile, exitdir);
	TrackBits     reachable = TrackdirBitsToTrackBits(TrackStatusToTrackdirBits(GetTileTrackStatus(new_tile, TRANSPORT_RAIL, 0)) & DiagdirReachesTrackdirs(exitdir));

	if (_settings_game.pf.pathfinder_for_trains != VPF_NTP && _settings_game.pf.forbid_90_deg) reachable &= ~TrackCrossesTracks(TrackdirToTrack(origin.trackdir));

	bool res_made = false;
	ChooseTrainTrack(v, new_tile, exitdir, reachable, true, &res_made, mark_as_stuck);

	if (!res_made) {
		/* Free the depot reservation as well. */
		if (v->u.rail.track & TRACK_BIT_DEPOT) SetDepotWaypointReservation(v->tile, false);
		return false;
	}

	if (HasBit(v->u.rail.flags, VRF_TRAIN_STUCK)) {
		v->load_unload_time_rem = 0;
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
	}
	ClrBit(v->u.rail.flags, VRF_TRAIN_STUCK);
	return true;
}


static bool CheckReverseTrain(Vehicle *v)
{
	if (_settings_game.difficulty.line_reverse_mode != 0 ||
			v->u.rail.track == TRACK_BIT_DEPOT || v->u.rail.track == TRACK_BIT_WORMHOLE ||
			!(v->direction & 1)) {
		return false;
	}

	uint reverse_best = 0;

	assert(v->u.rail.track);

	switch (_settings_game.pf.pathfinder_for_trains) {
		case VPF_YAPF: // YAPF
			reverse_best = YapfCheckReverseTrain(v);
			break;

		case VPF_NPF: { // NPF
			NPFFindStationOrTileData fstd;
			NPFFoundTargetData ftd;
			Vehicle *last = GetLastVehicleInChain(v);

			NPFFillWithOrderData(&fstd, v);

			Trackdir trackdir = GetVehicleTrackdir(v);
			Trackdir trackdir_rev = ReverseTrackdir(GetVehicleTrackdir(last));
			assert(trackdir != INVALID_TRACKDIR);
			assert(trackdir_rev != INVALID_TRACKDIR);

			ftd = NPFRouteToStationOrTileTwoWay(v->tile, trackdir, false, last->tile, trackdir_rev, false, &fstd, TRANSPORT_RAIL, 0, v->owner, v->u.rail.compatible_railtypes);
			if (ftd.best_bird_dist != 0) {
				/* We didn't find anything, just keep on going straight ahead */
				reverse_best = false;
			} else {
				if (NPFGetFlag(&ftd.node, NPF_FLAG_REVERSE)) {
					reverse_best = true;
				} else {
					reverse_best = false;
				}
			}
		} break;

		default:
		case VPF_NTP: { // NTP
			TrainTrackFollowerData fd;
			FillWithStationData(&fd, v);

			int i = _search_directions[FindFirstTrack(v->u.rail.track)][DirToDiagDir(v->direction)];

			int best_track = -1;
			uint reverse = 0;
			uint best_bird_dist  = 0;
			uint best_track_dist = 0;

			for (;;) {
				fd.best_bird_dist = UINT_MAX;
				fd.best_track_dist = UINT_MAX;

				NewTrainPathfind(v->tile, v->dest_tile, v->u.rail.compatible_railtypes, (DiagDirection)(reverse ^ i), (NTPEnumProc*)NtpCallbFindStation, &fd);

				if (best_track != -1) {
					if (best_bird_dist != 0) {
						if (fd.best_bird_dist != 0) {
							/* neither reached the destination, pick the one with the smallest bird dist */
							if (fd.best_bird_dist > best_bird_dist) goto bad;
							if (fd.best_bird_dist < best_bird_dist) goto good;
						} else {
							/* we found the destination for the first time */
							goto good;
						}
					} else {
						if (fd.best_bird_dist != 0) {
							/* didn't find destination, but we've found the destination previously */
							goto bad;
						} else {
							/* both old & new reached the destination, compare track length */
							if (fd.best_track_dist > best_track_dist) goto bad;
							if (fd.best_track_dist < best_track_dist) goto good;
						}
					}

					/* if we reach this position, there's two paths of equal value so far.
					 * pick one randomly. */
					int r = GB(Random(), 0, 8);
					if (_pick_track_table[i] == (v->direction & 3)) r += 80;
					if (_pick_track_table[best_track] == (v->direction & 3)) r -= 80;
					if (r <= 127) goto bad;
				}
good:;
				best_track = i;
				best_bird_dist = fd.best_bird_dist;
				best_track_dist = fd.best_track_dist;
				reverse_best = reverse;
bad:;
				if (reverse != 0) break;
				reverse = 2;
			}
		} break;
	}

	return reverse_best != 0;
}

TileIndex Train::GetOrderStationLocation(StationID station)
{
	if (station == this->last_station_visited) this->last_station_visited = INVALID_STATION;

	const Station *st = GetStation(station);
	if (!(st->facilities & FACIL_TRAIN)) {
		/* The destination station has no trainstation tiles. */
		this->cur_order_index++;
		return 0;
	}

	return st->xy;
}

void Train::MarkDirty()
{
	Vehicle *v = this;
	do {
		v->cur_image = v->GetImage(v->direction);
		MarkSingleVehicleDirty(v);
	} while ((v = v->Next()) != NULL);

	/* need to update acceleration and cached values since the goods on the train changed. */
	TrainCargoChanged(this);
	UpdateTrainAcceleration(this);
}

/**
 * This function looks at the vehicle and updates it's speed (cur_speed
 * and subspeed) variables. Furthermore, it returns the distance that
 * the train can drive this tick. This distance is expressed as 256 * n,
 * where n is the number of straight (long) tracks the train can
 * traverse. This means that moving along a straight track costs 256
 * "speed" and a diagonal track costs 192 "speed".
 * @param v The vehicle to update the speed of.
 * @return distance to drive.
 */
static int UpdateTrainSpeed(Vehicle *v)
{
	uint accel;

	if (v->vehstatus & VS_STOPPED || HasBit(v->u.rail.flags, VRF_REVERSING) || HasBit(v->u.rail.flags, VRF_TRAIN_STUCK)) {
		switch (_settings_game.vehicle.train_acceleration_model) {
			default: NOT_REACHED();
			case TAM_ORIGINAL:  accel = v->acceleration * -4; break;
			case TAM_REALISTIC: accel = GetTrainAcceleration(v, AM_BRAKE); break;
		}
	} else {
		switch (_settings_game.vehicle.train_acceleration_model) {
			default: NOT_REACHED();
			case TAM_ORIGINAL:  accel = v->acceleration * 2; break;
			case TAM_REALISTIC: accel = GetTrainAcceleration(v, AM_ACCEL); break;
		}
	}

	uint spd = v->subspeed + accel;
	v->subspeed = (byte)spd;
	{
		int tempmax = v->max_speed;
		if (v->cur_speed > v->max_speed)
			tempmax = v->cur_speed - (v->cur_speed / 10) - 1;
		v->cur_speed = spd = Clamp(v->cur_speed + ((int)spd >> 8), 0, tempmax);
	}

	/* Scale speed by 3/4. Previously this was only done when the train was
	 * facing diagonally and would apply to however many moves the train made
	 * regardless the of direction actually moved in. Now it is always scaled,
	 * 256 spd is used to go straight and 192 is used to go diagonally
	 * (3/4 of 256). This results in the same effect, but without the error the
	 * previous method caused.
	 *
	 * The scaling is done in this direction and not by multiplying the amount
	 * to be subtracted by 4/3 so that the leftover speed can be saved in a
	 * byte in v->progress.
	 */
	int scaled_spd = spd * 3 >> 2;

	scaled_spd += v->progress;
	v->progress = 0; // set later in TrainLocoHandler or TrainController
	return scaled_spd;
}

static void TrainEnterStation(Vehicle *v, StationID station)
{
	v->last_station_visited = station;

	/* check if a train ever visited this station before */
	Station *st = GetStation(station);
	if (!(st->had_vehicle_of_type & HVOT_TRAIN)) {
		st->had_vehicle_of_type |= HVOT_TRAIN;
		SetDParam(0, st->index);
		AddNewsItem(
			STR_8801_CITIZENS_CELEBRATE_FIRST,
			v->owner == _local_company ? NS_ARRIVAL_COMPANY : NS_ARRIVAL_OTHER,
			v->index,
			st->index
		);
		AI::NewEvent(v->owner, new AIEventStationFirstVehicle(st->index, v->index));
	}

	v->BeginLoading();

	StationAnimationTrigger(st, v->tile, STAT_ANIM_TRAIN_ARRIVES);
}

static byte AfterSetTrainPos(Vehicle *v, bool new_tile)
{
	byte old_z = v->z_pos;
	v->z_pos = GetSlopeZ(v->x_pos, v->y_pos);

	if (new_tile) {
		ClrBit(v->u.rail.flags, VRF_GOINGUP);
		ClrBit(v->u.rail.flags, VRF_GOINGDOWN);

		if (v->u.rail.track == TRACK_BIT_X || v->u.rail.track == TRACK_BIT_Y) {
			/* Any track that isn't TRACK_BIT_X or TRACK_BIT_Y cannot be sloped.
			 * To check whether the current tile is sloped, and in which
			 * direction it is sloped, we get the 'z' at the center of
			 * the tile (middle_z) and the edge of the tile (old_z),
			 * which we then can compare. */
			static const int HALF_TILE_SIZE = TILE_SIZE / 2;
			static const int INV_TILE_SIZE_MASK = ~(TILE_SIZE - 1);

			byte middle_z = GetSlopeZ((v->x_pos & INV_TILE_SIZE_MASK) | HALF_TILE_SIZE, (v->y_pos & INV_TILE_SIZE_MASK) | HALF_TILE_SIZE);

			/* For some reason tunnel tiles are always given as sloped :(
			 * But they are not sloped... */
			if (middle_z != v->z_pos && !IsTunnelTile(TileVirtXY(v->x_pos, v->y_pos))) {
				SetBit(v->u.rail.flags, (middle_z > old_z) ? VRF_GOINGUP : VRF_GOINGDOWN);
			}
		}
	}

	VehicleMove(v, true);
	return old_z;
}

static const Direction _new_vehicle_direction_table[11] = {
	DIR_N , DIR_NW, DIR_W , INVALID_DIR,
	DIR_NE, DIR_N , DIR_SW, INVALID_DIR,
	DIR_E , DIR_SE, DIR_S
};

static inline Direction GetNewVehicleDirectionByTile(TileIndex new_tile, TileIndex old_tile)
{
	uint offs = (TileY(new_tile) - TileY(old_tile) + 1) * 4 +
							TileX(new_tile) - TileX(old_tile) + 1;
	assert(offs < 11);
	return _new_vehicle_direction_table[offs];
}

static inline int GetDirectionToVehicle(const Vehicle *v, int x, int y)
{
	byte offs;

	x -= v->x_pos;
	if (x >= 0) {
		offs = (x > 2) ? 0 : 1;
	} else {
		offs = (x < -2) ? 2 : 1;
	}

	y -= v->y_pos;
	if (y >= 0) {
		offs += ((y > 2) ? 0 : 1) * 4;
	} else {
		offs += ((y < -2) ? 2 : 1) * 4;
	}

	assert(offs < 11);
	return _new_vehicle_direction_table[offs];
}

/* Check if the vehicle is compatible with the specified tile */
static inline bool CheckCompatibleRail(const Vehicle *v, TileIndex tile)
{
	return
		IsTileOwner(tile, v->owner) && (
			!IsFrontEngine(v) ||
			HasBit(v->u.rail.compatible_railtypes, GetRailType(tile))
		);
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

/** Modify the speed of the vehicle due to a turn */
static inline void AffectSpeedByDirChange(Vehicle *v, Direction new_dir)
{
	if (_settings_game.vehicle.train_acceleration_model != TAM_ORIGINAL) return;

	DirDiff diff = DirDifference(v->direction, new_dir);
	if (diff == DIRDIFF_SAME) return;

	const RailtypeSlowdownParams *rsp = &_railtype_slowdown[v->u.rail.railtype];
	v->cur_speed -= (diff == DIRDIFF_45RIGHT || diff == DIRDIFF_45LEFT ? rsp->small_turn : rsp->large_turn) * v->cur_speed >> 8;
}

/** Modify the speed of the vehicle due to a change in altitude */
static inline void AffectSpeedByZChange(Vehicle *v, byte old_z)
{
	if (old_z == v->z_pos || _settings_game.vehicle.train_acceleration_model != TAM_ORIGINAL) return;

	const RailtypeSlowdownParams *rsp = &_railtype_slowdown[v->u.rail.railtype];

	if (old_z < v->z_pos) {
		v->cur_speed -= (v->cur_speed * rsp->z_up >> 8);
	} else {
		uint16 spd = v->cur_speed + rsp->z_down;
		if (spd <= v->max_speed) v->cur_speed = spd;
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


static void SetVehicleCrashed(Vehicle *v)
{
	if (v->u.rail.crash_anim_pos != 0) return;

	/* Free a possible path reservation and try to mark all tiles occupied by the train reserved. */
	if (IsFrontEngine(v)) {
		/* Remove all reservations, also the ones currently under the train
		 * and any railway station paltform reservation. */
		FreeTrainTrackReservation(v);
		for (const Vehicle *u = v; u != NULL; u = u->Next()) {
			ClearPathReservation(u, u->tile, GetVehicleTrackdir(u));
			if (IsTileType(u->tile, MP_TUNNELBRIDGE)) {
				/* ClearPathReservation will not free the wormhole exit
				 * if the train has just entered the wormhole. */
				SetTunnelBridgeReservation(GetOtherTunnelBridgeEnd(u->tile), false);
			}
		}
	}

	/* we may need to update crossing we were approaching */
	TileIndex crossing = TrainApproachingCrossingTile(v);

	v->u.rail.crash_anim_pos++;

	InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
	InvalidateWindow(WC_VEHICLE_DETAILS, v->index);

	if (v->u.rail.track == TRACK_BIT_DEPOT) {
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
	}

	InvalidateWindowClassesData(WC_TRAINS_LIST, 0);

	for (; v != NULL; v = v->Next()) {
		v->vehstatus |= VS_CRASHED;
		MarkSingleVehicleDirty(v);
	}

	/* must be updated after the train has been marked crashed */
	if (crossing != INVALID_TILE) UpdateLevelCrossing(crossing);
}

static uint CountPassengersInTrain(const Vehicle *v)
{
	uint num = 0;

	for (; v != NULL; v = v->Next()) {
		if (IsCargoInClass(v->cargo_type, CC_PASSENGERS)) num += v->cargo.Count();
	}

	return num;
}

/**
 * Marks train as crashed and creates an AI event.
 * Doesn't do anything if the train is crashed already.
 * @param v first vehicle of chain
 * @return number of victims (including 2 drivers; zero if train was already crashed)
 */
static uint TrainCrashed(Vehicle *v)
{
	/* do not crash train twice */
	if (v->vehstatus & VS_CRASHED) return 0;

	/* two drivers + passengers */
	uint num = 2 + CountPassengersInTrain(v);

	SetVehicleCrashed(v);
	AI::NewEvent(v->owner, new AIEventVehicleCrashed(v->index, v->tile, AIEventVehicleCrashed::CRASH_TRAIN));

	return num;
}

struct TrainCollideChecker {
	Vehicle *v;  ///< vehicle we are testing for collision
	uint num;    ///< number of dead if train collided
};

static Vehicle *FindTrainCollideEnum(Vehicle *v, void *data)
{
	TrainCollideChecker *tcc = (TrainCollideChecker*)data;

	if (v->type != VEH_TRAIN) return NULL;

	/* get first vehicle now to make most usual checks faster */
	Vehicle *coll = v->First();

	/* can't collide with own wagons && can't crash in depot && the same height level */
	if (coll != tcc->v && v->u.rail.track != TRACK_BIT_DEPOT && abs(v->z_pos - tcc->v->z_pos) < 6) {
		int x_diff = v->x_pos - tcc->v->x_pos;
		int y_diff = v->y_pos - tcc->v->y_pos;

		/* needed to disable possible crash of competitor train in station by building diagonal track at its end */
		if (x_diff * x_diff + y_diff * y_diff > 25) return NULL;

		/* crash both trains */
		tcc->num += TrainCrashed(tcc->v);
		tcc->num += TrainCrashed(coll);

		/* Try to reserve all tiles directly under the crashed trains.
		 * As there might be more than two trains involved, we have to do that for all vehicles */
		const Vehicle *u;
		FOR_ALL_VEHICLES(u) {
			if (u->type == VEH_TRAIN && HASBITS(u->vehstatus, VS_CRASHED) && (u->u.rail.track & TRACK_BIT_DEPOT) == TRACK_BIT_NONE) {
				TrackBits trackbits = u->u.rail.track;
				if ((trackbits & TRACK_BIT_WORMHOLE) == TRACK_BIT_WORMHOLE) {
					/* Vehicle is inside a wormhole, v->u.rail.track contains no useful value then. */
					trackbits |= DiagDirToDiagTrackBits(GetTunnelBridgeDirection(u->tile));
				}
				TryReserveRailTrack(u->tile, TrackBitsToTrack(trackbits));
			}
		}
	}

	return NULL;
}

/**
 * Checks whether the specified train has a collision with another vehicle. If
 * so, destroys this vehicle, and the other vehicle if its subtype has TS_Front.
 * Reports the incident in a flashy news item, modifies station ratings and
 * plays a sound.
 */
static bool CheckTrainCollision(Vehicle *v)
{
	/* can't collide in depot */
	if (v->u.rail.track == TRACK_BIT_DEPOT) return false;

	assert(v->u.rail.track == TRACK_BIT_WORMHOLE || TileVirtXY(v->x_pos, v->y_pos) == v->tile);

	TrainCollideChecker tcc;
	tcc.v = v;
	tcc.num = 0;

	/* find colliding vehicles */
	if (v->u.rail.track == TRACK_BIT_WORMHOLE) {
		FindVehicleOnPos(v->tile, &tcc, FindTrainCollideEnum);
		FindVehicleOnPos(GetOtherTunnelBridgeEnd(v->tile), &tcc, FindTrainCollideEnum);
	} else {
		FindVehicleOnPosXY(v->x_pos, v->y_pos, &tcc, FindTrainCollideEnum);
	}

	/* any dead -> no crash */
	if (tcc.num == 0) return false;

	SetDParam(0, tcc.num);
	AddNewsItem(STR_8868_TRAIN_CRASH_DIE_IN_FIREBALL,
		NS_ACCIDENT_VEHICLE,
		v->index,
		0
	);

	ModifyStationRatingAround(v->tile, v->owner, -160, 30);
	SndPlayVehicleFx(SND_13_BIG_CRASH, v);
	return true;
}

static Vehicle *CheckVehicleAtSignal(Vehicle *v, void *data)
{
	DiagDirection exitdir = *(DiagDirection *)data;

	/* front engine of a train, not inside wormhole or depot, not crashed */
	if (v->type == VEH_TRAIN && IsFrontEngine(v) && (v->u.rail.track & TRACK_BIT_MASK) != 0 && !(v->vehstatus & VS_CRASHED)) {
		if (v->cur_speed <= 5 && TrainExitDir(v->direction, v->u.rail.track) == exitdir) return v;
	}

	return NULL;
}

static void TrainController(Vehicle *v, Vehicle *nomove)
{
	Vehicle *prev;

	/* For every vehicle after and including the given vehicle */
	for (prev = v->Previous(); v != nomove; prev = v, v = v->Next()) {
		DiagDirection enterdir = DIAGDIR_BEGIN;
		bool update_signals_crossing = false; // will we update signals or crossing state?

		GetNewVehiclePosResult gp = GetNewVehiclePos(v);
		if (v->u.rail.track != TRACK_BIT_WORMHOLE) {
			/* Not inside tunnel */
			if (gp.old_tile == gp.new_tile) {
				/* Staying in the old tile */
				if (v->u.rail.track == TRACK_BIT_DEPOT) {
					/* Inside depot */
					gp.x = v->x_pos;
					gp.y = v->y_pos;
				} else {
					/* Not inside depot */

					/* Reverse when we are at the end of the track already, do not move to the new position */
					if (IsFrontEngine(v) && !TrainCheckIfLineEnds(v)) return;

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
				Direction dir = GetNewVehicleDirectionByTile(gp.new_tile, gp.old_tile);
				enterdir = DirToDiagDir(dir);
				assert(IsValidDiagDirection(enterdir));

				/* Get the status of the tracks in the new tile and mask
				 * away the bits that aren't reachable. */
				TrackStatus ts = GetTileTrackStatus(gp.new_tile, TRANSPORT_RAIL, 0, ReverseDiagDir(enterdir));
				TrackdirBits reachable_trackdirs = DiagdirReachesTrackdirs(enterdir);

				TrackdirBits trackdirbits = TrackStatusToTrackdirBits(ts) & reachable_trackdirs;
				TrackBits red_signals = TrackdirBitsToTrackBits(TrackStatusToRedSignals(ts) & reachable_trackdirs);

				TrackBits bits = TrackdirBitsToTrackBits(trackdirbits);
				if (_settings_game.pf.pathfinder_for_trains != VPF_NTP && _settings_game.pf.forbid_90_deg && prev == NULL) {
					/* We allow wagons to make 90 deg turns, because forbid_90_deg
					 * can be switched on halfway a turn */
					bits &= ~TrackCrossesTracks(FindFirstTrack(v->u.rail.track));
				}

				if (bits == TRACK_BIT_NONE) goto invalid_rail;

				/* Check if the new tile contrains tracks that are compatible
				 * with the current train, if not, bail out. */
				if (!CheckCompatibleRail(v, gp.new_tile)) goto invalid_rail;

				TrackBits chosen_track;
				if (prev == NULL) {
					/* Currently the locomotive is active. Determine which one of the
					 * available tracks to choose */
					chosen_track = TrackToTrackBits(ChooseTrainTrack(v, gp.new_tile, enterdir, bits, false, NULL, true));
					assert(chosen_track & (bits | GetReservedTrackbits(gp.new_tile)));

					/* Check if it's a red signal and that force proceed is not clicked. */
					if (red_signals & chosen_track && v->u.rail.force_proceed == 0) {
						/* In front of a red signal */
						Trackdir i = FindFirstTrackdir(trackdirbits);

						/* Don't handle stuck trains here. */
						if (HasBit(v->u.rail.flags, VRF_TRAIN_STUCK)) return;

						if (!HasSignalOnTrackdir(gp.new_tile, ReverseTrackdir(i))) {
							v->cur_speed = 0;
							v->subspeed = 0;
							v->progress = 255 - 100;
							if (_settings_game.pf.wait_oneway_signal == 255 || ++v->load_unload_time_rem < _settings_game.pf.wait_oneway_signal * 20) return;
						} else if (HasSignalOnTrackdir(gp.new_tile, i)) {
							v->cur_speed = 0;
							v->subspeed = 0;
							v->progress = 255 - 10;
							if (_settings_game.pf.wait_twoway_signal == 255 || ++v->load_unload_time_rem < _settings_game.pf.wait_twoway_signal * 73) {
								DiagDirection exitdir = TrackdirToExitdir(i);
								TileIndex o_tile = TileAddByDiagDir(gp.new_tile, exitdir);

								exitdir = ReverseDiagDir(exitdir);

								/* check if a train is waiting on the other side */
								if (!HasVehicleOnPos(o_tile, &exitdir, &CheckVehicleAtSignal)) return;
							}
						}

						/* If we would reverse but are currently in a PBS block and
						 * reversing of stuck trains is disabled, don't reverse. */
						if (_settings_game.pf.wait_for_pbs_path == 255 && UpdateSignalsOnSegment(v->tile, enterdir, v->owner) == SIGSEG_PBS) {
							v->load_unload_time_rem = 0;
							return;
						}
						goto reverse_train_direction;
					} else {
						TryReserveRailTrack(gp.new_tile, TrackBitsToTrack(chosen_track));
					}
				} else {
					static const TrackBits _matching_tracks[8] = {
							TRACK_BIT_LEFT  | TRACK_BIT_RIGHT, TRACK_BIT_X,
							TRACK_BIT_UPPER | TRACK_BIT_LOWER, TRACK_BIT_Y,
							TRACK_BIT_LEFT  | TRACK_BIT_RIGHT, TRACK_BIT_X,
							TRACK_BIT_UPPER | TRACK_BIT_LOWER, TRACK_BIT_Y
					};

					/* The wagon is active, simply follow the prev vehicle. */
					chosen_track = (TrackBits)(byte)(_matching_tracks[GetDirectionToVehicle(prev, gp.x, gp.y)] & bits);
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
					if (IsFrontEngine(v) && HasPbsSignalOnTrackdir(gp.new_tile, tdir)) {
						SetSignalStateByTrackdir(gp.new_tile, tdir, SIGNAL_STATE_RED);
						MarkTileDirtyByTile(gp.new_tile);
					}

					/* Clear any track reservation when the last vehicle leaves the tile */
					if (v->Next() == NULL) ClearPathReservation(v, v->tile, GetVehicleTrackdir(v));

					v->tile = gp.new_tile;

					if (GetTileRailType(gp.new_tile) != GetTileRailType(gp.old_tile)) {
						TrainPowerChanged(v->First());
					}

					v->u.rail.track = chosen_track;
					assert(v->u.rail.track);
				}

				/* We need to update signal status, but after the vehicle position hash
				 * has been updated by AfterSetTrainPos() */
				update_signals_crossing = true;

				if (prev == NULL) AffectSpeedByDirChange(v, chosen_dir);

				v->direction = chosen_dir;

				if (IsFrontEngine(v)) {
					v->load_unload_time_rem = 0;

					/* If we are approching a crossing that is reserved, play the sound now. */
					TileIndex crossing = TrainApproachingCrossingTile(v);
					if (crossing != INVALID_TILE && GetCrossingReservation(crossing)) SndPlayTileFx(SND_0E_LEVEL_CROSSING, crossing);

					/* Always try to extend the reservation when entering a tile. */
					CheckNextTrainTile(v);
				}
			}
		} else {
			/* In a tunnel or on a bridge
			 * - for tunnels, only the part when the vehicle is not visible (part of enter/exit tile too)
			 * - for bridges, only the middle part - without the bridge heads */
			if (!(v->vehstatus & VS_HIDDEN)) {
				v->cur_speed =
					min(v->cur_speed, GetBridgeSpec(GetBridgeType(v->tile))->speed);
			}

			if (IsTileType(gp.new_tile, MP_TUNNELBRIDGE) && HasBit(VehicleEnterTile(v, gp.new_tile, gp.x, gp.y), VETS_ENTERED_WORMHOLE)) {
				/* Perform look-ahead on tunnel exit. */
				if (IsFrontEngine(v)) {
					TryReserveRailTrack(gp.new_tile, DiagDirToDiagTrack(GetTunnelBridgeDirection(gp.new_tile)));
					CheckNextTrainTile(v);
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
		byte old_z = AfterSetTrainPos(v, (gp.new_tile != gp.old_tile));

		if (prev == NULL) {
			/* This is the first vehicle in the train */
			AffectSpeedByZChange(v, old_z);
		}

		if (update_signals_crossing) {
			if (IsFrontEngine(v)) {
				if (TrainMovedChangeSignals(gp.new_tile, enterdir)) {
					/* We are entering a block with PBS signals right now, but
					 * not through a PBS signal. This means we don't have a
					 * reservation right now. As a conventional signal will only
					 * ever be green if no other train is in the block, getting
					 * a path should always be possible. If the player built
					 * such a strange network that it is not possible, the train
					 * will be marked as stuck and the player has to deal with
					 * the problem. */
					if ((!HasReservedTracks(gp.new_tile, v->u.rail.track) &&
							!TryReserveRailTrack(gp.new_tile, FindFirstTrack(v->u.rail.track))) ||
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
		if (IsFrontEngine(v) && v->tick_counter % _settings_game.pf.path_backoff_interval == 0) CheckNextTrainTile(v);
	}
	return;

invalid_rail:
	/* We've reached end of line?? */
	if (prev != NULL) error("Disconnecting train");

reverse_train_direction:
	v->load_unload_time_rem = 0;
	v->cur_speed = 0;
	v->subspeed = 0;
	ReverseTrainDirection(v);
}

/** Collect trackbits of all crashed train vehicles on a tile
 * @param v Vehicle passed from Find/HasVehicleOnPos()
 * @param data trackdirbits for the result
 * @return NULL to iterate over all vehicles on the tile.
 */
static Vehicle *CollectTrackbitsFromCrashedVehiclesEnum(Vehicle *v, void *data)
{
	TrackBits *trackbits = (TrackBits *)data;

	if (v->type == VEH_TRAIN && (v->vehstatus & VS_CRASHED) != 0) {
		if ((v->u.rail.track & TRACK_BIT_WORMHOLE) == TRACK_BIT_WORMHOLE) {
			/* Vehicle is inside a wormhole, v->u.rail.track contains no useful value then. */
			*trackbits |= DiagDirToDiagTrackBits(GetTunnelBridgeDirection(v->tile));
		} else {
			*trackbits |= v->u.rail.track;
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
static void DeleteLastWagon(Vehicle *v)
{
	Vehicle *first = v->First();

	/* Go to the last wagon and delete the link pointing there
	 * *u is then the one-before-last wagon, and *v the last
	 * one which will physicially be removed */
	Vehicle *u = v;
	for (; v->Next() != NULL; v = v->Next()) u = v;
	u->SetNext(NULL);

	if (first != v) {
		/* Recalculate cached train properties */
		TrainConsistChanged(first, false);
		/* Update the depot window if the first vehicle is in depot -
		 * if v == first, then it is updated in PreDestructor() */
		if (first->u.rail.track == TRACK_BIT_DEPOT) {
			InvalidateWindow(WC_VEHICLE_DEPOT, first->tile);
		}
	}

	/* 'v' shouldn't be accessed after it has been deleted */
	TrackBits trackbits = v->u.rail.track;
	TileIndex tile = v->tile;
	Owner owner = v->owner;

	delete v;
	v = NULL; // make sure nobody will try to read 'v' anymore

	if ((trackbits & TRACK_BIT_WORMHOLE) == TRACK_BIT_WORMHOLE) {
		/* Vehicle is inside a wormhole, v->u.rail.track contains no useful value then. */
		trackbits |= DiagDirToDiagTrackBits(GetTunnelBridgeDirection(tile));
	}

	Track track = TrackBitsToTrack(trackbits);
	if (HasReservedTracks(tile, trackbits)) {
		UnreserveRailTrack(tile, track);

		/* If there are still crashed vehicles on the tile, give the track reservation to them */
		TrackBits remaining_trackbits = TRACK_BIT_NONE;
		FindVehicleOnPos(tile, &remaining_trackbits, CollectTrackbitsFromCrashedVehiclesEnum);

		/* It is important that these two are the first in the loop, as reservation cannot deal with every trackbit combination */
		assert(TRACK_BEGIN == TRACK_X && TRACK_Y == TRACK_BEGIN + 1);
		for (Track t = TRACK_BEGIN; t < TRACK_END; t++) {
			if (HasBit(remaining_trackbits, t)) {
				TryReserveRailTrack(tile, t);
			}
		}
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

static void ChangeTrainDirRandomly(Vehicle *v)
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
			 * a bridge, because AfterSetTrainPos will put the vehicle under
			 * the bridge in that case */
			if (v->u.rail.track != TRACK_BIT_WORMHOLE) AfterSetTrainPos(v, false);
		}
	} while ((v = v->Next()) != NULL);
}

static void HandleCrashedTrain(Vehicle *v)
{
	int state = ++v->u.rail.crash_anim_pos;

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
		DeleteLastWagon(v);
		InvalidateWindow(WC_REPLACE_VEHICLE, (v->group_id << 16) | VEH_TRAIN);
	}
}

static void HandleBrokenTrain(Vehicle *v)
{
	if (v->breakdown_ctr != 1) {
		v->breakdown_ctr = 1;
		v->cur_speed = 0;

		if (v->breakdowns_since_last_service != 255)
			v->breakdowns_since_last_service++;

		InvalidateWindow(WC_VEHICLE_VIEW, v->index);
		InvalidateWindow(WC_VEHICLE_DETAILS, v->index);

		if (!PlayVehicleSound(v, VSE_BREAKDOWN)) {
			SndPlayVehicleFx((_settings_game.game_creation.landscape != LT_TOYLAND) ?
				SND_10_TRAIN_BREAKDOWN : SND_3A_COMEDY_BREAKDOWN_2, v);
		}

		if (!(v->vehstatus & VS_HIDDEN)) {
			Vehicle *u = CreateEffectVehicleRel(v, 4, 4, 5, EV_BREAKDOWN_SMOKE);
			if (u != NULL) u->u.effect.animation_state = v->breakdown_delay * 2;
		}
	}

	if (!(v->tick_counter & 3)) {
		if (!--v->breakdown_delay) {
			v->breakdown_ctr = 0;
			InvalidateWindow(WC_VEHICLE_VIEW, v->index);
		}
	}
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
static bool TrainApproachingLineEnd(Vehicle *v, bool signal)
{
	/* Calc position within the current tile */
	uint x = v->x_pos & 0xF;
	uint y = v->y_pos & 0xF;

	/* for diagonal directions, 'x' will be 0..15 -
	 * for other directions, it will be 1, 3, 5, ..., 15 */
	switch (v->direction) {
		case DIR_N : x = ~x + ~y + 25; break;
		case DIR_NW: x = y;            // FALLTHROUGH
		case DIR_NE: x = ~x + 16;      break;
		case DIR_E : x = ~x + y + 9;   break;
		case DIR_SE: x = y;            break;
		case DIR_S : x = x + y - 7;    break;
		case DIR_W : x = ~y + x + 9;   break;
		default: break;
	}

	/* do not reverse when approaching red signal */
	if (!signal && x + (v->u.rail.cached_veh_length + 1) / 2 >= TILE_SIZE) {
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
static bool TrainCanLeaveTile(const Vehicle *v)
{
	/* Exit if inside a tunnel/bridge or a depot */
	if (v->u.rail.track == TRACK_BIT_WORMHOLE || v->u.rail.track == TRACK_BIT_DEPOT) return false;

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
static TileIndex TrainApproachingCrossingTile(const Vehicle *v)
{
	assert(IsFrontEngine(v));
	assert(!(v->vehstatus & VS_CRASHED));

	if (!TrainCanLeaveTile(v)) return INVALID_TILE;

	DiagDirection dir = TrainExitDir(v->direction, v->u.rail.track);
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
static bool TrainCheckIfLineEnds(Vehicle *v)
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
	DiagDirection dir = TrainExitDir(v->direction, v->u.rail.track);
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
	if (_settings_game.pf.pathfinder_for_trains != VPF_NTP && _settings_game.pf.forbid_90_deg) {
		bits &= ~TrackCrossesTracks(FindFirstTrack(v->u.rail.track));
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


static void TrainLocoHandler(Vehicle *v, bool mode)
{
	/* train has crashed? */
	if (v->vehstatus & VS_CRASHED) {
		if (!mode) HandleCrashedTrain(v);
		return;
	}

	if (v->u.rail.force_proceed != 0) {
		v->u.rail.force_proceed--;
		ClrBit(v->u.rail.flags, VRF_TRAIN_STUCK);
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
	}

	/* train is broken down? */
	if (v->breakdown_ctr != 0) {
		if (v->breakdown_ctr <= 2) {
			HandleBrokenTrain(v);
			return;
		}
		if (!v->current_order.IsType(OT_LOADING)) v->breakdown_ctr--;
	}

	if (HasBit(v->u.rail.flags, VRF_REVERSING) && v->cur_speed == 0) {
		ReverseTrainDirection(v);
	}

	/* exit if train is stopped */
	if (v->vehstatus & VS_STOPPED && v->cur_speed == 0) return;

	bool valid_order = v->current_order.IsValid() && v->current_order.GetType() != OT_CONDITIONAL;
	if (ProcessOrders(v) && CheckReverseTrain(v)) {
		v->load_unload_time_rem = 0;
		v->cur_speed = 0;
		v->subspeed = 0;
		ReverseTrainDirection(v);
		return;
	}

	v->HandleLoading(mode);

	if (v->current_order.IsType(OT_LOADING)) return;

	if (CheckTrainStayInDepot(v)) return;

	if (!mode) HandleLocomotiveSmokeCloud(v);

	/* We had no order but have an order now, do look ahead. */
	if (!valid_order && v->current_order.IsValid()) {
		CheckNextTrainTile(v);
	}

	/* Handle stuck trains. */
	if (!mode && HasBit(v->u.rail.flags, VRF_TRAIN_STUCK)) {
		++v->load_unload_time_rem;

		/* Should we try reversing this tick if still stuck? */
		bool turn_around = v->load_unload_time_rem % (_settings_game.pf.wait_for_pbs_path * DAY_TICKS) == 0 && _settings_game.pf.wait_for_pbs_path < 255;

		if (!turn_around && v->load_unload_time_rem % _settings_game.pf.path_backoff_interval != 0 && v->u.rail.force_proceed == 0) return;
		if (!TryPathReserve(v)) {
			/* Still stuck. */
			if (turn_around) ReverseTrainDirection(v);

			if (HasBit(v->u.rail.flags, VRF_TRAIN_STUCK) && v->load_unload_time_rem > 2 * _settings_game.pf.wait_for_pbs_path * DAY_TICKS) {
				/* Show message to player. */
				if (_settings_client.gui.lost_train_warn && v->owner == _local_company) {
					SetDParam(0, v->index);
					AddNewsItem(
						STR_TRAIN_IS_STUCK,
						NS_ADVICE,
						v->index,
						0);
				}
				v->load_unload_time_rem = 0;
			}
			/* Exit if force proceed not pressed, else reset stuck flag anyway. */
			if (v->u.rail.force_proceed == 0) return;
			ClrBit(v->u.rail.flags, VRF_TRAIN_STUCK);
			v->load_unload_time_rem = 0;
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
		}
	}

	if (v->current_order.IsType(OT_LEAVESTATION)) {
		v->current_order.Free();
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
		return;
	}

	int j = UpdateTrainSpeed(v);

	/* we need to invalidate the widget if we are stopping from 'Stopping 0 km/h' to 'Stopped' */
	if (v->cur_speed == 0 && v->u.rail.last_speed == 0 && v->vehstatus & VS_STOPPED) {
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
	}

	int adv_spd = (v->direction & 1) ? 192 : 256;
	if (j < adv_spd) {
		/* if the vehicle has speed 0, update the last_speed field. */
		if (v->cur_speed == 0) SetLastSpeed(v, v->cur_speed);
	} else {
		TrainCheckIfLineEnds(v);
		/* Loop until the train has finished moving. */
		do {
			j -= adv_spd;
			TrainController(v, NULL);
			/* Don't continue to move if the train crashed. */
			if (CheckTrainCollision(v)) break;
			/* 192 spd used for going straight, 256 for going diagonally. */
			adv_spd = (v->direction & 1) ? 192 : 256;
		} while (j >= adv_spd);
		SetLastSpeed(v, v->cur_speed);
	}

	for (Vehicle *u = v; u != NULL; u = u->Next()) {
		if ((u->vehstatus & VS_HIDDEN) != 0) continue;

		uint16 old_image = u->cur_image;
		u->cur_image = u->GetImage(u->direction);
		if (old_image != u->cur_image) VehicleMove(u, true);
	}

	if (v->progress == 0) v->progress = j; // Save unused spd for next time, if TrainController didn't set progress
}



Money Train::GetRunningCost() const
{
	Money cost = 0;
	const Vehicle *v = this;

	do {
		const RailVehicleInfo *rvi = RailVehInfo(v->engine_type);

		byte cost_factor = GetVehicleProperty(v, 0x0D, rvi->running_cost);
		if (cost_factor == 0) continue;

		/* Halve running cost for multiheaded parts */
		if (IsMultiheaded(v)) cost_factor /= 2;

		cost += cost_factor * GetPriceByIndex(rvi->running_cost_class);
	} while ((v = GetNextVehicle(v)) != NULL);

	return cost;
}


void Train::Tick()
{
	if (_age_cargo_skip_counter == 0) this->cargo.AgeCargo();

	this->tick_counter++;

	if (IsFrontEngine(this)) {
		if (!(this->vehstatus & VS_STOPPED)) this->running_ticks++;
		this->current_order_time++;

		TrainLocoHandler(this, false);

		/* make sure vehicle wasn't deleted. */
		if (this->type == VEH_TRAIN && IsFrontEngine(this))
			TrainLocoHandler(this, true);
	} else if (IsFreeWagon(this) && HASBITS(this->vehstatus, VS_CRASHED)) {
		/* Delete flooded standalone wagon chain */
		if (++this->u.rail.crash_anim_pos >= 4400) delete this;
	}
}

static void CheckIfTrainNeedsService(Vehicle *v)
{
	static const uint MAX_ACCEPTABLE_DEPOT_DIST = 16;

	if (_settings_game.vehicle.servint_trains == 0 || !v->NeedsAutomaticServicing()) return;
	if (v->IsInDepot()) {
		VehicleServiceInDepot(v);
		return;
	}

	TrainFindDepotData tfdd = FindClosestTrainDepot(v, MAX_ACCEPTABLE_DEPOT_DIST);
	/* Only go to the depot if it is not too far out of our way. */
	if (tfdd.best_length == UINT_MAX || tfdd.best_length > MAX_ACCEPTABLE_DEPOT_DIST) {
		if (v->current_order.IsType(OT_GOTO_DEPOT)) {
			/* If we were already heading for a depot but it has
			 * suddenly moved farther away, we continue our normal
			 * schedule? */
			v->current_order.MakeDummy();
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
		}
		return;
	}

	const Depot *depot = GetDepotByTile(tfdd.tile);

	if (v->current_order.IsType(OT_GOTO_DEPOT) &&
			v->current_order.GetDestination() != depot->index &&
			!Chance16(3, 16)) {
		return;
	}

	v->current_order.MakeGoToDepot(depot->index, ODTFB_SERVICE);
	v->dest_tile = tfdd.tile;
	InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
}

void Train::OnNewDay()
{
	if ((++this->day_counter & 7) == 0) DecreaseVehicleValue(this);

	if (IsFrontEngine(this)) {
		CheckVehicleBreakdown(this);
		AgeVehicle(this);

		CheckIfTrainNeedsService(this);

		CheckOrders(this);

		/* update destination */
		if (this->current_order.IsType(OT_GOTO_STATION)) {
			TileIndex tile = GetStation(this->current_order.GetDestination())->train_tile;
			if (tile != INVALID_TILE) this->dest_tile = tile;
		}

		if (this->running_ticks != 0) {
			/* running costs */
			CommandCost cost(EXPENSES_TRAIN_RUN, this->GetRunningCost() * this->running_ticks / (DAYS_IN_YEAR  * DAY_TICKS));

			this->profit_this_year -= cost.GetCost();
			this->running_ticks = 0;

			SubtractMoneyFromCompanyFract(this->owner, cost);

			InvalidateWindow(WC_VEHICLE_DETAILS, this->index);
			InvalidateWindowClasses(WC_TRAINS_LIST);
		}
	} else if (IsTrainEngine(this)) {
		/* Also age engines that aren't front engines */
		AgeVehicle(this);
	}
}

void InitializeTrains()
{
	_age_cargo_skip_counter = 1;
}
