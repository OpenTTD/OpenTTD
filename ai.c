#include "stdafx.h"
#include "ttd.h"
#include "player.h"
#include "vehicle.h"
#include "engine.h"
#include "command.h"
#include "town.h"
#include "industry.h"
#include "station.h"
#include "pathfind.h"
#include "economy.h"
#include "airport.h"

// remove some day perhaps?
static Player *_cur_ai_player;
static uint _ai_service_interval;

typedef void AiStateAction(Player *p);

enum {
	AIS_0 = 0,
	AIS_1 = 1,
	AIS_VEH_LOOP = 2,
	AIS_VEH_CHECK_REPLACE_VEHICLE = 3,
	AIS_VEH_DO_REPLACE_VEHICLE = 4,
	AIS_WANT_NEW_ROUTE = 5,
	AIS_BUILD_DEFAULT_RAIL_BLOCKS = 6,
	AIS_BUILD_RAIL = 7,
	AIS_BUILD_RAIL_VEH = 8,
	AIS_DELETE_RAIL_BLOCKS = 9,
	AIS_BUILD_DEFAULT_ROAD_BLOCKS = 10,
	AIS_BUILD_ROAD = 11,
	AIS_BUILD_ROAD_VEHICLES = 12,
	AIS_DELETE_ROAD_BLOCKS = 13,
	AIS_AIRPORT_STUFF = 14,
	AIS_BUILD_DEFAULT_AIRPORT_BLOCKS = 15,
	AIS_BUILD_AIRCRAFT_VEHICLES = 16,
	AIS_CHECK_SHIP_STUFF = 17,
	AIS_BUILD_DEFAULT_SHIP_BLOCKS = 18,
	AIS_DO_SHIP_STUFF = 19,
	AIS_SELL_VEHICLE = 20,
	AIS_REMOVE_STATION = 21,
	AIS_REMOVE_TRACK = 22,
	AIS_REMOVE_SINGLE_RAIL_TILE = 23
};


#include "table/ai_rail.h"

static byte GetRailTrackStatus(TileIndex tile) {
	uint32 r = GetTileTrackStatus(tile, TRANSPORT_RAIL);
	return (byte) (r | r >> 8);
}


static void AiCase0(Player *p)
{
	p->ai.state = AIS_REMOVE_TRACK;
	p->ai.state_counter = 0;
}

static void AiCase1(Player *p)
{
	p->ai.cur_veh = NULL;
	p->ai.state = AIS_VEH_LOOP;
}

static void AiStateVehLoop(Player *p)
{
	Vehicle *v;

	v = p->ai.cur_veh == NULL ? _vehicles : p->ai.cur_veh+1;

	for (;v != endof(_vehicles); v++) {
		if (v->type == 0 || v->owner != _current_player)
			continue;

		if ((v->type == VEH_Train && v->subtype==0) ||
				v->type == VEH_Road ||
				(v->type == VEH_Aircraft && v->subtype <= 2) ||
				v->type == VEH_Ship) {

			/* replace engine? */
			if (v->type == VEH_Train && v->engine_type < 3 &&
					(_price.build_railvehicle >> 3) < p->player_money) {
				p->ai.state = AIS_VEH_CHECK_REPLACE_VEHICLE;
				p->ai.cur_veh = v;
				return;
			}

			/* not profitable? */
			if (v->age >= 730 &&
					v->profit_last_year < _price.station_value*5 &&
					v->profit_this_year < _price.station_value*5) {
				p->ai.state_counter = 0;
				p->ai.state = AIS_SELL_VEHICLE;
				p->ai.cur_veh = v;
				return;
			}

			/* not reliable? */
			if ((v->age != 0 &&
					_engines[v->engine_type].reliability < 35389) ||
					v->age >= v->max_age) {
				p->ai.state = AIS_VEH_CHECK_REPLACE_VEHICLE;
				p->ai.cur_veh = v;
				return;
			}
		}
	}

	p->ai.state = AIS_WANT_NEW_ROUTE;
	p->ai.state_counter = 0;
}

static int AiChooseTrainToBuild(byte railtype, int32 money, byte flag)
{
	int best_veh_index = -1;
	byte best_veh_score = 0;
	int32 r;
	int i;

	for (i = 0; i < NUM_TRAIN_ENGINES; i++) {
		const RailVehicleInfo *rvi = RailVehInfo(i);
		Engine *e = DEREF_ENGINE(i);

		if (e->railtype != railtype || rvi->flags & RVI_WAGON
		    || !HASBIT(e->player_avail, _current_player) || e->reliability < 0x8A3D)
			continue;

		r = DoCommandByTile(0, i, 0, 0, CMD_BUILD_RAIL_VEHICLE);
		if (r != CMD_ERROR &&
					(!(_cmd_build_rail_veh_var1&1) || !(flag&1)) &&
					r <= money &&
				_cmd_build_rail_veh_score >= best_veh_score) {
			best_veh_score = _cmd_build_rail_veh_score;
			best_veh_index = i;
		}
	}

	return best_veh_index;
}

static int AiChooseRoadVehToBuild(byte cargo, int32 money)
{
	int best_veh_index = -1;
	int32 best_veh_cost = 0;
	int32 r;

	int i = _cargoc.ai_roadveh_start[cargo];
	int end = i + _cargoc.ai_roadveh_count[cargo];
	Engine *e = &_engines[i];
	do {
		if (!HASBIT(e->player_avail, _current_player) || e->reliability < 0x8A3D)
			continue;

		r = DoCommandByTile(0, i, 0, 0, CMD_BUILD_ROAD_VEH);
		if (r != CMD_ERROR && r <= money && r >= best_veh_cost) {
			best_veh_cost = r;
			best_veh_index = i;
		}
	} while (++e, ++i != end);

	return best_veh_index;
}

static int AiChooseAircraftToBuild(int32 money, byte flag)
{
	int best_veh_index = -1;
	int32 best_veh_cost = 0;
	int32 r;

	int i = AIRCRAFT_ENGINES_INDEX;
	int end = i + NUM_AIRCRAFT_ENGINES;
	Engine *e = &_engines[i];
	do {
		if (!HASBIT(e->player_avail, _current_player) || e->reliability < 0x8A3D)
			continue;

		if (flag&1) {
			if (i<253) continue;
		} else {
			if (i>=253) continue;
		}

		r = DoCommandByTile(0, i, 0, 0, CMD_BUILD_AIRCRAFT);
		if (r != CMD_ERROR && r <= money && r >= best_veh_cost) {
			best_veh_cost = r;
			best_veh_index = i;
		}
	} while (++e, ++i != end);

	return best_veh_index;
}

static int32 AiGetBasePrice(Player *p)
{
	int32 base = _price.station_value;

	// adjust base price when more expensive vehicles are available
	if (p->ai.railtype_to_use == 1) base = (base * 3) >> 1;
	else if (p->ai.railtype_to_use == 2) base *= 2;

	return base;
}

#if 0
static int AiChooseShipToBuild(byte cargo, int32 money)
{
	// XXX: not done
	return 0;
}
#endif

static int AiChooseRoadVehToReplaceWith(Player *p, Vehicle *v)
{
	int32 avail_money = p->player_money + v->value;
	return AiChooseRoadVehToBuild(v->cargo_type, avail_money);
}

static int AiChooseAircraftToReplaceWith(Player *p, Vehicle *v)
{
	int32 avail_money = p->player_money + v->value;
	return AiChooseAircraftToBuild(avail_money, v->engine_type>=253?1:0);
}

static int AiChooseTrainToReplaceWith(Player *p, Vehicle *v)
{
	int32 avail_money = p->player_money + v->value;
	int num=0;
	Vehicle *u = v;

	while (++num, u->next != NULL) {
		u = u->next;
	}

	// XXX: check if a wagon
	return AiChooseTrainToBuild(v->u.rail.railtype, avail_money, 0);
}

static int AiChooseShipToReplaceWith(Player *p, Vehicle *v)
{
	error("!AiChooseShipToReplaceWith");

	/* maybe useless, but avoids compiler warning this way */
	return 0;
}

static void AiHandleGotoDepot(Player *p, int cmd)
{
	if (p->ai.cur_veh->current_order.type != OT_GOTO_DEPOT)
		DoCommandByTile(0, p->ai.cur_veh->index, 0, DC_EXEC, cmd);

	if (++p->ai.state_counter <= 1387) {
		p->ai.state = AIS_VEH_DO_REPLACE_VEHICLE;
		return;
	}

	if (p->ai.cur_veh->current_order.type == OT_GOTO_DEPOT) {
		p->ai.cur_veh->current_order.type = OT_DUMMY;
		p->ai.cur_veh->current_order.flags = 0;
		InvalidateWindow(WC_VEHICLE_VIEW, p->ai.cur_veh->index);
	}
}

static void AiRestoreVehicleOrders(Vehicle *v, BackuppedOrders *bak)
{
	const Order *os = bak->order;
	int ind = 0;

	while (os++->type != OT_NOTHING) {
		if (DoCommandByTile(0, v->index + (ind << 16), PackOrder(os), DC_EXEC, CMD_INSERT_ORDER) == CMD_ERROR)
			break;
		ind++;
	}
}

static void AiHandleReplaceTrain(Player *p)
{
	Vehicle *v = p->ai.cur_veh;
	BackuppedOrders orderbak[1];
	int veh;
	uint tile;

	// wait until the vehicle reaches the depot.
	if (!IsTrainDepotTile(v->tile) || v->u.rail.track != 0x80 || !(v->vehstatus&VS_STOPPED)) {
		AiHandleGotoDepot(p, CMD_TRAIN_GOTO_DEPOT);
		return;
	}

	veh = AiChooseTrainToReplaceWith(p, v);
	if (veh != -1) {
		BackupVehicleOrders(v, orderbak);
		tile = v->tile;

		if (DoCommandByTile(0, v->index, 2, DC_EXEC, CMD_SELL_RAIL_WAGON) != CMD_ERROR &&
			 DoCommandByTile(tile, veh, 0, DC_EXEC, CMD_BUILD_RAIL_VEHICLE) != CMD_ERROR) {
			veh = _new_train_id;
			AiRestoreVehicleOrders(&_vehicles[veh], orderbak);
			DoCommandByTile(0, veh, 0, DC_EXEC, CMD_START_STOP_TRAIN);

			DoCommandByTile(0, veh, _ai_service_interval, DC_EXEC, CMD_CHANGE_TRAIN_SERVICE_INT);
		}
	}
}

static void AiHandleReplaceRoadVeh(Player *p)
{
	Vehicle *v = p->ai.cur_veh;
	BackuppedOrders orderbak[1];
	int veh;
	uint tile;

	if (!IsRoadDepotTile(v->tile) || v->u.road.state != 254 || !(v->vehstatus&VS_STOPPED)) {
		AiHandleGotoDepot(p, CMD_SEND_ROADVEH_TO_DEPOT);
		return;
	}

	veh = AiChooseRoadVehToReplaceWith(p, v);
	if (veh != -1) {
		BackupVehicleOrders(v, orderbak);
		tile = v->tile;

		if (DoCommandByTile(0, v->index, 0, DC_EXEC, CMD_SELL_ROAD_VEH) != CMD_ERROR &&
			 DoCommandByTile(tile, veh, 0, DC_EXEC, CMD_BUILD_ROAD_VEH) != CMD_ERROR) {
			veh = _new_roadveh_id;
			AiRestoreVehicleOrders(&_vehicles[veh], orderbak);
			DoCommandByTile(0, veh, 0, DC_EXEC, CMD_START_STOP_ROADVEH);

			DoCommandByTile(0, veh, _ai_service_interval, DC_EXEC, CMD_CHANGE_TRAIN_SERVICE_INT);
		}
	}
}

static void AiHandleReplaceAircraft(Player *p)
{
	Vehicle *v = p->ai.cur_veh;
	int veh;
	BackuppedOrders orderbak[1];
	uint tile;

	if (!IsAircraftHangarTile(v->tile) && !(v->vehstatus&VS_STOPPED)) {
		AiHandleGotoDepot(p, CMD_SEND_AIRCRAFT_TO_HANGAR);
		return;
	}

	veh = AiChooseAircraftToReplaceWith(p, v);
	if (veh != -1) {
		BackupVehicleOrders(v, orderbak);
		tile = v->tile;

		if (DoCommandByTile(0, v->index, 0, DC_EXEC, CMD_SELL_AIRCRAFT) != CMD_ERROR &&
			 DoCommandByTile(tile, veh, 0, DC_EXEC, CMD_BUILD_AIRCRAFT) != CMD_ERROR) {
			veh = _new_aircraft_id;
			AiRestoreVehicleOrders(&_vehicles[veh], orderbak);
			DoCommandByTile(0, veh, 0, DC_EXEC, CMD_START_STOP_AIRCRAFT);

			DoCommandByTile(0, veh, _ai_service_interval, DC_EXEC, CMD_CHANGE_TRAIN_SERVICE_INT);
		}
	}
}

static void AiHandleReplaceShip(Player *p)
{
	error("!AiHandleReplaceShip");
}

typedef int CheckReplaceProc(Player *p, Vehicle *v);

static CheckReplaceProc * const _veh_check_replace_proc[] = {
	AiChooseTrainToReplaceWith,
	AiChooseRoadVehToReplaceWith,
	AiChooseShipToReplaceWith,
	AiChooseAircraftToReplaceWith,
};

typedef void DoReplaceProc(Player *p);
static DoReplaceProc * const _veh_do_replace_proc[] = {
	AiHandleReplaceTrain,
	AiHandleReplaceRoadVeh,
	AiHandleReplaceShip,
	AiHandleReplaceAircraft
};

static void AiStateCheckReplaceVehicle(Player *p)
{
	Vehicle *v = p->ai.cur_veh;

	if (v->type == 0 || v->owner != _current_player || v->type > VEH_Ship || _veh_check_replace_proc[v->type - VEH_Train](p, v) == -1) {
		p->ai.state = AIS_VEH_LOOP;
	} else {
		p->ai.state_counter = 0;
		p->ai.state = AIS_VEH_DO_REPLACE_VEHICLE;
	}
}

static void AiStateDoReplaceVehicle(Player *p)
{
	Vehicle *v = p->ai.cur_veh;
	p->ai.state = AIS_VEH_LOOP;
	// vehicle is not owned by the player anymore, something went very wrong.
	if (v->type == 0 || v->owner != _current_player)
		return;
	_veh_do_replace_proc[v->type - VEH_Train](p);
}

typedef struct FoundRoute {
	int distance;
	byte cargo;
	void *from;
	void *to;
} FoundRoute;

static Town *AiFindRandomTown() {
	Town *t = DEREF_TOWN(RandomRange(_total_towns));
	return (t->xy != 0) ? t : NULL;
}

static Industry *AiFindRandomIndustry() {
	Industry *i = DEREF_INDUSTRY(RandomRange(_total_industries));
	return (i->xy != 0) ? i : NULL;
}

static void AiFindSubsidyIndustryRoute(FoundRoute *fr)
{
	int i;
	byte cargo;
	Subsidy *s;
	Industry *from, *to_ind;
	Town *to_tow;
	TileIndex to_xy;

	// initially error
	fr->distance = -1;

	// Randomize subsidy index..
	i = RandomRange(lengthof(_subsidies) * 3);
	if (i >= lengthof(_subsidies))
		return;

	s = &_subsidies[i];

	// Don't want passengers or mail
	cargo = s->cargo_type;
	if (cargo == 0xFF || cargo == CT_PASSENGERS || cargo == CT_MAIL || s->age > 7)
		return;
	fr->cargo = cargo;

	fr->from = from = DEREF_INDUSTRY(s->from);

	if (cargo == CT_GOODS || cargo == CT_FOOD) {
		to_tow = DEREF_TOWN(s->to);
		if (to_tow->population < (uint32)(cargo == CT_FOOD ? 200 : 900))
			return; // error
		fr->to = to_tow;
		to_xy = to_tow->xy;
	} else {
		to_ind = DEREF_INDUSTRY(s->to);
		fr->to = to_ind;
		to_xy = to_ind->xy;
	}

	fr->distance = GetTileDist(from->xy, to_xy);
}

static void AiFindSubsidyPassengerRoute(FoundRoute *fr)
{
	uint i;
	Subsidy *s;
	Town *from,*to;

	// initially error
	fr->distance = -1;

	// Randomize subsidy index..
	i = RandomRange(lengthof(_subsidies) * 3);
	if (i >= lengthof(_subsidies))
		return;

	s = &_subsidies[i];

	// Only want passengers
	if (s->cargo_type != CT_PASSENGERS || s->age > 7)
		return;
	fr->cargo = s->cargo_type;

	fr->from = from = DEREF_TOWN(s->from);
	fr->to = to = DEREF_TOWN(s->to);

	// They must be big enough
	if (from->population < 400 || to->population < 400)
		return;

	fr->distance = GetTileDist(from->xy, to->xy);
}

