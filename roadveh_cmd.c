#include "stdafx.h"
#include "ttd.h"
#include "debug.h"
#include "table/strings.h"
#include "map.h"
#include "tile.h"
#include "vehicle.h"
#include "engine.h"
#include "command.h"
#include "station.h"
#include "news.h"
#include "gfx.h"
#include "pathfind.h"
#include "npf.h"
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

int32 EstimateRoadVehCost(byte engine_type)
{
	return ((_price.roadveh_base >> 3) * RoadVehInfo(engine_type)->base_cost) >> 5;
}

// p1 = engine_type
// p2 not used
int32 CmdBuildRoadVeh(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	int32 cost;
	Vehicle *v;
	UnitID unit_num;
	uint tile = TILE_FROM_XY(x,y);
	Engine *e;

	if (!IsEngineBuildable(p1, VEH_Road)) return CMD_ERROR;

	if (!IsRoadDepotTile((TileIndex)tile)) return CMD_ERROR;

	if (_map_owner[tile] != _current_player) return CMD_ERROR;

	SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);

	cost = EstimateRoadVehCost(p1);
	if (flags & DC_QUERY_COST)
		return cost;

	v = AllocateVehicle();
	if (v == NULL || IsOrderPoolFull())
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
		x = TileX(tile) * 16 + 8;
		y = TileY(tile) * 16 + 8;
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

		v->u.road.slot = NULL;
		v->u.road.slotindex = 0;
		v->u.road.slot_age = 0;

		v->last_station_visited = INVALID_STATION;
		v->max_speed = rvi->max_speed;
		v->engine_type = (byte)p1;

		e = &_engines[p1];
		v->reliability = e->reliability;
		v->reliability_spd_dec = e->reliability_spd_dec;
		v->max_age = e->lifelength * 366;
		_new_roadveh_id = v->index;

		v->string_id = STR_SV_ROADVEH_NAME;

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

	InvalidateWindow(WC_REPLACE_VEHICLE, VEH_Road); // updates the replace Road window

	return cost;
}

