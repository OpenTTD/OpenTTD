/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "gui.h"
#include "table/strings.h"
#include "map.h"
#include "tile.h"
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
#include "debug.h"
#include "waypoint.h"
#include "vehicle_gui.h"

#define IS_FIRSTHEAD_SPRITE(spritenum) \
	(is_custom_sprite(spritenum) ? IS_CUSTOM_FIRSTHEAD_SPRITE(spritenum) : _engine_sprite_add[spritenum] == 0)

static bool TrainCheckIfLineEnds(Vehicle *v);
static void TrainController(Vehicle *v);

static const byte _vehicle_initial_x_fract[4] = {10,8,4,8};
static const byte _vehicle_initial_y_fract[4] = {8,4,8,10};
static const byte _state_dir_table[4] = { 0x20, 8, 0x10, 4 };

/**
 * Recalculates the cached weight of a train and its vehicles. Should be called each time the cargo on
 * the consist changes.
 * @param v First vehicle of the consist.
 */
void TrainCargoChanged(Vehicle* v)
{
	Vehicle *u;
	uint16 weight = 0;

	for (u = v; u != NULL; u = u->next) {
		const RailVehicleInfo *rvi = RailVehInfo(u->engine_type);
		uint16 vweight = 0;

		vweight += (_cargoc.weights[u->cargo_type] * u->cargo_count) / 16;

		// Vehicle weight is not added for articulated parts.
		if (u->subtype != TS_Artic_Part) {
			// vehicle weight is the sum of the weight of the vehicle and the weight of its cargo
			vweight += rvi->weight;

			// powered wagons have extra weight added
			if (HASBIT(u->u.rail.flags, VRF_POWEREDWAGON))
				vweight += RailVehInfo(v->engine_type)->pow_wag_weight;
		}

		// consist weight is the sum of the weight of all vehicles in the consist
		weight += vweight;

		// store vehicle weight in cache
		u->u.rail.cached_veh_weight = vweight;
	};

	// store consist weight in cache
	v->u.rail.cached_weight = weight;
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
	uint32 power = 0;
	EngineID first_engine;

	assert(v->type == VEH_Train);

	assert(v->subtype == TS_Front_Engine || v->subtype == TS_Free_Car);

	rvi_v = RailVehInfo(v->engine_type);
	first_engine = (v->subtype == TS_Front_Engine) ? v->engine_type : INVALID_VEHICLE;
	v->u.rail.cached_total_length = 0;

	for (u = v; u != NULL; u = u->next) {
		const RailVehicleInfo *rvi_u = RailVehInfo(u->engine_type);
		uint16 veh_len;

		// update the 'first engine'
		u->u.rail.first_engine = (v == u) ? INVALID_VEHICLE : first_engine;

		if (rvi_u->visual_effect != 0) {
			u->u.rail.cached_vis_effect = rvi_u->visual_effect;
		} else {
			if (rvi_u->flags & RVI_WAGON || u->subtype == TS_Artic_Part) {
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

		if (u->subtype != TS_Artic_Part) {
			// power is the sum of the powers of all engines and powered wagons in the consist
			power += rvi_u->power;

			// check if its a powered wagon
			CLRBIT(u->u.rail.flags, VRF_POWEREDWAGON);
			if ((rvi_v->pow_wag_power != 0) && (rvi_u->flags & RVI_WAGON) && UsesWagonOverride(u)) {
				if (HASBIT(rvi_u->callbackmask, CBM_WAGON_POWER)) {
					uint16 callback = GetCallBackResult(CBID_WAGON_POWER,  u->engine_type, u);

					if (callback != CALLBACK_FAILED)
						u->u.rail.cached_vis_effect = callback;
				}

				if (u->u.rail.cached_vis_effect < 0x40) {
					/* wagon is powered */
					SETBIT(u->u.rail.flags, VRF_POWEREDWAGON); // cache 'powered' status
					power += rvi_v->pow_wag_power;
				}
			}

			// max speed is the minimum of the speed limits of all vehicles in the consist
			if (!(rvi_u->flags & RVI_WAGON) || _patches.wagon_speed_limits)
				if (rvi_u->max_speed != 0 && !UsesWagonOverride(u))
					max_speed = min(rvi_u->max_speed, max_speed);
		}

		// check the vehicle length (callback)
		veh_len = CALLBACK_FAILED;
		if (HASBIT(rvi_u->callbackmask, CBM_VEH_LENGTH))
			veh_len = GetCallBackResult(CBID_VEH_LENGTH,  u->engine_type, u);
		if (veh_len == CALLBACK_FAILED)
			veh_len = rvi_u->shorten_factor;
		veh_len = clamp(veh_len, 0, u->next == NULL ? 7 : 5); // the clamp on vehicles not the last in chain is stricter, as too short wagons can break the 'follow next vehicle' code
		u->u.rail.cached_veh_length = 8 - veh_len;
		v->u.rail.cached_total_length += u->u.rail.cached_veh_length;

	};

	// store consist weight/max speed in cache
	v->u.rail.cached_max_speed = max_speed;
	v->u.rail.cached_power = power;

	// recalculate cached weights too (we do this *after* the rest, so it is known which wagons are powered and need extra weight added)
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

	assert(v->type == VEH_Train);
	assert(IsTileType(v->tile, MP_STATION));
	//When does a train drive through a station
	//first we deal with the "new nonstop handling"
	if (_patches.new_nonstop && o->flags & OF_NON_STOP &&
			_m[tile].m2 == o->station )
		return false;

	if (v->last_station_visited == _m[tile].m2)
		return false;

	if (_m[tile].m2 != o->station &&
			(o->flags & OF_NON_STOP || _patches.new_nonstop))
		return false;

	return true;
}

//new acceleration
static int GetTrainAcceleration(Vehicle *v, bool mode)
{
	const Vehicle *u;
	int num = 0;	//number of vehicles, change this into the number of axles later
	int power = 0;
	int mass = 0;
	int max_speed = 2000;
	int area = 120;
	int friction = 35; //[1e-3]
	int drag_coeff = 20;	//[1e-4]
	int incl = 0;
	int resistance;
	int speed = v->cur_speed; //[mph]
	int force = 0x3FFFFFFF;
	int pos = 0;
	int lastpos = -1;
	int curvecount[2] = {0, 0};
	int sum = 0;
	int numcurve = 0;

	speed *= 10;
	speed /= 16;

	//first find the curve speed limit
	for (u = v; u->next != NULL; u = u->next, pos++) {
		int dir = u->direction;
		int ndir = u->next->direction;
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

	if (IsTileType(v->tile, MP_STATION) && v->subtype == TS_Front_Engine) {
		if (TrainShouldStop(v, v->tile)) {
			int station_length = 0;
			TileIndex tile = v->tile;
			int delta_v;

			max_speed = 120;
			do {
				station_length++;
				tile = TILE_ADD(tile, TileOffsByDir(v->direction / 2));
			} while (IsCompatibleTrainStationTile(tile, v->tile));

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

		if (u->u.rail.track == 0x80)
			max_speed = min(61, max_speed);

		if (HASBIT(u->u.rail.flags, VRF_GOINGUP)) {
			incl += u->u.rail.cached_veh_weight * 60;		//3% slope, quite a bit actually
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
	} else
		resistance = (area * (drag_coeff / 2) * speed * speed) / 10000;
	resistance += incl;
	resistance *= 4; //[N]

	if (speed > 0) {
		switch (v->u.rail.railtype) {
			case RAILTYPE_RAIL:
			case RAILTYPE_MONO:
				force = power / speed; //[N]
				force *= 22;
				force /= 10;
				break;

			case RAILTYPE_MAGLEV:
				force = power / 25;
				break;
		}
	} else {
		//"kickoff" acceleration
		force = (mass * 8) + resistance;
	}

	if (force <= 0) force = 10000;

	if (v->u.rail.railtype != RAILTYPE_MAGLEV) force = min(force, mass * 10 * 200);

	if (mode == AM_ACCEL) {
		return (force - resistance) / (mass * 4);
	} else {
		return min((-force - resistance) / (mass * 4), 10000 / (mass * 4));
	}
}

void UpdateTrainAcceleration(Vehicle *v)
{
	uint power = 0;
	uint weight = 0;

	assert(v->subtype == TS_Front_Engine);

	weight = v->u.rail.cached_weight;
	power = v->u.rail.cached_power;
	v->max_speed = v->u.rail.cached_max_speed;

	assert(weight != 0);

	v->acceleration = clamp(power / weight * 4, 1, 255);
}

int GetTrainImage(const Vehicle *v, byte direction)
{
	int img = v->spritenum;
	int base;

	if (is_custom_sprite(img)) {
		base = GetCustomVehicleSprite(v, direction + 4 * IS_CUSTOM_SECONDHEAD_SPRITE(img));
		if (base != 0) return base;
		img = orig_rail_vehicle_info[v->engine_type].image_index;
	}

	base = _engine_sprite_base[img] + ((direction + _engine_sprite_add[img]) & _engine_sprite_and[img]);

	if (v->cargo_count >= v->cargo_cap / 2) base += _wagon_full_adder[img];
	return base;
}

extern int _traininfo_vehicle_pitch;

void DrawTrainEngine(int x, int y, EngineID engine, uint32 image_ormod)
{
	const RailVehicleInfo *rvi = RailVehInfo(engine);

	int img = rvi->image_index;
	uint32 image = 0;

	if (is_custom_sprite(img)) {
		image = GetCustomVehicleIcon(engine, 6);
		if (image == 0) {
			img = orig_rail_vehicle_info[engine].image_index;
		} else {
			y += _traininfo_vehicle_pitch;
		}
	}
	if (image == 0) {
		image = (6 & _engine_sprite_and[img]) + _engine_sprite_base[img];
	}

	if (rvi->flags & RVI_MULTIHEAD) {
		DrawSprite(image | image_ormod, x - 14, y);
		x += 15;
		image = 0;
		if (is_custom_sprite(img)) {
			image = GetCustomVehicleIcon(engine, 2);
			if (image == 0) img = orig_rail_vehicle_info[engine].image_index;
		}
		if (image == 0) {
			image =
				((6 + _engine_sprite_add[img + 1]) & _engine_sprite_and[img + 1]) +
				_engine_sprite_base[img + 1];
		}
	}
	DrawSprite(image | image_ormod, x, y);
}

static uint CountArticulatedParts(const RailVehicleInfo *rvi, EngineID engine_type)
{
	uint16 callback;
	uint i;

	if (!HASBIT(rvi->callbackmask, CBM_ARTIC_ENGINE)) return 0;

	for (i = 1; i < 10; i++) {
		callback = GetCallBackResult(CBID_ARTIC_ENGINE + (i << 8), engine_type, NULL);
		if (callback == CALLBACK_FAILED || callback == 0xFF) break;
	}

	return i - 1;
}

static void AddArticulatedParts(const RailVehicleInfo *rvi, Vehicle **vl)
{
	const RailVehicleInfo *rvi_artic;
	EngineID engine_type;
	Vehicle *v = vl[0];
	Vehicle *u = v;
	uint16 callback;
	bool flip_image;
	uint i;

	if (!HASBIT(rvi->callbackmask, CBM_ARTIC_ENGINE))
		return;

	for (i = 1; i < 10; i++) {
		callback = GetCallBackResult(CBID_ARTIC_ENGINE + (i << 8), v->engine_type, NULL);
		if (callback == CALLBACK_FAILED || callback == 0xFF)
			return;

		u->next = vl[i];
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
		u->cargo_cap = rvi_artic->capacity;
		u->max_speed = 0;
		u->max_age = 0;
		u->engine_type = engine_type;
		u->value = 0;
		u->type = VEH_Train;
		u->subtype = TS_Artic_Part;
		u->cur_image = 0xAC2;

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

	num_vehicles = 1 + CountArticulatedParts(rvi, engine);

	if (!(flags & DC_QUERY_COST)) {
		Vehicle *vl[11]; // Allow for wagon and upto 10 artic parts.
		Vehicle* v;
		int x;
		int y;

		if (!AllocateVehicles(vl, num_vehicles))
			return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);

		if (flags & DC_EXEC) {
			Vehicle *u, *w;
			uint dir;

			v = vl[0];
			v->spritenum = rvi->image_index;

			u = NULL;

			FOR_ALL_VEHICLES(w) {
				if (w->type == VEH_Train && w->tile == tile &&
				    w->subtype == TS_Free_Car && w->engine_type == engine) {
					u = GetLastVehicleInChain(w);
					break;
				}
			}

			v->engine_type = engine;

			dir = GB(_m[tile].m5, 0, 2);

			v->direction = dir * 2 + 1;
			v->tile = tile;

			x = TileX(tile) * TILE_SIZE | _vehicle_initial_x_fract[dir];
			y = TileY(tile) * TILE_SIZE | _vehicle_initial_y_fract[dir];

			v->x_pos = x;
			v->y_pos = y;
			v->z_pos = GetSlopeZ(x,y);
			v->owner = _current_player;
			v->z_height = 6;
			v->u.rail.track = 0x80;
			v->vehstatus = VS_HIDDEN | VS_DEFPAL;

			v->subtype = TS_Free_Car;
			if (u != NULL) {
				u->next = v;
				v->subtype = TS_Not_First;
			}

			v->cargo_type = rvi->cargo_type;
			v->cargo_cap = rvi->capacity;
			v->value = value;
//			v->day_counter = 0;

			v->u.rail.railtype = GetEngine(engine)->railtype;

			v->build_year = _cur_year;
			v->type = VEH_Train;
			v->cur_image = 0xAC2;

			AddArticulatedParts(rvi, vl);

			_new_wagon_id = v->index;
			_new_vehicle_id = v->index;

			VehiclePositionChanged(v);
			TrainConsistChanged(GetFirstVehicleInChain(v));

			InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
		}
	}

	return value;
}

// Move all free vehicles in the depot to the train
static void NormalizeTrainVehInDepot(const Vehicle* u)
{
	const Vehicle* v;

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Train && v->subtype == TS_Free_Car &&
				v->tile == u->tile &&
				v->u.rail.track == 0x80) {
			if (DoCommandByTile(0, v->index | (u->index << 16), 1, DC_EXEC,
					CMD_MOVE_RAIL_VEHICLE) == CMD_ERROR)
				break;
		}
	}
}

static const byte _railveh_score[] = {
	1, 4, 7, 19, 20, 30, 31, 19,
	20, 21, 22, 10, 11, 30, 31, 32,
	33, 34, 35, 29, 45, 32, 50, 40,
	41, 51, 52, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 60, 62,
	63, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 70, 71, 72, 73,
	74, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0,
};


static int32 EstimateTrainCost(const RailVehicleInfo* rvi)
{
	return (rvi->base_cost * (_price.build_railvehicle >> 3)) >> 5;
}

void AddRearEngineToMultiheadedTrain(Vehicle *v, Vehicle *u, bool building)
{
	u->direction = v->direction;
	u->owner = v->owner;
	u->tile = v->tile;
	u->x_pos = v->x_pos;
	u->y_pos = v->y_pos;
	u->z_pos = v->z_pos;
	u->z_height = 6;
	u->u.rail.track = 0x80;
	u->vehstatus = v->vehstatus & ~VS_STOPPED;
	u->subtype = TS_Not_First;
	u->spritenum = v->spritenum + 1;
	u->cargo_type = v->cargo_type;
	u->cargo_cap = v->cargo_cap;
	u->u.rail.railtype = v->u.rail.railtype;
	if (building) v->next = u;
	u->engine_type = v->engine_type;
	u->build_year = v->build_year;
	if (building) v->value >>= 1;
	u->value = v->value;
	u->type = VEH_Train;
	u->cur_image = 0xAC2;
	VehiclePositionChanged(u);
}

/** Build a railroad vehicle.
 * @param x,y tile coordinates (depot) where rail-vehicle is built
 * @param p1 engine type id
 * @param p2 bit 0 build only one engine, even if it is a dualheaded engine.
          p2 bit 1 prevents any free cars from being added to the train
 */
int32 CmdBuildRailVehicle(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	const RailVehicleInfo *rvi;
	int value;
	Vehicle *v;
	UnitID unit_num;
	Engine *e;
	TileIndex tile = TileVirtXY(x, y);
	uint num_vehicles;

	/* Check if the engine-type is valid (for the player) */
	if (!IsEngineBuildable(p1, VEH_Train)) return CMD_ERROR;

	/* Check if the train is actually being built in a depot belonging
	 * to the player. Doesn't matter if only the cost is queried */
	if (!(flags & DC_QUERY_COST)) {
		if (!IsTileDepotType(tile, TRANSPORT_RAIL)) return CMD_ERROR;
		if (!IsTileOwner(tile, _current_player)) return CMD_ERROR;
	}

	SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);

	rvi = RailVehInfo(p1);
	e = GetEngine(p1);

	/* Check if depot and new engine uses the same kind of tracks */
	if (!IsCompatibleRail(e->railtype, GetRailType(tile))) return CMD_ERROR;

	if (rvi->flags & RVI_WAGON) return CmdBuildRailWagon(p1, tile, flags);

	value = EstimateTrainCost(rvi);

	//make sure we only pay for half a dualheaded engine if we only requested half of it
	if (rvi->flags&RVI_MULTIHEAD && HASBIT(p2,0))
		value /= 2;

	num_vehicles = (rvi->flags & RVI_MULTIHEAD && !HASBIT(p2, 0)) ? 2 : 1;
	num_vehicles += CountArticulatedParts(rvi, p1);

	if (!(flags & DC_QUERY_COST)) {
		Vehicle *vl[12]; // Allow for upto 10 artic parts and dual-heads
		if (!AllocateVehicles(vl, num_vehicles) || IsOrderPoolFull())
			return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);

		v = vl[0];

		unit_num = GetFreeUnitNumber(VEH_Train);
		if (unit_num > _patches.max_trains)
			return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);

		if (flags & DC_EXEC) {
			uint dir;

			v->unitnumber = unit_num;

			dir = GB(_m[tile].m5, 0, 2);

			v->direction = dir * 2 + 1;
			v->tile = tile;
			v->owner = _current_player;
			v->x_pos = (x |= _vehicle_initial_x_fract[dir]);
			v->y_pos = (y |= _vehicle_initial_y_fract[dir]);
			v->z_pos = GetSlopeZ(x,y);
			v->z_height = 6;
			v->u.rail.track = 0x80;
			v->vehstatus = VS_HIDDEN | VS_STOPPED | VS_DEFPAL;
			v->spritenum = rvi->image_index;
			v->cargo_type = rvi->cargo_type;
			v->cargo_cap = rvi->capacity;
			v->max_speed = rvi->max_speed;
			v->value = value;
			v->last_station_visited = INVALID_STATION;
			v->dest_tile = 0;

			v->engine_type = p1;

			v->reliability = e->reliability;
			v->reliability_spd_dec = e->reliability_spd_dec;
			v->max_age = e->lifelength * 366;

			v->string_id = STR_SV_TRAIN_NAME;
			v->u.rail.railtype = e->railtype;
			_new_train_id = v->index;
			_new_vehicle_id = v->index;

			v->service_interval = _patches.servint_trains;
			v->date_of_last_service = _date;
			v->build_year = _cur_year;
			v->type = VEH_Train;
			v->cur_image = 0xAC2;

			v->u.rail.shortest_platform[0] = 255;
			v->u.rail.shortest_platform[1] = 0;

			VehiclePositionChanged(v);

			if (rvi->flags & RVI_MULTIHEAD && !HASBIT(p2, 0)) {
				AddRearEngineToMultiheadedTrain(vl[0], vl[1], true);
			} else {
				AddArticulatedParts(rvi, vl);
			}

			TrainConsistChanged(v);
			UpdateTrainAcceleration(v);

			if (!HASBIT(p2, 1)) {	// check if the cars should be added to the new vehicle
				NormalizeTrainVehInDepot(v);
			}

			InvalidateWindow(WC_VEHICLE_DEPOT, tile);
			RebuildVehicleLists();
			InvalidateWindow(WC_COMPANY, v->owner);
			if (IsLocalPlayer()) {
				InvalidateWindow(WC_REPLACE_VEHICLE, VEH_Train); // updates the replace Train window
			}
		}
	}
	_cmd_build_rail_veh_score = _railveh_score[p1];

	return value;
}


