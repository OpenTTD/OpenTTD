#include "stdafx.h"
#include "ttd.h"
#include "table/strings.h"
#include "vehicle.h"
#include "engine.h"
#include "command.h"
#include "station.h"
#include "news.h"
#include "gfx.h"
#include "pathfind.h"
#include "player.h"
#include "sound.h"

void ShowRoadVehViewWindow(Vehicle *v);

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


static const uint16 _road_veh_fp_ax_or[4] = {
	0x100,0x200,1,2,
};

static const uint16 _road_veh_fp_ax_and[4] = {
	0x1009, 0x16, 0x520, 0x2A00
};

static const byte _road_reverse_table[4] = {
	6, 7, 14, 15
};

static const uint16 _road_pf_table_3[4] = {
	0x910, 0x1600, 0x2005, 0x2A
};

int GetRoadVehImage(Vehicle *v, byte direction)
{
	int img = v->spritenum;
	int image;

	if (is_custom_sprite(img)) {
		image = GetCustomVehicleSprite(v, direction);
		if (image) return image;
		img = _engine_original_sprites[v->engine_type];
	}

	image = direction + _roadveh_images[img];
	if (v->cargo_count >= (v->cargo_cap >> 1))
		image += _roadveh_full_adder[img];
	return image;
}

void DrawRoadVehEngine(int x, int y, int engine, uint32 image_ormod)
{
	int spritenum = RoadVehInfo(engine)->image_index;

	if (is_custom_sprite(spritenum)) {
		int sprite = GetCustomVehicleIcon(engine, 6);

		if (sprite) {
			DrawSprite(sprite | image_ormod, x, y);
			return;
		}
		spritenum = _engine_original_sprites[engine];
	}
	DrawSprite((6 + _roadveh_images[spritenum]) | image_ormod, x, y);
}

void DrawRoadVehEngineInfo(int engine, int x, int y, int maxw)
{
	const RoadVehicleInfo *rvi = RoadVehInfo(engine);

	SetDParam(0, ((_price.roadveh_base >> 3) * rvi->base_cost) >> 5);
	SetDParam(1, rvi->max_speed * 10 >> 5);
	SetDParam(2, rvi->running_cost * _price.roadveh_running >> 8);

	SetDParam(4, rvi->capacity);
	SetDParam(3, _cargoc.names_long_p[rvi->cargo_type]);

	DrawStringMultiCenter(x, y, STR_902A_COST_SPEED_RUNNING_COST, maxw);
}

static int32 EstimateRoadVehCost(byte engine_type)
{
	return ((_price.roadveh_base >> 3) * RoadVehInfo(engine_type)->base_cost) >> 5;
}

int32 CmdBuildRoadVeh(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	int32 cost;
	Vehicle *v;
	byte unit_num;
	uint tile = TILE_FROM_XY(x,y);
	Engine *e;

	SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);

	cost = EstimateRoadVehCost(p1);
	if (flags & DC_QUERY_COST)
		return cost;

	v = AllocateVehicle();
	if (v == NULL || _ptr_to_next_order >= endof(_order_array))
		return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);

	/* find the first free roadveh id */
	unit_num = GetFreeUnitNumber(VEH_Road);
	if (unit_num > _patches.max_roadveh)
		return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);

	if (flags & DC_EXEC) {
		const RoadVehicleInfo *rvi = RoadVehInfo(p1);

		v->unitnumber = unit_num;
		v->direction = 0;
		v->owner = _current_player;

		v->tile = tile;
		x = GET_TILE_X(tile)*16 + 8;
		y = GET_TILE_Y(tile)*16 + 8;
		v->x_pos = x;
		v->y_pos = y;
		v->z_pos = GetSlopeZ(x,y);
		v->z_height = 6;

		v->u.road.state = 254;
		v->vehstatus = VS_HIDDEN|VS_STOPPED|VS_DEFPAL;

		v->spritenum = rvi->image_index;
		v->cargo_type = rvi->cargo_type;
		v->cargo_cap = rvi->capacity;
//		v->cargo_count = 0;
		v->value = cost;
//		v->day_counter = 0;
//		v->next_order_param = v->next_order = 0;
//		v->load_unload_time_rem = 0;
//		v->progress = 0;

//	v->u.road.unk2 = 0;
//	v->u.road.overtaking = 0;

		v->last_station_visited = 0xFF;
		v->max_speed = rvi->max_speed;
		v->engine_type = (byte)p1;

		e = &_engines[p1];
		v->reliability = e->reliability;
		v->reliability_spd_dec = e->reliability_spd_dec;
		v->max_age = e->lifelength * 366;
		_new_roadveh_id = v->index;

		v->string_id = STR_SV_ROADVEH_NAME;
		_ptr_to_next_order->type = OT_NOTHING;
		_ptr_to_next_order->flags = 0;
		v->schedule_ptr = _ptr_to_next_order++;

		v->service_interval = _patches.servint_roadveh;

		v->date_of_last_service = _date;
		v->build_year = _cur_year;

		v->type = VEH_Road;
		v->cur_image = 0xC15;

		VehiclePositionChanged(v);

		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
		RebuildVehicleLists();
		InvalidateWindow(WC_COMPANY, v->owner);
	}

	return cost;
}

// p1 = vehicle
int32 CmdStartStopRoadVeh(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;

	v = &_vehicles[p1];

	if (v->type != VEH_Road || !CheckOwnership(v->owner))
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		v->vehstatus ^= VS_STOPPED;
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, 4);
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
	}

	return 0;
}

