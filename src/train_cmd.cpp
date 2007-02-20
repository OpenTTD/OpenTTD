/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "bridge_map.h"
#include "debug.h"
#include "functions.h"
#include "gui.h"
#include "station_map.h"
#include "table/strings.h"
#include "map.h"
#include "tile.h"
#include "tunnel_map.h"
#include "vehicle.h"
#include "command.h"
#include "pathfind.h"
#include "npf.h"
#include "station.h"
#include "table/train_cmd.h"
#include "news.h"
#include "engine.h"
#include "player.h"
#include "sound.h"
#include "depot.h"
#include "waypoint.h"
#include "vehicle_gui.h"
#include "train.h"
#include "bridge.h"
#include "newgrf_callbacks.h"
#include "newgrf_engine.h"
#include "newgrf_sound.h"
#include "newgrf_text.h"
#include "direction.h"
#include "yapf/yapf.h"
#include "date.h"
#include "cargotype.h"

static bool TrainCheckIfLineEnds(Vehicle *v);
static void TrainController(Vehicle *v, bool update_image);

static const byte _vehicle_initial_x_fract[4] = {10, 8, 4,  8};
static const byte _vehicle_initial_y_fract[4] = { 8, 4, 8, 10};
static const TrackBits _state_dir_table[4] = { TRACK_BIT_RIGHT, TRACK_BIT_LOWER, TRACK_BIT_LEFT, TRACK_BIT_UPPER };


/** Return the cargo weight multiplier to use for a rail vehicle
 * @param cargo Cargo type to get multiplier for
 * @return Cargo weight multiplier
 */
byte FreightWagonMult(CargoID cargo)
{
	// XXX NewCargos introduces a specific "is freight" flag for this test.
	if (cargo == CT_PASSENGERS || cargo == CT_MAIL) return 1;
	return _patches.freight_trains;
}


/**
 * Recalculates the cached total power of a train. Should be called when the consist is changed
 * @param v First vehicle of the consist.
 */
void TrainPowerChanged(Vehicle* v)
{
	Vehicle* u;
	uint32 power = 0;
	uint32 max_te = 0;

	for (u = v; u != NULL; u = u->next) {
		/* Power is not added for articulated parts */
		if (IsArticulatedPart(u)) continue;

		RailType railtype = (IsLevelCrossingTile(u->tile) ? GetRailTypeCrossing(u->tile) : GetRailType(u->tile));
		bool engine_has_power = HasPowerOnRail(u->u.rail.railtype, railtype);
		bool wagon_has_power  = HasPowerOnRail(v->u.rail.railtype, railtype);

		const RailVehicleInfo *rvi_u = RailVehInfo(u->engine_type);

		if (engine_has_power && rvi_u->power != 0) {
			power += rvi_u->power;
			/* Tractive effort in (tonnes * 1000 * 10 =) N */
			max_te += (u->u.rail.cached_veh_weight * 10000 * rvi_u->tractive_effort) / 256;
		}

		if (HASBIT(u->u.rail.flags, VRF_POWEREDWAGON) && (wagon_has_power)) {
			power += RailVehInfo(u->u.rail.first_engine)->pow_wag_power;
		}
	}

	if (v->u.rail.cached_power != power || v->u.rail.cached_max_te != max_te) {
		v->u.rail.cached_power = power;
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
	Vehicle *u;
	uint32 weight = 0;

	for (u = v; u != NULL; u = u->next) {
		const RailVehicleInfo *rvi = RailVehInfo(u->engine_type);
		uint32 vweight = (GetCargo(u->cargo_type)->weight * u->cargo_count * FreightWagonMult(u->cargo_type)) / 16;

		// Vehicle weight is not added for articulated parts.
		if (!IsArticulatedPart(u)) {
			// vehicle weight is the sum of the weight of the vehicle and the weight of its cargo
			vweight += rvi->weight;

			// powered wagons have extra weight added
			if (HASBIT(u->u.rail.flags, VRF_POWEREDWAGON))
				vweight += RailVehInfo(u->u.rail.first_engine)->pow_wag_weight;
		}

		// consist weight is the sum of the weight of all vehicles in the consist
		weight += vweight;

		// store vehicle weight in cache
		u->u.rail.cached_veh_weight = vweight;
	};

	// store consist weight in cache
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
	const RailVehicleInfo *rvi_v;
	Vehicle *u;
	uint16 max_speed = 0xFFFF;
	EngineID first_engine;

	assert(v->type == VEH_Train);

	assert(IsFrontEngine(v) || IsFreeWagon(v));

	rvi_v = RailVehInfo(v->engine_type);
	first_engine = IsFrontEngine(v) ? v->engine_type : INVALID_ENGINE;
	v->u.rail.cached_total_length = 0;
	v->u.rail.compatible_railtypes = 0;

	for (u = v; u != NULL; u = u->next) {
		const RailVehicleInfo *rvi_u = RailVehInfo(u->engine_type);
		uint16 veh_len;

		// Update the v->first cache. This is faster than having to brute force it later.
		if (u->first == NULL) u->first = v;

		// update the 'first engine'
		u->u.rail.first_engine = (v == u) ? (EngineID)INVALID_ENGINE : first_engine;
		u->u.rail.railtype = rvi_u->railtype;

		if (IsTrainEngine(u)) first_engine = u->engine_type;

		if (rvi_u->visual_effect != 0) {
			u->u.rail.cached_vis_effect = rvi_u->visual_effect;
		} else {
			if (IsTrainWagon(u) || IsArticulatedPart(u)) {
				// Wagons and articulated parts have no effect by default
				u->u.rail.cached_vis_effect = 0x40;
			} else if (rvi_u->engclass == 0) {
				// Steam is offset by -4 units
				u->u.rail.cached_vis_effect = 4;
			} else {
				// Diesel fumes and sparks come from the centre
				u->u.rail.cached_vis_effect = 8;
			}
		}

		if (!IsArticulatedPart(u)) {
			// check if its a powered wagon
			CLRBIT(u->u.rail.flags, VRF_POWEREDWAGON);

			/* Check powered wagon / visual effect callback */
			if (HASBIT(EngInfo(u->engine_type)->callbackmask, CBM_WAGON_POWER)) {
				uint16 callback = GetVehicleCallback(CBID_TRAIN_WAGON_POWER, 0, 0, u->engine_type, u);

				if (callback != CALLBACK_FAILED) u->u.rail.cached_vis_effect = callback;
			}

			if (rvi_v->pow_wag_power != 0 && rvi_u->railveh_type == RAILVEH_WAGON &&
				UsesWagonOverride(u) && (u->u.rail.cached_vis_effect < 0x40)) {
				/* wagon is powered */
				SETBIT(u->u.rail.flags, VRF_POWEREDWAGON); // cache 'powered' status
			}

			/* Do not count powered wagons for the compatible railtypes, as wagons always
			   have railtype normal */
			if (rvi_u->power > 0) {
				v->u.rail.compatible_railtypes |= GetRailTypeInfo(u->u.rail.railtype)->powered_railtypes;
			}

			/* Some electric engines can be allowed to run on normal rail. It happens to all
			 * existing electric engines when elrails are disabled and then re-enabled */
			if (HASBIT(u->u.rail.flags, VRF_EL_ENGINE_ALLOWED_NORMAL_RAIL)) {
				u->u.rail.railtype = RAILTYPE_RAIL;
				u->u.rail.compatible_railtypes |= (1 << RAILTYPE_RAIL);
			}

			// max speed is the minimum of the speed limits of all vehicles in the consist
			if ((rvi_u->railveh_type != RAILVEH_WAGON || _patches.wagon_speed_limits) &&
				rvi_u->max_speed != 0 && !UsesWagonOverride(u))
				max_speed = min(rvi_u->max_speed, max_speed);
		}

		// check the vehicle length (callback)
		veh_len = CALLBACK_FAILED;
		if (HASBIT(EngInfo(u->engine_type)->callbackmask, CBM_VEHICLE_LENGTH)) {
			veh_len = GetVehicleCallback(CBID_TRAIN_VEHICLE_LENGTH, 0, 0, u->engine_type, u);
		}
		if (veh_len == CALLBACK_FAILED) veh_len = rvi_u->shorten_factor;
		veh_len = clamp(veh_len, 0, u->next == NULL ? 7 : 5); // the clamp on vehicles not the last in chain is stricter, as too short wagons can break the 'follow next vehicle' code
		u->u.rail.cached_veh_length = 8 - veh_len;
		v->u.rail.cached_total_length += u->u.rail.cached_veh_length;

	};

	// store consist weight/max speed in cache
	v->u.rail.cached_max_speed = max_speed;

	// recalculate cached weights and power too (we do this *after* the rest, so it is known which wagons are powered and need extra weight added)
	TrainCargoChanged(v);
}

/* These two arrays are used for realistic acceleration. XXX: How should they
 * be interpreted? */
static const byte _curve_neighbours45[8][2] = {
	{7, 1},
	{0, 2},
	{1, 3},
	{2, 4},
	{3, 5},
	{4, 6},
	{5, 7},
	{6, 0},
};

static const byte _curve_neighbours90[8][2] = {
	{6, 2},
	{7, 3},
	{0, 4},
	{1, 5},
	{2, 6},
	{3, 7},
	{4, 0},
	{5, 1},
};

enum AccelType {
	AM_ACCEL,
	AM_BRAKE
};

static bool TrainShouldStop(const Vehicle* v, TileIndex tile)
{
	const Order* o = &v->current_order;
	StationID sid = GetStationIndex(tile);

	assert(v->type == VEH_Train);
	//When does a train drive through a station
	//first we deal with the "new nonstop handling"
	if (_patches.new_nonstop && o->flags & OF_NON_STOP && sid == o->dest) {
		return false;
	}

	if (v->last_station_visited == sid) return false;

	if (sid != o->dest && (o->flags & OF_NON_STOP || _patches.new_nonstop)) {
		return false;
	}

	return true;
}

//new acceleration
static int GetTrainAcceleration(Vehicle *v, bool mode)
{
	const Vehicle *u;
	int num = 0; //number of vehicles, change this into the number of axles later
	int power = 0;
	int mass = 0;
	int max_speed = 2000;
	int area = 120;
	int friction = 35; //[1e-3]
	int drag_coeff = 20; //[1e-4]
	int incl = 0;
	int resistance;
	int speed = v->cur_speed; //[mph]
	int force = 0x3FFFFFFF;
	int pos = 0;
	int lastpos = -1;
	int curvecount[2] = {0, 0};
	int sum = 0;
	int numcurve = 0;
	int max_te = v->u.rail.cached_max_te; // [N]

	speed *= 10;
	speed /= 16;

	//first find the curve speed limit
	for (u = v; u->next != NULL; u = u->next, pos++) {
		Direction dir = u->direction;
		Direction ndir = u->next->direction;
		int i;

		for (i = 0; i < 2; i++) {
			if ( _curve_neighbours45[dir][i] == ndir) {
				curvecount[i]++;
				if (lastpos != -1) {
					numcurve++;
					sum += pos - lastpos;
					if (pos - lastpos == 1) {
						max_speed = 88;
					}
				}
				lastpos = pos;
			}
		}

		//if we have a 90 degree turn, fix the speed limit to 60
		if (_curve_neighbours90[dir][0] == ndir ||
				_curve_neighbours90[dir][1] == ndir) {
			max_speed = 61;
		}
	}

	if (numcurve > 0) sum /= numcurve;

	if ((curvecount[0] != 0 || curvecount[1] != 0) && max_speed > 88) {
		int total = curvecount[0] + curvecount[1];

		if (curvecount[0] == 1 && curvecount[1] == 1) {
			max_speed = 0xFFFF;
		} else if (total > 1) {
			max_speed = 232 - (13 - clamp(sum, 1, 12)) * (13 - clamp(sum, 1, 12));
		}
	}

	max_speed += (max_speed / 2) * v->u.rail.railtype;

	if (IsTileType(v->tile, MP_STATION) && IsFrontEngine(v)) {
		if (TrainShouldStop(v, v->tile)) {
			int station_length = GetStationByTile(v->tile)->GetPlatformLength(v->tile, DirToDiagDir(v->direction));
			int delta_v;

			max_speed = 120;

			delta_v = v->cur_speed / (station_length + 1);
			if (v->max_speed > (v->cur_speed - delta_v))
				max_speed = v->cur_speed - (delta_v / 10);

			max_speed = max(max_speed, 25 * station_length);
		}
	}

	mass = v->u.rail.cached_weight;
	power = v->u.rail.cached_power * 746;
	max_speed = min(max_speed, v->u.rail.cached_max_speed);

	for (u = v; u != NULL; u = u->next) {
		num++;
		drag_coeff += 3;

		if (u->u.rail.track == TRACK_BIT_DEPOT) max_speed = min(max_speed, 61);

		if (HASBIT(u->u.rail.flags, VRF_GOINGUP)) {
			incl += u->u.rail.cached_veh_weight * 60; //3% slope, quite a bit actually
		} else if (HASBIT(u->u.rail.flags, VRF_GOINGDOWN)) {
			incl -= u->u.rail.cached_veh_weight * 60;
		}
	}

	v->max_speed = max_speed;

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

			case RAILTYPE_MAGLEV:
				force = power / 25;
				break;

			default: NOT_REACHED();
		}
	} else {
		//"kickoff" acceleration
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
	uint power = 0;
	uint weight = 0;

	assert(IsFrontEngine(v));

	weight = v->u.rail.cached_weight;
	power = v->u.rail.cached_power;
	v->max_speed = v->u.rail.cached_max_speed;

	assert(weight != 0);

	v->acceleration = clamp(power / weight * 4, 1, 255);
}