/* Check if all the wagons of the given train are in a depot, returns the
 * number of cars (including loco) then. If not, sets the error message to
 * STR_881A_TRAINS_CAN_ONLY_BE_ALTERED and returns -1 */
int CheckTrainStoppedInDepot(const Vehicle *v)
{
	int count;
	TileIndex tile = v->tile;

	/* check if stopped in a depot */
	if (!IsTileDepotType(tile, TRANSPORT_RAIL) || v->cur_speed != 0) {
		_error_message = STR_881A_TRAINS_CAN_ONLY_BE_ALTERED;
		return -1;
	}

	count = 0;
	for (; v != NULL; v = v->next) {
		count++;
		if (v->u.rail.track != 0x80 || v->tile != tile ||
				(v->subtype == TS_Front_Engine && !(v->vehstatus & VS_STOPPED))) {
			_error_message = STR_881A_TRAINS_CAN_ONLY_BE_ALTERED;
			return -1;
		}
	}

	return count;
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

		v->subtype = TS_Free_Car;
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
		if (dst->type == VEH_Train && dst->subtype == TS_Free_Car &&
				dst->tile == tile) {
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

/** Move a rail vehicle around inside the depot.
 * @param x,y unused
 * @param p1 various bitstuffed elements
 * - p1 (bit  0 - 15) source vehicle index
 * - p1 (bit 16 - 31) what wagon to put the source wagon AFTER, XXX - INVALID_VEHICLE to make a new line
 * @param p2 (bit 0) move all vehicles following the source vehicle
 */
int32 CmdMoveRailVehicle(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	VehicleID s = GB(p1, 0, 16);
	VehicleID d = GB(p1, 16, 16);
	Vehicle *src, *dst, *src_head, *dst_head;
	bool is_loco;

	if (!IsVehicleIndex(s)) return CMD_ERROR;

	src = GetVehicle(s);

	if (src->type != VEH_Train) return CMD_ERROR;

	is_loco = !(RailVehInfo(src->engine_type)->flags & RVI_WAGON) && IS_FIRSTHEAD_SPRITE(src->spritenum);

	// if nothing is selected as destination, try and find a matching vehicle to drag to.
	if (d == INVALID_VEHICLE) {
		dst = NULL;
		if (!is_loco) dst = FindGoodVehiclePos(src);
	} else {
		dst = GetVehicle(d);
	}

	// if an articulated part is being handled, deal with its parent vehicle
	while (src->subtype == TS_Artic_Part) src = GetPrevVehicleInChain(src);
	if (dst != NULL) {
		while (dst->subtype == TS_Artic_Part) dst = GetPrevVehicleInChain(dst);
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

	/* clear the ->first cache */
	{
		Vehicle *u;

		for (u = src_head; u != NULL; u = u->next) u->first = NULL;
		for (u = dst_head; u != NULL; u = u->next) u->first = NULL;
	}

	/* check if all vehicles in the source train are stopped inside a depot */
	if (CheckTrainStoppedInDepot(src_head) < 0) return CMD_ERROR;

	/* check if all the vehicles in the dest train are stopped,
	 * and that the length of the dest train is no longer than XXX vehicles */
	if (dst_head != NULL) {
		int num = CheckTrainStoppedInDepot(dst_head);
		if (num < 0) return CMD_ERROR;

		if (num > (_patches.mammoth_trains ? 100 : 9) && dst_head->subtype == TS_Front_Engine )
			return_cmd_error(STR_8819_TRAIN_TOO_LONG);

		// if it's a multiheaded vehicle we're dragging to, drag to the vehicle before..
		while (IS_CUSTOM_SECONDHEAD_SPRITE(dst->spritenum) || (
			!is_custom_sprite(dst->spritenum) && _engine_sprite_add[dst->spritenum] != 0)
		) {
			Vehicle *v = GetPrevVehicleInChain(dst);
			if (v == NULL || src == v) break;
			dst = v;
		}

		assert(dst_head->tile == src_head->tile);
	}

	// when moving all wagons, we can't have the same src_head and dst_head
	if (HASBIT(p2, 0) && src_head == dst_head) return 0;

	// moving a loco to a new line?, then we need to assign a unitnumber.
	if (dst == NULL && src->subtype != TS_Front_Engine && is_loco) {
		UnitID unit_num = GetFreeUnitNumber(VEH_Train);
		if (unit_num > _patches.max_trains)
			return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);

		if (flags & DC_EXEC)
			src->unitnumber = unit_num;
	}


	/* do it? */
	if (flags & DC_EXEC) {
		Vehicle *new_front = GetNextVehicle(src);	//used if next in line should make a train on it's own
		bool make_new_front = src->subtype == TS_Front_Engine;

		if (HASBIT(p2, 0)) {
			// unlink ALL wagons
			if (src != src_head) {
				Vehicle *v = src_head;
				while (GetNextVehicle(v) != src) v = GetNextVehicle(v);
				GetLastEnginePart(v)->next = NULL;
			} else {
				src_head = NULL;
			}
		} else {
			// if moving within the same chain, dont use dst_head as it may get invalidated
			if (src_head == dst_head)
				dst_head = NULL;
			// unlink single wagon from linked list
			src_head = UnlinkWagon(src, src_head);
			GetLastEnginePart(src)->next = NULL;
		}

		if (dst == NULL) {
			// move the train to an empty line. for locomotives, we set the type to 0. for wagons, 4.
			if (is_loco) {
				if (src->subtype != TS_Front_Engine) {
					// setting the type to 0 also involves setting up the orders field.
					src->subtype = TS_Front_Engine;
					assert(src->orders == NULL);
					src->num_orders = 0;
				}
			} else {
				src->subtype = TS_Free_Car;
			}
			dst_head = src;
		} else {
			if (src->subtype == TS_Front_Engine) {
				// the vehicle was previously a loco. need to free the order list and delete vehicle windows etc.
				DeleteWindowById(WC_VEHICLE_VIEW, src->index);
				DeleteVehicleOrders(src);
			}

			src->subtype = TS_Not_First;
			src->unitnumber = 0; // doesn't occupy a unitnumber anymore.

			// link in the wagon(s) in the chain.
			{
				Vehicle *v;

				for (v = src; GetNextVehicle(v) != NULL; v = GetNextVehicle(v));
				GetLastEnginePart(v)->next = dst->next;
			}
			dst->next = src;
		}

		/* If there is an engine behind first_engine we moved away, it should become new first_engine
		 * To do this, CmdMoveRailVehicle must be called once more
		 * since we set p2 to a condition that makes the statement false, we can't loop forever with this one */
		if (make_new_front && new_front != NULL && !(HASBIT(p2, 0))) {
			if (!(RailVehInfo(new_front->engine_type)->flags & RVI_WAGON)) {
				CmdMoveRailVehicle(x, y, flags, new_front->index | (INVALID_VEHICLE << 16), 1);
			}
		}

		if (src_head) {
			TrainConsistChanged(src_head);
			if (src_head->subtype == TS_Front_Engine) {
				UpdateTrainAcceleration(src_head);
				InvalidateWindow(WC_VEHICLE_DETAILS, src_head->index);
				/* Update the refit button and window */
				InvalidateWindow(WC_VEHICLE_REFIT, src_head->index);
				InvalidateWindowWidget(WC_VEHICLE_VIEW, src_head->index, 12);
			}
			/* Update the depot window */
			InvalidateWindow(WC_VEHICLE_DEPOT, src_head->tile);
		};

		if (dst_head) {
			TrainConsistChanged(dst_head);
			if (dst_head->subtype == TS_Front_Engine) {
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
 * @param x,y unused
 * @param p1 train to start/stop
 * @param p2 unused
 */
int32 CmdStartStopTrain(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;

	if (!IsVehicleIndex(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_Train || !CheckOwnership(v->owner)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		v->u.rail.days_since_order_progr = 0;
		v->vehstatus ^= VS_STOPPED;
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
	}
	return 0;
}

/**
 * Search for a matching rear-engine of a dual-headed train.
 * Do this as if you would find matching parentheses. If a new
 * engine is 'started', first 'close' that before 'closing' our
 * searched engine
 */
Vehicle *GetRearEngine(const Vehicle *v, EngineID engine)
{
	Vehicle *u;
	int en_count = 1;

	for (u = v->next; u != NULL; u = u->next) {
		if (u->engine_type == engine) { // find matching engine
			en_count += (IS_FIRSTHEAD_SPRITE(u->spritenum)) ? +1 : -1;

			if (en_count == 0) return (Vehicle *)u;
		}
	}
	return NULL;
}

/** Sell a (single) train wagon/engine.
 * @param x,y unused
 * @param p1 the wagon/engine index
 * @param p2 the selling mode
 * - p2 = 0: only sell the single dragged wagon/engine (and any belonging rear-engines)
 * - p2 = 1: sell the vehicle and all vehicles following it in the chain
             if the wagon is dragged, don't delete the possibly belonging rear-engine to some front
 * - p2 = 2: when selling attached locos, rearrange all vehicles after it to separate lines;
 *           all wagons of the same type will go on the same line. Used by the AI currently
 */
int32 CmdSellRailWagon(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v, *tmp, *first;
	int32 cost = 0;

	if (!IsVehicleIndex(p1) || p2 > 2) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_Train || !CheckOwnership(v->owner)) return CMD_ERROR;

	SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);

	while (v->subtype == TS_Artic_Part) v = GetPrevVehicleInChain(v);
	first = GetFirstVehicleInChain(v);

	// make sure the vehicle is stopped in the depot
	if (CheckTrainStoppedInDepot(first) < 0) return CMD_ERROR;

	if (flags & DC_EXEC) {
		if (v == first && first->subtype == TS_Front_Engine) {
			DeleteWindowById(WC_VEHICLE_VIEW, first->index);
		}
		if (IsLocalPlayer() && (p1 == 1 || !(RailVehInfo(v->engine_type)->flags & RVI_WAGON))) {
			InvalidateWindow(WC_REPLACE_VEHICLE, VEH_Train);
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
			Vehicle *rear = (RailVehInfo(v->engine_type)->flags & RVI_MULTIHEAD) ? GetRearEngine(v, v->engine_type) : NULL;
			if (rear != NULL) {
				cost -= v->value;
				if (flags & DC_EXEC) {
					v = UnlinkWagon(rear, v);
					DeleteVehicle(rear);
				}
			}

			/* 2. We are selling the first engine, some special action might be required
				* here, so take attention */
			if ((flags & DC_EXEC) && v == first) {
				Vehicle *new_f = GetNextVehicle(first);

				/* 2.1 If the first wagon is sold, update the first-> pointers to NULL */
				for (tmp = first; tmp != NULL; tmp = tmp->next) tmp->first = NULL;

				/* 2.2 If there are wagons present after the deleted front engine, check
					* if the second wagon (which will be first) is an engine. If it is one,
					* promote it as a new train, retaining the unitnumber, orders */
				if (new_f != NULL) {
					if (!(RailVehInfo(new_f->engine_type)->flags & RVI_WAGON) && IS_FIRSTHEAD_SPRITE(new_f->spritenum)) {
						switch_engine = true;
						/* Copy important data from the front engine */
						new_f->unitnumber = first->unitnumber;
						new_f->current_order = first->current_order;
						new_f->cur_order_index = first->cur_order_index;
						new_f->orders = first->orders;
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
				DeleteVehicle(v);

				/* 4 If the second wagon was an engine, update it to front_engine
					* which UnlinkWagon() has changed to TS_Free_Car */
				if (switch_engine) first->subtype = TS_Front_Engine;

				/* 5. If the train still exists, update its acceleration, window, etc. */
				if (first != NULL) {
					TrainConsistChanged(first);
					if (first->subtype == TS_Front_Engine) {
						InvalidateWindow(WC_VEHICLE_DETAILS, first->index);
						InvalidateWindow(WC_VEHICLE_REFIT, first->index);
						UpdateTrainAcceleration(first);
					}
				}


				/* (6.) Borked AI. If it sells an engine it expects all wagons lined
				* up on a new line to be added to the newly built loco. Replace it is.
				* Totally braindead cause building a new engine adds all loco-less
				* engines to its train anyways */
				if (p2 == 2 && ori_subtype == TS_Front_Engine) {
					for (v = first; v != NULL; v = tmp) {
						tmp = GetNextVehicle(v);
						DoCommandByTile(v->tile, v->index | INVALID_VEHICLE << 16, 0, DC_EXEC, CMD_MOVE_RAIL_VEHICLE);
					}
				}
			}
		} break;
		case 1: { /* Delete wagon and all wagons after it given certain criteria */
			/* 1. Count the number for first and rear engines for dualheads
			* to be able to deduce which ones go with which ones */
			int enf_count = 0;
			int enr_count = 0;
			for (tmp = first; tmp != NULL; tmp = GetNextVehicle(tmp)) {
				if (RailVehInfo(tmp->engine_type)->flags & RVI_MULTIHEAD)
					(IS_FIRSTHEAD_SPRITE(tmp->spritenum)) ? enf_count++ : enr_count++;
			}

			/* 2. Start deleting every vehicle after the selected one
			* If we encounter a matching rear-engine to a front-engine
			* earlier in the chain (before deletion), leave it alone */
			for (; v != NULL; v = tmp) {
				tmp = GetNextVehicle(v);

				if (RailVehInfo(v->engine_type)->flags & RVI_MULTIHEAD) {
					if (IS_FIRSTHEAD_SPRITE(v->spritenum)) {
						/* Always delete newly encountered front-engines */
						enf_count--;
					} else if (enr_count > enf_count) {
						/* More rear engines than front engines means this rear-engine does
						 * not belong to any front-engine; delete */
						enr_count--;
					} else {
						/* Otherwise leave it alone */
						continue;
					}
				}

				cost -= v->value;
				if (flags & DC_EXEC) {
					first = UnlinkWagon(v, first);
					DeleteVehicle(v);
				}
			}

			/* 3. If it is still a valid train after selling, update its acceleration and cached values */
			if ((flags & DC_EXEC) && first != NULL) {
				TrainConsistChanged(first);
				if (first->subtype == TS_Front_Engine)
					UpdateTrainAcceleration(first);
			}
		} break;
	}
	return cost;
}

static void UpdateTrainDeltaXY(Vehicle *v, int direction)
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
		if (_patches.vehicle_speed || !old != !spd)
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

		/* swap variables */
		swap_byte(&a->u.rail.track, &b->u.rail.track);
		swap_byte(&a->direction, &b->direction);

		/* toggle direction */
		if (!(a->u.rail.track & 0x80)) a->direction ^= 4;
		if (!(b->u.rail.track & 0x80)) b->direction ^= 4;

		/* swap more variables */
		swap_int32(&a->x_pos, &b->x_pos);
		swap_int32(&a->y_pos, &b->y_pos);
		swap_tile(&a->tile, &b->tile);
		swap_byte(&a->z_pos, &b->z_pos);

		SwapTrainFlags(&a->u.rail.flags, &b->u.rail.flags);

		/* update other vars */
		UpdateVarsAfterSwap(a);
		UpdateVarsAfterSwap(b);

		VehicleEnterTile(a, a->tile, a->x_pos, a->y_pos);
		VehicleEnterTile(b, b->tile, b->x_pos, b->y_pos);
	} else {
		if (!(a->u.rail.track & 0x80)) a->direction ^= 4;
		UpdateVarsAfterSwap(a);

		VehicleEnterTile(a, a->tile, a->x_pos, a->y_pos);
	}
}

/* Check if the vehicle is a train and is on the tile we are testing */
static void *TestTrainOnCrossing(Vehicle *v, void *data)
{
	if (v->tile != *(const TileIndex*)data || v->type != VEH_Train) return NULL;
	return v;
}

static void DisableTrainCrossing(TileIndex tile)
{
	if (IsTileType(tile, MP_STREET) &&
			IsLevelCrossing(tile) &&
			VehicleFromPos(tile, &tile, TestTrainOnCrossing) == NULL && // empty?
			GB(_m[tile].m5, 2, 1) != 0) { // Lights on?
		SB(_m[tile].m5, 2, 1, 0); // Switch lights off
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
		// start<>end, start+1<>end-1, ... */
		last = first;
		for (i = length - 3; i > 0; i--) last = last->next;

		differential = last->u.rail.cached_veh_length - base->u.rail.cached_veh_length;
		if (before) differential *= -1;

		if (differential > 0) {
			Vehicle* tempnext;

			// disconnect last car to make sure only this subset moves
			tempnext = last->next;
			last->next = NULL;

			for (i = 0; i < differential; i++) TrainController(first);

			last->next = tempnext;
		}

		base = first;
		first = first->next;
		length -= 2;
	}
}

static TileIndex GetVehicleTileOutOfTunnel(const Vehicle* v, bool reverse)
{
	TileIndex tile;
	byte direction = (!reverse) ? DirToDiagdir(v->direction) : ReverseDiagdir(v->direction >> 1);
	TileIndexDiff delta = TileOffsByDir(direction);

	if (v->u.rail.track != 0x40) return v->tile;

	for (tile = v->tile;; tile += delta) {
		if (IsTunnelTile(tile) && GB(_m[tile].m5, 0, 2) != direction && GetTileZ(tile) == v->z_pos)
 			break;
 	}
 	return tile;
}

static void ReverseTrainDirection(Vehicle *v)
{
	int l = 0, r = -1;
	Vehicle *u;
	TileIndex tile;
	Trackdir trackdir;
	TileIndex pbs_end_tile = v->u.rail.pbs_end_tile; // these may be changed, and we may need
	Trackdir pbs_end_trackdir = v->u.rail.pbs_end_trackdir; // the old values, so cache them

	u = GetLastVehicleInChain(v);
	tile = GetVehicleTileOutOfTunnel(u, false);
	trackdir = ReverseTrackdir(GetVehicleTrackdir(u));

	if (PBSTileReserved(tile) & (1 << TrackdirToTrack(trackdir))) {
		NPFFindStationOrTileData fstd;
		NPFFoundTargetData ftd;

		NPFFillWithOrderData(&fstd, v);

		tile = GetVehicleTileOutOfTunnel(u, true);

		DEBUG(pbs, 2) ("pbs: (%i) choose reverse (RV), tile:%x, trackdir:%i",v->unitnumber,  u->tile, trackdir);
		ftd = NPFRouteToStationOrTile(tile, trackdir, &fstd, TRANSPORT_RAIL, v->owner, v->u.rail.railtype, PBS_MODE_ANY);

		if (ftd.best_trackdir == 0xFF) {
			DEBUG(pbs, 0) ("pbs: (%i) no nodes encountered (RV)", v->unitnumber);
			CLRBIT(v->u.rail.flags, VRF_REVERSING);
			return;
		}

		// we found a way out of the pbs block
		if (NPFGetFlag(&ftd.node, NPF_FLAG_PBS_EXIT)) {
			if (NPFGetFlag(&ftd.node, NPF_FLAG_PBS_BLOCKED)) {
				CLRBIT(v->u.rail.flags, VRF_REVERSING);
				return;
			}
		}

		v->u.rail.pbs_end_tile = ftd.node.tile;
		v->u.rail.pbs_end_trackdir = ftd.node.direction;
	}

	tile = GetVehicleTileOutOfTunnel(v, false);
	trackdir = GetVehicleTrackdir(v);

	if (v->u.rail.pbs_status == PBS_STAT_HAS_PATH) {
		TileIndex tile = AddTileIndexDiffCWrap(v->tile, TileIndexDiffCByDir(TrackdirToExitdir(trackdir)));
		uint32 ts;
		assert(tile != INVALID_TILE);
		ts = GetTileTrackStatus(tile, TRANSPORT_RAIL);
		ts &= TrackdirReachesTrackdirs(trackdir);
		assert(ts != 0 && KillFirstBit2x64(ts) == 0);
		trackdir = FindFirstBit2x64(ts);
		PBSClearPath(tile, trackdir, pbs_end_tile, pbs_end_trackdir);
		v->u.rail.pbs_status = PBS_STAT_NONE;
	} else if (PBSTileReserved(tile) & (1 << TrackdirToTrack(trackdir))) {
		PBSClearPath(tile, trackdir, pbs_end_tile, pbs_end_trackdir);
		if (v->u.rail.track != 0x40)
			PBSReserveTrack(tile, trackdir & 7);
	};

	if (IsTileDepotType(v->tile, TRANSPORT_RAIL))
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);


	/* Check if we were approaching a rail/road-crossing */
	{
		TileIndex tile = v->tile;
		int t;
		/* Determine the diagonal direction in which we will exit this tile */
		t = v->direction >> 1;
		if (!(v->direction & 1) && v->u.rail.track != _state_dir_table[t]) {
			t = (t - 1) & 3;
		}
		/* Calculate next tile */
		tile += TileOffsByDir(t);

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

	if (IsTileDepotType(v->tile, TRANSPORT_RAIL))
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);

	CLRBIT(v->u.rail.flags, VRF_REVERSING);
}

/** Reverse train.
 * @param x,y unused
 * @param p1 train to reverse
 * @param p2 unused
 */
 int32 CmdReverseTrainDirection(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;

	if (!IsVehicleIndex(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_Train || !CheckOwnership(v->owner)) return CMD_ERROR;

	_error_message = STR_EMPTY;

//	if (v->u.rail.track & 0x80 || IsTileDepotType(v->tile, TRANSPORT_RAIL))
//		return CMD_ERROR;

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
	return 0;
}

/** Force a train through a red signal
 * @param x,y unused
 * @param p1 train to ignore the red signal
 * @param p2 unused
 */
int32 CmdForceTrainProceed(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;

	if (!IsVehicleIndex(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_Train || !CheckOwnership(v->owner)) return CMD_ERROR;

	if (flags & DC_EXEC) v->u.rail.force_proceed = 0x50;

	return 0;
}

/** Refits a train to the specified cargo type.
 * @param x,y unused
 * @param p1 vehicle ID of the train to refit
 * @param p2 the new cargo type to refit to (p2 & 0xFF)
 */
int32 CmdRefitRailVehicle(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	CargoID new_cid = GB(p2, 0, 8);
	Vehicle *v;
	int32 cost;
	uint num;

	if (!IsVehicleIndex(p1)) return CMD_ERROR;

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
		if (!CanRefitTo(v, new_cid)) continue;

		if (v->cargo_cap != 0) {
			const RailVehicleInfo *rvi = RailVehInfo(v->engine_type);
			uint16 amount = CALLBACK_FAILED;

			if (HASBIT(rvi->callbackmask, CBM_REFIT_CAP)) {
				/* Check the 'refit capacity' callback */
				CargoID temp_cid = v->cargo_type;
				v->cargo_type = new_cid;
				amount = GetCallBackResult(CBID_REFIT_CAP, v->engine_type, v);
				v->cargo_type = temp_cid;
			}

			if (amount == CALLBACK_FAILED) { // callback failed or not used, use default
				CargoID old_cid = rvi->cargo_type;
				/* normally, the capacity depends on the cargo type, a rail vehicle
				* can carry twice as much mail/goods as normal cargo,
				* and four times as much passengers */
				amount = rvi->capacity;
				(old_cid == CT_PASSENGERS) ||
				(amount <<= 1, old_cid == CT_MAIL || old_cid == CT_GOODS) ||
				(amount <<= 1, true);
				(new_cid == CT_PASSENGERS) ||
				(amount >>= 1, new_cid == CT_MAIL || new_cid == CT_GOODS) ||
				(amount >>= 1, true);
			};

			if (amount != 0) {
				if (new_cid != v->cargo_type) cost += _price.build_railvehicle >> 8;
				num += amount;
				if (flags & DC_EXEC) {
					v->cargo_count = 0;
					v->cargo_type = new_cid;
					v->cargo_cap = amount;
					InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
					InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
				}
			}
		}
	} while ( (v=v->next) != NULL );

	_returned_refit_amount = num;

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
	if (IsTileType(tile, MP_RAILWAY) && IsTileOwner(tile, tfdd->owner)) {
		if ((_m[tile].m5 & ~0x3) == 0xC0) {
			tfdd->best_length = length;
			tfdd->tile = tile;
			return true;
		}
	}

	return false;
}

// returns the tile of a depot to goto to. The given vehicle must not be
// crashed!
static TrainFindDepotData FindClosestTrainDepot(Vehicle *v)
{
	int i;
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

	if (v->u.rail.track == 0x40) tile = GetVehicleOutOfTunnelTile(v);

	if (_patches.new_pathfinding_all) {
		NPFFoundTargetData ftd;
		Vehicle* last = GetLastVehicleInChain(v);
		Trackdir trackdir = GetVehicleTrackdir(v);
		Trackdir trackdir_rev = ReverseTrackdir(GetVehicleTrackdir(last));

		assert (trackdir != INVALID_TRACKDIR);
		ftd = NPFRouteToDepotBreadthFirstTwoWay(v->tile, trackdir, last->tile, trackdir_rev, TRANSPORT_RAIL, v->owner, v->u.rail.railtype, NPF_INFINITE_PENALTY);
		if (ftd.best_bird_dist == 0) {
			/* Found target */
			tfdd.tile = ftd.node.tile;
			/* Our caller expects a number of tiles, so we just approximate that
			* number by this. It might not be completely what we want, but it will
			* work for now :-) We can possibly change this when the old pathfinder
			* is removed. */
			tfdd.best_length = ftd.best_path_dist / NPF_TILE_LENGTH;
			if (NPFGetFlag(&ftd.node, NPF_FLAG_REVERSE))
				tfdd.reverse = true;
		}
	} else {
		// search in the forward direction first.
		i = v->direction >> 1;
		if (!(v->direction & 1) && v->u.rail.track != _state_dir_table[i]) i = (i - 1) & 3;
		NewTrainPathfind(tile, 0, i, (NTPEnumProc*)NtpCallbFindDepot, &tfdd);
		if (tfdd.best_length == (uint)-1){
			tfdd.reverse = true;
			// search in backwards direction
			i = (v->direction^4) >> 1;
			if (!(v->direction & 1) && v->u.rail.track != _state_dir_table[i]) i = (i - 1) & 3;
			NewTrainPathfind(tile, 0, i, (NTPEnumProc*)NtpCallbFindDepot, &tfdd);
		}
	}

	return tfdd;
}

/** Send a train to a depot
 * @param x,y unused
 * @param p1 train to send to the depot
 * @param p2 unused
 */
int32 CmdSendTrainToDepot(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;
	TrainFindDepotData tfdd;

	if (!IsVehicleIndex(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_Train || !CheckOwnership(v->owner)) return CMD_ERROR;

	if (v->vehstatus & VS_CRASHED) return CMD_ERROR;

	if (v->current_order.type == OT_GOTO_DEPOT) {
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

	tfdd = FindClosestTrainDepot(v);
	if (tfdd.best_length == (uint)-1)
		return_cmd_error(STR_883A_UNABLE_TO_FIND_ROUTE_TO);

	if (flags & DC_EXEC) {
		v->dest_tile = tfdd.tile;
		v->current_order.type = OT_GOTO_DEPOT;
		v->current_order.flags = OF_NON_STOP | OF_FULL_LOAD;
		v->current_order.station = GetDepotByTile(tfdd.tile)->index;
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
		/* If there is no depot in front, reverse automatically */
		if (tfdd.reverse)
			DoCommandByTile(v->tile, v->index, 0, DC_EXEC, CMD_REVERSE_TRAIN_DIRECTION);
	}

	return 0;
}

/** Change the service interval for trains.
 * @param x,y unused
 * @param p1 vehicle ID that is being service-interval-changed
 * @param p2 new service interval
 */
int32 CmdChangeTrainServiceInt(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;
	uint16 serv_int = GetServiceIntervalClamped(p2); /* Double check the service interval from the user-input */

	if (serv_int != p2 || !IsVehicleIndex(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_Train || !CheckOwnership(v->owner)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		v->service_interval = serv_int;
		InvalidateWindowWidget(WC_VEHICLE_DETAILS, v->index, 8);
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

	if (v->vehstatus & VS_TRAIN_SLOWING || v->load_unload_time_rem != 0 || v->cur_speed < 2)
		return;

	u = v;

	do {
		EngineID engtype = v->engine_type;
		int effect_offset = GB(v->u.rail.cached_vis_effect, 0, 4) - 8;
		byte effect_type = GB(v->u.rail.cached_vis_effect, 4, 2);
		bool disable_effect = HASBIT(v->u.rail.cached_vis_effect, 6);
		int x, y;

		// no smoke?
		if ((RailVehInfo(engtype)->flags & RVI_WAGON && effect_type == 0) ||
				disable_effect ||
				GetEngine(engtype)->railtype > RAILTYPE_RAIL ||
				v->vehstatus & VS_HIDDEN ||
				v->u.rail.track & 0xC0) {
			continue;
		}

		// No smoke in depots or tunnels
		if (IsTileDepotType(v->tile, TRANSPORT_RAIL) || IsTunnelTile(v->tile))
			continue;

		if (effect_type == 0) {
			// Use default effect type for engine class.
			effect_type = RailVehInfo(engtype)->engclass;
		} else {
			effect_type--;
		}

		x = _vehicle_smoke_pos[v->direction] * effect_offset;
		y = _vehicle_smoke_pos[(v->direction + 2) % 8] * effect_offset;

		switch (effect_type) {
		case 0:
			// steam smoke.
			if (GB(v->tick_counter, 0, 4) == 0) {
				CreateEffectVehicleRel(v, x, y, 10, EV_STEAM_SMOKE);
			}
			break;

		case 1:
			// diesel smoke
			if (u->cur_speed <= 40 && CHANCE16(15, 128)) {
				CreateEffectVehicleRel(v, 0, 0, 10, EV_DIESEL_SMOKE);
			}
			break;

		case 2:
			// blue spark
			if (GB(v->tick_counter, 0, 2) == 0 && CHANCE16(1, 45)) {
				CreateEffectVehicleRel(v, 0, 0, 10, EV_ELECTRIC_SPARK);
			}
			break;
		}
	} while ((v = v->next) != NULL);
}

static void TrainPlayLeaveStationSound(const Vehicle* v)
{
	static const SoundFx sfx[] = {
		SND_04_TRAIN,
		SND_0A_TRAIN_HORN,
		SND_0A_TRAIN_HORN
	};

	EngineID engtype = v->engine_type;

	switch (GetEngine(engtype)->railtype) {
		case RAILTYPE_RAIL:
			SndPlayVehicleFx(sfx[RailVehInfo(engtype)->engclass], v);
			break;

		case RAILTYPE_MONO:
			SndPlayVehicleFx(SND_47_MAGLEV_2, v);
			break;

		case RAILTYPE_MAGLEV:
			SndPlayVehicleFx(SND_41_MAGLEV, v);
			break;
	}
}

static bool CheckTrainStayInDepot(Vehicle *v)
{
	Vehicle *u;

	// bail out if not all wagons are in the same depot or not in a depot at all
	for (u = v; u != NULL; u = u->next) {
		if (u->u.rail.track != 0x80 || u->tile != v->tile) return false;
	}

	if (v->u.rail.force_proceed == 0) {
		Trackdir trackdir = GetVehicleTrackdir(v);

		if (++v->load_unload_time_rem < 37) {
			InvalidateWindowClasses(WC_TRAINS_LIST);
			return true;
		}

		v->load_unload_time_rem = 0;

		if (PBSIsPbsSegment(v->tile, trackdir)) {
			NPFFindStationOrTileData fstd;
			NPFFoundTargetData ftd;

			if (PBSTileUnavail(v->tile) & (1 << trackdir)) return true;

			NPFFillWithOrderData(&fstd, v);

			DEBUG(pbs, 2) ("pbs: (%i) choose depot (DP), tile:%x, trackdir:%i",v->unitnumber,  v->tile, trackdir);
			ftd = NPFRouteToStationOrTile(v->tile, trackdir, &fstd, TRANSPORT_RAIL, v->owner, v->u.rail.railtype, PBS_MODE_GREEN);

			// we found a way out of the pbs block
			if (NPFGetFlag(&ftd.node, NPF_FLAG_PBS_EXIT)) {
				if (NPFGetFlag(&ftd.node, NPF_FLAG_PBS_BLOCKED) || NPFGetFlag(&ftd.node, NPF_FLAG_PBS_RED)) {
					return true;
				} else {
					v->u.rail.pbs_end_tile = ftd.node.tile;
					v->u.rail.pbs_end_trackdir = ftd.node.direction;
					goto green;
				}
			}
		}

		if (UpdateSignalsOnSegment(v->tile, v->direction)) {
			InvalidateWindowClasses(WC_TRAINS_LIST);
			return true;
		}
	}
green:
	VehicleServiceInDepot(v);
	InvalidateWindowClasses(WC_TRAINS_LIST);
	TrainPlayLeaveStationSound(v);

	v->u.rail.track = 1;
	if (v->direction & 2) v->u.rail.track = 2;

	v->vehstatus &= ~VS_HIDDEN;
	v->cur_speed = 0;

	UpdateTrainDeltaXY(v, v->direction);
	v->cur_image = GetTrainImage(v, v->direction);
	VehiclePositionChanged(v);
	UpdateSignalsOnSegment(v->tile, v->direction);
	UpdateTrainAcceleration(v);
	InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);

	return false;
}

/* Check for station tiles */
typedef struct TrainTrackFollowerData {
	TileIndex dest_coords;
	StationID station_index; // station index we're heading for
	uint best_bird_dist;
	uint best_track_dist;
	byte best_track;
} TrainTrackFollowerData;

static bool NtpCallbFindStation(TileIndex tile, TrainTrackFollowerData *ttfd, int track, uint length)
{
	// heading for nowhere?
	if (ttfd->dest_coords == 0)
		return false;

	// did we reach the final station?
	if ((ttfd->station_index == INVALID_STATION && tile == ttfd->dest_coords) ||
		(IsTileType(tile, MP_STATION) && IS_BYTE_INSIDE(_m[tile].m5, 0, 8) && _m[tile].m2 == ttfd->station_index)) {
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
		fd->station_index = v->current_order.station;
	} else {
		fd->station_index = INVALID_STATION;
	}
}

static const byte _initial_tile_subcoord[6][4][3] = {
{{ 15, 8, 1 },{ 0, 0, 0 },{ 0, 8, 5 },{ 0, 0, 0 }},
{{  0, 0, 0 },{ 8, 0, 3 },{ 0, 0, 0 },{ 8,15, 7 }},
{{  0, 0, 0 },{ 7, 0, 2 },{ 0, 7, 6 },{ 0, 0, 0 }},
{{ 15, 8, 2 },{ 0, 0, 0 },{ 0, 0, 0 },{ 8,15, 6 }},
{{ 15, 7, 0 },{ 8, 0, 4 },{ 0, 0, 0 },{ 0, 0, 0 }},
{{  0, 0, 0 },{ 0, 0, 0 },{ 0, 8, 4 },{ 7,15, 0 }},
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
#if PF_BENCHMARK
#if !defined(_MSC_VER)
unsigned int _rdtsc()
{
	unsigned int high, low;

	__asm__ __volatile__ ("rdtsc" : "=a" (low), "=d" (high));
	return low;
}
#else
#ifndef _M_AMD64
static unsigned int _declspec(naked) _rdtsc(void)
{
	_asm {
		rdtsc
		ret
	}
}
#endif
#endif
#endif



/* choose a track */
static byte ChooseTrainTrack(Vehicle *v, TileIndex tile, int enterdir, TrackdirBits trackdirbits)
{
	TrainTrackFollowerData fd;
	uint best_track;
#if PF_BENCHMARK
	int time = _rdtsc();
	static float f;
#endif

	assert( (trackdirbits & ~0x3F) == 0);

	/* quick return in case only one possible track is available */
	if (KILL_FIRST_BIT(trackdirbits) == 0)
		return FIND_FIRST_BIT(trackdirbits);

	if (_patches.new_pathfinding_all) { /* Use a new pathfinding for everything */
		NPFFindStationOrTileData fstd;
		NPFFoundTargetData ftd;
		Trackdir trackdir;
		uint16 pbs_tracks;

		NPFFillWithOrderData(&fstd, v);
		/* The enterdir for the new tile, is the exitdir for the old tile */
		trackdir = GetVehicleTrackdir(v);
		assert(trackdir != 0xff);

		pbs_tracks = PBSTileReserved(tile);
		pbs_tracks |= pbs_tracks << 8;
		pbs_tracks &= TrackdirReachesTrackdirs(trackdir);
		if (pbs_tracks || (v->u.rail.pbs_status == PBS_STAT_NEED_PATH)) {
			DEBUG(pbs, 2) ("pbs: (%i) choosefromblock, tile_org:%x tile_dst:%x  trackdir:%i  pbs_tracks:%i",v->unitnumber, tile,tile - TileOffsByDir(enterdir), trackdir, pbs_tracks);
			// clear the currently planned path
			if (v->u.rail.pbs_status != PBS_STAT_NEED_PATH) PBSClearPath(tile, FindFirstBit2x64(pbs_tracks), v->u.rail.pbs_end_tile, v->u.rail.pbs_end_trackdir);

			// try to find a route to a green exit signal
			ftd = NPFRouteToStationOrTile(tile - TileOffsByDir(enterdir), trackdir, &fstd, TRANSPORT_RAIL, v->owner, v->u.rail.railtype, PBS_MODE_ANY);

			v->u.rail.pbs_end_tile = ftd.node.tile;
			v->u.rail.pbs_end_trackdir = ftd.node.direction;

		} else
			ftd = NPFRouteToStationOrTile(tile - TileOffsByDir(enterdir), trackdir, &fstd, TRANSPORT_RAIL, v->owner, v->u.rail.railtype, PBS_MODE_NONE);

		if (ftd.best_trackdir == 0xff) {
			/* We are already at our target. Just do something */
			//TODO: maybe display error?
			//TODO: go straight ahead if possible?
			best_track = FIND_FIRST_BIT(trackdirbits);
		} else {
			/* If ftd.best_bird_dist is 0, we found our target and ftd.best_trackdir contains
			the direction we need to take to get there, if ftd.best_bird_dist is not 0,
			we did not find our target, but ftd.best_trackdir contains the direction leading
			to the tile closest to our target. */
			/* Discard enterdir information, making it a normal track */
			best_track = TrackdirToTrack(ftd.best_trackdir);
		}
	} else {
		FillWithStationData(&fd, v);

		/* New train pathfinding */
		fd.best_bird_dist = (uint)-1;
		fd.best_track_dist = (uint)-1;
		fd.best_track = 0xFF;

		NewTrainPathfind(tile - TileOffsByDir(enterdir), v->dest_tile,
			enterdir, (NTPEnumProc*)NtpCallbFindStation, &fd);

		if (fd.best_track == 0xff) {
			// blaha
			best_track = FIND_FIRST_BIT(trackdirbits);
		} else {
			best_track = fd.best_track & 7;
		}
	}

#if PF_BENCHMARK
	time = _rdtsc() - time;
	f = f * 0.99 + 0.01 * time;
	printf("PF time = %d %f\n", time, f);
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
			v->u.rail.track & 0xC0 ||
			!(v->direction & 1))
		return false;

	FillWithStationData(&fd, v);

	best_track = -1;
	reverse_best = reverse = 0;

	assert(v->u.rail.track);

	i = _search_directions[FIND_FIRST_BIT(v->u.rail.track)][v->direction>>1];

	if (_patches.new_pathfinding_all) { /* Use a new pathfinding for everything */
		NPFFindStationOrTileData fstd;
		NPFFoundTargetData ftd;
		byte trackdir, trackdir_rev;
		Vehicle* last = GetLastVehicleInChain(v);

		NPFFillWithOrderData(&fstd, v);

		trackdir = GetVehicleTrackdir(v);
		trackdir_rev = ReverseTrackdir(GetVehicleTrackdir(last));
		assert(trackdir != 0xff);
		assert(trackdir_rev != 0xff);

		ftd = NPFRouteToStationOrTileTwoWay(v->tile, trackdir, last->tile, trackdir_rev, &fstd, TRANSPORT_RAIL, v->owner, v->u.rail.railtype, PBS_MODE_NONE);

		if (ftd.best_bird_dist != 0) {
			/* We didn't find anything, just keep on going straight ahead */
			reverse_best = false;
		} else {
			if (NPFGetFlag(&ftd.node, NPF_FLAG_REVERSE))
				reverse_best = true;
			else
				reverse_best = false;
		}
	} else {
		while(true) {
			fd.best_bird_dist = (uint)-1;
			fd.best_track_dist = (uint)-1;

			NewTrainPathfind(v->tile, v->dest_tile, reverse ^ i, (NTPEnumProc*)NtpCallbFindStation, &fd);

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
			if (reverse != 0)
				break;
			reverse = 2;
		}
	}

	return reverse_best != 0;
}

static bool ProcessTrainOrder(Vehicle *v)
{
	const Order *order;
	bool result;

	// These are un-interruptible
	if (v->current_order.type >= OT_GOTO_DEPOT &&
			v->current_order.type <= OT_LEAVESTATION) {
		// Let a depot order in the orderlist interrupt.
		if (v->current_order.type != OT_GOTO_DEPOT ||
				!(v->current_order.flags & OF_UNLOAD))
			return false;
	}

	if (v->current_order.type == OT_GOTO_DEPOT &&
			(v->current_order.flags & (OF_PART_OF_ORDERS | OF_SERVICE_IF_NEEDED)) ==  (OF_PART_OF_ORDERS | OF_SERVICE_IF_NEEDED) &&
			!VehicleNeedsService(v)) {
		v->cur_order_index++;
	}

	// check if we've reached the waypoint?
	if (v->current_order.type == OT_GOTO_WAYPOINT && v->tile == v->dest_tile) {
		v->cur_order_index++;
	}

	// check if we've reached a non-stop station while TTDPatch nonstop is enabled..
	if (_patches.new_nonstop &&
			v->current_order.flags & OF_NON_STOP &&
			IsTileType(v->tile, MP_STATION) &&
			v->current_order.station == _m[v->tile].m2) {
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
	if (order->type    == v->current_order.type &&
			order->flags   == v->current_order.flags &&
			order->station == v->current_order.station)
		return false;

	// Otherwise set it, and determine the destination tile.
	v->current_order = *order;

	v->dest_tile = 0;

	// store the station length if no shorter station was visited this order round
	if (v->cur_order_index == 0) {
		if (v->u.rail.shortest_platform[1] != 0 && v->u.rail.shortest_platform[1] != 255) {
			// we went though a whole round of orders without interruptions, so we store the length of the shortest station
			v->u.rail.shortest_platform[0] = v->u.rail.shortest_platform[1];
		}
		// all platforms are shorter than 255, so now we can find the shortest in the next order round. They might have changed size
		v->u.rail.shortest_platform[1] = 255;
	}

	if (v->last_station_visited != INVALID_STATION) {
		Station *st = GetStation(v->last_station_visited);
		if (TileBelongsToRailStation(st, v->tile)) {
			byte length = GetStationPlatforms(st, v->tile);

			if (length < v->u.rail.shortest_platform[1]) {
				v->u.rail.shortest_platform[1] = length;
			}
		}
	}

	result = false;
	switch (order->type) {
		case OT_GOTO_STATION:
			if (order->station == v->last_station_visited)
				v->last_station_visited = INVALID_STATION;
			v->dest_tile = GetStation(order->station)->xy;
			result = CheckReverseTrain(v);
			break;

		case OT_GOTO_DEPOT:
			v->dest_tile = GetDepot(order->station)->xy;
			result = CheckReverseTrain(v);
			break;

		case OT_GOTO_WAYPOINT:
			v->dest_tile = GetWaypoint(order->station)->xy;
			result = CheckReverseTrain(v);
			break;
	}

	InvalidateVehicleOrder(v);

	return result;
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
	if (v->current_order.type == OT_NOTHING) return;

	if (v->current_order.type != OT_DUMMY) {
		if (v->current_order.type != OT_LOADING) return;
		if (mode) return;

		// don't mark the train as lost if we're loading on the final station.
		if (v->current_order.flags & OF_NON_STOP)
			v->u.rail.days_since_order_progr = 0;

		if (--v->load_unload_time_rem) return;

		if (v->current_order.flags & OF_FULL_LOAD && CanFillVehicle(v)) {
			v->u.rail.days_since_order_progr = 0; /* Prevent a train lost message for full loading trains */
			SET_EXPENSES_TYPE(EXPENSES_TRAIN_INC);
			if (LoadUnloadVehicle(v)) {
				InvalidateWindow(WC_TRAINS_LIST, v->owner);
				MarkTrainDirty(v);

				// need to update acceleration and cached values since the goods on the train changed.
				TrainCargoChanged(v);
				UpdateTrainAcceleration(v);
			}
			return;
		}

		TrainPlayLeaveStationSound(v);

		{
			Order b = v->current_order;
			v->current_order.type = OT_LEAVESTATION;
			v->current_order.flags = 0;

			// If this was not the final order, don't remove it from the list.
			if (!(b.flags & OF_NON_STOP)) return;
		}
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
			0);
	}

	// Did we reach the final destination?
	if (v->current_order.type == OT_GOTO_STATION &&
			v->current_order.station == station) {
		// Yeah, keep the load/unload flags
		// Non Stop now means if the order should be increased.
		v->current_order.type = OT_LOADING;
		v->current_order.flags &= OF_FULL_LOAD | OF_UNLOAD | OF_TRANSFER;
		v->current_order.flags |= OF_NON_STOP;
	} else {
		// No, just do a simple load
		v->current_order.type = OT_LOADING;
		v->current_order.flags = 0;
	}
	v->current_order.station = 0;

	SET_EXPENSES_TYPE(EXPENSES_TRAIN_INC);
	if (LoadUnloadVehicle(v) != 0) {
		InvalidateWindow(WC_TRAINS_LIST, v->owner);
		MarkTrainDirty(v);
		TrainCargoChanged(v);
		UpdateTrainAcceleration(v);
	}
	InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
}

static byte AfterSetTrainPos(Vehicle *v, bool new_tile)
{
	byte new_z, old_z;

	// need this hint so it returns the right z coordinate on bridges.
	_get_z_hint = v->z_pos;
	new_z = GetSlopeZ(v->x_pos, v->y_pos);
	_get_z_hint = 0;

	old_z = v->z_pos;
	v->z_pos = new_z;

	if (new_tile) {
		CLRBIT(v->u.rail.flags, VRF_GOINGUP);
		CLRBIT(v->u.rail.flags, VRF_GOINGDOWN);

		if (new_z != old_z) {
			TileIndex tile = TileVirtXY(v->x_pos, v->y_pos);

			// XXX workaround, whole UP/DOWN detection needs overhaul
			if (!IsTileType(tile, MP_TUNNELBRIDGE) || (_m[tile].m5 & 0x80) != 0)
				SETBIT(v->u.rail.flags, (new_z > old_z) ? VRF_GOINGUP : VRF_GOINGDOWN);
		}
	}

	VehiclePositionChanged(v);
	EndVehicleMove(v);
	return old_z;
}

static const byte _new_vehicle_direction_table[11] = {
	0, 7, 6, 0,
	1, 0, 5, 0,
	2, 3, 4,
};

static int GetNewVehicleDirectionByTile(TileIndex new_tile, TileIndex old_tile)
{
	uint offs = (TileY(new_tile) - TileY(old_tile) + 1) * 4 +
							TileX(new_tile) - TileX(old_tile) + 1;
	assert(offs < 11);
	return _new_vehicle_direction_table[offs];
}

static int GetNewVehicleDirection(const Vehicle *v, int x, int y)
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
	switch (GetTileType(tile)) {
		case MP_RAILWAY:
		case MP_STATION:
			// normal tracks, jump to owner check
			break;

		case MP_TUNNELBRIDGE:
			if ((_m[tile].m5 & 0xC0) == 0xC0) { // is bridge middle part?
				uint height;
				uint tileh = GetTileSlope(tile, &height);

				// correct Z position of a train going under a bridge on slopes
				if (CorrectZ(tileh)) height += 8;

				if (v->z_pos != height) return true; // train is going over bridge
			}
			break;

		case MP_STREET:
			// tracks over roads, do owner check of tracks
			return
				IsTileOwner(tile, v->owner) && (
					v->subtype != TS_Front_Engine ||
					IsCompatibleRail(v->u.rail.railtype, GB(_m[tile].m4, 0, 4))
				);

		default:
			return true;
	}

	return
		IsTileOwner(tile, v->owner) && (
			v->subtype != TS_Front_Engine ||
			IsCompatibleRail(v->u.rail.railtype, GetRailType(tile))
		);
}

typedef struct {
	byte small_turn, large_turn;
	byte z_up; // fraction to remove when moving up
	byte z_down; // fraction to remove when moving down
} RailtypeSlowdownParams;

static const RailtypeSlowdownParams _railtype_slowdown[3] = {
	// normal accel
	{256/4, 256/2, 256/4, 2}, // normal
	{256/4, 256/2, 256/4, 2}, // monorail
	{0,     256/2, 256/4, 2}, // maglev
};

/* Modify the speed of the vehicle due to a turn */
static void AffectSpeedByDirChange(Vehicle *v, byte new_dir)
{
	byte diff;
	const RailtypeSlowdownParams *rsp;

	if (_patches.realistic_acceleration || (diff = (v->direction - new_dir) & 7) == 0)
		return;

	rsp = &_railtype_slowdown[v->u.rail.railtype];
	v->cur_speed -= ((diff == 1 || diff == 7) ? rsp->small_turn : rsp->large_turn) * v->cur_speed >> 8;
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

static const byte _otherside_signal_directions[14] = {
	1, 3, 1, 3, 5, 3, 0, 0,
	5, 7, 7, 5, 7, 1,
};

static void TrainMovedChangeSignals(TileIndex tile, int dir)
{
	if (IsTileType(tile, MP_RAILWAY) && (_m[tile].m5 & 0xC0) == 0x40) {
		uint i = FindFirstBit2x64((_m[tile].m5 + (_m[tile].m5 << 8)) & _reachable_tracks[dir]);
		UpdateSignalsOnSegment(tile, _otherside_signal_directions[i]);
	}
}


typedef struct TrainCollideChecker {
	const Vehicle *v;
	const Vehicle *v_skip;
} TrainCollideChecker;

static void *FindTrainCollideEnum(Vehicle *v, void *data)
{
	const TrainCollideChecker* tcc = data;

	if (v != tcc->v &&
			v != tcc->v_skip &&
			v->type == VEH_Train &&
			v->u.rail.track != 0x80 &&
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

	if (v->u.rail.crash_anim_pos != 0)
		return;

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
 * so, destroys this vehicle, and the other vehicle if its subtype is 0 (TS_Front_Engine).
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
	if (v->u.rail.track == 0x80) return;

	assert(v->u.rail.track == 0x40 || TileVirtXY(v->x_pos, v->y_pos) == v->tile);

	tcc.v = v;
	tcc.v_skip = v->next;

	/* find colliding vehicle */
	realcoll = VehicleFromPos(TileVirtXY(v->x_pos, v->y_pos), &tcc, FindTrainCollideEnum);
	if (realcoll == NULL) return;

	coll = GetFirstVehicleInChain(realcoll);

	/* it can't collide with its own wagons */
	if (v == coll ||
			(v->u.rail.track & 0x40 && (v->direction & 2) != (realcoll->direction & 2)))
		return;

	//two drivers + passangers killed in train v
	num = 2 + CountPassengersInTrain(v);
	if (!(coll->vehstatus & VS_CRASHED))
		//two drivers + passangers killed in train coll (if it was not crashed already)
		num += 2 + CountPassengersInTrain(coll);

	SetVehicleCrashed(v);
	if (coll->subtype == TS_Front_Engine) SetVehicleCrashed(coll);

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
	byte direction;
} VehicleAtSignalData;

static void *CheckVehicleAtSignal(Vehicle *v, void *data)
{
	const VehicleAtSignalData* vasd = data;

	if (v->type == VEH_Train && v->subtype == TS_Front_Engine &&
			v->tile == vasd->tile) {
		byte diff = (v->direction - vasd->direction + 2) & 7;

		if (diff == 2 || (v->cur_speed <= 5 && diff <= 4))
			return v;
	}
	return NULL;
}

static void TrainController(Vehicle *v)
{
	Vehicle *prev;
	GetNewVehiclePosResult gp;
	uint32 r, tracks,ts;
	int i, enterdir, newdir, dir;
	byte chosen_dir;
	byte chosen_track;
	byte old_z;

	/* For every vehicle after and including the given vehicle */
	for (prev = GetPrevVehicleInChain(v); v != NULL; prev = v, v = v->next) {
		BeginVehicleMove(v);

		if (v->u.rail.track != 0x40) {
			/* Not inside tunnel */
			if (GetNewVehiclePos(v, &gp)) {
				/* Staying in the old tile */
				if (v->u.rail.track == 0x80) {
					/* inside depot */
					gp.x = v->x_pos;
					gp.y = v->y_pos;
				} else {
					/* is not inside depot */

					if ((prev == NULL) && (!TrainCheckIfLineEnds(v)))
						return;

					r = VehicleEnterTile(v, gp.new_tile, gp.x, gp.y);
					if (r & 0x8) {
						//debug("%x & 0x8", r);
						goto invalid_rail;
					}
					if (r & 0x2) {
						TrainEnterStation(v, r >> 8);
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

				byte bits;
				/* Determine what direction we're entering the new tile from */
				dir = GetNewVehicleDirectionByTile(gp.new_tile, gp.old_tile);
				enterdir = dir >> 1;
				assert(enterdir==0 || enterdir==1 || enterdir==2 || enterdir==3);

				/* Get the status of the tracks in the new tile and mask
				 * away the bits that aren't reachable. */
				ts = GetTileTrackStatus(gp.new_tile, TRANSPORT_RAIL) & _reachable_tracks[enterdir];

				/* Combine the from & to directions.
				 * Now, the lower byte contains the track status, and the byte at bit 16 contains
				 * the signal status. */
				tracks = ts|(ts >> 8);
				bits = tracks & 0xFF;
				if (_patches.new_pathfinding_all && _patches.forbid_90_deg && prev == NULL)
					/* We allow wagons to make 90 deg turns, because forbid_90_deg
					 * can be switched on halfway a turn */
					bits &= ~TrackCrossesTracks(FIND_FIRST_BIT(v->u.rail.track));

				if ( bits == 0) {
					//debug("%x == 0", bits);
					goto invalid_rail;
				}

				/* Check if the new tile contrains tracks that are compatible
				 * with the current train, if not, bail out. */
				if (!CheckCompatibleRail(v, gp.new_tile)) {
					//debug("!CheckCompatibleRail(%p, %x)", v, gp.new_tile);
					goto invalid_rail;
				}

				if (prev == NULL) {
					byte trackdir;
					/* Currently the locomotive is active. Determine which one of the
					 * available tracks to choose */
					chosen_track = 1 << ChooseTrainTrack(v, gp.new_tile, enterdir, bits);
					assert(chosen_track & tracks);

					trackdir = TrackEnterdirToTrackdir(FIND_FIRST_BIT(chosen_track), enterdir);
					assert(trackdir != 0xff);

					if (PBSIsPbsSignal(gp.new_tile,trackdir) && PBSIsPbsSegment(gp.new_tile,trackdir)) {
						// encountered a pbs signal, and possible a pbs block
						DEBUG(pbs, 3) ("pbs: (%i) arrive AT signal, tile:%x  pbs_stat:%i",v->unitnumber, gp.new_tile, v->u.rail.pbs_status);

						if (v->u.rail.pbs_status == PBS_STAT_NONE) {
							// we havent planned a path already, so try to find one now
							NPFFindStationOrTileData fstd;
							NPFFoundTargetData ftd;

							NPFFillWithOrderData(&fstd, v);

							DEBUG(pbs, 2) ("pbs: (%i) choose signal (TC), tile:%x, trackdir:%i",v->unitnumber,  gp.new_tile, trackdir);
							ftd = NPFRouteToStationOrTile(gp.new_tile, trackdir, &fstd, TRANSPORT_RAIL, v->owner, v->u.rail.railtype, PBS_MODE_GREEN);

							if (v->u.rail.force_proceed != 0)
								goto green_light;

							if (ftd.best_trackdir == 0xFF)
								goto red_light;

							// we found a way out of the pbs block
							if (NPFGetFlag(&ftd.node, NPF_FLAG_PBS_EXIT)) {
								if (NPFGetFlag(&ftd.node, NPF_FLAG_PBS_BLOCKED) || NPFGetFlag(&ftd.node, NPF_FLAG_PBS_RED))
									goto red_light;
								else {
									v->u.rail.pbs_end_tile = ftd.node.tile;
									v->u.rail.pbs_end_trackdir = ftd.node.direction;
									goto green_light;
								}

							};

						} else {
							// we have already planned a path through this pbs block
							// on entering the block, we reset our status
							v->u.rail.pbs_status = PBS_STAT_NONE;
							goto green_light;
						};
						DEBUG(pbs, 3) ("pbs: (%i) no green light found, or was no pbs-block",v->unitnumber);
					};

					/* Check if it's a red signal and that force proceed is not clicked. */
					if ( (tracks>>16)&chosen_track && v->u.rail.force_proceed == 0) goto red_light;
				} else {
					static byte _matching_tracks[8] = {0x30, 1, 0xC, 2, 0x30, 1, 0xC, 2};

					/* The wagon is active, simply follow the prev vehicle. */
					chosen_track = (byte)(_matching_tracks[GetDirectionToVehicle(prev, gp.x, gp.y)] & bits);
				}
green_light:
				if (v->next == NULL)
					PBSClearTrack(gp.old_tile, FIND_FIRST_BIT(v->u.rail.track));

				/* make sure chosen track is a valid track */
				assert(chosen_track==1 || chosen_track==2 || chosen_track==4 || chosen_track==8 || chosen_track==16 || chosen_track==32);

				/* Update XY to reflect the entrance to the new tile, and select the direction to use */
				{
					const byte *b = _initial_tile_subcoord[FIND_FIRST_BIT(chosen_track)][enterdir];
					gp.x = (gp.x & ~0xF) | b[0];
					gp.y = (gp.y & ~0xF) | b[1];
					chosen_dir = b[2];
				}

				/* Call the landscape function and tell it that the vehicle entered the tile */
				r = VehicleEnterTile(v, gp.new_tile, gp.x, gp.y);
				if (r&0x8){
					//debug("%x & 0x8", r);
					goto invalid_rail;
				}

				if (v->subtype == TS_Front_Engine) v->load_unload_time_rem = 0;

				if (!(r&0x4)) {
					v->tile = gp.new_tile;
					v->u.rail.track = chosen_track;
					assert(v->u.rail.track);
				}

				if (v->subtype == TS_Front_Engine)
				TrainMovedChangeSignals(gp.new_tile, enterdir);

				/* Signals can only change when the first
				 * (above) or the last vehicle moves. */
				if (v->next == NULL)
				TrainMovedChangeSignals(gp.old_tile, (enterdir) ^ 2);

				if (prev == NULL) {
					AffectSpeedByDirChange(v, chosen_dir);
				}

				v->direction = chosen_dir;
			}
		} else {
			/* in tunnel */
			GetNewVehiclePos(v, &gp);

			// Check if to exit the tunnel...
			if (!IsTunnelTile(gp.new_tile) ||
					!(VehicleEnterTile(v, gp.new_tile, gp.x, gp.y)&0x4) ) {
				v->x_pos = gp.x;
				v->y_pos = gp.y;
				VehiclePositionChanged(v);
				continue;
			}
		}

		/* update image of train, as well as delta XY */
		newdir = GetNewVehicleDirection(v, gp.x, gp.y);
		UpdateTrainDeltaXY(v, newdir);
		v->cur_image = GetTrainImage(v, newdir);

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
		i = FindFirstBit2x64(ts);

		if (!(_m[gp.new_tile].m3 & SignalAgainstTrackdir(i))) {
			v->cur_speed = 0;
			v->subspeed = 0;
			v->progress = 255-100;
			if (++v->load_unload_time_rem < _patches.wait_oneway_signal * 20)
				return;
		} else if (_m[gp.new_tile].m3 & SignalAlongTrackdir(i)){
			v->cur_speed = 0;
			v->subspeed = 0;
			v->progress = 255-10;
			if (++v->load_unload_time_rem < _patches.wait_twoway_signal * 73) {
				TileIndex o_tile = gp.new_tile + TileOffsByDir(enterdir);
				VehicleAtSignalData vasd;
				vasd.tile = o_tile;
				vasd.direction = dir ^ 4;

				/* check if a train is waiting on the other side */
				if (VehicleFromPos(o_tile, &vasd, CheckVehicleAtSignal) == NULL)
					return;
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

	// clear up reserved pbs tracks
	if (PBSTileReserved(v->tile) & v->u.rail.track) {
		if (v == u) {
			PBSClearPath(v->tile, FIND_FIRST_BIT(v->u.rail.track), v->u.rail.pbs_end_tile, v->u.rail.pbs_end_trackdir);
			PBSClearPath(v->tile, FIND_FIRST_BIT(v->u.rail.track) + 8, v->u.rail.pbs_end_tile, v->u.rail.pbs_end_trackdir);
		};
		if (v->tile != u->tile) {
			PBSClearTrack(v->tile, FIND_FIRST_BIT(v->u.rail.track));
		};
	}

	if (!(v->u.rail.track & 0xC0))
		SetSignalsOnBothDir(v->tile, FIND_FIRST_BIT(v->u.rail.track));

	/* Check if the wagon was on a road/rail-crossing and disable it if no
	 * others are on it */
	DisableTrainCrossing(v->tile);

	if (v->u.rail.track == 0x40) { // inside a tunnel
		TileIndex endtile = CheckTunnelBusy(v->tile, NULL);

		if (endtile == INVALID_TILE) // tunnel is busy (error returned)
			return;

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
	static int8 _random_dir_change[4] = { -1, 0, 0, 1};

	do {
		//I need to buffer the train direction
		if (!(v->u.rail.track & 0x40))
			v->direction = (v->direction + _random_dir_change[GB(Random(), 0, 2)]) & 7;
		if (!(v->vehstatus & VS_HIDDEN)) {
			BeginVehicleMove(v);
			UpdateTrainDeltaXY(v, v->direction);
			v->cur_image = GetTrainImage(v, v->direction);
			AfterSetTrainPos(v, false);
		}
	} while ( (v=v->next) != NULL);
}

static void HandleCrashedTrain(Vehicle *v)
{
	int state = ++v->u.rail.crash_anim_pos, index;
	uint32 r;
	Vehicle *u;

	if (state == 4 && v->u.rail.track != 0x40) {
		CreateEffectVehicleRel(v, 4, 4, 8, EV_EXPLOSION_LARGE);
	}

	if (state <= 200 && CHANCE16R(1, 7, r)) {
		index = (r * 10 >> 16);

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
		} while ( (u=u->next) != NULL);
	}

	if (state <= 240 && !(v->tick_counter&3)) {
		ChangeTrainDirRandomly(v);
	}

	if (state >= 4440 && !(v->tick_counter&0x1F)) {
		DeleteLastWagon(v);
		InvalidateWindow(WC_REPLACE_VEHICLE, VEH_Train);
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

		SndPlayVehicleFx((_opt.landscape != LT_CANDY) ?
			SND_10_TRAIN_BREAKDOWN : SND_3A_COMEDY_BREAKDOWN_2, v);

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
	int t;
	uint32 ts;
	byte trackdir;

	t = v->breakdown_ctr;
	if (t > 1) {
		v->vehstatus |= VS_TRAIN_SLOWING;

		break_speed = _breakdown_speeds[GB(~t, 4, 4)];
		if (break_speed < v->cur_speed) v->cur_speed = break_speed;
	} else {
		v->vehstatus &= ~VS_TRAIN_SLOWING;
	}

	if (v->u.rail.track & 0x40) return true; // exit if inside a tunnel
	if (v->u.rail.track & 0x80) return true; // exit if inside a depot

	tile = v->tile;

	// tunnel entrance?
	if (IsTunnelTile(tile) && GB(_m[tile].m5, 0, 2) * 2 + 1 == v->direction)
		return true;

	// depot?
	/* XXX -- When enabled, this makes it possible to crash trains of others
	     (by building a depot right against a station) */
/*	if (IsTileType(tile, MP_RAILWAY) && (_m[tile].m5 & 0xFC) == 0xC0)
		return true;*/

	/* Determine the non-diagonal direction in which we will exit this tile */
	t = v->direction >> 1;
	if (!(v->direction & 1) && v->u.rail.track != _state_dir_table[t]) {
		t = (t - 1) & 3;
	}
	/* Calculate next tile */
	tile += TileOffsByDir(t);
	// determine the track status on the next tile.
	ts = GetTileTrackStatus(tile, TRANSPORT_RAIL) & _reachable_tracks[t];

	// if there are tracks on the new tile, pick one (trackdir will only be used when its a signal tile, in which case only 1 trackdir is accessible for us)
	if (ts & TRACKDIR_BIT_MASK)
		trackdir = FindFirstBit2x64(ts & TRACKDIR_BIT_MASK);
	else
		trackdir = INVALID_TRACKDIR;

	/* Calc position within the current tile ?? */
	x = v->x_pos & 0xF;
	y = v->y_pos & 0xF;

	switch (v->direction) {
	case 0:
		x = (~x) + (~y) + 24;
		break;
	case 7:
		x = y;
		/* fall through */
	case 1:
		x = (~x) + 16;
		break;
	case 2:
		x = (~x) + y + 8;
		break;
	case 3:
		x = y;
		break;
	case 4:
		x = x + y - 8;
		break;
	case 6:
		x = (~y) + x + 8;
		break;
	}

	if (GB(ts, 0, 16) != 0) {
		/* If we approach a rail-piece which we can't enter, don't enter it! */
		if (x + 4 > 15 && !CheckCompatibleRail(v, tile)) {
			v->cur_speed = 0;
			ReverseTrainDirection(v);
			return false;
		}
		if ((ts &= (ts >> 16)) == 0) {
			// make a rail/road crossing red
			if (IsTileType(tile, MP_STREET) && IsLevelCrossing(tile)) {
				if (GB(_m[tile].m5, 2, 1) == 0) {
					SB(_m[tile].m5, 2, 1, 1);
					SndPlayVehicleFx(SND_0E_LEVEL_CROSSING, v);
					MarkTileDirtyByTile(tile);
				}
			}
			return true;
		}
	} else if (x + 4 > 15) {
		v->cur_speed = 0;
		ReverseTrainDirection(v);
		return false;
	}

	if  (v->u.rail.pbs_status == PBS_STAT_HAS_PATH)
		return true;

	if ((trackdir != INVALID_TRACKDIR) && (PBSIsPbsSignal(tile,trackdir) && PBSIsPbsSegment(tile,trackdir)) && !(IsTileType(v->tile, MP_STATION) && (v->current_order.station == _m[v->tile].m2))) {
		NPFFindStationOrTileData fstd;
		NPFFoundTargetData ftd;

		NPFFillWithOrderData(&fstd, v);

		DEBUG(pbs, 2) ("pbs: (%i) choose signal (CEOL), tile:%x  trackdir:%i", v->unitnumber, tile, trackdir);
		ftd = NPFRouteToStationOrTile(tile, trackdir, &fstd, TRANSPORT_RAIL, v->owner, v->u.rail.railtype, PBS_MODE_GREEN);

		if (ftd.best_trackdir != 0xFF && NPFGetFlag(&ftd.node, NPF_FLAG_PBS_EXIT)) {
			if (!(NPFGetFlag(&ftd.node, NPF_FLAG_PBS_BLOCKED) || NPFGetFlag(&ftd.node, NPF_FLAG_PBS_RED))) {
				v->u.rail.pbs_status = PBS_STAT_HAS_PATH;
				v->u.rail.pbs_end_tile = ftd.node.tile;
				v->u.rail.pbs_end_trackdir = ftd.node.direction;
				return true;
			}
		};
	};

	// slow down
	v->vehstatus |= VS_TRAIN_SLOWING;
	break_speed = _breakdown_speeds[x & 0xF];
	if (!(v->direction&1)) break_speed >>= 1;
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

	if (v->u.rail.force_proceed != 0)
		v->u.rail.force_proceed--;

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
	if (v->vehstatus & VS_STOPPED && v->cur_speed == 0)
		return;


	if (ProcessTrainOrder(v)) {
		v->load_unload_time_rem = 0;
		v->cur_speed = 0;
		v->subspeed = 0;
		ReverseTrainDirection(v);
		return;
	}

	HandleTrainLoading(v, mode);

	if (v->current_order.type == OT_LOADING)
		return;

	if (CheckTrainStayInDepot(v))
		return;

	if (!mode) HandleLocomotiveSmokeCloud(v);

	j = UpdateTrainSpeed(v);
	if (j == 0) {
		// if the vehicle has speed 0, update the last_speed field.
		if (v->cur_speed != 0)
			return;
	} else {
		TrainCheckIfLineEnds(v);

		do {
			TrainController(v);
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

	if (v->subtype == TS_Front_Engine) {
		TrainLocoHandler(v, false);

		// make sure vehicle wasn't deleted.
		if (v->type == VEH_Train && v->subtype == TS_Front_Engine)
			TrainLocoHandler(v, true);
	} else if (v->subtype == TS_Free_Car && HASBITS(v->vehstatus, VS_CRASHED)) {
		// Delete flooded standalone wagon
		if (++v->u.rail.crash_anim_pos >= 4400)
			DeleteVehicle(v);
	}
}


static const byte _depot_track_ind[4] = {0,1,0,1};

// Validation for the news item "Train is waiting in depot"
static bool ValidateTrainInDepot( uint data_a, uint data_b )
{
	Vehicle *v = GetVehicle(data_a);
	return  (v->u.rail.track == 0x80 && (v->vehstatus | VS_STOPPED));
}

void TrainEnterDepot(Vehicle *v, TileIndex tile)
{
	SetSignalsOnBothDir(tile, _depot_track_ind[GB(_m[tile].m5, 0, 2)]);

	if (v->subtype != TS_Front_Engine) v = GetFirstVehicleInChain(v);

	VehicleServiceInDepot(v);

	InvalidateWindow(WC_VEHICLE_DETAILS, v->index);

	v->load_unload_time_rem = 0;
	v->cur_speed = 0;

	TriggerVehicle(v, VEHICLE_TRIGGER_DEPOT);

	if (v->current_order.type == OT_GOTO_DEPOT) {
		Order t;

		InvalidateWindow(WC_VEHICLE_VIEW, v->index);

		t = v->current_order;
		v->current_order.type = OT_DUMMY;
		v->current_order.flags = 0;

		if (HASBIT(t.flags, OFB_PART_OF_ORDERS)) { // Part of the orderlist?
			v->u.rail.days_since_order_progr = 0;
			v->cur_order_index++;
		} else if (HASBIT(t.flags, OFB_HALT_IN_DEPOT)) { // User initiated?
			v->vehstatus |= VS_STOPPED;
			if (v->owner == _local_player) {
				SetDParam(0, v->unitnumber);
				AddValidatedNewsItem(
					STR_8814_TRAIN_IS_WAITING_IN_DEPOT,
					NEWS_FLAGS(NM_SMALL, NF_VIEWPORT|NF_VEHICLE, NT_ADVICE, 0),
					v->index,
					0,
					ValidateTrainInDepot);
			}
		}
	}
	InvalidateWindowClasses(WC_TRAINS_LIST);
}

static void CheckIfTrainNeedsService(Vehicle *v)
{
	const Depot* depot;
	TrainFindDepotData tfdd;

	if (PBSTileReserved(v->tile) & v->u.rail.track)     return;
	if (v->u.rail.pbs_status == PBS_STAT_HAS_PATH)      return;
	if (_patches.servint_trains == 0)                   return;
	if (!VehicleNeedsService(v))                        return;
	if (v->vehstatus & VS_STOPPED)                      return;
	if (_patches.gotodepot && VehicleHasDepotOrders(v)) return;

	// Don't interfere with a depot visit scheduled by the user, or a
	// depot visit by the order list.
	if (v->current_order.type == OT_GOTO_DEPOT &&
			(v->current_order.flags & (OF_HALT_IN_DEPOT | OF_PART_OF_ORDERS)) != 0)
		return;

	tfdd = FindClosestTrainDepot(v);
	/* Only go to the depot if it is not too far out of our way. */
	if (tfdd.best_length == (uint)-1 || tfdd.best_length > 16 ) {
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
			v->current_order.station != depot->index &&
			!CHANCE16(3,16))
		return;

	v->current_order.type = OT_GOTO_DEPOT;
	v->current_order.flags = OF_NON_STOP;
	v->current_order.station = depot->index;
	v->dest_tile = tfdd.tile;
	InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
}

int32 GetTrainRunningCost(const Vehicle *v)
{
	int32 cost = 0;

	do {
		const RailVehicleInfo *rvi = RailVehInfo(v->engine_type);
		if (rvi->running_cost_base)
			cost += rvi->running_cost_base * _price.running_rail[rvi->engclass];
	} while ( (v=v->next) != NULL );

	return cost;
}

void OnNewDay_Train(Vehicle *v)
{
	TileIndex tile;

	if ((++v->day_counter & 7) == 0)
		DecreaseVehicleValue(v);

	if (v->subtype == TS_Front_Engine) {
		CheckVehicleBreakdown(v);
		AgeVehicle(v);

		CheckIfTrainNeedsService(v);

		// check if train hasn't advanced in its order list for a set number of days
		if (_patches.lost_train_days && v->num_orders && !(v->vehstatus & (VS_STOPPED | VS_CRASHED) ) && ++v->u.rail.days_since_order_progr >= _patches.lost_train_days && v->owner == _local_player) {
			v->u.rail.days_since_order_progr = 0;
			SetDParam(0, v->unitnumber);
			AddNewsItem(
				STR_TRAIN_IS_LOST,
				NEWS_FLAGS(NM_SMALL, NF_VIEWPORT|NF_VEHICLE, NT_ADVICE, 0),
				v->index,
				0);
		}

		CheckOrders(v->index, OC_INIT);

		/* update destination */
		if (v->current_order.type == OT_GOTO_STATION &&
				(tile = GetStation(v->current_order.station)->train_tile) != 0) {
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
		if (v->type == VEH_Train && v->subtype == TS_Front_Engine) {

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