int32 CmdSellRoadVeh(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;

	SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);

	v = &_vehicles[p1];

	if (v->type != VEH_Road || !CheckOwnership(v->owner))
		return CMD_ERROR;

	if (!IsRoadDepotTile(v->tile) || v->u.road.state != 254 || !(v->vehstatus&VS_STOPPED))
		return_cmd_error(STR_9013_MUST_BE_STOPPED_INSIDE);

	if (flags & DC_EXEC) {
		// Invalidate depot
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
		RebuildVehicleLists();
		InvalidateWindow(WC_COMPANY, v->owner);
		DeleteWindowById(WC_VEHICLE_VIEW, v->index);
		DeleteVehicle(v);
	}

	return -(int32)v->value;
}

typedef struct RoadFindDepotData {
	uint best_length;
	uint tile;
	byte owner;
} RoadFindDepotData;


static const TileIndexDiff _road_find_sig_dir_mod[4] = {
	TILE_XY(-1,0),
	TILE_XY(0,1),
	TILE_XY(1,0),
	TILE_XY(0,-1),
};

static const byte _road_pf_directions[16] = {
	0, 1, 0, 1, 2, 1, 255, 255,
	2, 3, 3, 2, 3, 0, 255, 255,
};

static bool EnumRoadSignalFindDepot(uint tile, RoadFindDepotData *rfdd, int track, uint length, byte *state)
{
	tile += _road_find_sig_dir_mod[_road_pf_directions[track]];

	if (IS_TILETYPE(tile, MP_STREET) &&
			(_map5[tile] & 0xF0) == 0x20 &&
			_map_owner[tile] == rfdd->owner) {

		if (length < rfdd->best_length) {
			rfdd->best_length = length;
			rfdd->tile = tile;
		}
	}
	return false;
}

static int FindClosestRoadDepot(Vehicle *v)
{
	uint tile = v->tile;
	int i;
	RoadFindDepotData rfdd;

	if (v->u.road.state == 255) { tile = GetVehicleOutOfTunnelTile(v); }

	rfdd.owner = v->owner;
	rfdd.best_length = (uint)-1;

	/* search in all directions */
	for(i=0; i!=4; i++)
		FollowTrack(tile, 0x2000 | TRANSPORT_ROAD, i, (TPFEnumProc*)EnumRoadSignalFindDepot, NULL, &rfdd);

	if (rfdd.best_length == (uint)-1)
		return -1;

	return GetDepotByTile(rfdd.tile);
}

int32 CmdSendRoadVehToDepot(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v = &_vehicles[p1];
	int depot;

	if (v->type != VEH_Road || !CheckOwnership(v->owner))
		return CMD_ERROR;

	if (v->current_order.type == OT_GOTO_DEPOT) {
		if (flags & DC_EXEC) {
			if (v->current_order.flags & OF_UNLOAD)
				v->cur_order_index++;
			v->current_order.type = OT_DUMMY;
			v->current_order.flags = 0;
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, 4);
		}
		return 0;
	}

	depot = FindClosestRoadDepot(v);
	if (depot < 0)
		return_cmd_error(STR_9019_UNABLE_TO_FIND_LOCAL_DEPOT);

	if (flags & DC_EXEC) {
		v->current_order.type = OT_GOTO_DEPOT;
		v->current_order.flags = OF_NON_STOP | OF_FULL_LOAD;
		v->current_order.station = (byte)depot;
		v->dest_tile = _depots[depot].xy;
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, 4);
	}

	return 0;
}

int32 CmdTurnRoadVeh(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;

	v = &_vehicles[p1];

	if (v->type != VEH_Road || !CheckOwnership(v->owner))
		return CMD_ERROR;

	if (v->vehstatus & (VS_HIDDEN|VS_STOPPED) ||
			v->u.road.crashed_ctr != 0 ||
			v->breakdown_ctr != 0 ||
			v->u.road.overtaking != 0 ||
			v->cur_speed < 5) {
		_error_message = STR_EMPTY;
		return CMD_ERROR;
	}

	if (flags & DC_EXEC) {
		v->u.road.reverse_ctr = 180;
	}

	return 0;
}

int32 CmdChangeRoadVehServiceInt(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;

	v = &_vehicles[p1];

	if (v->type != VEH_Road || !CheckOwnership(v->owner))
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		v->service_interval = (uint16)p2;
		InvalidateWindowWidget(WC_VEHICLE_DETAILS, v->index, 7);
	}

	return 0;
}


static void MarkRoadVehDirty(Vehicle *v)
{
	v->cur_image = GetRoadVehImage(v, v->direction);
	MarkAllViewportsDirty(v->left_coord, v->top_coord, v->right_coord + 1, v->bottom_coord + 1);
}

static void UpdateRoadVehDeltaXY(Vehicle *v)
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
	uint32 x = _delta_xy_table[v->direction];
	v->x_offs = (byte)x;
	v->y_offs = (byte)(x>>=8);
	v->sprite_width = (byte)(x>>=8);
	v->sprite_height = (byte)(x>>=8);
}

static void ClearCrashedStation(Vehicle *v)
{
	uint tile = v->tile;
	Station *st = DEREF_STATION(_map2[tile]);
	byte *b, bb;

	b = (_map5[tile] >= 0x47) ? &st->bus_stop_status : &st->truck_stop_status;

	bb = *b;

	// mark station as not busy
	bb &= ~0x80;

	// free parking bay
	bb |= (v->u.road.state&0x02)?2:1;

	*b = bb;
}

static void RoadVehDelete(Vehicle *v)
{
	DeleteWindowById(WC_VEHICLE_VIEW, v->index);
	InvalidateWindow(WC_VEHICLE_DETAILS, v->index);

	RebuildVehicleLists();
	InvalidateWindow(WC_COMPANY, v->owner);

	if(IS_TILETYPE(v->tile, MP_STATION))
		ClearCrashedStation(v);

	BeginVehicleMove(v);
	EndVehicleMove(v);

	DeleteVehicle(v);
}

