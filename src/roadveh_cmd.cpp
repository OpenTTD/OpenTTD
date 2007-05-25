/* $Id$ */

/** @file roadveh_cmd.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "landscape.h"
#include "road_map.h"
#include "roadveh.h"
#include "station_map.h"
#include "table/strings.h"
#include "map.h"
#include "tile.h"
#include "vehicle.h"
#include "engine.h"
#include "command.h"
#include "station.h"
#include "news.h"
#include "pathfind.h"
#include "npf.h"
#include "player.h"
#include "sound.h"
#include "depot.h"
#include "bridge.h"
#include "tunnel_map.h"
#include "bridge_map.h"
#include "vehicle_gui.h"
#include "newgrf_callbacks.h"
#include "newgrf_engine.h"
#include "newgrf_text.h"
#include "newgrf_sound.h"
#include "yapf/yapf.h"
#include "date.h"
#include "cargotype.h"

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

/** 'Convert' the DiagDirection where a road vehicle should exit to
 * the trackdirs it can use to drive to the exit direction*/
static const TrackdirBits _road_exit_dir_to_incoming_trackdirs[DIAGDIR_END] = {
	TRACKDIR_BIT_LOWER_W | TRACKDIR_BIT_X_SW    | TRACKDIR_BIT_LEFT_S,
	TRACKDIR_BIT_LEFT_N  | TRACKDIR_BIT_UPPER_W | TRACKDIR_BIT_Y_NW,
	TRACKDIR_BIT_RIGHT_N | TRACKDIR_BIT_UPPER_E | TRACKDIR_BIT_X_NE,
	TRACKDIR_BIT_RIGHT_S | TRACKDIR_BIT_LOWER_E | TRACKDIR_BIT_Y_SE
};

/** Converts the exit direction of a depot to trackdir the vehicle is going to drive to */
static const Trackdir _roadveh_depot_exit_trackdir[DIAGDIR_END] = {
	TRACKDIR_X_NE, TRACKDIR_Y_SE, TRACKDIR_X_SW, TRACKDIR_Y_NW
};

int GetRoadVehImage(const Vehicle* v, Direction direction)
{
	int img = v->spritenum;
	int image;

	if (is_custom_sprite(img)) {
		image = GetCustomVehicleSprite(v, direction);
		if (image != 0) return image;
		img = orig_road_vehicle_info[v->engine_type - ROAD_ENGINES_INDEX].image_index;
	}

	image = direction + _roadveh_images[img];
	if (v->cargo_count >= v->cargo_cap / 2) image += _roadveh_full_adder[img];
	return image;
}

void DrawRoadVehEngine(int x, int y, EngineID engine, SpriteID pal)
{
	int spritenum = RoadVehInfo(engine)->image_index;

	if (is_custom_sprite(spritenum)) {
		int sprite = GetCustomVehicleIcon(engine, DIR_W);

		if (sprite != 0) {
			DrawSprite(sprite, pal, x, y);
			return;
		}
		spritenum = orig_road_vehicle_info[engine - ROAD_ENGINES_INDEX].image_index;
	}
	DrawSprite(6 + _roadveh_images[spritenum], pal, x, y);
}

static int32 EstimateRoadVehCost(EngineID engine_type)
{
	return ((_price.roadveh_base >> 3) * GetEngineProperty(engine_type, 0x11, RoadVehInfo(engine_type)->base_cost)) >> 5;
}

/** Build a road vehicle.
 * @param tile tile of depot where road vehicle is built
 * @param flags operation to perform
 * @param p1 bus/truck type being built (engine)
 * @param p2 bit 0 when set, the unitnumber will be 0, otherwise it will be a free number
 */
int32 CmdBuildRoadVeh(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	int32 cost;
	Vehicle *v;
	UnitID unit_num;
	Engine *e;

	if (!IsEngineBuildable(p1, VEH_ROAD, _current_player)) return_cmd_error(STR_ROAD_VEHICLE_NOT_AVAILABLE);

	SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);

	cost = EstimateRoadVehCost(p1);
	if (flags & DC_QUERY_COST) return cost;

	/* The ai_new queries the vehicle cost before building the route,
	 * so we must check against cheaters no sooner than now. --pasky */
	if (!IsTileDepotType(tile, TRANSPORT_ROAD)) return CMD_ERROR;
	if (!IsTileOwner(tile, _current_player)) return CMD_ERROR;

	v = AllocateVehicle();
	if (v == NULL) return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);

	/* find the first free roadveh id */
	unit_num = HASBIT(p2, 0) ? 0 : GetFreeUnitNumber(VEH_ROAD);
	if (unit_num > _patches.max_roadveh)
		return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);

	if (flags & DC_EXEC) {
		int x;
		int y;

		const RoadVehicleInfo *rvi = RoadVehInfo(p1);

		v->unitnumber = unit_num;
		v->direction = INVALID_DIR;
		v->owner = _current_player;

		v->tile = tile;
		x = TileX(tile) * TILE_SIZE + TILE_SIZE / 2;
		y = TileY(tile) * TILE_SIZE + TILE_SIZE / 2;
		v->x_pos = x;
		v->y_pos = y;
		v->z_pos = GetSlopeZ(x, y);

		v->u.road.state = RVSB_IN_DEPOT;
		v->vehstatus = VS_HIDDEN | VS_STOPPED | VS_DEFPAL;

		v->spritenum = rvi->image_index;
		v->cargo_type = rvi->cargo_type;
		v->cargo_subtype = 0;
		v->cargo_cap = rvi->capacity;
//		v->cargo_count = 0;
		v->value = cost;
//		v->day_counter = 0;
//		v->next_order_param = v->next_order = 0;
//		v->load_unload_time_rem = 0;
//		v->progress = 0;

//	v->u.road.unk2 = 0;
//	v->u.road.overtaking = 0;

		v->last_station_visited = INVALID_STATION;
		v->max_speed = rvi->max_speed;
		v->engine_type = (byte)p1;

		v->u.road.roadtype = HASBIT(EngInfo(v->engine_type)->misc_flags, EF_ROAD_TRAM) ? ROADTYPE_TRAM : ROADTYPE_ROAD;
		v->u.road.compatible_roadtypes = RoadTypeToRoadTypes(v->u.road.roadtype);

		e = GetEngine(p1);
		v->reliability = e->reliability;
		v->reliability_spd_dec = e->reliability_spd_dec;
		v->max_age = e->lifelength * 366;
		_new_vehicle_id = v->index;

		v->string_id = STR_SV_ROADVEH_NAME;

		v->service_interval = _patches.servint_roadveh;

		v->date_of_last_service = _date;
		v->build_year = _cur_year;

		v = new (v) RoadVehicle();
		v->cur_image = 0xC15;
		v->random_bits = VehicleRandomBits();

		v->vehicle_flags = 0;
		if (e->flags & ENGINE_EXCLUSIVE_PREVIEW) SETBIT(v->vehicle_flags, VF_BUILT_AS_PROTOTYPE);

		v->cargo_cap = GetVehicleProperty(v, 0x0F, rvi->capacity);

		VehiclePositionChanged(v);

		InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
		RebuildVehicleLists();
		InvalidateWindow(WC_COMPANY, v->owner);
		if (IsLocalPlayer())
			InvalidateAutoreplaceWindow(VEH_ROAD); // updates the replace Road window

		GetPlayer(_current_player)->num_engines[p1]++;
	}

	return cost;
}

/** Start/Stop a road vehicle.
 * @param tile unused
 * @param flags operation to perform
 * @param p1 road vehicle ID to start/stop
 * @param p2 unused
 */
int32 CmdStartStopRoadVeh(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;
	uint16 callback;

	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_ROAD || !CheckOwnership(v->owner)) return CMD_ERROR;

	/* Check if this road veh can be started/stopped. The callback will fail or
	 * return 0xFF if it can. */
	callback = GetVehicleCallback(CBID_VEHICLE_START_STOP_CHECK, 0, 0, v->engine_type, v);
	if (callback != CALLBACK_FAILED && callback != 0xFF) {
		StringID error = GetGRFStringID(GetEngineGRFID(v->engine_type), 0xD000 + callback);
		return_cmd_error(error);
	}

	if (flags & DC_EXEC) {
		if (IsRoadVehInDepotStopped(v)) {
			DeleteVehicleNews(p1, STR_9016_ROAD_VEHICLE_IS_WAITING);
		}

		v->vehstatus ^= VS_STOPPED;
		v->cur_speed = 0;
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
	}

	return 0;
}

