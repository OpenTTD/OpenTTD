#include "stdafx.h"
#include "ttd.h"
#include "table/strings.h"
#include "map.h"
#include "vehicle.h"
#include "command.h"
#include "pathfind.h"
#include "station.h"
#include "table/train_cmd.h"
#include "gfx.h"
#include "news.h"
#include "engine.h"
#include "player.h"
#include "sound.h"

#define is_firsthead_sprite(spritenum) \
	(is_custom_sprite(spritenum) \
		? is_custom_firsthead_sprite(spritenum) \
		: _engine_sprite_add[spritenum] == 0)

static bool TrainCheckIfLineEnds(Vehicle *v);

static const byte _vehicle_initial_x_fract[4] = {10,8,4,8};
static const byte _vehicle_initial_y_fract[4] = {8,4,8,10};
static const byte _state_dir_table[4] = { 0x20, 8, 0x10, 4 };

static const byte _signal_onedir[14] = {
	0x80, 0x80, 0x80, 0x20, 0x40, 0x10, 0, 0,
	0x40, 0x40, 0x40, 0x10, 0x80, 0x20
};

static const byte _signal_otherdir[14] = {
	0x40, 0x40, 0x40, 0x10, 0x80, 0x20, 0, 0,
	0x80, 0x80, 0x80, 0x20, 0x40, 0x10
};

void UpdateTrainAcceleration(Vehicle *v)
{
	uint acc, power=0, max_speed=5000, weight=0;
	Vehicle *u = v;

	assert(v->subtype == 0);

	// compute stuff like max speed, power, and weight.
	do {
		const RailVehicleInfo *rvi = RailVehInfo(u->engine_type);

		// power is sum of the power for all engines
		power += rvi->power;

		// limit the max speed to the speed of the slowest vehicle.
		if (rvi->max_speed && rvi->max_speed <= max_speed) max_speed = rvi->max_speed;

		// weight is the sum of the weight of the wagon and the weight of the cargo.
		weight += rvi->weight;
		weight += (_cargoc.weights[u->cargo_type] * u->cargo_count) >> 4;

	} while ( (u=u->next) != NULL);

	// these are shown in the UI
	v->u.rail.cached_weight = weight;
	v->u.rail.cached_power = power;
	v->max_speed = max_speed;

	assert(weight != 0);

	// compute acceleration
	acc = power / weight * 4;

	if (acc >= 255) acc=255;
	if (acc == 0) acc++;

	v->acceleration = (byte)acc;
}

#define F_GRAV 9.82f
#define F_THETA 0.05f

#define F_HP_KW 0.74569f
#define F_KPH_MS 0.27778f
#define F_MU 0.3f

#define F_COEF_FRIC 0.04f
#define F_COEF_ROLL 0.18f

#define F_CURVE_FACTOR (1/96.f)

static bool IsTunnelTile(TileIndex tile);

static int GetRealisticAcceleration(Vehicle *v)
{
	uint emass = 0;
	Vehicle *u = v;
	float f = 0.0f, spd;
	int curves = 0;

	assert(v->subtype == 0);

	// compute inclination force and number of curves.
	do {
		const RailVehicleInfo *rvi = RailVehInfo(u->engine_type);
		uint mass = rvi->weight + ((_cargoc.weights[u->cargo_type] * u->cargo_count) >> 4);
		if (rvi->power) emass += mass;

		if (u->u.rail.flags & VRF_GOINGUP) {
			f += (float)mass * ( -F_GRAV * F_THETA);
		} else if (u->u.rail.flags & VRF_GOINGDOWN) {
			f += (float)mass * ( F_GRAV * F_THETA);
		}

		// compute curve penalty..
		if (u->next != NULL) {
			uint diff = (u->direction - u->next->direction) & 7;
			if (diff) {
				curves += (diff == 1 || diff == 7) ? 1 : 3;
			}
		}
	} while ((u = u->next) != NULL);

	spd = (float)(v->cur_speed ? v->cur_speed : 1);

	// compute tractive effort
	{
		float te = (float)v->u.rail.cached_power * (F_HP_KW/F_KPH_MS) / spd;
		float te2 = (float)emass * (F_MU * F_GRAV);
		if (te > te2) te = te2;
		f += te;
	}

	// add air resistance
	{
		float cx = 1.0f; // NOT DONE

		// air resistance is doubled in tunnels.
		if (v->vehstatus == 0x40) cx *= 2;

		f -= cx * spd * spd * (F_KPH_MS * F_KPH_MS * 0.001f);
	}

	// after this f contains the acceleration.
	f /= (float)v->u.rail.cached_weight;

	// add friction to sum of forces (avoid mul by weight). (0.001 because we want kN)
	f -= (F_COEF_FRIC * F_GRAV * 0.001f + (F_COEF_ROLL * F_KPH_MS * F_GRAV * 0.001f) * spd);

	// penalty for curves?
	if (curves)
		 f -= (float)min(curves, 8) * F_CURVE_FACTOR;

	return (int)(f  * (1.0/(F_KPH_MS * 0.015f)) + 0.5f);
}


int GetTrainImage(Vehicle *v, byte direction)
{
	int img = v->spritenum;
	int base;

	if (is_custom_sprite(img)) {
		base = GetCustomVehicleSprite(v, direction + 4 * is_custom_secondhead_sprite(img));
		if (base) return base;
		img = _engine_original_sprites[v->engine_type];
	}

	base = _engine_sprite_base[img] + ((direction + _engine_sprite_add[img]) & _engine_sprite_and[img]);

	if (v->cargo_count >= (v->cargo_cap >> 1))
		base += _wagon_full_adder[img];
	return base;
}

void DrawTrainEngine(int x, int y, int engine, uint32 image_ormod)
{
	const RailVehicleInfo *rvi = RailVehInfo(engine);

	int img = rvi->image_index;
	uint32 image = 0;

	if (is_custom_sprite(img)) {
		image = GetCustomVehicleIcon(engine, 6);
		if (!image) img = _engine_original_sprites[engine];
	}
	if (!image) {
		image = (6 & _engine_sprite_and[img]) + _engine_sprite_base[img];
	}

	if (rvi->flags & RVI_MULTIHEAD) {
		DrawSprite(image | image_ormod, x-14, y);
		x += 15;
		image = 0;
		if (is_custom_sprite(img)) {
			image = GetCustomVehicleIcon(engine, 2);
			if (!image) img = _engine_original_sprites[engine];
		}
		if (!image) {
			image = ((6 + _engine_sprite_add[img+1]) & _engine_sprite_and[img+1]) + _engine_sprite_base[img+1];
		}
	}
	DrawSprite(image | image_ormod, x, y);
}

void DrawTrainEngineInfo(int engine, int x, int y, int maxw)
{
	const RailVehicleInfo *rvi = RailVehInfo(engine);
	int cap;
	uint multihead = ((rvi->flags & RVI_MULTIHEAD) ? 1 : 0);

	SetDParam(0, ((_price.build_railvehicle >> 3) * rvi->base_cost) >> 5);
	SetDParam(2, rvi->max_speed * 10 >> 4);
	SetDParam(3, rvi->power << multihead);
	SetDParam(1, rvi->weight << multihead);

	SetDParam(4, (rvi->running_cost_base * _price.running_rail[rvi->engclass] >> 8) << multihead);

	cap = rvi->capacity;
	SetDParam(5, STR_8838_N_A);
	if (cap != 0) {
		SetDParam(6, cap << multihead);
		SetDParam(5, _cargoc.names_long_p[rvi->cargo_type]);
	}
	DrawStringMultiCenter(x, y, STR_885B_COST_WEIGHT_T_SPEED_POWER, maxw);
}


static int32 CmdBuildRailWagon(uint engine, uint tile, uint32 flags)
{
	int32 value;
	Vehicle *v;
	const RailVehicleInfo *rvi;
	int dir;
	const Engine *e;
	int x,y;

	rvi = RailVehInfo(engine);
	value = (rvi->base_cost * _price.build_railwagon) >> 8;

	if (!(flags & DC_QUERY_COST)) {
		_error_message = STR_00E1_TOO_MANY_VEHICLES_IN_GAME;

		v = AllocateVehicle();
		if (v == NULL)
			return CMD_ERROR;

		if (flags & DC_EXEC) {
			byte img = rvi->image_index;
			Vehicle *u;

			v->spritenum = img;

			u = _vehicles;
			for(;;) {
				if (u->type == VEH_Train && u->tile == (TileIndex)tile &&
				    u->subtype == 4 && u->engine_type == engine) {
					u = GetLastVehicleInChain(u);
					break;
				}

				if (++u == endof(_vehicles)) {
					u = NULL;
					break;
				}
			}

			v->engine_type = engine;

			dir = _map5[tile] & 3;

			v->direction = (byte)(dir*2+1);
			v->tile = (TileIndex)tile;

			x = GET_TILE_X(tile)*16 | _vehicle_initial_x_fract[dir];
			y = GET_TILE_Y(tile)*16 | _vehicle_initial_y_fract[dir];

			v->x_pos = x;
			v->y_pos = y;
			v->z_pos = GetSlopeZ(x,y);
			v->owner = _current_player;
			v->z_height = 6;
			v->u.rail.track = 0x80;
			v->vehstatus = VS_HIDDEN | VS_DEFPAL;

			v->subtype = 4;
			if (u != NULL) {
				u->next = v;
				v->subtype = 2;
				v->u.rail.first_engine = u->u.rail.first_engine;
				if (v->u.rail.first_engine == 0xffff && u->subtype == 0)
					v->u.rail.first_engine = u->engine_type;
			} else {
				v->u.rail.first_engine = 0xffff;
			}

			v->cargo_type = rvi->cargo_type;
			v->cargo_cap = rvi->capacity;
			v->value = value;
//			v->day_counter = 0;

			e = &_engines[engine];
			v->u.rail.railtype = e->railtype;

			v->build_year = _cur_year;
			v->type = VEH_Train;
			v->cur_image = 0xAC2;

			_new_wagon_id = v->index;

			VehiclePositionChanged(v);

			InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
		}
	}

	return value;
}