static byte SetRoadVehPosition(Vehicle *v, int x, int y)
{
	byte new_z, old_z;

	// need this hint so it returns the right z coordinate on bridges.
	_get_z_hint = v->z_pos;
	new_z = GetSlopeZ(v->x_pos=x, v->y_pos=y);
	_get_z_hint = 0;

	old_z = v->z_pos;
	v->z_pos = new_z;

	VehiclePositionChanged(v);
	EndVehicleMove(v);
	return old_z;
}

static void RoadVehSetRandomDirection(Vehicle *v)
{
	static const int8 _turn_prob[4] = { -1, 0, 0, 1 };
	uint32 r = Random();
	v->direction = (v->direction+_turn_prob[r&3])&7;
	BeginVehicleMove(v);
	UpdateRoadVehDeltaXY(v);
	v->cur_image = GetRoadVehImage(v, v->direction);
	SetRoadVehPosition(v, v->x_pos, v->y_pos);
}

static void RoadVehIsCrashed(Vehicle *v)
{
	v->u.road.crashed_ctr++;
	if (v->u.road.crashed_ctr == 2) {
		CreateEffectVehicleRel(v,4,4,8,EV_CRASHED_SMOKE);
	} else if (v->u.road.crashed_ctr <= 45) {
		if ((v->tick_counter&7)==0)
			RoadVehSetRandomDirection(v);
	} else if (v->u.road.crashed_ctr >= 2220) {
		RoadVehDelete(v);
	}
}

static void *EnumCheckRoadVehCrashTrain(Vehicle *v, Vehicle *u)
{
	if (v->type != VEH_Train ||
			myabs(v->z_pos - u->z_pos) > 6 ||
			myabs(v->x_pos - u->x_pos) > 4 ||
			myabs(v->y_pos - u->y_pos) > 4)
				return NULL;
	return v;
}

static void RoadVehCrash(Vehicle *v)
{
	uint16 pass;

	v->u.road.crashed_ctr++;
	v->vehstatus |= VS_CRASHED;

	InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, 4);

	pass = 1;
	if (v->cargo_type == 0)
		pass += v->cargo_count;
	v->cargo_count = 0;
	SetDParam(0, pass);

	AddNewsItem(STR_9031_ROAD_VEHICLE_CRASH_DRIVER+(pass!=1),
		NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_VEHICLE, NT_ACCIDENT, 0),
		v->index,
		0);

	ModifyStationRatingAround(v->tile, v->owner, -160, 22);
	SndPlayVehicleFx(SND_12_EXPLOSION, v);
}

static void RoadVehCheckTrainCrash(Vehicle *v)
{
	uint tile;

	if (v->u.road.state == 255)
		return;

	tile = v->tile;

	// Make sure it's a road/rail crossing
	if (!IS_TILETYPE(tile, MP_STREET) ||
	    (_map5[tile] & 0xF0) != 0x10)
				return;

	if (VehicleFromPos(tile, v, (VehicleFromPosProc*)EnumCheckRoadVehCrashTrain) != NULL)
		RoadVehCrash(v);
}

static void HandleBrokenRoadVeh(Vehicle *v)
{
	if (v->breakdown_ctr != 1) {
		v->breakdown_ctr = 1;
		v->cur_speed = 0;

		if (v->breakdowns_since_last_service != 255)
			v->breakdowns_since_last_service++;

		InvalidateWindow(WC_VEHICLE_VIEW, v->index);
		InvalidateWindow(WC_VEHICLE_DETAILS, v->index);

		SndPlayVehicleFx((_opt.landscape != LT_CANDY) ?
			SND_0F_VEHICLE_BREAKDOWN : SND_35_COMEDY_BREAKDOWN, v);

		if (!(v->vehstatus & VS_HIDDEN)) {
			Vehicle *u = CreateEffectVehicleRel(v, 4, 4, 5, EV_BREAKDOWN_SMOKE);
			if (u)
				u->u.special.unk0 = v->breakdown_delay * 2;
		}
	}

	if (!(v->tick_counter & 1)) {
		if (!--v->breakdown_delay) {
			v->breakdown_ctr = 0;
			InvalidateWindow(WC_VEHICLE_VIEW, v->index);
		}
	}
}

static void ProcessRoadVehOrder(Vehicle *v)
{
	Order order;
	Station *st;

	if (v->current_order.type >= OT_GOTO_DEPOT && v->current_order.type <= OT_LEAVESTATION) {
		// Let a depot order in the schedule interrupt.
		if (v->current_order.type != OT_GOTO_DEPOT ||
				!(v->current_order.flags & OF_UNLOAD))
			return;
	}

	if (v->current_order.type == OT_GOTO_DEPOT &&
			(v->current_order.flags & (OF_UNLOAD | OF_FULL_LOAD)) == (OF_UNLOAD | OF_FULL_LOAD) &&
			!VehicleNeedsService(v)) {
		v->cur_order_index++;
	}

	if (v->cur_order_index >= v->num_orders)
		v->cur_order_index = 0;

	order = v->schedule_ptr[v->cur_order_index];

	if (order.type == OT_NOTHING) {
		v->current_order.type = OT_NOTHING;
		v->current_order.flags = 0;
		v->dest_tile = 0;
		return;
	}

	if (order.type == v->current_order.type &&
			order.flags == v->current_order.flags &&
			order.station == v->current_order.station)
		return;

	v->current_order = order;

	v->dest_tile = 0;

	if (order.type == OT_GOTO_STATION) {
		if (order.station == v->last_station_visited)
			v->last_station_visited = 0xFF;
		st = DEREF_STATION(order.station);
		v->dest_tile = v->cargo_type==CT_PASSENGERS ? st->bus_tile : st->lorry_tile;
	} else if (order.type == OT_GOTO_DEPOT) {
		v->dest_tile = _depots[order.station].xy;
	}

	InvalidateVehicleOrderWidget(v);
}