void ClearSlot(Vehicle *v)
{
	RoadStop *rs = v->u.road.slot;
	if (v->u.road.slot == NULL) return;

	v->u.road.slot = NULL;
	v->u.road.slot_age = 0;

	assert(rs->num_vehicles != 0);
	rs->num_vehicles--;

	DEBUG(ms, 3, "Clearing slot at 0x%X", rs->xy);
}

/** Sell a road vehicle.
 * @param tile unused
 * @param flags operation to perform
 * @param p1 vehicle ID to be sold
 * @param p2 unused
 */
int32 CmdSellRoadVeh(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;

	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_ROAD || !CheckOwnership(v->owner)) return CMD_ERROR;

	SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);

	if (!IsRoadVehInDepotStopped(v)) {
		return_cmd_error(STR_9013_MUST_BE_STOPPED_INSIDE);
	}

	if (flags & DC_EXEC) {
		// Invalidate depot
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
		RebuildVehicleLists();
		InvalidateWindow(WC_COMPANY, v->owner);
		DeleteWindowById(WC_VEHICLE_VIEW, v->index);
		DeleteDepotHighlightOfVehicle(v);
		DeleteVehicle(v);
	}

	return -(int32)v->value;
}

struct RoadFindDepotData {
	uint best_length;
	TileIndex tile;
	OwnerByte owner;
};

static const DiagDirection _road_pf_directions[] = {
	DIAGDIR_NE, DIAGDIR_SE, DIAGDIR_NE, DIAGDIR_SE, DIAGDIR_SW, DIAGDIR_SE, INVALID_DIAGDIR, INVALID_DIAGDIR,
	DIAGDIR_SW, DIAGDIR_NW, DIAGDIR_NW, DIAGDIR_SW, DIAGDIR_NW, DIAGDIR_NE, INVALID_DIAGDIR, INVALID_DIAGDIR
};

static bool EnumRoadSignalFindDepot(TileIndex tile, void* data, Trackdir trackdir, uint length, byte* state)
{
	RoadFindDepotData* rfdd = (RoadFindDepotData*)data;

	tile += TileOffsByDiagDir(_road_pf_directions[trackdir]);

	if (IsTileType(tile, MP_STREET) &&
			GetRoadTileType(tile) == ROAD_TILE_DEPOT &&
			IsTileOwner(tile, rfdd->owner) &&
			length < rfdd->best_length) {
		rfdd->best_length = length;
		rfdd->tile = tile;
	}
	return false;
}

static const Depot* FindClosestRoadDepot(const Vehicle* v)
{
	TileIndex tile = v->tile;

	if (_patches.yapf.road_use_yapf) {
		Depot* ret = YapfFindNearestRoadDepot(v);
		return ret;
	} else if (_patches.new_pathfinding_all) {
		NPFFoundTargetData ftd;
		/* See where we are now */
		Trackdir trackdir = GetVehicleTrackdir(v);

		ftd = NPFRouteToDepotBreadthFirst(v->tile, trackdir, TRANSPORT_ROAD, v->u.road.compatible_roadtypes, v->owner, INVALID_RAILTYPE);
		if (ftd.best_bird_dist == 0) {
			return GetDepotByTile(ftd.node.tile); /* Target found */
		} else {
			return NULL; /* Target not found */
		}
		/* We do not search in two directions here, why should we? We can't reverse right now can we? */
	} else {
		RoadFindDepotData rfdd;

		rfdd.owner = v->owner;
		rfdd.best_length = (uint)-1;

		/* search in all directions */
		for (DiagDirection i = DIAGDIR_BEGIN; i != DIAGDIR_END; i++) {
			FollowTrack(tile, 0x2000 | TRANSPORT_ROAD, v->u.road.compatible_roadtypes, i, EnumRoadSignalFindDepot, NULL, &rfdd);
		}

		if (rfdd.best_length == (uint)-1) return NULL;

		return GetDepotByTile(rfdd.tile);
	}
}

/** Send a road vehicle to the depot.
 * @param tile unused
 * @param flags operation to perform
 * @param p1 vehicle ID to send to the depot
 * @param p2 various bitmasked elements
 * - p2 bit 0-3 - DEPOT_ flags (see vehicle.h)
 * - p2 bit 8-10 - VLW flag (for mass goto depot)
 */
int32 CmdSendRoadVehToDepot(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;
	const Depot *dep;

	if (p2 & DEPOT_MASS_SEND) {
		/* Mass goto depot requested */
		if (!ValidVLWFlags(p2 & VLW_MASK)) return CMD_ERROR;
		return SendAllVehiclesToDepot(VEH_ROAD, flags, p2 & DEPOT_SERVICE, _current_player, (p2 & VLW_MASK), p1);
	}

	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_ROAD || !CheckOwnership(v->owner)) return CMD_ERROR;

	if (v->vehstatus & VS_CRASHED) return CMD_ERROR;

	if (IsRoadVehInDepot(v)) return CMD_ERROR;

	/* If the current orders are already goto-depot */
	if (v->current_order.type == OT_GOTO_DEPOT) {
		if (!!(p2 & DEPOT_SERVICE) == HASBIT(v->current_order.flags, OFB_HALT_IN_DEPOT)) {
			/* We called with a different DEPOT_SERVICE setting.
			 * Now we change the setting to apply the new one and let the vehicle head for the same depot.
			 * Note: the if is (true for requesting service == true for ordered to stop in depot) */
			if (flags & DC_EXEC) {
				TOGGLEBIT(v->current_order.flags, OFB_HALT_IN_DEPOT);
				InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
			}
			return 0;
		}

		if (p2 & DEPOT_DONT_CANCEL) return CMD_ERROR; // Requested no cancelation of depot orders
		if (flags & DC_EXEC) {
			/* If the orders to 'goto depot' are in the orders list (forced servicing),
			 * then skip to the next order; effectively cancelling this forced service */
			if (HASBIT(v->current_order.flags, OFB_PART_OF_ORDERS))
				v->cur_order_index++;

			v->current_order.type = OT_DUMMY;
			v->current_order.flags = 0;
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
		}
		return 0;
	}

	dep = FindClosestRoadDepot(v);
	if (dep == NULL) return_cmd_error(STR_9019_UNABLE_TO_FIND_LOCAL_DEPOT);

	if (flags & DC_EXEC) {
		if (v->current_order.type == OT_LOADING) v->LeaveStation();

		ClearSlot(v);
		v->current_order.type = OT_GOTO_DEPOT;
		v->current_order.flags = OF_NON_STOP;
		if (!(p2 & DEPOT_SERVICE)) SETBIT(v->current_order.flags, OFB_HALT_IN_DEPOT);
		v->current_order.refit_cargo = CT_INVALID;
		v->current_order.dest = dep->index;
		v->dest_tile = dep->xy;
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
	}

	return 0;
}

/** Turn a roadvehicle around.
 * @param tile unused
 * @param flags operation to perform
 * @param p1 vehicle ID to turn
 * @param p2 unused
 */
int32 CmdTurnRoadVeh(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;

	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_ROAD || !CheckOwnership(v->owner)) return CMD_ERROR;

	if (v->vehstatus & VS_STOPPED ||
			v->u.road.roadtype == ROADTYPE_TRAM ||
			v->u.road.crashed_ctr != 0 ||
			v->breakdown_ctr != 0 ||
			v->u.road.overtaking != 0 ||
			v->u.road.state == RVSB_WORMHOLE ||
			IsRoadVehInDepot(v) ||
			v->cur_speed < 5) {
		return CMD_ERROR;
	}

	if (IsTunnelTile(v->tile) && DirToDiagDir(v->direction) == GetTunnelDirection(v->tile)) return CMD_ERROR;
	if (IsBridgeTile(v->tile) && DirToDiagDir(v->direction) == GetBridgeRampDirection(v->tile)) return CMD_ERROR;

	if (flags & DC_EXEC) v->u.road.reverse_ctr = 180;

	return 0;
}


void RoadVehicle::MarkDirty()
{
	this->cur_image = GetRoadVehImage(this, this->direction);
	MarkAllViewportsDirty(this->left_coord, this->top_coord, this->right_coord + 1, this->bottom_coord + 1);
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
	this->sprite_width  = GB(x, 16, 8);
	this->sprite_height = GB(x, 24, 8);
	this->z_height      = 6;
}

static void ClearCrashedStation(Vehicle *v)
{
	RoadStop *rs = GetRoadStopByTile(v->tile, GetRoadStopType(v->tile));

	/* Mark the station entrance as not busy */
	rs->SetEntranceBusy(false);

	/* Free the parking bay */
	rs->FreeBay(HASBIT(v->u.road.state, RVS_USING_SECOND_BAY));
}

