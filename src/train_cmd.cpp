/* $Id$ */

/** @file train_cmd.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "bridge_map.h"
#include "debug.h"
#include "tile_cmd.h"
#include "landscape.h"
#include "gui.h"
#include "station_map.h"
#include "tunnel_map.h"
#include "timetable.h"
#include "articulated_vehicles.h"
#include "command_func.h"
#include "pathfind.h"
#include "npf.h"
#include "station.h"
#include "news.h"
#include "engine.h"
#include "player_func.h"
#include "player_base.h"
#include "depot.h"
#include "waypoint.h"
#include "vehicle_gui.h"
#include "train.h"
#include "bridge.h"
#include "newgrf_callbacks.h"
#include "newgrf_engine.h"
#include "newgrf_sound.h"
#include "newgrf_text.h"
#include "direction_func.h"
#include "yapf/yapf.h"
#include "cargotype.h"
#include "group.h"
#include "table/sprites.h"
#include "tunnelbridge_map.h"
#include "strings_func.h"
#include "functions.h"
#include "window_func.h"
#include "date_func.h"
#include "vehicle_func.h"
#include "sound_func.h"
#include "signal_func.h"
#include "variables.h"
#include "autoreplace_gui.h"
#include "gfx_func.h"
#include "settings_type.h"

#include "table/strings.h"
#include "table/train_cmd.h"

static bool TrainCheckIfLineEnds(Vehicle *v);
static void TrainController(Vehicle *v, bool update_image);
static TileIndex TrainApproachingCrossingTile(const Vehicle *v);

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
	return _patches.freight_trains;
}


/**
 * Recalculates the cached total power of a train. Should be called when the consist is changed
 * @param v First vehicle of the consist.
 */
void TrainPowerChanged(Vehicle* v)
{
	uint32 total_power = 0;
	uint32 max_te = 0;

	for (const Vehicle *u = v; u != NULL; u = u->Next()) {
		/* Power is not added for articulated parts */
		if (IsArticulatedPart(u)) continue;

		RailType railtype = GetRailType(u->tile);
		bool engine_has_power = HasPowerOnRail(u->u.rail.railtype, railtype);
		bool wagon_has_power  = HasPowerOnRail(v->u.rail.railtype, railtype);

		const RailVehicleInfo *rvi_u = RailVehInfo(u->engine_type);

		if (engine_has_power) {
			uint16 power = GetVehicleProperty(u, 0x0B, rvi_u->power);
			if (power != 0) {
				total_power += power;
				/* Tractive effort in (tonnes * 1000 * 10 =) N */
				max_te += (u->u.rail.cached_veh_weight * 10000 * GetVehicleProperty(u, 0x1F, rvi_u->tractive_effort)) / 256;
			}
		}

		if (HasBit(u->u.rail.flags, VRF_POWEREDWAGON) && (wagon_has_power)) {
			total_power += RailVehInfo(u->u.rail.first_engine)->pow_wag_power;
		}
	}

	if (v->u.rail.cached_power != total_power || v->u.rail.cached_max_te != max_te) {
		/* If it has no power (no catenary), stop the train */
		if (total_power == 0) v->vehstatus |= VS_STOPPED;

		v->u.rail.cached_power = total_power;
		v->u.rail.cached_max_te = max_te;
		InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
	}
}


/**
 * Recalculates the cached weight of a train and its vehicles. Should be called each time the cargo on
 * the consist changes.
 * @param v First vehicle of the consist.
 */