static void HandleRoadVehLoading(Vehicle *v)
{
	if (v->current_order.type == OT_NOTHING)
		return;

	if (v->current_order.type != OT_DUMMY) {
		if (v->current_order.type != OT_LOADING)
			return;

		if (--v->load_unload_time_rem)
			return;

		if (v->current_order.flags & OF_FULL_LOAD && CanFillVehicle(v)) {
			SET_EXPENSES_TYPE(EXPENSES_ROADVEH_INC);
			if (LoadUnloadVehicle(v)) {
				InvalidateWindow(WC_ROADVEH_LIST, v->owner);
				MarkRoadVehDirty(v);
			}
			return;
		}

		{
			Order b = v->current_order;
			v->current_order.type = OT_LEAVESTATION;
			v->current_order.flags = 0;
			if (!(b.flags & OF_NON_STOP))
				return;
		}
	}

	v->cur_order_index++;
	InvalidateVehicleOrderWidget(v);
}

static void StartRoadVehSound(Vehicle *v)
{
	SoundFx s = RoadVehInfo(v->engine_type)->sfx;
	if (s == SND_19_BUS_START_PULL_AWAY && (v->tick_counter & 3) == 0)
		s = SND_1A_BUS_START_PULL_AWAY_WITH_HORN;
	SndPlayVehicleFx(s, v);
}

typedef struct RoadVehFindData {
	int x,y;
	Vehicle *veh;
	byte dir;
} RoadVehFindData;

void *EnumCheckRoadVehClose(Vehicle *v, RoadVehFindData *rvf)
{
	static const short _dists[] = {
		-4, -8, -4, -1, 4, 8, 4, 1,
		-4, -1, 4, 8, 4, 1, -4, -8,
	};

	short x_diff = v->x_pos - rvf->x;
	short y_diff = v->y_pos - rvf->y;

	if (rvf->veh == v ||
			v->type != VEH_Road ||
			v->u.road.state == 254 ||
			myabs(v->z_pos - rvf->veh->z_pos) > 6 ||
			v->direction != rvf->dir ||
			(_dists[v->direction] < 0 && (x_diff <= _dists[v->direction] || x_diff > 0)) ||
			(_dists[v->direction] > 0 && (x_diff >= _dists[v->direction] || x_diff < 0)) ||
			(_dists[v->direction+8] < 0 && (y_diff <= _dists[v->direction+8] || y_diff > 0)) ||
			(_dists[v->direction+8] > 0 && (y_diff >= _dists[v->direction+8] || y_diff < 0)))
   				return NULL;

	return v;
}

static Vehicle *RoadVehFindCloseTo(Vehicle *v, int x, int y, byte dir)
{
	RoadVehFindData rvf;
	Vehicle *u;

	if (v->u.road.reverse_ctr != 0)
		return NULL;

	rvf.x = x;
	rvf.y = y;
	rvf.dir = dir;
	rvf.veh = v;
	u = VehicleFromPos(TILE_FROM_XY(x,y), &rvf, (VehicleFromPosProc*)EnumCheckRoadVehClose);

	// This code protects a roadvehicle from being blocked for ever
	//  If more then 1480 / 74 days a road vehicle is blocked, it will
	//  drive just through it. The ultimate backup-code of TTD.
	// It can be disabled.
	if (u == NULL) {
		v->u.road.unk2 = 0;
		return NULL;
	}

	if (++v->u.road.unk2 > 1480)
		return NULL;

	return u;
}

static void RoadVehArrivesAt(Vehicle *v, Station *st)
{
	if (v->engine_type < 123) {
		/* Check if station was ever visited before */
		if (!(st->had_vehicle_of_type & HVOT_BUS)) {
			uint32 flags;

			st->had_vehicle_of_type |= HVOT_BUS;
			SetDParam(0, st->index);
			flags = (v->owner == _local_player) ? NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_VEHICLE, NT_ARRIVAL_PLAYER, 0) : NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_VEHICLE, NT_ARRIVAL_OTHER, 0);
			AddNewsItem(
				STR_902F_CITIZENS_CELEBRATE_FIRST,
				flags,
				v->index,
				0);
		}
	} else {
		/* Check if station was ever visited before */
		if (!(st->had_vehicle_of_type & HVOT_TRUCK)) {
			uint32 flags;

			st->had_vehicle_of_type |= HVOT_TRUCK;
			SetDParam(0, st->index);
			flags = (v->owner == _local_player) ? NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_VEHICLE, NT_ARRIVAL_PLAYER, 0) : NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_VEHICLE, NT_ARRIVAL_OTHER, 0);
			AddNewsItem(
				STR_9030_CITIZENS_CELEBRATE_FIRST,
				flags,
				v->index,
				0);
		}
	}
}

static bool RoadVehAccelerate(Vehicle *v)
{
	uint spd = v->cur_speed + 1 + ((v->u.road.overtaking != 0)?1:0);
	byte t;

	// Clamp
	spd = min(spd, v->max_speed);

	//updates statusbar only if speed have changed to save CPU time
	if (spd != v->cur_speed) {
		v->cur_speed = spd;
		if (_patches.vehicle_speed)
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, 4);
	}

	// Decrease somewhat when turning
	if (!(v->direction&1))
		spd = spd * 3 >> 2;

	if (spd == 0)
		return false;

	if ((byte)++spd == 0)
		return true;

	v->progress = (t = v->progress) - (byte)spd;

	return (t < v->progress);
}