static void AiFindRandomIndustryRoute(FoundRoute *fr)
{
	Industry *i,*i2;
	Town *t;
	uint32 r;
	byte cargo;

	// initially error
	fr->distance = -1;

	r = Random();

	// pick a source
	fr->from = i = AiFindRandomIndustry();
	if (i == NULL)
		return;

	// pick a random produced cargo
	cargo = i->produced_cargo[0];
	if (r&1 && i->produced_cargo[1] != 0xFF)
		cargo = i->produced_cargo[1];

	fr->cargo = cargo;

	// don't allow passengers
	if (cargo == 0xFF || cargo == CT_PASSENGERS)
		return;

	if (cargo != CT_GOODS && cargo != CT_FOOD) {
		// pick a dest, and see if it can receive
		i2 = AiFindRandomIndustry();
		if (i2 == NULL || i == i2 || !(i2->accepts_cargo[0] == cargo || i2->accepts_cargo[1] == cargo || i2->accepts_cargo[2] == cargo))
			return;

		fr->to = i2;
		fr->distance = GetTileDist(i->xy, i2->xy);
	} else {
		// pick a dest town, and see if it's big enough
		t = AiFindRandomTown();
		if (t == NULL || t->population < (uint32)(cargo == CT_FOOD ? 200 : 900))
			return;

		fr->to = t;
		fr->distance = GetTileDist(i->xy, t->xy);
	}
}

static void AiFindRandomPassengerRoute(FoundRoute *fr)
{
	uint32 r;
	Town *source, *dest;

	// initially error
	fr->distance = -1;

	r = Random();

	fr->from = source = AiFindRandomTown();
	if (source == NULL || source->population < 400)
		return;

	fr->to = dest = AiFindRandomTown();
	if (dest == NULL || source == dest || dest->population < 400)
		return;

	fr->distance = GetTileDist(source->xy, dest->xy);
}

// Warn: depends on 'xy' being the first element in both Town and Industry
#define GET_TOWN_OR_INDUSTRY_TILE(p) (((Town*)(p))->xy)

static bool AiCheckIfRouteIsGood(Player *p, FoundRoute *fr, byte bitmask)
{
	TileIndex from_tile, to_tile;
	Station *st;
	int dist, cur;
	uint same_station = 0;

	// Make sure distance to closest station is < 37 pixels.
	from_tile = GET_TOWN_OR_INDUSTRY_TILE(fr->from);
	to_tile = GET_TOWN_OR_INDUSTRY_TILE(fr->to);

	dist = 0xFFFF;
	FOR_ALL_STATIONS(st) if (st->xy != 0 && st->owner == _current_player) {
		cur = GetTileDist1D(from_tile, st->xy);
		if (cur < dist) dist = cur;
		cur = GetTileDist1D(to_tile, st->xy);
		if (cur < dist) dist = cur;
		if (to_tile == from_tile && st->xy == to_tile)
		    same_station++;
	}

	// To prevent the AI from building ten busstations in the same town, do some calculations
	//  For each road or airport station, we want 350 of population!
	if ((bitmask == 2 || bitmask == 4) && same_station > 2 && ((Town *)(fr->from))->population < same_station * 350)
	    return false;

	if (dist != 0xFFFF && dist > 37)
		return false;

	if (p->ai.route_type_mask != 0 && !(p->ai.route_type_mask&bitmask) && !CHANCE16(1,5))
		return false;

	if (fr->cargo == CT_PASSENGERS || fr->cargo == CT_MAIL) {
		if (((Town*)fr->from)->pct_pass_transported > 0x99 ||
				((Town*)fr->to)->pct_pass_transported > 0x99)
			return false;

		// Make sure it has a reasonably good rating
		if ( ((Town*)fr->from)->ratings[_current_player] < -100 ||
				((Town*)fr->to)->ratings[_current_player] < -100)
			return false;
	} else {
		Industry *i = (Industry*)fr->from;

		if (i->pct_transported[fr->cargo != i->produced_cargo[0]] > 0x99 ||
				i->total_production[fr->cargo != i->produced_cargo[0]] == 0)
			return false;
	}

	p->ai.route_type_mask |= bitmask;
	return true;
}

static byte AiGetDirectionBetweenTiles(TileIndex a, TileIndex b)
{
	byte i = (GET_TILE_X(a) < GET_TILE_X(b)) ? 1 : 0;
	if (GET_TILE_Y(a) >= GET_TILE_Y(b))	i ^= 3;
	return i;
}

static TileIndex AiGetPctTileBetween(TileIndex a, TileIndex b, byte pct)
{
	return TILE_XY(
			GET_TILE_X(a) + ((GET_TILE_X(b) - GET_TILE_X(a)) * pct >> 8),
			GET_TILE_Y(a) + ((GET_TILE_Y(b) - GET_TILE_Y(a)) * pct >> 8)
		);
}

static void AiWantLongIndustryRoute(Player *p)
{
	int i;
	FoundRoute fr;

	i = 60;
	for(;;) {
		// look for one from the subsidy list
		AiFindSubsidyIndustryRoute(&fr);
		if (IS_INT_INSIDE(fr.distance, 60, 90+1))
			break;

		// try a random one
		AiFindRandomIndustryRoute(&fr);
		if (IS_INT_INSIDE(fr.distance, 60, 90+1))
			break;

		// only test 60 times
		if (--i == 0)
			return;
	}

	if (!AiCheckIfRouteIsGood(p, &fr, 1))
		return;

	// Fill the source field
	p->ai.dst.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.to);
	p->ai.src.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.from);

	p->ai.src.use_tile = 0;
	p->ai.src.rand_rng = 9;
	p->ai.src.cur_building_rule = 0xFF;
	p->ai.src.unk6 = 1;
	p->ai.src.unk7 = 0;
	p->ai.src.buildcmd_a = 0x24;
	p->ai.src.buildcmd_b = 0xFF;
	p->ai.src.direction = AiGetDirectionBetweenTiles(
			p->ai.src.spec_tile,
			p->ai.dst.spec_tile
		);
	p->ai.src.cargo = fr.cargo | 0x80;

	// Fill the dest field

	p->ai.dst.use_tile = 0;
	p->ai.dst.rand_rng = 9;
	p->ai.dst.cur_building_rule = 0xFF;
	p->ai.dst.unk6 = 1;
	p->ai.dst.unk7 = 0;
	p->ai.dst.buildcmd_a = 0x34;
	p->ai.dst.buildcmd_b = 0xFF;
	p->ai.dst.direction = AiGetDirectionBetweenTiles(
			p->ai.dst.spec_tile,
			p->ai.src.spec_tile
		);
	p->ai.dst.cargo = fr.cargo;

	// Fill middle field 1
	p->ai.mid1.spec_tile = AiGetPctTileBetween(
			p->ai.src.spec_tile,
			p->ai.dst.spec_tile,
			0x55
		);
	p->ai.mid1.use_tile = 0;
	p->ai.mid1.rand_rng = 6;
	p->ai.mid1.cur_building_rule = 0xFF;
	p->ai.mid1.unk6 = 2;
	p->ai.mid1.unk7 = 1;
	p->ai.mid1.buildcmd_a = 0x30;
	p->ai.mid1.buildcmd_b = 0xFF;
	p->ai.mid1.direction = p->ai.src.direction;
	p->ai.mid1.cargo = fr.cargo;

	// Fill middle field 2
	p->ai.mid2.spec_tile = AiGetPctTileBetween(
			p->ai.src.spec_tile,
			p->ai.dst.spec_tile,
			0xAA
		);
	p->ai.mid2.use_tile = 0;
	p->ai.mid2.rand_rng = 6;
	p->ai.mid2.cur_building_rule = 0xFF;
	p->ai.mid2.unk6 = 2;
	p->ai.mid2.unk7 = 1;
	p->ai.mid2.buildcmd_a = 0xFF;
	p->ai.mid2.buildcmd_b = 0xFF;
	p->ai.mid2.direction = p->ai.dst.direction;
	p->ai.mid2.cargo = fr.cargo;

	// Fill common fields
	p->ai.cargo_type = fr.cargo;
	p->ai.num_wagons = 3;
	p->ai.build_kind = 2;
	p->ai.num_build_rec = 4;
	p->ai.num_loco_to_build = 2;
	p->ai.num_want_fullload = 2;
	p->ai.wagon_list[0] = INVALID_VEHICLE;
	p->ai.order_list_blocks[0] = 0;
	p->ai.order_list_blocks[1] = 1;
	p->ai.order_list_blocks[2] = 255;

	p->ai.state = AIS_BUILD_DEFAULT_RAIL_BLOCKS;
	p->ai.state_mode = -1;
	p->ai.state_counter = 0;
	p->ai.timeout_counter = 0;
}

static void AiWantMediumIndustryRoute(Player *p)
{
	int i;
	FoundRoute fr;

	i = 60;
	for(;;) {

		// look for one from the subsidy list
		AiFindSubsidyIndustryRoute(&fr);
		if (IS_INT_INSIDE(fr.distance, 40, 60+1))
			break;

		// try a random one
		AiFindRandomIndustryRoute(&fr);
		if (IS_INT_INSIDE(fr.distance, 40, 60+1))
			break;

		// only test 60 times
		if (--i == 0)
			return;
	}

	if (!AiCheckIfRouteIsGood(p, &fr, 1))
		return;

	// Fill the source field
	p->ai.src.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.from);
	p->ai.src.use_tile = 0;
	p->ai.src.rand_rng = 9;
	p->ai.src.cur_building_rule = 0xFF;
	p->ai.src.unk6 = 1;
	p->ai.src.unk7 = 0;
	p->ai.src.buildcmd_a = 0x10;
	p->ai.src.buildcmd_b = 0xFF;
	p->ai.src.direction = AiGetDirectionBetweenTiles(
			GET_TOWN_OR_INDUSTRY_TILE(fr.from),
			GET_TOWN_OR_INDUSTRY_TILE(fr.to)
		);
	p->ai.src.cargo = fr.cargo | 0x80;

	// Fill the dest field
	p->ai.dst.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.to);
	p->ai.dst.use_tile = 0;
	p->ai.dst.rand_rng = 9;
	p->ai.dst.cur_building_rule = 0xFF;
	p->ai.dst.unk6 = 1;
	p->ai.dst.unk7 = 0;
	p->ai.dst.buildcmd_a = 0xFF;
	p->ai.dst.buildcmd_b = 0xFF;
	p->ai.dst.direction = AiGetDirectionBetweenTiles(
			GET_TOWN_OR_INDUSTRY_TILE(fr.to),
			GET_TOWN_OR_INDUSTRY_TILE(fr.from)
		);
	p->ai.dst.cargo = fr.cargo;

	// Fill common fields
	p->ai.cargo_type = fr.cargo;
	p->ai.num_wagons = 3;
	p->ai.build_kind = 1;
	p->ai.num_build_rec = 2;
	p->ai.num_loco_to_build = 1;
	p->ai.num_want_fullload = 1;
	p->ai.wagon_list[0] = INVALID_VEHICLE;
	p->ai.order_list_blocks[0] = 0;
	p->ai.order_list_blocks[1] = 1;
	p->ai.order_list_blocks[2] = 255;
	p->ai.state = AIS_BUILD_DEFAULT_RAIL_BLOCKS;
	p->ai.state_mode = -1;
	p->ai.state_counter = 0;
	p->ai.timeout_counter = 0;
}

static void AiWantShortIndustryRoute(Player *p)
{
	int i;
	FoundRoute fr;

	i = 60;
	for(;;) {

		// look for one from the subsidy list
		AiFindSubsidyIndustryRoute(&fr);
		if (IS_INT_INSIDE(fr.distance, 15, 40+1))
			break;

		// try a random one
		AiFindRandomIndustryRoute(&fr);
		if (IS_INT_INSIDE(fr.distance, 15, 40+1))
			break;

		// only test 60 times
		if (--i == 0)
			return;
	}

	if (!AiCheckIfRouteIsGood(p, &fr, 1))
		return;

	// Fill the source field
	p->ai.src.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.from);
	p->ai.src.use_tile = 0;
	p->ai.src.rand_rng = 9;
	p->ai.src.cur_building_rule = 0xFF;
	p->ai.src.unk6 = 1;
	p->ai.src.unk7 = 0;
	p->ai.src.buildcmd_a = 0x10;
	p->ai.src.buildcmd_b = 0xFF;
	p->ai.src.direction = AiGetDirectionBetweenTiles(
			GET_TOWN_OR_INDUSTRY_TILE(fr.from),
			GET_TOWN_OR_INDUSTRY_TILE(fr.to)
		);
	p->ai.src.cargo = fr.cargo | 0x80;

	// Fill the dest field
	p->ai.dst.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.to);
	p->ai.dst.use_tile = 0;
	p->ai.dst.rand_rng = 9;
	p->ai.dst.cur_building_rule = 0xFF;
	p->ai.dst.unk6 = 1;
	p->ai.dst.unk7 = 0;
	p->ai.dst.buildcmd_a = 0xFF;
	p->ai.dst.buildcmd_b = 0xFF;
	p->ai.dst.direction = AiGetDirectionBetweenTiles(
			GET_TOWN_OR_INDUSTRY_TILE(fr.to),
			GET_TOWN_OR_INDUSTRY_TILE(fr.from)
		);
	p->ai.dst.cargo = fr.cargo;

	// Fill common fields
	p->ai.cargo_type = fr.cargo;
	p->ai.num_wagons = 2;
	p->ai.build_kind = 1;
	p->ai.num_build_rec = 2;
	p->ai.num_loco_to_build = 1;
	p->ai.num_want_fullload = 1;
	p->ai.wagon_list[0] = INVALID_VEHICLE;
	p->ai.order_list_blocks[0] = 0;
	p->ai.order_list_blocks[1] = 1;
	p->ai.order_list_blocks[2] = 255;
	p->ai.state = AIS_BUILD_DEFAULT_RAIL_BLOCKS;
	p->ai.state_mode = -1;
	p->ai.state_counter = 0;
	p->ai.timeout_counter = 0;
}

static void AiWantMailRoute(Player *p)
{
	int i;
	FoundRoute fr;

	i = 60;
	for(;;) {

		// look for one from the subsidy list
		AiFindSubsidyPassengerRoute(&fr);
		if (IS_INT_INSIDE(fr.distance, 60, 110+1))
			break;

		// try a random one
		AiFindRandomPassengerRoute(&fr);
		if (IS_INT_INSIDE(fr.distance, 60, 110+1))
			break;

		// only test 60 times
		if (--i == 0)
			return;
	}

	fr.cargo = CT_MAIL;
	if (!AiCheckIfRouteIsGood(p, &fr, 1))
		return;

	// Fill the source field
	p->ai.src.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.from);
	p->ai.src.use_tile = 0;
	p->ai.src.rand_rng = 7;
	p->ai.src.cur_building_rule = 0xFF;
	p->ai.src.unk6 = 1;
	p->ai.src.unk7 = 0;
	p->ai.src.buildcmd_a = 0x24;
	p->ai.src.buildcmd_b = 0xFF;
	p->ai.src.direction = AiGetDirectionBetweenTiles(
			GET_TOWN_OR_INDUSTRY_TILE(fr.from),
			GET_TOWN_OR_INDUSTRY_TILE(fr.to)
		);
	p->ai.src.cargo = fr.cargo;

	// Fill the dest field
	p->ai.dst.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.to);
	p->ai.dst.use_tile = 0;
	p->ai.dst.rand_rng = 7;
	p->ai.dst.cur_building_rule = 0xFF;
	p->ai.dst.unk6 = 1;
	p->ai.dst.unk7 = 0;
	p->ai.dst.buildcmd_a = 0x34;
	p->ai.dst.buildcmd_b = 0xFF;
	p->ai.dst.direction = AiGetDirectionBetweenTiles(
			GET_TOWN_OR_INDUSTRY_TILE(fr.to),
			GET_TOWN_OR_INDUSTRY_TILE(fr.from)
		);
	p->ai.dst.cargo = fr.cargo;

	// Fill middle field 1
	p->ai.mid1.spec_tile = AiGetPctTileBetween(
			GET_TOWN_OR_INDUSTRY_TILE(fr.from),
			GET_TOWN_OR_INDUSTRY_TILE(fr.to),
			0x55
		);
	p->ai.mid1.use_tile = 0;
	p->ai.mid1.rand_rng = 6;
	p->ai.mid1.cur_building_rule = 0xFF;
	p->ai.mid1.unk6 = 2;
	p->ai.mid1.unk7 = 1;
	p->ai.mid1.buildcmd_a = 0x30;
	p->ai.mid1.buildcmd_b = 0xFF;
	p->ai.mid1.direction = p->ai.src.direction;
	p->ai.mid1.cargo = fr.cargo;

	// Fill middle field 2
	p->ai.mid2.spec_tile = AiGetPctTileBetween(
			GET_TOWN_OR_INDUSTRY_TILE(fr.from),
			GET_TOWN_OR_INDUSTRY_TILE(fr.to),
			0xAA
		);
	p->ai.mid2.use_tile = 0;
	p->ai.mid2.rand_rng = 6;
	p->ai.mid2.cur_building_rule = 0xFF;
	p->ai.mid2.unk6 = 2;
	p->ai.mid2.unk7 = 1;
	p->ai.mid2.buildcmd_a = 0xFF;
	p->ai.mid2.buildcmd_b = 0xFF;
	p->ai.mid2.direction = p->ai.dst.direction;
	p->ai.mid2.cargo = fr.cargo;

	// Fill common fields
	p->ai.cargo_type = fr.cargo;
	p->ai.num_wagons = 3;
	p->ai.build_kind = 2;
	p->ai.num_build_rec = 4;
	p->ai.num_loco_to_build = 2;
	p->ai.num_want_fullload = 0;
	p->ai.wagon_list[0] = INVALID_VEHICLE;
	p->ai.order_list_blocks[0] = 0;
	p->ai.order_list_blocks[1] = 1;
	p->ai.order_list_blocks[2] = 255;
	p->ai.state = AIS_BUILD_DEFAULT_RAIL_BLOCKS;
	p->ai.state_mode = -1;
	p->ai.state_counter = 0;
	p->ai.timeout_counter = 0;
}