static void RoadVehDelete(Vehicle *v)
{
	DeleteWindowById(WC_VEHICLE_VIEW, v->index);

	RebuildVehicleLists();
	InvalidateWindow(WC_COMPANY, v->owner);

	if (IsTileType(v->tile, MP_STATION)) ClearCrashedStation(v);

	BeginVehicleMove(v);
	EndVehicleMove(v);

	DeleteVehicle(v);
}

static byte SetRoadVehPosition(Vehicle *v, int x, int y)
{
	byte new_z, old_z;

	/* need this hint so it returns the right z coordinate on bridges. */
	v->x_pos = x;
	v->y_pos = y;
	new_z = GetSlopeZ(x, y);

	old_z = v->z_pos;
	v->z_pos = new_z;

	VehiclePositionChanged(v);
	EndVehicleMove(v);
	return old_z;
}

static void RoadVehSetRandomDirection(Vehicle *v)
{
	static const DirDiff delta[] = {
		DIRDIFF_45LEFT, DIRDIFF_SAME, DIRDIFF_SAME, DIRDIFF_45RIGHT
	};

	uint32 r = Random();

	v->direction = ChangeDir(v->direction, delta[r & 3]);
	BeginVehicleMove(v);
	v->UpdateDeltaXY(v->direction);
	v->cur_image = GetRoadVehImage(v, v->direction);
	SetRoadVehPosition(v, v->x_pos, v->y_pos);
}

static void RoadVehIsCrashed(Vehicle *v)
{
	v->u.road.crashed_ctr++;
	if (v->u.road.crashed_ctr == 2) {
		CreateEffectVehicleRel(v, 4, 4, 8, EV_EXPLOSION_LARGE);
	} else if (v->u.road.crashed_ctr <= 45) {
		if ((v->tick_counter & 7) == 0) RoadVehSetRandomDirection(v);
	} else if (v->u.road.crashed_ctr >= 2220) {
		RoadVehDelete(v);
	}
}

static void* EnumCheckRoadVehCrashTrain(Vehicle* v, void* data)
{
	const Vehicle* u = (Vehicle*)data;

	return
		v->type == VEH_TRAIN &&
		myabs(v->z_pos - u->z_pos) <= 6 &&
		myabs(v->x_pos - u->x_pos) <= 4 &&
		myabs(v->y_pos - u->y_pos) <= 4 ?
			v : NULL;
}

static void RoadVehCrash(Vehicle *v)
{
	uint16 pass;

	v->u.road.crashed_ctr++;
	v->vehstatus |= VS_CRASHED;
	ClearSlot(v);

	InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);

	pass = 1;
	if (IsCargoInClass(v->cargo_type, CC_PASSENGERS)) pass += v->cargo_count;
	v->cargo_count = 0;

	SetDParam(0, pass);
	AddNewsItem(
		(pass == 1) ?
			STR_9031_ROAD_VEHICLE_CRASH_DRIVER : STR_9032_ROAD_VEHICLE_CRASH_DIE,
		NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_VEHICLE, NT_ACCIDENT, 0),
		v->index,
		0
	);

	ModifyStationRatingAround(v->tile, v->owner, -160, 22);
	SndPlayVehicleFx(SND_12_EXPLOSION, v);
}

static void RoadVehCheckTrainCrash(Vehicle *v)
{
	TileIndex tile;

	if (v->u.road.state == RVSB_WORMHOLE) return;

	tile = v->tile;

	if (!IsLevelCrossingTile(tile)) return;

	if (VehicleFromPos(tile, v, EnumCheckRoadVehCrashTrain) != NULL)
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

		if (!PlayVehicleSound(v, VSE_BREAKDOWN)) {
			SndPlayVehicleFx((_opt.landscape != LT_TOYLAND) ?
				SND_0F_VEHICLE_BREAKDOWN : SND_35_COMEDY_BREAKDOWN, v);
		}

		if (!(v->vehstatus & VS_HIDDEN)) {
			Vehicle *u = CreateEffectVehicleRel(v, 4, 4, 5, EV_BREAKDOWN_SMOKE);
			if (u != NULL) u->u.special.unk0 = v->breakdown_delay * 2;
		}
	}

	if ((v->tick_counter & 1) == 0) {
		if (--v->breakdown_delay == 0) {
			v->breakdown_ctr = 0;
			InvalidateWindow(WC_VEHICLE_VIEW, v->index);
		}
	}
}

static void ProcessRoadVehOrder(Vehicle *v)
{
	const Order *order;

	switch (v->current_order.type) {
		case OT_GOTO_DEPOT:
			/* Let a depot order in the orderlist interrupt. */
			if (!(v->current_order.flags & OF_PART_OF_ORDERS)) return;
			if (v->current_order.flags & OF_SERVICE_IF_NEEDED &&
					!VehicleNeedsService(v)) {
				v->cur_order_index++;
			}
			break;

		case OT_LOADING:
		case OT_LEAVESTATION:
			return;

		default: break;
	}

	if (v->cur_order_index >= v->num_orders) v->cur_order_index = 0;

	order = GetVehicleOrder(v, v->cur_order_index);

	if (order == NULL) {
		v->current_order.Free();
		v->dest_tile = 0;
		ClearSlot(v);
		return;
	}

	if (order->type  == v->current_order.type &&
			order->flags == v->current_order.flags &&
			order->dest  == v->current_order.dest) {
		return;
	}

	v->current_order = *order;

	switch (order->type) {
		case OT_GOTO_STATION: {
			const RoadStop* rs;

			if (order->dest == v->last_station_visited) {
				v->last_station_visited = INVALID_STATION;
			}

			rs = GetStation(order->dest)->GetPrimaryRoadStop(
				IsCargoInClass(v->cargo_type, CC_PASSENGERS) ? RoadStop::BUS : RoadStop::TRUCK
			);

			if (rs != NULL) {
				TileIndex dest = rs->xy;
				uint mindist = DistanceManhattan(v->tile, rs->xy);

				for (rs = rs->next; rs != NULL; rs = rs->next) {
					uint dist = DistanceManhattan(v->tile, rs->xy);

					if (dist < mindist) {
						mindist = dist;
						dest = rs->xy;
					}
				}
				v->dest_tile = dest;
			} else {
				/* There is no stop left at the station, so don't even TRY to go there */
				v->cur_order_index++;
				v->dest_tile = 0;
			}
			break;
		}

		case OT_GOTO_DEPOT:
			v->dest_tile = GetDepot(order->dest)->xy;
			break;

		default:
			v->dest_tile = 0;
			break;
	}

	InvalidateVehicleOrder(v);
}

static void StartRoadVehSound(const Vehicle* v)
{
	if (!PlayVehicleSound(v, VSE_START)) {
		SoundFx s = RoadVehInfo(v->engine_type)->sfx;
		if (s == SND_19_BUS_START_PULL_AWAY && (v->tick_counter & 3) == 0)
			s = SND_1A_BUS_START_PULL_AWAY_WITH_HORN;
		SndPlayVehicleFx(s, v);
	}
}

struct RoadVehFindData {
	int x;
	int y;
	const Vehicle* veh;
	Direction dir;
};

static void* EnumCheckRoadVehClose(Vehicle *v, void* data)
{
	static const int8 dist_x[] = { -4, -8, -4, -1, 4, 8, 4, 1 };
	static const int8 dist_y[] = { -4, -1, 4, 8, 4, 1, -4, -8 };

	const RoadVehFindData* rvf = (RoadVehFindData*)data;

	short x_diff = v->x_pos - rvf->x;
	short y_diff = v->y_pos - rvf->y;

	return
		rvf->veh != v &&
		v->type == VEH_ROAD &&
		!IsRoadVehInDepot(v) &&
		myabs(v->z_pos - rvf->veh->z_pos) < 6 &&
		v->direction == rvf->dir &&
		(dist_x[v->direction] >= 0 || (x_diff > dist_x[v->direction] && x_diff <= 0)) &&
		(dist_x[v->direction] <= 0 || (x_diff < dist_x[v->direction] && x_diff >= 0)) &&
		(dist_y[v->direction] >= 0 || (y_diff > dist_y[v->direction] && y_diff <= 0)) &&
		(dist_y[v->direction] <= 0 || (y_diff < dist_y[v->direction] && y_diff >= 0)) ?
			v : NULL;
}