static byte RoadVehGetNewDirection(Vehicle *v, int x, int y)
{
	static const byte _roadveh_new_dir[11] = {
		0, 7, 6, 0,
		1, 0, 5, 0,
		2, 3, 4
	};

	x = x - v->x_pos + 1;
	y = y - v->y_pos + 1;

	if ((uint)x > 2 || (uint)y > 2)
		return v->direction;
	return _roadveh_new_dir[y*4+x];
}

static byte RoadVehGetSlidingDirection(Vehicle *v, int x, int y)
{
	byte b = RoadVehGetNewDirection(v,x,y);
	byte d = v->direction;
	if (b == d) return d;
	d = (d+1)&7;
	if (b==d) return d;
	d = (d-2)&7;
	if (b==d) return d;
	if (b==((d-1)&7)) return d;
	if (b==((d-2)&7)) return d;
	return (d+2)&7;
}

typedef struct OvertakeData {
	Vehicle *u, *v;
	uint tile;
	byte tilebits;
} OvertakeData;

void *EnumFindVehToOvertake(Vehicle *v, OvertakeData *od)
{
	if (v->tile != (TileIndex)od->tile ||
			v->type != VEH_Road ||
			v == od->u ||
			v == od->v)
				return NULL;
	return v;
}

static bool FindRoadVehToOvertake(OvertakeData *od)
{
	uint32 bits;

	bits = GetTileTrackStatus(od->tile, TRANSPORT_ROAD)&0x3F;

	if (!(od->tilebits & bits) || (bits&0x3C) || (bits & 0x3F3F0000))
		return true;
	return VehicleFromPos(od->tile, od, (VehicleFromPosProc*)EnumFindVehToOvertake) != NULL;
}

static void RoadVehCheckOvertake(Vehicle *v, Vehicle *u)
{
	OvertakeData od;
	byte tt;

	od.v = v;
	od.u = u;

	if (u->max_speed >= v->max_speed &&
			!(u->vehstatus&VS_STOPPED) &&
			u->cur_speed != 0)
				return;

	if (v->direction != u->direction || !(v->direction&1))
		return;

	if (v->u.road.state >= 32 || (v->u.road.state&7) > 1 )
		return;

	tt = (byte)(GetTileTrackStatus(v->tile, TRANSPORT_ROAD) & 0x3F);
	if ((tt & 3) == 0)
		return;
	if ((tt & 0x3C) != 0)
		return;

	if (tt == 3) {
		tt = (v->direction&2)?2:1;
	}
	od.tilebits = tt;

	od.tile = v->tile;
	if (FindRoadVehToOvertake(&od))
		return;

	od.tile = v->tile + _tileoffs_by_dir[v->direction>>1];
	if (FindRoadVehToOvertake(&od))
		return;

	if (od.u->cur_speed == 0 || od.u->vehstatus&VS_STOPPED) {
		v->u.road.overtaking_ctr = 0x11;
		v->u.road.overtaking = 0x10;
	} else {
//		if (FindRoadVehToOvertake(&od))
//			return;
		v->u.road.overtaking_ctr = 0;
		v->u.road.overtaking = 0x10;
	}
}

static void RoadZPosAffectSpeed(Vehicle *v, byte old_z)
{
	if (old_z == v->z_pos)
		return;

	if (old_z < v->z_pos) {
		v->cur_speed = v->cur_speed * 232 >> 8;
	} else {
		uint16 spd = v->cur_speed + 2;
		if (spd <= v->max_speed)
			v->cur_speed = spd;
	}
}

static int PickRandomBit(uint bits)
{
	uint num = 0;
	uint b = bits;
	uint i;

	do {
		if (b & 1)
			num++;
	} while (b >>= 1);

	num = ((uint16)Random() * num >> 16);

	for(i=0; !((bits & 1) && ((int)--num) < 0); bits>>=1,i++);
	return i;
}

typedef struct {
	TileIndex dest;
	uint maxtracklen;
	uint mindist;
} FindRoadToChooseData;

static bool EnumRoadTrackFindDist(uint tile, FindRoadToChooseData *frd, int track, uint length, byte *state)
{
	uint dist = GetTileDist(tile, frd->dest);
	if (dist <= frd->mindist) {
		if (dist != frd->mindist || length < frd->maxtracklen) {
			frd->maxtracklen = length;
		}
		frd->mindist = dist;
	}
	return false;
}