static void AiWantPassengerRoute(Player *p)
{
	int i;
	FoundRoute fr;

	i = 60;
	for(;;) {

		// look for one from the subsidy list
		AiFindSubsidyPassengerRoute(&fr);
		if (IS_INT_INSIDE(fr.distance, 0, 55+1))
			break;

		// try a random one
		AiFindRandomPassengerRoute(&fr);
		if (IS_INT_INSIDE(fr.distance, 0, 55+1))
			break;

		// only test 60 times
		if (--i == 0)
			return;
	}

	fr.cargo = CT_PASSENGERS;
	if (!AiCheckIfRouteIsGood(p, &fr, 1))
		return;

	// Fill the source field
	p->ai.src.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.from);
	p->ai.src.use_tile = 0;
	p->ai.src.rand_rng = 7;
	p->ai.src.cur_building_rule = 0xFF;
	p->ai.src.unk6 = 1;
	p->ai.src.unk7 = 0;
	p->ai.src.buildcmd_a = 0x10;
	p->ai.src.buildcmd_b = 0xFF;
	p->ai.src.direction = AiGetDirectionBetweenTiles(
			GET_TOWN_OR_INDUSTRY_TILE(fr.from),
			GET_TOWN_OR_INDUSTRY_TILE(fr.to)
		);
	p->ai.src.cargo = fr.cargo;

	// Fill the dest field
	p->ai.dst.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.to);
	p->ai.dst.use_tile = 0;
	p->ai.dst.rand_rng = 7;
	p->ai.dst.cur_building_rule = 0xFF;
	p->ai.dst.unk6 = 1;
	p->ai.dst.unk7 = 0;
	p->ai.dst.buildcmd_a = 0xFF;
	p->ai.dst.buildcmd_b = 0xFF;
	p->ai.dst.direction = AiGetDirectionBetweenTiles(
			GET_TOWN_OR_INDUSTRY_TILE(fr.to),
			GET_TOWN_OR_INDUSTRY_TILE(fr.from)
		);
	p->ai.dst.cargo = fr.cargo;

	// Fill common fields
	p->ai.cargo_type = fr.cargo;
	p->ai.num_wagons = 2;
	p->ai.build_kind = 1;
	p->ai.num_build_rec = 2;
	p->ai.num_loco_to_build = 1;
	p->ai.num_want_fullload = 0;
	p->ai.wagon_list[0] = INVALID_VEHICLE;
	p->ai.order_list_blocks[0] = 0;
	p->ai.order_list_blocks[1] = 1;
	p->ai.order_list_blocks[2] = 255;
	p->ai.state = AIS_BUILD_DEFAULT_RAIL_BLOCKS;
	p->ai.state_mode = -1;
	p->ai.state_counter = 0;
	p->ai.timeout_counter = 0;
}

static void AiWantTrainRoute(Player *p)
{
	uint16 r;
	p->ai.railtype_to_use = p->max_railtype - 1;
	r = (uint16)Random();

	if (r > 0xD000) {
		AiWantLongIndustryRoute(p);
	} else if (r > 0x6000) {
		AiWantMediumIndustryRoute(p);
	} else if (r > 0x1000) {
		AiWantShortIndustryRoute(p);
	} else if (r > 0x800) {
		AiWantPassengerRoute(p);
	} else {
		AiWantMailRoute(p);
	}
}

static void AiWantLongRoadIndustryRoute(Player *p)
{
	int i;
	FoundRoute fr;

	i = 60;
	for(;;) {

		// look for one from the subsidy list
		AiFindSubsidyIndustryRoute(&fr);
		if (IS_INT_INSIDE(fr.distance, 35, 55+1))
			break;

		// try a random one
		AiFindRandomIndustryRoute(&fr);
		if (IS_INT_INSIDE(fr.distance, 35, 55+1))
			break;

		// only test 60 times
		if (--i == 0)
			return;
	}

	if (!AiCheckIfRouteIsGood(p, &fr, 2))
		return;

	// Fill the source field
	p->ai.src.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.from);
	p->ai.src.use_tile = 0;
	p->ai.src.rand_rng = 9;
	p->ai.src.cur_building_rule = 0xFF;
	p->ai.src.buildcmd_a = 1;
	p->ai.src.direction = 0;
	p->ai.src.cargo = fr.cargo | 0x80;

	// Fill the dest field
	p->ai.dst.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.to);
	p->ai.dst.use_tile = 0;
	p->ai.dst.rand_rng = 9;
	p->ai.dst.cur_building_rule = 0xFF;
	p->ai.dst.buildcmd_a = 0xFF;
	p->ai.dst.direction = 0;
	p->ai.dst.cargo = fr.cargo;

	// Fill common fields
	p->ai.cargo_type = fr.cargo;
	p->ai.num_build_rec = 2;
	p->ai.num_loco_to_build = 5;
	p->ai.num_want_fullload = 5;

//	p->ai.loco_id = INVALID_VEHICLE;
	p->ai.order_list_blocks[0] = 0;
	p->ai.order_list_blocks[1] = 1;
	p->ai.order_list_blocks[2] = 255;

	p->ai.state = AIS_BUILD_DEFAULT_ROAD_BLOCKS;
	p->ai.state_mode = -1;
	p->ai.state_counter = 0;
	p->ai.timeout_counter = 0;
}

static void AiWantMediumRoadIndustryRoute(Player *p)
{
	int i;
	FoundRoute fr;

	i = 60;
	for(;;) {

		// look for one from the subsidy list
		AiFindSubsidyIndustryRoute(&fr);
		if (IS_INT_INSIDE(fr.distance, 15, 40+1))
			break;

		// try a random one
		AiFindRandomIndustryRoute(&fr);
		if (IS_INT_INSIDE(fr.distance, 15, 40+1))
			break;

		// only test 60 times
		if (--i == 0)
			return;
	}

	if (!AiCheckIfRouteIsGood(p, &fr, 2))
		return;

	// Fill the source field
	p->ai.src.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.from);
	p->ai.src.use_tile = 0;
	p->ai.src.rand_rng = 9;
	p->ai.src.cur_building_rule = 0xFF;
	p->ai.src.buildcmd_a = 1;
	p->ai.src.direction = 0;
	p->ai.src.cargo = fr.cargo | 0x80;

	// Fill the dest field
	p->ai.dst.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.to);
	p->ai.dst.use_tile = 0;
	p->ai.dst.rand_rng = 9;
	p->ai.dst.cur_building_rule = 0xFF;
	p->ai.dst.buildcmd_a = 0xFF;
	p->ai.dst.direction = 0;
	p->ai.dst.cargo = fr.cargo;

	// Fill common fields
	p->ai.cargo_type = fr.cargo;
	p->ai.num_build_rec = 2;
	p->ai.num_loco_to_build = 3;
	p->ai.num_want_fullload = 3;

//	p->ai.loco_id = INVALID_VEHICLE;
	p->ai.order_list_blocks[0] = 0;
	p->ai.order_list_blocks[1] = 1;
	p->ai.order_list_blocks[2] = 255;

	p->ai.state = AIS_BUILD_DEFAULT_ROAD_BLOCKS;
	p->ai.state_mode = -1;
	p->ai.state_counter = 0;
	p->ai.timeout_counter = 0;
}

static void AiWantLongRoadPassengerRoute(Player *p)
{
	int i;
	FoundRoute fr;

	i = 60;
	for(;;) {

		// look for one from the subsidy list
		AiFindSubsidyPassengerRoute(&fr);
		if (IS_INT_INSIDE(fr.distance, 55, 180+1))
			break;

		// try a random one
		AiFindRandomPassengerRoute(&fr);
		if (IS_INT_INSIDE(fr.distance, 55, 180+1))
			break;

		// only test 60 times
		if (--i == 0)
			return;
	}

	fr.cargo = CT_PASSENGERS;

	if (!AiCheckIfRouteIsGood(p, &fr, 2))
		return;

	// Fill the source field
	p->ai.src.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.to);
	p->ai.src.use_tile = 0;
	p->ai.src.rand_rng = 10;
	p->ai.src.cur_building_rule = 0xFF;
	p->ai.src.buildcmd_a = 1;
	p->ai.src.direction = 0;
	p->ai.src.cargo = CT_PASSENGERS;

	// Fill the dest field
	p->ai.dst.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.from);
	p->ai.dst.use_tile = 0;
	p->ai.dst.rand_rng = 10;
	p->ai.dst.cur_building_rule = 0xFF;
	p->ai.dst.buildcmd_a = 0xFF;
	p->ai.dst.direction = 0;
	p->ai.dst.cargo = CT_PASSENGERS;

	// Fill common fields
	p->ai.cargo_type = CT_PASSENGERS;
	p->ai.num_build_rec = 2;
	p->ai.num_loco_to_build = 4;
	p->ai.num_want_fullload = 0;

//	p->ai.loco_id = INVALID_VEHICLE;
	p->ai.order_list_blocks[0] = 0;
	p->ai.order_list_blocks[1] = 1;
	p->ai.order_list_blocks[2] = 255;

	p->ai.state = AIS_BUILD_DEFAULT_ROAD_BLOCKS;
	p->ai.state_mode = -1;
	p->ai.state_counter = 0;
	p->ai.timeout_counter = 0;
}

static void AiWantPassengerRouteInsideTown(Player *p)
{
	int i;
	FoundRoute fr;
	Town *t;

	i = 60;
	for(;;) {
		// Find a town big enough
		t = AiFindRandomTown();
		if (t != NULL && t->population >= 700)
			break;

		// only test 60 times
		if (--i == 0)
			return;
	}

	fr.cargo = CT_PASSENGERS;
	fr.from = fr.to = t;

	if (!AiCheckIfRouteIsGood(p, &fr, 2))
		return;

	// Fill the source field
	p->ai.src.spec_tile = t->xy;
	p->ai.src.use_tile = 0;
	p->ai.src.rand_rng = 10;
	p->ai.src.cur_building_rule = 0xFF;
	p->ai.src.buildcmd_a = 1;
	p->ai.src.direction = 0;
	p->ai.src.cargo = CT_PASSENGERS;

	// Fill the dest field
	p->ai.dst.spec_tile = t->xy;
	p->ai.dst.use_tile = 0;
	p->ai.dst.rand_rng = 10;
	p->ai.dst.cur_building_rule = 0xFF;
	p->ai.dst.buildcmd_a = 0xFF;
	p->ai.dst.direction = 0;
	p->ai.dst.cargo = CT_PASSENGERS;

	// Fill common fields
	p->ai.cargo_type = CT_PASSENGERS;
	p->ai.num_build_rec = 2;
	p->ai.num_loco_to_build = 2;
	p->ai.num_want_fullload = 0;

//	p->ai.loco_id = INVALID_VEHICLE;
	p->ai.order_list_blocks[0] = 0;
	p->ai.order_list_blocks[1] = 1;
	p->ai.order_list_blocks[2] = 255;

	p->ai.state = AIS_BUILD_DEFAULT_ROAD_BLOCKS;
	p->ai.state_mode = -1;
	p->ai.state_counter = 0;
	p->ai.timeout_counter = 0;
}

static void AiWantRoadRoute(Player *p)
{
	uint16 r = (uint16)Random();

	if (r > 0x4000) {
		AiWantLongRoadIndustryRoute(p);
	} else if (r > 0x2000) {
		AiWantMediumRoadIndustryRoute(p);
	} else if (r > 0x1000) {
		AiWantLongRoadPassengerRoute(p);
	} else {
		AiWantPassengerRouteInsideTown(p);
	}

}

static void AiWantPassengerAircraftRoute(Player *p)
{
	FoundRoute fr;
	int i;

	i = 60;
	for(;;) {

		// look for one from the subsidy list
		AiFindSubsidyPassengerRoute(&fr);
		if (IS_INT_INSIDE(fr.distance,0,95+1))
			break;

		// try a random one
		AiFindRandomPassengerRoute(&fr);
		if (IS_INT_INSIDE(fr.distance,0,95+1))
			break;

		// only test 60 times
		if (--i == 0)
			return;
	}

	fr.cargo = CT_PASSENGERS;
	if (!AiCheckIfRouteIsGood(p, &fr, 4))
		return;


	// Fill the source field
	p->ai.src.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.to);
	p->ai.src.use_tile = 0;
	p->ai.src.rand_rng = 12;
	p->ai.src.cur_building_rule = 0xFF;
	p->ai.src.cargo = fr.cargo;

	// Fill the dest field
	p->ai.dst.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.from);
	p->ai.dst.use_tile = 0;
	p->ai.dst.rand_rng = 12;
	p->ai.dst.cur_building_rule = 0xFF;
	p->ai.dst.cargo = fr.cargo;

	// Fill common fields
	p->ai.cargo_type = fr.cargo;
	p->ai.build_kind = 0;
	p->ai.num_build_rec = 2;
	p->ai.num_loco_to_build = 1;
	p->ai.num_want_fullload = 1;
//	p->ai.loco_id = INVALID_VEHICLE;
	p->ai.order_list_blocks[0] = 0;
	p->ai.order_list_blocks[1] = 1;
	p->ai.order_list_blocks[2] = 255;

	p->ai.state = AIS_AIRPORT_STUFF;
	p->ai.timeout_counter = 0;
}

static void AiWantOilRigAircraftRoute(Player *p)
{
	int i;
	FoundRoute fr;
	Town *t;
	Industry *in;

	i = 60;
	for(;;) {
		// Find a town
		t = AiFindRandomTown();
		if (t != NULL) {
			// Find a random oil rig industry
			in = DEREF_INDUSTRY(RandomRange(lengthof(_industries)));
			if (in != NULL && in->type == IT_OIL_RIG) {
				if (GetTileDist(t->xy, in->xy) < 60)
					break;
			}
		}

		// only test 60 times
		if (--i == 0)
			return;
	}

	fr.cargo = CT_PASSENGERS;
	fr.from = fr.to = t;

	if (!AiCheckIfRouteIsGood(p, &fr, 4))
		return;

	// Fill the source field
	p->ai.src.spec_tile = t->xy;
	p->ai.src.use_tile = 0;
	p->ai.src.rand_rng = 12;
	p->ai.src.cur_building_rule = 0xFF;
	p->ai.src.cargo = CT_PASSENGERS;

	// Fill the dest field
	p->ai.dst.spec_tile = in->xy;
	p->ai.dst.use_tile = 0;
	p->ai.dst.rand_rng = 5;
	p->ai.dst.cur_building_rule = 0xFF;
	p->ai.dst.cargo = CT_PASSENGERS;

	// Fill common fields
	p->ai.cargo_type = CT_PASSENGERS;
	p->ai.build_kind = 1;
	p->ai.num_build_rec = 2;
	p->ai.num_loco_to_build = 1;
	p->ai.num_want_fullload = 0;
//	p->ai.loco_id = INVALID_VEHICLE;
	p->ai.order_list_blocks[0] = 0;
	p->ai.order_list_blocks[1] = 1;
	p->ai.order_list_blocks[2] = 255;

	p->ai.state = AIS_AIRPORT_STUFF;
	p->ai.timeout_counter = 0;
}

static void AiWantAircraftRoute(Player *p)
{
	uint16 r = (uint16)Random();

	if (r >= 0x2AAA || _date < 0x3912) {
		AiWantPassengerAircraftRoute(p);
	} else {
		AiWantOilRigAircraftRoute(p);
	}
}