// Move all free vehicles in the depot to the train
static void NormalizeTrainVehInDepot(Vehicle *u)
{
	Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Train && v->subtype==4 &&
				v->tile == u->tile &&
				v->u.rail.track == 0x80) {
			if (DoCommandByTile(0,v->index | (u->index<<16), 1, DC_EXEC, CMD_MOVE_RAIL_VEHICLE) == CMD_ERROR)
				break;
		}
	}
}

static const byte _railveh_unk1[] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 1, 1, 0, 0, 0,
	0, 0, 0, 0, 1, 0, 1, 0,
	0, 1, 1, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 1,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0,
};

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


static int32 EstimateTrainCost(const RailVehicleInfo *rvi)
{
	return (rvi->base_cost * (_price.build_railvehicle >> 3)) >> 5;
}

/* Build a railroad vehicle
 * p1 = vehicle type id
 */

int32 CmdBuildRailVehicle(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	const RailVehicleInfo *rvi;
	int value,dir;
	Vehicle *v, *u;
	byte unit_num;
	Engine *e;
	uint tile;

	_cmd_build_rail_veh_var1 = 0;

	SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);

	tile = TILE_FROM_XY(x,y);
	rvi = RailVehInfo(p1);

	if (rvi->flags & RVI_WAGON) {
		return CmdBuildRailWagon(p1, tile, flags);
	}

	value = EstimateTrainCost(rvi);

	if (!(flags & DC_QUERY_COST)) {
		v = AllocateVehicle();
		if (v == NULL || _ptr_to_next_order >= endof(_order_array))
			return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);

		unit_num = GetFreeUnitNumber(VEH_Train);
		if (unit_num > _patches.max_trains)
			return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);

		if (flags & DC_EXEC) {
			v->unitnumber = unit_num;

			dir = _map5[tile] & 3;

			v->direction = (byte)(dir*2+1);
			v->tile = (TileIndex)tile;
			v->owner = _current_player;
			v->x_pos = (x |= _vehicle_initial_x_fract[dir]);
			v->y_pos = (y |= _vehicle_initial_y_fract[dir]);
			v->z_pos = GetSlopeZ(x,y);
			v->z_height = 6;
			v->u.rail.track = 0x80;
			v->u.rail.first_engine = 0xffff;
			v->vehstatus = VS_HIDDEN | VS_STOPPED | VS_DEFPAL;
//			v->subtype = 0;
			v->spritenum = rvi->image_index;
			v->cargo_type = rvi->cargo_type;
			v->cargo_cap = rvi->capacity;
			v->max_speed = rvi->max_speed;
//			v->cargo_count = 0;
			v->value = value;
//			v->day_counter = 0;
//			v->current_order = 0;
//			v->next_station = 0;
//			v->load_unload_time_rem = 0;
//			v->progress = 0;
//			v->targetairport = 0;
//			v->crash_anim_pos = 0;
			v->last_station_visited = 0xff;
			v->dest_tile = 0;
//			v->profit_last_year = 0;
//			v->profit_this_year = 0;

			v->engine_type = (byte)p1;
			e = &_engines[p1];

			v->reliability = e->reliability;
			v->reliability_spd_dec = e->reliability_spd_dec;
			v->max_age = e->lifelength * 366;

			v->string_id = STR_SV_TRAIN_NAME;
//			v->cur_speed = 0;
//			v->subspeed = 0;
			v->u.rail.railtype = e->railtype;
			_new_train_id = v->index;
//			v->cur_order_index = 0;
//			v->num_orders = 0;

			_ptr_to_next_order->type = OT_NOTHING;
			_ptr_to_next_order->flags = 0;
			v->schedule_ptr = _ptr_to_next_order++;
//			v->next_in_chain = 0xffff;
//			v->next = NULL;

			v->service_interval = _patches.servint_trains;
//			v->breakdown_ctr = 0;
//			v->breakdowns_since_last_service = 0;
//			v->unk4D = 0;
			v->date_of_last_service = _date;
			v->build_year = _cur_year;
			v->type = VEH_Train;
			v->cur_image = 0xAC2;

			VehiclePositionChanged(v);

			if (rvi->flags&RVI_MULTIHEAD && (u=AllocateVehicle()) != NULL) {
				u->direction = v->direction;
				u->owner = v->owner;
				u->tile = v->tile;
				u->x_pos = v->x_pos;
				u->y_pos = v->y_pos;
				u->z_pos = v->z_pos;
				u->z_height = 6;
				u->u.rail.track = 0x80;
				v->u.rail.first_engine = 0xffff;
				u->vehstatus = VS_HIDDEN | VS_DEFPAL;
				u->subtype = 2;
				u->spritenum = v->spritenum + 1;
				u->cargo_type = v->cargo_type;
				u->cargo_cap = v->cargo_cap;
				u->u.rail.railtype = v->u.rail.railtype;
//				u->next_in_chain = 0xffff;
				v->next = u;
				u->engine_type = v->engine_type;
				u->build_year = v->build_year;
				v->value = u->value = v->value >> 1;
//				u->day_counter = 0;
				u->type = VEH_Train;
				u->cur_image = 0xAC2;
				VehiclePositionChanged(u);
			}

			UpdateTrainAcceleration(v);
			NormalizeTrainVehInDepot(v);

			InvalidateWindow(WC_VEHICLE_DEPOT, tile);
			RebuildVehicleLists();
			InvalidateWindow(WC_COMPANY, v->owner);
		}
	}
	_cmd_build_rail_veh_var1 = _railveh_unk1[p1];
	_cmd_build_rail_veh_score = _railveh_score[p1];
	return value;
}


bool IsTrainDepotTile(TileIndex tile)
{
	return IS_TILETYPE(tile, MP_RAILWAY) &&
					(_map5[tile] & 0xFC) == 0xC0;
}

static bool IsTunnelTile(TileIndex tile)
{
	return IS_TILETYPE(tile, MP_TUNNELBRIDGE) &&
				 (_map5[tile]&0x80) == 0;
}


int CheckStoppedInDepot(Vehicle *v)
{
	int count;
	TileIndex tile = v->tile;

	/* check if stopped in a depot */
	if (!IsTrainDepotTile(tile) || v->cur_speed != 0) {
errmsg:
		_error_message = STR_881A_TRAINS_CAN_ONLY_BE_ALTERED;
		return -1;
	}

	count = 0;
	do {
		count++;
		if (v->u.rail.track != 0x80 || v->tile != (TileIndex)tile ||
				(v->subtype==0 && !(v->vehstatus&VS_STOPPED)))
			goto errmsg;
	} while ( (v=v->next) != NULL);

	return count;
}

// unlink a rail wagon from the linked list.
// returns the new value of first
static Vehicle *UnlinkWagon(Vehicle *v, Vehicle *first)
{
	// unlinking the first vehicle of the chain?
	v->u.rail.first_engine = 0xffff;
	if (v == first) {
		Vehicle *u;
		if ((v=v->next) == NULL) return NULL;
		for (u=v; u; u=u->next) u->u.rail.first_engine = v->engine_type;
		v->subtype = 4;
		return v;
	} else {
		Vehicle *u;
		for(u=first; u->next!=v; u=u->next) {}
		u->next = v->next;
		return first;
	}
}

static Vehicle *FindGoodVehiclePos(Vehicle *src)
{
	Vehicle *dst;
	uint16 eng = src->engine_type;
	TileIndex tile = src->tile;

	FOR_ALL_VEHICLES(dst) {
		if (dst->type==VEH_Train && dst->subtype==4 && dst->tile==tile) {
			// check so all vehicles in the line have the same engine.
			Vehicle *v = dst;
			while (v->engine_type == eng) {
				if ((v = v->next) == NULL) return dst;
			}
		}
	}

	return NULL;
}

/* p1 & 0xffff= source vehicle index
   p1 & 0xffff0000 = what wagon to put the wagon AFTER, 0xffff0000 to make a new line
	 p2 & 1 = move all vehicles following the vehicle..
 */