// Returns direction to choose
// or -1 if the direction is currently blocked
static int RoadFindPathToDest(Vehicle *v, uint tile, int direction)
{
#define return_track(x) {best_track = x; goto found_best_track; }

	uint16 signal;
	uint bitmask;
	uint desttile;
	FindRoadToChooseData frd;
	int best_track;
	uint best_dist, best_maxlen;
	uint i;
	byte m5;

	{
		uint32 r;
		r = GetTileTrackStatus(tile, TRANSPORT_ROAD);
		signal = (uint16)(r >> 16);
		bitmask = (uint16)r;
	}

	if (IS_TILETYPE(tile, MP_STREET)) {
		if ((_map5[tile]&0xF0) == 0x20 && v->owner == _map_owner[tile])
			/* Road crossing */
			bitmask |= _road_veh_fp_ax_or[_map5[tile]&3];
	} else if (IS_TILETYPE(tile, MP_STATION)) {
		if (_map_owner[tile] == OWNER_NONE || _map_owner[tile] == v->owner) {
			/* Our station */
			Station *st = DEREF_STATION(_map2[tile]);
			byte val = _map5[tile];
			if (v->cargo_type != CT_PASSENGERS) {
				if (IS_BYTE_INSIDE(val, 0x43, 0x47) && (_patches.roadveh_queue || st->truck_stop_status&3))
					bitmask |= _road_veh_fp_ax_or[(val-0x43)&3];
			} else {
				if (IS_BYTE_INSIDE(val, 0x47, 0x4B) && (_patches.roadveh_queue || st->bus_stop_status&3))
					bitmask |= _road_veh_fp_ax_or[(val-0x47)&3];
			}
		}
	}
	/* The above lookups should be moved to GetTileTrackStatus in the
	 * future, but that requires more changes to the pathfinder and other
	 * stuff, probably even more arguments to GTTS.
	 */

	/* remove unreachable tracks */
	bitmask &= _road_veh_fp_ax_and[direction];
	if (bitmask == 0) {
		// reverse
		return_track(_road_reverse_table[direction]);
	}

	if (v->u.road.reverse_ctr != 0) {
		v->u.road.reverse_ctr = 0;
		if (v->tile != (TileIndex)tile) {
			return_track(_road_reverse_table[direction]);
		}
	}

	desttile = v->dest_tile;
	if (desttile == 0) {
		// Pick a random direction
		return_track(PickRandomBit(bitmask));
	}

	// Only one direction to choose between?
	if (!(bitmask & (bitmask - 1))) {
		return_track(FindFirstBit2x64(bitmask));
	}

	if (IS_TILETYPE(desttile, MP_STREET)) {
		m5 = _map5[desttile];
		if ((m5&0xF0) == 0x20)
			goto do_it;
	} else if (IS_TILETYPE(desttile, MP_STATION)) {
		m5 = _map5[desttile];
		if (IS_BYTE_INSIDE(m5, 0x43, 0x4B)) {
			m5 -= 0x43;
do_it:;
			desttile += _tileoffs_by_dir[m5&3];
			if (desttile == tile && bitmask&_road_pf_table_3[m5&3]) {
				return_track(FindFirstBit2x64(bitmask&_road_pf_table_3[m5&3]));
			}
		}
	}
	// do pathfind
	frd.dest = desttile;

	best_track = -1;
	best_dist = (uint)-1;
	best_maxlen = (uint)-1;
	i = 0;
	do {
		if (bitmask & 1) {
			if (best_track == -1) best_track = i; // in case we don't find the path, just pick a direction
			frd.maxtracklen = (uint)-1;
			frd.mindist = (uint)-1;
			FollowTrack(tile, 0x3000 | TRANSPORT_ROAD, _road_pf_directions[i], (TPFEnumProc*)EnumRoadTrackFindDist, NULL, &frd);

			if (frd.mindist < best_dist || (frd.mindist==best_dist && frd.maxtracklen < best_maxlen)) {
				best_dist = frd.mindist;
				best_maxlen = frd.maxtracklen;
				best_track = i;
			}
		}
	} while (++i,(bitmask>>=1) != 0);

found_best_track:;

	if (HASBIT(signal, best_track))
		return -1;

	return best_track;
}

typedef struct RoadDriveEntry {
	byte x,y;
} RoadDriveEntry;

#include "table/roadveh.h"

static const byte _road_veh_data_1[] = {
	20, 20, 16, 16, 0, 0, 0, 0,
	19, 19, 15, 15, 0, 0, 0, 0,
	16, 16, 12, 12, 0, 0, 0, 0,
	15, 15, 11, 11
};

static const byte _roadveh_data_2[4] = { 0,1,8,9 };