static Vehicle* RoadVehFindCloseTo(Vehicle* v, int x, int y, Direction dir)
{
	RoadVehFindData rvf;
	Vehicle *u;

	if (v->u.road.reverse_ctr != 0) return NULL;

	rvf.x = x;
	rvf.y = y;
	rvf.dir = dir;
	rvf.veh = v;
	u = (Vehicle*)VehicleFromPos(TileVirtXY(x, y), &rvf, EnumCheckRoadVehClose);

	/* This code protects a roadvehicle from being blocked for ever
	 * If more than 1480 / 74 days a road vehicle is blocked, it will
	 * drive just through it. The ultimate backup-code of TTD.
	 * It can be disabled. */
	if (u == NULL) {
		v->u.road.blocked_ctr = 0;
		return NULL;
	}

	if (++v->u.road.blocked_ctr > 1480) return NULL;

	return u;
}

static void RoadVehArrivesAt(const Vehicle* v, Station* st)
{
	if (IsCargoInClass(v->cargo_type, CC_PASSENGERS)) {
		/* Check if station was ever visited before */
		if (!(st->had_vehicle_of_type & HVOT_BUS)) {
			uint32 flags;

			st->had_vehicle_of_type |= HVOT_BUS;
			SetDParam(0, st->index);
			flags = (v->owner == _local_player) ? NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_VEHICLE, NT_ARRIVAL_PLAYER, 0) : NEWS_FLAGS(NM_THIN, NF_VIEWPORT|NF_VEHICLE, NT_ARRIVAL_OTHER, 0);
			AddNewsItem(
				v->u.road.roadtype == ROADTYPE_ROAD ? STR_902F_CITIZENS_CELEBRATE_FIRST : STR_902F_CITIZENS_CELEBRATE_FIRST_TRAM,
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
				v->u.road.roadtype == ROADTYPE_ROAD ? STR_9030_CITIZENS_CELEBRATE_FIRST : STR_9030_CITIZENS_CELEBRATE_FIRST_TRAM,
				flags,
				v->index,
				0
			);
		}
	}
}

static bool RoadVehAccelerate(Vehicle *v)
{
	uint spd = v->cur_speed + 1 + (v->u.road.overtaking != 0 ? 1 : 0);
	byte t;

	/* Clamp */
	spd = min(spd, v->max_speed);
	if (v->u.road.state == RVSB_WORMHOLE && !(v->vehstatus & VS_HIDDEN)) {
		spd = min(spd, GetBridge(GetBridgeType(v->tile))->speed * 2);
	}

	/* updates statusbar only if speed have changed to save CPU time */
	if (spd != v->cur_speed) {
		v->cur_speed = spd;
		if (_patches.vehicle_speed) {
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
		}
	}

	/* Decrease somewhat when turning */
	if (!(v->direction & 1)) spd = spd * 3 >> 2;

	if (spd == 0) return false;

	if ((byte)++spd == 0) return true;

	v->progress = (t = v->progress) - (byte)spd;

	return (t < v->progress);
}

static Direction RoadVehGetNewDirection(const Vehicle* v, int x, int y)
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

static Direction RoadVehGetSlidingDirection(const Vehicle* v, int x, int y)
{
	Direction new_dir = RoadVehGetNewDirection(v, x, y);
	Direction old_dir = v->direction;
	DirDiff delta;

	if (new_dir == old_dir) return old_dir;
	delta = (DirDifference(new_dir, old_dir) > DIRDIFF_REVERSE ? DIRDIFF_45LEFT : DIRDIFF_45RIGHT);
	return ChangeDir(old_dir, delta);
}

struct OvertakeData {
	const Vehicle* u;
	const Vehicle* v;
	TileIndex tile;
	byte tilebits;
};

static void* EnumFindVehToOvertake(Vehicle* v, void* data)
{
	const OvertakeData* od = (OvertakeData*)data;

	return
		v->tile == od->tile && v->type == VEH_ROAD && v != od->u && v != od->v ?
			v : NULL;
}

static bool FindRoadVehToOvertake(OvertakeData *od)
{
	uint32 bits;

	bits = GetTileTrackStatus(od->tile, TRANSPORT_ROAD, od->v->u.road.compatible_roadtypes) & 0x3F;

	if (!(od->tilebits & bits) || (bits & 0x3C) || (bits & 0x3F3F0000))
		return true;
	return VehicleFromPos(od->tile, od, EnumFindVehToOvertake) != NULL;
}

static void RoadVehCheckOvertake(Vehicle *v, Vehicle *u)
{
	OvertakeData od;
	byte tt;

	od.v = v;
	od.u = u;

	if (u->max_speed >= v->max_speed &&
			!(u->vehstatus & VS_STOPPED) &&
			u->cur_speed != 0) {
		return;
	}

	/* Trams can't overtake other trams */
	if (v->u.road.roadtype == ROADTYPE_TRAM) return;

	if (v->direction != u->direction || !(v->direction & 1)) return;

	/* Check if vehicle is in a road stop, depot, tunnel or bridge or not on a straight road */
	if (v->u.road.state >= RVSB_IN_ROAD_STOP || !IsStraightRoadTrackdir((Trackdir)(v->u.road.state & RVSB_TRACKDIR_MASK))) return;

	tt = GetTileTrackStatus(v->tile, TRANSPORT_ROAD, v->u.road.compatible_roadtypes) & 0x3F;
	if ((tt & 3) == 0) return;
	if ((tt & 0x3C) != 0) return;

	if (tt == 3) tt = (v->direction & 2) ? 2 : 1;
	od.tilebits = tt;

	od.tile = v->tile;
	if (FindRoadVehToOvertake(&od)) return;

	od.tile = v->tile + TileOffsByDiagDir(DirToDiagDir(v->direction));
	if (FindRoadVehToOvertake(&od)) return;

	if (od.u->cur_speed == 0 || od.u->vehstatus& VS_STOPPED) {
		v->u.road.overtaking_ctr = 0x11;
		v->u.road.overtaking = 0x10;
	} else {
//		if (FindRoadVehToOvertake(&od)) return;
		v->u.road.overtaking_ctr = 0;
		v->u.road.overtaking = 0x10;
	}
}

static void RoadZPosAffectSpeed(Vehicle *v, byte old_z)
{
	if (old_z == v->z_pos) return;

	if (old_z < v->z_pos) {
		v->cur_speed = v->cur_speed * 232 / 256; // slow down by ~10%
	} else {
		uint16 spd = v->cur_speed + 2;
		if (spd <= v->max_speed) v->cur_speed = spd;
	}
}

static int PickRandomBit(uint bits)
{
	uint num = 0;
	uint b = bits;
	uint i;

	do {
		if (b & 1) num++;
	} while (b >>= 1);

	num = RandomRange(num);

	for (i = 0; !(bits & 1) || (int)--num >= 0; bits >>= 1, i++) {}
	return i;
}

struct FindRoadToChooseData {
	TileIndex dest;
	uint maxtracklen;
	uint mindist;
};

static bool EnumRoadTrackFindDist(TileIndex tile, void* data, Trackdir trackdir, uint length, byte* state)
{
	FindRoadToChooseData* frd = (FindRoadToChooseData*)data;
	uint dist = DistanceManhattan(tile, frd->dest);

	if (dist <= frd->mindist) {
		if (dist != frd->mindist || length < frd->maxtracklen) {
			frd->maxtracklen = length;
		}
		frd->mindist = dist;
	}
	return false;
}

static inline NPFFoundTargetData PerfNPFRouteToStationOrTile(TileIndex tile, Trackdir trackdir, NPFFindStationOrTileData* target, TransportType type, uint sub_type, Owner owner, RailTypeMask railtypes)
{

	void* perf = NpfBeginInterval();
	NPFFoundTargetData ret = NPFRouteToStationOrTile(tile, trackdir, target, type, sub_type, owner, railtypes);
	int t = NpfEndInterval(perf);
	DEBUG(yapf, 4, "[NPFR] %d us - %d rounds - %d open - %d closed -- ", t, 0, _aystar_stats_open_size, _aystar_stats_closed_size);
	return ret;
}

/**
 * Returns direction to for a road vehicle to take or
 * INVALID_TRACKDIR if the direction is currently blocked
 * @param v        the Vehicle to do the pathfinding for
 * @param tile     the where to start the pathfinding
 * @param enterdir the direction the vehicle enters the tile from
 * @return the Trackdir to take
 */