int32 CmdMoveRailVehicle(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *src, *dst, *src_head, *dst_head;
	bool is_loco;

	src = &_vehicles[p1 & 0xffff];
	if (src->type != VEH_Train) return CMD_ERROR;

	is_loco = !(RailVehInfo(src->engine_type)->flags & RVI_WAGON)
	          && is_firsthead_sprite(src->spritenum);

	// if nothing is selected as destination, try and find a matching vehicle to drag to.
	if (((int32)p1 >> 16) == -1) {
		dst = NULL;
		if (!is_loco) dst = FindGoodVehiclePos(src);
	} else {
		dst = &_vehicles[((int32)p1 >> 16)];
	}

	// don't move the same vehicle..
	if (src == dst)
		return 0;

	/* the player must be the owner */
	if (!CheckOwnership(src->owner) || (dst!=NULL && !CheckOwnership(dst->owner)))
		return CMD_ERROR;

	/* locate the head of the two chains */
	src_head = GetFirstVehicleInChain(src);
	dst_head = NULL;
	if (dst != NULL) dst_head = GetFirstVehicleInChain(dst);

	/* check if all vehicles in the source train are stopped */
	if (CheckStoppedInDepot(src_head) < 0)
		return CMD_ERROR;

	/* check if all the vehicles in the dest train are stopped,
	 * and that the length of the dest train is no longer than XXX vehicles */
	if (dst_head != NULL) {
		int num = CheckStoppedInDepot(dst_head);
		if (num < 0)
			return CMD_ERROR;

		if (num > (_patches.mammoth_trains ? 100 : 9) && dst_head->subtype==0)
			return_cmd_error(STR_8819_TRAIN_TOO_LONG);

		// if it's a multiheaded vehicle we're dragging to, drag to the vehicle before..
		while (is_custom_secondhead_sprite(dst->spritenum)
		       || (!is_custom_sprite(dst->spritenum) && _engine_sprite_add[dst->spritenum] != 0)) {
			Vehicle *v = GetPrevVehicleInChain(dst);
			if (!v || src == v) break;
			dst = v;
		}

		assert(dst_head->tile == src_head->tile);
	}

	// when moving all wagons, we can't have the same src_head and dst_head
	if (p2 & 1 && src_head == dst_head)
		return 0;

	// moving a loco to a new line?, then we need to assign a unitnumber.
	if (dst == NULL && src->subtype != 0 && is_loco) {
		uint unit_num = GetFreeUnitNumber(VEH_Train);
		if (unit_num > _patches.max_trains)
			return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);

		if (flags & DC_EXEC)
			src->unitnumber = unit_num;
	}


	/* do it? */
	if (flags & DC_EXEC) {
		if (p2 & 1) {
			// unlink ALL wagons
			if (src != src_head) {
				Vehicle *v = src_head;
				while (v->next != src) v=v->next;
				v->next = NULL;
			}
		} else {
			// unlink single wagon from linked list
			UnlinkWagon(src, src_head);
			src->next = NULL;
		}

		if (dst == NULL) {
			// move the train to an empty line. for locomotives, we set the type to 0. for wagons, 4.
			if (is_loco) {
				if (src->subtype != 0) {
					// setting the type to 0 also involves setting up the schedule_ptr field.
					src->subtype = 0;
					assert(src->schedule_ptr == NULL);
					_ptr_to_next_order->type = OT_NOTHING;
					_ptr_to_next_order->flags = 0;
					src->schedule_ptr = _ptr_to_next_order++;
					src->num_orders = 0;
				}
				dst_head = src;
			} else {
				src->subtype = 4;
			}
			src->u.rail.first_engine = 0xffff;
		} else {
			if (src->subtype == 0) {
				// the vehicle was previously a loco. need to free the schedule list and delete vehicle windows etc.
				DeleteWindowById(WC_VEHICLE_VIEW, src->index);
				DeleteVehicleSchedule(src);
			}

			src->subtype = 2;
			src->unitnumber = 0; // doesn't occupy a unitnumber anymore.

			// setup first_engine
			src->u.rail.first_engine = dst->u.rail.first_engine;
			if (src->u.rail.first_engine == 0xffff && dst->subtype == 0)
				src->u.rail.first_engine = dst->engine_type;

			// link in the wagon(s) in the chain.
			{
				Vehicle *v = src;
				while (v->next != NULL) {
					v->next->u.rail.first_engine = v->u.rail.first_engine;
					v = v->next;
				}
				v->next = dst->next;
			}
			dst->next = src;
		}

		if (src_head->subtype == 0)
			UpdateTrainAcceleration(src_head);
		InvalidateWindow(WC_VEHICLE_DETAILS, src_head->index);

		if (dst_head) {
			if (dst_head->subtype == 0)
				UpdateTrainAcceleration(dst_head);
			InvalidateWindow(WC_VEHICLE_DETAILS, dst_head->index);
		}

		InvalidateWindow(WC_VEHICLE_DEPOT, src_head->tile);
		RebuildVehicleLists();
	}

	return 0;
}

/* p1 = train to start / stop */
int32 CmdStartStopTrain(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;

	v = &_vehicles[p1];

	if (!CheckOwnership(v->owner))
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		v->u.rail.days_since_order_progr = 0;
		v->vehstatus ^= VS_STOPPED;
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, 4);
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
	}
	return 0;
}

// p1 = wagon/loco index
// p2 = mode
//   0: sell just the vehicle
//   1: sell the vehicle and all vehicles that follow it in the chain
//   2: when selling attached locos, rearrange all vehicles after it to separate lines.
int32 CmdSellRailWagon(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v, *first,*last;
	int32 cost;

	SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);

	v = &_vehicles[p1];

	if (!CheckOwnership(v->owner))
		return CMD_ERROR;

	// get first vehicle in chain
	first = v;
	if (first->subtype != 0) {
		first = GetFirstVehicleInChain(first);
		last = GetLastVehicleInChain(first);
		//now if:
		// 1) we delete a whole a chain, and
		// 2) we don't actually try to delete the last engine
		// 3) the first and the last vehicle of that train are of the same type, and
		// 4) the first and the last vehicle of the chain are not identical
		// 5) and of "engine" type (i.e. not a carriage)
		// then let the last vehicle live
		if ( (p2 == 1) && (v != last) && ( last->engine_type == first->engine_type ) && (last != first) && (first->subtype == 0) )
			last = GetPrevVehicleInChain(last);
		else
			last = NULL;
	} else {
		if (p2 != 1) {
			// sell last part of multiheaded?
			last = GetLastVehicleInChain(v);
			// Check if the end-part is the same engine and check if it is the rear-end
			if (last->engine_type != first->engine_type || is_firsthead_sprite(last->spritenum))
				last = NULL;
		} else {
			last = NULL;
		}
	}

	// make sure the vehicle is stopped in the depot
	if (CheckStoppedInDepot(first) < 0)
		return CMD_ERROR;


	if (flags & DC_EXEC) {
		// always redraw the depot. maybe redraw train list
		InvalidateWindow(WC_VEHICLE_DEPOT, first->tile);
		if (first->subtype == 0) {
			RebuildVehicleLists();
		}
		// when selling an attached locomotive. we need to delete its window.
		if (v->subtype == 0) {
			DeleteWindowById(WC_VEHICLE_VIEW, v->index);

			// rearrange all vehicles that follow to separate lines.
			if (p2 == 2) {
				Vehicle *u,*tmp;
				u = v->next;
				while (u != last) {
					tmp = u;
					u = u->next;
					DoCommandByTile(tmp->tile, tmp->index | ((-1)<<16), 0, DC_EXEC, CMD_MOVE_RAIL_VEHICLE);
				}
			}
		}

		// delete the vehicles
		cost = 0;
		for(;;) {
			Vehicle *tmp;

			assert(first);
			first = UnlinkWagon(v, first);
			cost -= v->value;
			tmp = v;
			DeleteVehicle(tmp);
			if ( v == last ) {
				last = NULL;
				break;
			}
			if ( (v=v->next) == last || p2 != 1) break;
		}

		// delete last vehicle of multiheaded train?
		if (last) {
			first = UnlinkWagon(last, first);
			cost -= last->value;
			DeleteVehicle(last);
		}

		// an attached train changed?
		if (first && first->subtype == 0) {
			UpdateTrainAcceleration(first);
			InvalidateWindow(WC_VEHICLE_DETAILS, first->index);
		}
	} else {
		cost = 0;
		for(;;) {
			cost -= v->value;
			if ( v == last ) {
				last = NULL;
				break;
			}
			if ( (v=v->next) == last || p2 != 1) break;
		}
		if (last) cost -= last->value;
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

	v->x_offs = (byte)x;
	v->y_offs = (byte)(x>>=8);
	v->sprite_width = (byte)(x>>=8);
	v->sprite_height = (byte)(x>>=8);
}

static void UpdateVarsAfterSwap(Vehicle *v)
{
	UpdateTrainDeltaXY(v, v->direction);
	v->cur_image = GetTrainImage(v, v->direction);
	BeginVehicleMove(v);
	VehiclePositionChanged(v);
	EndVehicleMove(v);
}