int GetTrainImage(const Vehicle* v, Direction direction)
{
	int img = v->spritenum;
	int base;

	if (HASBIT(v->u.rail.flags, VRF_REVERSE_DIRECTION)) direction = ReverseDir(direction);

	if (is_custom_sprite(img)) {
		base = GetCustomVehicleSprite(v, (Direction)(direction + 4 * IS_CUSTOM_SECONDHEAD_SPRITE(img)));
		if (base != 0) return base;
		img = orig_rail_vehicle_info[v->engine_type].image_index;
	}

	base = _engine_sprite_base[img] + ((direction + _engine_sprite_add[img]) & _engine_sprite_and[img]);

	if (v->cargo_count >= v->cargo_cap / 2) base += _wagon_full_adder[img];
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
			img = orig_rail_vehicle_info[engine].image_index;
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
			if (image == 0) img = orig_rail_vehicle_info[engine].image_index;
		}
		if (image == 0) {
			image =
				((6 + _engine_sprite_add[img + 1]) & _engine_sprite_and[img + 1]) +
				_engine_sprite_base[img + 1];
		}
	}
	DrawSprite(image, pal, x, y);
}

uint CountArticulatedParts(EngineID engine_type)
{
	uint16 callback;
	uint i;

	if (!HASBIT(EngInfo(engine_type)->callbackmask, CBM_ARTIC_ENGINE)) return 0;

	for (i = 1; i < 10; i++) {
		callback = GetVehicleCallback(CBID_TRAIN_ARTIC_ENGINE, i, 0, engine_type, NULL);
		if (callback == CALLBACK_FAILED || callback == 0xFF) break;
	}

	return i - 1;
}

static void AddArticulatedParts(Vehicle **vl)
{
	const RailVehicleInfo *rvi_artic;
	EngineID engine_type;
	Vehicle *v = vl[0];
	Vehicle *u = v;
	uint16 callback;
	bool flip_image;
	uint i;

	if (!HASBIT(EngInfo(v->engine_type)->callbackmask, CBM_ARTIC_ENGINE)) return;

	for (i = 1; i < 10; i++) {
		callback = GetVehicleCallback(CBID_TRAIN_ARTIC_ENGINE, i, 0, v->engine_type, v);
		if (callback == CALLBACK_FAILED || callback == 0xFF) return;

		/* Attempt to use pre-allocated vehicles until they run out. This can happen
		 * if the callback returns different values depending on the cargo type. */
		u->next = vl[i];
		if (u->next == NULL) u->next = AllocateVehicle();
		if (u->next == NULL) return;

		u = u->next;

		engine_type = GB(callback, 0, 7);
		flip_image = HASBIT(callback, 7);
		rvi_artic = RailVehInfo(engine_type);

		// get common values from first engine
		u->direction = v->direction;
		u->owner = v->owner;
		u->tile = v->tile;
		u->x_pos = v->x_pos;
		u->y_pos = v->y_pos;
		u->z_pos = v->z_pos;
		u->z_height = v->z_height;
		u->u.rail.track = v->u.rail.track;
		u->u.rail.railtype = v->u.rail.railtype;
		u->build_year = v->build_year;
		u->vehstatus = v->vehstatus & ~VS_STOPPED;
		u->u.rail.first_engine = v->engine_type;

		// get more settings from rail vehicle info
		u->spritenum = rvi_artic->image_index;
		if (flip_image) u->spritenum++;
		u->cargo_type = rvi_artic->cargo_type;
		u->cargo_subtype = 0;
		u->cargo_cap = rvi_artic->capacity;
		u->max_speed = 0;
		u->max_age = 0;
		u->engine_type = engine_type;
		u->value = 0;
		u->type = VEH_Train;
		u->subtype = 0;
		SetArticulatedPart(u);
		u->cur_image = 0xAC2;
		u->random_bits = VehicleRandomBits();

		VehiclePositionChanged(u);
	}
}

static int32 CmdBuildRailWagon(EngineID engine, TileIndex tile, uint32 flags)
{
	int32 value;
	const RailVehicleInfo *rvi;
	uint num_vehicles;

	SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);

	rvi = RailVehInfo(engine);
	value = (rvi->base_cost * _price.build_railwagon) >> 8;

	num_vehicles = 1 + CountArticulatedParts(engine);

	if (!(flags & DC_QUERY_COST)) {
		Vehicle *vl[11]; // Allow for wagon and upto 10 artic parts.
		Vehicle* v;
		int x;
		int y;

		memset(&vl, 0, sizeof(vl));

		if (!AllocateVehicles(vl, num_vehicles))
			return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);

		if (flags & DC_EXEC) {
			Vehicle *u, *w;
			DiagDirection dir;

			v = vl[0];
			v->spritenum = rvi->image_index;

			u = NULL;

			FOR_ALL_VEHICLES(w) {
				if (w->type == VEH_Train && w->tile == tile &&
				    IsFreeWagon(w) && w->engine_type == engine) {
					u = GetLastVehicleInChain(w);
					break;
				}
			}

			v->engine_type = engine;

			dir = GetRailDepotDirection(tile);

			v->direction = DiagDirToDir(dir);
			v->tile = tile;

			x = TileX(tile) * TILE_SIZE | _vehicle_initial_x_fract[dir];
			y = TileY(tile) * TILE_SIZE | _vehicle_initial_y_fract[dir];

			v->x_pos = x;
			v->y_pos = y;
			v->z_pos = GetSlopeZ(x,y);
			v->owner = _current_player;
			v->z_height = 6;
			v->u.rail.track = TRACK_BIT_DEPOT;
			v->vehstatus = VS_HIDDEN | VS_DEFPAL;

			v->subtype = 0;
			SetTrainWagon(v);
			if (u != NULL) {
				u->next = v;
			} else {
				SetFreeWagon(v);
				InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
			}

			v->cargo_type = rvi->cargo_type;
			v->cargo_subtype = 0;
			v->cargo_cap = rvi->capacity;
			v->value = value;
//			v->day_counter = 0;

			v->u.rail.railtype = rvi->railtype;

			v->build_year = _cur_year;
			v->type = VEH_Train;
			v->cur_image = 0xAC2;
			v->random_bits = VehicleRandomBits();

			AddArticulatedParts(vl);

			_new_vehicle_id = v->index;

			VehiclePositionChanged(v);
			TrainConsistChanged(GetFirstVehicleInChain(v));

			InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
			if (IsLocalPlayer()) {
				InvalidateAutoreplaceWindow(VEH_Train); // updates the replace Train window
			}
			GetPlayer(_current_player)->num_engines[engine]++;
		}
	}

	return value;
}

// Move all free vehicles in the depot to the train
static void NormalizeTrainVehInDepot(const Vehicle* u)
{
	const Vehicle* v;

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Train && IsFreeWagon(v) &&
				v->tile == u->tile &&
				v->u.rail.track == TRACK_BIT_DEPOT) {
			if (CmdFailed(DoCommand(0, v->index | (u->index << 16), 1, DC_EXEC,
					CMD_MOVE_RAIL_VEHICLE)))
				break;
		}
	}
}

static int32 EstimateTrainCost(const RailVehicleInfo* rvi)
{
	return rvi->base_cost * (_price.build_railvehicle >> 3) >> 5;
}

static void AddRearEngineToMultiheadedTrain(Vehicle* v, Vehicle* u, bool building)
{
	u->direction = v->direction;
	u->owner = v->owner;
	u->tile = v->tile;
	u->x_pos = v->x_pos;
	u->y_pos = v->y_pos;
	u->z_pos = v->z_pos;
	u->z_height = 6;
	u->u.rail.track = TRACK_BIT_DEPOT;
	u->vehstatus = v->vehstatus & ~VS_STOPPED;
	u->subtype = 0;
	SetMultiheaded(u);
	u->spritenum = v->spritenum + 1;
	u->cargo_type = v->cargo_type;
	u->cargo_subtype = v->cargo_subtype;
	u->cargo_cap = v->cargo_cap;
	u->u.rail.railtype = v->u.rail.railtype;
	if (building) v->next = u;
	u->engine_type = v->engine_type;
	u->build_year = v->build_year;
	if (building) v->value >>= 1;
	u->value = v->value;
	u->type = VEH_Train;
	u->cur_image = 0xAC2;
	u->random_bits = VehicleRandomBits();
	VehiclePositionChanged(u);
}

/** Build a railroad vehicle.
 * @param tile tile of the depot where rail-vehicle is built
 * @param p1 engine type id
 * @param p2 bit 0 when set, the train will get number 0, otherwise it will get a free number
 *           bit 1 prevents any free cars from being added to the train
 */
int32 CmdBuildRailVehicle(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	const RailVehicleInfo *rvi;
	int value;
	Vehicle *v;
	UnitID unit_num;
	uint num_vehicles;

	/* Check if the engine-type is valid (for the player) */
	if (!IsEngineBuildable(p1, VEH_Train, _current_player)) return_cmd_error(STR_ENGINE_NOT_BUILDABLE);

	/* Check if the train is actually being built in a depot belonging
	 * to the player. Doesn't matter if only the cost is queried */
	if (!(flags & DC_QUERY_COST)) {
		if (!IsTileDepotType(tile, TRANSPORT_RAIL)) return CMD_ERROR;
		if (!IsTileOwner(tile, _current_player)) return CMD_ERROR;
	}

	SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);

	rvi = RailVehInfo(p1);

	/* Check if depot and new engine uses the same kind of tracks */
	/* We need to see if the engine got power on the tile to avoid eletric engines in non-electric depots */
	if (!HasPowerOnRail(rvi->railtype, GetRailType(tile))) return CMD_ERROR;

	if (rvi->railveh_type == RAILVEH_WAGON) return CmdBuildRailWagon(p1, tile, flags);

	value = EstimateTrainCost(rvi);

	num_vehicles = (rvi->railveh_type == RAILVEH_MULTIHEAD) ? 2 : 1;
	num_vehicles += CountArticulatedParts(p1);

	if (!(flags & DC_QUERY_COST)) {
		Vehicle *vl[12]; // Allow for upto 10 artic parts and dual-heads

		memset(&vl, 0, sizeof(vl));

		if (!AllocateVehicles(vl, num_vehicles))
			return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);

		v = vl[0];

		unit_num = HASBIT(p2, 0) ? 0 : GetFreeUnitNumber(VEH_Train);
		if (unit_num > _patches.max_trains)
			return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);

		if (flags & DC_EXEC) {
			DiagDirection dir = GetRailDepotDirection(tile);
			int x = TileX(tile) * TILE_SIZE + _vehicle_initial_x_fract[dir];
			int y = TileY(tile) * TILE_SIZE + _vehicle_initial_y_fract[dir];

			v->unitnumber = unit_num;
			v->direction = DiagDirToDir(dir);
			v->tile = tile;
			v->owner = _current_player;
			v->x_pos = x;
			v->y_pos = y;
			v->z_pos = GetSlopeZ(x,y);
			v->z_height = 6;
			v->u.rail.track = TRACK_BIT_DEPOT;
			v->vehstatus = VS_HIDDEN | VS_STOPPED | VS_DEFPAL;
			v->spritenum = rvi->image_index;
			v->cargo_type = rvi->cargo_type;
			v->cargo_subtype = 0;
			v->cargo_cap = rvi->capacity;
			v->max_speed = rvi->max_speed;
			v->value = value;
			v->last_station_visited = INVALID_STATION;
			v->dest_tile = 0;

			v->engine_type = p1;

			const Engine *e = GetEngine(p1);
			v->reliability = e->reliability;
			v->reliability_spd_dec = e->reliability_spd_dec;
			v->max_age = e->lifelength * 366;

			v->string_id = STR_SV_TRAIN_NAME;
			v->u.rail.railtype = rvi->railtype;
			_new_vehicle_id = v->index;

			v->service_interval = _patches.servint_trains;
			v->date_of_last_service = _date;
			v->build_year = _cur_year;
			v->type = VEH_Train;
			v->cur_image = 0xAC2;
			v->random_bits = VehicleRandomBits();

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
				AddArticulatedParts(vl);
			}

			TrainConsistChanged(v);
			UpdateTrainAcceleration(v);

			if (!HASBIT(p2, 1)) { // check if the cars should be added to the new vehicle
				NormalizeTrainVehInDepot(v);
			}

			InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
			RebuildVehicleLists();
			InvalidateWindow(WC_COMPANY, v->owner);
			if (IsLocalPlayer())
				InvalidateAutoreplaceWindow(VEH_Train); // updates the replace Train window

			GetPlayer(_current_player)->num_engines[p1]++;
		}
	}

	return value;
}


/* Check if all the wagons of the given train are in a depot, returns the
 * number of cars (including loco) then. If not it returns -1 */