static Trackdir RoadFindPathToDest(Vehicle* v, TileIndex tile, DiagDirection enterdir)
{
#define return_track(x) { best_track = (Trackdir)x; goto found_best_track; }

	TileIndex desttile;
	FindRoadToChooseData frd;
	Trackdir best_track;

	uint32 r  = GetTileTrackStatus(tile, TRANSPORT_ROAD, v->u.road.compatible_roadtypes);
	TrackdirBits signal    = (TrackdirBits)GB(r, 16, 16);
	TrackdirBits trackdirs = (TrackdirBits)GB(r,  0, 16);

	if (IsTileType(tile, MP_STREET)) {
		if (GetRoadTileType(tile) == ROAD_TILE_DEPOT && (!IsTileOwner(tile, v->owner) || GetRoadDepotDirection(tile) == enterdir || (GetRoadTypes(tile) & v->u.road.compatible_roadtypes) == 0)) {
			/* Road depot owned by another player or with the wrong orientation */
			trackdirs = TRACKDIR_BIT_NONE;
		}
	} else if (IsTileType(tile, MP_STATION) && IsStandardRoadStopTile(tile)) {
		/* Standard road stop (drive-through stops are treated as normal road) */
		if (!IsTileOwner(tile, v->owner) || GetRoadStopDir(tile) == enterdir) {
			/* different station owner or wrong orientation */
			trackdirs = TRACKDIR_BIT_NONE;
		} else {
			/* Our station */
			RoadStop::Type rstype = IsCargoInClass(v->cargo_type, CC_PASSENGERS) ? RoadStop::BUS : RoadStop::TRUCK;

			if (GetRoadStopType(tile) != rstype) {
				/* Wrong station type */
				trackdirs = TRACKDIR_BIT_NONE;
			} else {
				/* Proper station type, check if there is free loading bay */
				if (!_patches.roadveh_queue &&  IsStandardRoadStopTile(tile) &&
						!GetRoadStopByTile(tile, rstype)->HasFreeBay()) {
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

	if (v->u.road.reverse_ctr != 0) {
		/* What happens here?? */
		v->u.road.reverse_ctr = 0;
		if (v->tile != tile) {
			return_track(_road_reverse_table[enterdir]);
		}
	}

	desttile = v->dest_tile;
	if (desttile == 0) {
		/* We've got no destination, pick a random track */
		return_track(PickRandomBit(trackdirs));
	}

	/* Only one track to choose between? */
	if (!(KillFirstBit2x64(trackdirs))) {
		return_track(FindFirstBit2x64(trackdirs));
	}

	if (_patches.yapf.road_use_yapf) {
		Trackdir trackdir = YapfChooseRoadTrack(v, tile, enterdir);
		if (trackdir != INVALID_TRACKDIR) return_track(trackdir);
		return_track(PickRandomBit(trackdirs));
	} else if (_patches.new_pathfinding_all) {
		NPFFindStationOrTileData fstd;
		NPFFoundTargetData ftd;
		Trackdir trackdir;

		NPFFillWithOrderData(&fstd, v);
		trackdir = DiagdirToDiagTrackdir(enterdir);
		//debug("Finding path. Enterdir: %d, Trackdir: %d", enterdir, trackdir);

		ftd = PerfNPFRouteToStationOrTile(tile - TileOffsByDiagDir(enterdir), trackdir, &fstd, TRANSPORT_ROAD, v->u.road.compatible_roadtypes, v->owner, INVALID_RAILTYPE);
		if (ftd.best_trackdir == INVALID_TRACKDIR) {
			/* We are already at our target. Just do something
			 * @todo: maybe display error?
			 * @todo: go straight ahead if possible? */
			return_track(FindFirstBit2x64(trackdirs));
		} else {
			/* If ftd.best_bird_dist is 0, we found our target and ftd.best_trackdir contains
			the direction we need to take to get there, if ftd.best_bird_dist is not 0,
			we did not find our target, but ftd.best_trackdir contains the direction leading
			to the tile closest to our target. */
			return_track(ftd.best_trackdir);
		}
	} else {
		DiagDirection dir;

		if (IsTileType(desttile, MP_STREET)) {
			if (GetRoadTileType(desttile) == ROAD_TILE_DEPOT) {
				dir = GetRoadDepotDirection(desttile);
				goto do_it;
			}
		} else if (IsTileType(desttile, MP_STATION)) {
			/* For drive-through stops we can head for the actual station tile */
			if (IsStandardRoadStopTile(desttile)) {
				dir = GetRoadStopDir(desttile);
do_it:;
				/* When we are heading for a depot or station, we just
				 * pretend we are heading for the tile in front, we'll
				 * see from there */
				desttile += TileOffsByDiagDir(dir);
				if (desttile == tile && trackdirs & _road_exit_dir_to_incoming_trackdirs[dir]) {
					/* If we are already in front of the
					 * station/depot and we can get in from here,
					 * we enter */
					return_track(FindFirstBit2x64(trackdirs & _road_exit_dir_to_incoming_trackdirs[dir]));
				}
			}
		}
		/* Do some pathfinding */
		frd.dest = desttile;

		best_track = INVALID_TRACKDIR;
		uint best_dist = (uint)-1;
		uint best_maxlen = (uint)-1;
		uint bitmask = (uint)trackdirs;
		for (int i = 0; bitmask != 0; bitmask >>= 1, i++) {
			if (HASBIT(bitmask, 0)) {
				if (best_track == INVALID_TRACKDIR) best_track = (Trackdir)i; // in case we don't find the path, just pick a track
				frd.maxtracklen = (uint)-1;
				frd.mindist = (uint)-1;
				FollowTrack(tile, 0x2000 | TRANSPORT_ROAD, v->u.road.compatible_roadtypes, _road_pf_directions[i], EnumRoadTrackFindDist, NULL, &frd);

				if (frd.mindist < best_dist || (frd.mindist == best_dist && frd.maxtracklen < best_maxlen)) {
					best_dist = frd.mindist;
					best_maxlen = frd.maxtracklen;
					best_track = (Trackdir)i;
				}
			}
		}
	}

found_best_track:;

	if (HASBIT(signal, best_track)) return INVALID_TRACKDIR;

	return best_track;
}

static uint RoadFindPathToStop(const Vehicle *v, TileIndex tile)
{
	uint dist;
	if (_patches.yapf.road_use_yapf) {
		/* use YAPF */
		dist = YapfRoadVehDistanceToTile(v, tile);
	} else {
		/* use NPF */
		NPFFindStationOrTileData fstd;
		Trackdir trackdir = GetVehicleTrackdir(v);
		assert(trackdir != INVALID_TRACKDIR);

		fstd.dest_coords = tile;
		fstd.station_index = INVALID_STATION; // indicates that the destination is a tile, not a station

		dist = NPFRouteToStationOrTile(v->tile, trackdir, &fstd, TRANSPORT_ROAD, v->u.road.compatible_roadtypes, v->owner, INVALID_RAILTYPE).best_path_dist;
		/* change units from NPF_TILE_LENGTH to # of tiles */
		if (dist != UINT_MAX)
			dist = (dist + NPF_TILE_LENGTH - 1) / NPF_TILE_LENGTH;
	}
	return dist;
}

enum {
	RDE_NEXT_TILE = 0x80,
	RDE_TURNED    = 0x40,

	/* Start frames for when a vehicle enters a tile/changes its state.
	 * The start frame is different for vehicles that turned around or
	 * are leaving the depot as the do not start at the edge of the tile */
	RVC_DEFAULT_START_FRAME      = 0,
	RVC_TURN_AROUND_START_FRAME  = 1,
	RVC_DEPOT_START_FRAME        = 6,
	/* Stop frame for a vehicle in a drive-through stop */
	RVC_DRIVE_THROUGH_STOP_FRAME = 7
};

struct RoadDriveEntry {
	byte x, y;
};

#include "table/roadveh.h"

static const byte _road_veh_data_1[] = {
	20, 20, 16, 16, 0, 0, 0, 0,
	19, 19, 15, 15, 0, 0, 0, 0,
	16, 16, 12, 12, 0, 0, 0, 0,
	15, 15, 11, 11
};

static void RoadVehController(Vehicle *v)
{
	Direction new_dir;
	Direction old_dir;
	RoadDriveEntry rd;
	int x,y;
	uint32 r;

	/* decrease counters */
	v->tick_counter++;
	if (v->u.road.reverse_ctr != 0) v->u.road.reverse_ctr--;

	/* handle crashed */
	if (v->u.road.crashed_ctr != 0) {
		RoadVehIsCrashed(v);
		return;
	}

	RoadVehCheckTrainCrash(v);

	/* road vehicle has broken down? */
	if (v->breakdown_ctr != 0) {
		if (v->breakdown_ctr <= 2) {
			HandleBrokenRoadVeh(v);
			return;
		}
		v->breakdown_ctr--;
	}

	if (v->vehstatus & VS_STOPPED) return;

	ProcessRoadVehOrder(v);
	v->HandleLoading();

	if (v->current_order.type == OT_LOADING) return;

	if (IsRoadVehInDepot(v)) {
		/* Vehicle is about to leave a depot */
		DiagDirection dir;
		const RoadDriveEntry* rdp;
		Trackdir tdir;

		v->cur_speed = 0;

		dir = GetRoadDepotDirection(v->tile);
		v->direction = DiagDirToDir(dir);

		tdir = _roadveh_depot_exit_trackdir[dir];
		rdp = _road_drive_data[v->u.road.roadtype][(_opt.road_side << RVS_DRIVE_SIDE) + tdir];

		x = TileX(v->tile) * TILE_SIZE + (rdp[RVC_DEPOT_START_FRAME].x & 0xF);
		y = TileY(v->tile) * TILE_SIZE + (rdp[RVC_DEPOT_START_FRAME].y & 0xF);

		if (RoadVehFindCloseTo(v, x, y, v->direction) != NULL) return;

		VehicleServiceInDepot(v);

		StartRoadVehSound(v);

		BeginVehicleMove(v);

		v->vehstatus &= ~VS_HIDDEN;
		v->u.road.state = tdir;
		v->u.road.frame = RVC_DEPOT_START_FRAME;

		v->cur_image = GetRoadVehImage(v, v->direction);
		v->UpdateDeltaXY(v->direction);
		SetRoadVehPosition(v,x,y);

		InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
		return;
	}

	/* Check if vehicle needs to proceed, return if it doesn't */
	if (!RoadVehAccelerate(v)) return;

	if (v->u.road.overtaking != 0)  {
		if (++v->u.road.overtaking_ctr >= 35)
			/* If overtaking just aborts at a random moment, we can have a out-of-bound problem,
			 *  if the vehicle started a corner. To protect that, only allow an abort of
			 *  overtake if we are on straight roads */
			if (v->u.road.state < RVSB_IN_ROAD_STOP && IsStraightRoadTrackdir((Trackdir)v->u.road.state)) {
				v->u.road.overtaking = 0;
			}
	}

	/* Save old vehicle position to use at end of move to set viewport area dirty */
	BeginVehicleMove(v);

	if (v->u.road.state == RVSB_WORMHOLE) {
		/* Vehicle is entering a depot or is on a bridge or in a tunnel */
		GetNewVehiclePosResult gp = GetNewVehiclePos(v);

		const Vehicle *u = RoadVehFindCloseTo(v, gp.x, gp.y, v->direction);
		if (u != NULL && u->cur_speed < v->cur_speed) {
			v->cur_speed = u->cur_speed;
			return;
		}

		if ((IsTunnelTile(gp.new_tile) || IsBridgeTile(gp.new_tile)) && HASBIT(VehicleEnterTile(v, gp.new_tile, gp.x, gp.y), VETS_ENTERED_WORMHOLE)) {
			/* Vehicle has just entered a bridge or tunnel */
			v->cur_image = GetRoadVehImage(v, v->direction);
			v->UpdateDeltaXY(v->direction);
			SetRoadVehPosition(v,gp.x,gp.y);
			return;
		}

		v->x_pos = gp.x;
		v->y_pos = gp.y;
		VehiclePositionChanged(v);
		if (!(v->vehstatus & VS_HIDDEN)) EndVehicleMove(v);
		return;
	}

	/* Get move position data for next frame.
	 * For a drive-through road stop use 'straight road' move data.
	 * In this case v->u.road.state is masked to give the road stop entry direction. */
	rd = _road_drive_data[v->u.road.roadtype][(
		(HASBIT(v->u.road.state, RVS_IN_DT_ROAD_STOP) ? v->u.road.state & RVSB_ROAD_STOP_TRACKDIR_MASK : v->u.road.state) +
		(_opt.road_side << RVS_DRIVE_SIDE)) ^ v->u.road.overtaking][v->u.road.frame + 1];

	if (rd.x & RDE_NEXT_TILE) {
		TileIndex tile = v->tile + TileOffsByDiagDir(rd.x & 3);
		Trackdir dir = RoadFindPathToDest(v, tile, (DiagDirection)(rd.x & 3));
		uint32 r;
		Direction newdir;
		const RoadDriveEntry *rdp;

		if (dir == INVALID_TRACKDIR) {
			v->cur_speed = 0;
			return;
		}

again:
		if (IsReversingRoadTrackdir(dir)) {
			/* Turning around */
			if (v->u.road.roadtype == ROADTYPE_TRAM) {
				RoadBits needed; // The road bits the tram needs to be able to turn around
				switch (dir) {
					default: NOT_REACHED();
					case TRACKDIR_RVREV_NE: needed = ROAD_SW; break;
					case TRACKDIR_RVREV_SE: needed = ROAD_NW; break;
					case TRACKDIR_RVREV_SW: needed = ROAD_NE; break;
					case TRACKDIR_RVREV_NW: needed = ROAD_SE; break;
				}
				if (!IsTileType(tile, MP_STREET) || GetRoadTileType(tile) != ROAD_TILE_NORMAL || (needed & GetRoadBits(tile, ROADTYPE_TRAM)) == ROAD_NONE) {
					/* The tram cannot turn here */
					v->cur_speed = 0;
					return;
				}
			} else {
				tile = v->tile;
			}
		}

		/* Get position data for first frame on the new tile */
		rdp = _road_drive_data[v->u.road.roadtype][(dir + (_opt.road_side << RVS_DRIVE_SIDE)) ^ v->u.road.overtaking];

		x = TileX(tile) * TILE_SIZE + rdp[RVC_DEFAULT_START_FRAME].x;
		y = TileY(tile) * TILE_SIZE + rdp[RVC_DEFAULT_START_FRAME].y;

		newdir = RoadVehGetSlidingDirection(v, x, y);
		if (RoadVehFindCloseTo(v, x, y, newdir) != NULL) return;

		r = VehicleEnterTile(v, tile, x, y);
		if (HASBIT(r, VETS_CANNOT_ENTER)) {
			if (!IsTileType(tile, MP_TUNNELBRIDGE)) {
				v->cur_speed = 0;
				return;
			}
			/* Try an about turn to re-enter the previous tile */
			dir = _road_reverse_table[rd.x & 3];
			goto again;
		}

		if (IS_BYTE_INSIDE(v->u.road.state, RVSB_IN_ROAD_STOP, RVSB_IN_DT_ROAD_STOP_END) && IsTileType(v->tile, MP_STATION)) {
			if (IsReversingRoadTrackdir(dir) && IS_BYTE_INSIDE(v->u.road.state, RVSB_IN_ROAD_STOP, RVSB_IN_ROAD_STOP_END)) {
				/* New direction is trying to turn vehicle around.
				 * We can't turn at the exit of a road stop so wait.*/
				v->cur_speed = 0;
				return;
			}
			if (IsRoadStop(v->tile)) {
				RoadStop *rs = GetRoadStopByTile(v->tile, GetRoadStopType(v->tile));

				/* Vehicle is leaving a road stop tile, mark bay as free
				 * For drive-through stops, only do it if the vehicle stopped here */
				if (IsStandardRoadStopTile(v->tile) || HASBIT(v->u.road.state, RVS_IS_STOPPING)) {
					rs->FreeBay(HASBIT(v->u.road.state, RVS_USING_SECOND_BAY));
					CLRBIT(v->u.road.state, RVS_IS_STOPPING);
				}
				if (IsStandardRoadStopTile(v->tile)) rs->SetEntranceBusy(false);
			}
		}

		if (!HASBIT(r, VETS_ENTERED_WORMHOLE)) {
			v->tile = tile;
			v->u.road.state = (byte)dir;
			v->u.road.frame = RVC_DEFAULT_START_FRAME;
		}
		if (newdir != v->direction) {
			v->direction = newdir;
			v->cur_speed -= v->cur_speed >> 2;
		}

		v->cur_image = GetRoadVehImage(v, newdir);
		v->UpdateDeltaXY(v->direction);
		RoadZPosAffectSpeed(v, SetRoadVehPosition(v, x, y));
		return;
	}

	if (rd.x & RDE_TURNED) {
		/* Vehicle has finished turning around, it will now head back onto the same tile */
		Trackdir dir = RoadFindPathToDest(v, v->tile, (DiagDirection)(rd.x & 3));
		uint32 r;
		Direction newdir;
		const RoadDriveEntry *rdp;

		if (dir == INVALID_TRACKDIR) {
			v->cur_speed = 0;
			return;
		}

		rdp = _road_drive_data[v->u.road.roadtype][(_opt.road_side << RVS_DRIVE_SIDE) + dir];

		x = TileX(v->tile) * TILE_SIZE + rdp[RVC_TURN_AROUND_START_FRAME].x;
		y = TileY(v->tile) * TILE_SIZE + rdp[RVC_TURN_AROUND_START_FRAME].y;

		newdir = RoadVehGetSlidingDirection(v, x, y);
		if (RoadVehFindCloseTo(v, x, y, newdir) != NULL) return;

		r = VehicleEnterTile(v, v->tile, x, y);
		if (HASBIT(r, VETS_CANNOT_ENTER)) {
			v->cur_speed = 0;
			return;
		}

		v->u.road.state = dir;
		v->u.road.frame = RVC_TURN_AROUND_START_FRAME;

		if (newdir != v->direction) {
			v->direction = newdir;
			v->cur_speed -= v->cur_speed >> 2;
		}

		v->cur_image = GetRoadVehImage(v, newdir);
		v->UpdateDeltaXY(v->direction);
		RoadZPosAffectSpeed(v, SetRoadVehPosition(v, x, y));
		return;
	}

	/* Calculate new position for the vehicle */
	x = (v->x_pos & ~15) + (rd.x & 15);
	y = (v->y_pos & ~15) + (rd.y & 15);

	new_dir = RoadVehGetSlidingDirection(v, x, y);

	if (!IS_BYTE_INSIDE(v->u.road.state, RVSB_IN_ROAD_STOP, RVSB_IN_ROAD_STOP_END)) {
		/* Vehicle is not in a road stop.
		 * Check for another vehicle to overtake */
		Vehicle* u = RoadVehFindCloseTo(v, x, y, new_dir);

		if (u != NULL) {
			v->cur_speed = u->cur_speed;
			/* There is a vehicle in front overtake it if possible */
			if (v->u.road.overtaking == 0) RoadVehCheckOvertake(v, u);
			return;
		}
	}

	old_dir = v->direction;
	if (new_dir != old_dir) {
		v->direction = new_dir;
		v->cur_speed -= (v->cur_speed >> 2);
		if (old_dir != v->u.road.state) {
			/* The vehicle is in a road stop */
			v->cur_image = GetRoadVehImage(v, new_dir);
			v->UpdateDeltaXY(v->direction);
			SetRoadVehPosition(v, v->x_pos, v->y_pos);
			/* Note, return here means that the frame counter is not incremented
			 * for vehicles changing direction in a road stop. This causes frames to
			 * be repeated. (XXX) Is this intended? */
			return;
		}
	}

	/* If the vehicle is in a normal road stop and the frame equals the stop frame OR
	 * if the vehicle is in a drive-through road stop and this is the destination station
	 * and it's the correct type of stop (bus or truck) and the frame equals the stop frame...
	 * (the station test and stop type test ensure that other vehicles, using the road stop as
	 * a through route, do not stop) */
	if ((IS_BYTE_INSIDE(v->u.road.state, RVSB_IN_ROAD_STOP, RVSB_IN_ROAD_STOP_END) &&
			_road_veh_data_1[v->u.road.state - RVSB_IN_ROAD_STOP + (_opt.road_side << RVS_DRIVE_SIDE)] == v->u.road.frame) ||
			(IS_BYTE_INSIDE(v->u.road.state, RVSB_IN_DT_ROAD_STOP, RVSB_IN_DT_ROAD_STOP_END) &&
			v->current_order.dest == GetStationIndex(v->tile) &&
			GetRoadStopType(v->tile) == (IsCargoInClass(v->cargo_type, CC_PASSENGERS) ? RoadStop::BUS : RoadStop::TRUCK) &&
			v->u.road.frame == RVC_DRIVE_THROUGH_STOP_FRAME)) {

		RoadStop *rs = GetRoadStopByTile(v->tile, GetRoadStopType(v->tile));
		Station* st = GetStationByTile(v->tile);

		/* Vehicle is at the stop position (at a bay) in a road stop.
		 * Note, if vehicle is loading/unloading it has already been handled,
		 * so if we get here the vehicle has just arrived or is just ready to leave. */
		if (v->current_order.type != OT_LEAVESTATION &&
				v->current_order.type != OT_GOTO_DEPOT) {
			/* Vehicle has arrived at a bay in a road stop */

			if (IsDriveThroughStopTile(v->tile)) {
				TileIndex next_tile = TILE_ADD(v->tile, TileOffsByDir(v->direction));
				RoadStop::Type type = IsCargoInClass(v->cargo_type, CC_PASSENGERS) ? RoadStop::BUS : RoadStop::TRUCK;

				/* Check if next inline bay is free */
				if (IsDriveThroughStopTile(next_tile) && (GetRoadStopType(next_tile) == type)) {
					RoadStop *rs_n = GetRoadStopByTile(next_tile, type);

					if (rs_n->IsFreeBay(HASBIT(v->u.road.state, RVS_USING_SECOND_BAY))) {
						/* Bay in next stop along is free - use it */
						ClearSlot(v);
						rs_n->num_vehicles++;
						v->u.road.slot = rs_n;
						v->dest_tile = rs_n->xy;
						v->u.road.slot_age = 14;

						v->u.road.frame++;
						RoadZPosAffectSpeed(v, SetRoadVehPosition(v, x, y));
						return;
					}
				}
			}

			rs->SetEntranceBusy(false);

			v->last_station_visited = GetStationIndex(v->tile);

			RoadVehArrivesAt(v, st);
			v->BeginLoading();

			return;
		}

		/* Vehicle is ready to leave a bay in a road stop */
		if (v->current_order.type != OT_GOTO_DEPOT) {
			if (rs->IsEntranceBusy()) {
				/* Road stop entrance is busy, so wait as there is nowhere else to go */
				v->cur_speed = 0;
				return;
			}
			v->current_order.Free();
			ClearSlot(v);
		}

		if (IsStandardRoadStopTile(v->tile)) rs->SetEntranceBusy(true);

		if (rs == v->u.road.slot) {
			/* We are leaving the correct station */
			ClearSlot(v);
		} else if (v->u.road.slot != NULL) {
			/* We are leaving the wrong station
			 * XXX The question is .. what to do? Actually we shouldn't be here
			 * but I guess we need to clear the slot */
			DEBUG(ms, 0, "Vehicle %d (index %d) arrived at wrong stop", v->unitnumber, v->index);
			if (v->tile != v->dest_tile) {
				DEBUG(ms, 2, " current tile 0x%X is not destination tile 0x%X. Route problem", v->tile, v->dest_tile);
			}
			if (v->dest_tile != v->u.road.slot->xy) {
				DEBUG(ms, 2, " stop tile 0x%X is not destination tile 0x%X. Multistop desync", v->u.road.slot->xy, v->dest_tile);
			}
			if (v->current_order.type != OT_GOTO_STATION) {
				DEBUG(ms, 2, " current order type (%d) is not OT_GOTO_STATION", v->current_order.type);
			} else {
				if (v->current_order.dest != st->index)
					DEBUG(ms, 2, " current station %d is not target station in current_order.station (%d)",
							st->index, v->current_order.dest);
			}

			DEBUG(ms, 2, " force a slot clearing");
			ClearSlot(v);
		}

		StartRoadVehSound(v);
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
	}

	/* Check tile position conditions - i.e. stop position in depot,
	 * entry onto bridge or into tunnel */
	r = VehicleEnterTile(v, v->tile, x, y);
	if (HASBIT(r, VETS_CANNOT_ENTER)) {
		v->cur_speed = 0;
		return;
	}

	/* Move to next frame unless vehicle arrived at a stop position
	 * in a depot or entered a tunnel/bridge */
	if (!HASBIT(r, VETS_ENTERED_WORMHOLE)) v->u.road.frame++;

	v->cur_image = GetRoadVehImage(v, v->direction);
	v->UpdateDeltaXY(v->direction);
	RoadZPosAffectSpeed(v, SetRoadVehPosition(v, x, y));
}

static void AgeRoadVehCargo(Vehicle *v)
{
	if (_age_cargo_skip_counter != 0) return;
	if (v->cargo_days != 255) v->cargo_days++;
}

void RoadVeh_Tick(Vehicle *v)
{
	AgeRoadVehCargo(v);
	RoadVehController(v);
}

static void CheckIfRoadVehNeedsService(Vehicle *v)
{
	const Depot* depot;

	if (_patches.servint_roadveh == 0) return;
	if (!VehicleNeedsService(v)) return;
	if (v->vehstatus & VS_STOPPED) return;
	if (_patches.gotodepot && VehicleHasDepotOrders(v)) return;

	/* Don't interfere with a depot visit scheduled by the user, or a
	 * depot visit by the order list. */
	if (v->current_order.type == OT_GOTO_DEPOT &&
			(v->current_order.flags & (OF_HALT_IN_DEPOT | OF_PART_OF_ORDERS)) != 0)
		return;

	/* If we already got a slot at a stop, use that FIRST, and go to a depot later */
	if (v->u.road.slot != NULL) return;

	if (IsRoadVehInDepot(v)) {
		VehicleServiceInDepot(v);
		return;
	}

	/* XXX If we already have a depot order, WHY do we search over and over? */
	depot = FindClosestRoadDepot(v);

	if (depot == NULL || DistanceManhattan(v->tile, depot->xy) > 12) {
		if (v->current_order.type == OT_GOTO_DEPOT) {
			v->current_order.type = OT_DUMMY;
			v->current_order.flags = 0;
			InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
		}
		return;
	}

	if (v->current_order.type == OT_GOTO_DEPOT &&
			v->current_order.flags & OF_NON_STOP &&
			!CHANCE16(1, 20)) {
		return;
	}

	if (v->current_order.type == OT_LOADING) v->LeaveStation();
	ClearSlot(v);

	v->current_order.type = OT_GOTO_DEPOT;
	v->current_order.flags = OF_NON_STOP;
	v->current_order.dest = depot->index;
	v->dest_tile = depot->xy;
	InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
}

void OnNewDay_RoadVeh(Vehicle *v)
{
	int32 cost;

	if ((++v->day_counter & 7) == 0) DecreaseVehicleValue(v);
	if (v->u.road.blocked_ctr == 0) CheckVehicleBreakdown(v);

	AgeVehicle(v);
	CheckIfRoadVehNeedsService(v);

	CheckOrders(v);

	/* Current slot has expired */
	if (v->current_order.type == OT_GOTO_STATION && v->u.road.slot != NULL && v->u.road.slot_age-- == 0) {
		DEBUG(ms, 3, "Slot expired for vehicle %d (index %d) at stop 0x%X",
			v->unitnumber, v->index, v->u.road.slot->xy);
		ClearSlot(v);
	}

	if (v->vehstatus & VS_STOPPED) return;

	/* update destination */
	if (v->current_order.type == OT_GOTO_STATION && v->u.road.slot == NULL && !(v->vehstatus & VS_CRASHED)) {
		Station* st = GetStation(v->current_order.dest);
		RoadStop* rs = st->GetPrimaryRoadStop(IsCargoInClass(v->cargo_type, CC_PASSENGERS) ? RoadStop::BUS : RoadStop::TRUCK);
		RoadStop* best = NULL;

		if (rs != NULL) {
			/* We try to obtain a slot if:
			 * 1) we're reasonably close to the primary road stop
			 * or
			 * 2) we're somewhere close to the station rectangle (to make sure we do assign
			 *    slots even if the station and its road stops are incredibly spread out)
			 */
			if (DistanceManhattan(v->tile, rs->xy) < 16 || st->rect.PtInExtendedRect(TileX(v->tile), TileY(v->tile), 2)) {
				uint dist, badness;
				uint minbadness = UINT_MAX;

				DEBUG(ms, 2, "Attempting to obtain a slot for vehicle %d (index %d) at station %d (0x%X)",
					v->unitnumber, v->index, st->index, st->xy
				);
				/* Now we find the nearest road stop that has a free slot */
				for (; rs != NULL; rs = rs->next) {
					dist = RoadFindPathToStop(v, rs->xy);
					if (dist == UINT_MAX) {
						DEBUG(ms, 4, " stop 0x%X is unreachable, not treating further", rs->xy);
						continue;
					}
					badness = (rs->num_vehicles + 1) * (rs->num_vehicles + 1) + dist;

					DEBUG(ms, 4, " stop 0x%X has %d vehicle%s waiting", rs->xy, rs->num_vehicles, rs->num_vehicles == 1 ? "":"s");
					DEBUG(ms, 4, " distance is %u", dist);
					DEBUG(ms, 4, " badness %u", badness);

					if (badness < minbadness) {
						best = rs;
						minbadness = badness;
					}
				}

				if (best != NULL) {
					best->num_vehicles++;
					DEBUG(ms, 3, "Assigned to stop 0x%X", best->xy);

					v->u.road.slot = best;
					v->dest_tile = best->xy;
					v->u.road.slot_age = 14;
				} else {
					DEBUG(ms, 3, "Could not find a suitable stop");
				}
			} else {
				DEBUG(ms, 5, "Distance from station too far. Postponing slotting for vehicle %d (index %d) at station %d, (0x%X)",
						v->unitnumber, v->index, st->index, st->xy);
			}
		} else {
			DEBUG(ms, 4, "No road stop for vehicle %d (index %d) at station %d (0x%X)",
					v->unitnumber, v->index, st->index, st->xy);
		}
	}

	cost = RoadVehInfo(v->engine_type)->running_cost * _price.roadveh_running / 364;

	v->profit_this_year -= cost >> 8;

	SET_EXPENSES_TYPE(EXPENSES_ROADVEH_RUN);
	SubtractMoneyFromPlayerFract(v->owner, cost);

	InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
	InvalidateWindowClasses(WC_ROADVEH_LIST);
}


void RoadVehiclesYearlyLoop()
{
	Vehicle *v;

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_ROAD) {
			v->profit_last_year = v->profit_this_year;
			v->profit_this_year = 0;
			InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
		}
	}
}

/** Refit a road vehicle to the specified cargo type
 * @param tile unused
 * @param flags operation to perform
 * @param p1 Vehicle ID of the vehicle to refit
 * @param p2 Bitstuffed elements
 * - p2 = (bit 0-7) - the new cargo type to refit to
 * - p2 = (bit 8-15) - the new cargo subtype to refit to
 * - p2 = (bit 16) - refit only this vehicle (ignored)
 * @return cost of refit or error
 */
int32 CmdRefitRoadVeh(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;
	int32 cost;
	CargoID new_cid = GB(p2, 0, 8);
	byte new_subtype = GB(p2, 8, 8);
	uint16 capacity = CALLBACK_FAILED;

	if (!IsValidVehicleID(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type != VEH_ROAD || !CheckOwnership(v->owner)) return CMD_ERROR;
	if (!IsRoadVehInDepotStopped(v)) return_cmd_error(STR_9013_MUST_BE_STOPPED_INSIDE);

	if (new_cid >= NUM_CARGO || !CanRefitTo(v->engine_type, new_cid)) return CMD_ERROR;

	SET_EXPENSES_TYPE(EXPENSES_ROADVEH_RUN);

	if (HASBIT(EngInfo(v->engine_type)->callbackmask, CBM_REFIT_CAPACITY)) {
		/* Back up the cargo type */
		CargoID temp_cid = v->cargo_type;
		byte temp_subtype = v->cargo_subtype;
		v->cargo_type = new_cid;
		v->cargo_subtype = new_subtype;

		/* Check the refit capacity callback */
		capacity = GetVehicleCallback(CBID_VEHICLE_REFIT_CAPACITY, 0, 0, v->engine_type, v);

		/* Restore the original cargo type */
		v->cargo_type = temp_cid;
		v->cargo_subtype = temp_subtype;
	}

	if (capacity == CALLBACK_FAILED) {
		/* callback failed or not used, use default capacity */
		const RoadVehicleInfo *rvi = RoadVehInfo(v->engine_type);

		CargoID old_cid = rvi->cargo_type;
		/* normally, the capacity depends on the cargo type, a vehicle can
		 * carry twice as much mail/goods as normal cargo, and four times as
		 * many passengers
		 */
		capacity = GetVehicleProperty(v, 0x0F, rvi->capacity);
		switch (old_cid) {
			case CT_PASSENGERS: break;
			case CT_MAIL:
			case CT_GOODS: capacity *= 2; break;
			default:       capacity *= 4; break;
		}
		switch (new_cid) {
			case CT_PASSENGERS: break;
			case CT_MAIL:
			case CT_GOODS: capacity /= 2; break;
			default:       capacity /= 4; break;
		}
	}
	_returned_refit_capacity = capacity;

	cost = 0;
	if (IsHumanPlayer(v->owner) && new_cid != v->cargo_type) {
		cost = GetRefitCost(v->engine_type);
	}

	if (flags & DC_EXEC) {
		v->cargo_cap = capacity;
		v->cargo_count = (v->cargo_type == new_cid) ? min(capacity, v->cargo_count) : 0;
		v->cargo_type = new_cid;
		v->cargo_subtype = new_subtype;
		InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
		RebuildVehicleLists();
	}

	return cost;
}