static void SetLastSpeed(Vehicle *v, int spd) {
	int old = v->u.rail.last_speed;
	if (spd != old) {
		v->u.rail.last_speed = spd;
		if (_patches.vehicle_speed || !old != !spd)
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, 4);
	}
}

static void ReverseTrainSwapVeh(Vehicle *v, int l, int r)
{
	Vehicle *a, *b;

	/* locate vehicles to swap */
	for(a=v; l!=0; l--) { a = a->next; }
	for(b=v; r!=0; r--) { b = b->next; }

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
		swap_int16(&a->x_pos, &b->x_pos);
		swap_int16(&a->y_pos, &b->y_pos);
		swap_tile(&a->tile, &b->tile);
		swap_byte(&a->z_pos, &b->z_pos);

		/* update other vars */
		UpdateVarsAfterSwap(a);
		UpdateVarsAfterSwap(b);
	} else {
		if (!(a->u.rail.track & 0x80)) a->direction ^= 4;
		UpdateVarsAfterSwap(a);
	}
}

static void ReverseTrainDirection(Vehicle *v)
{
	int l = 0, r = -1;
	Vehicle *u;

	if (IsTrainDepotTile(v->tile))
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);

	/* Check if we were approaching a rail/road-crossing */
	{
		TileIndex tile = v->tile;
		int t;
		/* Determine the non-diagonal direction in which we will exit this tile */
		t = v->direction >> 1;
		if (!(v->direction & 1) && v->u.rail.track != _state_dir_table[t]) {
			t = (t - 1) & 3;
		}
		/* Calculate next tile */
		tile += _tileoffs_by_dir[t];
		if (IS_TILETYPE(tile, MP_STREET) && (_map5[tile] & 0xF0)==0x10) {
			if (_map5[tile] & 4) {
				_map5[tile] &= ~4;
				MarkTileDirtyByTile(tile);
			}
		}
	}

	// count number of vehicles
	u = v;
	do r++; while ( (u = u->next) != NULL );

	/* swap start<>end, start+1<>end-1, ... */
	do {
		ReverseTrainSwapVeh(v, l++, r--);
	} while (l <= r);

	if (IsTrainDepotTile(v->tile))
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);

	v->u.rail.flags &= ~VRF_REVERSING;
}

/* p1 = vehicle */
int32 CmdReverseTrainDirection(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;

	v = &_vehicles[p1];

	if (!CheckOwnership(v->owner))
		return CMD_ERROR;

	_error_message = STR_EMPTY;

//	if (v->u.rail.track & 0x80 || IsTrainDepotTile(v->tile))
//		return CMD_ERROR;

	if (v->u.rail.crash_anim_pos != 0 || v->breakdown_ctr != 0)
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		if (_patches.realistic_acceleration && v->cur_speed != 0) {
			v->u.rail.flags ^= VRF_REVERSING;
		} else {
			v->cur_speed = 0;
			SetLastSpeed(v, 0);
			ReverseTrainDirection(v);
		}
	}
	return 0;
}

int32 CmdForceTrainProceed(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;

	v = &_vehicles[p1];

	if (!CheckOwnership(v->owner))
		return CMD_ERROR;

	if (flags & DC_EXEC)
		v->u.rail.force_proceed = 0x50;

	return 0;
}

// p1 = vehicle to refit
// p2 = new cargo

int32 CmdRefitRailVehicle(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;
	int32 cost;
	uint num;

	SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);

	v = &_vehicles[p1];
	if (!CheckOwnership(v->owner) || CheckStoppedInDepot(v) < 0)
		return CMD_ERROR;

	cost = 0;
	num = 0;

	do {
		/* XXX: We also refit all the attached wagons en-masse if they
		 * can be refitted. This is how TTDPatch does it.  TODO: Have
		 * some nice [Refit] button near each wagon. --pasky */
		if ((!(RailVehInfo(v->engine_type)->flags & RVI_WAGON)
		     || (_engine_refit_masks[v->engine_type] & (1 << p2)))
		    && (byte) p2 != v->cargo_type && v->cargo_cap != 0) {
			cost += (_price.build_railvehicle >> 8);
			num += v->cargo_cap;
			if (flags & DC_EXEC) {
				v->cargo_count = 0;
				v->cargo_type = (byte)p2;
				InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
			}
		}
	} while ( (v=v->next) != NULL);

	_returned_refit_amount = num;

	return cost;
}

int GetDepotByTile(uint tile)
{
	Depot *d;
	int i=0;
	for(d=_depots; d->xy != (TileIndex)tile; d++) { i++; }
	return i;
}

typedef struct TrainFindDepotData {
	uint best_length;
	uint tile;
	byte owner;
} TrainFindDepotData;

static bool TrainFindDepotEnumProc(uint tile, TrainFindDepotData *tfdd, int track, uint length, byte *state)
{
	if (IS_TILETYPE(tile, MP_RAILWAY) && _map_owner[tile] == tfdd->owner) {
		if ((_map5[tile] & ~0x3) == 0xC0) {
			if (length < tfdd->best_length) {
				tfdd->best_length = length;
				tfdd->tile = tile;
			}
			return true;
		}

		// make sure the train doesn't run against a oneway signal
		if ((_map5[tile] & 0xC0) == 0x40) {
			if (!(_map3_lo[tile] & _signal_onedir[track]) && _map3_lo[tile] & _signal_otherdir[track])
				return true;
		}
	}

	// stop  searching if we've found a destination that is closer already.
	return length >= tfdd->best_length;
}

// returns the tile of a depot to goto to.
static TrainFindDepotData FindClosestTrainDepot(Vehicle *v)
{
	int i;
	TrainFindDepotData tfdd;
	uint tile = v->tile;

	tfdd.owner = v->owner;
	tfdd.best_length = (uint)-1;

	if (IsTrainDepotTile(tile)){
		tfdd.tile = tile;
		tfdd.best_length = 0;
		return tfdd;
	}

	if (v->u.rail.track == 0x40) { tile = GetVehicleOutOfTunnelTile(v); }

	if (!_patches.new_depot_finding) {
		// search in all directions
		for(i=0; i!=4; i++)
			NewTrainPathfind(tile, i, (TPFEnumProc*)TrainFindDepotEnumProc, &tfdd, NULL);
	} else {
		// search in the forward direction first.
		i = v->direction >> 1;
		if (!(v->direction & 1) && v->u.rail.track != _state_dir_table[i]) { i = (i - 1) & 3; }
		NewTrainPathfind(tile, i, (TPFEnumProc*)TrainFindDepotEnumProc, &tfdd, NULL);
		if (tfdd.best_length == (uint)-1){
			// search in backwards direction
			i = (v->direction^4) >> 1;
			if (!(v->direction & 1) && v->u.rail.track != _state_dir_table[i]) { i = (i - 1) & 3; }
			NewTrainPathfind(tile, i, (TPFEnumProc*)TrainFindDepotEnumProc, &tfdd, NULL);
		}
	}

	return tfdd;
}

int32 CmdTrainGotoDepot(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v = &_vehicles[p1];
	TrainFindDepotData tfdd;

	if (v->current_order.type == OT_GOTO_DEPOT) {
		if (flags & DC_EXEC) {
			if (v->current_order.flags & OF_UNLOAD) {
				v->u.rail.days_since_order_progr = 0;
				v->cur_order_index++;
			}

			v->current_order.type = OT_DUMMY;
			v->current_order.flags = 0;
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, 4);
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
		v->current_order.station = GetDepotByTile(tfdd.tile);
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, 4);
	}

	return 0;
}

/* p1 = vehicle
 * p2 = new service int
 */
int32 CmdChangeTrainServiceInt(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v = &_vehicles[p1];

	if (!CheckOwnership(v->owner))
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		v->service_interval = (uint16)p2;
		InvalidateWindowWidget(WC_VEHICLE_DETAILS, v->index, 8);
	}

	return 0;
}

void OnTick_Train()
{
	_age_cargo_skip_counter = (_age_cargo_skip_counter == 0) ? 184 : (_age_cargo_skip_counter - 1);
}

static const int8 _vehicle_smoke_pos[16] = {
	-4, -4, -4, 0, 4, 4, 4, 0,
	-4,  0,  4, 4, 4, 0,-4,-4,
};

static void HandleLocomotiveSmokeCloud(Vehicle *v)
{
	Vehicle *u;

	if (v->vehstatus & VS_TRAIN_SLOWING || v->load_unload_time_rem != 0 || v->cur_speed < 2)
		return;

	u = v;

	do {
		int engtype = v->engine_type;

		// no smoke?
		if (RailVehInfo(engtype)->flags & 2
		    || _engines[engtype].railtype > 0
		    || (v->vehstatus&VS_HIDDEN) || (v->u.rail.track & 0xC0) )
			continue;

		switch (RailVehInfo(engtype)->engclass) {
		case 0:
			// steam smoke.
			if ( (v->tick_counter&0xF) == 0 && !IsTrainDepotTile(v->tile) && !IsTunnelTile(v->tile)) {
				CreateEffectVehicleRel(v,
					(_vehicle_smoke_pos[v->direction]),
					(_vehicle_smoke_pos[v->direction+8]),
					10,
					EV_STEAM_SMOKE);
			}
			break;

		case 1:
			// diesel smoke
			if (u->cur_speed <= 40 && !IsTrainDepotTile(v->tile) && !IsTunnelTile(v->tile) && (uint16)Random() <= 0x1E00) {
				CreateEffectVehicleRel(v, 0,0,10, EV_SMOKE_3);
			}
			break;

		case 2:
			// blue spark
			if ( (v->tick_counter&0x3) == 0 && !IsTrainDepotTile(v->tile) && !IsTunnelTile(v->tile) && (uint16)Random() <= 0x5B0) {
				CreateEffectVehicleRel(v, 0,0,10, EV_SMOKE_2);
			}
			break;
		}
	} while ( (v = v->next) != NULL );

}