static void AiWantShipRoute(Player *p)
{
	// XXX
//	error("AiWaitShipRoute");
}



static void AiStateWantNewRoute(Player *p)
{
	uint16 r;
	int i;

	if (p->player_money < AiGetBasePrice(p) * 500) {
		p->ai.state = AIS_0;
		return;
	}

	i = 200;
	for(;;) {
		r = (uint16)Random();

		if (_patches.ai_disable_veh_train && _patches.ai_disable_veh_roadveh &&
			_patches.ai_disable_veh_aircraft && _patches.ai_disable_veh_ship)
			return;

		if (r < 0x7626) {
			if (_patches.ai_disable_veh_train) continue;
			AiWantTrainRoute(p);
		} else if (r < 0xC4EA) {
			if (_patches.ai_disable_veh_roadveh) continue;
			AiWantRoadRoute(p);
		} else if (r < 0xD89B) {
			if (_patches.ai_disable_veh_aircraft) continue;
			AiWantAircraftRoute(p);
		} else {
			if (_patches.ai_disable_veh_ship) continue;
			AiWantShipRoute(p);
		}

		// got a route?
		if (p->ai.state != AIS_WANT_NEW_ROUTE)
			break;

		// time out?
		if (--i == 0) {
			if (++p->ai.state_counter == 556) p->ai.state = AIS_0;
			break;
		}
	}
}

static bool AiCheckTrackResources(TileIndex tile, const AiDefaultBlockData *p, byte cargo)
{
	uint values[NUM_CARGO];
	int w,h;
	uint tile2;

	for(;p->mode != 4;p++) if (p->mode == 1) {
		tile2 = TILE_ADD(tile, p->tileoffs);

		w = ((p->attr>>1) & 7);
		h = ((p->attr>>4) & 7);
		if (p->attr&1) intswap(w, h);

		if (cargo & 0x80) {
			GetProductionAroundTiles(values, tile2, w, h);
			return values[cargo & 0x7F] != 0;
		} else {
			GetAcceptanceAroundTiles(values, tile2, w, h);
			if (!(values[cargo] & ~7))
				return false;
			if (cargo != CT_MAIL)
				return true;
			return !!((values[cargo]>>1) & ~7);
		}
	}

	return true;
}

static int32 AiDoBuildDefaultRailTrack(TileIndex tile, const AiDefaultBlockData *p, byte flag)
{
	int32 r;
	int32 total_cost = 0;
	Town *t = NULL;
	int rating = 0;
	int i,j,k;

	for(;;) {
		// This will seldomly overflow for valid reasons. Mask it to be on the safe side.
		uint c = TILE_MASK(tile + p->tileoffs);

		_cleared_town = NULL;

		if (p->mode < 2) {
			if (p->mode == 0) {
				// Depot
				r = DoCommandByTile(c, _cur_ai_player->ai.railtype_to_use, p->attr, flag | DC_AUTO | DC_NO_WATER | DC_AI_BUILDING, CMD_BUILD_TRAIN_DEPOT);
			} else {
				// Station
				r = DoCommandByTile(c, (p->attr&1) | (p->attr>>4)<<8 | (p->attr>>1&7)<<16, _cur_ai_player->ai.railtype_to_use, flag | DC_AUTO | DC_NO_WATER | DC_AI_BUILDING, CMD_BUILD_RAILROAD_STATION);
			}

			if (r == CMD_ERROR) return CMD_ERROR;
			total_cost += r;

clear_town_stuff:;
			if (_cleared_town != NULL) {
				if (t != NULL && t != _cleared_town)
					return CMD_ERROR;
				t = _cleared_town;
				rating += _cleared_town_rating;
			}
		} else if (p->mode == 2) {
			// Rail
			if (IS_TILETYPE(c, MP_RAILWAY))
				return CMD_ERROR;

			j = p->attr;
			k = 0;

			// Build the rail
			for(i=0; i!=6; i++,j>>=1) {
				if (j&1) {
					k = i;
					r = DoCommandByTile(c, _cur_ai_player->ai.railtype_to_use, i, flag | DC_AUTO | DC_NO_WATER, CMD_BUILD_SINGLE_RAIL);
					if (r == CMD_ERROR) return CMD_ERROR;
					total_cost += r;
				}
			}

			/* signals too? */
			if (j&3) {
				// Can't build signals on a road.
				if (IS_TILETYPE(c, MP_STREET))
					return CMD_ERROR;

				if (flag & DC_EXEC) {
					j = 4 - j;
					do {
						r = DoCommandByTile(c, k, 0, flag, CMD_BUILD_SIGNALS);
					} while (--j);
				} else {
					r = _price.build_signals;
				}
				if (r == CMD_ERROR) return CMD_ERROR;
				total_cost += r;
			}
		} else if (p->mode == 3) {
			//Clear stuff and then build single rail.
			if (GetTileSlope(c,NULL) != 0)
				return CMD_ERROR;
			r = DoCommandByTile(c, 0, 0, flag | DC_AUTO | DC_NO_WATER | DC_AI_BUILDING, CMD_LANDSCAPE_CLEAR);
			if (r == CMD_ERROR) return CMD_ERROR;
			total_cost += r + _price.build_rail;

			if (flag & DC_EXEC) {
				DoCommandByTile(c, _cur_ai_player->ai.railtype_to_use, p->attr&1, flag | DC_AUTO | DC_NO_WATER | DC_AI_BUILDING, CMD_BUILD_SINGLE_RAIL);
			}

			goto clear_town_stuff;
		} else {
			// Unk
			break;
		}

		p++;
	}

	if (!(flag & DC_EXEC)) {
		if (t != NULL && rating > t->ratings[_current_player]) {
			return CMD_ERROR;
		}
	}

	return total_cost;
}

// Returns rule and cost
static int AiBuildDefaultRailTrack(TileIndex tile, byte p0, byte p1, byte p2, byte p3, byte dir, byte cargo, int32 *cost)
{
	int i;
	const AiDefaultRailBlock *p;

	for(i=0; (p = _default_rail_track_data[i]) != NULL; i++) {
		if (p->p0 == p0 && p->p1 == p1 && p->p2 == p2 && p->p3 == p3 &&
				(p->dir == 0xFF || p->dir == dir || ((p->dir-1)&3) == dir)) {
			*cost = AiDoBuildDefaultRailTrack(tile, p->data, DC_NO_TOWN_RATING);
			if (*cost != CMD_ERROR && AiCheckTrackResources(tile, p->data, cargo))
				return i;
		}
	}

	return -1;
}

static const byte _terraform_up_flags[] = {
	14, 13, 12, 11,
	10, 9, 8, 7,
	6, 5, 4, 3,
	2, 1, 0, 1,
	2, 1, 4, 1,
	2, 1, 8, 1,
	2, 1, 4, 2,
	2, 1
};

static const byte _terraform_down_flags[] = {
	1, 2, 3, 4,
	5, 6, 1, 8,
	9, 10, 8, 12,
	4, 2, 0, 0,
	1, 2, 3, 4,
	5, 6, 2, 8,
	9, 10, 1, 12,
	8, 4
};

static void AiDoTerraformLand(TileIndex tile, int dir, int unk, int mode)
{
	byte old_player;
	uint32 r;
	uint slope;
	int h;

	old_player = _current_player;
	_current_player = OWNER_NONE;

	r = Random();

	unk &= (int)r;

	do {
		tile = TILE_MASK(tile + _tileoffs_by_dir[dir]);

		r >>= 2;
		if (r&2) {
			dir++;
			if (r&1)
				dir -= 2;
		}
		dir &= 3;
	} while (--unk >= 0);

	slope = GetTileSlope(tile, &h);

	if (slope != 0) {
		if (mode > 0 || (mode == 0 && !(r&0xC))) {
			// Terraform up
			DoCommandByTile(tile, _terraform_up_flags[slope-1], 1,
				DC_EXEC | DC_AUTO | DC_NO_WATER, CMD_TERRAFORM_LAND);
		} else if (h != 0) {
			// Terraform down
			DoCommandByTile(tile, _terraform_down_flags[slope-1], 0,
				DC_EXEC | DC_AUTO | DC_NO_WATER, CMD_TERRAFORM_LAND);
		}
	}

	_current_player = old_player;
}

static void AiStateBuildDefaultRailBlocks(Player *p)
{
	int i, j;
	AiBuildRec *aib;
	int rule;
	int32 cost;

	// time out?
	if (++p->ai.timeout_counter == 1388) {
		p->ai.state = AIS_DELETE_RAIL_BLOCKS;
		return;
	}

	// do the following 8 times
	i = 8;
	do {
		// check if we can build the default track
		aib = &p->ai.src;
		j = p->ai.num_build_rec;
		do {
			// this item has already been built?
			if (aib->cur_building_rule != 255)
				continue;

			// adjust the coordinate randomly,
			// to make sure that we find a position.
			aib->use_tile = AdjustTileCoordRandomly(aib->spec_tile, aib->rand_rng);

			// check if the track can be build there.
			rule = AiBuildDefaultRailTrack(aib->use_tile,
				p->ai.build_kind, p->ai.num_wagons,
				aib->unk6, aib->unk7,
				aib->direction, aib->cargo, &cost);

			if (rule == -1) {
				// cannot build, terraform after a while
				if (p->ai.state_counter >= 600) {
					AiDoTerraformLand(aib->use_tile, Random()&3, 3, (int8)p->ai.state_mode);
				}
				// also try the other terraform direction
				if (++p->ai.state_counter >= 1000) {
					p->ai.state_counter = 0;
					p->ai.state_mode = -p->ai.state_mode;
				}
			} else if (CheckPlayerHasMoney(cost)) {
				int32 r;
				// player has money, build it.
				aib->cur_building_rule = rule;

				r = AiDoBuildDefaultRailTrack(
					aib->use_tile,
					_default_rail_track_data[rule]->data,
					DC_EXEC | DC_NO_TOWN_RATING
				);
				assert(r != CMD_ERROR);
			}
		} while (++aib,--j);
	} while (--i);

	// check if we're done with all of them
	aib = &p->ai.src;
	j = p->ai.num_build_rec;
	do {
		if (aib->cur_building_rule == 255)
			return;
	} while (++aib,--j);

	// yep, all are done. switch state to the rail building state.
	p->ai.state = AIS_BUILD_RAIL;
	p->ai.state_mode = 255;
}

static TileIndex AiGetEdgeOfDefaultRailBlock(byte rule, TileIndex tile, byte cmd, int *dir)
{
	const AiDefaultBlockData *p = _default_rail_track_data[rule]->data;

	while (p->mode != 3 || !((--cmd) & 0x80)) p++;

	return tile + p->tileoffs - _tileoffs_by_dir[*dir = p->attr];
}

typedef struct AiRailPathFindData {
	TileIndex tile;
	TileIndex tile2;
	int count;
	bool flag;
} AiRailPathFindData;

static bool AiEnumFollowTrack(uint tile, AiRailPathFindData *a, int track, uint length, byte *state)
{
	if (a->flag)
		return true;

	if (length > 20 || tile == a->tile) {
		a->flag = true;
		return true;
	}

	if (GetTileDist1D(tile, a->tile2) < 4)
		a->count++;

	return false;
}

static bool AiDoFollowTrack(Player *p)
{
	AiRailPathFindData arpfd;
	arpfd.tile = p->ai.start_tile_a;
	arpfd.tile2 = p->ai.cur_tile_a;
	arpfd.flag = false;
	arpfd.count = 0;
	FollowTrack(p->ai.cur_tile_a + _tileoffs_by_dir[p->ai.cur_dir_a], 0x2000 | TRANSPORT_RAIL, p->ai.cur_dir_a^2,
		(TPFEnumProc*)AiEnumFollowTrack, NULL, &arpfd);
	return arpfd.count > 8;
}

typedef struct AiRailFinder {
	TileIndex final_tile;
	byte final_dir;
	byte depth;
	byte recursive_mode;
	byte cur_best_dir;
	byte best_dir;
	byte cur_best_depth;
	byte best_depth;
	uint cur_best_dist;
	const byte *best_ptr;
	uint best_dist;
	TileIndex cur_best_tile, best_tile;
	TileIndex bridge_end_tile;
	Player *player;
	TileInfo ti;
} AiRailFinder;

static const byte _ai_table_15[4][8] = {
	{0, 0, 4, 3, 3, 1, 128+0, 64},
	{1, 1, 2, 0, 4, 2, 128+1, 65},
	{0, 2, 2, 3, 5, 1, 128+2, 66},
	{1, 3, 5, 0, 3, 2, 128+3, 67}
};

static const byte _dir_table_1[] = {3, 9, 12, 6};
static const byte _dir_table_2[] = {12, 6, 3, 9};


static bool AiIsTileBanned(Player *p, TileIndex tile, byte val) {
	int i;

	for(i=0; i!=p->ai.banned_tile_count; i++)
		if (p->ai.banned_tiles[i] == tile &&
				p->ai.banned_val[i] == val)
					return true;
	return false;
}

static void AiBanTile(Player *p, TileIndex tile, byte val) {
	int i;

	for(i=lengthof(p->ai.banned_tiles)-1; i!=0; i--) {
		p->ai.banned_tiles[i] = p->ai.banned_tiles[i-1];
		p->ai.banned_val[i] = p->ai.banned_val[i-1];
	}

	p->ai.banned_tiles[0] = tile;
	p->ai.banned_val[0] = val;

	if (p->ai.banned_tile_count != lengthof(p->ai.banned_tiles))
		p->ai.banned_tile_count++;
}

static void AiBuildRailRecursive(AiRailFinder *arf, TileIndex tile, int dir);

static bool AiCheckRailPathBetter(AiRailFinder *arf, const byte *p)
{
	bool better = false;

	if (arf->recursive_mode < 1) {
		// Mode is 0. This means destination has not been found yet.
		// If the found path is shorter than the current one, remember it.
		if (arf->cur_best_dist < arf->best_dist) {
			arf->best_dir = arf->cur_best_dir;
			arf->best_dist = arf->cur_best_dist;
			arf->best_ptr = p;
			arf->best_tile = arf->cur_best_tile;
			better = true;
		}
	} else if (arf->recursive_mode > 1) {
		// Mode is 2.
		if (arf->best_dist != 0 || arf->cur_best_depth < arf->best_depth) {
			arf->best_depth = arf->cur_best_depth;
			arf->best_dist = 0;
			arf->best_ptr = p;
			arf->best_tile = 0;
			better = true;
		}
	}
	arf->recursive_mode = 0;
	arf->cur_best_dist = (uint)-1;
	arf->cur_best_depth = 0xff;

	return better;
}

static inline void AiCheckBuildRailBridgeHere(AiRailFinder *arf, TileIndex tile, const byte *p)
{
	TileIndex tile_new;
	bool flag;

	int dir2 = p[0] & 3;

	FindLandscapeHeightByTile(&arf->ti, tile);

	if (arf->ti.tileh == _dir_table_1[dir2] || (arf->ti.tileh==0 && arf->ti.z!=0)) {
		tile_new = tile;
		// Allow bridges directly over bottom tiles
		flag = arf->ti.z == 0;
		for(;;) {
			if (tile_new < -_tileoffs_by_dir[dir2]) return; // Wraping around map, no bridge possible!
			tile_new = TILE_MASK(tile_new + _tileoffs_by_dir[dir2]);
			FindLandscapeHeightByTile(&arf->ti, tile_new);
			if (arf->ti.tileh != 0 || arf->ti.type == MP_CLEAR || arf->ti.type == MP_TREES) {
				if (!flag) return;
				break;
			}
			if (arf->ti.type != MP_WATER && arf->ti.type != MP_RAILWAY && arf->ti.type != MP_STREET)
				return;
			flag = true;
		}

		// Is building a (rail)bridge possible at this place (type doesn't matter)?
		if (DoCommandByTile(tile_new, tile, arf->player->ai.railtype_to_use<<8,
			DC_AUTO, CMD_BUILD_BRIDGE) == CMD_ERROR)
				return;
		AiBuildRailRecursive(arf, tile_new, dir2);

		// At the bottom depth, check if the new path is better than the old one.
		if (arf->depth == 1) {
			if (AiCheckRailPathBetter(arf, p))
				arf->bridge_end_tile = tile_new;
		}
	}
}

static inline void AiCheckBuildRailTunnelHere(AiRailFinder *arf, TileIndex tile, const byte *p)
{
	FindLandscapeHeightByTile(&arf->ti, tile);

	if (arf->ti.tileh == _dir_table_2[p[0]&3] && arf->ti.z!=0) {
		int32 cost = DoCommandByTile(tile, arf->player->ai.railtype_to_use, 0, DC_AUTO, CMD_BUILD_TUNNEL);

		if (cost != CMD_ERROR && cost <= (arf->player->player_money>>4)) {
			AiBuildRailRecursive(arf, _build_tunnel_endtile, p[0]&3);
			if (arf->depth == 1) {
				AiCheckRailPathBetter(arf, p);
			}
		}
	}
}