static void RoadVehEventHandler(Vehicle *v)
{
	GetNewVehiclePosResult gp;
	byte new_dir, old_dir;
	RoadDriveEntry rd;
	int x,y;
	Station *st;
	uint32 r;
	Vehicle *u;

	// decrease counters
	v->tick_counter++;
	if (v->u.road.reverse_ctr != 0)
		v->u.road.reverse_ctr--;

	// handle crashed
	if (v->u.road.crashed_ctr != 0) {
		RoadVehIsCrashed(v);
		return;
	}

	RoadVehCheckTrainCrash(v);

	// road vehicle has broken down?
	if (v->breakdown_ctr != 0) {
		if (v->breakdown_ctr <= 2) {
			HandleBrokenRoadVeh(v);
			return;
		}
		v->breakdown_ctr--;
	}

	// exit if vehicle is stopped
	if (v->vehstatus & VS_STOPPED)
		return;

	ProcessRoadVehOrder(v);
	HandleRoadVehLoading(v);

	if (v->current_order.type == OT_LOADING)
		return;

	if (v->u.road.state == 254) {
		int dir;
		const RoadDriveEntry*rdp;
		byte rd2;

		v->cur_speed = 0;

		dir = _map5[v->tile]&3;
		v->direction = dir*2+1;

		rd2 = _roadveh_data_2[dir];
		rdp = _road_drive_data[(_opt.road_side<<4) + rd2];

		x = GET_TILE_X(v->tile)*16 + (rdp[6].x&0xF);
		y = GET_TILE_Y(v->tile)*16 + (rdp[6].y&0xF);

		if (RoadVehFindCloseTo(v,x,y,v->direction))
			return;

		VehicleServiceInDepot(v);

		StartRoadVehSound(v);

		BeginVehicleMove(v);

		v->vehstatus &= ~VS_HIDDEN;
		v->u.road.state = rd2;
		v->u.road.frame = 6;

		v->cur_image = GetRoadVehImage(v, v->direction);
		UpdateRoadVehDeltaXY(v);
		SetRoadVehPosition(v,x,y);

		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
		return;
	}

	if (!RoadVehAccelerate(v))
		return;

	if (v->u.road.overtaking != 0)  {
		if (++v->u.road.overtaking_ctr >= 35)
			v->u.road.overtaking = 0;
	}

	BeginVehicleMove(v);

	if (v->u.road.state == 255) {
		GetNewVehiclePos(v, &gp);

		if (RoadVehFindCloseTo(v, gp.x, gp.y, v->direction)) {
			v->cur_speed = 0;
			return;
		}

		if (IS_TILETYPE(gp.new_tile, MP_TUNNELBRIDGE) &&
				(_map5[gp.new_tile]&0xF0) == 0 &&
				(VehicleEnterTile(v, gp.new_tile, gp.x, gp.y)&4)) {

			//new_dir = RoadGetNewDirection(v, gp.x, gp.y)
			v->cur_image = GetRoadVehImage(v, v->direction);
			UpdateRoadVehDeltaXY(v);
			SetRoadVehPosition(v,gp.x,gp.y);
			return;
		}

		v->x_pos = gp.x;
		v->y_pos = gp.y;
		VehiclePositionChanged(v);
		return;
	}

	rd = _road_drive_data[(v->u.road.state + (_opt.road_side<<4)) ^ v->u.road.overtaking][v->u.road.frame+1];

// switch to another tile
	if (rd.x & 0x80) {
		uint tile = v->tile + _tileoffs_by_dir[rd.x&3];
		int dir = RoadFindPathToDest(v, tile, rd.x&3);
		int tmp;
		uint32 r;
		byte newdir;
		const RoadDriveEntry *rdp;

		if (dir == -1) {
			v->cur_speed = 0;
			return;
		}

again:
		if ((dir & 7) >= 6) {
			tile = v->tile;
		}

		tmp = (dir+(_opt.road_side<<4))^v->u.road.overtaking;
		rdp = _road_drive_data[tmp];

		tmp &= ~0x10;

		x = GET_TILE_X(tile)*16 + rdp[0].x;
		y = GET_TILE_Y(tile)*16 + rdp[0].y;

		if (RoadVehFindCloseTo(v, x, y, newdir=RoadVehGetSlidingDirection(v, x, y)))
			return;

		r = VehicleEnterTile(v, tile, x, y);
		if (r & 8) {
			if (!IS_TILETYPE(tile, MP_TUNNELBRIDGE)) {
				v->cur_speed = 0;
				return;
			}
			dir = _road_reverse_table[rd.x&3];
			goto again;
		}

		if (IS_BYTE_INSIDE(v->u.road.state, 0x20, 0x30) && IS_TILETYPE(v->tile, MP_STATION)) {
			if ((tmp&7) >= 6) { v->cur_speed = 0; return; }
			if (IS_BYTE_INSIDE(_map5[v->tile], 0x43, 0x4B)) {
				Station *st = DEREF_STATION(_map2[v->tile]);
				byte *b;

				if (_map5[v->tile] >= 0x47) {
					b = &st->bus_stop_status;
				} else {
					b = &st->truck_stop_status;
				}
				*b = (*b | ((v->u.road.state&2)?2:1)) & 0x7F;
			}
		}

		if (!(r & 4)) {
			v->tile = tile;
			v->u.road.state = (byte)tmp;
			v->u.road.frame = 0;
		}
		if (newdir != v->direction) {
			v->direction = newdir;
			v->cur_speed -= v->cur_speed >> 2;
		}

		v->cur_image = GetRoadVehImage(v, newdir);
		UpdateRoadVehDeltaXY(v);
		RoadZPosAffectSpeed(v, SetRoadVehPosition(v, x, y));
		return;
	}

	if (rd.x & 0x40) {
		int dir = RoadFindPathToDest(v, v->tile,	rd.x&3);
		uint32 r;
		int tmp;
		byte newdir;
		const RoadDriveEntry *rdp;

		if (dir == -1) {
			v->cur_speed = 0;
			return;
		}

		tmp = (_opt.road_side<<4) + dir;
		rdp = _road_drive_data[tmp];

		x = GET_TILE_X(v->tile)*16 + rdp[1].x;
		y = GET_TILE_Y(v->tile)*16 + rdp[1].y;

		if (RoadVehFindCloseTo(v, x, y, newdir=RoadVehGetSlidingDirection(v, x, y)))
			return;

		r = VehicleEnterTile(v, v->tile, x, y);
		if (r & 8) {
			v->cur_speed = 0;
			return;
		}

		v->u.road.state = tmp & ~16;
		v->u.road.frame = 1;

		if (newdir != v->direction) {
			v->direction = newdir;
			v->cur_speed -= v->cur_speed >> 2;
		}

		v->cur_image = GetRoadVehImage(v, newdir);
		UpdateRoadVehDeltaXY(v);
		RoadZPosAffectSpeed(v, SetRoadVehPosition(v, x, y));
		return;
	}

	x = (v->x_pos&~15)+(rd.x&15);
	y = (v->y_pos&~15)+(rd.y&15);

	new_dir = RoadVehGetSlidingDirection(v, x, y);

	if (!IS_BYTE_INSIDE(v->u.road.state, 0x20, 0x30) && (u=RoadVehFindCloseTo(v, x, y, new_dir)) != NULL) {
		if (v->u.road.overtaking == 0)
			RoadVehCheckOvertake(v, u);
		return;
	}

	old_dir = v->direction;
	if (new_dir != old_dir) {
		v->direction = new_dir;
		v->cur_speed -= (v->cur_speed >> 2);
		if (old_dir != v->u.road.state) {
			v->cur_image = GetRoadVehImage(v, new_dir);
			UpdateRoadVehDeltaXY(v);
			SetRoadVehPosition(v, v->x_pos, v->y_pos);
			return;
		}
	}

	if (v->u.road.state >= 0x20 &&
			_road_veh_data_1[v->u.road.state - 0x20 + (_opt.road_side<<4)] == v->u.road.frame) {
		byte *b;

		st = DEREF_STATION(_map2[v->tile]);
		b = IS_BYTE_INSIDE(_map5[v->tile], 0x43, 0x47) ? &st->truck_stop_status : &st->bus_stop_status;

		if (v->current_order.type != OT_LEAVESTATION &&
				v->current_order.type != OT_GOTO_DEPOT) {
			Order old_order;

			*b &= ~0x80;

			v->last_station_visited = _map2[v->tile];

			RoadVehArrivesAt(v, st);

			old_order = v->current_order;
			v->current_order.type = OT_LOADING;
			v->current_order.flags = 0;

			if (old_order.type == OT_GOTO_STATION &&
					v->current_order.station == v->last_station_visited) {
				v->current_order.flags =
					(old_order.flags & (OF_FULL_LOAD | OF_UNLOAD)) | OF_NON_STOP;
			}

			SET_EXPENSES_TYPE(EXPENSES_ROADVEH_INC);
			if (LoadUnloadVehicle(v)) {
				InvalidateWindow(WC_ROADVEH_LIST, v->owner);
				MarkRoadVehDirty(v);
			}
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, 4);
			return;
		}

		if (v->current_order.type != OT_GOTO_DEPOT) {
			if (*b&0x80) {
				v->cur_speed = 0;
				return;
			}
			v->current_order.type = OT_NOTHING;
			v->current_order.flags = 0;
		}
		*b |= 0x80;

		StartRoadVehSound(v);
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, 4);
	}

	r = VehicleEnterTile(v, v->tile, x, y);
	if (r & 8) {
		v->cur_speed = 0;
		return;
	}

	if ((r & 4) == 0) {
		v->u.road.frame++;
	}

	v->cur_image = GetRoadVehImage(v, v->direction);
	UpdateRoadVehDeltaXY(v);
	RoadZPosAffectSpeed(v, SetRoadVehPosition(v, x, y));
}