static void TrainPlayLeaveStationSound(Vehicle *v)
{
	static const SoundFx sfx[] = {
		SND_04_TRAIN,
		SND_0A_TRAIN_HORN,
		SND_0A_TRAIN_HORN
	};

	int engtype = v->engine_type;

	switch (_engines[engtype].railtype) {
		case 0:
			SndPlayVehicleFx(sfx[RailVehInfo(engtype)->engclass], v);
			break;
		case 1:
			SndPlayVehicleFx(SND_47_MAGLEV_2, v);
			break;
		case 2:
			SndPlayVehicleFx(SND_41_MAGLEV, v);
			break;
	}
}

static bool CheckTrainStayInDepot(Vehicle *v)
{
	Vehicle *u;

	// bail out if not all wagons are in the same depot or not in a depot at all
	for (u = v; u != NULL; u = u->next)
		if (u->u.rail.track != 0x80 || u->tile != v->tile)
			return false;

	if (v->u.rail.force_proceed == 0) {
		if (++v->load_unload_time_rem < 37)
			return true;
		v->load_unload_time_rem = 0;

		if (UpdateSignalsOnSegment(v->tile, v->direction))
			return true;
	}

	VehicleServiceInDepot(v);
	TrainPlayLeaveStationSound(v);

	v->u.rail.track = 1;
	if (v->direction & 2)
		v->u.rail.track = 2;

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

typedef struct TrainTrackFollowerData {
	TileIndex dest_coords;
	int station_index; // station index we're heading for
	uint best_bird_dist;
	uint best_track_dist;
	byte best_track;
} TrainTrackFollowerData;

static bool TrainTrackFollower(uint tile, TrainTrackFollowerData *ttfd, int track, uint length, byte *state){
	if (IS_TILETYPE(tile, MP_RAILWAY) && (_map5[tile]&0xC0) == 0x40) {
		// the tile has a signal
		byte m3 = _map3_lo[tile];
		if (!(m3 & _signal_onedir[track])) {
			// if one way signal not pointing towards us, stop going in this direction.
			if (m3 & _signal_otherdir[track])
				return true;
		} else if (_map2[tile] & _signal_onedir[track]) {
			// green signal in our direction. either one way or two way.
			*state = true;
		} else if (m3 & _signal_otherdir[track]) {
			// two way signal. unless we passed another green signal on the way,
			// stop going in this direction.
			if (!*state) return true;
		}
	}

	// heading for nowhere?
	if (ttfd->dest_coords == 0)
		return false;

	// did we reach the final station?
 if ((ttfd->station_index == -1 && tile == ttfd->dest_coords) ||
  (IS_TILETYPE(tile, MP_STATION) && IS_BYTE_INSIDE(_map5[tile], 0, 8) && _map2[tile] == ttfd->station_index)) {
  /* We do not check for dest_coords if we have a station_index,
   * because in that case the dest_coords are just an
   * approximation of where the station is */
		// found station
		ttfd->best_bird_dist = 0;
		if (length < ttfd->best_track_dist) {
			ttfd->best_track_dist = length;
			ttfd->best_track = state[1];
		}
		return true;
	} else {
		uint dist;

		// we've actually found the destination already. no point searching in directions longer than this.
		if (ttfd->best_track_dist != (uint)-1)
			return length >= ttfd->best_track_dist;

		// didn't find station
		dist = GetTileDist(tile, ttfd->dest_coords);
		if (dist < ttfd->best_bird_dist) {
			ttfd->best_bird_dist = dist;
			ttfd->best_track = state[1];
		}
		return false;
	}
}

static void FillWithStationData(TrainTrackFollowerData *fd, Vehicle *v)
{
        fd->dest_coords = v->dest_tile;
        if (v->current_order.type == OT_GOTO_STATION)
                fd->station_index = v->current_order.station;
        else
                fd->station_index = -1;

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

/* choose a track */
static byte ChooseTrainTrack(Vehicle *v, uint tile, int direction, byte trackbits)
{
	TrainTrackFollowerData fd;
	int bits = trackbits;
	uint best_track;
#if 0
	int time = rdtsc();
	static float f;
#endif

	assert( (bits & ~0x3F) == 0);

	/* quick return in case only one possible direction is available */
	if (KILL_FIRST_BIT(bits) == 0)
		return FIND_FIRST_BIT(bits);

	FillWithStationData(&fd, v);

	if (_patches.new_pathfinding) {
		fd.best_bird_dist = (uint)-1;
		fd.best_track_dist = (uint)-1;
		fd.best_track = 0xFF;
		NewTrainPathfind(tile - _tileoffs_by_dir[direction], direction, (TPFEnumProc*)TrainTrackFollower, &fd, NULL);

//		printf("Train %d %s\n", v->unitnumber, fd.best_track_dist == -1 ? "NOTFOUND" : "FOUND");

		if (fd.best_track == 0xff) {
			// blaha
			best_track = FIND_FIRST_BIT(bits);
		} else {
			best_track = fd.best_track & 7;
		}
	} else {
		int i, r;
		uint best_bird_dist  = 0;
		uint best_track_dist = 0;
		byte train_dir = v->direction & 3;


		best_track = -1;

		do {
			i = FIND_FIRST_BIT(bits);
			bits = KILL_FIRST_BIT(bits);

			fd.best_bird_dist = (uint)-1;
			fd.best_track_dist = (uint)-1;

			NewTrainPathfind(tile, _search_directions[i][direction], (TPFEnumProc*)TrainTrackFollower, &fd, NULL);
			if (best_track != -1) {
				if (best_track_dist == -1) {
					if (fd.best_track_dist == -1) {
						/* neither reached the destination, pick the one with the smallest bird dist */
						if (fd.best_bird_dist > best_bird_dist) goto bad;
						if (fd.best_bird_dist < best_bird_dist) goto good;
					} else {
						/* we found the destination for the first time */
						goto good;
					}
				} else {
					if (fd.best_track_dist == -1) {
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
				r = (byte)Random();
				if (_pick_track_table[i] == train_dir) r += 80;
				if (_pick_track_table[best_track] == train_dir) r -= 80;

				if (r <= 127) goto bad;
			}
	good:;
			best_track = i;
			best_bird_dist = fd.best_bird_dist;
			best_track_dist = fd.best_track_dist;
	bad:;
		} while (bits != 0);
//		printf("Train %d %s\n", v->unitnumber, best_track_dist == -1 ? "NOTFOUND" : "FOUND");
		assert(best_track != -1);
	}

#if 0
	time = rdtsc() - time;
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

	while(true) {
		fd.best_bird_dist = (uint)-1;
		fd.best_track_dist = (uint)-1;

		NewTrainPathfind(v->tile, reverse ^ i, (TPFEnumProc*)TrainTrackFollower, &fd, NULL);

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
			r = (byte)Random();
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

	return reverse_best != 0;
}

static bool ProcessTrainOrder(Vehicle *v)
{
	Order order;
	bool result;

	// These are un-interruptible
	if (v->current_order.type >= OT_GOTO_DEPOT &&
			v->current_order.type <= OT_LEAVESTATION) {
		// Let a depot order in the schedule interrupt.
		if (v->current_order.type != OT_GOTO_DEPOT ||
				!(v->current_order.flags & OF_UNLOAD))
			return false;
	}

	if (v->current_order.type == OT_GOTO_DEPOT &&
			(v->current_order.flags & (OF_UNLOAD | OF_FULL_LOAD)) ==  (OF_UNLOAD | OF_FULL_LOAD) &&
			!VehicleNeedsService(v)) {
		v->cur_order_index++;
	}

	// check if we've reached the waypoint?
	if (v->current_order.type == OT_GOTO_WAYPOINT && v->tile == v->dest_tile) {
		v->cur_order_index++;
	}

	// check if we've reached a non-stop station while TTDPatch nonstop is enabled..
	if (_patches.new_nonstop && v->current_order.flags & OF_NON_STOP &&
			v->current_order.station == _map2[v->tile]) {
		v->cur_order_index++;
	}

	// Get the current order
	if (v->cur_order_index >= v->num_orders)
		v->cur_order_index = 0;
	order = v->schedule_ptr[v->cur_order_index];

	// If no order, do nothing.
	if (order.type == OT_NOTHING) {
		v->current_order.type = OT_NOTHING;
		v->current_order.flags = 0;
		v->dest_tile = 0;
		return false;
	}

	// If it is unchanged, keep it.
	if (order.type == v->current_order.type &&
			order.flags == v->current_order.flags &&
			order.station == v->current_order.station)
		return false;

	// Otherwise set it, and determine the destination tile.
	v->current_order = order;

	v->dest_tile = 0;

	result = false;
	if (order.type == OT_GOTO_STATION) {
		if (order.station == v->last_station_visited)
			v->last_station_visited = 0xFF;
		v->dest_tile = DEREF_STATION(order.station)->xy;
		result = CheckReverseTrain(v);
	} else if (order.type == OT_GOTO_DEPOT) {
		v->dest_tile = _depots[order.station].xy;
		result = CheckReverseTrain(v);
	} else if (order.type == OT_GOTO_WAYPOINT) {
		v->dest_tile = _waypoints[order.station].xy;
		result = CheckReverseTrain(v);
	}

	InvalidateVehicleOrderWidget(v);

	return result;
}

static void MarkTrainDirty(Vehicle *v)
{
	do {
		v->cur_image = GetTrainImage(v, v->direction);
		MarkAllViewportsDirty(v->left_coord, v->top_coord, v->right_coord + 1, v->bottom_coord + 1);
	} while ( (v=v->next) != NULL);
}

static void HandleTrainLoading(Vehicle *v, bool mode)
{
	if (v->current_order.type == OT_NOTHING)
		return;

	if (v->current_order.type != OT_DUMMY) {
		if (v->current_order.type != OT_LOADING)
			return;

		if (mode)
			return;

		// don't mark the train as lost if we're loading on the final station.
		if (v->current_order.flags & OF_NON_STOP)
			v->u.rail.days_since_order_progr = 0;

		if (--v->load_unload_time_rem)
			return;

		if (v->current_order.flags & OF_FULL_LOAD && CanFillVehicle(v)) {
			SET_EXPENSES_TYPE(EXPENSES_TRAIN_INC);
			if (LoadUnloadVehicle(v)) {
				InvalidateWindow(WC_TRAINS_LIST, v->owner);
				MarkTrainDirty(v);

				// need to update acceleration since the goods on the train changed.
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
			if (!(b.flags & OF_NON_STOP))
				return;
		}
	}

	v->u.rail.days_since_order_progr = 0;
	v->cur_order_index++;
	InvalidateVehicleOrderWidget(v);
}

static int UpdateTrainSpeed(Vehicle *v)
{
	uint spd;
	uint accel;

	if (v->vehstatus & VS_STOPPED || v->u.rail.flags & VRF_REVERSING) {
		accel = -v->acceleration * 2;
	} else {
		accel = v->acceleration;
		if (_patches.realistic_acceleration) {
			accel = GetRealisticAcceleration(v);
		}
	}

	spd = v->subspeed + accel * 2;
	v->subspeed = (byte)spd;
	v->cur_speed = spd = clamp(v->cur_speed + ((int)spd >> 8), 0, v->max_speed);

	if (!(v->direction & 1)) spd = spd * 3 >> 2;

	spd += v->progress;
	v->progress = (byte)spd;
	return (spd >> 8);
}

static void TrainEnterStation(Vehicle *v, int station)
{
	Station *st;
	uint32 flags;

	v->last_station_visited = station;

	/* check if a train ever visited this station before */
	st = DEREF_STATION(station);
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
			v->current_order.station == (byte)station) {
		// Yeah, keep the load/unload flags
		// Non Stop now means if the order should be increased.
		v->current_order.type = OT_LOADING;
		v->current_order.flags &= OF_FULL_LOAD | OF_UNLOAD;
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
		UpdateTrainAcceleration(v);
	}
	InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, 4);
}

static byte AfterSetTrainPos(Vehicle *v)
{
	byte new_z, old_z;

	// need this hint so it returns the right z coordinate on bridges.
	_get_z_hint = v->z_pos;
	new_z = GetSlopeZ(v->x_pos, v->y_pos);
	_get_z_hint = 0;

	old_z = v->z_pos;
	v->z_pos = new_z;

	v->u.rail.flags &= ~(VRF_GOINGUP | VRF_GOINGDOWN);

	if (new_z != old_z) {
		v->u.rail.flags |= (new_z > old_z) ? VRF_GOINGUP : VRF_GOINGDOWN;
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

static int GetNewVehicleDirectionByTile(uint new_tile, uint old_tile)
{
	uint offs = (GET_TILE_Y(new_tile) - GET_TILE_Y(old_tile) + 1) * 4 +
							GET_TILE_X(new_tile) - GET_TILE_X(old_tile) + 1;
	assert(offs < 11);
	return _new_vehicle_direction_table[offs];
}

static int GetNewVehicleDirection(Vehicle *v, int x, int y)
{
	uint offs = (y - v->y_pos + 1) * 4 + (x - v->x_pos + 1);
	assert(offs < 11);
	return _new_vehicle_direction_table[offs];
}

static int GetDirectionToVehicle(Vehicle *v, int x, int y)
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
static bool CheckCompatibleRail(Vehicle *v, uint tile)
{
	if (IS_TILETYPE(tile, MP_RAILWAY) || IS_TILETYPE(tile, MP_STATION)) {
		// normal tracks, jump to owner check
	} else if (IS_TILETYPE(tile, MP_TUNNELBRIDGE)) {
		if ((_map5[tile] & 0xC0) == 0xC0) {// is bridge middle part?
			TileInfo ti;
			FindLandscapeHeightByTile(&ti, tile);

			// correct Z position of a train going under a bridge on slopes
			if (CORRECT_Z(ti.tileh))
				ti.z += 8;

			if(v->z_pos != ti.z) // train is going over bridge
				return true;
		}
	} else if (IS_TILETYPE(tile, MP_STREET)) { // train is going over a road-crossing
		// tracks over roads, do owner check of tracks (_map_owner[tile])
		if (_map_owner[tile] != v->owner || (v->subtype == 0 && (_map3_hi[tile] & 0xF) != v->u.rail.railtype))
			return false;

		return true;
	} else
		return true;

	if (_map_owner[tile] != v->owner ||
			(v->subtype == 0 && (_map3_lo[tile] & 0xF) != v->u.rail.railtype))
		return false;

	return true;
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
	if (old_z == v->z_pos || _patches.realistic_acceleration)
		return;

	rsp = &_railtype_slowdown[v->u.rail.railtype];

	if (old_z < v->z_pos) {
		v->cur_speed -= (v->cur_speed * rsp->z_up >> 8);
	} else {
		uint16 spd = v->cur_speed + rsp->z_down;
		if (spd <= v->max_speed)
			v->cur_speed = spd;
	}
}

static const byte _otherside_signal_directions[14] = {
	1, 3, 1, 3, 5, 3, 0, 0,
	5, 7, 7, 5, 7, 1,
};

static void TrainMovedChangeSignals(uint tile, int dir)
{
	int i;
	if (IS_TILETYPE(tile, MP_RAILWAY) && (_map5[tile]&0xC0)==0x40) {
		i = FindFirstBit2x64((_map5[tile]+(_map5[tile]<<8)) & _reachable_tracks[dir]);
		UpdateSignalsOnSegment(tile, _otherside_signal_directions[i]);
	}
}


typedef struct TrainCollideChecker {
	Vehicle *v, *v_skip;

} TrainCollideChecker;

void *FindTrainCollideEnum(Vehicle *v, TrainCollideChecker *tcc)
{
	if (v == tcc->v || v == tcc->v_skip || v->type != VEH_Train || v->u.rail.track==0x80)
		return 0;

	if ( myabs(v->z_pos - tcc->v->z_pos) > 6 ||
			 myabs(v->x_pos - tcc->v->x_pos) >= 6 ||
			 myabs(v->y_pos - tcc->v->y_pos) >= 6)
				return NULL;
	return v;
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

	InvalidateWindowWidget(WC_VEHICLE_VIEW, u->index, 4);
}

static int CountPassengersInTrain(Vehicle *v)
{
	int num = 0;
	BEGIN_ENUM_WAGONS(v)
		if (v->cargo_type == 0) num += v->cargo_count;
	END_ENUM_WAGONS(v)
	return num;
}

/*
 * Checks whether the specified tried has a collision with another vehicle. If
 * so, destroys this vehicle, and the other vehicle if its subtype is 0 (?).
 * Reports the incident in a flashy news item, modifies station ratings and
 * plays a sound.
 */
static void CheckTrainCollision(Vehicle *v)
{
	TrainCollideChecker tcc;
	Vehicle *coll,*realcoll;
	int num;

	/* can't collide in depot */
	if (v->u.rail.track == 0x80)
		return;

	if ( !(v->u.rail.track == 0x40) )
		assert((uint)TILE_FROM_XY(v->x_pos, v->y_pos) == v->tile);

	tcc.v = v;
	tcc.v_skip = v->next;

	/* find colliding vehicle */
	realcoll = coll = VehicleFromPos(TILE_FROM_XY(v->x_pos, v->y_pos), &tcc, (VehicleFromPosProc*)FindTrainCollideEnum);
	if (coll == NULL)
		return;


	coll = GetFirstVehicleInChain(coll);

	/* it can't collide with its own wagons */
	if ( (v == coll) || ( (v->u.rail.track & 0x40) && ( (v->direction & 2) != (realcoll->direction & 2) ) ) )
		return;

	//two drivers + passangers killed in train v
	num = 2 + CountPassengersInTrain(v);
	if(!(coll->vehstatus&VS_CRASHED))
		//two drivers + passangers killed in train coll (if it was not crashed already)
		num += 2 + CountPassengersInTrain(coll);

	SetVehicleCrashed(v);
	if (coll->subtype == 0)
		SetVehicleCrashed(coll);


	SetDParam(0, num);

	AddNewsItem(STR_8868_TRAIN_CRASH_DIE_IN_FIREBALL,
		NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_VEHICLE, NT_ACCIDENT, 0),
		v->index,
		0);

	ModifyStationRatingAround(v->tile, v->owner, -160, 30);
	SndPlayVehicleFx(SND_13_BIG_CRASH, v);
}

static void *CheckVehicleAtSignal(Vehicle *v, void *data)
{
	uint32 d = (uint32)data;

	if (v->type == VEH_Train && v->subtype == 0 && v->tile == (TileIndex)(d >> 8)) {
		byte diff = (v->direction - (byte)d + 2) & 7;
		if (diff == 2 || (v->cur_speed <= 5 && diff <= 4))
			return (void*)1;
	}
	return 0;
}

static void TrainController(Vehicle *v)
{
	Vehicle *prev = NULL;
	GetNewVehiclePosResult gp;
	uint32 r, tracks,ts;
	int dir, i;
	byte chosen_dir;
	byte chosen_track;
	byte old_z;

	/* For every vehicle after and including the given vehicle */
	for(;;) {
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

					if (!TrainCheckIfLineEnds(v))
						return;

					r = VehicleEnterTile(v, gp.new_tile, gp.x, gp.y);
					if (r & 0x8)
						goto invalid_rail;
					if (r & 0x2) {
						TrainEnterStation(v, r >> 8);
						return;
					}

					if (v->current_order.type == OT_LEAVESTATION) {
						v->current_order.type = OT_NOTHING;
						v->current_order.flags = 0;
						InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, 4);
					}
				}
			} else {
				/* A new tile is about to be entered. */

				/* Determine what direction we're entering the new tile from */
				dir = GetNewVehicleDirectionByTile(gp.new_tile, gp.old_tile);
				assert(dir==1 || dir==3 || dir==5 || dir==7);

				/* Get the status of the tracks in the new tile and mask
				 * away the bits that aren't reachable. */
				ts = GetTileTrackStatus(gp.new_tile, TRANSPORT_RAIL) & _reachable_tracks[dir >> 1];

				/* Combine the from & to directions.
				 * Now, the lower byte contains the track status, and the byte at bit 16 contains
				 * the signal status. */
				tracks = ts|(ts >> 8);
				if ( (byte) tracks == 0)
					goto invalid_rail;

				/* Check if the new tile contrains tracks that are compatible
				 * with the current train, if not, bail out. */
				if (!CheckCompatibleRail(v, gp.new_tile))
					goto invalid_rail;

				if (prev == NULL) {
					/* Currently the locomotive is active. Determine which one of the
					 * available tracks to choose */
					chosen_track = 1 << ChooseTrainTrack(v, gp.new_tile, dir>>1, (byte)tracks);

					/* Check if it's a red signal and that force proceed is not clicked. */
					if ( (tracks>>16)&chosen_track && v->u.rail.force_proceed == 0) goto red_light;
				} else {
					static byte _matching_tracks[8] = {0x30, 1, 0xC, 2, 0x30, 1, 0xC, 2};

					/* The wagon is active, simply follow the prev vehicle. */
					chosen_track = (byte)(_matching_tracks[GetDirectionToVehicle(prev, gp.x, gp.y)] & tracks);
				}

				/* make sure chosen track is a valid track */
				assert(chosen_track==1 || chosen_track==2 || chosen_track==4 || chosen_track==8 || chosen_track==16 || chosen_track==32);

				/* Update XY to reflect the entrance to the new tile, and select the direction to use */
				{
					const byte *b = _initial_tile_subcoord[FIND_FIRST_BIT(chosen_track)][dir>>1];
					gp.x = (gp.x & ~0xF) | b[0];
					gp.y = (gp.y & ~0xF) | b[1];
					chosen_dir = b[2];
				}

				/* Call the landscape function and tell it that the vehicle entered the tile */
				r = VehicleEnterTile(v, gp.new_tile, gp.x, gp.y);
				if (r&0x8)
					goto invalid_rail;

				if (v->subtype == 0) v->load_unload_time_rem = 0;

				if (!(r&0x4)) {
					v->tile = gp.new_tile;
					v->u.rail.track = chosen_track;
				}

				if (v->subtype == 0)
					TrainMovedChangeSignals(gp.new_tile, dir>>1);

				/* Signals can only change when the first
				 * (above) or the last vehicle moves. */
				if (v->next == NULL)
					TrainMovedChangeSignals(gp.old_tile, (dir>>1) ^ 2);

				if (prev == NULL) {
					AffectSpeedByDirChange(v, chosen_dir);
				}

				v->direction = chosen_dir;
			}
		} else {
			/* in tunnel */
			GetNewVehiclePos(v, &gp);

			if (IS_TILETYPE(gp.new_tile, MP_TUNNELBRIDGE) &&
					!(_map5[gp.new_tile] & 0xF0)) {
				r = VehicleEnterTile(v, gp.new_tile, gp.x, gp.y);
				if (r & 0x4) goto common;
			}

			v->x_pos = gp.x;
			v->y_pos = gp.y;
			VehiclePositionChanged(v);
			if (prev == NULL)
				CheckTrainCollision(v);
			goto next_vehicle;
		}
common:;

		/* update image of train, as well as delta XY */
		dir = GetNewVehicleDirection(v, gp.x, gp.y);
		UpdateTrainDeltaXY(v, dir);
		v->cur_image = GetTrainImage(v, dir);

		v->x_pos = gp.x;
		v->y_pos = gp.y;

		/* update the Z position of the vehicle */
		old_z = AfterSetTrainPos(v);

		if (prev == NULL) {
			/* This is the first vehicle in the train */
			AffectSpeedByZChange(v, old_z);
			CheckTrainCollision(v);
		}

next_vehicle:;
		/* continue with next vehicle */
		prev = v;
		if ((v=v->next) == NULL)
			return;
	}

invalid_rail:
	/* We've reached end of line?? */
	if (prev != NULL) {
		error("!Disconnecting train");
	}
	goto reverse_train_direction;

red_light: {
	/* We're in front of a red signal ?? */
		/* find the first set bit in ts. need to do it in 2 steps, since
		 * FIND_FIRST_BIT only handles 6 bits at a time. */
		i = FindFirstBit2x64(ts);

		if (!(_map3_lo[gp.new_tile] & _signal_otherdir[i])) {
			v->cur_speed = 0;
			v->subspeed = 0;
			v->progress = 255-100;
			if (++v->load_unload_time_rem < _patches.wait_oneway_signal * 20)
				return;
		} else if (_map3_lo[gp.new_tile] & _signal_onedir[i]){
			v->cur_speed = 0;
			v->subspeed = 0;
			v->progress = 255-10;
			if (++v->load_unload_time_rem < _patches.wait_twoway_signal * 73) {
				uint o_tile = gp.new_tile + _tileoffs_by_dir[dir>>1];
				/* check if a train is waiting on the other side */
				if (VehicleFromPos(o_tile, (void*)( (o_tile<<8) | (dir^4)), (VehicleFromPosProc*)CheckVehicleAtSignal) == NULL)
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

extern uint CheckTunnelBusy(uint tile, int *length);

static void DeleteLastWagon(Vehicle *v)
{
	Vehicle *u = v;
	int t;

	while (v->next != NULL) {
		u = v;
		v = v->next;
	}
	u->next = NULL;

	InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
	DeleteWindowById(WC_VEHICLE_VIEW, v->index);
	RebuildVehicleLists();
	InvalidateWindow(WC_COMPANY, v->owner);

	BeginVehicleMove(v);
	EndVehicleMove(v);
	DeleteVehicle(v);

	if (!((t=v->u.rail.track) & 0xC0)) {
		SetSignalsOnBothDir(v->tile, FIND_FIRST_BIT(t));
	}

	if (v->u.rail.track == 0x40) {
		int length;
		TileIndex endtile = CheckTunnelBusy(v->tile, &length);
		if ((v->direction == 1) || (v->direction == 5) )
			SetSignalsOnBothDir(v->tile,0);
			SetSignalsOnBothDir(endtile,0);
		if ((v->direction == 3) || (v->direction == 7) )
			SetSignalsOnBothDir(v->tile,1);
			SetSignalsOnBothDir(endtile,1);
	}
}

static void ChangeTrainDirRandomly(Vehicle *v)
{
	static int8 _random_dir_change[4] = { -1, 0, 0, 1};

	do {
		//I need to buffer the train direction
		if (!(v->u.rail.track & 0x40))
			v->direction = (v->direction + _random_dir_change[InteractiveRandom()&3]) & 7;
		if (!(v->vehstatus & VS_HIDDEN)) {
			BeginVehicleMove(v);
			UpdateTrainDeltaXY(v, v->direction);
			v->cur_image = GetTrainImage(v, v->direction);
			AfterSetTrainPos(v);
		}
	} while ( (v=v->next) != NULL);
}

static void HandleCrashedTrain(Vehicle *v)
{
	int state = ++v->u.rail.crash_anim_pos, index;
	uint32 r;
	Vehicle *u;

	if ( (state == 4) && (v->u.rail.track != 0x40) ) {
		CreateEffectVehicleRel(v, 4, 4, 8, EV_CRASHED_SMOKE);
	}

	if (state <= 200 && (uint16)(r=InteractiveRandom()) <= 0x2492) {
		index = (r * 10 >> 16);

		u = v;
		do {
			if (--index < 0) {
				r = InteractiveRandom();

				CreateEffectVehicleRel(u,
					2 + ((r>>8)&7),
					2 + ((r>>16)&7),
					5 + (r&7),
					EV_DEMOLISH);
				break;
			}
		} while ( (u=u->next) != NULL);
	}

	if (state <= 240 && !(v->tick_counter&3)) {
		ChangeTrainDirRandomly(v);
	}

	if (state >= 4440 && !(v->tick_counter&0x1F))
		DeleteLastWagon(v);
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
			if (u)
				u->u.special.unk0 = v->breakdown_delay * 2;
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
	uint tile;
	uint x,y;
	int t;
	uint32 ts;

	if ((uint)(t=v->breakdown_ctr) > 1) {
		v->vehstatus |= VS_TRAIN_SLOWING;

		t = _breakdown_speeds[ ((~t) >> 4) & 0xF];
		if ((uint16)t <= v->cur_speed)
			v->cur_speed = t;
	} else {
		v->vehstatus &= ~VS_TRAIN_SLOWING;
	}

	// exit if inside a tunnel
	if (v->u.rail.track & 0x40)
		return true;

	tile = v->tile;

	// tunnel entrance?
	if (IS_TILETYPE(tile, MP_TUNNELBRIDGE) &&
			(_map5[tile] & 0xF0) == 0 && (byte)((_map5[tile] & 3)*2+1) == v->direction)
				return true;

	// depot?
	/* XXX -- When enabled, this makes it possible to crash trains of others
	     (by building a depot right against a station) */
/*	if (IS_TILETYPE(tile, MP_RAILWAY) && (_map5[tile] & 0xFC) == 0xC0)
		return true;*/

	/* Determine the non-diagonal direction in which we will exit this tile */
	t = v->direction >> 1;
	if (!(v->direction & 1) && v->u.rail.track != _state_dir_table[t]) {
		t = (t - 1) & 3;
	}
	/* Calculate next tile */
	tile += _tileoffs_by_dir[t];
	// determine the track status on the next tile.
 	ts = GetTileTrackStatus(tile, TRANSPORT_RAIL) & _reachable_tracks[t];

	/* Calc position within the current tile ?? */
	x = v->x_pos & 0xF;
	y = v->y_pos & 0xF;

	switch(v->direction) {
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

	if ( (uint16)ts != 0) {
		/* If we approach a rail-piece which we can't enter, don't enter it! */
		if (x + 4 > 15 && !CheckCompatibleRail(v, tile)) {
			v->cur_speed = 0;
			ReverseTrainDirection(v);
			return false;
		}
		if ((ts &= (ts >> 16)) == 0) {
			// make a rail/road crossing red
			if (IS_TILETYPE(tile, MP_STREET) && (_map5[tile] & 0xF0)==0x10) {
				if (!(_map5[tile] & 4)) {
					_map5[tile] |= 4;
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

	// slow down
	v->vehstatus |= VS_TRAIN_SLOWING;
	t = _breakdown_speeds[x & 0xF];
	if (!(v->direction&1)) t>>=1;
	if ((uint16)t < v->cur_speed)
		v->cur_speed = t;

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

	if (v->u.rail.flags & VRF_REVERSING && v->cur_speed == 0) {
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

	if (v->subtype == 0) {
		TrainLocoHandler(v, false);

		// make sure vehicle wasn't deleted.
		if (v->type == VEH_Train && v->subtype == 0)
			TrainLocoHandler(v, true);
	}
}


static const byte _depot_track_ind[4] = {0,1,0,1};

// Validation for the news item "Train is waiting in depot"
bool ValidateTrainInDepot( uint data_a, uint data_b )
{
	Vehicle *v = &_vehicles[data_a];
	if (v->u.rail.track == 0x80 && (v->vehstatus | VS_STOPPED))
		return true;
	else
		return false;
}

void TrainEnterDepot(Vehicle *v, uint tile)
{
	SetSignalsOnBothDir(tile, _depot_track_ind[_map5[tile]&3]);

	if (v->subtype != 0)
		v = GetFirstVehicleInChain(v);

	VehicleServiceInDepot(v);

	InvalidateWindow(WC_VEHICLE_DETAILS, v->index);

	v->load_unload_time_rem = 0;
	v->cur_speed = 0;

	MaybeRenewVehicle(v, EstimateTrainCost(RailVehInfo(v->engine_type)));

	TriggerVehicle(v, VEHICLE_TRIGGER_DEPOT);

	if (v->current_order.type == OT_GOTO_DEPOT) {
		Order t;

		InvalidateWindow(WC_VEHICLE_VIEW, v->index);

		t = v->current_order;
		v->current_order.type = OT_DUMMY;
		v->current_order.flags = 0;

		if (t.flags & OF_UNLOAD) { // Part of the schedule?
			v->u.rail.days_since_order_progr = 0;
			v->cur_order_index++;
		} else if (t.flags & OF_FULL_LOAD) { // User initiated?
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
}

static void CheckIfTrainNeedsService(Vehicle *v)
{
	byte depot;
	TrainFindDepotData tfdd;

	if (_patches.servint_trains == 0)
		return;

	if (!VehicleNeedsService(v))
		return;

	if (v->vehstatus & VS_STOPPED)
		return;

	if (_patches.gotodepot && ScheduleHasDepotOrders(v->schedule_ptr))
		return;

	// Don't interfere with a depot visit scheduled by the user, or a
	// depot visit by the order list.
	if (v->current_order.type == OT_GOTO_DEPOT &&
			(v->current_order.flags & (OF_FULL_LOAD | OF_UNLOAD)) != 0)
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
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, 4);
		}
		return;
	}

	depot = GetDepotByTile(tfdd.tile);

	if (v->current_order.type == OT_GOTO_DEPOT &&
			v->current_order.station != depot &&
			!CHANCE16(3,16))
		return;

	v->current_order.type = OT_GOTO_DEPOT;
	v->current_order.flags = OF_NON_STOP;
	v->current_order.station = depot;
	v->dest_tile = tfdd.tile;
	InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, 4);
}

int32 GetTrainRunningCost(Vehicle *v)
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

	if (v->subtype == 0) {
		CheckVehicleBreakdown(v);
		AgeVehicle(v);

		CheckIfTrainNeedsService(v);

		// check if train hasn't advanced in its order list for a set number of days
		if (_patches.lost_train_days && v->num_orders && !(v->vehstatus & VS_STOPPED) && ++v->u.rail.days_since_order_progr >= _patches.lost_train_days && v->owner == _local_player) {
			v->u.rail.days_since_order_progr = 0;
			SetDParam(0, v->unitnumber);
			AddNewsItem(
				STR_TRAIN_IS_LOST,
				NEWS_FLAGS(NM_SMALL, NF_VIEWPORT|NF_VEHICLE, NT_ADVICE, 0),
				v->index,
				0);
		}

		CheckOrders(v);

		/* update destination */
		if (v->current_order.type == OT_GOTO_STATION &&
				(tile = DEREF_STATION(v->current_order.station)->train_tile) != 0)
					v->dest_tile = tile;

		if ((v->vehstatus & VS_STOPPED) == 0) {
			/* running costs */
			int32 cost = GetTrainRunningCost(v) / 364;

			v->profit_this_year -= cost >> 8;

			SET_EXPENSES_TYPE(EXPENSES_TRAIN_RUN);
			SubtractMoneyFromPlayerFract(v->owner, cost);

			InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
			InvalidateWindow(WC_TRAINS_LIST, v->owner);
		}
	}
}

void TrainsYearlyLoop()
{
	Vehicle *v;

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Train && v->subtype == 0) {

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

extern void ShowTrainViewWindow(Vehicle *v);

void HandleClickOnTrain(Vehicle *v)
{
	if (v->subtype != 0) v = GetFirstVehicleInChain(v);
	ShowTrainViewWindow(v);
}

void InitializeTrains()
{
	_age_cargo_skip_counter = 1;
}

int ScheduleHasDepotOrders(const Order *schedule)
{
	for (; schedule->type != OT_NOTHING; schedule++)
		if (schedule->type == OT_GOTO_DEPOT)
			return true;
	return false;
}