static void AiBuildRailRecursive(AiRailFinder *arf, TileIndex tile, int dir)
{
	const byte *p;

	tile = TILE_MASK(tile + _tileoffs_by_dir[dir]);

	// Reached destination?
	if (tile == arf->final_tile) {
		if (arf->final_dir != (dir^2)) {
			if (arf->recursive_mode != 2)
				arf->recursive_mode = 1;
		} else if (arf->recursive_mode != 2) {
			arf->recursive_mode = 2;
			arf->cur_best_depth = arf->depth;
		} else {
			if (arf->depth < arf->cur_best_depth)
				arf->cur_best_depth = arf->depth;
		}
		return;
	}

	// Depth too deep?
	if (arf->depth >= 4) {
		uint dist = GetTileDist1Db(tile, arf->final_tile);
		if (dist < arf->cur_best_dist) {
			// Store the tile that is closest to the final position.
			arf->cur_best_depth = arf->depth;
			arf->cur_best_dist = dist;
			arf->cur_best_tile = tile;
			arf->cur_best_dir = dir;
		}
		return;
	}

	// Increase recursion depth
	arf->depth++;

	// Grab pointer to list of stuff that is possible to build
	p = _ai_table_15[dir];

	// Try to build a single rail in all directions.
	FindLandscapeHeightByTile(&arf->ti, tile);
	if (arf->ti.z == 0) {
		p += 6;
	} else {
		do {
			// Make sure the tile is not in the list of banned tiles and that a rail can be built here.
			if (!AiIsTileBanned(arf->player, tile, p[0]) &&
					DoCommandByTile(tile, arf->player->ai.railtype_to_use, p[0], DC_AUTO | DC_NO_WATER | DC_NO_RAIL_OVERLAP, CMD_BUILD_SINGLE_RAIL) != CMD_ERROR) {
				AiBuildRailRecursive(arf, tile, p[1]);
			}

			// At the bottom depth?
			if (arf->depth == 1) {
				AiCheckRailPathBetter(arf, p);
			}

			p += 2;
		} while (!(p[0]&0x80));
	}

	AiCheckBuildRailBridgeHere(arf, tile, p);
	AiCheckBuildRailTunnelHere(arf, tile, p+1);

	arf->depth--;
}


static const byte _dir_table_3[]= {0x25, 0x2A, 0x19, 0x16};

static void AiBuildRailConstruct(Player *p)
{
	AiRailFinder arf;
	int i;

	// Check too much lookahead?
	if (AiDoFollowTrack(p)) {
		p->ai.state_counter = (Random()&0xE)+6; // Destruct this amount of blocks
		p->ai.state_mode = 1; // Start destruct

		// Ban this tile and don't reach it for a while.
		AiBanTile(p, p->ai.cur_tile_a, FindFirstBit(GetRailTrackStatus(p->ai.cur_tile_a)));
		return;
	}

	// Setup recursive finder and call it.
	arf.player = p;
	arf.final_tile = p->ai.cur_tile_b;
	arf.final_dir = p->ai.cur_dir_b;
	arf.depth = 0;
	arf.recursive_mode = 0;
	arf.best_ptr = NULL;
	arf.cur_best_dist = (uint)-1;
	arf.cur_best_depth = 0xff;
	arf.best_dist = (uint)-1;
	arf.best_depth = 0xff;
	arf.cur_best_tile = 0;
	arf.best_tile = 0;
	AiBuildRailRecursive(&arf, p->ai.cur_tile_a, p->ai.cur_dir_a);

	// Reached destination?
	if (arf.recursive_mode == 2 && arf.cur_best_depth == 0) {
		p->ai.state_mode = 255;
		return;
	}

	// Didn't find anything to build?
	if (arf.best_ptr == NULL) {
		// Terraform some
		for(i=0; i!=5; i++)
			AiDoTerraformLand(p->ai.cur_tile_a, p->ai.cur_dir_a, 3, 0);

		if (++p->ai.state_counter == 21) {
			p->ai.state_counter = 40;
			p->ai.state_mode = 1;

			// Ban this tile
			AiBanTile(p, p->ai.cur_tile_a, FindFirstBit(GetRailTrackStatus(p->ai.cur_tile_a)));
		}
		return;
	}

	p->ai.cur_tile_a += _tileoffs_by_dir[p->ai.cur_dir_a];

	if (arf.best_ptr[0]&0x80) {
		int i;
		int32 bridge_len = GetBridgeLength(arf.bridge_end_tile, p->ai.cur_tile_a);

		/*	Figure out what (rail)bridge type to build
				start with best bridge, then go down to worse and worse bridges
				unnecessary to check for worse bridge (i=0), since AI will always build that.
				AI is so fucked up that fixing this small thing will probably not solve a thing
		*/
		for(i = 10 + (p->ai.railtype_to_use << 8); i != 0; i--) {
			if (CheckBridge_Stuff(i, bridge_len)) {
				int32 cost = DoCommandByTile(arf.bridge_end_tile, p->ai.cur_tile_a, i, DC_AUTO, CMD_BUILD_BRIDGE);
				if (cost != CMD_ERROR && cost < (p->player_money >> 5))
					break;
			}
		}

		// Build it
		DoCommandByTile(arf.bridge_end_tile, p->ai.cur_tile_a, i, DC_AUTO | DC_EXEC, CMD_BUILD_BRIDGE);

		p->ai.cur_tile_a = arf.bridge_end_tile;
		p->ai.state_counter = 0;
	} else if (arf.best_ptr[0]&0x40) {
		// tunnel
		DoCommandByTile(p->ai.cur_tile_a, p->ai.railtype_to_use, 0, DC_AUTO | DC_EXEC, CMD_BUILD_TUNNEL);
		p->ai.cur_tile_a = _build_tunnel_endtile;
		p->ai.state_counter = 0;
	} else {
		// rail
		p->ai.cur_dir_a = arf.best_ptr[1];
		DoCommandByTile(p->ai.cur_tile_a, p->ai.railtype_to_use, arf.best_ptr[0],
			DC_EXEC | DC_AUTO | DC_NO_WATER | DC_NO_RAIL_OVERLAP, CMD_BUILD_SINGLE_RAIL);
		p->ai.state_counter = 0;
	}

	if (arf.best_tile != 0) {
		for(i=0; i!=2; i++)
			AiDoTerraformLand(arf.best_tile, arf.best_dir, 3, 0);
	}
}

static bool AiRemoveTileAndGoForward(Player *p)
{
	byte b;
	int bit;
	const byte *ptr;
	uint tile = p->ai.cur_tile_a;
	int offs;
	TileIndex tilenew;

	if (IS_TILETYPE(tile, MP_TUNNELBRIDGE)) {
		if (!(_map5[tile] & 0x80)) {
			// Clear the tunnel and continue at the other side of it.
			if (DoCommandByTile(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR) == CMD_ERROR)
				return false;
			p->ai.cur_tile_a = TILE_MASK(_build_tunnel_endtile - _tileoffs_by_dir[p->ai.cur_dir_a]);
			return true;
		}

		if (!(_map5[tile] & 0x40)) {

			// Check if the bridge points in the right direction.
			// This is not really needed the first place AiRemoveTileAndGoForward is called.
			if ((_map5[tile]&1) != (p->ai.cur_dir_a&1))
				return false;

			// Find other side of bridge.
			offs = _tileoffs_by_dir[p->ai.cur_dir_a];
			do {
				tile = TILE_MASK(tile - offs);
			} while (_map5[tile] & 0x40);

			tilenew = TILE_MASK(tile - offs);
			// And clear the bridge.
			if (DoCommandByTile(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR) == CMD_ERROR)
				return false;
			p->ai.cur_tile_a = tilenew;
			return true;
		}
	}

	// Find the railtype at the position. Quit if no rail there.
	b = GetRailTrackStatus(tile) & _dir_table_3[p->ai.cur_dir_a];
	if (b == 0)
		return false;

	// Convert into a bit position that CMD_REMOVE_SINGLE_RAIL expects.
	bit = FindFirstBit(b);

	// Then remove and signals if there are any.
	if (IS_TILETYPE(tile, MP_RAILWAY) &&
			(_map5[tile]&0xC0) == 0x40) {
		DoCommandByTile(tile, 0, 0, DC_EXEC, CMD_REMOVE_SIGNALS);
	}

	// And also remove the rail.
	if (DoCommandByTile(tile, 0, bit, DC_EXEC, CMD_REMOVE_SINGLE_RAIL) == CMD_ERROR)
		return false;

	// Find the direction at the other edge of the rail.
	ptr = _ai_table_15[p->ai.cur_dir_a^2];
	while (ptr[0] != bit) ptr+=2;
	p->ai.cur_dir_a = ptr[1] ^ 2;

	// And then also switch tile.
	p->ai.cur_tile_a = TILE_MASK(p->ai.cur_tile_a - _tileoffs_by_dir[p->ai.cur_dir_a]);

	return true;
}


static void AiBuildRailDestruct(Player *p)
{
	// Decrease timeout.
	if (!--p->ai.state_counter) {
		p->ai.state_mode = 2;
		p->ai.state_counter = 0;
	}

	// Don't do anything if the destination is already reached.
	if (p->ai.cur_tile_a == p->ai.start_tile_a)
		return;

	AiRemoveTileAndGoForward(p);
}


static void AiBuildRail(Player *p)
{
	int i;

	if (p->ai.state_mode < 1) {
		// Construct mode, build new rail.
		AiBuildRailConstruct(p);
	} else if (p->ai.state_mode == 1) {

		// Destruct mode, destroy the rail currently built.
		AiBuildRailDestruct(p);
	} else if (p->ai.state_mode == 2) {

		// Terraform some and then try building again.
		for(i=0; i!=4; i++)
			AiDoTerraformLand(p->ai.cur_tile_a, p->ai.cur_dir_a, 3, 0);

		if (++p->ai.state_counter == 4) {
			p->ai.state_counter = 0;
			p->ai.state_mode = 0;
		}
	}
}

static void AiStateBuildRail(Player *p)
{
	int num;
	AiBuildRec *aib;
	byte cmd;
	TileIndex tile;
	int dir;

	// time out?
	if (++p->ai.timeout_counter == 1388) {
		p->ai.state = AIS_DELETE_RAIL_BLOCKS;
		return;
	}

	// Currently building a rail between two points?
	if (p->ai.state_mode != 255) {
		AiBuildRail(p);

		// Alternate between edges
		swap_tile(&p->ai.start_tile_a, &p->ai.start_tile_b);
		swap_tile(&p->ai.cur_tile_a, &p->ai.cur_tile_b);
		swap_byte(&p->ai.start_dir_a, &p->ai.start_dir_b);
		swap_byte(&p->ai.cur_dir_a, &p->ai.cur_dir_b);
		return;
	}

	// Now, find two new points to build between
	num = p->ai.num_build_rec;
	aib = &p->ai.src;

	for(;;) {
		cmd = aib->buildcmd_a;
		aib->buildcmd_a = 255;
		if (cmd != 255) break;

		cmd = aib->buildcmd_b;
		aib->buildcmd_b = 255;
		if (cmd != 255) break;

		aib++;
		if (--num == 0) {
			p->ai.state = AIS_BUILD_RAIL_VEH;
			p->ai.state_counter = 0; // timeout
			return;
		}
	}

	// Find first edge to build from.
	tile = AiGetEdgeOfDefaultRailBlock(aib->cur_building_rule, aib->use_tile, cmd&3, &dir);
	p->ai.start_tile_a = tile;
	p->ai.cur_tile_a = tile;
	p->ai.start_dir_a = dir;
	p->ai.cur_dir_a = dir;
	DoCommandByTile(TILE_MASK(tile + _tileoffs_by_dir[dir]), 0, (dir&1)?1:0, DC_EXEC, CMD_REMOVE_SINGLE_RAIL);

	assert(TILE_MASK(tile) != 0xFF00);

	// Find second edge to build to
	aib = (&p->ai.src) + ((cmd >> 4)&0xF);
	tile = AiGetEdgeOfDefaultRailBlock(aib->cur_building_rule, aib->use_tile, (cmd>>2)&3, &dir);
	p->ai.start_tile_b = tile;
	p->ai.cur_tile_b = tile;
	p->ai.start_dir_b = dir;
	p->ai.cur_dir_b = dir;
	DoCommandByTile(TILE_MASK(tile + _tileoffs_by_dir[dir]), 0, (dir&1)?1:0, DC_EXEC, CMD_REMOVE_SINGLE_RAIL);

	assert(TILE_MASK(tile) != 0xFF00);

	// And setup state.
	p->ai.state_mode = 2;
	p->ai.state_counter = 0;
	p->ai.banned_tile_count = 0;
}

static int AiGetStationIdByDef(TileIndex tile, int id)
{
	const AiDefaultBlockData *p = _default_rail_track_data[id]->data;
	while (p->mode != 1) p++;
	return _map2[TILE_ADD(tile,p->tileoffs)];
}

static void AiStateBuildRailVeh(Player *p)
{
	const AiDefaultBlockData *ptr;
	TileIndex tile;
	int i, veh;
	int cargo;
	int32 cost;
	Vehicle *v;
	uint loco_id;

	ptr = _default_rail_track_data[p->ai.src.cur_building_rule]->data;
	while (ptr->mode != 0) {	ptr++; }

	tile = TILE_ADD(p->ai.src.use_tile, ptr->tileoffs);

	cargo = p->ai.cargo_type;
	for(i=0;;) {
		if (p->ai.wagon_list[i] == INVALID_VEHICLE) {
			veh = _cargoc.ai_railwagon[p->ai.railtype_to_use][cargo];
			cost = DoCommandByTile(tile, veh, 0, DC_EXEC, CMD_BUILD_RAIL_VEHICLE);
			if (cost == CMD_ERROR) goto handle_nocash;
			p->ai.wagon_list[i] = _new_wagon_id;
			p->ai.wagon_list[i+1] = INVALID_VEHICLE;
			return;
		}
		if (cargo == CT_MAIL)
			cargo = CT_PASSENGERS;
		if (++i == p->ai.num_wagons * 2 - 1)
			break;
	}

	// Which locomotive to build?
	veh = AiChooseTrainToBuild(p->ai.railtype_to_use, p->player_money, (cargo!=CT_PASSENGERS)?1:0);
	if (veh == -1) {
handle_nocash:
		// after a while, if AI still doesn't have cash, get out of this block by selling the wagons.
		if (++p->ai.state_counter == 1000) {
			for(i=0; p->ai.wagon_list[i] != INVALID_VEHICLE; i++) {
				cost = DoCommandByTile(tile, p->ai.wagon_list[i], 0, DC_EXEC, CMD_SELL_RAIL_WAGON);
				assert(cost != CMD_ERROR);
			}
			p->ai.state =	AIS_0;
		}
		return;
	}

	// Try to build the locomotive
	cost = DoCommandByTile(tile, veh, 0, DC_EXEC, CMD_BUILD_RAIL_VEHICLE);
	assert(cost != CMD_ERROR);
	loco_id = _new_train_id;

	// Sell a vehicle if the train is double headed.
	v = &_vehicles[loco_id];
	if (v->next != NULL) {
		i = p->ai.wagon_list[p->ai.num_wagons*2-2];
		p->ai.wagon_list[p->ai.num_wagons*2-2] = INVALID_VEHICLE;
		DoCommandByTile(tile, i, 0, DC_EXEC, CMD_SELL_RAIL_WAGON);
	}

	// Move the wagons onto the train
	for(i=0; p->ai.wagon_list[i] != INVALID_VEHICLE; i++) {
		DoCommandByTile(tile, p->ai.wagon_list[i] | (loco_id << 16), 0, DC_EXEC, CMD_MOVE_RAIL_VEHICLE);
	}

	for(i=0; p->ai.order_list_blocks[i] != 0xFF; i++) {
		AiBuildRec *aib = (&p->ai.src) + p->ai.order_list_blocks[i];
		bool is_pass = (p->ai.cargo_type == CT_PASSENGERS ||
							p->ai.cargo_type == CT_MAIL ||
							(_opt.landscape==LT_NORMAL && p->ai.cargo_type == CT_VALUABLES));
		Order order;

		order.type = OT_GOTO_STATION;
		order.flags = 0;
		order.station = AiGetStationIdByDef(aib->use_tile, aib->cur_building_rule);

		if (!is_pass && i == 1) order.flags |= OF_UNLOAD;
		if (p->ai.num_want_fullload != 0 && (is_pass || i == 0))
			order.flags |= OF_FULL_LOAD;

		DoCommandByTile(0, loco_id + (i << 16),	PackOrder(&order), DC_EXEC, CMD_INSERT_ORDER);
	}

	DoCommandByTile(0, loco_id, 0, DC_EXEC, CMD_START_STOP_TRAIN);

	DoCommandByTile(0, loco_id, _ai_service_interval, DC_EXEC, CMD_CHANGE_TRAIN_SERVICE_INT);

	if (p->ai.num_want_fullload != 0)
		p->ai.num_want_fullload--;

	if (--p->ai.num_loco_to_build != 0) {
//		p->ai.loco_id = INVALID_VEHICLE;
		p->ai.wagon_list[0] = INVALID_VEHICLE;
	} else {
		p->ai.state =	AIS_0;
	}
}