void RoadVehEnterDepot(Vehicle *v)
{
	v->u.road.state = 254;
	v->vehstatus |= VS_HIDDEN;

	InvalidateWindow(WC_VEHICLE_DETAILS, v->index);

	MaybeRenewVehicle(v, EstimateRoadVehCost(v->engine_type));

	VehicleServiceInDepot(v);

	TriggerVehicle(v, VEHICLE_TRIGGER_DEPOT);

	if (v->current_order.type == OT_GOTO_DEPOT) {
		Order t;

		InvalidateWindow(WC_VEHICLE_VIEW, v->index);

		t = v->current_order;
		v->current_order.type = OT_DUMMY;
		v->current_order.flags = 0;

		// Part of the schedule?
		if (t.flags & OF_UNLOAD) {
			v->cur_order_index++;
		} else if (t.flags & OF_FULL_LOAD) {
			v->vehstatus |= VS_STOPPED;
			if (v->owner == _local_player) {
				SetDParam(0, v->unitnumber);
				AddNewsItem(
					STR_9016_ROAD_VEHICLE_IS_WAITING,
					NEWS_FLAGS(NM_SMALL, NF_VIEWPORT|NF_VEHICLE, NT_ADVICE, 0),
					v->index,
					0);
			}
		}
	}

	InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
}

static void AgeRoadVehCargo(Vehicle *v)
{
	if (_age_cargo_skip_counter != 0)
		return;
	if (v->cargo_days != 255)
		v->cargo_days++;
}

void RoadVeh_Tick(Vehicle *v)
{
	AgeRoadVehCargo(v);
	RoadVehEventHandler(v);
}

static void CheckIfRoadVehNeedsService(Vehicle *v)
{
	int i;

	if (_patches.servint_roadveh == 0)
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

	i = FindClosestRoadDepot(v);

	if (i < 0 || GetTileDist(v->tile, (&_depots[i])->xy) > 12) {
		if (v->current_order.type == OT_GOTO_DEPOT) {
			v->current_order.type = OT_DUMMY;
			v->current_order.flags = 0;
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, 4);
		}
		return;
	}

	if (v->current_order.type == OT_GOTO_DEPOT &&
			v->current_order.flags & OF_NON_STOP &&
			!CHANCE16(1,20))
		return;

	v->current_order.type = OT_GOTO_DEPOT;
	v->current_order.flags = OF_NON_STOP;
	v->current_order.station = (byte)i;
	v->dest_tile = (&_depots[i])->xy;
	InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, 4);
}

void OnNewDay_RoadVeh(Vehicle *v)
{
	int32 cost;
	Station *st;
	uint tile;

	if ((++v->day_counter & 7) == 0)
		DecreaseVehicleValue(v);

	if (v->u.road.unk2 == 0)
		CheckVehicleBreakdown(v);

	AgeVehicle(v);
	CheckIfRoadVehNeedsService(v);

	CheckOrders(v);

	/* update destination */
	if (v->current_order.type == OT_GOTO_STATION) {
		st = DEREF_STATION(v->current_order.station);
		if ((tile=(v->cargo_type==CT_PASSENGERS ? st->bus_tile : st->lorry_tile)) != 0)
			v->dest_tile = tile;
	}

	if (v->vehstatus & VS_STOPPED)
		return;

	cost = RoadVehInfo(v->engine_type)->running_cost * _price.roadveh_running / 364;

	v->profit_this_year -= cost >> 8;

	SET_EXPENSES_TYPE(EXPENSES_ROADVEH_RUN);
	SubtractMoneyFromPlayerFract(v->owner, cost);

	InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
	InvalidateWindow(WC_ROADVEH_LIST, v->owner);
}

void HandleClickOnRoadVeh(Vehicle *v)
{
	ShowRoadVehViewWindow(v);
}

void RoadVehiclesYearlyLoop()
{
	Vehicle *v;

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Road) {
			v->profit_last_year = v->profit_this_year;
			v->profit_this_year = 0;
			InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
		}
	}
}