// p1 = vehicle
int32 CmdStartStopRoadVeh(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;

	if (!IsVehicleIndex(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_Road || !CheckOwnership(v->owner))
		return CMD_ERROR;

	if (flags & DC_EXEC) {
		v->vehstatus ^= VS_STOPPED;
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
	}

	return 0;
}

//  p1 = vehicle index in GetVehicle()
//  p2 not used
int32 CmdSellRoadVeh(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;

	if (!IsVehicleIndex(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_Road || !CheckOwnership(v->owner))
		return CMD_ERROR;

	SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);

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
	InvalidateWindow(WC_REPLACE_VEHICLE, VEH_Road); // updates the replace Road window

	return -(int32)v->value;
}

typedef struct RoadFindDepotData {
	uint best_length;
	uint tile;
	byte owner;
} RoadFindDepotData;

static const byte _road_pf_directions[16] = {
	0, 1, 0, 1, 2, 1, 255, 255,
	2, 3, 3, 2, 3, 0, 255, 255,
};

static bool EnumRoadSignalFindDepot(uint tile, RoadFindDepotData *rfdd, int track, uint length, byte *state)
{
	tile += TileOffsByDir(_road_pf_directions[track]);

	if (IsTileType(tile, MP_STREET) &&
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

	if (v->u.road.state == 255) { tile = GetVehicleOutOfTunnelTile(v); }

	if (_patches.new_pathfinding_all) {
		NPFFoundTargetData ftd;
		/* See where we are now */
		byte trackdir = _dir_to_diag_trackdir[(v->direction>>1)&3];
		ftd = NPFRouteToDepotBreadthFirst(v->tile, trackdir, TRANSPORT_ROAD);
		if (ftd.best_bird_dist == 0)
			return GetDepotByTile(ftd.node.tile); /* Target found */
		else
			return -1; /* Target not found */
		/* We do not search in two directions here, why should we? We can't reverse right now can we? */
	} else {
		RoadFindDepotData rfdd;
		rfdd.owner = v->owner;
		rfdd.best_length = (uint)-1;

		/* search in all directions */
		for(i=0; i!=4; i++)
			FollowTrack(tile, 0x2000 | TRANSPORT_ROAD, i, (TPFEnumProc*)EnumRoadSignalFindDepot, NULL, &rfdd);

		if (rfdd.best_length == (uint)-1)
			return -1;

		return GetDepotByTile(rfdd.tile);
	}
}

/*  Send a road vehicle to the nearest depot
	p1 = index of the road vehicle
	p2 = bit 0 = do not stop in depot
		 bit 1 = set v->set_for_replacement
		 bit 2 = clear v->set_for_replacement */
int32 CmdSendRoadVehToDepot(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;
	int depot;

	if (!IsVehicleIndex(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_Road || !CheckOwnership(v->owner))
		return CMD_ERROR;

	if (HASBIT(p2, 0)) v->set_for_replacement = true;
	if (HASBIT(p2, 2)) v->set_for_replacement = false;

	if (HASBIT(p2, 1) || HASBIT(p2, 2)) return CMD_ERROR;   // vehicle has a depot in schedule. It just needed to alter set_for_replacement

	if (v->current_order.type == OT_GOTO_DEPOT) {
		if (flags & DC_EXEC) {
			if (v->current_order.flags & OF_UNLOAD)
				v->cur_order_index++;
			v->current_order.type = OT_DUMMY;
			v->current_order.flags = 0;
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
		}
		return 0;
	}

	depot = FindClosestRoadDepot(v);
	if (depot < 0)
		return_cmd_error(STR_9019_UNABLE_TO_FIND_LOCAL_DEPOT);

	if (flags & DC_EXEC) {
		v->current_order.type = OT_GOTO_DEPOT;
		v->current_order.flags = p2 == 0 ? OF_NON_STOP | OF_FULL_LOAD : 0;
		v->current_order.station = (byte)depot;
		v->dest_tile = _depots[depot].xy;
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
	}

	return 0;
}

int32 CmdTurnRoadVeh(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;

	if (!IsVehicleIndex(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

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

	if (!IsVehicleIndex(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

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
	byte *b, bb;

	RoadStop *rs = GetRoadStopByTile(tile, GetRoadStopType(tile));
	b = &rs->status;

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

	if (IsTileType(v->tile, MP_STATION))
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

	InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);

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
	if (!IsTileType(tile, MP_STREET) ||
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
	const Order *order;
	const Station *st;

	if (v->current_order.type >= OT_GOTO_DEPOT && v->current_order.type <= OT_LEAVESTATION) {
		// Let a depot order in the orderlist interrupt.
		if (v->current_order.type != OT_GOTO_DEPOT ||
				!(v->current_order.flags & OF_UNLOAD))
			return;
	}

	if (v->current_order.type == OT_GOTO_DEPOT &&
			(v->current_order.flags & (OF_UNLOAD | OF_FULL_LOAD)) == (OF_UNLOAD | OF_FULL_LOAD) &&
			!VehicleNeedsService(v) && !v->set_for_replacement) {
		v->cur_order_index++;
	}

	if (v->cur_order_index >= v->num_orders)
		v->cur_order_index = 0;

	order = GetVehicleOrder(v, v->cur_order_index);

	if (order == NULL) {
		v->current_order.type = OT_NOTHING;
		v->current_order.flags = 0;
		v->dest_tile = 0;
		return;
	}

	if (order->type    == v->current_order.type &&
			order->flags   == v->current_order.flags &&
			order->station == v->current_order.station)
		return;

	v->current_order = *order;

	v->dest_tile = 0;

	if (order->type == OT_GOTO_STATION) {
		if (order->station == v->last_station_visited)
			v->last_station_visited = INVALID_STATION;
		st = GetStation(order->station);

		{
			int32 *dist;
			int32 mindist = 0xFFFFFFFF;
			int num;
			RoadStopType type;
			RoadStop *rs;

			type = (v->cargo_type == CT_PASSENGERS) ? RS_BUS : RS_TRUCK;
			num = GetNumRoadStops(st, type);
			rs = GetPrimaryRoadStop(st, type);

			if (rs == NULL) {
				//There is no stop left at the station, so don't even TRY to go there
				v->cur_order_index++;
				InvalidateVehicleOrder(v);

				return;
			}

			dist = malloc(num * sizeof(int32));

			do {
				*dist = DistanceSquare(v->tile, rs->xy);
				if (*dist < mindist) {
					v->dest_tile = rs->xy;
				}
				rs = rs->next;
			} while ( rs != NULL );

			free(dist);
			dist = NULL;
		}
	} else if (order->type == OT_GOTO_DEPOT) {
		v->dest_tile = _depots[order->station].xy;
	}

	InvalidateVehicleOrder(v);
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
	InvalidateVehicleOrder(v);

	if (v->current_order.type != OT_GOTO_DEPOT && v->owner == _local_player) {
		// only the vehicle owner needs to calculate the rest (locally)
		if ((_autoreplace_array[v->engine_type] != v->engine_type) ||
			(_patches.autorenew && v->age - v->max_age > (_patches.autorenew_months * 30))) {
			byte flags = 1;
			// the flags means, bit 0 = needs to go to depot, bit 1 = have depot in orders
			if (VehicleHasDepotOrders(v)) SETBIT(flags, 1);
			if (!(HASBIT(flags, 1) && v->set_for_replacement)) {
				_current_player = _local_player;
				DoCommandP(v->tile, v->index, flags, NULL, CMD_SEND_ROADVEH_TO_DEPOT | CMD_SHOW_NO_ERROR);
				_current_player = OWNER_NONE;
			}
		} else { // no need to go to a depot
			if (v->set_for_replacement) {
				// it seems that the user clicked "Stop replacing"
				_current_player = _local_player;
				DoCommandP(v->tile, v->index, 1 | (1 << 2), NULL, CMD_SEND_ROADVEH_TO_DEPOT | CMD_SHOW_NO_ERROR);
				_current_player = OWNER_NONE;
			}
		}
	}
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

static void *EnumCheckRoadVehClose(Vehicle *v, RoadVehFindData *rvf)
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
	//  If more than 1480 / 74 days a road vehicle is blocked, it will
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
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
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

static void *EnumFindVehToOvertake(Vehicle *v, OvertakeData *od)
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

	od.tile = v->tile + TileOffsByDir(v->direction >> 1);
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
	uint dist = DistanceManhattan(tile, frd->dest);
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
static int RoadFindPathToDest(Vehicle *v, uint tile, int enterdir)
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

	if (IsTileType(tile, MP_STREET)) {
		if ((_map5[tile]&0xF0) == 0x20 && v->owner == _map_owner[tile])
			/* Road crossing */
			bitmask |= _road_veh_fp_ax_or[_map5[tile]&3];
	} else if (IsTileType(tile, MP_STATION)) {
		if (_map_owner[tile] == OWNER_NONE || _map_owner[tile] == v->owner) {
			/* Our station */
			Station *st = GetStation(_map2[tile]);
			byte val = _map5[tile];
			if (v->cargo_type != CT_PASSENGERS) {
				if (IS_BYTE_INSIDE(val, 0x43, 0x47) && (_patches.roadveh_queue || st->truck_stops->status&3))
					bitmask |= _road_veh_fp_ax_or[(val-0x43)&3];
			} else {
				if (IS_BYTE_INSIDE(val, 0x47, 0x4B) && (_patches.roadveh_queue || st->bus_stops->status&3))
					bitmask |= _road_veh_fp_ax_or[(val-0x47)&3];
			}
		}
	}
	/* The above lookups should be moved to GetTileTrackStatus in the
	 * future, but that requires more changes to the pathfinder and other
	 * stuff, probably even more arguments to GTTS.
	 */

	/* remove unreachable tracks */
	bitmask &= _road_veh_fp_ax_and[enterdir];
	if (bitmask == 0) {
		/* No reachable tracks, so we'll reverse */
		return_track(_road_reverse_table[enterdir]);
	}

	if (v->u.road.reverse_ctr != 0) {
		/* What happens here?? */
		v->u.road.reverse_ctr = 0;
		if (v->tile != (TileIndex)tile) {
			return_track(_road_reverse_table[enterdir]);
		}
	}

	desttile = v->dest_tile;
	if (desttile == 0) {
		// Pick a random track
		return_track(PickRandomBit(bitmask));
	}

	// Only one track to choose between?
	if (!(KillFirstBit2x64(bitmask))) {
		return_track(FindFirstBit2x64(bitmask));
	}

	if (_patches.new_pathfinding_all) {
		NPFFindStationOrTileData fstd;
		NPFFoundTargetData ftd;
		byte trackdir;

		NPFFillWithOrderData(&fstd, v);
		trackdir = _dir_to_diag_trackdir[enterdir];
		//debug("Finding path. Enterdir: %d, Trackdir: %d", enterdir, trackdir);

		ftd = NPFRouteToStationOrTile(tile - TileOffsByDir(enterdir), trackdir, &fstd, TRANSPORT_ROAD);
		if (ftd.best_bird_dist != 0 || ftd.best_trackdir == 0xff) {
			/* Not found, just do something, or we are already there */
			//TODO: maybe display error?
			//TODO: go straight ahead if possible?
			return_track(FindFirstBit2x64(bitmask));
		} else {
			return_track(ftd.best_trackdir);
		}
	} else {
		if (IsTileType(desttile, MP_STREET)) {
			m5 = _map5[desttile];
			if ((m5&0xF0) == 0x20)
				/* We are heading for a Depot */
				goto do_it;
		} else if (IsTileType(desttile, MP_STATION)) {
			m5 = _map5[desttile];
			if (IS_BYTE_INSIDE(m5, 0x43, 0x4B)) {
				/* We are heading for a station */
				m5 -= 0x43;
do_it:;
				/* When we are heading for a depot or station, we just
				 * pretend we are heading for the tile in front, we'll
				 * see from there */
				desttile += TileOffsByDir(m5 & 3);
				if (desttile == tile && bitmask&_road_pf_table_3[m5&3]) {
					/* If we are already in front of the
					 * station/depot and we can get in from here,
					 * we enter */
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
				if (best_track == -1) best_track = i; // in case we don't find the path, just pick a track
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
	}

found_best_track:;

	if (HASBIT(signal, best_track))
		return -1;

	return best_track;
}

static int RoadFindPathToStation(const Vehicle *v, TileIndex tile)
{
	FindRoadToChooseData frd;
	int i, best_track = -1;
	uint best_dist = (uint) -1, best_maxlen = (uint) -1;

	frd.dest = tile;
	frd.maxtracklen = (uint) -1;
	frd.mindist = (uint) -1;

	for (i = 0; i < 4; i++) {
		FollowTrack(v->tile, 0x2000 | TRANSPORT_ROAD, i, (TPFEnumProc*)EnumRoadTrackFindDist, NULL, &frd);

		if (frd.mindist < best_dist || (frd.mindist == best_dist && frd.maxtracklen < best_maxlen )) {
			best_dist = frd.mindist;
			best_maxlen = frd.maxtracklen;
			best_track = i;
		}
	}
	return best_maxlen;
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

static inline void ClearSlot(Vehicle *v, RoadStop *rs)
{
	DEBUG(ms, 3) ("Multistop: Clearing slot %d at 0x%x", v->u.road.slotindex, rs->xy);
	v->u.road.slot = NULL;
	v->u.road.slot_age = 0;
	rs->slot[v->u.road.slotindex] = INVALID_SLOT;
}

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

		x = TileX(v->tile) * 16 + (rdp[6].x & 0xF);
		y = TileY(v->tile) * 16 + (rdp[6].y & 0xF);

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

		if (IsTileType(gp.new_tile, MP_TUNNELBRIDGE) &&
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
		uint tile = v->tile + TileOffsByDir(rd.x & 3);
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
			/* Turning around */
			tile = v->tile;
		}

		tmp = (dir+(_opt.road_side<<4))^v->u.road.overtaking;
		rdp = _road_drive_data[tmp];

		tmp &= ~0x10;

		x = TileX(tile) * 16 + rdp[0].x;
		y = TileY(tile) * 16 + rdp[0].y;

		if (RoadVehFindCloseTo(v, x, y, newdir=RoadVehGetSlidingDirection(v, x, y)))
			return;

		r = VehicleEnterTile(v, tile, x, y);
		if (r & 8) {
			if (!IsTileType(tile, MP_TUNNELBRIDGE)) {
				v->cur_speed = 0;
				return;
			}
			dir = _road_reverse_table[rd.x&3];
			goto again;
		}

		if (IS_BYTE_INSIDE(v->u.road.state, 0x20, 0x30) && IsTileType(v->tile, MP_STATION)) {
			if ((tmp&7) >= 6) { v->cur_speed = 0; return; }
			if (IS_BYTE_INSIDE(_map5[v->tile], 0x43, 0x4B)) {
				RoadStop *rs = GetRoadStopByTile(v->tile, GetRoadStopType(v->tile));
				byte *b = &rs->status;

				//we have reached a loading bay, mark it as used
				//and clear the usage bit (0x80) of the stop
				*b = (*b | ((v->u.road.state&2)?2:1)) & ~0x80;
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

		x = TileX(v->tile) * 16 + rdp[1].x;
		y = TileY(v->tile) * 16 + rdp[1].y;

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
		RoadStop *rs = GetRoadStopByTile(v->tile, GetRoadStopType(v->tile));
		byte *b = &rs->status;

		st = GetStation(_map2[v->tile]);

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
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
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

		if (rs == v->u.road.slot) {
			//we have arrived at the correct station
			ClearSlot(v, rs);
		} else if (v->u.road.slot != NULL) {
			//we have arrived at the wrong station
			//XXX The question is .. what to do? Actually we shouldn't be here
			//but I guess we need to clear the slot
 			DEBUG(ms, 1) ("Multistop: Wrong station, force a slot clearing. Vehicle %d at 0x%x, should go to 0x%x of station %d (%x), destination 0x%x", v->unitnumber, v->tile, v->u.road.slot->xy, st->index, st->xy, v->dest_tile);
			ClearSlot(v, v->u.road.slot);
		}

		StartRoadVehSound(v);
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
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

	MaybeReplaceVehicle(v);

	VehicleServiceInDepot(v);

	TriggerVehicle(v, VEHICLE_TRIGGER_DEPOT);

	if (v->current_order.type == OT_GOTO_DEPOT) {
		Order t;

		InvalidateWindow(WC_VEHICLE_VIEW, v->index);

		t = v->current_order;
		v->current_order.type = OT_DUMMY;
		v->current_order.flags = 0;

		// Part of the orderlist?
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
	InvalidateWindowClasses(WC_ROADVEH_LIST);
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

	if (_patches.servint_roadveh == 0 && !v->set_for_replacement)
		return;

	if (!VehicleNeedsService(v) && !v->set_for_replacement)
		return;

	if (v->vehstatus & VS_STOPPED)
		return;

	if (_patches.gotodepot && VehicleHasDepotOrders(v))
		return;

	// Don't interfere with a depot visit scheduled by the user, or a
	// depot visit by the order list.
	if (v->current_order.type == OT_GOTO_DEPOT &&
			(v->current_order.flags & (OF_FULL_LOAD | OF_UNLOAD)) != 0)
		return;

	//If we already got a slot at a stop, use that FIRST, and go to a depot later
	if (v->u.road.slot != NULL)
		return;

	i = FindClosestRoadDepot(v);

	if (i < 0 || DistanceManhattan(v->tile, (&_depots[i])->xy) > 12) {
		if (v->current_order.type == OT_GOTO_DEPOT && !(
			DistanceManhattan(v->tile, v->dest_tile) > 25 && v->set_for_replacement)) {
			/*  a vehicle needs a greater distance to a depot to loose it than to find it since
			they can circle forever othervise if they are in a loop with an unlucky distance	*/
			v->current_order.type = OT_DUMMY;
			v->current_order.flags = 0;
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
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
	InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
}

int dist_compare(const void *a, const void *b)
{
	return ( *(const uint32 *)a) - ( *(const uint32 *) b);
}

void OnNewDay_RoadVeh(Vehicle *v)
{
	int32 cost;
	Station *st;

	if ((++v->day_counter & 7) == 0)
		DecreaseVehicleValue(v);

	if (v->u.road.unk2 == 0)
		CheckVehicleBreakdown(v);

	AgeVehicle(v);
	CheckIfRoadVehNeedsService(v);

	CheckOrders(v->index, OC_INIT);

	/* update destination */
	if (v->current_order.type == OT_GOTO_STATION) {
		RoadStop *rs;
		uint32 mindist = 0xFFFFFFFF;
		int num;
		RoadStopType type = (v->cargo_type == CT_PASSENGERS) ? RS_BUS : RS_TRUCK;

		typedef struct {
			uint32 dist;
			RoadStop *rs;
		} StopStruct;

		StopStruct *stop, *firststop;

		st = GetStation(v->current_order.station);
		rs = GetPrimaryRoadStop(st, type);
		num = GetNumRoadStops(st, type);

		firststop = stop = malloc(num * sizeof(StopStruct));

		//Current slot has expired
		if ( (v->u.road.slot_age++ <= 0) && (v->u.road.slot != NULL)) {
			ClearSlot(v, v->u.road.slot);
		}

		//We do not have a slot, so make one
		if (v->u.road.slot == NULL) {
			//first we need to find out how far our stations are away.
			if ( rs == NULL )
				goto no_stop;

			DEBUG(ms, 2) ("Multistop: Attempting to obtain a slot for vehicle %d at station %d (0x%x)", v->unitnumber, st->index, st->xy);
			do {
				stop->dist = 0xFFFFFFFF;

				//FIXME This doesn't fully work yet, as it only goes
				//to one tile BEFORE the stop in question and doesn't
				//regard the direction of the exit
				stop->dist = RoadFindPathToStation(v, rs->xy);
				DEBUG(ms, 3) ("Multistop: Distance to stop at 0x%x is %d", rs->xy, stop->dist);
				stop->rs = rs;

				if (stop->dist < mindist) {
					mindist = stop->dist;
				}

				stop++;
				rs = rs->next;
			} while (rs != NULL);

			if (mindist < 120) {	//if we're reasonably close, get us a slot
				int k;
				bubblesort(firststop, num, sizeof(StopStruct), dist_compare);

				stop = firststop;
				for (k = 0; k < num; k++) {
					int i;
					for (i = 0; i < NUM_SLOTS; i++) {
						if ((stop->rs->slot[i] == INVALID_SLOT) && (stop->dist < 120)) {

							//Hooray we found a free slot. Assign it
							DEBUG(ms, 1) ("Multistop: Slot %d at 0x%x assigned to vehicle %d", i, stop->rs->xy, v->unitnumber);
							stop->rs->slot[i] = v->index;
							v->u.road.slot = stop->rs;

							v->dest_tile = stop->rs->xy;
							v->u.road.slot_age = -30;
							v->u.road.slotindex = i;

							goto have_slot;	//jump out of BOTH loops

						}
					}
					stop++;
				}
			}

have_slot:
		//now we couldn't assign a slot for one reason or another.
		//so we just go to the nearest station
			if (v->u.road.slot == NULL) {
				DEBUG(ms, 1) ("Multistop: No free slot found for vehicle %d, going to default station", v->unitnumber);
				v->dest_tile = firststop->rs->xy;
			}
		}

		free(firststop);
		firststop = stop = NULL;
	}

no_stop:
	if (v->vehstatus & VS_STOPPED)
		return;

	cost = RoadVehInfo(v->engine_type)->running_cost * _price.roadveh_running / 364;

	v->profit_this_year -= cost >> 8;

	SET_EXPENSES_TYPE(EXPENSES_ROADVEH_RUN);
	SubtractMoneyFromPlayerFract(v->owner, cost);

	InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
	InvalidateWindowClasses(WC_ROADVEH_LIST);
}

void HandleClickOnRoadVeh(Vehicle *v)
{
	ShowRoadVehViewWindow(v);
}

void RoadVehiclesYearlyLoop(void)
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