static void AiStateDeleteRailBlocks(Player *p)
{
	int num;
	AiBuildRec *aib;
	const AiDefaultBlockData *b;

	num = p->ai.num_build_rec;
	aib = &p->ai.src;
	do {
		if (aib->cur_building_rule != 255) {
			b = _default_rail_track_data[aib->cur_building_rule]->data;
			while (b->mode != 4) {
				DoCommandByTile(TILE_ADD(aib->use_tile, b->tileoffs), 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
				b++;
			}
		}
	} while (++aib,--num);

	p->ai.state = AIS_0;
}

static bool AiCheckRoadResources(TileIndex tile, const AiDefaultBlockData *p, byte cargo)
{
	uint values[NUM_CARGO];
	for(;;p++) {
		if (p->mode == 4) {
			return true;
		} else if (p->mode == 1) {
			uint tile2 = TILE_ADD(tile, p->tileoffs);
			if (cargo & 0x80) {
				GetProductionAroundTiles(values, tile2, 1, 1);
				return values[cargo & 0x7F] != 0;
			} else {
				GetAcceptanceAroundTiles(values, tile2, 1, 1);
				return (values[cargo]&~7) != 0;
			}
		}
	}
}

static bool _want_road_truck_station;
static int32 AiDoBuildDefaultRoadBlock(TileIndex tile, const AiDefaultBlockData *p, byte flag);

// Returns rule and cost
static int AiFindBestDefaultRoadBlock(TileIndex tile, byte direction, byte cargo, int32 *cost)
{
	int i;
	const AiDefaultRoadBlock *p;

	_want_road_truck_station = (cargo & 0x7F) != CT_PASSENGERS;

	for(i=0; (p = _road_default_block_data[i]) != NULL; i++) {
		if (p->dir == direction) {
			*cost = AiDoBuildDefaultRoadBlock(tile, p->data, 0);
			if (*cost != CMD_ERROR && AiCheckRoadResources(tile, p->data, cargo))
				return i;
		}
	}

	return -1;
}

static int32 AiDoBuildDefaultRoadBlock(TileIndex tile, const AiDefaultBlockData *p, byte flag)
{
	int32 r;
	int32 total_cost = 0;
	Town *t = NULL;
	int rating = 0;
	int roadflag = 0;

	for(;p->mode != 4;p++) {
		uint c = TILE_MASK(tile+ p->tileoffs);

		_cleared_town = NULL;

		if (p->mode == 2) {
			if (IS_TILETYPE(c, MP_STREET) &&
					(_map5[c]&0xF0)==0 &&
					(_map5[c]&p->attr)!=0) {
				roadflag |= 2;

				// all bits are already built?
				if ((_map5[c]&p->attr)==p->attr)
					continue;
			}

			r = DoCommandByTile(c, p->attr, 0, flag | DC_AUTO | DC_NO_WATER, CMD_BUILD_ROAD);
			if (r == CMD_ERROR)
				return CMD_ERROR;
			total_cost += r;

			continue;
		}

		if (p->mode == 0) {
			// Depot
			r = DoCommandByTile(c, p->attr, 0, flag | DC_AUTO | DC_NO_WATER | DC_AI_BUILDING, CMD_BUILD_ROAD_DEPOT);
			goto clear_town_stuff;
		} else if (p->mode == 1) {
			if (_want_road_truck_station) {
				// Truck station
				r = DoCommandByTile(c, p->attr, 0, flag | DC_AUTO | DC_NO_WATER | DC_AI_BUILDING, CMD_BUILD_TRUCK_STATION);
			} else {
				// Bus station
				r = DoCommandByTile(c, p->attr, 0, flag | DC_AUTO | DC_NO_WATER | DC_AI_BUILDING, CMD_BUILD_BUS_STATION);
			}
clear_town_stuff:;

			if (r == CMD_ERROR) return CMD_ERROR;
			total_cost += r;

			if (_cleared_town != NULL) {
				if (t != NULL && t != _cleared_town)
					return CMD_ERROR;
				t = _cleared_town;
				rating += _cleared_town_rating;
			}
		} else if (p->mode == 3) {
			if (flag & DC_EXEC)
				continue;

			if (GetTileSlope(c, NULL) != 0)
				return CMD_ERROR;

			if (!(IS_TILETYPE(c, MP_STREET) && (_map5[c]&0xF0)==0)) {
				r = DoCommandByTile(c, 0, 0, flag | DC_AUTO | DC_NO_WATER | DC_AI_BUILDING, CMD_LANDSCAPE_CLEAR);
				if (r == CMD_ERROR) return CMD_ERROR;
			}

		}
	}

	if (!_want_road_truck_station && !(roadflag&2))
		return CMD_ERROR;

	if (!(flag & DC_EXEC)) {
		if (t != NULL && rating > t->ratings[_current_player]) {
			return CMD_ERROR;
		}
	}
	return total_cost;
}

// Make sure the blocks are not too close to each other
static bool AiCheckBlockDistances(Player *p, TileIndex tile)
{
	AiBuildRec *aib;
	int num;

	num = p->ai.num_build_rec;
	aib = &p->ai.src;

	do {
		if (aib->cur_building_rule != 255) {
			if (GetTileDist(aib->use_tile, tile) < 9)
				return false;
		}
	} while (++aib, --num);

	return true;
}


static void AiStateBuildDefaultRoadBlocks(Player *p)
{
	int i, j;
	AiBuildRec *aib;
	int rule;
	int32 cost;

	// time out?
	if (++p->ai.timeout_counter == 1388) {
		p->ai.state = AIS_DELETE_RAIL_BLOCKS;
		return;
	}

	// do the following 8 times
	i = 8;
	do {
		// check if we can build the default track
		aib = &p->ai.src;
		j = p->ai.num_build_rec;
		do {
			// this item has already been built?
			if (aib->cur_building_rule != 255)
				continue;

			// adjust the coordinate randomly,
			// to make sure that we find a position.
			aib->use_tile = AdjustTileCoordRandomly(aib->spec_tile, aib->rand_rng);

			// check if the road can be built there.
			rule = AiFindBestDefaultRoadBlock(aib->use_tile,
				aib->direction, aib->cargo, &cost);

			if (rule == -1) {
				// cannot build, terraform after a while
				if (p->ai.state_counter >= 600) {
					AiDoTerraformLand(aib->use_tile, Random()&3, 3, (int8)p->ai.state_mode);
				}
				// also try the other terraform direction
				if (++p->ai.state_counter >= 1000) {
					p->ai.state_counter = 0;
					p->ai.state_mode = -p->ai.state_mode;
				}
			} else if (CheckPlayerHasMoney(cost) && AiCheckBlockDistances(p,aib->use_tile)) {
				int32 r;

				// player has money, build it.
				aib->cur_building_rule = rule;

				r = AiDoBuildDefaultRoadBlock(
					aib->use_tile,
					_road_default_block_data[rule]->data,
					DC_EXEC | DC_NO_TOWN_RATING
				);
				assert(r != CMD_ERROR);
			}
		} while (++aib,--j);
	} while (--i);

	// check if we're done with all of them
	aib = &p->ai.src;
	j = p->ai.num_build_rec;
	do {
		if (aib->cur_building_rule == 255)
			return;
	} while (++aib,--j);

	// yep, all are done. switch state to the rail building state.
	p->ai.state = AIS_BUILD_ROAD;
	p->ai.state_mode = 255;
}

typedef struct {
	TileIndex final_tile;
	byte final_dir;
	byte depth;
	byte recursive_mode;
	byte cur_best_dir;
	byte best_dir;
	byte cur_best_depth;
	byte best_depth;
	uint cur_best_dist;
	const byte *best_ptr;
	uint best_dist;
	TileIndex cur_best_tile, best_tile;
	TileIndex bridge_end_tile;
	Player *player;
	TileInfo ti;
} AiRoadFinder;

typedef struct AiRoadEnum {
	TileIndex dest;
	TileIndex best_tile;
	int best_track;
	uint best_dist;
} AiRoadEnum;

static const byte _dir_by_track[] = {
	0,1,0,1,2,1, 0,0,
	2,3,3,2,3,0,
};

static void AiBuildRoadRecursive(AiRoadFinder *arf, TileIndex tile, int dir);

static bool AiCheckRoadPathBetter(AiRoadFinder *arf, const byte *p)
{
	bool better = false;

	if (arf->recursive_mode < 1) {
		// Mode is 0. This means destination has not been found yet.
		// If the found path is shorter than the current one, remember it.
		if (arf->cur_best_dist < arf->best_dist ||
			(arf->cur_best_dist == arf->best_dist && arf->cur_best_depth < arf->best_depth)) {
			arf->best_depth = arf->cur_best_depth;
			arf->best_dist = arf->cur_best_dist;
			arf->best_dir = arf->cur_best_dir;
			arf->best_ptr = p;
			arf->best_tile = arf->cur_best_tile;
			better = true;
		}
	} else if (arf->recursive_mode > 1) {
		// Mode is 2.
		if (arf->best_dist != 0 || arf->cur_best_depth < arf->best_depth) {
			arf->best_depth = arf->cur_best_depth;
			arf->best_dist = 0;
			arf->best_ptr = p;
			arf->best_tile = 0;
			better = true;
		}
	}
	arf->recursive_mode = 0;
	arf->cur_best_dist = (uint)-1;
	arf->cur_best_depth = 0xff;

	return better;
}


static bool AiEnumFollowRoad(uint tile, AiRoadEnum *a, int track, uint length, byte *state)
{
	uint dist = GetTileDist(tile, a->dest);
	uint tile2;

	if (dist <= a->best_dist) {
		tile2 = TILE_MASK(tile + _tileoffs_by_dir[_dir_by_track[track]]);
		if (IS_TILETYPE(tile2, MP_STREET) &&
				(_map5[tile2]&0xF0) == 0) {
			a->best_dist = dist;
			a->best_tile = tile;
			a->best_track = track;
		}
	}

	return false;
}

static const uint16 _ai_road_table_and[4] = {
	0x1009,
	0x16,
	0x520,
	0x2A00,
};

static bool AiCheckRoadFinished(Player *p)
{
	AiRoadEnum are;
	uint tile;
	int dir = p->ai.cur_dir_a;
	uint32 bits;
	int i;

	are.dest = p->ai.cur_tile_b;
	tile = TILE_MASK(p->ai.cur_tile_a + _tileoffs_by_dir[dir]);

	bits = GetTileTrackStatus(tile, TRANSPORT_ROAD) & _ai_road_table_and[dir];
	if (bits == 0) {
		return false;
	}

	are.best_dist = (uint)-1;

	for_each_bit(i, bits) {
		FollowTrack(tile, 0x3000 | TRANSPORT_ROAD, _dir_by_track[i], (TPFEnumProc*)AiEnumFollowRoad, NULL, &are);
	}

	if (GetTileDist(tile, are.dest) <= are.best_dist)
		return false;

	if (are.best_dist == 0)
		return true;

	p->ai.cur_tile_a = are.best_tile;
	p->ai.cur_dir_a = _dir_by_track[are.best_track];
	return false;
}


static bool AiBuildRoadHelper(uint tile, int flags, int type)
{
	static const byte _road_bits[] = {
		8+2,
		1+4,
		1+8,
		4+2,
		1+2,
		8+4,
	};
	return DoCommandByTile(tile, _road_bits[type], 0, flags, CMD_BUILD_ROAD) != CMD_ERROR;
}

static inline void AiCheckBuildRoadBridgeHere(AiRoadFinder *arf, TileIndex tile, const byte *p)
{
	TileIndex tile_new;
	bool flag;

	int dir2 = p[0] & 3;

	FindLandscapeHeightByTile(&arf->ti, tile);
	if (arf->ti.tileh == _dir_table_1[dir2] || (arf->ti.tileh==0 && arf->ti.z!=0)) {
		tile_new = tile;
		// Allow bridges directly over bottom tiles
		flag = arf->ti.z == 0;
		for(;;) {
			if (tile_new < -_tileoffs_by_dir[dir2]) return; // Wraping around map, no bridge possible!
			tile_new = TILE_MASK(tile_new + _tileoffs_by_dir[dir2]);
			FindLandscapeHeightByTile(&arf->ti, tile_new);
			if (arf->ti.tileh != 0 || arf->ti.type == MP_CLEAR || arf->ti.type == MP_TREES) {
				// Allow a bridge if either we have a tile that's water, rail or street,
				// or if we found an up tile.
				if (!flag) return;
				break;
			}
			if (arf->ti.type != MP_WATER && arf->ti.type != MP_RAILWAY && arf->ti.type != MP_STREET)
				return;
			flag = true;
		}

		// Is building a (rail)bridge possible at this place (type doesn't matter)?
		if (DoCommandByTile(tile_new, tile, 0x8000, DC_AUTO, CMD_BUILD_BRIDGE) == CMD_ERROR)
			return;
		AiBuildRoadRecursive(arf, tile_new, dir2);

		// At the bottom depth, check if the new path is better than the old one.
		if (arf->depth == 1) {
			if (AiCheckRoadPathBetter(arf, p))
				arf->bridge_end_tile = tile_new;
		}
	}
}

static inline void AiCheckBuildRoadTunnelHere(AiRoadFinder *arf, TileIndex tile, const byte *p)
{
	FindLandscapeHeightByTile(&arf->ti, tile);

	if (arf->ti.tileh == _dir_table_2[p[0]&3] && arf->ti.z!=0) {
		int32 cost = DoCommandByTile(tile, 0x200, 0, DC_AUTO, CMD_BUILD_TUNNEL);

		if (cost != CMD_ERROR && cost <= (arf->player->player_money>>4)) {
			AiBuildRoadRecursive(arf, _build_tunnel_endtile, p[0]&3);
			if (arf->depth == 1) {
				AiCheckRoadPathBetter(arf, p);
			}
		}
	}
}



static void AiBuildRoadRecursive(AiRoadFinder *arf, TileIndex tile, int dir)
{
	const byte *p;

	tile = TILE_MASK(tile + _tileoffs_by_dir[dir]);

	// Reached destination?
	if (tile == arf->final_tile) {
		if ((arf->final_dir^2) == dir) {
			arf->recursive_mode = 2;
			arf->cur_best_depth = arf->depth;
		}
		return;
	}

	// Depth too deep?
	if (arf->depth >= 4) {
		uint dist = GetTileDist1Db(tile, arf->final_tile);
		if (dist < arf->cur_best_dist) {
			// Store the tile that is closest to the final position.
			arf->cur_best_dist = dist;
			arf->cur_best_tile = tile;
			arf->cur_best_dir = dir;
			arf->cur_best_depth = arf->depth;
		}
		return;
	}

	// Increase recursion depth
	arf->depth++;

	// Grab pointer to list of stuff that is possible to build
	p = _ai_table_15[dir];

	// Try to build a single rail in all directions.
	FindLandscapeHeightByTile(&arf->ti, tile);
	if (arf->ti.z == 0) {
		p += 6;
	} else {
		do {
			// Make sure that a road can be built here.
			if (AiBuildRoadHelper(tile, DC_AUTO | DC_NO_WATER | DC_AI_BUILDING, p[0])) {
				AiBuildRoadRecursive(arf, tile, p[1]);
			}

			// At the bottom depth?
			if (arf->depth == 1) {
				AiCheckRoadPathBetter(arf, p);
			}

			p += 2;
		} while (!(p[0]&0x80));
	}

	AiCheckBuildRoadBridgeHere(arf, tile, p);
	AiCheckBuildRoadTunnelHere(arf, tile, p+1);

	arf->depth--;
}

int sw;


static void AiBuildRoadConstruct(Player *p)
{
	AiRoadFinder arf;
	int i;
	uint tile;

	// Reached destination?
	if (AiCheckRoadFinished(p)) {
		p->ai.state_mode = 255;
		return;
	}

	// Setup recursive finder and call it.
	arf.player = p;
	arf.final_tile = p->ai.cur_tile_b;
	arf.final_dir = p->ai.cur_dir_b;
	arf.depth = 0;
	arf.recursive_mode = 0;
	arf.best_ptr = NULL;
	arf.cur_best_dist = (uint)-1;
	arf.cur_best_depth = 0xff;
	arf.best_dist = (uint)-1;
	arf.best_depth =  0xff;
	arf.cur_best_tile = 0;
	arf.best_tile = 0;
	AiBuildRoadRecursive(&arf, p->ai.cur_tile_a, p->ai.cur_dir_a);

	// Reached destination?
	if (arf.recursive_mode == 2 && arf.cur_best_depth == 0) {
		p->ai.state_mode = 255;
		return;
	}

	// Didn't find anything to build?
	if (arf.best_ptr == NULL) {
		// Terraform some
do_some_terraform:
		for(i=0; i!=5; i++)
			AiDoTerraformLand(p->ai.cur_tile_a, p->ai.cur_dir_a, 3, 0);

		if (++p->ai.state_counter == 21) {
			p->ai.state_mode = 1;

			p->ai.cur_tile_a = TILE_MASK(p->ai.cur_tile_a + _tileoffs_by_dir[p->ai.cur_dir_a]);
			p->ai.cur_dir_a ^= 2;
			p->ai.state_counter = 0;
		}
		return;
	}

	tile = TILE_MASK(p->ai.cur_tile_a + _tileoffs_by_dir[p->ai.cur_dir_a]);

	if (arf.best_ptr[0]&0x80) {
		int i;
		int32 bridge_len;
		p->ai.cur_tile_a = arf.bridge_end_tile;
		bridge_len = GetBridgeLength(tile, p->ai.cur_tile_a); // tile

		/*	Figure out what (road)bridge type to build
				start with best bridge, then go down to worse and worse bridges
				unnecessary to check for worse bridge (i=0), since AI will always build that.
				AI is so fucked up that fixing this small thing will probably not solve a thing
		*/
		for(i = 10; i != 0; i--) {
			if (CheckBridge_Stuff(i, bridge_len)) {
				int32 cost = DoCommandByTile(tile, p->ai.cur_tile_a, i + (0x80 << 8), DC_AUTO, CMD_BUILD_BRIDGE);
				if (cost != CMD_ERROR && cost < (p->player_money >> 5))
					break;
			}
		}

		// Build it
		DoCommandByTile(tile, p->ai.cur_tile_a, i + (0x80 << 8), DC_AUTO | DC_EXEC, CMD_BUILD_BRIDGE);

		p->ai.state_counter = 0;
	} else if (arf.best_ptr[0]&0x40) {
		// tunnel
		DoCommandByTile(tile, 0x200, 0, DC_AUTO | DC_EXEC, CMD_BUILD_TUNNEL);
		p->ai.cur_tile_a = _build_tunnel_endtile;
		p->ai.state_counter = 0;
	} else {

		// road
		if (!AiBuildRoadHelper(tile, DC_EXEC | DC_AUTO | DC_NO_WATER | DC_AI_BUILDING, arf.best_ptr[0]))
			goto do_some_terraform;

		p->ai.cur_dir_a = arf.best_ptr[1];
		p->ai.cur_tile_a = tile;
		p->ai.state_counter = 0;
	}

	if (arf.best_tile != 0) {
		for(i=0; i!=2; i++)
			AiDoTerraformLand(arf.best_tile, arf.best_dir, 3, 0);
	}
}


static void AiBuildRoad(Player *p)
{
	int i;

	if (p->ai.state_mode < 1) {
		// Construct mode, build new road.
		AiBuildRoadConstruct(p);
	} else if (p->ai.state_mode == 1) {
		// Destruct mode, not implemented for roads.
		p->ai.state_mode = 2;
		p->ai.state_counter = 0;
	} else if (p->ai.state_mode == 2) {

		// Terraform some and then try building again.
		for(i=0; i!=4; i++)
			AiDoTerraformLand(p->ai.cur_tile_a, p->ai.cur_dir_a, 3, 0);

		if (++p->ai.state_counter == 4) {
			p->ai.state_counter = 0;
			p->ai.state_mode = 0;
		}
	}
}

static TileIndex AiGetRoadBlockEdge(byte rule, TileIndex tile, int *dir)
{
	const AiDefaultBlockData *p = _road_default_block_data[rule]->data;
	while (p->mode != 1) p++;
	*dir = p->attr;
	return TILE_ADD(tile, p->tileoffs);
}


static void AiStateBuildRoad(Player *p)
{
	int num;
	AiBuildRec *aib;
	byte cmd;
	TileIndex tile;
	int dir;

	// time out?
	if (++p->ai.timeout_counter == 1388) {
		p->ai.state = AIS_DELETE_ROAD_BLOCKS;
		return;
	}

	// Currently building a road between two points?
	if (p->ai.state_mode != 255) {
		AiBuildRoad(p);

		// Alternate between edges
		swap_tile(&p->ai.start_tile_a, &p->ai.start_tile_b);
		swap_tile(&p->ai.cur_tile_a, &p->ai.cur_tile_b);
		swap_byte(&p->ai.start_dir_a, &p->ai.start_dir_b);
		swap_byte(&p->ai.cur_dir_a, &p->ai.cur_dir_b);

		sw ^= 1;
		return;
	}

	// Now, find two new points to build between
	num = p->ai.num_build_rec;
	aib = &p->ai.src;

	for(;;) {
		cmd = aib->buildcmd_a;
		aib->buildcmd_a = 255;
		if (cmd != 255) break;

		aib++;
		if (--num == 0) {
			p->ai.state = AIS_BUILD_ROAD_VEHICLES;
			return;
		}
	}

	// Find first edge to build from.
	tile = AiGetRoadBlockEdge(aib->cur_building_rule, aib->use_tile, &dir);
	p->ai.start_tile_a = tile;
	p->ai.cur_tile_a = tile;
	p->ai.start_dir_a = dir;
	p->ai.cur_dir_a = dir;

	// Find second edge to build to
	aib = (&p->ai.src) + (cmd&0xF);
	tile = AiGetRoadBlockEdge(aib->cur_building_rule, aib->use_tile, &dir);
	p->ai.start_tile_b = tile;
	p->ai.cur_tile_b = tile;
	p->ai.start_dir_b = dir;
	p->ai.cur_dir_b = dir;

	// And setup state.
	p->ai.state_mode = 2;
	p->ai.state_counter = 0;
	p->ai.banned_tile_count = 0;
}

static int AiGetStationIdFromRoadBlock(TileIndex tile, int id)
{
	const AiDefaultBlockData *p = _road_default_block_data[id]->data;
	while (p->mode != 1) p++;
	return _map2[TILE_ADD(tile, p->tileoffs)];
}

static void AiStateBuildRoadVehicles(Player *p)
{
	const AiDefaultBlockData *ptr;
	uint tile,loco_id;
	int veh, i;
	int32 cost;

	ptr = _road_default_block_data[p->ai.src.cur_building_rule]->data;
	for(;ptr->mode != 0;ptr++) {}
	tile = TILE_ADD(p->ai.src.use_tile, ptr->tileoffs);

	veh = AiChooseRoadVehToBuild(p->ai.cargo_type, p->player_money);
	if (veh == -1) {
		p->ai.state = AIS_0;
		return;
	}

	cost = DoCommandByTile(tile, veh, 0, DC_EXEC, CMD_BUILD_ROAD_VEH);
	if (cost == CMD_ERROR)
		return;

	loco_id = _new_roadveh_id;

	for(i=0; p->ai.order_list_blocks[i] != 0xFF; i++) {
		AiBuildRec *aib = (&p->ai.src) + p->ai.order_list_blocks[i];
		bool is_pass = (p->ai.cargo_type == CT_PASSENGERS ||
							p->ai.cargo_type == CT_MAIL ||
							(_opt.landscape==LT_NORMAL && p->ai.cargo_type == CT_VALUABLES));
		Order order;

		order.type = OT_GOTO_STATION;
		order.flags = 0;
		order.station = AiGetStationIdFromRoadBlock(aib->use_tile, aib->cur_building_rule);

		if (!is_pass && i == 1) order.flags |= OF_UNLOAD;
		if (p->ai.num_want_fullload != 0 && (is_pass || i == 0))
			order.flags |= OF_FULL_LOAD;

		DoCommandByTile(0, loco_id + (i << 16),	PackOrder(&order), DC_EXEC, CMD_INSERT_ORDER);
	}

	DoCommandByTile(0, loco_id, 0, DC_EXEC, CMD_START_STOP_ROADVEH);

	DoCommandByTile(0, loco_id, _ai_service_interval, DC_EXEC, CMD_CHANGE_TRAIN_SERVICE_INT);

	if (p->ai.num_want_fullload != 0)
		p->ai.num_want_fullload--;

	if (--p->ai.num_loco_to_build == 0) {
		p->ai.state =	AIS_0;
	}
}

static void AiStateDeleteRoadBlocks(Player *p)
{
	int num;
	AiBuildRec *aib;
	const AiDefaultBlockData *b;

	num = p->ai.num_build_rec;
	aib = &p->ai.src;
	do {
		if (aib->cur_building_rule != 255) {
			b = _road_default_block_data[aib->cur_building_rule]->data;
			while (b->mode != 4) {
				if (b->mode <= 1) {
					DoCommandByTile(TILE_ADD(aib->use_tile, b->tileoffs), 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
				}
				b++;
			}
		}
	} while (++aib,--num);

	p->ai.state = AIS_0;
}

static bool AiCheckIfHangar(Station *st)
{
	uint tile = st->airport_tile;
	// HANGAR of airports
	// 0x20 - hangar large airport (32)
	// 0x41 - hangar small airport (65)
	return (_map5[tile] == 32 || _map5[tile] == 65);
}

static void AiStateAirportStuff(Player *p)
{
	Station *st;
	byte acc_planes;
	int i;
	AiBuildRec *aib;
	byte rule;

	// Here we look for an airport we could use instead of building a new
	// one. If we find such an aiport for any waypoint,
	// AiStateBuildDefaultAirportBlocks() will kindly skip that one when
	// building the waypoints.

	i = 0;
	do {
		// We do this all twice - once for the source (town in the case
		// of oilrig route) and then for the destination (oilrig in the
		// case of oilrig route).
		aib = &p->ai.src + i;

		FOR_ALL_STATIONS(st) {
			// Dismiss ghost stations.
			if (st->xy == 0)
				continue;

			// Is this an airport?
			if (!(st->facilities & FACIL_AIRPORT))
				continue;

			// Do we own the airport? (Oilrigs aren't owned, though.)
			if (st->owner != OWNER_NONE && st->owner != _current_player)
				continue;

			acc_planes = GetAirport(st->airport_type)->acc_planes;

			// Dismiss heliports, unless we are checking an oilrig.
			if (acc_planes == HELICOPTERS_ONLY && !(p->ai.build_kind == 1 && i == 1))
				continue;

			// Dismiss country airports if we are doing the other
			// endpoint of an oilrig route.
			if (acc_planes == AIRCRAFT_ONLY && (p->ai.build_kind == 1 && i == 0))
				continue;

			// Dismiss airports too far away.
			if (GetTileDist1D(st->airport_tile, aib->spec_tile) > aib->rand_rng)
				continue;

			// It's ideal airport, let's take it!

			/* XXX: This part is utterly broken - rule should
			 * contain number of the rule appropriate for the
			 * airport type (country, town, ...), see
			 * _airport_default_block_data (rule is just an index
			 * in this array). But the only difference between the
			 * currently existing two rules (rule 0 - town and rule
			 * 1 - country) is the attr field which is used only
			 * when building new airports - and that's irrelevant
			 * for us. So using just about any rule will suffice
			 * here for now (some of the new airport types would be
			 * broken because they will probably need different
			 * tileoff values etc), no matter that
			 * AiCheckIfHangar() makes no sense. --pasky */
			if (acc_planes == HELICOPTERS_ONLY) {
				/* Heliports should have maybe own rulesets but
				 * OTOH we don't want AI to pick them up when
				 * looking for a suitable airport type to build.
				 * So any of rules 0 or 1 would do for now. The
				 * original rule number was 2 but that's a bug
				 * because we have no such rule. */
				rule = 1;
			} else {
				rule = AiCheckIfHangar(st);
			}

			aib->cur_building_rule = rule;
			aib->use_tile = st->airport_tile;
			break;
		}
	} while (++i != p->ai.num_build_rec);

	p->ai.state = AIS_BUILD_DEFAULT_AIRPORT_BLOCKS;
	p->ai.state_mode = 255;
	p->ai.state_counter = 0;
}

static int32 AiDoBuildDefaultAirportBlock(TileIndex tile, const AiDefaultBlockData *p, byte flag)
{
	int32 total_cost = 0, r;

	for(;p->mode == 0;p++) {
		if (!HASBIT(_avail_aircraft, p->attr))
			return CMD_ERROR;
		r = DoCommandByTile(TILE_MASK(tile + p->tileoffs), p->attr,0,flag | DC_AUTO | DC_NO_WATER,CMD_BUILD_AIRPORT);
		if (r == CMD_ERROR)
			return CMD_ERROR;
		total_cost += r;
	}

	return total_cost;
}

static bool AiCheckAirportResources(TileIndex tile, const AiDefaultBlockData *p, byte cargo)
{
	uint values[NUM_CARGO];
	int w,h;
	uint tile2;

	for(;p->mode==0;p++) {
		tile2 = TILE_ADD(tile, p->tileoffs);
		w = _airport_size_x[p->attr];
		h = _airport_size_y[p->attr];
		if (cargo & 0x80) {
			GetProductionAroundTiles(values, tile2, w, h);
			return values[cargo & 0x7F] != 0;
		} else {
			GetAcceptanceAroundTiles(values, tile2, w, h);
			return values[cargo] >= 8;
		}
	}
	return true;
}

static int AiFindBestDefaultAirportBlock(TileIndex tile, byte cargo, byte heli, int32 *cost)
{
	int i;
	const AiDefaultBlockData *p;
	for(i=0; (p = _airport_default_block_data[i]) != NULL; i++) {
		// If we are doing a helicopter service, avoid building
		// airports where they can't land.
		if (heli && GetAirport(p->attr)->acc_planes == AIRCRAFT_ONLY)
			continue;

		*cost = AiDoBuildDefaultAirportBlock(tile, p, 0);
		if (*cost != CMD_ERROR && AiCheckAirportResources(tile, p, cargo))
			return i;
	}
	return -1;
}

static void AiStateBuildDefaultAirportBlocks(Player *p)
{
	int i, j;
	AiBuildRec *aib;
	int rule;
	int32 cost;

	// time out?
	if (++p->ai.timeout_counter == 1388) {
		p->ai.state = AIS_0;
		return;
	}

	// do the following 8 times
	i = 8;
	do {
		// check if we can build the default
		aib = &p->ai.src;
		j = p->ai.num_build_rec;
		do {
			// this item has already been built?
			if (aib->cur_building_rule != 255)
				continue;

			// adjust the coordinate randomly,
			// to make sure that we find a position.
			aib->use_tile = AdjustTileCoordRandomly(aib->spec_tile, aib->rand_rng);

			// check if the aircraft stuff can be built there.
			rule = AiFindBestDefaultAirportBlock(aib->use_tile, aib->cargo, p->ai.build_kind, &cost);

#if 0
			if (!IS_TILETYPE(aib->use_tile, MP_STREET) &&
					!IS_TILETYPE(aib->use_tile, MP_RAILWAY) &&
					!IS_TILETYPE(aib->use_tile, MP_STATION)
					) {

				_map_type_and_height[aib->use_tile] = 0xa1;
				_map5[aib->use_tile] = 0x80;
				MarkTileDirtyByTile(aib->use_tile);
			}
#endif
//			redsq_debug(aib->use_tile);

			if (rule == -1) {
				// cannot build, terraform after a while
				if (p->ai.state_counter >= 600) {
					AiDoTerraformLand(aib->use_tile, Random()&3, 3, (int8)p->ai.state_mode);
				}
				// also try the other terraform direction
				if (++p->ai.state_counter >= 1000) {
					p->ai.state_counter = 0;
					p->ai.state_mode = -p->ai.state_mode;
				}
			} else if (CheckPlayerHasMoney(cost) && AiCheckBlockDistances(p,aib->use_tile)) {
				// player has money, build it.
				int32 r;

				aib->cur_building_rule = rule;

				r = AiDoBuildDefaultAirportBlock(
					aib->use_tile,
					_airport_default_block_data[rule],
					DC_EXEC | DC_NO_TOWN_RATING
				);
				assert(r != CMD_ERROR);
			}
		} while (++aib,--j);
	} while (--i);

	// check if we're done with all of them
	aib = &p->ai.src;
	j = p->ai.num_build_rec;
	do {
		if (aib->cur_building_rule == 255)
			return;
	} while (++aib,--j);

	// yep, all are done. switch state.
	p->ai.state = AIS_BUILD_AIRCRAFT_VEHICLES;
}

static int AiGetStationIdFromAircraftBlock(TileIndex tile, int id)
{
	const AiDefaultBlockData *p = _airport_default_block_data[id];
	while (p->mode != 1) p++;
	return _map2[TILE_ADD(tile, p->tileoffs)];
}

static void AiStateBuildAircraftVehicles(Player *p)
{
	const AiDefaultBlockData *ptr;
	uint tile;
	int veh;
	int32 cost;
	int i;
	uint loco_id;

	ptr = _airport_default_block_data[p->ai.src.cur_building_rule];
	for(;ptr->mode!=0;ptr++) {}

	tile = TILE_ADD(p->ai.src.use_tile, ptr->tileoffs);

	veh = AiChooseAircraftToBuild(p->player_money, p->ai.build_kind!=0 ? 1 : 0);
	if (veh == -1) {
		return;
	}

	cost = DoCommandByTile(tile, veh, 0, DC_EXEC, CMD_BUILD_AIRCRAFT);
	if (cost == CMD_ERROR)
		return;
	loco_id = _new_aircraft_id;

	for(i=0; p->ai.order_list_blocks[i] != 0xFF; i++) {
		AiBuildRec *aib = (&p->ai.src) + p->ai.order_list_blocks[i];
		bool is_pass = (p->ai.cargo_type == CT_PASSENGERS || p->ai.cargo_type == CT_MAIL);
		Order order;

		order.type = OT_GOTO_STATION;
		order.flags = 0;
		order.station = AiGetStationIdFromAircraftBlock(aib->use_tile, aib->cur_building_rule);

		if (!is_pass && i == 1) order.flags |= OF_UNLOAD;
		if (p->ai.num_want_fullload != 0 && (is_pass || i == 0))
			order.flags |= OF_FULL_LOAD;

		DoCommandByTile(0, loco_id + (i << 16), PackOrder(&order), DC_EXEC, CMD_INSERT_ORDER);
	}

	DoCommandByTile(0, loco_id, 0, DC_EXEC, CMD_START_STOP_AIRCRAFT);

	DoCommandByTile(0, loco_id, _ai_service_interval, DC_EXEC, CMD_CHANGE_TRAIN_SERVICE_INT);

	if (p->ai.num_want_fullload != 0)
		p->ai.num_want_fullload--;

	if (--p->ai.num_loco_to_build == 0) {
		p->ai.state =	AIS_0;
	}
}

static void AiStateCheckShipStuff(Player *p)
{
	// XXX
	error("!AiStateCheckShipStuff");
}

static void AiStateBuildDefaultShipBlocks(Player *p)
{
	// XXX
	error("!AiStateBuildDefaultShipBlocks");
}

static void AiStateDoShipStuff(Player *p)
{
	// XXX
	error("!AiStateDoShipStuff");
}

static void AiStateSellVeh(Player *p)
{
	Vehicle *v = p->ai.cur_veh;

	if (v->owner == _current_player) {
		if (v->type == VEH_Train) {

			if (!IsTrainDepotTile(v->tile) || v->u.rail.track != 0x80 || !(v->vehstatus&VS_STOPPED)) {
				if (v->current_order.type != OT_GOTO_DEPOT)
					DoCommandByTile(0, v->index, 0, DC_EXEC, CMD_TRAIN_GOTO_DEPOT);
				goto going_to_depot;
			}

			// Sell whole train
			DoCommandByTile(v->tile, v->index, 1, DC_EXEC, CMD_SELL_RAIL_WAGON);

		} else if (v->type == VEH_Road) {
			if (!IsRoadDepotTile(v->tile) || v->u.road.state != 254 || !(v->vehstatus&VS_STOPPED)) {
				if (v->current_order.type != OT_GOTO_DEPOT)
					DoCommandByTile(0, v->index, 0, DC_EXEC, CMD_SEND_ROADVEH_TO_DEPOT);
				goto going_to_depot;
			}

			DoCommandByTile(0, v->index, 0, DC_EXEC, CMD_SELL_ROAD_VEH);
		} else if (v->type == VEH_Aircraft) {
			if (!IsAircraftHangarTile(v->tile) && !(v->vehstatus&VS_STOPPED)) {
				if (v->current_order.type != OT_GOTO_DEPOT)
					DoCommandByTile(0, v->index, 0, DC_EXEC, CMD_SEND_AIRCRAFT_TO_HANGAR);
				goto going_to_depot;
			}

			DoCommandByTile(0, v->index, 0, DC_EXEC, CMD_SELL_AIRCRAFT);
			} else if (v->type == VEH_Ship) {
			// XXX: not implemented
			error("!v->type == VEH_Ship");
		}
	}

	goto return_to_loop;
going_to_depot:;
	if (++p->ai.state_counter <= 832)
		return;

	if (v->current_order.type == OT_GOTO_DEPOT) {
		v->current_order.type = OT_DUMMY;
		v->current_order.flags = 0;
		InvalidateWindow(WC_VEHICLE_VIEW, v->index);
	}
return_to_loop:;
	p->ai.state = AIS_VEH_LOOP;
}

static void AiStateRemoveStation(Player *p)
{
	// Remove stations that aren't in use by any vehicle
	byte in_use[256], *used;
	Order *ord;
	Station *st;
	uint tile;

	// Go to this state when we're done.
	p->ai.state = AIS_1;

	// Get a list of all stations that are in use by a vehicle
	memset(in_use, 0, sizeof(in_use));
	for (ord = _order_array; ord != _ptr_to_next_order; ++ord) {
		if (ord->type == OT_GOTO_STATION)
			in_use[ord->station] = 1;
	}

	// Go through all stations and delete those that aren't in use
	used=in_use;
	FOR_ALL_STATIONS(st) {
		if (st->xy != 0 && st->owner == _current_player && !*used &&
				((tile = st->bus_tile) != 0 ||
					(tile = st->lorry_tile) != 0 ||
					(tile = st->train_tile) != 0 ||
					(tile = st->dock_tile) != 0 ||
					(tile = st->airport_tile) != 0)) {
			DoCommandByTile(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
		}
		used++;
	}

}

static void AiRemovePlayerRailOrRoad(Player *p, uint tile)
{
	byte m5;

	if (IS_TILETYPE(tile, MP_RAILWAY)) {
		if (_map_owner[tile] != _current_player)
			return;
		m5 = _map5[tile];
		if ((m5&~0x3) != 0xC0) {
is_rail_crossing:;
			m5 = GetRailTrackStatus(tile);

			if (m5 == 0xC || m5 == 0x30)
				return;

			if (m5&0x25) {
pos_0:
				if (!(GetRailTrackStatus(TILE_MASK(tile-TILE_XY(1,0)))&0x19)) {
					p->ai.cur_dir_a = 0;
					p->ai.cur_tile_a = tile;
					p->ai.state = AIS_REMOVE_SINGLE_RAIL_TILE;
					return;
				}
			}

			if (m5&0x2A) {
pos_1:
				if (!(GetRailTrackStatus(TILE_MASK(tile+TILE_XY(0,1)))&0x16)) {
					p->ai.cur_dir_a = 1;
					p->ai.cur_tile_a = tile;
					p->ai.state = AIS_REMOVE_SINGLE_RAIL_TILE;
					return;
				}
			}

			if (m5&0x19) {
pos_2:
				if (!(GetRailTrackStatus(TILE_MASK(tile+TILE_XY(1,0)))&0x25)) {
					p->ai.cur_dir_a = 2;
					p->ai.cur_tile_a = tile;
					p->ai.state = AIS_REMOVE_SINGLE_RAIL_TILE;
					return;
				}
			}

			if (m5&0x16) {
pos_3:
				if (!(GetRailTrackStatus(TILE_MASK(tile-TILE_XY(0,1)))&0x2A)) {
					p->ai.cur_dir_a = 3;
					p->ai.cur_tile_a = tile;
					p->ai.state = AIS_REMOVE_SINGLE_RAIL_TILE;
					return;
				}
			}
		} else {
			static const byte _depot_bits[] = {0x19,0x16,0x25,0x2A};

			m5 &= 3;
			if (GetRailTrackStatus(tile + _tileoffs_by_dir[m5]) & _depot_bits[m5])
				return;

			DoCommandByTile(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
		}
	} else if (IS_TILETYPE(tile, MP_STREET)) {
		if (_map_owner[tile] != _current_player)
			return;

		if ( (_map5[tile]&0xF0) == 0x10)
			goto is_rail_crossing;

		if ( (_map5[tile]&0xF0) == 0x20) {
			int dir;

			// Check if there are any stations around.
			if (IS_TILETYPE(tile + TILE_XY(-1,0), MP_STATION) &&
					_map_owner[tile + TILE_XY(-1,0)] == _current_player)
						return;

			if (IS_TILETYPE(tile + TILE_XY(1,0), MP_STATION) &&
					_map_owner[tile + TILE_XY(1,0)] == _current_player)
						return;

			if (IS_TILETYPE(tile + TILE_XY(0,-1), MP_STATION) &&
					_map_owner[tile + TILE_XY(0,-1)] == _current_player)
						return;

			if (IS_TILETYPE(tile + TILE_XY(0,1), MP_STATION) &&
					_map_owner[tile + TILE_XY(0,1)] == _current_player)
						return;

			dir = _map5[tile] & 3;

			DoCommandByTile(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
			DoCommandByTile(
				TILE_MASK(tile + _tileoffs_by_dir[dir]),
				8 >> (dir ^ 2),
				0,
				DC_EXEC,
				CMD_REMOVE_ROAD);
		}
	} else if (IS_TILETYPE(tile, MP_TUNNELBRIDGE)) {
		byte b;

		if (_map_owner[tile] != _current_player || (_map5[tile] & 0xC6) != 0x80)
			return;

		m5 = 0;

		b = _map5[tile] & 0x21;
		if (b == 0) goto pos_0;
		if (b == 1) goto pos_3;
		if (b == 0x20) goto pos_2;
		goto pos_1;
	}
}

static void AiStateRemoveTrack(Player *p)
{
	int num = 1000;

	do {
		uint tile = ++p->ai.state_counter;

		// Iterated all tiles?
		if (tile == 0) {
			p->ai.state = AIS_REMOVE_STATION;
			return;
		}

		// Remove player stuff in that tile
		AiRemovePlayerRailOrRoad(p, tile);
		if (p->ai.state != AIS_REMOVE_TRACK)
			return;
	} while (--num);
}

static void AiStateRemoveSingleRailTile(Player *p)
{
	// Remove until we can't remove more.
	if (!AiRemoveTileAndGoForward(p)) {
		p->ai.state = AIS_REMOVE_TRACK;
	}
}

static AiStateAction * const _ai_actions[] = {
	AiCase0,
	AiCase1,
	AiStateVehLoop,
	AiStateCheckReplaceVehicle,
	AiStateDoReplaceVehicle,
	AiStateWantNewRoute,

	AiStateBuildDefaultRailBlocks,
	AiStateBuildRail,
	AiStateBuildRailVeh,
	AiStateDeleteRailBlocks,

	AiStateBuildDefaultRoadBlocks,
	AiStateBuildRoad,
	AiStateBuildRoadVehicles,
	AiStateDeleteRoadBlocks,

	AiStateAirportStuff,
	AiStateBuildDefaultAirportBlocks,
	AiStateBuildAircraftVehicles,

	AiStateCheckShipStuff,
	AiStateBuildDefaultShipBlocks,
	AiStateDoShipStuff,

	AiStateSellVeh,
	AiStateRemoveStation,
	AiStateRemoveTrack,

	AiStateRemoveSingleRailTile
};

extern void ShowBuyCompanyDialog(uint player);

static void AiHandleTakeover(Player *p)
{
	if (p->bankrupt_timeout != 0) {
		if ((p->bankrupt_timeout-=8) > 0)
			return;
		p->bankrupt_timeout = 0;
		DeleteWindowById(WC_BUY_COMPANY, _current_player);
		if (_current_player == _local_player) {
			AskExitToGameMenu();
			return;
		}
		if (IS_HUMAN_PLAYER(_current_player))
			return;
	}

	if (p->bankrupt_asked == 255)
		return;

	{
		uint asked = p->bankrupt_asked;
		Player *pp, *best_pl = NULL;
		int32 best_val = -1;
		uint old_p;

		// Ask the guy with the highest performance hist.
		FOR_ALL_PLAYERS(pp) {
			if (pp->is_active &&
					!(asked&1) &&
					pp->bankrupt_asked == 0 &&
					best_val < pp->old_economy[1].performance_history) {
				best_val = pp->old_economy[1].performance_history;
				best_pl = pp;
			}
			asked>>=1;
		}

		// Asked all players?
		if (best_val == -1) {
			p->bankrupt_asked = 255;
			return;
		}

		SETBIT(p->bankrupt_asked, best_pl->index);

		if (best_pl->index == _local_player) {
			p->bankrupt_timeout = 4440;
			ShowBuyCompanyDialog(_current_player);
			return;
		}
		if (IS_HUMAN_PLAYER(best_pl->index))
			return;

		// Too little money for computer to buy it?
		if (best_pl->player_money >> 1 >= p->bankrupt_value) {
			// Computer wants to buy it.
			old_p = _current_player;
			_current_player = p->index;
			DoCommandByTile(0, old_p, 0, DC_EXEC, CMD_BUY_COMPANY);
			_current_player = old_p;
		}
	}
}

static void AiAdjustLoan(Player *p)
{
	int32 base = AiGetBasePrice(p);

	if (p->player_money > base * 1400) {
		// Decrease loan
		if (p->current_loan != 0) {
			DoCommandByTile(0, _current_player, 0, DC_EXEC, CMD_DECREASE_LOAN);
		}
	} else if (p->player_money < base * 500) {
		// Increase loan
		if (p->current_loan < _economy.max_loan &&
				p->num_valid_stat_ent >= 2 &&
				-(p->old_economy[0].expenses+p->old_economy[1].expenses) < base * 60) {
			DoCommandByTile(0, _current_player, 0, DC_EXEC, CMD_INCREASE_LOAN);
		}
	}
}

static void AiBuildCompanyHQ(Player *p)
{
	TileIndex tile;

	if (p->location_of_house == 0 &&
			p->last_build_coordinate != 0) {
		tile = AdjustTileCoordRandomly(p->last_build_coordinate, 8);
		DoCommandByTile(tile, 0, 0, DC_EXEC | DC_AUTO | DC_NO_WATER, CMD_BUILD_COMPANY_HQ);
	}
}


void AiDoGameLoop(Player *p)
{
	_cur_ai_player = p;

	if (p->bankrupt_asked != 0) {
		AiHandleTakeover(p);
		return;
	}

	// Ugly hack to make sure the service interval of the AI is good, not looking
	//  to the patch-setting
	// Also, it takes into account the setting if the service-interval is in days
	//  or in %
	_ai_service_interval = _patches.servint_ispercent?80:180;

	if (IS_HUMAN_PLAYER(_current_player))
		return;

	AiAdjustLoan(p);
	AiBuildCompanyHQ(p);

	if (_opt.diff.competitor_speed == 4) {
		/* ultraspeed */
		_ai_actions[p->ai.state](p);
		if (p->bankrupt_asked != 0)
			return;
	} else if (_opt.diff.competitor_speed != 3) {
		p->ai.tick++;
		if (!(p->ai.tick&1))
			return;
		if (_opt.diff.competitor_speed != 2) {
			if (!(p->ai.tick&2))
				return;
			if (_opt.diff.competitor_speed == 0) {
				if (!(p->ai.tick&4))
					return;
			}
		}
	}
#if 0
	{
		static byte old_state = 99;
		static bool hasdots = false;
		char *_ai_state_names[]={
			"AiCase0",
			"AiCase1",
			"AiStateVehLoop",
			"AiStateCheckReplaceVehicle",
			"AiStateDoReplaceVehicle",
			"AiStateWantNewRoute",
			"AiStateBuildDefaultRailBlocks",
			"AiStateBuildRail",
			"AiStateBuildRailVeh",
			"AiStateDeleteRailBlocks",
			"AiStateBuildDefaultRoadBlocks",
			"AiStateBuildRoad",
			"AiStateBuildRoadVehicles",
			"AiStateDeleteRoadBlocks",
			"AiStateAirportStuff",
			"AiStateBuildDefaultAirportBlocks",
			"AiStateBuildAircraftVehicles",
			"AiStateCheckShipStuff",
			"AiStateBuildDefaultShipBlocks",
			"AiStateDoShipStuff",
			"AiStateSellVeh",
			"AiStateRemoveStation",
			"AiStateRemoveTrack",
			"AiStateRemoveSingleRailTile"
		};

		if (p->ai.state != old_state) {
			if (hasdots)
				printf("\n");
			hasdots=false;
			printf("AiState: %s\n", _ai_state_names[old_state=p->ai.state]);
		} else {
			printf(".");
			hasdots=true;
		}
	}
#endif

	_ai_actions[p->ai.state](p);
}