int CheckTrainInDepot(const Vehicle *v, bool needs_to_be_stopped)
{
	int count;
	TileIndex tile = v->tile;

	/* check if stopped in a depot */
	if (!IsTileDepotType(tile, TRANSPORT_RAIL) || v->cur_speed != 0) return -1;

	count = 0;
	for (; v != NULL; v = v->next) {
		/* This count is used by the depot code to determine the number of engines
		 * in the consist. Exclude articulated parts so that autoreplacing to
		 * engines with more articulated parts than before works correctly.
		 *
		 * Also skip counting rear ends of multiheaded engines */
		if (!IsArticulatedPart(v) && !(!IsTrainEngine(v) && IsMultiheaded(v))) count++;
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
	return (CheckTrainInDepot(v, false) > 0);
}

/**
 * Unlink a rail wagon from the consist.
 * @param v Vehicle to remove.
 * @param first The first vehicle of the consist.
 * @return The first vehicle of the consist.
 */
static Vehicle *UnlinkWagon(Vehicle *v, Vehicle *first)
{
	Vehicle *u;

	// unlinking the first vehicle of the chain?
	if (v == first) {
		v = GetNextVehicle(v);
		if (v == NULL) return NULL;

		if (IsTrainWagon(v)) SetFreeWagon(v);

		return v;
	}

	for (u = first; GetNextVehicle(u) != v; u = GetNextVehicle(u)) {}
	GetLastEnginePart(u)->next = GetNextVehicle(v);
	return first;
}

static Vehicle *FindGoodVehiclePos(const Vehicle *src)
{
	Vehicle *dst;
	EngineID eng = src->engine_type;
	TileIndex tile = src->tile;

	FOR_ALL_VEHICLES(dst) {
		if (dst->type == VEH_Train && IsFreeWagon(dst) && dst->tile == tile) {
			// check so all vehicles in the line have the same engine.
			Vehicle *v = dst;

			while (v->engine_type == eng) {
				v = v->next;
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
	UnlinkWagon(v, GetFirstVehicleInChain(v));
	if (dest == NULL) return;

	v->next = dest->next;
	dest->next = v;
	ClearFreeWagon(v);
	ClearFrontEngine(v);
}

/*
 * move around on the train so rear engines are placed correctly according to the other engines
 * always call with the front engine
 */
static void NormaliseTrainConsist(Vehicle *v)
{
	Vehicle *u;

	if (IsFreeWagon(v)) return;

	assert(IsFrontEngine(v));

	for (; v != NULL; v = GetNextVehicle(v)) {
		if (!IsMultiheaded(v) || !IsTrainEngine(v)) continue;

		/* make sure that there are no free cars before next engine */
		for (u = v; u->next != NULL && !IsTrainEngine(u->next); u = u->next);

		if (u == v->u.rail.other_multiheaded_part) continue;
		AddWagonToConsist(v->u.rail.other_multiheaded_part, u);
	}
}

/** Move a rail vehicle around inside the depot.
 * @param tile unused
 * @param p1 various bitstuffed elements
 * - p1 (bit  0 - 15) source vehicle index
 * - p1 (bit 16 - 31) what wagon to put the source wagon AFTER, XXX - INVALID_VEHICLE to make a new line
 * @param p2 (bit 0) move all vehicles following the source vehicle
 */
int32 CmdMoveRailVehicle(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	VehicleID s = GB(p1, 0, 16);
	VehicleID d = GB(p1, 16, 16);
	Vehicle *src, *dst, *src_head, *dst_head;

	if (!IsValidVehicleID(s)) return CMD_ERROR;

	src = GetVehicle(s);

	if (src->type != VEH_Train) return CMD_ERROR;

	// if nothing is selected as destination, try and find a matching vehicle to drag to.
	if (d == INVALID_VEHICLE) {
		dst = IsTrainEngine(src) ? NULL : FindGoodVehiclePos(src);
	} else {
		dst = GetVehicle(d);
	}

	// if an articulated part is being handled, deal with its parent vehicle
	while (IsArticulatedPart(src)) src = GetPrevVehicleInChain(src);
	if (dst != NULL) {
		while (IsArticulatedPart(dst)) dst = GetPrevVehicleInChain(dst);
	}

	// don't move the same vehicle..
	if (src == dst) return 0;

	/* the player must be the owner */
	if (!CheckOwnership(src->owner) || (dst != NULL && !CheckOwnership(dst->owner)))
		return CMD_ERROR;

	/* locate the head of the two chains */
	src_head = GetFirstVehicleInChain(src);
	dst_head = NULL;
	if (dst != NULL) {
		dst_head = GetFirstVehicleInChain(dst);
		// Now deal with articulated part of destination wagon
		dst = GetLastEnginePart(dst);
	}

	if (dst != NULL && IsMultiheaded(dst) && !IsTrainEngine(dst) && IsTrainWagon(src)) {
		/* We are moving a wagon to the rear part of a multiheaded engine */
		if (dst->next == NULL) {
			/* It's the last one, so we will add the wagon just before the rear engine */
			dst = GetPrevVehicleInChain(dst);
			/* Now if the vehicle we want to link to is the vehicle itself, drop out */
			if (dst == src) return CMD_ERROR;
			// if dst is NULL, it means that dst got a rear multiheaded engine as first engine. We can't use that
			if (dst == NULL) return CMD_ERROR;
		} else {
			/* there are more units on this train, so we will add the wagon after the next one*/
			dst = dst->next;
		}
	}

	if (IsTrainEngine(src) && dst_head != NULL) {
		/* we need to make sure that we didn't place it between a pair of multiheaded engines */
		Vehicle *u, *engine = NULL;

		for (u = dst_head; u != NULL; u = u->next) {
			if (IsTrainEngine(u) && IsMultiheaded(u) && u->u.rail.other_multiheaded_part != NULL) {
				engine = u;
			}
			if (engine != NULL && engine->u.rail.other_multiheaded_part == u) {
				engine = NULL;
			}
			if (u == dst) {
				if (engine != NULL) dst = engine->u.rail.other_multiheaded_part;
				break;
			}
		}
	}

	if (IsMultiheaded(src) && !IsTrainEngine(src)) return_cmd_error(STR_REAR_ENGINE_FOLLOW_FRONT_ERROR);

	// when moving all wagons, we can't have the same src_head and dst_head
	if (HASBIT(p2, 0) && src_head == dst_head) return 0;

	{
		int src_len = 0;
		int max_len = _patches.mammoth_trains ? 100 : 9;

		// check if all vehicles in the source train are stopped inside a depot.
		src_len = CheckTrainStoppedInDepot(src_head);
		if (src_len < 0) return_cmd_error(STR_881A_TRAINS_CAN_ONLY_BE_ALTERED);

		// check the destination row if the source and destination aren't the same.
		if (src_head != dst_head) {
			int dst_len = 0;

			if (dst_head != NULL) {
				// check if all vehicles in the dest train are stopped.
				dst_len = CheckTrainStoppedInDepot(dst_head);
				if (dst_len < 0) return_cmd_error(STR_881A_TRAINS_CAN_ONLY_BE_ALTERED);

				assert(dst_head->tile == src_head->tile);
			}

			// We are moving between rows, so only count the wagons from the source
			// row that are being moved.
			if (HASBIT(p2, 0)) {
				const Vehicle *u;
				for (u = src_head; u != src && u != NULL; u = GetNextVehicle(u))
					src_len--;
			} else {
				// If moving only one vehicle, just count that.
				src_len = 1;
			}

			if (src_len + dst_len > max_len) {
				// Abort if we're adding too many wagons to a train.
				if (dst_head != NULL && IsFrontEngine(dst_head)) return_cmd_error(STR_8819_TRAIN_TOO_LONG);
				// Abort if we're making a train on a new row.
				if (dst_head == NULL && IsTrainEngine(src)) return_cmd_error(STR_8819_TRAIN_TOO_LONG);
			}
		} else {
			// Abort if we're creating a new train on an existing row.
			if (src_len > max_len && src == src_head && IsTrainEngine(GetNextVehicle(src_head)))
				return_cmd_error(STR_8819_TRAIN_TOO_LONG);
		}
	}

	// moving a loco to a new line?, then we need to assign a unitnumber.
	if (dst == NULL && !IsFrontEngine(src) && IsTrainEngine(src)) {
		UnitID unit_num = GetFreeUnitNumber(VEH_Train);
		if (unit_num > _patches.max_trains)
			return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);

		if (flags & DC_EXEC) src->unitnumber = unit_num;
	}

	if (dst_head != NULL) {
		/* Check NewGRF Callback 0x1D */
		uint16 callback = GetVehicleCallbackParent(CBID_TRAIN_ALLOW_WAGON_ATTACH, 0, 0, dst_head->engine_type, src, dst_head);
		if (callback != CALLBACK_FAILED) {
			if (callback == 0xFD) return_cmd_error(STR_INCOMPATIBLE_RAIL_TYPES);
			if (callback < 0xFD) {
				StringID error = GetGRFStringID(GetEngineGRFID(dst_head->engine_type), 0xD000 + callback);
				return_cmd_error(error);
			}
		}
	}

	/* do it? */
	if (flags & DC_EXEC) {
		/* clear the ->first cache */
		{
			Vehicle *u;

			for (u = src_head; u != NULL; u = u->next) u->first = NULL;
			for (u = dst_head; u != NULL; u = u->next) u->first = NULL;
		}

		if (HASBIT(p2, 0)) {
			// unlink ALL wagons
			if (src != src_head) {
				Vehicle *v = src_head;
				while (GetNextVehicle(v) != src) v = GetNextVehicle(v);
				GetLastEnginePart(v)->next = NULL;
			} else {
				InvalidateWindowData(WC_VEHICLE_DEPOT, src_head->tile); // We removed a line
				src_head = NULL;
			}
		} else {
			// if moving within the same chain, dont use dst_head as it may get invalidated
			if (src_head == dst_head) dst_head = NULL;
			// unlink single wagon from linked list
			src_head = UnlinkWagon(src, src_head);
			GetLastEnginePart(src)->next = NULL;
		}

		if (dst == NULL) {
			/* We make a new line in the depot, so we know already that we invalidate the window data */
			InvalidateWindowData(WC_VEHICLE_DEPOT, src->tile);

			// move the train to an empty line. for locomotives, we set the type to TS_Front. for wagons, 4.
			if (IsTrainEngine(src)) {
				if (!IsFrontEngine(src)) {
					// setting the type to 0 also involves setting up the orders field.
					SetFrontEngine(src);
					assert(src->orders == NULL);
					src->num_orders = 0;
				}
			} else {
				SetFreeWagon(src);
			}
			dst_head = src;
		} else {
			if (IsFrontEngine(src)) {
				// the vehicle was previously a loco. need to free the order list and delete vehicle windows etc.
				DeleteWindowById(WC_VEHICLE_VIEW, src->index);
				DeleteVehicleOrders(src);
			}

			if (IsFrontEngine(src) || IsFreeWagon(src)) {
				InvalidateWindowData(WC_VEHICLE_DEPOT, src->tile);
				ClearFrontEngine(src);
				ClearFreeWagon(src);
				src->unitnumber = 0; // doesn't occupy a unitnumber anymore.
			}

			// link in the wagon(s) in the chain.
			{
				Vehicle *v;

				for (v = src; GetNextVehicle(v) != NULL; v = GetNextVehicle(v));
				GetLastEnginePart(v)->next = dst->next;
			}
			dst->next = src;
		}
		if (src->u.rail.other_multiheaded_part != NULL) {
			if (src->u.rail.other_multiheaded_part == src_head) {
				src_head = src_head->next;
			}
			AddWagonToConsist(src->u.rail.other_multiheaded_part, src);
			// previous line set the front engine to the old front. We need to clear that
			src->u.rail.other_multiheaded_part->first = NULL;
		}

		if (HASBIT(p2, 0) && src_head != NULL && src_head != src) {
			/* if we stole a rear multiheaded engine, we better give it back to the front end */
			Vehicle *engine = NULL, *u;
			for (u = src_head; u != NULL; u = u->next) {
				if (IsMultiheaded(u)) {
					if (IsTrainEngine(u)) {
						engine = u;
						continue;
					}
					/* we got the rear engine to match with the front one */
					engine = NULL;
				}
			}
			if (engine != NULL && engine->u.rail.other_multiheaded_part != NULL) {
				AddWagonToConsist(engine->u.rail.other_multiheaded_part, engine);
				// previous line set the front engine to the old front. We need to clear that
				engine->u.rail.other_multiheaded_part->first = NULL;
			}
		}

		/* If there is an engine behind first_engine we moved away, it should become new first_engine
		 * To do this, CmdMoveRailVehicle must be called once more
		 * we can't loop forever here because next time we reach this line we will have a front engine */
		if (src_head != NULL && !IsFrontEngine(src_head) && IsTrainEngine(src_head)) {
			CmdMoveRailVehicle(0, flags, src_head->index | (INVALID_VEHICLE << 16), 1);
			src_head = NULL; // don't do anything more to this train since the new call will do it
		}

		if (src_head != NULL) {
			NormaliseTrainConsist(src_head);
			TrainConsistChanged(src_head);
			if (IsFrontEngine(src_head)) {
				UpdateTrainAcceleration(src_head);
				InvalidateWindow(WC_VEHICLE_DETAILS, src_head->index);
				/* Update the refit button and window */
				InvalidateWindow(WC_VEHICLE_REFIT, src_head->index);
				InvalidateWindowWidget(WC_VEHICLE_VIEW, src_head->index, 12);
			}
			/* Update the depot window */
			InvalidateWindow(WC_VEHICLE_DEPOT, src_head->tile);
		};

		if (dst_head != NULL) {
			NormaliseTrainConsist(dst_head);
			TrainConsistChanged(dst_head);
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

	return 0;
}

/** Start/Stop a train.
 * @param tile unused
 * @param p1 train to start/stop
 * @param p2 unused
 */
int32 CmdStartStopTrain(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;
	uint16 callback;

	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_Train || !CheckOwnership(v->owner)) return CMD_ERROR;

	/* Check if this train can be started/stopped. The callback will fail or
	 * return 0xFF if it can. */
	callback = GetVehicleCallback(CBID_VEHICLE_START_STOP_CHECK, 0, 0, v->engine_type, v);
	if (callback != CALLBACK_FAILED && callback != 0xFF) {
		StringID error = GetGRFStringID(GetEngineGRFID(v->engine_type), 0xD000 + callback);
		return_cmd_error(error);
	}

	if (v->vehstatus & VS_STOPPED && v->u.rail.cached_power == 0) return_cmd_error(STR_TRAIN_START_NO_CATENARY);

	if (flags & DC_EXEC) {
		if (v->vehstatus & VS_STOPPED && v->u.rail.track == TRACK_BIT_DEPOT) {
			DeleteVehicleNews(p1, STR_8814_TRAIN_IS_WAITING_IN_DEPOT);
		}

		v->u.rail.days_since_order_progr = 0;
		v->vehstatus ^= VS_STOPPED;
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
	}
	return 0;
}

/** Sell a (single) train wagon/engine.
 * @param tile unused
 * @param p1 the wagon/engine index
 * @param p2 the selling mode
 * - p2 = 0: only sell the single dragged wagon/engine (and any belonging rear-engines)
 * - p2 = 1: sell the vehicle and all vehicles following it in the chain
             if the wagon is dragged, don't delete the possibly belonging rear-engine to some front
 * - p2 = 2: when selling attached locos, rearrange all vehicles after it to separate lines;
 *           all wagons of the same type will go on the same line. Used by the AI currently
 */
int32 CmdSellRailWagon(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v, *tmp, *first;
	Vehicle *new_f = NULL;
	int32 cost = 0;

	if (!IsValidVehicleID(p1) || p2 > 2) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_Train || !CheckOwnership(v->owner)) return CMD_ERROR;

	SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);

	while (IsArticulatedPart(v)) v = GetPrevVehicleInChain(v);
	first = GetFirstVehicleInChain(v);

	// make sure the vehicle is stopped in the depot
	if (CheckTrainStoppedInDepot(first) < 0) {
		return_cmd_error(STR_881A_TRAINS_CAN_ONLY_BE_ALTERED);
	}

	if (IsMultiheaded(v) && !IsTrainEngine(v)) return_cmd_error(STR_REAR_ENGINE_FOLLOW_FRONT_ERROR);

	if (flags & DC_EXEC) {
		if (v == first && IsFrontEngine(first)) {
			DeleteWindowById(WC_VEHICLE_VIEW, first->index);
		}
		InvalidateWindow(WC_VEHICLE_DEPOT, first->tile);
		RebuildVehicleLists();
	}

	switch (p2) {
		case 0: case 2: { /* Delete given wagon */
			bool switch_engine = false;    // update second wagon to engine?
			byte ori_subtype = v->subtype; // backup subtype of deleted wagon in case DeleteVehicle() changes

			/* 1. Delete the engine, if it is dualheaded also delete the matching
			 * rear engine of the loco (from the point of deletion onwards) */
			Vehicle *rear = (IsMultiheaded(v) &&
				IsTrainEngine(v)) ? v->u.rail.other_multiheaded_part : NULL;

			if (rear != NULL) {
				cost -= rear->value;
				if (flags & DC_EXEC) {
					UnlinkWagon(rear, first);
					DeleteDepotHighlightOfVehicle(rear);
					DeleteVehicle(rear);
				}
			}

			/* 2. We are selling the first engine, some special action might be required
			 * here, so take attention */
			if ((flags & DC_EXEC) && v == first) {
				new_f = GetNextVehicle(first);

				/* 2.1 If the first wagon is sold, update the first-> pointers to NULL */
				for (tmp = first; tmp != NULL; tmp = tmp->next) tmp->first = NULL;

				/* 2.2 If there are wagons present after the deleted front engine, check
         * if the second wagon (which will be first) is an engine. If it is one,
         * promote it as a new train, retaining the unitnumber, orders */
				if (new_f != NULL) {
					if (IsTrainEngine(new_f)) {
						switch_engine = true;
						/* Copy important data from the front engine */
						new_f->unitnumber = first->unitnumber;
						new_f->current_order = first->current_order;
						new_f->cur_order_index = first->cur_order_index;
						new_f->orders = first->orders;
						if (first->prev_shared != NULL) {
							first->prev_shared->next_shared = new_f;
							new_f->prev_shared = first->prev_shared;
						}

						if (first->next_shared != NULL) {
							first->next_shared->prev_shared = new_f;
							new_f->next_shared = first->next_shared;
						}

						new_f->num_orders = first->num_orders;
						first->orders = NULL; // XXX - to not to delete the orders */
						if (IsLocalPlayer()) ShowTrainViewWindow(new_f);
					}
				}
			}

			/* 3. Delete the requested wagon */
			cost -= v->value;
			if (flags & DC_EXEC) {
				first = UnlinkWagon(v, first);
				DeleteDepotHighlightOfVehicle(v);
				DeleteVehicle(v);

				/* 4 If the second wagon was an engine, update it to front_engine
					* which UnlinkWagon() has changed to TS_Free_Car */
				if (switch_engine) SetFrontEngine(first);

				/* 5. If the train still exists, update its acceleration, window, etc. */
				if (first != NULL) {
					NormaliseTrainConsist(first);
					TrainConsistChanged(first);
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
				if (p2 == 2 && HASBIT(ori_subtype, Train_Front)) {
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
			for (; v != NULL; v = tmp) {
				tmp = GetNextVehicle(v);

				if (IsMultiheaded(v)) {
					if (IsTrainEngine(v)) {
						/* We got a front engine of a multiheaded set. Now we will sell the rear end too */
						Vehicle *rear = v->u.rail.other_multiheaded_part;

						if (rear != NULL) {
							cost -= rear->value;
							if (flags & DC_EXEC) {
								first = UnlinkWagon(rear, first);
								DeleteDepotHighlightOfVehicle(rear);
								DeleteVehicle(rear);
							}
						}
					} else if (v->u.rail.other_multiheaded_part != NULL) {
						/* The front to this engine is earlier in this train. Do nothing */
						continue;
					}
				}

				cost -= v->value;
				if (flags & DC_EXEC) {
					first = UnlinkWagon(v, first);
					DeleteDepotHighlightOfVehicle(v);
					DeleteVehicle(v);
				}
			}

			/* 3. If it is still a valid train after selling, update its acceleration and cached values */
			if (flags & DC_EXEC && first != NULL) {
				NormaliseTrainConsist(first);
				TrainConsistChanged(first);
				if (IsFrontEngine(first)) UpdateTrainAcceleration(first);
				InvalidateWindow(WC_VEHICLE_DETAILS, first->index);
				InvalidateWindow(WC_VEHICLE_REFIT, first->index);
			}
		} break;
	}
	return cost;
}

static void UpdateTrainDeltaXY(Vehicle *v, Direction direction)
{
#define MKIT(a,b,c,d) ((a&0xFF)<<24) | ((b&0xFF)<<16) | ((c&0xFF)<<8) | ((d&0xFF)<<0)
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

	v->x_offs        = GB(x,  0, 8);
	v->y_offs        = GB(x,  8, 8);
	v->sprite_width  = GB(x, 16, 8);
	v->sprite_height = GB(x, 24, 8);
}

static void UpdateVarsAfterSwap(Vehicle *v)
{
	UpdateTrainDeltaXY(v, v->direction);
	v->cur_image = GetTrainImage(v, v->direction);
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
	byte flag1, flag2;

	flag1 = *swap_flag1;
	flag2 = *swap_flag2;

	/* Clear the flags */
	CLRBIT(*swap_flag1, VRF_GOINGUP);
	CLRBIT(*swap_flag1, VRF_GOINGDOWN);
	CLRBIT(*swap_flag2, VRF_GOINGUP);
	CLRBIT(*swap_flag2, VRF_GOINGDOWN);

	/* Reverse the rail-flags (if needed) */
	if (HASBIT(flag1, VRF_GOINGUP)) {
		SETBIT(*swap_flag2, VRF_GOINGDOWN);
	} else if (HASBIT(flag1, VRF_GOINGDOWN)) {
		SETBIT(*swap_flag2, VRF_GOINGUP);
	}
	if (HASBIT(flag2, VRF_GOINGUP)) {
		SETBIT(*swap_flag1, VRF_GOINGDOWN);
	} else if (HASBIT(flag2, VRF_GOINGDOWN)) {
		SETBIT(*swap_flag1, VRF_GOINGUP);
	}
}

static void ReverseTrainSwapVeh(Vehicle *v, int l, int r)
{
	Vehicle *a, *b;

	/* locate vehicles to swap */
	for (a = v; l != 0; l--) a = a->next;
	for (b = v; r != 0; r--) b = b->next;

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

/* Check if the vehicle is a train and is on the tile we are testing */
static void *TestTrainOnCrossing(Vehicle *v, void *data)
{
	if (v->tile != *(const TileIndex*)data || v->type != VEH_Train) return NULL;
	return v;
}

static void DisableTrainCrossing(TileIndex tile)
{
	if (IsLevelCrossingTile(tile) &&
			VehicleFromPos(tile, &tile, TestTrainOnCrossing) == NULL && // empty?
			IsCrossingBarred(tile)) {
		UnbarCrossing(tile);
		MarkTileDirtyByTile(tile);
	}
}

/**
 * Advances wagons for train reversing, needed for variable length wagons.
 * Needs to be called once before the train is reversed, and once after it.
 * @param v First vehicle in chain
 * @param before Set to true for the call before reversing, false otherwise
 */
static void AdvanceWagons(Vehicle *v, bool before)
{
	Vehicle* base;
	Vehicle* first;
	int length;

	base = v;
	first = base->next;
	length = CountVehiclesInChain(v);

	while (length > 2) {
		Vehicle* last;
		int differential;
		int i;

		// find pairwise matching wagon
		// start<>end, start+1<>end-1, ...
		last = first;
		for (i = length - 3; i > 0; i--) last = last->next;

		differential = last->u.rail.cached_veh_length - base->u.rail.cached_veh_length;
		if (before) differential *= -1;

		if (differential > 0) {
			Vehicle* tempnext;

			// disconnect last car to make sure only this subset moves
			tempnext = last->next;
			last->next = NULL;

			for (i = 0; i < differential; i++) TrainController(first, false);

			last->next = tempnext;
		}

		base = first;
		first = first->next;
		length -= 2;
	}
}


static void ReverseTrainDirection(Vehicle *v)
{
	int l = 0, r = -1;
	Vehicle *u;

	if (IsTileDepotType(v->tile, TRANSPORT_RAIL)) {
		InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
	}

	/* Check if we were approaching a rail/road-crossing */
	{
		TileIndex tile = v->tile;
		DiagDirection dir = DirToDiagDir(v->direction);

		/* Determine the diagonal direction in which we will exit this tile */
		if (!(v->direction & 1) && v->u.rail.track != _state_dir_table[dir]) {
			dir = ChangeDiagDir(dir, DIAGDIRDIFF_90LEFT);
		}
		/* Calculate next tile */
		tile += TileOffsByDiagDir(dir);

		/* Check if the train left a rail/road-crossing */
		DisableTrainCrossing(tile);
	}

	// count number of vehicles
	u = v;
	do r++; while ( (u = u->next) != NULL );

	AdvanceWagons(v, true);

	/* swap start<>end, start+1<>end-1, ... */
	do {
		ReverseTrainSwapVeh(v, l++, r--);
	} while (l <= r);

	AdvanceWagons(v, false);

	if (IsTileDepotType(v->tile, TRANSPORT_RAIL)) {
		InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
	}

	CLRBIT(v->u.rail.flags, VRF_REVERSING);
}

/** Reverse train.
 * @param tile unused
 * @param p1 train to reverse
 * @param p2 if true, reverse a unit in a train (needs to be in a depot)
 */
int32 CmdReverseTrainDirection(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;

	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_Train || !CheckOwnership(v->owner)) return CMD_ERROR;

	if (p2) {
		// turn a single unit around
		Vehicle *front;

		if (IsMultiheaded(v) || HASBIT(EngInfo(v->engine_type)->callbackmask, CBM_ARTIC_ENGINE)) {
			return_cmd_error(STR_ONLY_TURN_SINGLE_UNIT);
		}

		front = GetFirstVehicleInChain(v);
		// make sure the vehicle is stopped in the depot
		if (CheckTrainStoppedInDepot(front) < 0) {
			return_cmd_error(STR_881A_TRAINS_CAN_ONLY_BE_ALTERED);
		}

		if (flags & DC_EXEC) {
			TOGGLEBIT(v->u.rail.flags, VRF_REVERSE_DIRECTION);
			InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
			InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
		}
	} else {
		//turn the whole train around
		if (v->u.rail.crash_anim_pos != 0 || v->breakdown_ctr != 0) return CMD_ERROR;

		if (flags & DC_EXEC) {
			if (_patches.realistic_acceleration && v->cur_speed != 0) {
				TOGGLEBIT(v->u.rail.flags, VRF_REVERSING);
			} else {
				v->cur_speed = 0;
				SetLastSpeed(v, 0);
				ReverseTrainDirection(v);
			}
		}
	}
	return 0;
}

/** Force a train through a red signal
 * @param tile unused
 * @param p1 train to ignore the red signal
 * @param p2 unused
 */
int32 CmdForceTrainProceed(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;

	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_Train || !CheckOwnership(v->owner)) return CMD_ERROR;

	if (flags & DC_EXEC) v->u.rail.force_proceed = 0x50;

	return 0;
}

/** Refits a train to the specified cargo type.
 * @param tile unused
 * @param p1 vehicle ID of the train to refit
 * param p2 various bitstuffed elements
 * - p2 = (bit 0-7) - the new cargo type to refit to
 * - p2 = (bit 8-15) - the new cargo subtype to refit to
 */
int32 CmdRefitRailVehicle(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	CargoID new_cid = GB(p2, 0, 8);
	byte new_subtype = GB(p2, 8, 8);
	Vehicle *v;
	int32 cost;
	uint num;

	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_Train || !CheckOwnership(v->owner)) return CMD_ERROR;
	if (CheckTrainStoppedInDepot(v) < 0) return_cmd_error(STR_TRAIN_MUST_BE_STOPPED);

	/* Check cargo */
	if (new_cid > NUM_CARGO) return CMD_ERROR;

	SET_EXPENSES_TYPE(EXPENSES_TRAIN_RUN);

	cost = 0;
	num = 0;

	do {
		/* XXX: We also refit all the attached wagons en-masse if they
		 * can be refitted. This is how TTDPatch does it.  TODO: Have
		 * some nice [Refit] button near each wagon. --pasky */
		if (!CanRefitTo(v->engine_type, new_cid)) continue;

		if (v->cargo_cap != 0) {
			const RailVehicleInfo *rvi = RailVehInfo(v->engine_type);
			uint16 amount = CALLBACK_FAILED;

			if (HASBIT(EngInfo(v->engine_type)->callbackmask, CBM_REFIT_CAPACITY)) {
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
			};

			if (amount != 0) {
				if (new_cid != v->cargo_type) {
					cost += GetRefitCost(v->engine_type);
				}

				num += amount;
				if (flags & DC_EXEC) {
					v->cargo_count = (v->cargo_type == new_cid) ? min(amount, v->cargo_count) : 0;
					v->cargo_type = new_cid;
					v->cargo_cap = amount;
					v->cargo_subtype = new_subtype;
					InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
					InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
					RebuildVehicleLists();
				}
			}
		}
	} while ((v = v->next) != NULL);

	_returned_refit_capacity = num;

	/* Update the train's cached variables */
	if (flags & DC_EXEC) TrainConsistChanged(GetFirstVehicleInChain(GetVehicle(p1)));

	return cost;
}

typedef struct TrainFindDepotData {
	uint best_length;
	TileIndex tile;
	PlayerID owner;
	/**
	 * true if reversing is necessary for the train to get to this depot
	 * This value is unused when new depot finding and NPF are both disabled
	 */
	bool reverse;
} TrainFindDepotData;

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

// returns the tile of a depot to goto to. The given vehicle must not be
// crashed!
static TrainFindDepotData FindClosestTrainDepot(Vehicle *v, int max_distance)
{
	TrainFindDepotData tfdd;
	TileIndex tile = v->tile;

	assert(!(v->vehstatus & VS_CRASHED));

	tfdd.owner = v->owner;
	tfdd.best_length = (uint)-1;
	tfdd.reverse = false;

	if (IsTileDepotType(tile, TRANSPORT_RAIL)){
		tfdd.tile = tile;
		tfdd.best_length = 0;
		return tfdd;
	}

	if (_patches.yapf.rail_use_yapf) {
		bool found = YapfFindNearestRailDepotTwoWay(v, max_distance, NPF_INFINITE_PENALTY, &tfdd.tile, &tfdd.reverse);
		tfdd.best_length = found ? max_distance / 2 : -1; // some fake distance or NOT_FOUND
	} else if (_patches.new_pathfinding_all) {
		NPFFoundTargetData ftd;
		Vehicle* last = GetLastVehicleInChain(v);
		Trackdir trackdir = GetVehicleTrackdir(v);
		Trackdir trackdir_rev = ReverseTrackdir(GetVehicleTrackdir(last));

		assert (trackdir != INVALID_TRACKDIR);
		ftd = NPFRouteToDepotBreadthFirstTwoWay(v->tile, trackdir, last->tile, trackdir_rev, TRANSPORT_RAIL, v->owner, v->u.rail.compatible_railtypes, NPF_INFINITE_PENALTY);
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
		// search in the forward direction first.
		DiagDirection i;

		i = DirToDiagDir(v->direction);
		if (!(v->direction & 1) && v->u.rail.track != _state_dir_table[i]) {
			i = ChangeDiagDir(i, DIAGDIRDIFF_90LEFT);
		}
		NewTrainPathfind(tile, 0, v->u.rail.compatible_railtypes, i, (NTPEnumProc*)NtpCallbFindDepot, &tfdd);
		if (tfdd.best_length == (uint)-1){
			tfdd.reverse = true;
			// search in backwards direction
			i = ReverseDiagDir(DirToDiagDir(v->direction));
			if (!(v->direction & 1) && v->u.rail.track != _state_dir_table[i]) {
				i = ChangeDiagDir(i, DIAGDIRDIFF_90LEFT);
			}
			NewTrainPathfind(tile, 0, v->u.rail.compatible_railtypes, i, (NTPEnumProc*)NtpCallbFindDepot, &tfdd);
		}
	}

	return tfdd;
}

/** Send a train to a depot
 * @param tile unused
 * @param p1 train to send to the depot
 * @param p2 various bitmasked elements
 * - p2 bit 0-3 - DEPOT_ flags (see vehicle.h)
 * - p2 bit 8-10 - VLW flag (for mass goto depot)
 */
int32 CmdSendTrainToDepot(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;
	TrainFindDepotData tfdd;

	if (p2 & DEPOT_MASS_SEND) {
		/* Mass goto depot requested */
		if (!ValidVLWFlags(p2 & VLW_MASK)) return CMD_ERROR;
		return SendAllVehiclesToDepot(VEH_Train, flags, p2 & DEPOT_SERVICE, _current_player, (p2 & VLW_MASK), p1);
	}

	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_Train || !CheckOwnership(v->owner)) return CMD_ERROR;

	if (v->vehstatus & VS_CRASHED) return CMD_ERROR;

	if (v->current_order.type == OT_GOTO_DEPOT) {
		if (!!(p2 & DEPOT_SERVICE) == HASBIT(v->current_order.flags, OFB_HALT_IN_DEPOT)) {
			/* We called with a different DEPOT_SERVICE setting.
			 * Now we change the setting to apply the new one and let the vehicle head for the same depot.
			 * Note: the if is (true for requesting service == true for ordered to stop in depot)          */
			if (flags & DC_EXEC) {
				TOGGLEBIT(v->current_order.flags, OFB_HALT_IN_DEPOT);
				InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
			}
			return 0;
		}

		if (p2 & DEPOT_DONT_CANCEL) return CMD_ERROR; // Requested no cancelation of depot orders
		if (flags & DC_EXEC) {
			if (HASBIT(v->current_order.flags, OFB_PART_OF_ORDERS)) {
				v->u.rail.days_since_order_progr = 0;
				v->cur_order_index++;
			}

			v->current_order.type = OT_DUMMY;
			v->current_order.flags = 0;
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
		}
		return 0;
	}

	/* check if at a standstill (not stopped only) in a depot
	 * the check is down here to make it possible to alter stop/service for trains entering the depot */
	if (IsTileDepotType(v->tile, TRANSPORT_RAIL) && v->cur_speed == 0) return CMD_ERROR;

	tfdd = FindClosestTrainDepot(v, 0);
	if (tfdd.best_length == (uint)-1) return_cmd_error(STR_883A_UNABLE_TO_FIND_ROUTE_TO);

	if (flags & DC_EXEC) {
		v->dest_tile = tfdd.tile;
		v->current_order.type = OT_GOTO_DEPOT;
		v->current_order.flags = OF_NON_STOP;
		if (!(p2 & DEPOT_SERVICE)) SETBIT(v->current_order.flags, OFB_HALT_IN_DEPOT);
		v->current_order.dest = GetDepotByTile(tfdd.tile)->index;
		v->current_order.refit_cargo = CT_INVALID;
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
		/* If there is no depot in front, reverse automatically */
		if (tfdd.reverse)
			DoCommand(v->tile, v->index, 0, DC_EXEC, CMD_REVERSE_TRAIN_DIRECTION);
	}

	return 0;
}


void OnTick_Train(void)
{
	_age_cargo_skip_counter = (_age_cargo_skip_counter == 0) ? 184 : (_age_cargo_skip_counter - 1);
}

static const int8 _vehicle_smoke_pos[8] = {
	1, 1, 1, 0, -1, -1, -1, 0
};

static void HandleLocomotiveSmokeCloud(const Vehicle* v)
{
	const Vehicle* u;
	bool sound = false;

	if (v->vehstatus & VS_TRAIN_SLOWING || v->load_unload_time_rem != 0 || v->cur_speed < 2)
		return;

	u = v;

	do {
		const RailVehicleInfo *rvi = RailVehInfo(v->engine_type);
		int effect_offset = GB(v->u.rail.cached_vis_effect, 0, 4) - 8;
		byte effect_type = GB(v->u.rail.cached_vis_effect, 4, 2);
		bool disable_effect = HASBIT(v->u.rail.cached_vis_effect, 6);
		int x, y;

		// no smoke?
		if ((rvi->railveh_type == RAILVEH_WAGON && effect_type == 0) ||
				disable_effect ||
				rvi->railtype > RAILTYPE_ELECTRIC ||
				v->vehstatus & VS_HIDDEN) {
			continue;
		}

		// No smoke in depots or tunnels
		if (IsTileDepotType(v->tile, TRANSPORT_RAIL) || IsTunnelTile(v->tile)) continue;

		// No sparks for electric vehicles on nonelectrified tracks
		if (!HasPowerOnRail(v->u.rail.railtype, GetTileRailType(v->tile, TrackdirToTrack(GetVehicleTrackdir(v))))) continue;

		if (effect_type == 0) {
			// Use default effect type for engine class.
			effect_type = rvi->engclass;
		} else {
			effect_type--;
		}

		x = _vehicle_smoke_pos[v->direction] * effect_offset;
		y = _vehicle_smoke_pos[(v->direction + 2) % 8] * effect_offset;

		if (HASBIT(v->u.rail.flags, VRF_REVERSE_DIRECTION)) {
			x = -x;
			y = -y;
		}

		switch (effect_type) {
		case 0:
			// steam smoke.
			if (GB(v->tick_counter, 0, 4) == 0) {
				CreateEffectVehicleRel(v, x, y, 10, EV_STEAM_SMOKE);
				sound = true;
			}
			break;

		case 1:
			// diesel smoke
			if (u->cur_speed <= 40 && CHANCE16(15, 128)) {
				CreateEffectVehicleRel(v, 0, 0, 10, EV_DIESEL_SMOKE);
				sound = true;
			}
			break;

		case 2:
			// blue spark
			if (GB(v->tick_counter, 0, 2) == 0 && CHANCE16(1, 45)) {
				CreateEffectVehicleRel(v, 0, 0, 10, EV_ELECTRIC_SPARK);
				sound = true;
			}
			break;
		}
	} while ((v = v->next) != NULL);

	if (sound) PlayVehicleSound(u, VSE_TRAIN_EFFECT);
}

static void TrainPlayLeaveStationSound(const Vehicle* v)
{
	static const SoundFx sfx[] = {
		SND_04_TRAIN,
		SND_0A_TRAIN_HORN,
		SND_0A_TRAIN_HORN
	};

	EngineID engtype = v->engine_type;

	if (PlayVehicleSound(v, VSE_START)) return;

	switch (RailVehInfo(engtype)->railtype) {
		case RAILTYPE_RAIL:
		case RAILTYPE_ELECTRIC:
			SndPlayVehicleFx(sfx[RailVehInfo(engtype)->engclass], v);
			break;

		case RAILTYPE_MONO: SndPlayVehicleFx(SND_47_MAGLEV_2, v); break;
		case RAILTYPE_MAGLEV: SndPlayVehicleFx(SND_41_MAGLEV, v); break;
		default: NOT_REACHED();
	}
}

static bool CheckTrainStayInDepot(Vehicle *v)
{
	Vehicle *u;

	// bail out if not all wagons are in the same depot or not in a depot at all
	for (u = v; u != NULL; u = u->next) {
		if (u->u.rail.track != TRACK_BIT_DEPOT || u->tile != v->tile) return false;
	}

	// if the train got no power, then keep it in the depot
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

		if (UpdateSignalsOnSegment(v->tile, DirToDiagDir(v->direction))) {
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

	UpdateTrainDeltaXY(v, v->direction);
	v->cur_image = GetTrainImage(v, v->direction);
	VehiclePositionChanged(v);
	UpdateSignalsOnSegment(v->tile, DirToDiagDir(v->direction));
	UpdateTrainAcceleration(v);
	InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);

	return false;
}

/* Check for station tiles */
typedef struct TrainTrackFollowerData {
	TileIndex dest_coords;
	StationID station_index; // station index we're heading for
	uint best_bird_dist;
	uint best_track_dist;
	TrackdirByte best_track;
} TrainTrackFollowerData;

static bool NtpCallbFindStation(TileIndex tile, TrainTrackFollowerData *ttfd, Trackdir track, uint length)
{
	// heading for nowhere?
	if (ttfd->dest_coords == 0) return false;

	// did we reach the final station?
	if ((ttfd->station_index == INVALID_STATION && tile == ttfd->dest_coords) || (
				IsTileType(tile, MP_STATION) &&
				IsRailwayStation(tile) &&
				GetStationIndex(tile) == ttfd->station_index
			)) {
		/* We do not check for dest_coords if we have a station_index,
		 * because in that case the dest_coords are just an
		 * approximation of where the station is */
		// found station
		ttfd->best_track = track;
		return true;
	} else {
		uint dist;

		// didn't find station, keep track of the best path so far.
		dist = DistanceManhattan(tile, ttfd->dest_coords);
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
	{ 0, 9, 2, 9 }, // track 1
	{ 9, 1, 9, 3 }, // track 2
	{ 9, 0, 3, 9 }, // track upper
	{ 1, 9, 9, 2 }, // track lower
	{ 3, 2, 9, 9 }, // track left
	{ 9, 9, 1, 0 }, // track right
};

static const byte _pick_track_table[6] = {1, 3, 2, 2, 0, 0};

/* choose a track */
static Track ChooseTrainTrack(Vehicle* v, TileIndex tile, DiagDirection enterdir, TrackBits tracks)
{
	TrainTrackFollowerData fd;
	Track best_track;
	// pathfinders are able to tell that route was only 'guessed'
	bool path_not_found = false;

#ifdef PF_BENCHMARK
	TIC()
#endif

	assert((tracks & ~0x3F) == 0);

	/* quick return in case only one possible track is available */
	if (KILL_FIRST_BIT(tracks) == 0) return FindFirstTrack(tracks);

	if (_patches.yapf.rail_use_yapf) {
		Trackdir trackdir = YapfChooseRailTrack(v, tile, enterdir, tracks, &path_not_found);
		if (trackdir != INVALID_TRACKDIR) {
			best_track = TrackdirToTrack(trackdir);
		} else {
			best_track = FindFirstTrack(tracks);
		}
	} else if (_patches.new_pathfinding_all) { /* Use a new pathfinding for everything */
		void* perf = NpfBeginInterval();
		int time = 0;

		NPFFindStationOrTileData fstd;
		NPFFoundTargetData ftd;
		Trackdir trackdir;

		NPFFillWithOrderData(&fstd, v);
		/* The enterdir for the new tile, is the exitdir for the old tile */
		trackdir = GetVehicleTrackdir(v);
		assert(trackdir != 0xff);

		ftd = NPFRouteToStationOrTile(tile - TileOffsByDiagDir(enterdir), trackdir, &fstd, TRANSPORT_RAIL, v->owner, v->u.rail.compatible_railtypes);

		if (ftd.best_trackdir == 0xff) {
			/* We are already at our target. Just do something */
			//TODO: maybe display error?
			//TODO: go straight ahead if possible?
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

		time = NpfEndInterval(perf);
		DEBUG(yapf, 4, "[NPFT] %d us - %d rounds - %d open - %d closed -- ", time, 0, _aystar_stats_open_size, _aystar_stats_closed_size);
	} else {
		void* perf = NpfBeginInterval();
		int time = 0;

		FillWithStationData(&fd, v);

		/* New train pathfinding */
		fd.best_bird_dist = (uint)-1;
		fd.best_track_dist = (uint)-1;
		fd.best_track = INVALID_TRACKDIR;

		NewTrainPathfind(tile - TileOffsByDiagDir(enterdir), v->dest_tile,
			v->u.rail.compatible_railtypes, enterdir, (NTPEnumProc*)NtpCallbFindStation, &fd);

		// check whether the path was found or only 'guessed'
		if (fd.best_bird_dist != 0) path_not_found = true;

		if (fd.best_track == 0xff) {
			// blaha
			best_track = FindFirstTrack(tracks);
		} else {
			best_track = TrackdirToTrack(fd.best_track);
		}

		time = NpfEndInterval(perf);
		DEBUG(yapf, 4, "[NTPT] %d us - %d rounds - %d open - %d closed -- ", time, 0, 0, 0);
	}
	// handle "path not found" state
	if (path_not_found) {
		// PF didn't find the route
		if (!HASBIT(v->u.rail.flags, VRF_NO_PATH_TO_DESTINATION)) {
			// it is first time the problem occurred, set the "path not found" flag
			SETBIT(v->u.rail.flags, VRF_NO_PATH_TO_DESTINATION);
			// and notify user about the event
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
		// route found, is the train marked with "path not found" flag?
		if (HASBIT(v->u.rail.flags, VRF_NO_PATH_TO_DESTINATION)) {
			// clear the flag as the PF's problem was solved
			CLRBIT(v->u.rail.flags, VRF_NO_PATH_TO_DESTINATION);
			// can we also delete the "News" item somehow?
		}
	}

#ifdef PF_BENCHMARK
	TOC("PF time = ", 1)
#endif

	return best_track;
}


static bool CheckReverseTrain(Vehicle *v)
{
	TrainTrackFollowerData fd;
	int i, r;
	int best_track;
	uint best_bird_dist  = 0;
	uint best_track_dist = 0;
	uint reverse, reverse_best;

	if (_opt.diff.line_reverse_mode != 0 ||
			v->u.rail.track == TRACK_BIT_DEPOT || v->u.rail.track == TRACK_BIT_WORMHOLE ||
			!(v->direction & 1))
		return false;

	FillWithStationData(&fd, v);

	best_track = -1;
	reverse_best = reverse = 0;

	assert(v->u.rail.track);

	i = _search_directions[FIND_FIRST_BIT(v->u.rail.track)][DirToDiagDir(v->direction)];

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

		ftd = NPFRouteToStationOrTileTwoWay(v->tile, trackdir, last->tile, trackdir_rev, &fstd, TRANSPORT_RAIL, v->owner, v->u.rail.compatible_railtypes);
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
				r = GB(Random(), 0, 8);
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
	const Order *order;
	bool at_waypoint = false;

	switch (v->current_order.type) {
		case OT_GOTO_DEPOT:
			if (!(v->current_order.flags & OF_PART_OF_ORDERS)) return false;
			if ((v->current_order.flags & OF_SERVICE_IF_NEEDED) &&
					!VehicleNeedsService(v)) {
				v->cur_order_index++;
			}
			break;

		case OT_LOADING:
		case OT_LEAVESTATION:
			return false;

		default: break;
	}

	// check if we've reached the waypoint?
	if (v->current_order.type == OT_GOTO_WAYPOINT && v->tile == v->dest_tile) {
		v->cur_order_index++;
		at_waypoint = true;
	}

	// check if we've reached a non-stop station while TTDPatch nonstop is enabled..
	if (_patches.new_nonstop &&
			v->current_order.flags & OF_NON_STOP &&
			IsTileType(v->tile, MP_STATION) &&
			v->current_order.dest == GetStationIndex(v->tile)) {
		v->cur_order_index++;
	}

	// Get the current order
	if (v->cur_order_index >= v->num_orders) v->cur_order_index = 0;

	order = GetVehicleOrder(v, v->cur_order_index);

	// If no order, do nothing.
	if (order == NULL) {
		v->current_order.type = OT_NOTHING;
		v->current_order.flags = 0;
		v->dest_tile = 0;
		return false;
	}

	// If it is unchanged, keep it.
	if (order->type  == v->current_order.type &&
			order->flags == v->current_order.flags &&
			order->dest  == v->current_order.dest)
		return false;

	// Otherwise set it, and determine the destination tile.
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

	return !at_waypoint && CheckReverseTrain(v);
}

static void MarkTrainDirty(Vehicle *v)
{
	do {
		v->cur_image = GetTrainImage(v, v->direction);
		MarkAllViewportsDirty(v->left_coord, v->top_coord, v->right_coord + 1, v->bottom_coord + 1);
	} while ((v = v->next) != NULL);
}

static void HandleTrainLoading(Vehicle *v, bool mode)
{
	switch (v->current_order.type) {
		case OT_LOADING: {
			if (mode) return;

			// don't mark the train as lost if we're loading on the final station.
			if (v->current_order.flags & OF_NON_STOP) {
				v->u.rail.days_since_order_progr = 0;
			}

			if (--v->load_unload_time_rem) return;

			if (CanFillVehicle(v) && (
						v->current_order.flags & OF_FULL_LOAD ||
						(_patches.gradual_loading && !HASBIT(v->load_status, LS_LOADING_FINISHED))
					)) {
				v->u.rail.days_since_order_progr = 0; /* Prevent a train lost message for full loading trains */
				SET_EXPENSES_TYPE(EXPENSES_TRAIN_INC);
				if (LoadUnloadVehicle(v, false)) {
					InvalidateWindow(WC_TRAINS_LIST, v->owner);
					MarkTrainDirty(v);

					// need to update acceleration and cached values since the goods on the train changed.
					TrainCargoChanged(v);
					UpdateTrainAcceleration(v);
				}
				return;
			}

			TrainPlayLeaveStationSound(v);

			Order b = v->current_order;
			v->LeaveStation();

			// If this was not the final order, don't remove it from the list.
			if (!(b.flags & OF_NON_STOP)) return;
			break;
		}

		case OT_DUMMY: break;

		default: return;
	}

	v->u.rail.days_since_order_progr = 0;
	v->cur_order_index++;
	InvalidateVehicleOrder(v);
}

static int UpdateTrainSpeed(Vehicle *v)
{
	uint spd;
	uint accel;

	if (v->vehstatus & VS_STOPPED || HASBIT(v->u.rail.flags, VRF_REVERSING)) {
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

	spd = v->subspeed + accel * 2;
	v->subspeed = (byte)spd;
	{
		int tempmax = v->max_speed;
		if (v->cur_speed > v->max_speed)
			tempmax = v->cur_speed - (v->cur_speed / 10) - 1;
		v->cur_speed = spd = clamp(v->cur_speed + ((int)spd >> 8), 0, tempmax);
	}

	if (!(v->direction & 1)) spd = spd * 3 >> 2;

	spd += v->progress;
	v->progress = (byte)spd;
	return (spd >> 8);
}

static void TrainEnterStation(Vehicle *v, StationID station)
{
	Station *st;
	uint32 flags;

	v->last_station_visited = station;

	/* check if a train ever visited this station before */
	st = GetStation(station);
	if (!(st->had_vehicle_of_type & HVOT_TRAIN)) {
		st->had_vehicle_of_type |= HVOT_TRAIN;
		SetDParam(0, st->index);
		flags = (v->owner == _local_player) ? NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_VEHICLE, NT_ARRIVAL_PLAYER, 0) : NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_VEHICLE, NT_ARRIVAL_OTHER, 0);
		AddNewsItem(
			STR_8801_CITIZENS_CELEBRATE_FIRST,
			flags,
			v->index,
			0
		);
	}

	// Did we reach the final destination?
	if (v->current_order.type == OT_GOTO_STATION &&
			v->current_order.dest == station) {
		// Yeah, keep the load/unload flags
		// Non Stop now means if the order should be increased.
		v->BeginLoading();
		v->current_order.flags &= OF_FULL_LOAD | OF_UNLOAD | OF_TRANSFER;
		v->current_order.flags |= OF_NON_STOP;
	} else {
		// No, just do a simple load
		v->BeginLoading();
		v->current_order.flags = 0;
	}
	v->current_order.dest = 0;

	SET_EXPENSES_TYPE(EXPENSES_TRAIN_INC);
	if (LoadUnloadVehicle(v, true) != 0) {
		InvalidateWindow(WC_TRAINS_LIST, v->owner);
		TrainCargoChanged(v);
		UpdateTrainAcceleration(v);
	}
	MarkTrainDirty(v);
	InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
}

static byte AfterSetTrainPos(Vehicle *v, bool new_tile)
{
	byte new_z, old_z;

	// need this hint so it returns the right z coordinate on bridges.
	new_z = GetSlopeZ(v->x_pos, v->y_pos);

	old_z = v->z_pos;
	v->z_pos = new_z;

	if (new_tile) {
		CLRBIT(v->u.rail.flags, VRF_GOINGUP);
		CLRBIT(v->u.rail.flags, VRF_GOINGDOWN);

		if (new_z != old_z) {
			TileIndex tile = TileVirtXY(v->x_pos, v->y_pos);

			// XXX workaround, whole UP/DOWN detection needs overhaul
			if (!IsTunnelTile(tile)) {
				SETBIT(v->u.rail.flags, (new_z > old_z) ? VRF_GOINGUP : VRF_GOINGDOWN);
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

static Direction GetNewVehicleDirection(const Vehicle *v, int x, int y)
{
	uint offs = (y - v->y_pos + 1) * 4 + (x - v->x_pos + 1);
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
			HASBIT(
				v->u.rail.compatible_railtypes,
				IsTileType(tile, MP_STREET) ? GetRailTypeCrossing(tile) : GetRailType(tile)
			)
		);
}

typedef struct {
	byte small_turn, large_turn;
	byte z_up; // fraction to remove when moving up
	byte z_down; // fraction to remove when moving down
} RailtypeSlowdownParams;

static const RailtypeSlowdownParams _railtype_slowdown[] = {
	// normal accel
	{256 / 4, 256 / 2, 256 / 4, 2}, // normal
	{256 / 4, 256 / 2, 256 / 4, 2}, // electrified
	{256 / 4, 256 / 2, 256 / 4, 2}, // monorail
	{0,       256 / 2, 256 / 4, 2}, // maglev
};

/* Modify the speed of the vehicle due to a turn */
static void AffectSpeedByDirChange(Vehicle* v, Direction new_dir)
{
	DirDiff diff;
	const RailtypeSlowdownParams *rsp;

	if (_patches.realistic_acceleration) return;

	diff = DirDifference(v->direction, new_dir);
	if (diff == DIRDIFF_SAME) return;

	rsp = &_railtype_slowdown[v->u.rail.railtype];
	v->cur_speed -= (diff == DIRDIFF_45RIGHT || diff == DIRDIFF_45LEFT ? rsp->small_turn : rsp->large_turn) * v->cur_speed >> 8;
}

/* Modify the speed of the vehicle due to a change in altitude */
static void AffectSpeedByZChange(Vehicle *v, byte old_z)
{
	const RailtypeSlowdownParams *rsp;
	if (old_z == v->z_pos || _patches.realistic_acceleration) return;

	rsp = &_railtype_slowdown[v->u.rail.railtype];

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
		UpdateSignalsOnSegment(tile, _otherside_signal_directions[i]);
	}
}


typedef struct TrainCollideChecker {
	const Vehicle *v;
	const Vehicle *v_skip;
} TrainCollideChecker;

static void *FindTrainCollideEnum(Vehicle *v, void *data)
{
	const TrainCollideChecker* tcc = (TrainCollideChecker*)data;

	if (v != tcc->v &&
			v != tcc->v_skip &&
			v->type == VEH_Train &&
			v->u.rail.track != TRACK_BIT_DEPOT &&
			myabs(v->z_pos - tcc->v->z_pos) <= 6 &&
			myabs(v->x_pos - tcc->v->x_pos) < 6 &&
			myabs(v->y_pos - tcc->v->y_pos) < 6) {
		return v;
	} else {
		return NULL;
	}
}

static void SetVehicleCrashed(Vehicle *v)
{
	Vehicle *u;

	if (v->u.rail.crash_anim_pos != 0) return;

	v->u.rail.crash_anim_pos++;

	u = v;
	BEGIN_ENUM_WAGONS(v)
		v->vehstatus |= VS_CRASHED;
	END_ENUM_WAGONS(v)

	InvalidateWindowWidget(WC_VEHICLE_VIEW, u->index, STATUS_BAR);
}

static uint CountPassengersInTrain(const Vehicle* v)
{
	uint num = 0;
	BEGIN_ENUM_WAGONS(v)
		if (v->cargo_type == CT_PASSENGERS) num += v->cargo_count;
	END_ENUM_WAGONS(v)
	return num;
}

/*
 * Checks whether the specified train has a collision with another vehicle. If
 * so, destroys this vehicle, and the other vehicle if its subtype has TS_Front.
 * Reports the incident in a flashy news item, modifies station ratings and
 * plays a sound.
 */
static void CheckTrainCollision(Vehicle *v)
{
	TrainCollideChecker tcc;
	Vehicle *coll;
	Vehicle *realcoll;
	uint num;

	/* can't collide in depot */
	if (v->u.rail.track == TRACK_BIT_DEPOT) return;

	assert(v->u.rail.track == TRACK_BIT_WORMHOLE || TileVirtXY(v->x_pos, v->y_pos) == v->tile);

	tcc.v = v;
	tcc.v_skip = v->next;

	/* find colliding vehicle */
	realcoll = (Vehicle*)VehicleFromPos(TileVirtXY(v->x_pos, v->y_pos), &tcc, FindTrainCollideEnum);
	if (realcoll == NULL) return;

	coll = GetFirstVehicleInChain(realcoll);

	/* it can't collide with its own wagons */
	if (v == coll ||
			(v->u.rail.track == TRACK_BIT_WORMHOLE && (v->direction & 2) != (realcoll->direction & 2)))
		return;

	//two drivers + passangers killed in train v
	num = 2 + CountPassengersInTrain(v);
	if (!(coll->vehstatus & VS_CRASHED))
		//two drivers + passangers killed in train coll (if it was not crashed already)
		num += 2 + CountPassengersInTrain(coll);

	SetVehicleCrashed(v);
	if (IsFrontEngine(coll)) SetVehicleCrashed(coll);

	SetDParam(0, num);
	AddNewsItem(STR_8868_TRAIN_CRASH_DIE_IN_FIREBALL,
		NEWS_FLAGS(NM_THIN, NF_VIEWPORT | NF_VEHICLE, NT_ACCIDENT, 0),
		v->index,
		0
	);

	ModifyStationRatingAround(v->tile, v->owner, -160, 30);
	SndPlayVehicleFx(SND_13_BIG_CRASH, v);
}

typedef struct VehicleAtSignalData {
	TileIndex tile;
	Direction direction;
} VehicleAtSignalData;

static void *CheckVehicleAtSignal(Vehicle *v, void *data)
{
	const VehicleAtSignalData* vasd = (VehicleAtSignalData*)data;

	if (v->type == VEH_Train && IsFrontEngine(v) && v->tile == vasd->tile) {
		DirDiff diff = ChangeDirDiff(DirDifference(v->direction, vasd->direction), DIRDIFF_90RIGHT);

		if (diff == DIRDIFF_90RIGHT || (v->cur_speed <= 5 && diff <= DIRDIFF_REVERSE)) return v;
	}
	return NULL;
}

static void TrainController(Vehicle *v, bool update_image)
{
	Vehicle *prev;
	GetNewVehiclePosResult gp;
	uint32 r, tracks, ts;
	Trackdir i;
	DiagDirection enterdir;
	Direction dir;
	Direction newdir;
	Direction chosen_dir;
	TrackBits chosen_track;
	byte old_z;

	/* For every vehicle after and including the given vehicle */
	for (prev = GetPrevVehicleInChain(v); v != NULL; prev = v, v = v->next) {
		BeginVehicleMove(v);

		if (v->u.rail.track != TRACK_BIT_WORMHOLE) {
			/* Not inside tunnel */
			if (GetNewVehiclePos(v, &gp)) {
				/* Staying in the old tile */
				if (v->u.rail.track == TRACK_BIT_DEPOT) {
					/* Inside depot */
					gp.x = v->x_pos;
					gp.y = v->y_pos;
				} else {
					/* Not inside depot */

					if (IsFrontEngine(v) && !TrainCheckIfLineEnds(v)) return;

					r = VehicleEnterTile(v, gp.new_tile, gp.x, gp.y);
					if (HASBIT(r, VETS_CANNOT_ENTER)) {
						goto invalid_rail;
					}
					if (HASBIT(r, VETS_ENTERED_STATION)) {
						TrainEnterStation(v, r >> VETS_STATION_ID_OFFSET);
						return;
					}

					if (v->current_order.type == OT_LEAVESTATION) {
						v->current_order.type = OT_NOTHING;
						v->current_order.flags = 0;
						InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
					}
				}
			} else {
				/* A new tile is about to be entered. */

				TrackBits bits;
				/* Determine what direction we're entering the new tile from */
				dir = GetNewVehicleDirectionByTile(gp.new_tile, gp.old_tile);
				enterdir = DirToDiagDir(dir);
				assert(IsValidDiagDirection(enterdir));

				/* Get the status of the tracks in the new tile and mask
				 * away the bits that aren't reachable. */
				ts = GetTileTrackStatus(gp.new_tile, TRANSPORT_RAIL) & _reachable_tracks[enterdir];

				/* Combine the from & to directions.
				 * Now, the lower byte contains the track status, and the byte at bit 16 contains
				 * the signal status. */
				tracks = ts | (ts >> 8);
				bits = (TrackBits)(tracks & TRACK_BIT_MASK);
				if ((_patches.new_pathfinding_all || _patches.yapf.rail_use_yapf) && _patches.forbid_90_deg && prev == NULL) {
					/* We allow wagons to make 90 deg turns, because forbid_90_deg
					 * can be switched on halfway a turn */
					bits &= ~TrackCrossesTracks(FindFirstTrack(v->u.rail.track));
				}

				if (bits == TRACK_BIT_NONE) goto invalid_rail;

				/* Check if the new tile contrains tracks that are compatible
				 * with the current train, if not, bail out. */
				if (!CheckCompatibleRail(v, gp.new_tile)) goto invalid_rail;

				if (prev == NULL) {
					/* Currently the locomotive is active. Determine which one of the
					 * available tracks to choose */
					chosen_track = 1 << ChooseTrainTrack(v, gp.new_tile, enterdir, bits);
					assert(chosen_track & tracks);

					/* Check if it's a red signal and that force proceed is not clicked. */
					if ((tracks >> 16) & chosen_track && v->u.rail.force_proceed == 0) goto red_light;
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
				{
					const byte *b = _initial_tile_subcoord[FIND_FIRST_BIT(chosen_track)][enterdir];
					gp.x = (gp.x & ~0xF) | b[0];
					gp.y = (gp.y & ~0xF) | b[1];
					chosen_dir = (Direction)b[2];
				}

				/* Call the landscape function and tell it that the vehicle entered the tile */
				r = VehicleEnterTile(v, gp.new_tile, gp.x, gp.y);
				if (HASBIT(r, VETS_CANNOT_ENTER)) {
					goto invalid_rail;
				}

				if (IsLevelCrossingTile(v->tile) && v->next == NULL) {
					UnbarCrossing(v->tile);
					MarkTileDirtyByTile(v->tile);
				}

				if (IsFrontEngine(v)) v->load_unload_time_rem = 0;

				if (!HASBIT(r, VETS_ENTERED_WORMHOLE)) {
					v->tile = gp.new_tile;

					if (GetTileRailType(gp.new_tile, FindFirstTrack(chosen_track)) != GetTileRailType(gp.old_tile, FindFirstTrack(v->u.rail.track))) {
						TrainPowerChanged(GetFirstVehicleInChain(v));
					}

					v->u.rail.track = chosen_track;
					assert(v->u.rail.track);
				}

				if (IsFrontEngine(v)) TrainMovedChangeSignals(gp.new_tile, enterdir);

				/* Signals can only change when the first
				 * (above) or the last vehicle moves. */
				if (v->next == NULL) TrainMovedChangeSignals(gp.old_tile, ReverseDiagDir(enterdir));

				if (prev == NULL) AffectSpeedByDirChange(v, chosen_dir);

				v->direction = chosen_dir;
			}
		} else {
			/* In tunnel or on a bridge */
			GetNewVehiclePos(v, &gp);

			SetSpeedLimitOnBridge(v);

			if (!(IsTunnelTile(gp.new_tile) || IsBridgeTile(gp.new_tile)) || !HASBIT(VehicleEnterTile(v, gp.new_tile, gp.x, gp.y), VETS_ENTERED_WORMHOLE)) {
				v->x_pos = gp.x;
				v->y_pos = gp.y;
				VehiclePositionChanged(v);
				if (!(v->vehstatus & VS_HIDDEN)) EndVehicleMove(v);
				continue;
			}
		}

		/* update image of train, as well as delta XY */
		newdir = GetNewVehicleDirection(v, gp.x, gp.y);
		UpdateTrainDeltaXY(v, newdir);
		if (update_image) v->cur_image = GetTrainImage(v, newdir);

		v->x_pos = gp.x;
		v->y_pos = gp.y;

		/* update the Z position of the vehicle */
		old_z = AfterSetTrainPos(v, (gp.new_tile != gp.old_tile));

		if (prev == NULL) {
			/* This is the first vehicle in the train */
			AffectSpeedByZChange(v, old_z);
		}
	}
	return;

invalid_rail:
	/* We've reached end of line?? */
	if (prev != NULL) error("!Disconnecting train");
	goto reverse_train_direction;

red_light: {
	/* We're in front of a red signal ?? */
		/* find the first set bit in ts. need to do it in 2 steps, since
		 * FIND_FIRST_BIT only handles 6 bits at a time. */
		i = FindFirstTrackdir((TrackdirBits)(uint16)ts);

		if (!HasSignalOnTrackdir(gp.new_tile, ReverseTrackdir(i))) {
			v->cur_speed = 0;
			v->subspeed = 0;
			v->progress = 255 - 100;
			if (++v->load_unload_time_rem < _patches.wait_oneway_signal * 20) return;
		} else if (HasSignalOnTrackdir(gp.new_tile, i)){
			v->cur_speed = 0;
			v->subspeed = 0;
			v->progress = 255-10;
			if (++v->load_unload_time_rem < _patches.wait_twoway_signal * 73) {
				TileIndex o_tile = gp.new_tile + TileOffsByDiagDir(enterdir);
				VehicleAtSignalData vasd;
				vasd.tile = o_tile;
				vasd.direction = ReverseDir(dir);

				/* check if a train is waiting on the other side */
				if (VehicleFromPos(o_tile, &vasd, CheckVehicleAtSignal) == NULL) return;
			}
		}
	}

reverse_train_direction:
	v->load_unload_time_rem = 0;
	v->cur_speed = 0;
	v->subspeed = 0;
	ReverseTrainDirection(v);
}

extern TileIndex CheckTunnelBusy(TileIndex tile, uint *length);

/**
 * Deletes/Clears the last wagon of a crashed train. It takes the engine of the
 * train, then goes to the last wagon and deletes that. Each call to this function
 * will remove the last wagon of a crashed train. If this wagon was on a crossing,
 * or inside a tunnel, recalculate the signals as they might need updating
 * @param v the @Vehicle of which last wagon is to be removed
 */
static void DeleteLastWagon(Vehicle *v)
{
	Vehicle *u = v;

	/* Go to the last wagon and delete the link pointing there
	 * *u is then the one-before-last wagon, and *v the last
	 * one which will physicially be removed */
	for (; v->next != NULL; v = v->next) u = v;
	u->next = NULL;

	InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
	DeleteWindowById(WC_VEHICLE_VIEW, v->index);
	RebuildVehicleLists();
	InvalidateWindow(WC_COMPANY, v->owner);

	BeginVehicleMove(v);
	EndVehicleMove(v);
	DeleteVehicle(v);

	if (v->u.rail.track != TRACK_BIT_DEPOT && v->u.rail.track != TRACK_BIT_WORMHOLE)
		SetSignalsOnBothDir(v->tile, FIND_FIRST_BIT(v->u.rail.track));

	/* Check if the wagon was on a road/rail-crossing and disable it if no
	 * others are on it */
	DisableTrainCrossing(v->tile);

	if ((v->u.rail.track == TRACK_BIT_WORMHOLE && v->vehstatus & VS_HIDDEN)) { // inside a tunnel
		TileIndex endtile = CheckTunnelBusy(v->tile, NULL);

		if (endtile == INVALID_TILE) return; // tunnel is busy (error returned)

		switch (v->direction) {
			case 1:
			case 5:
				SetSignalsOnBothDir(v->tile, 0);
				SetSignalsOnBothDir(endtile, 0);
				break;

			case 3:
			case 7:
				SetSignalsOnBothDir(v->tile, 1);
				SetSignalsOnBothDir(endtile, 1);
				break;

			default:
				break;
		}
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
			UpdateTrainDeltaXY(v, v->direction);
			v->cur_image = GetTrainImage(v, v->direction);
			/* Refrain from updating the z position of the vehicle when on
			   a bridge, because AfterSetTrainPos will put the vehicle under
			   the bridge in that case */
			if (v->u.rail.track != TRACK_BIT_WORMHOLE) AfterSetTrainPos(v, false);
		}
	} while ((v = v->next) != NULL);
}

static void HandleCrashedTrain(Vehicle *v)
{
	int state = ++v->u.rail.crash_anim_pos;
	uint32 r;
	Vehicle *u;

	if (state == 4 && !(v->vehstatus & VS_HIDDEN)) {
		CreateEffectVehicleRel(v, 4, 4, 8, EV_EXPLOSION_LARGE);
	}

	if (state <= 200 && CHANCE16R(1, 7, r)) {
		int index = (r * 10 >> 16);

		u = v;
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
		} while ((u = u->next) != NULL);
	}

	if (state <= 240 && !(v->tick_counter & 3)) ChangeTrainDirRandomly(v);

	if (state >= 4440 && !(v->tick_counter&0x1F)) {
		DeleteLastWagon(v);
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
			SndPlayVehicleFx((_opt.landscape != LT_CANDY) ?
				SND_10_TRAIN_BREAKDOWN : SND_3A_COMEDY_BREAKDOWN_2, v);
		}

		if (!(v->vehstatus & VS_HIDDEN)) {
			Vehicle *u = CreateEffectVehicleRel(v, 4, 4, 5, EV_BREAKDOWN_SMOKE);
			if (u != NULL) u->u.special.unk0 = v->breakdown_delay * 2;
		}
	}

	if (!(v->tick_counter & 3)) {
		if (!--v->breakdown_delay) {
			v->breakdown_ctr = 0;
			InvalidateWindow(WC_VEHICLE_VIEW, v->index);
		}
	}
}

static const byte _breakdown_speeds[16] = {
	225, 210, 195, 180, 165, 150, 135, 120, 105, 90, 75, 60, 45, 30, 15, 15
};

static bool TrainCheckIfLineEnds(Vehicle *v)
{
	TileIndex tile;
	uint x,y;
	uint16 break_speed;
	DiagDirection dir;
	int t;
	uint32 ts;

	t = v->breakdown_ctr;
	if (t > 1) {
		v->vehstatus |= VS_TRAIN_SLOWING;

		break_speed = _breakdown_speeds[GB(~t, 4, 4)];
		if (break_speed < v->cur_speed) v->cur_speed = break_speed;
	} else {
		v->vehstatus &= ~VS_TRAIN_SLOWING;
	}

	if (v->u.rail.track == TRACK_BIT_WORMHOLE) return true; // exit if inside a tunnel
	if (v->u.rail.track == TRACK_BIT_DEPOT) return true; // exit if inside a depot

	tile = v->tile;

	if (IsTileType(tile, MP_TUNNELBRIDGE)) {
		DiagDirection dir;

		dir = IsTunnel(tile) ? GetTunnelDirection(tile) : GetBridgeRampDirection(tile);
		if (DiagDirToDir(dir) == v->direction) return true;
	}

	// depot?
	/* XXX -- When enabled, this makes it possible to crash trains of others
	     (by building a depot right against a station) */
/*	if (IsTileType(tile, MP_RAILWAY) && GetRailTileType(tile) == RAIL_TILE_DEPOT_WAYPOINT)
		return true;*/

	/* Determine the non-diagonal direction in which we will exit this tile */
	dir = DirToDiagDir(v->direction);
	if (!(v->direction & 1) && v->u.rail.track != _state_dir_table[dir]) {
		dir = ChangeDiagDir(dir, DIAGDIRDIFF_90LEFT);
	}
	/* Calculate next tile */
	tile += TileOffsByDiagDir(dir);
	// determine the track status on the next tile.
	ts = GetTileTrackStatus(tile, TRANSPORT_RAIL) & _reachable_tracks[dir];

	/* Calc position within the current tile ?? */
	x = v->x_pos & 0xF;
	y = v->y_pos & 0xF;

	switch (v->direction) {
		case DIR_N : x = ~x + ~y + 24; break;
		case DIR_NW: x = y;            /* FALLTHROUGH */
		case DIR_NE: x = ~x + 16;      break;
		case DIR_E : x = ~x + y + 8;   break;
		case DIR_SE: x = y;            break;
		case DIR_S : x = x + y - 8;    break;
		case DIR_W : x = ~y + x + 8;   break;
		default: break;
	}

	if (GB(ts, 0, 16) != 0) {
		/* If we approach a rail-piece which we can't enter, or the back of a depot, don't enter it! */
		if (x + 4 >= TILE_SIZE &&
				(!CheckCompatibleRail(v, tile) ||
				(IsTileDepotType(tile, TRANSPORT_RAIL) &&
				GetRailDepotDirection(tile) == dir))) {
			v->cur_speed = 0;
			ReverseTrainDirection(v);
			return false;
		}
		if ((ts &= (ts >> 16)) == 0) {
			// make a rail/road crossing red
			if (IsLevelCrossingTile(tile)) {
				if (!IsCrossingBarred(tile)) {
					BarCrossing(tile);
					SndPlayVehicleFx(SND_0E_LEVEL_CROSSING, v);
					MarkTileDirtyByTile(tile);
				}
			}
			return true;
		}
	} else if (x + 4 >= TILE_SIZE) {
		v->cur_speed = 0;
		ReverseTrainDirection(v);
		return false;
	}

	// slow down
	v->vehstatus |= VS_TRAIN_SLOWING;
	break_speed = _breakdown_speeds[x & 0xF];
	if (!(v->direction & 1)) break_speed >>= 1;
	if (break_speed < v->cur_speed) v->cur_speed = break_speed;

	return true;
}

static void TrainLocoHandler(Vehicle *v, bool mode)
{
	int j;

	/* train has crashed? */
	if (v->u.rail.crash_anim_pos != 0) {
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

	if (HASBIT(v->u.rail.flags, VRF_REVERSING) && v->cur_speed == 0) {
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

	HandleTrainLoading(v, mode);

	if (v->current_order.type == OT_LOADING) return;

	if (CheckTrainStayInDepot(v)) return;

	if (!mode) HandleLocomotiveSmokeCloud(v);

	j = UpdateTrainSpeed(v);
	if (j == 0) {
		// if the vehicle has speed 0, update the last_speed field.
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


void Train_Tick(Vehicle *v)
{
	if (_age_cargo_skip_counter == 0 && v->cargo_days != 0xff)
		v->cargo_days++;

	v->tick_counter++;

	if (IsFrontEngine(v)) {
		TrainLocoHandler(v, false);

		// make sure vehicle wasn't deleted.
		if (v->type == VEH_Train && IsFrontEngine(v))
			TrainLocoHandler(v, true);
	} else if (IsFreeWagon(v) && HASBITS(v->vehstatus, VS_CRASHED)) {
		// Delete flooded standalone wagon
		if (++v->u.rail.crash_anim_pos >= 4400)
			DeleteVehicle(v);
	}
}

#define MAX_ACCEPTABLE_DEPOT_DIST 16

static void CheckIfTrainNeedsService(Vehicle *v)
{
	const Depot* depot;
	TrainFindDepotData tfdd;

	if (_patches.servint_trains == 0)                   return;
	if (!VehicleNeedsService(v))                        return;
	if (v->vehstatus & VS_STOPPED)                      return;
	if (_patches.gotodepot && VehicleHasDepotOrders(v)) return;

	// Don't interfere with a depot visit scheduled by the user, or a
	// depot visit by the order list.
	if (v->current_order.type == OT_GOTO_DEPOT &&
			(v->current_order.flags & (OF_HALT_IN_DEPOT | OF_PART_OF_ORDERS)) != 0)
		return;

	if (CheckTrainIsInsideDepot(v)) {
		VehicleServiceInDepot(v);
		return;
	}

	tfdd = FindClosestTrainDepot(v, MAX_ACCEPTABLE_DEPOT_DIST);
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

	depot = GetDepotByTile(tfdd.tile);

	if (v->current_order.type == OT_GOTO_DEPOT &&
			v->current_order.dest != depot->index &&
			!CHANCE16(3, 16)) {
		return;
	}

	v->current_order.type = OT_GOTO_DEPOT;
	v->current_order.flags = OF_NON_STOP;
	v->current_order.dest = depot->index;
	v->dest_tile = tfdd.tile;
	InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
}

int32 GetTrainRunningCost(const Vehicle *v)
{
	int32 cost = 0;

	do {
		const RailVehicleInfo *rvi = RailVehInfo(v->engine_type);
		if (rvi->running_cost_base > 0)
			cost += rvi->running_cost_base * _price.running_rail[rvi->running_cost_class];
	} while ((v = GetNextVehicle(v)) != NULL);

	return cost;
}

void OnNewDay_Train(Vehicle *v)
{
	TileIndex tile;

	if ((++v->day_counter & 7) == 0) DecreaseVehicleValue(v);

	if (IsFrontEngine(v)) {
		CheckVehicleBreakdown(v);
		AgeVehicle(v);

		CheckIfTrainNeedsService(v);

		CheckOrders(v);

		/* update destination */
		if (v->current_order.type == OT_GOTO_STATION &&
				(tile = GetStation(v->current_order.dest)->train_tile) != 0) {
			v->dest_tile = tile;
		}

		if ((v->vehstatus & VS_STOPPED) == 0) {
			/* running costs */
			int32 cost = GetTrainRunningCost(v) / 364;

			v->profit_this_year -= cost >> 8;

			SET_EXPENSES_TYPE(EXPENSES_TRAIN_RUN);
			SubtractMoneyFromPlayerFract(v->owner, cost);

			InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
			InvalidateWindowClasses(WC_TRAINS_LIST);
		}
	}
}

void TrainsYearlyLoop(void)
{
	Vehicle *v;

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Train && IsFrontEngine(v)) {

			// show warning if train is not generating enough income last 2 years (corresponds to a red icon in the vehicle list)
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


void InitializeTrains(void)
{
	_age_cargo_skip_counter = 1;
}

/*
 * Link front and rear multiheaded engines to each other
 * This is done when loading a savegame
 */
void ConnectMultiheadedTrains(void)
{
	Vehicle *v;

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Train) {
			v->u.rail.other_multiheaded_part = NULL;
		}
	}

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Train && IsFrontEngine(v)) {
			Vehicle *u = v;

			BEGIN_ENUM_WAGONS(u) {
				if (u->u.rail.other_multiheaded_part != NULL) continue; // we already linked this one

				if (IsMultiheaded(u)) {
					if (!IsTrainEngine(u)) {
						/* we got a rear car without a front car. We will convert it to a front one */
						SetTrainEngine(u);
						u->spritenum--;
					}

					{
						Vehicle *w;

						for (w = u->next; w != NULL && (w->engine_type != u->engine_type || w->u.rail.other_multiheaded_part != NULL); w = GetNextVehicle(w));
						if (w != NULL) {
							/* we found a car to partner with this engine. Now we will make sure it face the right way */
							if (IsTrainEngine(w)) {
								ClearTrainEngine(w);
								w->spritenum++;
							}
						}

						if (w != NULL) {
							w->u.rail.other_multiheaded_part = u;
							u->u.rail.other_multiheaded_part = w;
						} else {
							/* we got a front car and no rear cars. We will fake this one for forget that it should have been multiheaded */
							ClearMultiheaded(u);
						}
					}
				}
			} END_ENUM_WAGONS(u)
		}
	}
}

/*
 *  Converts all trains to the new subtype format introduced in savegame 16.2
 *  It also links multiheaded engines or make them forget they are multiheaded if no suitable partner is found
 */
void ConvertOldMultiheadToNew(void)
{
	Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Train) {
			SETBIT(v->subtype, 7); // indicates that it's the old format and needs to be converted in the next loop
		}
	}

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Train) {
			if (HASBIT(v->subtype, 7) && ((v->subtype & ~0x80) == 0 || (v->subtype & ~0x80) == 4)) {
				Vehicle *u = v;

				BEGIN_ENUM_WAGONS(u) {
					const RailVehicleInfo *rvi = RailVehInfo(u->engine_type);

					CLRBIT(u->subtype, 7);
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