static void TrainCargoChanged(Vehicle* v)
{
	uint32 weight = 0;

	for (Vehicle *u = v; u != NULL; u = u->Next()) {
		uint32 vweight = GetCargo(u->cargo_type)->weight * u->cargo.Count() * FreightWagonMult(u->cargo_type) / 16;

		/* Vehicle weight is not added for articulated parts. */
		if (!IsArticulatedPart(u)) {
			/* vehicle weight is the sum of the weight of the vehicle and the weight of its cargo */
			vweight += GetVehicleProperty(u, 0x16, RailVehInfo(u->engine_type)->weight);

			/* powered wagons have extra weight added */
			if (HasBit(u->u.rail.flags, VRF_POWEREDWAGON))
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


/**
 * Recalculates the cached stuff of a train. Should be called each time a vehicle is added
 * to/removed from the chain, and when the game is loaded.
 * Note: this needs to be called too for 'wagon chains' (in the depot, without an engine)
 * @param v First vehicle of the chain.
 */
void TrainConsistChanged(Vehicle* v)
{
	uint16 max_speed = 0xFFFF;

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

		if (!HasBit(EngInfo(u->engine_type)->misc_flags, EF_RAIL_TILTS)) train_can_tilt = false;

		/* update the 'first engine' */
		u->u.rail.first_engine = v == u ? INVALID_ENGINE : first_engine;
		u->u.rail.railtype = rvi_u->railtype;

		if (IsTrainEngine(u)) first_engine = u->engine_type;

		/* Cache wagon override sprite group. NULL is returned if there is none */
		u->u.rail.cached_override = GetWagonOverrideSpriteSet(u->engine_type, u->cargo_type, u->u.rail.first_engine);

		/* Reset color map */
		u->colormap = PAL_NONE;

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

		if (!IsArticulatedPart(u)) {
			/* Check powered wagon / visual effect callback */
			if (HasBit(EngInfo(u->engine_type)->callbackmask, CBM_TRAIN_WAGON_POWER)) {
				uint16 callback = GetVehicleCallback(CBID_TRAIN_WAGON_POWER, 0, 0, u->engine_type, u);

				if (callback != CALLBACK_FAILED) u->u.rail.cached_vis_effect = callback;
			}

			if (rvi_v->pow_wag_power != 0 && rvi_u->railveh_type == RAILVEH_WAGON &&
				UsesWagonOverride(u) && !HasBit(u->u.rail.cached_vis_effect, 7)) {
				/* wagon is powered */
				SetBit(u->u.rail.flags, VRF_POWEREDWAGON); // cache 'powered' status
			} else {
				ClrBit(u->u.rail.flags, VRF_POWEREDWAGON);
			}

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
			if ((rvi_u->railveh_type != RAILVEH_WAGON || _patches.wagon_speed_limits) && !UsesWagonOverride(u)) {
				uint16 speed = GetVehicleProperty(u, 0x09, rvi_u->max_speed);
				if (speed != 0) max_speed = min(speed, max_speed);
			}
		}

		if (u->cargo_type == rvi_u->cargo_type && u->cargo_subtype == 0) {
			/* Set cargo capacity if we've not been refitted */
			u->cargo_cap = GetVehicleProperty(u, 0x14, rvi_u->capacity);
		}

		u->u.rail.user_def_data = GetVehicleProperty(u, 0x25, rvi_u->user_def_data);

		/* check the vehicle length (callback) */
		uint16 veh_len = CALLBACK_FAILED;
		if (HasBit(EngInfo(u->engine_type)->callbackmask, CBM_VEHICLE_LENGTH)) {
			veh_len = GetVehicleCallback(CBID_VEHICLE_LENGTH, 0, 0, u->engine_type, u);
		}
		if (veh_len == CALLBACK_FAILED) veh_len = rvi_u->shorten_factor;
		veh_len = Clamp(veh_len, 0, u->Next() == NULL ? 7 : 5); // the clamp on vehicles not the last in chain is stricter, as too short wagons can break the 'follow next vehicle' code
		u->u.rail.cached_veh_length = 8 - veh_len;
		v->u.rail.cached_total_length += u->u.rail.cached_veh_length;
	}

	/* store consist weight/max speed in cache */
	v->u.rail.cached_max_speed = max_speed;
	v->u.rail.cached_tilt = train_can_tilt;

	/* recalculate cached weights and power too (we do this *after* the rest, so it is known which wagons are powered and need extra weight added) */
	TrainCargoChanged(v);
}

enum AccelType {
	AM_ACCEL,
	AM_BRAKE
};

static bool TrainShouldStop(const Vehicle* v, TileIndex tile)
{
	const Order* o = &v->current_order;
	StationID sid = GetStationIndex(tile);

	assert(v->type == VEH_TRAIN);
	/* When does a train drive through a station
	 * first we deal with the "new nonstop handling" */
	if (_patches.new_nonstop && o->flags & OFB_NON_STOP && sid == o->dest) {
		return false;
	}

	if (v->last_station_visited == sid) return false;

	if (sid != o->dest && (o->flags & OFB_NON_STOP || _patches.new_nonstop)) {
		return false;
	}

	return true;
}

/** new acceleration*/
static int GetTrainAcceleration(Vehicle *v, bool mode)
{
	static const int absolute_max_speed = 2000;
	int max_speed = absolute_max_speed;
	int speed = v->cur_speed * 10 / 16; // km-ish/h -> mp/h
	int curvecount[2] = {0, 0};

	/*first find the curve speed limit */
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

		/*if we have a 90 degree turn, fix the speed limit to 60 */
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
		if (TrainShouldStop(v, v->tile)) {
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

	int num = 0; //number of vehicles, change this into the number of axles later
	int incl = 0;
	int drag_coeff = 20; //[1e-4]
	for (const Vehicle *u = v; u != NULL; u = u->Next()) {
		num++;
		drag_coeff += 3;

		if (u->u.rail.track == TRACK_BIT_DEPOT) max_speed = min(max_speed, 61);

		if (HasBit(u->u.rail.flags, VRF_GOINGUP)) {
			incl += u->u.rail.cached_veh_weight * 60; //3% slope, quite a bit actually
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

	/* Due to the mph to m/s conversion below, at speeds below 3 mph the force is
	 * actually double the train's power */
	const int max_te = v->u.rail.cached_max_te; // [N]
	int force;
	if (speed > 2) {
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

	if (force <= 0) force = 10000;

	if (v->u.rail.railtype != RAILTYPE_MAGLEV) force = min(force, mass * 10 * 200);

	if (mode == AM_ACCEL) {
		return (force - resistance) / (mass * 4);
	} else {
		return min((-force - resistance) / (mass * 4), -10000 / (mass * 4));
	}
}

static void UpdateTrainAcceleration(Vehicle* v)
{
	assert(IsFrontEngine(v));

	v->max_speed = v->u.rail.cached_max_speed;

	uint power = v->u.rail.cached_power;
	uint weight = v->u.rail.cached_weight;
	assert(weight != 0);
	v->acceleration = Clamp(power / weight * 4, 1, 255);
}

int Train::GetImage(Direction direction) const
{
	int img = this->spritenum;
	int base;

	if (HasBit(this->u.rail.flags, VRF_REVERSE_DIRECTION)) direction = ReverseDir(direction);

	if (is_custom_sprite(img)) {
		base = GetCustomVehicleSprite(this, (Direction)(direction + 4 * IS_CUSTOM_SECONDHEAD_SPRITE(img)));
		if (base != 0) return base;
		img = _orig_rail_vehicle_info[this->engine_type].image_index;
	}

	base = _engine_sprite_base[img] + ((direction + _engine_sprite_add[img]) & _engine_sprite_and[img]);

	if (this->cargo.Count() >= this->cargo_cap / 2U) base += _wagon_full_adder[img];
	return base;
}

void DrawTrainEngine(int x, int y, EngineID engine, SpriteID pal)
{
	const RailVehicleInfo *rvi = RailVehInfo(engine);

	int img = rvi->image_index;
	SpriteID image = 0;

	if (is_custom_sprite(img)) {
		image = GetCustomVehicleIcon(engine, DIR_W);
		if (image == 0) {
			img = _orig_rail_vehicle_info[engine].image_index;
		} else {
			y += _traininfo_vehicle_pitch;
		}
	}
	if (image == 0) {
		image = (6 & _engine_sprite_and[img]) + _engine_sprite_base[img];
	}

	if (rvi->railveh_type == RAILVEH_MULTIHEAD) {
		DrawSprite(image, pal, x - 14, y);
		x += 15;
		image = 0;
		if (is_custom_sprite(img)) {
			image = GetCustomVehicleIcon(engine, DIR_E);
			if (image == 0) img = _orig_rail_vehicle_info[engine].image_index;
		}
		if (image == 0) {
			image =
				((6 + _engine_sprite_add[img + 1]) & _engine_sprite_and[img + 1]) +
				_engine_sprite_base[img + 1];
		}
	}
	DrawSprite(image, pal, x, y);
}

static CommandCost CmdBuildRailWagon(EngineID engine, TileIndex tile, uint32 flags)
{
	const RailVehicleInfo *rvi = RailVehInfo(engine);
	CommandCost value(EXPENSES_NEW_VEHICLES, (GetEngineProperty(engine, 0x17, rvi->base_cost) * _price.build_railwagon) >> 8);

	uint num_vehicles = 1 + CountArticulatedParts(engine, false);

	if (!(flags & DC_QUERY_COST)) {
		/* Allow for the wagon and the articulated parts, plus one to "terminate" the list. */
		Vehicle **vl = (Vehicle**)alloca(sizeof(*vl) * (num_vehicles + 1));
		memset(vl, 0, sizeof(*vl) * (num_vehicles + 1));

		if (!Vehicle::AllocateList(vl, num_vehicles))
			return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);

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
			v->owner = _current_player;
			v->u.rail.track = TRACK_BIT_DEPOT;
			v->vehstatus = VS_HIDDEN | VS_DEFPAL;

			v->subtype = 0;
			SetTrainWagon(v);

			if (u != NULL) {
				u->SetNext(v);
			} else {
				SetFreeWagon(v);
				InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
			}

			v->cargo_type = rvi->cargo_type;
			v->cargo_subtype = 0;
			v->cargo_cap = rvi->capacity;
			v->value = value.GetCost();
//			v->day_counter = 0;

			v->u.rail.railtype = rvi->railtype;

			v->build_year = _cur_year;
			v->cur_image = 0xAC2;
			v->random_bits = VehicleRandomBits();

			v->group_id = DEFAULT_GROUP;

			AddArticulatedParts(vl, VEH_TRAIN);

			_new_vehicle_id = v->index;

			VehiclePositionChanged(v);
			TrainConsistChanged(v->First());
			UpdateTrainGroupID(v->First());

			InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
			if (IsLocalPlayer()) {
				InvalidateAutoreplaceWindow(v->engine_type, v->group_id); // updates the replace Train window
			}
			GetPlayer(_current_player)->num_engines[engine]++;
		}
	}

	return value;
}

/** Move all free vehicles in the depot to the train */
static void NormalizeTrainVehInDepot(const Vehicle* u)
{
	const Vehicle* v;

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

static CommandCost EstimateTrainCost(EngineID engine, const RailVehicleInfo* rvi)
{
	return CommandCost(EXPENSES_NEW_VEHICLES, GetEngineProperty(engine, 0x17, rvi->base_cost) * (_price.build_railvehicle >> 3) >> 5);
}

static void AddRearEngineToMultiheadedTrain(Vehicle* v, Vehicle* u, bool building)
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
	u->subtype = 0;
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
	VehiclePositionChanged(u);
}

/** Build a railroad vehicle.
 * @param tile tile of the depot where rail-vehicle is built
 * @param flags type of operation
 * @param p1 engine type id
 * @param p2 bit 0 when set, the train will get number 0, otherwise it will get a free number
 *           bit 1 prevents any free cars from being added to the train
 */
CommandCost CmdBuildRailVehicle(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	/* Check if the engine-type is valid (for the player) */
	if (!IsEngineBuildable(p1, VEH_TRAIN, _current_player)) return_cmd_error(STR_RAIL_VEHICLE_NOT_AVAILABLE);

	/* Check if the train is actually being built in a depot belonging
	 * to the player. Doesn't matter if only the cost is queried */
	if (!(flags & DC_QUERY_COST)) {
		if (!IsTileDepotType(tile, TRANSPORT_RAIL)) return CMD_ERROR;
		if (!IsTileOwner(tile, _current_player)) return CMD_ERROR;
	}

	const RailVehicleInfo *rvi = RailVehInfo(p1);

	/* Check if depot and new engine uses the same kind of tracks */
	/* We need to see if the engine got power on the tile to avoid eletric engines in non-electric depots */
	if (!HasPowerOnRail(rvi->railtype, GetRailType(tile))) return CMD_ERROR;

	if (rvi->railveh_type == RAILVEH_WAGON) return CmdBuildRailWagon(p1, tile, flags);

	CommandCost value = EstimateTrainCost(p1, rvi);

	uint num_vehicles =
		(rvi->railveh_type == RAILVEH_MULTIHEAD ? 2 : 1) +
		CountArticulatedParts(p1, false);

	if (!(flags & DC_QUERY_COST)) {
		/* Allow for the dual-heads and the articulated parts, plus one to "terminate" the list. */
		Vehicle **vl = (Vehicle**)alloca(sizeof(*vl) * (num_vehicles + 1));
		memset(vl, 0, sizeof(*vl) * (num_vehicles + 1));

		if (!Vehicle::AllocateList(vl, num_vehicles))
			return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);

		Vehicle *v = vl[0];

		UnitID unit_num = HasBit(p2, 0) ? 0 : GetFreeUnitNumber(VEH_TRAIN);
		if (unit_num > _patches.max_trains)
			return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);

		if (flags & DC_EXEC) {
			DiagDirection dir = GetRailDepotDirection(tile);
			int x = TileX(tile) * TILE_SIZE + _vehicle_initial_x_fract[dir];
			int y = TileY(tile) * TILE_SIZE + _vehicle_initial_y_fract[dir];

			v = new (v) Train();
			v->unitnumber = unit_num;
			v->direction = DiagDirToDir(dir);
			v->tile = tile;
			v->owner = _current_player;
			v->x_pos = x;
			v->y_pos = y;
			v->z_pos = GetSlopeZ(x, y);
			v->u.rail.track = TRACK_BIT_DEPOT;
			v->vehstatus = VS_HIDDEN | VS_STOPPED | VS_DEFPAL;
			v->spritenum = rvi->image_index;
			v->cargo_type = rvi->cargo_type;
			v->cargo_subtype = 0;
			v->cargo_cap = rvi->capacity;
			v->max_speed = rvi->max_speed;
			v->value = value.GetCost();
			v->last_station_visited = INVALID_STATION;
			v->dest_tile = 0;

			v->engine_type = p1;

			const Engine *e = GetEngine(p1);
			v->reliability = e->reliability;
			v->reliability_spd_dec = e->reliability_spd_dec;
			v->max_age = e->lifelength * 366;

			v->name = NULL;
			v->u.rail.railtype = rvi->railtype;
			_new_vehicle_id = v->index;

			v->service_interval = _patches.servint_trains;
			v->date_of_last_service = _date;
			v->build_year = _cur_year;
			v->cur_image = 0xAC2;
			v->random_bits = VehicleRandomBits();

			v->vehicle_flags = 0;
			if (e->flags & ENGINE_EXCLUSIVE_PREVIEW) SetBit(v->vehicle_flags, VF_BUILT_AS_PROTOTYPE);

			v->group_id = DEFAULT_GROUP;

			v->subtype = 0;
			SetFrontEngine(v);
			SetTrainEngine(v);

			VehiclePositionChanged(v);

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

			TrainConsistChanged(v);
			UpdateTrainAcceleration(v);
			UpdateTrainGroupID(v);

			if (!HasBit(p2, 1)) { // check if the cars should be added to the new vehicle
				NormalizeTrainVehInDepot(v);
			}

			InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
			RebuildVehicleLists();
			InvalidateWindow(WC_COMPANY, v->owner);
			if (IsLocalPlayer())
				InvalidateAutoreplaceWindow(v->engine_type, v->group_id); // updates the replace Train window

			GetPlayer(_current_player)->num_engines[p1]++;
		}
	}

	return value;
}


/* Check if all the wagons of the given train are in a depot, returns the
 * number of cars (including loco) then. If not it returns -1 */
int CheckTrainInDepot(const Vehicle *v, bool needs_to_be_stopped)
{
	TileIndex tile = v->tile;

	/* check if stopped in a depot */
	if (!IsTileDepotType(tile, TRANSPORT_RAIL) || v->cur_speed != 0) return -1;

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
 * @param p1 various bitstuffed elements
 * - p1 (bit  0 - 15) source vehicle index
 * - p1 (bit 16 - 31) what wagon to put the source wagon AFTER, XXX - INVALID_VEHICLE to make a new line
 * @param p2 (bit 0) move all vehicles following the source vehicle
 */
CommandCost CmdMoveRailVehicle(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
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

	{
		int max_len = _patches.mammoth_trains ? 100 : 9;

		/* check if all vehicles in the source train are stopped inside a depot. */
		int src_len = CheckTrainStoppedInDepot(src_head);
		if (src_len < 0) return_cmd_error(STR_881A_TRAINS_CAN_ONLY_BE_ALTERED);

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
		UnitID unit_num = GetFreeUnitNumber(VEH_TRAIN);
		if (unit_num > _patches.max_trains)
			return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);

		if (flags & DC_EXEC) src->unitnumber = unit_num;
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
	if (dst_head != NULL && !src_in_dst) {
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
					assert(src->orders == NULL);
					src->num_orders = 0;

					// Decrease the engines number of the src engine_type
					if (!IsDefaultGroupID(src->group_id) && IsValidGroupID(src->group_id)) {
						GetGroup(src->group_id)->num_engines[src->engine_type]--;
					}

					// If we move an engine to a new line affect it to the DEFAULT_GROUP
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

				for (v = src; GetNextVehicle(v) != NULL; v = GetNextVehicle(v));
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
			CmdMoveRailVehicle(0, flags, src_head->index | (INVALID_VEHICLE << 16), 1);
			SetTrainGroupID(src_head, tmp_g);
			src_head = NULL; // don't do anything more to this train since the new call will do it
		}

		if (src_head != NULL) {
			NormaliseTrainConsist(src_head);
			TrainConsistChanged(src_head);
			UpdateTrainGroupID(src_head);
			if (IsFrontEngine(src_head)) {
				UpdateTrainAcceleration(src_head);
				InvalidateWindow(WC_VEHICLE_DETAILS, src_head->index);
				/* Update the refit button and window */
				InvalidateWindow(WC_VEHICLE_REFIT, src_head->index);
				InvalidateWindowWidget(WC_VEHICLE_VIEW, src_head->index, 12);
			}
			/* Update the depot window */
			InvalidateWindow(WC_VEHICLE_DEPOT, src_head->tile);
		}

		if (dst_head != NULL) {
			NormaliseTrainConsist(dst_head);
			TrainConsistChanged(dst_head);
			UpdateTrainGroupID(dst_head);
			if (IsFrontEngine(dst_head)) {
				UpdateTrainAcceleration(dst_head);
				InvalidateWindow(WC_VEHICLE_DETAILS, dst_head->index);
				/* Update the refit button and window */
				InvalidateWindowWidget(WC_VEHICLE_VIEW, dst_head->index, 12);
				InvalidateWindow(WC_VEHICLE_REFIT, dst_head->index);
			}
			/* Update the depot window */
			InvalidateWindow(WC_VEHICLE_DEPOT, dst_head->tile);
		}

		RebuildVehicleLists();
	}

	return CommandCost();
}

/** Start/Stop a train.
 * @param tile unused
 * @param flags type of operation
 * @param p1 train to start/stop
 * @param p2 unused
 */
CommandCost CmdStartStopTrain(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	Vehicle *v = GetVehicle(p1);

	if (v->type != VEH_TRAIN || !CheckOwnership(v->owner)) return CMD_ERROR;

	/* Check if this train can be started/stopped. The callback will fail or
	 * return 0xFF if it can. */
	uint16 callback = GetVehicleCallback(CBID_VEHICLE_START_STOP_CHECK, 0, 0, v->engine_type, v);
	if (callback != CALLBACK_FAILED && callback != 0xFF) {
		StringID error = GetGRFStringID(GetEngineGRFID(v->engine_type), 0xD000 + callback);
		return_cmd_error(error);
	}

	if (v->vehstatus & VS_STOPPED && v->u.rail.cached_power == 0) return_cmd_error(STR_TRAIN_START_NO_CATENARY);

	if (flags & DC_EXEC) {
		if (v->vehstatus & VS_STOPPED && v->u.rail.track == TRACK_BIT_DEPOT) {
			DeleteVehicleNews(p1, STR_8814_TRAIN_IS_WAITING_IN_DEPOT);
		}

		v->vehstatus ^= VS_STOPPED;
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
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
             if the wagon is dragged, don't delete the possibly belonging rear-engine to some front
 * - p2 = 2: when selling attached locos, rearrange all vehicles after it to separate lines;
 *           all wagons of the same type will go on the same line. Used by the AI currently
 */
CommandCost CmdSellRailWagon(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	/* Check if we deleted a vehicle window */
	Window *w = NULL;

	if (!IsValidVehicleID(p1) || p2 > 2) return CMD_ERROR;

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
			w = FindWindowById(WC_VEHICLE_VIEW, first->index);
			if (w != NULL) DeleteWindow(w);
		}
		InvalidateWindow(WC_VEHICLE_DEPOT, first->tile);
		RebuildVehicleLists();
	}

	CommandCost cost(EXPENSES_NEW_VEHICLES);
	switch (p2) {
		case 0: case 2: { /* Delete given wagon */
			bool switch_engine = false;    // update second wagon to engine?
			byte ori_subtype = v->subtype; // backup subtype of deleted wagon in case DeleteVehicle() changes

			/* 1. Delete the engine, if it is dualheaded also delete the matching
			 * rear engine of the loco (from the point of deletion onwards) */
			Vehicle *rear = (IsMultiheaded(v) &&
				IsTrainEngine(v)) ? v->u.rail.other_multiheaded_part : NULL;

			if (rear != NULL) {
				cost.AddCost(-rear->value);
				if (flags & DC_EXEC) {
					UnlinkWagon(rear, first);
					DeleteDepotHighlightOfVehicle(rear);
					delete rear;
				}
			}

			/* 2. We are selling the first engine, some special action might be required
			 * here, so take attention */
			if ((flags & DC_EXEC) && v == first) {
				Vehicle *new_f = GetNextVehicle(first);

				/* 2.2 If there are wagons present after the deleted front engine, check
				 * if the second wagon (which will be first) is an engine. If it is one,
				 * promote it as a new train, retaining the unitnumber, orders */
				if (new_f != NULL && IsTrainEngine(new_f)) {
					switch_engine = true;
					/* Copy important data from the front engine */
					new_f->unitnumber      = first->unitnumber;
					new_f->current_order   = first->current_order;
					new_f->cur_order_index = first->cur_order_index;
					new_f->orders          = first->orders;
					new_f->num_orders      = first->num_orders;
					new_f->group_id        = first->group_id;

					if (first->prev_shared != NULL) {
						first->prev_shared->next_shared = new_f;
						new_f->prev_shared = first->prev_shared;
					}

					if (first->next_shared != NULL) {
						first->next_shared->prev_shared = new_f;
						new_f->next_shared = first->next_shared;
					}

					/*
					 * Remove all order information from the front train, to
					 * prevent the order and the shared order list to be
					 * destroyed by Destroy/DeleteVehicle.
					 */
					first->orders      = NULL;
					first->prev_shared = NULL;
					first->next_shared = NULL;
					first->group_id    = DEFAULT_GROUP;

					/* If we deleted a window then open a new one for the 'new' train */
					if (IsLocalPlayer() && w != NULL) ShowVehicleViewWindow(new_f);
				}
			}

			/* 3. Delete the requested wagon */
			cost.AddCost(-v->value);
			if (flags & DC_EXEC) {
				first = UnlinkWagon(v, first);
				DeleteDepotHighlightOfVehicle(v);
				delete v;

				/* 4 If the second wagon was an engine, update it to front_engine
					* which UnlinkWagon() has changed to TS_Free_Car */
				if (switch_engine) SetFrontEngine(first);

				/* 5. If the train still exists, update its acceleration, window, etc. */
				if (first != NULL) {
					NormaliseTrainConsist(first);
					TrainConsistChanged(first);
					UpdateTrainGroupID(first);
					if (IsFrontEngine(first)) {
						InvalidateWindow(WC_VEHICLE_DETAILS, first->index);
						InvalidateWindow(WC_VEHICLE_REFIT, first->index);
						UpdateTrainAcceleration(first);
					}
				}


				/* (6.) Borked AI. If it sells an engine it expects all wagons lined
				 * up on a new line to be added to the newly built loco. Replace it is.
				 * Totally braindead cause building a new engine adds all loco-less
				 * engines to its train anyways */
				if (p2 == 2 && HasBit(ori_subtype, Train_Front)) {
					Vehicle *tmp;
					for (v = first; v != NULL; v = tmp) {
						tmp = GetNextVehicle(v);
						DoCommand(v->tile, v->index | INVALID_VEHICLE << 16, 0, DC_EXEC, CMD_MOVE_RAIL_VEHICLE);
					}
				}
			}
		} break;
		case 1: { /* Delete wagon and all wagons after it given certain criteria */
			/* Start deleting every vehicle after the selected one
			 * If we encounter a matching rear-engine to a front-engine
			 * earlier in the chain (before deletion), leave it alone */
			Vehicle *tmp;
			for (; v != NULL; v = tmp) {
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
								DeleteDepotHighlightOfVehicle(rear);
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
					DeleteDepotHighlightOfVehicle(v);
					delete v;
				}
			}

			/* 3. If it is still a valid train after selling, update its acceleration and cached values */
			if (flags & DC_EXEC && first != NULL) {
				NormaliseTrainConsist(first);
				TrainConsistChanged(first);
				UpdateTrainGroupID(first);
				if (IsFrontEngine(first)) UpdateTrainAcceleration(first);
				InvalidateWindow(WC_VEHICLE_DETAILS, first->index);
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
	this->sprite_width  = GB(x, 16, 8);
	this->sprite_height = GB(x, 24, 8);
	this->z_height      = 6;
}

static void UpdateVarsAfterSwap(Vehicle *v)
{
	v->UpdateDeltaXY(v->direction);
	v->cur_image = v->GetImage(v->direction);
	BeginVehicleMove(v);
	VehiclePositionChanged(v);
	EndVehicleMove(v);
}

static void SetLastSpeed(Vehicle* v, int spd)
{
	int old = v->u.rail.last_speed;
	if (spd != old) {
		v->u.rail.last_speed = spd;
		if (_patches.vehicle_speed || (old == 0) != (spd == 0))
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
	}
}

static void SwapTrainFlags(byte *swap_flag1, byte *swap_flag2)
{
	byte flag1 = *swap_flag1;
	byte flag2 = *swap_flag2;

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
static void *TrainOnTileEnum(Vehicle *v, void *)
{
	return (v->type == VEH_TRAIN) ? v : NULL;
}


/**
 * Checks if a train is approaching a rail-road crossing
 * @param v vehicle on tile
 * @param data tile with crossing we are testing
 * @return v if it is approaching a crossing, NULL otherwise
 */
static void *TrainApproachingCrossingEnum(Vehicle *v, void *data)
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
 * @return pointer to vehicle approaching the crossing
 * @pre tile is a rail-road crossing
 */
static Vehicle *TrainApproachingCrossing(TileIndex tile)
{
	assert(IsLevelCrossingTile(tile));

	DiagDirection dir = AxisToDiagDir(OtherAxis(GetCrossingRoadAxis(tile)));
	TileIndex tile_from = tile + TileOffsByDiagDir(dir);

	Vehicle *v = (Vehicle *)VehicleFromPos(tile_from, &tile, &TrainApproachingCrossingEnum);

	if (v != NULL) return v;

	dir = ReverseDiagDir(dir);
	tile_from = tile + TileOffsByDiagDir(dir);

	return (Vehicle *)VehicleFromPos(tile_from, &tile, &TrainApproachingCrossingEnum);
}


/**
 * Sets correct crossing state
 * @param tile tile to update
 * @pre tile is a rail-road crossing
 */
void UpdateTrainCrossing(TileIndex tile)
{
	assert(IsLevelCrossingTile(tile));

	UnbarCrossing(tile);

	/* train on crossing || train approaching crossing */
	if (VehicleFromPos(tile, NULL, &TrainOnTileEnum) || TrainApproachingCrossing(tile)) {
		BarCrossing(tile);
	}

	MarkTileDirtyByTile(tile);
}


/**
 * Advances wagons for train reversing, needed for variable length wagons.
 * Needs to be called once before the train is reversed, and once after it.
 * @param v First vehicle in chain
 * @param before Set to true for the call before reversing, false otherwise
 */
static void AdvanceWagons(Vehicle *v, bool before)
{
	Vehicle *base = v;
	Vehicle *first = base->Next();
	uint length = CountVehiclesInChain(v);

	while (length > 2) {
		/* find pairwise matching wagon
		 * start<>end, start+1<>end-1, ... */
		Vehicle *last = first;
		for (uint i = length - 3; i > 0; i--) last = last->Next();

		int differential = last->u.rail.cached_veh_length - base->u.rail.cached_veh_length;
		if (before) differential *= -1;

		if (differential > 0) {
			/* disconnect last car to make sure only this subset moves */
			Vehicle *tempnext = last->Next();
			last->SetNext(NULL);

			/* do not update images now because the wagons are disconnected
			 * and that could cause problems with NewGRFs */
			for (int i = 0; i < differential; i++) TrainController(first, false);

			last->SetNext(tempnext);
		}

		base = first;
		first = first->Next();
		length -= 2;
	}
}


static void ReverseTrainDirection(Vehicle *v)
{
	if (IsTileDepotType(v->tile, TRANSPORT_RAIL)) {
		InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
	}

	/* Check if we were approaching a rail/road-crossing */
	TileIndex crossing = TrainApproachingCrossingTile(v);

	/* count number of vehicles */
	int r = 0;  ///< number of vehicles - 1
	for (const Vehicle *u = v; (u = u->Next()) != NULL;) { r++; }

	AdvanceWagons(v, true);

	/* swap start<>end, start+1<>end-1, ... */
	int l = 0;
	do {
		ReverseTrainSwapVeh(v, l++, r--);
	} while (l <= r);

	AdvanceWagons(v, false);

	if (IsTileDepotType(v->tile, TRANSPORT_RAIL)) {
		InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
	}

	/* update all images */
	for (Vehicle *u = v; u != NULL; u = u->Next()) { u->cur_image = u->GetImage(u->direction); }

	ClrBit(v->u.rail.flags, VRF_REVERSING);

	/* update crossing we were approaching */
	if (crossing != INVALID_TILE) UpdateTrainCrossing(crossing);

	/* maybe we are approaching crossing now, after reversal */
	crossing = TrainApproachingCrossingTile(v);
	if (crossing != INVALID_TILE) UpdateTrainCrossing(crossing);
}

/** Reverse train.
 * @param tile unused
 * @param flags type of operation
 * @param p1 train to reverse
 * @param p2 if true, reverse a unit in a train (needs to be in a depot)
 */
CommandCost CmdReverseTrainDirection(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	Vehicle *v = GetVehicle(p1);

	if (v->type != VEH_TRAIN || !CheckOwnership(v->owner)) return CMD_ERROR;

	if (p2) {
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
			if (_patches.realistic_acceleration && v->cur_speed != 0) {
				ToggleBit(v->u.rail.flags, VRF_REVERSING);
			} else {
				v->cur_speed = 0;
				SetLastSpeed(v, 0);
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
CommandCost CmdForceTrainProceed(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
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
CommandCost CmdRefitRailVehicle(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
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

		if (v->cargo_cap != 0) {
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
				const RailVehicleInfo *rvi = RailVehInfo(v->engine_type);
				CargoID old_cid = rvi->cargo_type;
				/* normally, the capacity depends on the cargo type, a rail vehicle can
				 * carry twice as much mail/goods as normal cargo, and four times as
				 * many passengers
				 */
				amount = rvi->capacity;
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

			if (amount != 0) {
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
					RebuildVehicleLists();
				}
			}
		}
	} while ((v = v->Next()) != NULL && !only_this);

	_returned_refit_capacity = num;

	/* Update the train's cached variables */
	if (flags & DC_EXEC) TrainConsistChanged(GetVehicle(p1)->First());

	return cost;
}

struct TrainFindDepotData {
	uint best_length;
	TileIndex tile;
	PlayerID owner;
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
	tfdd.best_length = (uint)-1;
	tfdd.reverse = false;

	TileIndex tile = v->tile;
	if (IsTileDepotType(tile, TRANSPORT_RAIL)) {
		tfdd.tile = tile;
		tfdd.best_length = 0;
		return tfdd;
	}

	if (_patches.yapf.rail_use_yapf) {
		bool found = YapfFindNearestRailDepotTwoWay(v, max_distance, NPF_INFINITE_PENALTY, &tfdd.tile, &tfdd.reverse);
		tfdd.best_length = found ? max_distance / 2 : -1; // some fake distance or NOT_FOUND
	} else if (_patches.new_pathfinding_all) {
		Vehicle* last = GetLastVehicleInChain(v);
		Trackdir trackdir = GetVehicleTrackdir(v);
		Trackdir trackdir_rev = ReverseTrackdir(GetVehicleTrackdir(last));

		assert(trackdir != INVALID_TRACKDIR);
		NPFFoundTargetData ftd = NPFRouteToDepotBreadthFirstTwoWay(v->tile, trackdir, last->tile, trackdir_rev, TRANSPORT_RAIL, 0, v->owner, v->u.rail.compatible_railtypes, NPF_INFINITE_PENALTY);
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
	} else {
		/* search in the forward direction first. */
		DiagDirection i = TrainExitDir(v->direction, v->u.rail.track);
		NewTrainPathfind(tile, 0, v->u.rail.compatible_railtypes, i, (NTPEnumProc*)NtpCallbFindDepot, &tfdd);
		if (tfdd.best_length == (uint)-1){
			tfdd.reverse = true;
			/* search in backwards direction */
			i = TrainExitDir(ReverseDir(v->direction), v->u.rail.track);
			NewTrainPathfind(tile, 0, v->u.rail.compatible_railtypes, i, (NTPEnumProc*)NtpCallbFindDepot, &tfdd);
		}
	}

	return tfdd;
}

/** Send a train to a depot
 * @param tile unused
 * @param flags type of operation
 * @param p1 train to send to the depot
 * @param p2 various bitmasked elements
 * - p2 bit 0-3 - DEPOT_ flags (see vehicle.h)
 * - p2 bit 8-10 - VLW flag (for mass goto depot)
 */
CommandCost CmdSendTrainToDepot(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	if (p2 & DEPOT_MASS_SEND) {
		/* Mass goto depot requested */
		if (!ValidVLWFlags(p2 & VLW_MASK)) return CMD_ERROR;
		return SendAllVehiclesToDepot(VEH_TRAIN, flags, p2 & DEPOT_SERVICE, _current_player, (p2 & VLW_MASK), p1);
	}

	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	Vehicle *v = GetVehicle(p1);

	if (v->type != VEH_TRAIN || !CheckOwnership(v->owner)) return CMD_ERROR;

	if (v->vehstatus & VS_CRASHED) return CMD_ERROR;

	if (v->current_order.type == OT_GOTO_DEPOT) {
		if (!!(p2 & DEPOT_SERVICE) == HasBit(v->current_order.flags, OF_HALT_IN_DEPOT)) {
			/* We called with a different DEPOT_SERVICE setting.
			 * Now we change the setting to apply the new one and let the vehicle head for the same depot.
			 * Note: the if is (true for requesting service == true for ordered to stop in depot)          */
			if (flags & DC_EXEC) {
				ClrBit(v->current_order.flags, OF_PART_OF_ORDERS);
				ToggleBit(v->current_order.flags, OF_HALT_IN_DEPOT);
				InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
			}
			return CommandCost();
		}

		if (p2 & DEPOT_DONT_CANCEL) return CMD_ERROR; // Requested no cancelation of depot orders
		if (flags & DC_EXEC) {
			if (HasBit(v->current_order.flags, OF_PART_OF_ORDERS)) {
				v->cur_order_index++;
			}

			v->current_order.type = OT_DUMMY;
			v->current_order.flags = 0;
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
		}
		return CommandCost();
	}

	/* check if at a standstill (not stopped only) in a depot
	 * the check is down here to make it possible to alter stop/service for trains entering the depot */
	if (IsTileDepotType(v->tile, TRANSPORT_RAIL) && v->cur_speed == 0) return CMD_ERROR;

	TrainFindDepotData tfdd = FindClosestTrainDepot(v, 0);
	if (tfdd.best_length == (uint)-1) return_cmd_error(STR_883A_UNABLE_TO_FIND_ROUTE_TO);

	if (flags & DC_EXEC) {
		if (v->current_order.type == OT_LOADING) v->LeaveStation();

		v->dest_tile = tfdd.tile;
		v->current_order.type = OT_GOTO_DEPOT;
		v->current_order.flags = OFB_NON_STOP;
		if (!(p2 & DEPOT_SERVICE)) SetBit(v->current_order.flags, OF_HALT_IN_DEPOT);
		v->current_order.dest = GetDepotByTile(tfdd.tile)->index;
		v->current_order.refit_cargo = CT_INVALID;
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
		/* If there is no depot in front, reverse automatically */
		if (tfdd.reverse) DoCommand(v->tile, v->index, 0, DC_EXEC, CMD_REVERSE_TRAIN_DIRECTION);
	}

	return CommandCost();
}


void OnTick_Train()
{
	_age_cargo_skip_counter = (_age_cargo_skip_counter == 0) ? 184 : (_age_cargo_skip_counter - 1);
}

static const int8 _vehicle_smoke_pos[8] = {
	1, 1, 1, 0, -1, -1, -1, 0
};

static void HandleLocomotiveSmokeCloud(const Vehicle* v)
{
	bool sound = false;

	if (v->vehstatus & VS_TRAIN_SLOWING || v->load_unload_time_rem != 0 || v->cur_speed < 2)
		return;

	const Vehicle* u = v;

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
		if (IsTileDepotType(v->tile, TRANSPORT_RAIL) || IsTunnelTile(v->tile)) continue;

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
		}
	} while ((v = v->Next()) != NULL);

	if (sound) PlayVehicleSound(u, VSE_TRAIN_EFFECT);
}

static void TrainPlayLeaveStationSound(const Vehicle* v)
{
	static const SoundFx sfx[] = {
		SND_04_TRAIN,
		SND_0A_TRAIN_HORN,
		SND_0A_TRAIN_HORN,
		SND_47_MAGLEV_2,
		SND_41_MAGLEV
	};

	if (PlayVehicleSound(v, VSE_START)) return;

	EngineID engtype = v->engine_type;
	SndPlayVehicleFx(sfx[RailVehInfo(engtype)->engclass], v);
}

void Train::PlayLeaveStationSound() const
{
	TrainPlayLeaveStationSound(this);
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

	if (v->u.rail.force_proceed == 0) {
		if (++v->load_unload_time_rem < 37) {
			InvalidateWindowClasses(WC_TRAINS_LIST);
			return true;
		}

		v->load_unload_time_rem = 0;

		if (UpdateSignalsOnSegment(v->tile, INVALID_DIAGDIR, v->owner)) {
			InvalidateWindowClasses(WC_TRAINS_LIST);
			return true;
		}
	}

	VehicleServiceInDepot(v);
	InvalidateWindowClasses(WC_TRAINS_LIST);
	TrainPlayLeaveStationSound(v);

	v->u.rail.track = TRACK_BIT_X;
	if (v->direction & 2) v->u.rail.track = TRACK_BIT_Y;

	v->vehstatus &= ~VS_HIDDEN;
	v->cur_speed = 0;

	v->UpdateDeltaXY(v->direction);
	v->cur_image = v->GetImage(v->direction);
	VehiclePositionChanged(v);
	UpdateSignalsOnSegment(v->tile, INVALID_DIAGDIR, v->owner);
	UpdateTrainAcceleration(v);
	InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);

	return false;
}

/* Check for station tiles */
struct TrainTrackFollowerData {
	TileIndex dest_coords;
	StationID station_index; // station index we're heading for
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

static void FillWithStationData(TrainTrackFollowerData* fd, const Vehicle* v)
{
	fd->dest_coords = v->dest_tile;
	if (v->current_order.type == OT_GOTO_STATION) {
		fd->station_index = v->current_order.dest;
	} else {
		fd->station_index = INVALID_STATION;
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

static const uint32 _reachable_tracks[4] = {
	0x10091009,
	0x00160016,
	0x05200520,
	0x2A002A00,
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

/* choose a track */
static Track ChooseTrainTrack(Vehicle* v, TileIndex tile, DiagDirection enterdir, TrackBits tracks)
{
	Track best_track;
	/* pathfinders are able to tell that route was only 'guessed' */
	bool path_not_found = false;

#ifdef PF_BENCHMARK
	TIC()
#endif

	assert((tracks & ~TRACK_BIT_MASK) == 0);

	/* quick return in case only one possible track is available */
	if (KillFirstBit(tracks) == TRACK_BIT_NONE) return FindFirstTrack(tracks);

	if (_patches.yapf.rail_use_yapf) {
		Trackdir trackdir = YapfChooseRailTrack(v, tile, enterdir, tracks, &path_not_found);
		if (trackdir != INVALID_TRACKDIR) {
			best_track = TrackdirToTrack(trackdir);
		} else {
			best_track = FindFirstTrack(tracks);
		}
	} else if (_patches.new_pathfinding_all) { /* Use a new pathfinding for everything */
		void* perf = NpfBeginInterval();

		NPFFindStationOrTileData fstd;
		NPFFillWithOrderData(&fstd, v);
		/* The enterdir for the new tile, is the exitdir for the old tile */
		Trackdir trackdir = GetVehicleTrackdir(v);
		assert(trackdir != 0xff);

		NPFFoundTargetData ftd = NPFRouteToStationOrTile(tile - TileOffsByDiagDir(enterdir), trackdir, &fstd, TRANSPORT_RAIL, 0, v->owner, v->u.rail.compatible_railtypes);

		if (ftd.best_trackdir == 0xff) {
			/* We are already at our target. Just do something
			 * @todo maybe display error?
			 * @todo: go straight ahead if possible? */
			best_track = FindFirstTrack(tracks);
		} else {
			/* If ftd.best_bird_dist is 0, we found our target and ftd.best_trackdir contains
			the direction we need to take to get there, if ftd.best_bird_dist is not 0,
			we did not find our target, but ftd.best_trackdir contains the direction leading
			to the tile closest to our target. */
			if (ftd.best_bird_dist != 0) path_not_found = true;
			/* Discard enterdir information, making it a normal track */
			best_track = TrackdirToTrack(ftd.best_trackdir);
		}

		int time = NpfEndInterval(perf);
		DEBUG(yapf, 4, "[NPFT] %d us - %d rounds - %d open - %d closed -- ", time, 0, _aystar_stats_open_size, _aystar_stats_closed_size);
	} else {
		void* perf = NpfBeginInterval();

		TrainTrackFollowerData fd;
		FillWithStationData(&fd, v);

		/* New train pathfinding */
		fd.best_bird_dist = (uint)-1;
		fd.best_track_dist = (uint)-1;
		fd.best_track = INVALID_TRACKDIR;

		NewTrainPathfind(tile - TileOffsByDiagDir(enterdir), v->dest_tile,
			v->u.rail.compatible_railtypes, enterdir, (NTPEnumProc*)NtpCallbFindStation, &fd);

		/* check whether the path was found or only 'guessed' */
		if (fd.best_bird_dist != 0) path_not_found = true;

		if (fd.best_track == 0xff) {
			/* blaha */
			best_track = FindFirstTrack(tracks);
		} else {
			best_track = TrackdirToTrack(fd.best_track);
		}

		int time = NpfEndInterval(perf);
		DEBUG(yapf, 4, "[NTPT] %d us - %d rounds - %d open - %d closed -- ", time, 0, 0, 0);
	}
	/* handle "path not found" state */
	if (path_not_found) {
		/* PF didn't find the route */
		if (!HasBit(v->u.rail.flags, VRF_NO_PATH_TO_DESTINATION)) {
			/* it is first time the problem occurred, set the "path not found" flag */
			SetBit(v->u.rail.flags, VRF_NO_PATH_TO_DESTINATION);
			/* and notify user about the event */
			if (_patches.lost_train_warn && v->owner == _local_player) {
				SetDParam(0, v->unitnumber);
				AddNewsItem(
					STR_TRAIN_IS_LOST,
					NEWS_FLAGS(NM_SMALL, NF_VIEWPORT|NF_VEHICLE, NT_ADVICE, 0),
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

#ifdef PF_BENCHMARK
	TOC("PF time = ", 1)
#endif

	return best_track;
}


static bool CheckReverseTrain(Vehicle *v)
{
	if (_opt.diff.line_reverse_mode != 0 ||
			v->u.rail.track == TRACK_BIT_DEPOT || v->u.rail.track == TRACK_BIT_WORMHOLE ||
			!(v->direction & 1))
		return false;

	TrainTrackFollowerData fd;
	FillWithStationData(&fd, v);

	uint reverse_best = 0;

	assert(v->u.rail.track);

	int i = _search_directions[FIND_FIRST_BIT(v->u.rail.track)][DirToDiagDir(v->direction)];

	if (_patches.yapf.rail_use_yapf) {
		reverse_best = YapfCheckReverseTrain(v);
	} else if (_patches.new_pathfinding_all) { /* Use a new pathfinding for everything */
		NPFFindStationOrTileData fstd;
		NPFFoundTargetData ftd;
		Trackdir trackdir, trackdir_rev;
		Vehicle* last = GetLastVehicleInChain(v);

		NPFFillWithOrderData(&fstd, v);

		trackdir = GetVehicleTrackdir(v);
		trackdir_rev = ReverseTrackdir(GetVehicleTrackdir(last));
		assert(trackdir != 0xff);
		assert(trackdir_rev != 0xff);

		ftd = NPFRouteToStationOrTileTwoWay(v->tile, trackdir, last->tile, trackdir_rev, &fstd, TRANSPORT_RAIL, 0, v->owner, v->u.rail.compatible_railtypes);
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
	} else {
		int best_track = -1;
		uint reverse = 0;
		uint best_bird_dist  = 0;
		uint best_track_dist = 0;

		for (;;) {
			fd.best_bird_dist = (uint)-1;
			fd.best_track_dist = (uint)-1;

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
	}

	return reverse_best != 0;
}

static bool ProcessTrainOrder(Vehicle *v)
{
	switch (v->current_order.type) {
		case OT_GOTO_DEPOT:
			if (!(v->current_order.flags & OFB_PART_OF_ORDERS)) return false;
			if ((v->current_order.flags & OFB_SERVICE_IF_NEEDED) &&
					!VehicleNeedsService(v)) {
				UpdateVehicleTimetable(v, true);
				v->cur_order_index++;
			}
			break;

		case OT_LOADING:
		case OT_LEAVESTATION:
			return false;

		default: break;
	}

	/**
	 * Reversing because of order change is allowed only just after leaving a
	 * station (and the difficulty setting to allowed, of course)
	 * this can be detected because only after OT_LEAVESTATION, current_order
	 * will be reset to nothing. (That also happens if no order, but in that case
	 * it won't hit the point in code where may_reverse is checked)
	 */
	bool may_reverse = v->current_order.type == OT_NOTHING;

	/* check if we've reached the waypoint? */
	if (v->current_order.type == OT_GOTO_WAYPOINT && v->tile == v->dest_tile) {
		UpdateVehicleTimetable(v, true);
		v->cur_order_index++;
	}

	/* check if we've reached a non-stop station while TTDPatch nonstop is enabled.. */
	if (_patches.new_nonstop &&
			v->current_order.flags & OFB_NON_STOP &&
			IsTileType(v->tile, MP_STATION) &&
			v->current_order.dest == GetStationIndex(v->tile)) {
		UpdateVehicleTimetable(v, true);
		v->cur_order_index++;
	}

	/* Get the current order */
	if (v->cur_order_index >= v->num_orders) v->cur_order_index = 0;

	const Order *order = GetVehicleOrder(v, v->cur_order_index);

	/* If no order, do nothing. */
	if (order == NULL) {
		v->current_order.Free();
		v->dest_tile = 0;
		return false;
	}

	/* If it is unchanged, keep it. */
	if (order->type  == v->current_order.type &&
			order->flags == v->current_order.flags &&
			order->dest  == v->current_order.dest)
		return false;

	/* Otherwise set it, and determine the destination tile. */
	v->current_order = *order;

	v->dest_tile = 0;

	InvalidateVehicleOrder(v);

	switch (order->type) {
		case OT_GOTO_STATION:
			if (order->dest == v->last_station_visited)
				v->last_station_visited = INVALID_STATION;
			v->dest_tile = GetStation(order->dest)->xy;
			break;

		case OT_GOTO_DEPOT:
			v->dest_tile = GetDepot(order->dest)->xy;
			break;

		case OT_GOTO_WAYPOINT:
			v->dest_tile = GetWaypoint(order->dest)->xy;
			break;

		default:
			return false;
	}

	return may_reverse && CheckReverseTrain(v);
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

static int UpdateTrainSpeed(Vehicle *v)
{
	uint accel;

	if (v->vehstatus & VS_STOPPED || HasBit(v->u.rail.flags, VRF_REVERSING)) {
		if (_patches.realistic_acceleration) {
			accel = GetTrainAcceleration(v, AM_BRAKE) * 2;
		} else {
			accel = v->acceleration * -2;
		}
	} else {
		if (_patches.realistic_acceleration) {
			accel = GetTrainAcceleration(v, AM_ACCEL);
		} else {
			accel = v->acceleration;
		}
	}

	uint spd = v->subspeed + accel * 2;
	v->subspeed = (byte)spd;
	{
		int tempmax = v->max_speed;
		if (v->cur_speed > v->max_speed)
			tempmax = v->cur_speed - (v->cur_speed / 10) - 1;
		v->cur_speed = spd = Clamp(v->cur_speed + ((int)spd >> 8), 0, tempmax);
	}

	if (!(v->direction & 1)) spd = spd * 3 >> 2;

	spd += v->progress;
	v->progress = (byte)spd;
	return (spd >> 8);
}

static void TrainEnterStation(Vehicle *v, StationID station)
{
	v->last_station_visited = station;

	/* check if a train ever visited this station before */
	Station *st = GetStation(station);
	if (!(st->had_vehicle_of_type & HVOT_TRAIN)) {
		st->had_vehicle_of_type |= HVOT_TRAIN;
		SetDParam(0, st->index);
		uint32 flags = v->owner == _local_player ?
			NEWS_FLAGS(NM_THIN, NF_VIEWPORT | NF_VEHICLE, NT_ARRIVAL_PLAYER, 0) :
			NEWS_FLAGS(NM_THIN, NF_VIEWPORT | NF_VEHICLE, NT_ARRIVAL_OTHER,  0);
		AddNewsItem(
			STR_8801_CITIZENS_CELEBRATE_FIRST,
			flags,
			v->index,
			0
		);
	}

	v->BeginLoading();
	v->current_order.dest = 0;
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

	VehiclePositionChanged(v);
	EndVehicleMove(v);
	return old_z;
}

static const Direction _new_vehicle_direction_table[11] = {
	DIR_N , DIR_NW, DIR_W , INVALID_DIR,
	DIR_NE, DIR_N , DIR_SW, INVALID_DIR,
	DIR_E , DIR_SE, DIR_S
};

static Direction GetNewVehicleDirectionByTile(TileIndex new_tile, TileIndex old_tile)
{
	uint offs = (TileY(new_tile) - TileY(old_tile) + 1) * 4 +
							TileX(new_tile) - TileX(old_tile) + 1;
	assert(offs < 11);
	return _new_vehicle_direction_table[offs];
}

static int GetDirectionToVehicle(const Vehicle *v, int x, int y)
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
static bool CheckCompatibleRail(const Vehicle *v, TileIndex tile)
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
	// normal accel
	{256 / 4, 256 / 2, 256 / 4, 2}, ///< normal
	{256 / 4, 256 / 2, 256 / 4, 2}, ///< electrified
	{256 / 4, 256 / 2, 256 / 4, 2}, ///< monorail
	{0,       256 / 2, 256 / 4, 2}, ///< maglev
};

/** Modify the speed of the vehicle due to a turn */
static void AffectSpeedByDirChange(Vehicle* v, Direction new_dir)
{
	if (_patches.realistic_acceleration) return;

	DirDiff diff = DirDifference(v->direction, new_dir);
	if (diff == DIRDIFF_SAME) return;

	const RailtypeSlowdownParams *rsp = &_railtype_slowdown[v->u.rail.railtype];
	v->cur_speed -= (diff == DIRDIFF_45RIGHT || diff == DIRDIFF_45LEFT ? rsp->small_turn : rsp->large_turn) * v->cur_speed >> 8;
}

/** Modify the speed of the vehicle due to a change in altitude */
static void AffectSpeedByZChange(Vehicle *v, byte old_z)
{
	if (old_z == v->z_pos || _patches.realistic_acceleration) return;

	const RailtypeSlowdownParams *rsp = &_railtype_slowdown[v->u.rail.railtype];

	if (old_z < v->z_pos) {
		v->cur_speed -= (v->cur_speed * rsp->z_up >> 8);
	} else {
		uint16 spd = v->cur_speed + rsp->z_down;
		if (spd <= v->max_speed) v->cur_speed = spd;
	}
}

static const DiagDirection _otherside_signal_directions[] = {
	DIAGDIR_NE, DIAGDIR_SE, DIAGDIR_NE, DIAGDIR_SE, DIAGDIR_SW, DIAGDIR_SE, INVALID_DIAGDIR, INVALID_DIAGDIR,
	DIAGDIR_SW, DIAGDIR_NW, DIAGDIR_NW, DIAGDIR_SW, DIAGDIR_NW, DIAGDIR_NE
};

static void TrainMovedChangeSignals(TileIndex tile, DiagDirection dir)
{
	if (IsTileType(tile, MP_RAILWAY) &&
			GetRailTileType(tile) == RAIL_TILE_SIGNALS) {
		uint i = FindFirstBit2x64(GetTrackBits(tile) * 0x101 & _reachable_tracks[dir]);
		UpdateSignalsOnSegment(tile, _otherside_signal_directions[i], GetTileOwner(tile));
	}
}


static void SetVehicleCrashed(Vehicle *v)
{
	if (v->u.rail.crash_anim_pos != 0) return;

	/* we may need to update crossing we were approaching */
	TileIndex crossing = TrainApproachingCrossingTile(v);

	v->u.rail.crash_anim_pos++;

	InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
	InvalidateWindow(WC_VEHICLE_DETAILS, v->index);

	if (v->u.rail.track == TRACK_BIT_DEPOT) {
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
	}

	RebuildVehicleLists();

	BEGIN_ENUM_WAGONS(v)
		v->vehstatus |= VS_CRASHED;
		MarkSingleVehicleDirty(v);
	END_ENUM_WAGONS(v)

	/* must be updated after the train has been marked crashed */
	if (crossing != INVALID_TILE) UpdateTrainCrossing(crossing);
}

static uint CountPassengersInTrain(const Vehicle* v)
{
	uint num = 0;
	BEGIN_ENUM_WAGONS(v)
		if (IsCargoInClass(v->cargo_type, CC_PASSENGERS)) num += v->cargo.Count();
	END_ENUM_WAGONS(v)
	return num;
}

struct TrainCollideChecker {
	Vehicle *v;
	uint num;
};

static void *FindTrainCollideEnum(Vehicle *v, void *data)
{
	TrainCollideChecker* tcc = (TrainCollideChecker*)data;

	if (v->type != VEH_TRAIN) return NULL;

	/* get first vehicle now to make most usual checks faster */
	Vehicle *coll = v->First();

	/* can't collide with own wagons && can't crash in depot && the same height level */
	if (coll != tcc->v && v->u.rail.track != TRACK_BIT_DEPOT && abs(v->z_pos - tcc->v->z_pos) < 6) {
		int x_diff = v->x_pos - tcc->v->x_pos;
		int y_diff = v->y_pos - tcc->v->y_pos;

		/* needed to disable possible crash of competitor train in station by building diagonal track at its end */
		if (x_diff * x_diff + y_diff * y_diff > 25) return NULL;

		if (!(tcc->v->vehstatus & VS_CRASHED)) {
			/* two drivers + passengers killed in train tcc->v (if it was not crashed already) */
			tcc->num += 2 + CountPassengersInTrain(tcc->v);
			SetVehicleCrashed(tcc->v);
		}

		if (!(coll->vehstatus & VS_CRASHED)) {
			/* two drivers + passengers killed in train coll (if it was not crashed already) */
			tcc->num += 2 + CountPassengersInTrain(coll);
			SetVehicleCrashed(coll);
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
static void CheckTrainCollision(Vehicle *v)
{
	/* can't collide in depot */
	if (v->u.rail.track == TRACK_BIT_DEPOT) return;

	assert(v->u.rail.track == TRACK_BIT_WORMHOLE || TileVirtXY(v->x_pos, v->y_pos) == v->tile);

	TrainCollideChecker tcc;
	tcc.v = v;
	tcc.num = 0;

	/* find colliding vehicles */
	if (v->u.rail.track == TRACK_BIT_WORMHOLE) {
		VehicleFromPos(v->tile, &tcc, FindTrainCollideEnum);
		VehicleFromPos(GetOtherTunnelBridgeEnd(v->tile), &tcc, FindTrainCollideEnum);
	} else {
		VehicleFromPosXY(v->x_pos, v->y_pos, &tcc, FindTrainCollideEnum);
	}

	/* any dead -> no crash */
	if (tcc.num == 0) return;

	SetDParam(0, tcc.num);
	AddNewsItem(STR_8868_TRAIN_CRASH_DIE_IN_FIREBALL,
		NEWS_FLAGS(NM_THIN, NF_VIEWPORT | NF_VEHICLE, NT_ACCIDENT, 0),
		v->index,
		0
	);

	ModifyStationRatingAround(v->tile, v->owner, -160, 30);
	SndPlayVehicleFx(SND_13_BIG_CRASH, v);
}

static void *CheckVehicleAtSignal(Vehicle *v, void *data)
{
	Direction dir = *(Direction*)data;

	if (v->type == VEH_TRAIN && IsFrontEngine(v)) {
		DirDiff diff = ChangeDirDiff(DirDifference(v->direction, dir), DIRDIFF_90RIGHT);

		if (diff == DIRDIFF_90RIGHT || (v->cur_speed <= 5 && diff <= DIRDIFF_REVERSE)) return v;
	}
	return NULL;
}

static void TrainController(Vehicle *v, bool update_image)
{
	Vehicle *prev;

	/* For every vehicle after and including the given vehicle */
	for (prev = v->Previous(); v != NULL; prev = v, v = v->Next()) {
		DiagDirection enterdir = DIAGDIR_BEGIN;
		bool update_signals_crossing = false; // will we update signals or crossing state?
		BeginVehicleMove(v);

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

					if (IsFrontEngine(v) && !TrainCheckIfLineEnds(v)) return;

					uint32 r = VehicleEnterTile(v, gp.new_tile, gp.x, gp.y);
					if (HasBit(r, VETS_CANNOT_ENTER)) {
						goto invalid_rail;
					}
					if (HasBit(r, VETS_ENTERED_STATION)) {
						TrainEnterStation(v, r >> VETS_STATION_ID_OFFSET);
						return;
					}

					if (v->current_order.type == OT_LEAVESTATION) {
						v->current_order.Free();
						InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
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
				uint32 ts = GetTileTrackStatus(gp.new_tile, TRANSPORT_RAIL, 0) & _reachable_tracks[enterdir];

				/* Combine the from & to directions.
				 * Now, the lower byte contains the track status, and the byte at bit 16 contains
				 * the signal status. */
				uint32 tracks = ts | (ts >> 8);
				TrackBits bits = (TrackBits)(tracks & TRACK_BIT_MASK);
				if ((_patches.new_pathfinding_all || _patches.yapf.rail_use_yapf) && _patches.forbid_90_deg && prev == NULL) {
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
					chosen_track = TrackToTrackBits(ChooseTrainTrack(v, gp.new_tile, enterdir, bits));
					assert(chosen_track & tracks);

					/* Check if it's a red signal and that force proceed is not clicked. */
					if ((tracks >> 16) & chosen_track && v->u.rail.force_proceed == 0) {
						/* In front of a red signal
						 * find the first set bit in ts. need to do it in 2 steps, since
						 * FIND_FIRST_BIT only handles 6 bits at a time. */
						Trackdir i = FindFirstTrackdir((TrackdirBits)(uint16)ts);

						if (!HasSignalOnTrackdir(gp.new_tile, ReverseTrackdir(i))) {
							v->cur_speed = 0;
							v->subspeed = 0;
							v->progress = 255 - 100;
							if (++v->load_unload_time_rem < _patches.wait_oneway_signal * 20) return;
						} else if (HasSignalOnTrackdir(gp.new_tile, i)) {
							v->cur_speed = 0;
							v->subspeed = 0;
							v->progress = 255 - 10;
							if (++v->load_unload_time_rem < _patches.wait_twoway_signal * 73) {
								TileIndex o_tile = gp.new_tile + TileOffsByDiagDir(enterdir);
								Direction rdir = ReverseDir(dir);

								/* check if a train is waiting on the other side */
								if (VehicleFromPos(o_tile, &rdir, &CheckVehicleAtSignal) == NULL) return;
							}
						}
						goto reverse_train_direction;
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

				if (IsFrontEngine(v)) v->load_unload_time_rem = 0;

				if (!HasBit(r, VETS_ENTERED_WORMHOLE)) {
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
			}
		} else {
			/* In a tunnel or on a bridge
			 * - for tunnels, only the part when the vehicle is not visible (part of enter/exit tile too)
			 * - for bridges, only the middle part - without the bridge heads */
			if (!(v->vehstatus & VS_HIDDEN)) {
				v->cur_speed =
					min(v->cur_speed, GetBridge(GetBridgeType(v->tile))->speed);
			}

			if (!(IsTunnelTile(gp.new_tile) || IsBridgeTile(gp.new_tile)) || !HasBit(VehicleEnterTile(v, gp.new_tile, gp.x, gp.y), VETS_ENTERED_WORMHOLE)) {
				v->x_pos = gp.x;
				v->y_pos = gp.y;
				VehiclePositionChanged(v);
				if (!(v->vehstatus & VS_HIDDEN)) EndVehicleMove(v);
				continue;
			}
		}

		/* update image of train, as well as delta XY */
		v->UpdateDeltaXY(v->direction);
		if (update_image) v->cur_image = v->GetImage(v->direction);

		v->x_pos = gp.x;
		v->y_pos = gp.y;

		/* update the Z position of the vehicle */
		byte old_z = AfterSetTrainPos(v, (gp.new_tile != gp.old_tile));

		if (prev == NULL) {
			/* This is the first vehicle in the train */
			AffectSpeedByZChange(v, old_z);
		}

		if (update_signals_crossing) {
			if (IsFrontEngine(v)) TrainMovedChangeSignals(gp.new_tile, enterdir);

			/* Signals can only change when the first
			 * (above) or the last vehicle moves. */
			if (v->Next() == NULL) {
				TrainMovedChangeSignals(gp.old_tile, ReverseDiagDir(enterdir));
				if (IsLevelCrossingTile(gp.old_tile)) UpdateTrainCrossing(gp.old_tile);
			}
		}
	}
	return;

invalid_rail:
	/* We've reached end of line?? */
	if (prev != NULL) error("!Disconnecting train");

reverse_train_direction:
	v->load_unload_time_rem = 0;
	v->cur_speed = 0;
	v->subspeed = 0;
	ReverseTrainDirection(v);
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

	if (first == v) {
		/* Removing front vehicle (the last to go) */
		DeleteWindowById(WC_VEHICLE_VIEW, v->index);
		InvalidateWindow(WC_COMPANY, v->owner);
	} else {
		/* Recalculate cached train properties */
		TrainConsistChanged(first);
		InvalidateWindow(WC_VEHICLE_DETAILS, first->index);
		/* Update the depot window if the first vehicle is in depot -
		 * if v == first, then it is updated in PreDestructor() */
		if (first->u.rail.track == TRACK_BIT_DEPOT) {
			InvalidateWindow(WC_VEHICLE_DEPOT, first->tile);
		}
	}

	RebuildVehicleLists();

	MarkSingleVehicleDirty(v);

	/* 'v' shouldn't be accessed after it has been deleted */
	TrackBits track = v->u.rail.track;
	TileIndex tile = v->tile;
	Owner owner = v->owner;

	delete v;
	v = NULL; // make sure nobody will won't try to read 'v' anymore

	/* check if the wagon was on a road/rail-crossing */
	if (IsLevelCrossingTile(tile)) UpdateTrainCrossing(tile);

	/* Update signals */
	if (IsTileType(tile, MP_TUNNELBRIDGE) || IsTileDepotType(tile, TRANSPORT_RAIL)) {
		UpdateSignalsOnSegment(tile, INVALID_DIAGDIR, owner);
	} else {
		SetSignalsOnBothDir(tile, (Track)(FIND_FIRST_BIT(track)), owner);
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
			BeginVehicleMove(v);
			v->UpdateDeltaXY(v->direction);
			v->cur_image = v->GetImage(v->direction);
			/* Refrain from updating the z position of the vehicle when on
			   a bridge, because AfterSetTrainPos will put the vehicle under
			   the bridge in that case */
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

	if (state >= 4440 && !(v->tick_counter&0x1F)) {
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
			SndPlayVehicleFx((_opt.landscape != LT_TOYLAND) ?
				SND_10_TRAIN_BREAKDOWN : SND_3A_COMEDY_BREAKDOWN_2, v);
		}

		if (!(v->vehstatus & VS_HIDDEN)) {
			Vehicle *u = CreateEffectVehicleRel(v, 4, 4, 5, EV_BREAKDOWN_SMOKE);
			if (u != NULL) u->u.special.animation_state = v->breakdown_delay * 2;
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
		case DIR_NW: x = y;            /* FALLTHROUGH */
		case DIR_NE: x = ~x + 16;      break;
		case DIR_E : x = ~x + y + 9;   break;
		case DIR_SE: x = y;            break;
		case DIR_S : x = x + y - 7;    break;
		case DIR_W : x = ~y + x + 9;   break;
		default: break;
	}

	/* do not reverse when approaching red signal */
	if (!signal && x + 4 >= TILE_SIZE) {
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
	if (IsTileDepotType(tile, TRANSPORT_RAIL)) {
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

	/* not a crossing || wrong axis || wrong railtype || wrong owner */
	if (!IsLevelCrossingTile(tile) || DiagDirToAxis(dir) == GetCrossingRoadAxis(tile) ||
			!CheckCompatibleRail(v, tile) || GetTileOwner(tile) != v->owner) {
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
	uint32 ts = GetTileTrackStatus(tile, TRANSPORT_RAIL, 0) & _reachable_tracks[dir];

	/* We are sure the train is not entering a depot, it is detected above */

	/* no suitable trackbits at all || wrong railtype || not our track ||
	 *   tunnel/bridge from opposite side || depot from opposite side */
	if (GB(ts, 0, 16) == 0 || !CheckCompatibleRail(v, tile) || GetTileOwner(tile) != v->owner ||
			(IsTileType(tile, MP_TUNNELBRIDGE) && GetTunnelBridgeDirection(tile) != dir) ||
			(IsTileDepotType(tile, TRANSPORT_RAIL) && GetRailDepotDirection(tile) == dir) ) {
		return TrainApproachingLineEnd(v, false);
	}

	/* approaching red signal */
	if ((ts & (ts >> 16)) != 0) return TrainApproachingLineEnd(v, true);

	/* approaching a rail/road crossing? then make it red */
	if (IsLevelCrossingTile(tile) && !IsCrossingBarred(tile)) {
		BarCrossing(tile);
		SndPlayVehicleFx(SND_0E_LEVEL_CROSSING, v);
		MarkTileDirtyByTile(tile);
	}

	return true;
}


static void TrainLocoHandler(Vehicle *v, bool mode)
{
	/* train has crashed? */
	if (v->vehstatus & VS_CRASHED) {
		if (!mode) HandleCrashedTrain(v);
		return;
	}

	if (v->u.rail.force_proceed != 0) v->u.rail.force_proceed--;

	/* train is broken down? */
	if (v->breakdown_ctr != 0) {
		if (v->breakdown_ctr <= 2) {
			HandleBrokenTrain(v);
			return;
		}
		v->breakdown_ctr--;
	}

	if (HasBit(v->u.rail.flags, VRF_REVERSING) && v->cur_speed == 0) {
		ReverseTrainDirection(v);
	}

	/* exit if train is stopped */
	if (v->vehstatus & VS_STOPPED && v->cur_speed == 0) return;

	if (ProcessTrainOrder(v)) {
		v->load_unload_time_rem = 0;
		v->cur_speed = 0;
		v->subspeed = 0;
		ReverseTrainDirection(v);
		return;
	}

	v->HandleLoading(mode);

	if (v->current_order.type == OT_LOADING) return;

	if (CheckTrainStayInDepot(v)) return;

	if (!mode) HandleLocomotiveSmokeCloud(v);

	int j = UpdateTrainSpeed(v);
	if (j == 0) {
		/* if the vehicle has speed 0, update the last_speed field. */
		if (v->cur_speed != 0) return;
	} else {
		TrainCheckIfLineEnds(v);

		do {
			TrainController(v, true);
			CheckTrainCollision(v);
			if (v->cur_speed <= 0x100)
				break;
		} while (--j != 0);
	}

	SetLastSpeed(v, v->cur_speed);
}



Money Train::GetRunningCost() const
{
	Money cost = 0;
	const Vehicle *v = this;

	do {
		const RailVehicleInfo *rvi = RailVehInfo(v->engine_type);

		byte cost_factor = GetVehicleProperty(v, 0x0D, rvi->running_cost_base);
		if (cost_factor == 0) continue;

		cost += cost_factor * _price.running_rail[rvi->running_cost_class];
	} while ((v = GetNextVehicle(v)) != NULL);

	return cost;
}


void Train::Tick()
{
	if (_age_cargo_skip_counter == 0) this->cargo.AgeCargo();

	this->tick_counter++;

	if (IsFrontEngine(this)) {
		this->current_order_time++;

		TrainLocoHandler(this, false);

		/* make sure vehicle wasn't deleted. */
		if (this->type == VEH_TRAIN && IsFrontEngine(this))
			TrainLocoHandler(this, true);
	} else if (IsFreeWagon(this) && HASBITS(this->vehstatus, VS_CRASHED)) {
		/* Delete flooded standalone wagon chain */
		if (++this->u.rail.crash_anim_pos >= 4400) DeleteVehicleChain(this);
	}
}

#define MAX_ACCEPTABLE_DEPOT_DIST 16

static void CheckIfTrainNeedsService(Vehicle *v)
{
	if (_patches.servint_trains == 0 || !VehicleNeedsService(v)) return;
	if (v->IsInDepot()) {
		VehicleServiceInDepot(v);
		return;
	}

	TrainFindDepotData tfdd = FindClosestTrainDepot(v, MAX_ACCEPTABLE_DEPOT_DIST);
	/* Only go to the depot if it is not too far out of our way. */
	if (tfdd.best_length == (uint)-1 || tfdd.best_length > MAX_ACCEPTABLE_DEPOT_DIST) {
		if (v->current_order.type == OT_GOTO_DEPOT) {
			/* If we were already heading for a depot but it has
			 * suddenly moved farther away, we continue our normal
			 * schedule? */
			v->current_order.type = OT_DUMMY;
			v->current_order.flags = 0;
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
		}
		return;
	}

	const Depot* depot = GetDepotByTile(tfdd.tile);

	if (v->current_order.type == OT_GOTO_DEPOT &&
			v->current_order.dest != depot->index &&
			!Chance16(3, 16)) {
		return;
	}

	v->current_order.type = OT_GOTO_DEPOT;
	v->current_order.flags = OFB_NON_STOP;
	v->current_order.dest = depot->index;
	v->dest_tile = tfdd.tile;
	InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
}

void OnNewDay_Train(Vehicle *v)
{
	if ((++v->day_counter & 7) == 0) DecreaseVehicleValue(v);

	if (IsFrontEngine(v)) {
		CheckVehicleBreakdown(v);
		AgeVehicle(v);

		CheckIfTrainNeedsService(v);

		CheckOrders(v);

		/* update destination */
		if (v->current_order.type == OT_GOTO_STATION) {
			TileIndex tile = GetStation(v->current_order.dest)->train_tile;
			if (tile != 0) v->dest_tile = tile;
		}

		if ((v->vehstatus & VS_STOPPED) == 0) {
			/* running costs */
			CommandCost cost(EXPENSES_TRAIN_RUN, v->GetRunningCost() / 364);

			v->profit_this_year -= cost.GetCost() >> 8;

			SubtractMoneyFromPlayerFract(v->owner, cost);

			InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
			InvalidateWindowClasses(WC_TRAINS_LIST);
		}
	} else if (IsTrainEngine(v)) {
		/* Also age engines that aren't front engines */
		AgeVehicle(v);
	}
}

void TrainsYearlyLoop()
{
	Vehicle *v;

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_TRAIN && IsFrontEngine(v)) {
			/* show warning if train is not generating enough income last 2 years (corresponds to a red icon in the vehicle list) */
			if (_patches.train_income_warn && v->owner == _local_player && v->age >= 730 && v->profit_this_year < 0) {
				SetDParam(1, v->profit_this_year);
				SetDParam(0, v->unitnumber);
				AddNewsItem(
					STR_TRAIN_IS_UNPROFITABLE,
					NEWS_FLAGS(NM_SMALL, NF_VIEWPORT|NF_VEHICLE, NT_ADVICE, 0),
					v->index,
					0);
			}

			v->profit_last_year = v->profit_this_year;
			v->profit_this_year = 0;
			InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
		}
	}
}


void InitializeTrains()
{
	_age_cargo_skip_counter = 1;
}

/*
 * Link front and rear multiheaded engines to each other
 * This is done when loading a savegame
 */
void ConnectMultiheadedTrains()
{
	Vehicle *v;

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_TRAIN) {
			v->u.rail.other_multiheaded_part = NULL;
		}
	}

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_TRAIN && IsFrontEngine(v)) {
			Vehicle *u = v;

			BEGIN_ENUM_WAGONS(u) {
				if (u->u.rail.other_multiheaded_part != NULL) continue; // we already linked this one

				if (IsMultiheaded(u)) {
					if (!IsTrainEngine(u)) {
						/* we got a rear car without a front car. We will convert it to a front one */
						SetTrainEngine(u);
						u->spritenum--;
					}

					Vehicle *w;
					for (w = u->Next(); w != NULL && (w->engine_type != u->engine_type || w->u.rail.other_multiheaded_part != NULL); w = GetNextVehicle(w));
					if (w != NULL) {
						/* we found a car to partner with this engine. Now we will make sure it face the right way */
						if (IsTrainEngine(w)) {
							ClearTrainEngine(w);
							w->spritenum++;
						}
						w->u.rail.other_multiheaded_part = u;
						u->u.rail.other_multiheaded_part = w;
					} else {
						/* we got a front car and no rear cars. We will fake this one for forget that it should have been multiheaded */
						ClearMultiheaded(u);
					}
				}
			} END_ENUM_WAGONS(u)
		}
	}
}

/**
 *  Converts all trains to the new subtype format introduced in savegame 16.2
 *  It also links multiheaded engines or make them forget they are multiheaded if no suitable partner is found
 */
void ConvertOldMultiheadToNew()
{
	Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_TRAIN) {
			SetBit(v->subtype, 7); // indicates that it's the old format and needs to be converted in the next loop
		}
	}

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_TRAIN) {
			if (HasBit(v->subtype, 7) && ((v->subtype & ~0x80) == 0 || (v->subtype & ~0x80) == 4)) {
				Vehicle *u = v;

				BEGIN_ENUM_WAGONS(u) {
					const RailVehicleInfo *rvi = RailVehInfo(u->engine_type);

					ClrBit(u->subtype, 7);
					switch (u->subtype) {
						case 0: /* TS_Front_Engine */
							if (rvi->railveh_type == RAILVEH_MULTIHEAD) SetMultiheaded(u);
							SetFrontEngine(u);
							SetTrainEngine(u);
							break;

						case 1: /* TS_Artic_Part */
							u->subtype = 0;
							SetArticulatedPart(u);
							break;

						case 2: /* TS_Not_First */
							u->subtype = 0;
							if (rvi->railveh_type == RAILVEH_WAGON) {
								// normal wagon
								SetTrainWagon(u);
								break;
							}
							if (rvi->railveh_type == RAILVEH_MULTIHEAD && rvi->image_index == u->spritenum - 1) {
								// rear end of a multiheaded engine
								SetMultiheaded(u);
								break;
							}
							if (rvi->railveh_type == RAILVEH_MULTIHEAD) SetMultiheaded(u);
							SetTrainEngine(u);
							break;

						case 4: /* TS_Free_Car */
							u->subtype = 0;
							SetTrainWagon(u);
							SetFreeWagon(u);
							break;
						default: NOT_REACHED(); break;
					}
				} END_ENUM_WAGONS(u)
			}
		}
	}
}
