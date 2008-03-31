/* $Id$ */

#include "../../stdafx.h"
#include "../../openttd.h"
#include "../../aircraft.h"
#include "../../bridge_map.h"
#include "../../tile_cmd.h"
#include "../../landscape.h"
#include "../../rail_map.h"
#include "../../road_map.h"
#include "../../roadveh.h"
#include "../../station_map.h"
#include "../../tunnel_map.h"
#include "../../engine.h"
#include "../../command_func.h"
#include "../../town.h"
#include "../../industry.h"
#include "../../pathfind.h"
#include "../../airport.h"
#include "../../depot.h"
#include "../../variables.h"
#include "../../bridge.h"
#include "../../date_func.h"
#include "../../tunnelbridge_map.h"
#include "../../window_func.h"
#include "../../vehicle_func.h"
#include "../../functions.h"
#include "../../saveload.h"
#include "../../player_func.h"
#include "../../player_base.h"
#include "../../settings_type.h"
#include "default.h"
#include "../../tunnelbridge.h"
#include "../../order_func.h"

#include "../../table/ai_rail.h"

// remove some day perhaps?
static uint _ai_service_interval;
PlayerAI _players_ai[MAX_PLAYERS];

typedef void AiStateAction(Player *p);

enum {
	AIS_0                            =  0,
	AIS_1                            =  1,
	AIS_VEH_LOOP                     =  2,
	AIS_VEH_CHECK_REPLACE_VEHICLE    =  3,
	AIS_VEH_DO_REPLACE_VEHICLE       =  4,
	AIS_WANT_NEW_ROUTE               =  5,
	AIS_BUILD_DEFAULT_RAIL_BLOCKS    =  6,
	AIS_BUILD_RAIL                   =  7,
	AIS_BUILD_RAIL_VEH               =  8,
	AIS_DELETE_RAIL_BLOCKS           =  9,
	AIS_BUILD_DEFAULT_ROAD_BLOCKS    = 10,
	AIS_BUILD_ROAD                   = 11,
	AIS_BUILD_ROAD_VEHICLES          = 12,
	AIS_DELETE_ROAD_BLOCKS           = 13,
	AIS_AIRPORT_STUFF                = 14,
	AIS_BUILD_DEFAULT_AIRPORT_BLOCKS = 15,
	AIS_BUILD_AIRCRAFT_VEHICLES      = 16,
	AIS_CHECK_SHIP_STUFF             = 17,
	AIS_BUILD_DEFAULT_SHIP_BLOCKS    = 18,
	AIS_DO_SHIP_STUFF                = 19,
	AIS_SELL_VEHICLE                 = 20,
	AIS_REMOVE_STATION               = 21,
	AIS_REMOVE_TRACK                 = 22,
	AIS_REMOVE_SINGLE_RAIL_TILE      = 23
};


static inline TrackBits GetRailTrackStatus(TileIndex tile)
{
	return TrackStatusToTrackBits(GetTileTrackStatus(tile, TRANSPORT_RAIL, 0));
}


static void AiCase0(Player *p)
{
	_players_ai[p->index].state = AIS_REMOVE_TRACK;
	_players_ai[p->index].state_counter = 0;
}

static void AiCase1(Player *p)
{
	_players_ai[p->index].cur_veh = NULL;
	_players_ai[p->index].state = AIS_VEH_LOOP;
}

static void AiStateVehLoop(Player *p)
{
	Vehicle *v;
	uint index;

	index = (_players_ai[p->index].cur_veh == NULL) ? 0 : _players_ai[p->index].cur_veh->index + 1;

	FOR_ALL_VEHICLES_FROM(v, index) {
		if (v->owner != _current_player) continue;

		if ((v->type == VEH_TRAIN && v->subtype == 0) ||
				v->type == VEH_ROAD ||
				(v->type == VEH_AIRCRAFT && IsNormalAircraft(v)) ||
				v->type == VEH_SHIP) {
			/* replace engine? */
			if (v->type == VEH_TRAIN && v->engine_type < 3 &&
					(_price.build_railvehicle >> 3) < p->player_money) {
				_players_ai[p->index].state = AIS_VEH_CHECK_REPLACE_VEHICLE;
				_players_ai[p->index].cur_veh = v;
				return;
			}

			/* not profitable? */
			if (v->age >= 730 &&
					v->profit_last_year < _price.station_value * 5 * 256 &&
					v->profit_this_year < _price.station_value * 5 * 256) {
				_players_ai[p->index].state_counter = 0;
				_players_ai[p->index].state = AIS_SELL_VEHICLE;
				_players_ai[p->index].cur_veh = v;
				return;
			}

			/* not reliable? */
			if (v->age >= v->max_age || (
						v->age != 0 &&
						GetEngine(v->engine_type)->reliability < 35389
					)) {
				_players_ai[p->index].state = AIS_VEH_CHECK_REPLACE_VEHICLE;
				_players_ai[p->index].cur_veh = v;
				return;
			}
		}
	}

	_players_ai[p->index].state = AIS_WANT_NEW_ROUTE;
	_players_ai[p->index].state_counter = 0;
}

static EngineID AiChooseTrainToBuild(RailType railtype, Money money, byte flag, TileIndex tile)
{
	EngineID best_veh_index = INVALID_ENGINE;
	byte best_veh_score = 0;
	EngineID i;

	FOR_ALL_ENGINEIDS_OF_TYPE(i, VEH_TRAIN) {
		const RailVehicleInfo *rvi = RailVehInfo(i);
		const Engine* e = GetEngine(i);

		if (!IsCompatibleRail(rvi->railtype, railtype) ||
				rvi->railveh_type == RAILVEH_WAGON ||
				(rvi->railveh_type == RAILVEH_MULTIHEAD && flag & 1) ||
				!HasBit(e->player_avail, _current_player) ||
				e->reliability < 0x8A3D) {
			continue;
		}

		/* Don't choose an engine designated for passenger use for freight. */
		if (rvi->ai_passenger_only != 0 && flag == 1) continue;

		CommandCost ret = DoCommand(tile, i, 0, 0, CMD_BUILD_RAIL_VEHICLE);
		if (CmdSucceeded(ret) && ret.GetCost() <= money && rvi->ai_rank >= best_veh_score) {
			best_veh_score = rvi->ai_rank;
			best_veh_index = i;
		}
	}

	return best_veh_index;
}

static EngineID AiChooseRoadVehToBuild(CargoID cargo, Money money, TileIndex tile)
{
	EngineID best_veh_index = INVALID_ENGINE;
	int32 best_veh_rating = 0;
	EngineID i;

	FOR_ALL_ENGINEIDS_OF_TYPE(i, VEH_ROAD) {
		const RoadVehicleInfo *rvi = RoadVehInfo(i);
		const Engine* e = GetEngine(i);

		if (!HasBit(e->player_avail, _current_player) || e->reliability < 0x8A3D) {
			continue;
		}

		/* Skip vehicles which can't take our cargo type */
		if (rvi->cargo_type != cargo && !CanRefitTo(i, cargo)) continue;

		/* Rate and compare the engine by speed & capacity */
		int rating = rvi->max_speed * rvi->capacity;
		if (rating <= best_veh_rating) continue;

		CommandCost ret = DoCommand(tile, i, 0, 0, CMD_BUILD_ROAD_VEH);
		if (CmdFailed(ret)) continue;

		/* Add the cost of refitting */
		if (rvi->cargo_type != cargo) ret.AddCost(GetRefitCost(i));
		if (ret.GetCost() > money) continue;

		best_veh_rating = rating;
		best_veh_index = i;
	}

	return best_veh_index;
}

/**
 * Choose aircraft to build.
 * @param money current AI money
 * @param forbidden forbidden flags - AIR_HELI = 0 (always allowed), AIR_CTOL = 1 (bit 0), AIR_FAST = 2 (bit 1)
 * @return EngineID of aircraft to build
 */
static EngineID AiChooseAircraftToBuild(Money money, byte forbidden)
{
	EngineID best_veh_index = INVALID_ENGINE;
	Money best_veh_cost = 0;
	EngineID i;

	FOR_ALL_ENGINEIDS_OF_TYPE(i, VEH_AIRCRAFT) {
		const Engine* e = GetEngine(i);

		if (!HasBit(e->player_avail, _current_player) || e->reliability < 0x8A3D) {
			continue;
		}

		if ((AircraftVehInfo(i)->subtype & forbidden) != 0) continue;

		CommandCost ret = DoCommand(0, i, 0, DC_QUERY_COST, CMD_BUILD_AIRCRAFT);
		if (CmdSucceeded(ret) && ret.GetCost() <= money && ret.GetCost() >= best_veh_cost) {
			best_veh_cost = ret.GetCost();
			best_veh_index = i;
		}
	}

	return best_veh_index;
}

static Money AiGetBasePrice(const Player* p)
{
	Money base = _price.station_value;

	// adjust base price when more expensive vehicles are available
	switch (_players_ai[p->index].railtype_to_use) {
		default: NOT_REACHED();
		case RAILTYPE_RAIL:     break;
		case RAILTYPE_ELECTRIC: break;
		case RAILTYPE_MONO:     base = (base * 3) >> 1; break;
		case RAILTYPE_MAGLEV:   base *= 2; break;
	}

	return base;
}

static EngineID AiChooseRoadVehToReplaceWith(const Player* p, const Vehicle* v)
{
	Money avail_money = p->player_money + v->value;
	return AiChooseRoadVehToBuild(v->cargo_type, avail_money, v->tile);
}

static EngineID AiChooseAircraftToReplaceWith(const Player* p, const Vehicle* v)
{
	Money avail_money = p->player_money + v->value;

	/* determine forbidden aircraft bits */
	byte forbidden = 0;
	const Order *o;

	FOR_VEHICLE_ORDERS(v, o) {
		if (!o->IsValid()) continue;
		if (!IsValidStationID(o->dest)) continue;
		const Station *st = GetStation(o->dest);
		if (!(st->facilities & FACIL_AIRPORT)) continue;

		AirportFTAClass::Flags flags = st->Airport()->flags;
		if (!(flags & AirportFTAClass::AIRPLANES)) forbidden |= AIR_CTOL | AIR_FAST; // no planes for heliports / oil rigs
		if (flags & AirportFTAClass::SHORT_STRIP) forbidden |= AIR_FAST; // no fast planes for small airports
	}

	return AiChooseAircraftToBuild(
		avail_money, forbidden
	);
}

static EngineID AiChooseTrainToReplaceWith(const Player* p, const Vehicle* v)
{
	Money avail_money = p->player_money + v->value;
	const Vehicle* u = v;
	int num = 0;

	while (++num, u->Next() != NULL) {
		u = u->Next();
	}

	// XXX: check if a wagon
	return AiChooseTrainToBuild(v->u.rail.railtype, avail_money, 0, v->tile);
}

static EngineID AiChooseShipToReplaceWith(const Player* p, const Vehicle* v)
{
	/* Ships are not implemented in this (broken) AI */
	return INVALID_ENGINE;
}

static void AiHandleGotoDepot(Player *p, int cmd)
{
	if (_players_ai[p->index].cur_veh->current_order.type != OT_GOTO_DEPOT)
		DoCommand(0, _players_ai[p->index].cur_veh->index, 0, DC_EXEC, cmd);

	if (++_players_ai[p->index].state_counter <= 1387) {
		_players_ai[p->index].state = AIS_VEH_DO_REPLACE_VEHICLE;
		return;
	}

	if (_players_ai[p->index].cur_veh->current_order.type == OT_GOTO_DEPOT) {
		_players_ai[p->index].cur_veh->current_order.type = OT_DUMMY;
		_players_ai[p->index].cur_veh->current_order.flags = 0;
		InvalidateWindow(WC_VEHICLE_VIEW, _players_ai[p->index].cur_veh->index);
	}
}

static void AiRestoreVehicleOrders(Vehicle *v, BackuppedOrders *bak)
{
	if (bak->order == NULL) return;

	for (uint i = 0; bak->order[i].type != OT_NOTHING; i++) {
		if (!DoCommandP(0, v->index + (i << 16), PackOrder(&bak->order[i]), NULL, CMD_INSERT_ORDER | CMD_NO_TEST_IF_IN_NETWORK))
			break;
	}
}

static void AiHandleReplaceTrain(Player *p)
{
	const Vehicle* v = _players_ai[p->index].cur_veh;
	BackuppedOrders orderbak;
	EngineID veh;

	// wait until the vehicle reaches the depot.
	if (!IsTileDepotType(v->tile, TRANSPORT_RAIL) || v->u.rail.track != 0x80 || !(v->vehstatus&VS_STOPPED)) {
		AiHandleGotoDepot(p, CMD_SEND_TRAIN_TO_DEPOT);
		return;
	}

	veh = AiChooseTrainToReplaceWith(p, v);
	if (veh != INVALID_ENGINE) {
		TileIndex tile;

		BackupVehicleOrders(v, &orderbak);
		tile = v->tile;

		if (CmdSucceeded(DoCommand(0, v->index, 2, DC_EXEC, CMD_SELL_RAIL_WAGON)) &&
				CmdSucceeded(DoCommand(tile, veh, 0, DC_EXEC, CMD_BUILD_RAIL_VEHICLE))) {
			VehicleID veh = _new_vehicle_id;
			AiRestoreVehicleOrders(GetVehicle(veh), &orderbak);
			DoCommand(0, veh, 0, DC_EXEC, CMD_START_STOP_TRAIN);

			DoCommand(0, veh, _ai_service_interval, DC_EXEC, CMD_CHANGE_SERVICE_INT);
		}
	}
}

static void AiHandleReplaceRoadVeh(Player *p)
{
	const Vehicle* v = _players_ai[p->index].cur_veh;
	BackuppedOrders orderbak;
	EngineID veh;

	if (!v->IsStoppedInDepot()) {
		AiHandleGotoDepot(p, CMD_SEND_ROADVEH_TO_DEPOT);
		return;
	}

	veh = AiChooseRoadVehToReplaceWith(p, v);
	if (veh != INVALID_ENGINE) {
		TileIndex tile;

		BackupVehicleOrders(v, &orderbak);
		tile = v->tile;

		if (CmdSucceeded(DoCommand(0, v->index, 0, DC_EXEC, CMD_SELL_ROAD_VEH)) &&
				CmdSucceeded(DoCommand(tile, veh, 0, DC_EXEC, CMD_BUILD_ROAD_VEH))) {
			VehicleID veh = _new_vehicle_id;

			AiRestoreVehicleOrders(GetVehicle(veh), &orderbak);
			DoCommand(0, veh, 0, DC_EXEC, CMD_START_STOP_ROADVEH);
			DoCommand(0, veh, _ai_service_interval, DC_EXEC, CMD_CHANGE_SERVICE_INT);
		}
	}
}

static void AiHandleReplaceAircraft(Player *p)
{
	const Vehicle* v = _players_ai[p->index].cur_veh;
	BackuppedOrders orderbak;
	EngineID veh;

	if (!v->IsStoppedInDepot()) {
		AiHandleGotoDepot(p, CMD_SEND_AIRCRAFT_TO_HANGAR);
		return;
	}

	veh = AiChooseAircraftToReplaceWith(p, v);
	if (veh != INVALID_ENGINE) {
		TileIndex tile;

		BackupVehicleOrders(v, &orderbak);
		tile = v->tile;

		if (CmdSucceeded(DoCommand(0, v->index, 0, DC_EXEC, CMD_SELL_AIRCRAFT)) &&
				CmdSucceeded(DoCommand(tile, veh, 0, DC_EXEC, CMD_BUILD_AIRCRAFT))) {
			VehicleID veh = _new_vehicle_id;
			AiRestoreVehicleOrders(GetVehicle(veh), &orderbak);
			DoCommand(0, veh, 0, DC_EXEC, CMD_START_STOP_AIRCRAFT);

			DoCommand(0, veh, _ai_service_interval, DC_EXEC, CMD_CHANGE_SERVICE_INT);
		}
	}
}

static void AiHandleReplaceShip(Player *p)
{
	/* Ships are not implemented in this (broken) AI */
}

typedef EngineID CheckReplaceProc(const Player* p, const Vehicle* v);

static CheckReplaceProc* const _veh_check_replace_proc[] = {
	AiChooseTrainToReplaceWith,
	AiChooseRoadVehToReplaceWith,
	AiChooseShipToReplaceWith,
	AiChooseAircraftToReplaceWith,
};

typedef void DoReplaceProc(Player *p);
static DoReplaceProc* const _veh_do_replace_proc[] = {
	AiHandleReplaceTrain,
	AiHandleReplaceRoadVeh,
	AiHandleReplaceShip,
	AiHandleReplaceAircraft
};

static void AiStateCheckReplaceVehicle(Player *p)
{
	const Vehicle* v = _players_ai[p->index].cur_veh;

	if (!v->IsValid() ||
			v->owner != _current_player ||
			v->type > VEH_SHIP ||
			_veh_check_replace_proc[v->type - VEH_TRAIN](p, v) == INVALID_ENGINE) {
		_players_ai[p->index].state = AIS_VEH_LOOP;
	} else {
		_players_ai[p->index].state_counter = 0;
		_players_ai[p->index].state = AIS_VEH_DO_REPLACE_VEHICLE;
	}
}

static void AiStateDoReplaceVehicle(Player *p)
{
	const Vehicle* v = _players_ai[p->index].cur_veh;

	_players_ai[p->index].state = AIS_VEH_LOOP;
	// vehicle is not owned by the player anymore, something went very wrong.
	if (!v->IsValid() || v->owner != _current_player) return;
	_veh_do_replace_proc[v->type - VEH_TRAIN](p);
}

struct FoundRoute {
	int distance;
	CargoID cargo;
	void *from;
	void *to;
};

static Town *AiFindRandomTown()
{
	return GetRandomTown();
}

static Industry *AiFindRandomIndustry()
{
	int num = RandomRange(GetMaxIndustryIndex());
	if (IsValidIndustryID(num)) return GetIndustry(num);

	return NULL;
}

static void AiFindSubsidyIndustryRoute(FoundRoute *fr)
{
	uint i;
	CargoID cargo;
	const Subsidy* s;
	Industry* from;
	TileIndex to_xy;

	// initially error
	fr->distance = -1;

	// Randomize subsidy index..
	i = RandomRange(lengthof(_subsidies) * 3);
	if (i >= lengthof(_subsidies)) return;

	s = &_subsidies[i];

	// Don't want passengers or mail
	cargo = s->cargo_type;
	if (cargo == CT_INVALID ||
			cargo == CT_PASSENGERS ||
			cargo == CT_MAIL ||
			s->age > 7) {
		return;
	}
	fr->cargo = cargo;

	fr->from = from = GetIndustry(s->from);

	if (cargo == CT_GOODS || cargo == CT_FOOD) {
		Town* to_tow = GetTown(s->to);

		if (to_tow->population < (cargo == CT_FOOD ? 200U : 900U)) return; // error
		fr->to = to_tow;
		to_xy = to_tow->xy;
	} else {
		Industry* to_ind = GetIndustry(s->to);

		fr->to = to_ind;
		to_xy = to_ind->xy;
	}

	fr->distance = DistanceManhattan(from->xy, to_xy);
}

static void AiFindSubsidyPassengerRoute(FoundRoute *fr)
{
	uint i;
	const Subsidy* s;
	Town *from, *to;

	// initially error
	fr->distance = -1;

	// Randomize subsidy index..
	i = RandomRange(lengthof(_subsidies) * 3);
	if (i >= lengthof(_subsidies)) return;

	s = &_subsidies[i];

	// Only want passengers
	if (s->cargo_type != CT_PASSENGERS || s->age > 7) return;
	fr->cargo = s->cargo_type;

	fr->from = from = GetTown(s->from);
	fr->to = to = GetTown(s->to);

	// They must be big enough
	if (from->population < 400 || to->population < 400) return;

	fr->distance = DistanceManhattan(from->xy, to->xy);
}

static void AiFindRandomIndustryRoute(FoundRoute *fr)
{
	Industry* i;
	uint32 r;
	CargoID cargo;

	// initially error
	fr->distance = -1;

	r = Random();

	// pick a source
	fr->from = i = AiFindRandomIndustry();
	if (i == NULL) return;

	// pick a random produced cargo
	cargo = i->produced_cargo[0];
	if (r & 1 && i->produced_cargo[1] != CT_INVALID) cargo = i->produced_cargo[1];

	fr->cargo = cargo;

	// don't allow passengers
	if (cargo == CT_INVALID || cargo == CT_PASSENGERS) return;

	if (cargo != CT_GOODS && cargo != CT_FOOD) {
		// pick a dest, and see if it can receive
		Industry* i2 = AiFindRandomIndustry();
		if (i2 == NULL || i == i2 ||
				(i2->accepts_cargo[0] != cargo &&
				i2->accepts_cargo[1] != cargo &&
				i2->accepts_cargo[2] != cargo)) {
			return;
		}

		fr->to = i2;
		fr->distance = DistanceManhattan(i->xy, i2->xy);
	} else {
		// pick a dest town, and see if it's big enough
		Town* t = AiFindRandomTown();

		if (t == NULL || t->population < (cargo == CT_FOOD ? 200U : 900U)) return;

		fr->to = t;
		fr->distance = DistanceManhattan(i->xy, t->xy);
	}
}

static void AiFindRandomPassengerRoute(FoundRoute *fr)
{
	Town* source;
	Town* dest;

	// initially error
	fr->distance = -1;

	fr->from = source = AiFindRandomTown();
	if (source == NULL || source->population < 400) return;

	fr->to = dest = AiFindRandomTown();
	if (dest == NULL || source == dest || dest->population < 400) return;

	fr->distance = DistanceManhattan(source->xy, dest->xy);
}

// Warn: depends on 'xy' being the first element in both Town and Industry
#define GET_TOWN_OR_INDUSTRY_TILE(p) (((Town*)(p))->xy)

static bool AiCheckIfRouteIsGood(Player *p, FoundRoute *fr, byte bitmask)
{
	TileIndex from_tile, to_tile;
	Station *st;
	int dist;
	uint same_station = 0;

	from_tile = GET_TOWN_OR_INDUSTRY_TILE(fr->from);
	to_tile = GET_TOWN_OR_INDUSTRY_TILE(fr->to);

	dist = 0xFFFF;
	FOR_ALL_STATIONS(st) {
		int cur;

		if (st->owner != _current_player) continue;
		cur = DistanceMax(from_tile, st->xy);
		if (cur < dist) dist = cur;
		cur = DistanceMax(to_tile, st->xy);
		if (cur < dist) dist = cur;
		if (to_tile == from_tile && st->xy == to_tile) same_station++;
	}

	// To prevent the AI from building ten busstations in the same town, do some calculations
	//  For each road or airport station, we want 350 of population!
	if ((bitmask == 2 || bitmask == 4) &&
			same_station > 2 &&
			((Town*)fr->from)->population < same_station * 350) {
		return false;
	}

	/* Requiring distance to nearest station to be always under 37 tiles may be suboptimal,
	 * Especially for longer aircraft routes that start and end pretty at any arbitrary place on map
	 * While it may be nice for AI to cluster their creations together, hardcoded limit is not ideal.
	 * If AI will randomly start on some isolated spot, it will never get out of there.
	 * AI will have chance of randomly rejecting routes further than 37 tiles from their network,
	 * so there will be some attempt to cluster the network together */

	/* Random value between 37 and 292. Low values are exponentially more likely
	 * With 50% chance the value will be under 52 tiles */
	int min_distance = 36 + (1 << (Random() % 9)); // 0..8

	/* Make sure distance to closest station is < min_distance tiles. */
	if (dist != 0xFFFF && dist > min_distance) return false;

	if (_players_ai[p->index].route_type_mask != 0 &&
			!(_players_ai[p->index].route_type_mask & bitmask) &&
			!Chance16(1, 5)) {
		return false;
	}

	if (fr->cargo == CT_PASSENGERS || fr->cargo == CT_MAIL) {
		const Town* from = (const Town*)fr->from;
		const Town* to   = (const Town*)fr->to;

		if (from->pct_pass_transported > 0x99 ||
				to->pct_pass_transported > 0x99) {
			return false;
		}

		// Make sure it has a reasonably good rating
		if (from->ratings[_current_player] < -100 ||
				to->ratings[_current_player] < -100) {
			return false;
		}
	} else {
		const Industry* i = (const Industry*)fr->from;

		if (i->last_month_pct_transported[fr->cargo != i->produced_cargo[0]] > 0x99 ||
				i->last_month_production[fr->cargo != i->produced_cargo[0]] == 0) {
			return false;
		}
	}

	_players_ai[p->index].route_type_mask |= bitmask;
	return true;
}

static byte AiGetDirectionBetweenTiles(TileIndex a, TileIndex b)
{
	byte i = (TileX(a) < TileX(b)) ? 1 : 0;
	if (TileY(a) >= TileY(b)) i ^= 3;
	return i;
}

static TileIndex AiGetPctTileBetween(TileIndex a, TileIndex b, byte pct)
{
	return TileXY(
		TileX(a) + ((TileX(b) - TileX(a)) * pct >> 8),
		TileY(a) + ((TileY(b) - TileY(a)) * pct >> 8)
	);
}

static void AiWantLongIndustryRoute(Player *p)
{
	int i;
	FoundRoute fr;

	i = 60;
	for (;;) {
		// look for one from the subsidy list
		AiFindSubsidyIndustryRoute(&fr);
		if (IsInsideMM(fr.distance, 60, 90 + 1)) break;

		// try a random one
		AiFindRandomIndustryRoute(&fr);
		if (IsInsideMM(fr.distance, 60, 90 + 1)) break;

		// only test 60 times
		if (--i == 0) return;
	}

	if (!AiCheckIfRouteIsGood(p, &fr, 1)) return;

	// Fill the source field
	_players_ai[p->index].dst.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.to);
	_players_ai[p->index].src.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.from);

	_players_ai[p->index].src.use_tile = 0;
	_players_ai[p->index].src.rand_rng = 9;
	_players_ai[p->index].src.cur_building_rule = 0xFF;
	_players_ai[p->index].src.unk6 = 1;
	_players_ai[p->index].src.unk7 = 0;
	_players_ai[p->index].src.buildcmd_a = 0x24;
	_players_ai[p->index].src.buildcmd_b = 0xFF;
	_players_ai[p->index].src.direction = AiGetDirectionBetweenTiles(
		_players_ai[p->index].src.spec_tile,
		_players_ai[p->index].dst.spec_tile
	);
	_players_ai[p->index].src.cargo = fr.cargo | 0x80;

	// Fill the dest field

	_players_ai[p->index].dst.use_tile = 0;
	_players_ai[p->index].dst.rand_rng = 9;
	_players_ai[p->index].dst.cur_building_rule = 0xFF;
	_players_ai[p->index].dst.unk6 = 1;
	_players_ai[p->index].dst.unk7 = 0;
	_players_ai[p->index].dst.buildcmd_a = 0x34;
	_players_ai[p->index].dst.buildcmd_b = 0xFF;
	_players_ai[p->index].dst.direction = AiGetDirectionBetweenTiles(
		_players_ai[p->index].dst.spec_tile,
		_players_ai[p->index].src.spec_tile
	);
	_players_ai[p->index].dst.cargo = fr.cargo;

	// Fill middle field 1
	_players_ai[p->index].mid1.spec_tile = AiGetPctTileBetween(
		_players_ai[p->index].src.spec_tile,
		_players_ai[p->index].dst.spec_tile,
		0x55
	);
	_players_ai[p->index].mid1.use_tile = 0;
	_players_ai[p->index].mid1.rand_rng = 6;
	_players_ai[p->index].mid1.cur_building_rule = 0xFF;
	_players_ai[p->index].mid1.unk6 = 2;
	_players_ai[p->index].mid1.unk7 = 1;
	_players_ai[p->index].mid1.buildcmd_a = 0x30;
	_players_ai[p->index].mid1.buildcmd_b = 0xFF;
	_players_ai[p->index].mid1.direction = _players_ai[p->index].src.direction;
	_players_ai[p->index].mid1.cargo = fr.cargo;

	// Fill middle field 2
	_players_ai[p->index].mid2.spec_tile = AiGetPctTileBetween(
		_players_ai[p->index].src.spec_tile,
		_players_ai[p->index].dst.spec_tile,
		0xAA
	);
	_players_ai[p->index].mid2.use_tile = 0;
	_players_ai[p->index].mid2.rand_rng = 6;
	_players_ai[p->index].mid2.cur_building_rule = 0xFF;
	_players_ai[p->index].mid2.unk6 = 2;
	_players_ai[p->index].mid2.unk7 = 1;
	_players_ai[p->index].mid2.buildcmd_a = 0xFF;
	_players_ai[p->index].mid2.buildcmd_b = 0xFF;
	_players_ai[p->index].mid2.direction = _players_ai[p->index].dst.direction;
	_players_ai[p->index].mid2.cargo = fr.cargo;

	// Fill common fields
	_players_ai[p->index].cargo_type = fr.cargo;
	_players_ai[p->index].num_wagons = 3;
	_players_ai[p->index].build_kind = 2;
	_players_ai[p->index].num_build_rec = 4;
	_players_ai[p->index].num_loco_to_build = 2;
	_players_ai[p->index].num_want_fullload = 2;
	_players_ai[p->index].wagon_list[0] = INVALID_VEHICLE;
	_players_ai[p->index].order_list_blocks[0] = 0;
	_players_ai[p->index].order_list_blocks[1] = 1;
	_players_ai[p->index].order_list_blocks[2] = 255;

	_players_ai[p->index].state = AIS_BUILD_DEFAULT_RAIL_BLOCKS;
	_players_ai[p->index].state_mode = UCHAR_MAX;
	_players_ai[p->index].state_counter = 0;
	_players_ai[p->index].timeout_counter = 0;
}

static void AiWantMediumIndustryRoute(Player *p)
{
	int i;
	FoundRoute fr;

	i = 60;
	for (;;) {
		// look for one from the subsidy list
		AiFindSubsidyIndustryRoute(&fr);
		if (IsInsideMM(fr.distance, 40, 60 + 1)) break;

		// try a random one
		AiFindRandomIndustryRoute(&fr);
		if (IsInsideMM(fr.distance, 40, 60 + 1)) break;

		// only test 60 times
		if (--i == 0) return;
	}

	if (!AiCheckIfRouteIsGood(p, &fr, 1)) return;

	// Fill the source field
	_players_ai[p->index].src.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.from);
	_players_ai[p->index].src.use_tile = 0;
	_players_ai[p->index].src.rand_rng = 9;
	_players_ai[p->index].src.cur_building_rule = 0xFF;
	_players_ai[p->index].src.unk6 = 1;
	_players_ai[p->index].src.unk7 = 0;
	_players_ai[p->index].src.buildcmd_a = 0x10;
	_players_ai[p->index].src.buildcmd_b = 0xFF;
	_players_ai[p->index].src.direction = AiGetDirectionBetweenTiles(
		GET_TOWN_OR_INDUSTRY_TILE(fr.from),
		GET_TOWN_OR_INDUSTRY_TILE(fr.to)
	);
	_players_ai[p->index].src.cargo = fr.cargo | 0x80;

	// Fill the dest field
	_players_ai[p->index].dst.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.to);
	_players_ai[p->index].dst.use_tile = 0;
	_players_ai[p->index].dst.rand_rng = 9;
	_players_ai[p->index].dst.cur_building_rule = 0xFF;
	_players_ai[p->index].dst.unk6 = 1;
	_players_ai[p->index].dst.unk7 = 0;
	_players_ai[p->index].dst.buildcmd_a = 0xFF;
	_players_ai[p->index].dst.buildcmd_b = 0xFF;
	_players_ai[p->index].dst.direction = AiGetDirectionBetweenTiles(
		GET_TOWN_OR_INDUSTRY_TILE(fr.to),
		GET_TOWN_OR_INDUSTRY_TILE(fr.from)
	);
	_players_ai[p->index].dst.cargo = fr.cargo;

	// Fill common fields
	_players_ai[p->index].cargo_type = fr.cargo;
	_players_ai[p->index].num_wagons = 3;
	_players_ai[p->index].build_kind = 1;
	_players_ai[p->index].num_build_rec = 2;
	_players_ai[p->index].num_loco_to_build = 1;
	_players_ai[p->index].num_want_fullload = 1;
	_players_ai[p->index].wagon_list[0] = INVALID_VEHICLE;
	_players_ai[p->index].order_list_blocks[0] = 0;
	_players_ai[p->index].order_list_blocks[1] = 1;
	_players_ai[p->index].order_list_blocks[2] = 255;
	_players_ai[p->index].state = AIS_BUILD_DEFAULT_RAIL_BLOCKS;
	_players_ai[p->index].state_mode = UCHAR_MAX;
	_players_ai[p->index].state_counter = 0;
	_players_ai[p->index].timeout_counter = 0;
}

static void AiWantShortIndustryRoute(Player *p)
{
	int i;
	FoundRoute fr;

	i = 60;
	for (;;) {
		// look for one from the subsidy list
		AiFindSubsidyIndustryRoute(&fr);
		if (IsInsideMM(fr.distance, 15, 40 + 1)) break;

		// try a random one
		AiFindRandomIndustryRoute(&fr);
		if (IsInsideMM(fr.distance, 15, 40 + 1)) break;

		// only test 60 times
		if (--i == 0) return;
	}

	if (!AiCheckIfRouteIsGood(p, &fr, 1)) return;

	// Fill the source field
	_players_ai[p->index].src.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.from);
	_players_ai[p->index].src.use_tile = 0;
	_players_ai[p->index].src.rand_rng = 9;
	_players_ai[p->index].src.cur_building_rule = 0xFF;
	_players_ai[p->index].src.unk6 = 1;
	_players_ai[p->index].src.unk7 = 0;
	_players_ai[p->index].src.buildcmd_a = 0x10;
	_players_ai[p->index].src.buildcmd_b = 0xFF;
	_players_ai[p->index].src.direction = AiGetDirectionBetweenTiles(
		GET_TOWN_OR_INDUSTRY_TILE(fr.from),
		GET_TOWN_OR_INDUSTRY_TILE(fr.to)
	);
	_players_ai[p->index].src.cargo = fr.cargo | 0x80;

	// Fill the dest field
	_players_ai[p->index].dst.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.to);
	_players_ai[p->index].dst.use_tile = 0;
	_players_ai[p->index].dst.rand_rng = 9;
	_players_ai[p->index].dst.cur_building_rule = 0xFF;
	_players_ai[p->index].dst.unk6 = 1;
	_players_ai[p->index].dst.unk7 = 0;
	_players_ai[p->index].dst.buildcmd_a = 0xFF;
	_players_ai[p->index].dst.buildcmd_b = 0xFF;
	_players_ai[p->index].dst.direction = AiGetDirectionBetweenTiles(
		GET_TOWN_OR_INDUSTRY_TILE(fr.to),
		GET_TOWN_OR_INDUSTRY_TILE(fr.from)
	);
	_players_ai[p->index].dst.cargo = fr.cargo;

	// Fill common fields
	_players_ai[p->index].cargo_type = fr.cargo;
	_players_ai[p->index].num_wagons = 2;
	_players_ai[p->index].build_kind = 1;
	_players_ai[p->index].num_build_rec = 2;
	_players_ai[p->index].num_loco_to_build = 1;
	_players_ai[p->index].num_want_fullload = 1;
	_players_ai[p->index].wagon_list[0] = INVALID_VEHICLE;
	_players_ai[p->index].order_list_blocks[0] = 0;
	_players_ai[p->index].order_list_blocks[1] = 1;
	_players_ai[p->index].order_list_blocks[2] = 255;
	_players_ai[p->index].state = AIS_BUILD_DEFAULT_RAIL_BLOCKS;
	_players_ai[p->index].state_mode = UCHAR_MAX;
	_players_ai[p->index].state_counter = 0;
	_players_ai[p->index].timeout_counter = 0;
}

static void AiWantMailRoute(Player *p)
{
	int i;
	FoundRoute fr;

	i = 60;
	for (;;) {
		// look for one from the subsidy list
		AiFindSubsidyPassengerRoute(&fr);
		if (IsInsideMM(fr.distance, 60, 110 + 1)) break;

		// try a random one
		AiFindRandomPassengerRoute(&fr);
		if (IsInsideMM(fr.distance, 60, 110 + 1)) break;

		// only test 60 times
		if (--i == 0) return;
	}

	fr.cargo = CT_MAIL;
	if (!AiCheckIfRouteIsGood(p, &fr, 1)) return;

	// Fill the source field
	_players_ai[p->index].src.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.from);
	_players_ai[p->index].src.use_tile = 0;
	_players_ai[p->index].src.rand_rng = 7;
	_players_ai[p->index].src.cur_building_rule = 0xFF;
	_players_ai[p->index].src.unk6 = 1;
	_players_ai[p->index].src.unk7 = 0;
	_players_ai[p->index].src.buildcmd_a = 0x24;
	_players_ai[p->index].src.buildcmd_b = 0xFF;
	_players_ai[p->index].src.direction = AiGetDirectionBetweenTiles(
		GET_TOWN_OR_INDUSTRY_TILE(fr.from),
		GET_TOWN_OR_INDUSTRY_TILE(fr.to)
	);
	_players_ai[p->index].src.cargo = fr.cargo;

	// Fill the dest field
	_players_ai[p->index].dst.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.to);
	_players_ai[p->index].dst.use_tile = 0;
	_players_ai[p->index].dst.rand_rng = 7;
	_players_ai[p->index].dst.cur_building_rule = 0xFF;
	_players_ai[p->index].dst.unk6 = 1;
	_players_ai[p->index].dst.unk7 = 0;
	_players_ai[p->index].dst.buildcmd_a = 0x34;
	_players_ai[p->index].dst.buildcmd_b = 0xFF;
	_players_ai[p->index].dst.direction = AiGetDirectionBetweenTiles(
		GET_TOWN_OR_INDUSTRY_TILE(fr.to),
		GET_TOWN_OR_INDUSTRY_TILE(fr.from)
	);
	_players_ai[p->index].dst.cargo = fr.cargo;

	// Fill middle field 1
	_players_ai[p->index].mid1.spec_tile = AiGetPctTileBetween(
		GET_TOWN_OR_INDUSTRY_TILE(fr.from),
		GET_TOWN_OR_INDUSTRY_TILE(fr.to),
		0x55
	);
	_players_ai[p->index].mid1.use_tile = 0;
	_players_ai[p->index].mid1.rand_rng = 6;
	_players_ai[p->index].mid1.cur_building_rule = 0xFF;
	_players_ai[p->index].mid1.unk6 = 2;
	_players_ai[p->index].mid1.unk7 = 1;
	_players_ai[p->index].mid1.buildcmd_a = 0x30;
	_players_ai[p->index].mid1.buildcmd_b = 0xFF;
	_players_ai[p->index].mid1.direction = _players_ai[p->index].src.direction;
	_players_ai[p->index].mid1.cargo = fr.cargo;

	// Fill middle field 2
	_players_ai[p->index].mid2.spec_tile = AiGetPctTileBetween(
		GET_TOWN_OR_INDUSTRY_TILE(fr.from),
		GET_TOWN_OR_INDUSTRY_TILE(fr.to),
		0xAA
	);
	_players_ai[p->index].mid2.use_tile = 0;
	_players_ai[p->index].mid2.rand_rng = 6;
	_players_ai[p->index].mid2.cur_building_rule = 0xFF;
	_players_ai[p->index].mid2.unk6 = 2;
	_players_ai[p->index].mid2.unk7 = 1;
	_players_ai[p->index].mid2.buildcmd_a = 0xFF;
	_players_ai[p->index].mid2.buildcmd_b = 0xFF;
	_players_ai[p->index].mid2.direction = _players_ai[p->index].dst.direction;
	_players_ai[p->index].mid2.cargo = fr.cargo;

	// Fill common fields
	_players_ai[p->index].cargo_type = fr.cargo;
	_players_ai[p->index].num_wagons = 3;
	_players_ai[p->index].build_kind = 2;
	_players_ai[p->index].num_build_rec = 4;
	_players_ai[p->index].num_loco_to_build = 2;
	_players_ai[p->index].num_want_fullload = 0;
	_players_ai[p->index].wagon_list[0] = INVALID_VEHICLE;
	_players_ai[p->index].order_list_blocks[0] = 0;
	_players_ai[p->index].order_list_blocks[1] = 1;
	_players_ai[p->index].order_list_blocks[2] = 255;
	_players_ai[p->index].state = AIS_BUILD_DEFAULT_RAIL_BLOCKS;
	_players_ai[p->index].state_mode = UCHAR_MAX;
	_players_ai[p->index].state_counter = 0;
	_players_ai[p->index].timeout_counter = 0;
}

static void AiWantPassengerRoute(Player *p)
{
	int i;
	FoundRoute fr;

	i = 60;
	for (;;) {
		// look for one from the subsidy list
		AiFindSubsidyPassengerRoute(&fr);
		if (IsInsideMM(fr.distance, 0, 55 + 1)) break;

		// try a random one
		AiFindRandomPassengerRoute(&fr);
		if (IsInsideMM(fr.distance, 0, 55 + 1)) break;

		// only test 60 times
		if (--i == 0) return;
	}

	fr.cargo = CT_PASSENGERS;
	if (!AiCheckIfRouteIsGood(p, &fr, 1)) return;

	// Fill the source field
	_players_ai[p->index].src.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.from);
	_players_ai[p->index].src.use_tile = 0;
	_players_ai[p->index].src.rand_rng = 7;
	_players_ai[p->index].src.cur_building_rule = 0xFF;
	_players_ai[p->index].src.unk6 = 1;
	_players_ai[p->index].src.unk7 = 0;
	_players_ai[p->index].src.buildcmd_a = 0x10;
	_players_ai[p->index].src.buildcmd_b = 0xFF;
	_players_ai[p->index].src.direction = AiGetDirectionBetweenTiles(
		GET_TOWN_OR_INDUSTRY_TILE(fr.from),
		GET_TOWN_OR_INDUSTRY_TILE(fr.to)
	);
	_players_ai[p->index].src.cargo = fr.cargo;

	// Fill the dest field
	_players_ai[p->index].dst.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.to);
	_players_ai[p->index].dst.use_tile = 0;
	_players_ai[p->index].dst.rand_rng = 7;
	_players_ai[p->index].dst.cur_building_rule = 0xFF;
	_players_ai[p->index].dst.unk6 = 1;
	_players_ai[p->index].dst.unk7 = 0;
	_players_ai[p->index].dst.buildcmd_a = 0xFF;
	_players_ai[p->index].dst.buildcmd_b = 0xFF;
	_players_ai[p->index].dst.direction = AiGetDirectionBetweenTiles(
		GET_TOWN_OR_INDUSTRY_TILE(fr.to),
		GET_TOWN_OR_INDUSTRY_TILE(fr.from)
	);
	_players_ai[p->index].dst.cargo = fr.cargo;

	// Fill common fields
	_players_ai[p->index].cargo_type = fr.cargo;
	_players_ai[p->index].num_wagons = 2;
	_players_ai[p->index].build_kind = 1;
	_players_ai[p->index].num_build_rec = 2;
	_players_ai[p->index].num_loco_to_build = 1;
	_players_ai[p->index].num_want_fullload = 0;
	_players_ai[p->index].wagon_list[0] = INVALID_VEHICLE;
	_players_ai[p->index].order_list_blocks[0] = 0;
	_players_ai[p->index].order_list_blocks[1] = 1;
	_players_ai[p->index].order_list_blocks[2] = 255;
	_players_ai[p->index].state = AIS_BUILD_DEFAULT_RAIL_BLOCKS;
	_players_ai[p->index].state_mode = UCHAR_MAX;
	_players_ai[p->index].state_counter = 0;
	_players_ai[p->index].timeout_counter = 0;
}

static void AiWantTrainRoute(Player *p)
{
	uint16 r = GB(Random(), 0, 16);

	_players_ai[p->index].railtype_to_use = GetBestRailtype(p->index);

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
	for (;;) {
		// look for one from the subsidy list
		AiFindSubsidyIndustryRoute(&fr);
		if (IsInsideMM(fr.distance, 35, 55 + 1)) break;

		// try a random one
		AiFindRandomIndustryRoute(&fr);
		if (IsInsideMM(fr.distance, 35, 55 + 1)) break;

		// only test 60 times
		if (--i == 0) return;
	}

	if (!AiCheckIfRouteIsGood(p, &fr, 2)) return;

	// Fill the source field
	_players_ai[p->index].src.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.from);
	_players_ai[p->index].src.use_tile = 0;
	_players_ai[p->index].src.rand_rng = 9;
	_players_ai[p->index].src.cur_building_rule = 0xFF;
	_players_ai[p->index].src.buildcmd_a = 1;
	_players_ai[p->index].src.direction = 0;
	_players_ai[p->index].src.cargo = fr.cargo | 0x80;

	// Fill the dest field
	_players_ai[p->index].dst.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.to);
	_players_ai[p->index].dst.use_tile = 0;
	_players_ai[p->index].dst.rand_rng = 9;
	_players_ai[p->index].dst.cur_building_rule = 0xFF;
	_players_ai[p->index].dst.buildcmd_a = 0xFF;
	_players_ai[p->index].dst.direction = 0;
	_players_ai[p->index].dst.cargo = fr.cargo;

	// Fill common fields
	_players_ai[p->index].cargo_type = fr.cargo;
	_players_ai[p->index].num_build_rec = 2;
	_players_ai[p->index].num_loco_to_build = 5;
	_players_ai[p->index].num_want_fullload = 5;

//	_players_ai[p->index].loco_id = INVALID_VEHICLE;
	_players_ai[p->index].order_list_blocks[0] = 0;
	_players_ai[p->index].order_list_blocks[1] = 1;
	_players_ai[p->index].order_list_blocks[2] = 255;

	_players_ai[p->index].state = AIS_BUILD_DEFAULT_ROAD_BLOCKS;
	_players_ai[p->index].state_mode = UCHAR_MAX;
	_players_ai[p->index].state_counter = 0;
	_players_ai[p->index].timeout_counter = 0;
}

static void AiWantMediumRoadIndustryRoute(Player *p)
{
	int i;
	FoundRoute fr;

	i = 60;
	for (;;) {
		// look for one from the subsidy list
		AiFindSubsidyIndustryRoute(&fr);
		if (IsInsideMM(fr.distance, 15, 40 + 1)) break;

		// try a random one
		AiFindRandomIndustryRoute(&fr);
		if (IsInsideMM(fr.distance, 15, 40 + 1)) break;

		// only test 60 times
		if (--i == 0) return;
	}

	if (!AiCheckIfRouteIsGood(p, &fr, 2)) return;

	// Fill the source field
	_players_ai[p->index].src.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.from);
	_players_ai[p->index].src.use_tile = 0;
	_players_ai[p->index].src.rand_rng = 9;
	_players_ai[p->index].src.cur_building_rule = 0xFF;
	_players_ai[p->index].src.buildcmd_a = 1;
	_players_ai[p->index].src.direction = 0;
	_players_ai[p->index].src.cargo = fr.cargo | 0x80;

	// Fill the dest field
	_players_ai[p->index].dst.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.to);
	_players_ai[p->index].dst.use_tile = 0;
	_players_ai[p->index].dst.rand_rng = 9;
	_players_ai[p->index].dst.cur_building_rule = 0xFF;
	_players_ai[p->index].dst.buildcmd_a = 0xFF;
	_players_ai[p->index].dst.direction = 0;
	_players_ai[p->index].dst.cargo = fr.cargo;

	// Fill common fields
	_players_ai[p->index].cargo_type = fr.cargo;
	_players_ai[p->index].num_build_rec = 2;
	_players_ai[p->index].num_loco_to_build = 3;
	_players_ai[p->index].num_want_fullload = 3;

//	_players_ai[p->index].loco_id = INVALID_VEHICLE;
	_players_ai[p->index].order_list_blocks[0] = 0;
	_players_ai[p->index].order_list_blocks[1] = 1;
	_players_ai[p->index].order_list_blocks[2] = 255;

	_players_ai[p->index].state = AIS_BUILD_DEFAULT_ROAD_BLOCKS;
	_players_ai[p->index].state_mode = UCHAR_MAX;
	_players_ai[p->index].state_counter = 0;
	_players_ai[p->index].timeout_counter = 0;
}

static void AiWantLongRoadPassengerRoute(Player *p)
{
	int i;
	FoundRoute fr;

	i = 60;
	for (;;) {
		// look for one from the subsidy list
		AiFindSubsidyPassengerRoute(&fr);
		if (IsInsideMM(fr.distance, 55, 180 + 1)) break;

		// try a random one
		AiFindRandomPassengerRoute(&fr);
		if (IsInsideMM(fr.distance, 55, 180 + 1)) break;

		// only test 60 times
		if (--i == 0) return;
	}

	fr.cargo = CT_PASSENGERS;

	if (!AiCheckIfRouteIsGood(p, &fr, 2)) return;

	// Fill the source field
	_players_ai[p->index].src.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.to);
	_players_ai[p->index].src.use_tile = 0;
	_players_ai[p->index].src.rand_rng = 10;
	_players_ai[p->index].src.cur_building_rule = 0xFF;
	_players_ai[p->index].src.buildcmd_a = 1;
	_players_ai[p->index].src.direction = 0;
	_players_ai[p->index].src.cargo = CT_PASSENGERS;

	// Fill the dest field
	_players_ai[p->index].dst.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.from);
	_players_ai[p->index].dst.use_tile = 0;
	_players_ai[p->index].dst.rand_rng = 10;
	_players_ai[p->index].dst.cur_building_rule = 0xFF;
	_players_ai[p->index].dst.buildcmd_a = 0xFF;
	_players_ai[p->index].dst.direction = 0;
	_players_ai[p->index].dst.cargo = CT_PASSENGERS;

	// Fill common fields
	_players_ai[p->index].cargo_type = CT_PASSENGERS;
	_players_ai[p->index].num_build_rec = 2;
	_players_ai[p->index].num_loco_to_build = 4;
	_players_ai[p->index].num_want_fullload = 0;

//	_players_ai[p->index].loco_id = INVALID_VEHICLE;
	_players_ai[p->index].order_list_blocks[0] = 0;
	_players_ai[p->index].order_list_blocks[1] = 1;
	_players_ai[p->index].order_list_blocks[2] = 255;

	_players_ai[p->index].state = AIS_BUILD_DEFAULT_ROAD_BLOCKS;
	_players_ai[p->index].state_mode = UCHAR_MAX;
	_players_ai[p->index].state_counter = 0;
	_players_ai[p->index].timeout_counter = 0;
}

static void AiWantPassengerRouteInsideTown(Player *p)
{
	int i;
	FoundRoute fr;
	Town *t;

	i = 60;
	for (;;) {
		// Find a town big enough
		t = AiFindRandomTown();
		if (t != NULL && t->population >= 700) break;

		// only test 60 times
		if (--i == 0) return;
	}

	fr.cargo = CT_PASSENGERS;
	fr.from = fr.to = t;

	if (!AiCheckIfRouteIsGood(p, &fr, 2)) return;

	// Fill the source field
	_players_ai[p->index].src.spec_tile = t->xy;
	_players_ai[p->index].src.use_tile = 0;
	_players_ai[p->index].src.rand_rng = 10;
	_players_ai[p->index].src.cur_building_rule = 0xFF;
	_players_ai[p->index].src.buildcmd_a = 1;
	_players_ai[p->index].src.direction = 0;
	_players_ai[p->index].src.cargo = CT_PASSENGERS;

	// Fill the dest field
	_players_ai[p->index].dst.spec_tile = t->xy;
	_players_ai[p->index].dst.use_tile = 0;
	_players_ai[p->index].dst.rand_rng = 10;
	_players_ai[p->index].dst.cur_building_rule = 0xFF;
	_players_ai[p->index].dst.buildcmd_a = 0xFF;
	_players_ai[p->index].dst.direction = 0;
	_players_ai[p->index].dst.cargo = CT_PASSENGERS;

	// Fill common fields
	_players_ai[p->index].cargo_type = CT_PASSENGERS;
	_players_ai[p->index].num_build_rec = 2;
	_players_ai[p->index].num_loco_to_build = 2;
	_players_ai[p->index].num_want_fullload = 0;

//	_players_ai[p->index].loco_id = INVALID_VEHICLE;
	_players_ai[p->index].order_list_blocks[0] = 0;
	_players_ai[p->index].order_list_blocks[1] = 1;
	_players_ai[p->index].order_list_blocks[2] = 255;

	_players_ai[p->index].state = AIS_BUILD_DEFAULT_ROAD_BLOCKS;
	_players_ai[p->index].state_mode = UCHAR_MAX;
	_players_ai[p->index].state_counter = 0;
	_players_ai[p->index].timeout_counter = 0;
}

static void AiWantRoadRoute(Player *p)
{
	uint16 r = GB(Random(), 0, 16);

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

	/* Get aircraft that would be bought for this route
	 * (probably, as conditions may change before the route is fully built,
	 * like running out of money and having to select different aircraft, etc ...) */
	EngineID veh = AiChooseAircraftToBuild(p->player_money, _players_ai[p->index].build_kind != 0 ? AIR_CTOL : 0);

	/* No aircraft buildable mean no aircraft route */
	if (veh == INVALID_ENGINE) return;

	const AircraftVehicleInfo *avi = AircraftVehInfo(veh);

	/* For passengers, "optimal" number of days in transit is about 80 to 100
	 * Calculate "maximum optimal number of squares" from speed for 80 days
	 * 20 days should be enough for takeoff, land, taxi, etc ...
	 *
	 * "A vehicle traveling at 100kph will cross 5.6 tiles per day" ->
	 * Since in table aircraft speeds are in "real km/h", this should be accurate
	 * We get max_squares = avi->max_speed * 5.6 / 100.0 * 80 */
	int max_squares = avi->max_speed * 448 / 100;

	/* For example this will be 10456 tiles for 2334 km/h aircrafts with realistic aircraft speeds
	 * and 836 with "unrealistic" speeds, much more than the original 95 squares limit
	 *
	 * Size of the map, if not rectangular, it is the larger dimension of it
	 */
	int map_size = max(MapSizeX(), MapSizeY());

	/* Minimum distance between airports is half of map size, clamped between 1% and 20% of optimum.
	 * May prevent building plane routes at all on small maps, but they will be ineffective there, so
	 * it is feature, not a bug.
	 * On smaller distances, buses or trains are usually more effective approach anyway.
	 * Additional safeguard is needing at least 20 squares,
	 * which may trigger in highly unusual configurations */
	int min_squares = max(20, max(max_squares / 100, min(max_squares / 5, map_size / 2)));

	/* Should not happen, unless aircraft with real speed under approx. 5 km/h is selected.
	 * No such exist, unless using some NewGRF with ballons, zeppelins or similar
	 * slow-moving stuff. In that case, bail out, it is faster to walk by foot anyway :). */
	if (max_squares < min_squares) return;

	i = 60;
	for (;;) {

		// look for one from the subsidy list
		AiFindSubsidyPassengerRoute(&fr);
		if (IsInsideMM(fr.distance, min_squares, max_squares + 1)) break;

		// try a random one
		AiFindRandomPassengerRoute(&fr);
		if (IsInsideMM(fr.distance, min_squares, max_squares + 1)) break;

		// only test 60 times
		if (--i == 0) return;
	}

	fr.cargo = CT_PASSENGERS;
	if (!AiCheckIfRouteIsGood(p, &fr, 4)) return;


	// Fill the source field
	_players_ai[p->index].src.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.to);
	_players_ai[p->index].src.use_tile = 0;
	_players_ai[p->index].src.rand_rng = 12;
	_players_ai[p->index].src.cur_building_rule = 0xFF;
	_players_ai[p->index].src.cargo = fr.cargo;

	// Fill the dest field
	_players_ai[p->index].dst.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.from);
	_players_ai[p->index].dst.use_tile = 0;
	_players_ai[p->index].dst.rand_rng = 12;
	_players_ai[p->index].dst.cur_building_rule = 0xFF;
	_players_ai[p->index].dst.cargo = fr.cargo;

	// Fill common fields
	_players_ai[p->index].cargo_type = fr.cargo;
	_players_ai[p->index].build_kind = 0;
	_players_ai[p->index].num_build_rec = 2;
	_players_ai[p->index].num_loco_to_build = 1;
	/* Using full load always may not be the best.
	 * Pick random value and rely on selling the vehicle & route
	 * afterwards if the choice was utterly wrong (or maybe altering the value if AI is improved)
	 * When traffic is very low or very assymetric, is is better not to full load
	 * When traffic is high, full/non-full make no difference
	 * It should be better to run with aircraft only one way full 6 times per year,
	 * rather than two way full 1 times.
	 * Practical experiments with AI show that the non-full-load aircrafts are usually
	 * those that survive
	 * Also, non-full load is more resistant against starving (by building better stations
	 * or using exclusive rights)
	 */
	_players_ai[p->index].num_want_fullload = Chance16(1, 5); // 20% chance
//	_players_ai[p->index].loco_id = INVALID_VEHICLE;
	_players_ai[p->index].order_list_blocks[0] = 0;
	_players_ai[p->index].order_list_blocks[1] = 1;
	_players_ai[p->index].order_list_blocks[2] = 255;

	_players_ai[p->index].state = AIS_AIRPORT_STUFF;
	_players_ai[p->index].timeout_counter = 0;
}

static void AiWantOilRigAircraftRoute(Player *p)
{
	int i;
	FoundRoute fr;
	Town *t;
	Industry *in;

	i = 60;
	for (;;) {
		// Find a town
		t = AiFindRandomTown();
		if (t != NULL) {
			// Find a random oil rig industry
			in = AiFindRandomIndustry();
			if (in != NULL && GetIndustrySpec(in->type)->behaviour & INDUSTRYBEH_AI_AIRSHIP_ROUTES) {
				if (DistanceManhattan(t->xy, in->xy) < 60)
					break;
			}
		}

		// only test 60 times
		if (--i == 0) return;
	}

	fr.cargo = CT_PASSENGERS;
	fr.from = fr.to = t;

	if (!AiCheckIfRouteIsGood(p, &fr, 4)) return;

	// Fill the source field
	_players_ai[p->index].src.spec_tile = t->xy;
	_players_ai[p->index].src.use_tile = 0;
	_players_ai[p->index].src.rand_rng = 12;
	_players_ai[p->index].src.cur_building_rule = 0xFF;
	_players_ai[p->index].src.cargo = CT_PASSENGERS;

	// Fill the dest field
	_players_ai[p->index].dst.spec_tile = in->xy;
	_players_ai[p->index].dst.use_tile = 0;
	_players_ai[p->index].dst.rand_rng = 5;
	_players_ai[p->index].dst.cur_building_rule = 0xFF;
	_players_ai[p->index].dst.cargo = CT_PASSENGERS;

	// Fill common fields
	_players_ai[p->index].cargo_type = CT_PASSENGERS;
	_players_ai[p->index].build_kind = 1;
	_players_ai[p->index].num_build_rec = 2;
	_players_ai[p->index].num_loco_to_build = 1;
	_players_ai[p->index].num_want_fullload = 0;
//	_players_ai[p->index].loco_id = INVALID_VEHICLE;
	_players_ai[p->index].order_list_blocks[0] = 0;
	_players_ai[p->index].order_list_blocks[1] = 1;
	_players_ai[p->index].order_list_blocks[2] = 255;

	_players_ai[p->index].state = AIS_AIRPORT_STUFF;
	_players_ai[p->index].timeout_counter = 0;
}

static void AiWantAircraftRoute(Player *p)
{
	uint16 r = (uint16)Random();

	if (r >= 0x2AAA || _date < 0x3912 + DAYS_TILL_ORIGINAL_BASE_YEAR) {
		AiWantPassengerAircraftRoute(p);
	} else {
		AiWantOilRigAircraftRoute(p);
	}
}



static void AiStateWantNewRoute(Player *p)
{
	uint16 r;
	int i;

	if (p->player_money < AiGetBasePrice(p) * 500) {
		_players_ai[p->index].state = AIS_0;
		return;
	}

	i = 200;
	for (;;) {
		r = (uint16)Random();

		if (_patches.ai_disable_veh_train &&
				_patches.ai_disable_veh_roadveh &&
				_patches.ai_disable_veh_aircraft &&
				_patches.ai_disable_veh_ship) {
			return;
		}

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
			/* Ships are not implemented in this (broken) AI */
		}

		// got a route?
		if (_players_ai[p->index].state != AIS_WANT_NEW_ROUTE) break;

		// time out?
		if (--i == 0) {
			if (++_players_ai[p->index].state_counter == 556) _players_ai[p->index].state = AIS_0;
			break;
		}
	}
}

static bool AiCheckTrackResources(TileIndex tile, const AiDefaultBlockData *p, byte cargo)
{
	uint rad = (_patches.modified_catchment) ? CA_TRAIN : CA_UNMODIFIED;

	for (; p->mode != 4; p++) {
		AcceptedCargo values;
		TileIndex tile2;
		uint w;
		uint h;

		if (p->mode != 1) continue;

		tile2 = TILE_ADD(tile, ToTileIndexDiff(p->tileoffs));
		w = GB(p->attr, 1, 3);
		h = GB(p->attr, 4, 3);

		if (p->attr & 1) Swap(w, h);

		if (cargo & 0x80) {
			GetProductionAroundTiles(values, tile2, w, h, rad);
			return values[cargo & 0x7F] != 0;
		} else {
			GetAcceptanceAroundTiles(values, tile2, w, h, rad);
			if (!(values[cargo] & ~7))
				return false;
			if (cargo != CT_MAIL)
				return true;
			return !!((values[cargo] >> 1) & ~7);
		}
	}

	return true;
}

static CommandCost AiDoBuildDefaultRailTrack(TileIndex tile, const AiDefaultBlockData* p, RailType railtype, byte flag)
{
	CommandCost ret;
	CommandCost total_cost(EXPENSES_CONSTRUCTION);
	Town *t = NULL;
	int rating = 0;
	int i, j, k;

	for (;;) {
		// This will seldomly overflow for valid reasons. Mask it to be on the safe side.
		uint c = TILE_MASK(tile + ToTileIndexDiff(p->tileoffs));

		_cleared_town = NULL;

		if (p->mode < 2) {
			if (p->mode == 0) {
				// Depot
				ret = DoCommand(c, railtype, p->attr, flag | DC_AUTO | DC_NO_WATER | DC_AI_BUILDING, CMD_BUILD_TRAIN_DEPOT);
			} else {
				// Station
				ret = DoCommand(c, (p->attr & 1) | (p->attr >> 4) << 8 | (p->attr >> 1 & 7) << 16, railtype, flag | DC_AUTO | DC_NO_WATER | DC_AI_BUILDING, CMD_BUILD_RAILROAD_STATION);
			}

			if (CmdFailed(ret)) return CMD_ERROR;
			total_cost.AddCost(ret);

clear_town_stuff:;
			if (_cleared_town != NULL) {
				if (t != NULL && t != _cleared_town)
					return CMD_ERROR;
				t = _cleared_town;
				rating += _cleared_town_rating;
			}
		} else if (p->mode == 2) {
			/* Rail */
			if (IsTileType(c, MP_RAILWAY)) return CMD_ERROR;

			j = p->attr;
			k = 0;

			/* Build the rail
			 * note: FOR_EACH_SET_BIT cannot be used here
			 */
			for (i = 0; i != 6; i++, j >>= 1) {
				if (j & 1) {
					k = i;
					ret = DoCommand(c, railtype, i, flag | DC_AUTO | DC_NO_WATER, CMD_BUILD_SINGLE_RAIL);
					if (CmdFailed(ret)) return CMD_ERROR;
					total_cost.AddCost(ret);
				}
			}

			/* signals too? */
			if (j & 3) {
				// Can't build signals on a road.
				if (IsTileType(c, MP_ROAD)) return CMD_ERROR;

				if (flag & DC_EXEC) {
					j = 4 - j;
					do {
						ret = DoCommand(c, k, 0, flag, CMD_BUILD_SIGNALS);
					} while (--j);
				} else {
					ret.AddCost(_price.build_signals);
				}
				if (CmdFailed(ret)) return CMD_ERROR;
				total_cost.AddCost(ret);
			}
		} else if (p->mode == 3) {
			//Clear stuff and then build single rail.
			if (GetTileSlope(c, NULL) != SLOPE_FLAT) return CMD_ERROR;
			ret = DoCommand(c, 0, 0, flag | DC_AUTO | DC_NO_WATER | DC_AI_BUILDING, CMD_LANDSCAPE_CLEAR);
			if (CmdFailed(ret)) return CMD_ERROR;
			total_cost.AddCost(ret);
			total_cost.AddCost(_price.build_rail);

			if (flag & DC_EXEC) {
				DoCommand(c, railtype, p->attr & 1, flag | DC_AUTO | DC_NO_WATER | DC_AI_BUILDING, CMD_BUILD_SINGLE_RAIL);
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
static int AiBuildDefaultRailTrack(TileIndex tile, byte p0, byte p1, byte p2, byte p3, byte dir, byte cargo, RailType railtype, CommandCost* cost)
{
	int i;
	const AiDefaultRailBlock *p;

	for (i = 0; (p = _default_rail_track_data[i]) != NULL; i++) {
		if (p->p0 == p0 && p->p1 == p1 && p->p2 == p2 && p->p3 == p3 &&
				(p->dir == 0xFF || p->dir == dir || ((p->dir - 1) & 3) == dir)) {
			*cost = AiDoBuildDefaultRailTrack(tile, p->data, railtype, DC_NO_TOWN_RATING);
			if (CmdSucceeded(*cost) && AiCheckTrackResources(tile, p->data, cargo))
				return i;
		}
	}

	return -1;
}

static const byte _terraform_up_flags[] = {
	14, 13, 12, 11,
	10,  9,  8,  7,
	 6,  5,  4,  3,
	 2,  1,  0,  1,
	 2,  1,  4,  1,
	 2,  1,  8,  1,
	 2,  1,  4,  2,
	 2,  1
};

static const byte _terraform_down_flags[] = {
	1,  2, 3,  4,
	5,  6, 1,  8,
	9, 10, 8, 12,
	4,  2, 0,  0,
	1,  2, 3,  4,
	5,  6, 2,  8,
	9, 10, 1, 12,
	8,  4
};

static void AiDoTerraformLand(TileIndex tile, DiagDirection dir, int unk, int mode)
{
	PlayerID old_player;
	uint32 r;
	Slope slope;
	uint h;

	old_player = _current_player;
	_current_player = OWNER_NONE;

	r = Random();

	unk &= (int)r;

	do {
		tile = TILE_MASK(tile + TileOffsByDiagDir(dir));

		r >>= 2;
		if (r & 2) {
			dir = ChangeDiagDir(dir, (r & 1) ? DIAGDIRDIFF_90LEFT : DIAGDIRDIFF_90RIGHT);
		}
	} while (--unk >= 0);

	slope = GetTileSlope(tile, &h);

	if (slope != SLOPE_FLAT) {
		if (mode > 0 || (mode == 0 && !(r & 0xC))) {
			// Terraform up
			DoCommand(tile, _terraform_up_flags[slope - 1], 1,
				DC_EXEC | DC_AUTO | DC_NO_WATER, CMD_TERRAFORM_LAND);
		} else if (h != 0) {
			// Terraform down
			DoCommand(tile, _terraform_down_flags[slope - 1], 0,
				DC_EXEC | DC_AUTO | DC_NO_WATER, CMD_TERRAFORM_LAND);
		}
	}

	_current_player = old_player;
}

static void AiStateBuildDefaultRailBlocks(Player *p)
{
	uint i;
	int j;
	AiBuildRec *aib;
	int rule;
	CommandCost cost;

	// time out?
	if (++_players_ai[p->index].timeout_counter == 1388) {
		_players_ai[p->index].state = AIS_DELETE_RAIL_BLOCKS;
		return;
	}

	// do the following 8 times
	for (i = 0; i < 8; i++) {
		// check if we can build the default track
		aib = &_players_ai[p->index].src;
		j = _players_ai[p->index].num_build_rec;
		do {
			// this item has already been built?
			if (aib->cur_building_rule != 255) continue;

			// adjust the coordinate randomly,
			// to make sure that we find a position.
			aib->use_tile = AdjustTileCoordRandomly(aib->spec_tile, aib->rand_rng);

			// check if the track can be build there.
			rule = AiBuildDefaultRailTrack(aib->use_tile,
				_players_ai[p->index].build_kind, _players_ai[p->index].num_wagons,
				aib->unk6, aib->unk7,
				aib->direction, aib->cargo,
				_players_ai[p->index].railtype_to_use,
				&cost
			);

			if (rule == -1) {
				// cannot build, terraform after a while
				if (_players_ai[p->index].state_counter >= 600) {
					AiDoTerraformLand(aib->use_tile, (DiagDirection)(Random() & 3), 3, (int8)_players_ai[p->index].state_mode);
				}
				// also try the other terraform direction
				if (++_players_ai[p->index].state_counter >= 1000) {
					_players_ai[p->index].state_counter = 0;
					_players_ai[p->index].state_mode = -_players_ai[p->index].state_mode;
				}
			} else if (CheckPlayerHasMoney(cost)) {
				// player has money, build it.
				aib->cur_building_rule = rule;

				AiDoBuildDefaultRailTrack(
					aib->use_tile,
					_default_rail_track_data[rule]->data,
					_players_ai[p->index].railtype_to_use,
					DC_EXEC | DC_NO_TOWN_RATING
				);
			}
		} while (++aib, --j);
	}

	// check if we're done with all of them
	aib = &_players_ai[p->index].src;
	j = _players_ai[p->index].num_build_rec;
	do {
		if (aib->cur_building_rule == 255) return;
	} while (++aib, --j);

	// yep, all are done. switch state to the rail building state.
	_players_ai[p->index].state = AIS_BUILD_RAIL;
	_players_ai[p->index].state_mode = 255;
}

static TileIndex AiGetEdgeOfDefaultRailBlock(byte rule, TileIndex tile, byte cmd, DiagDirection *dir)
{
	const AiDefaultBlockData *p = _default_rail_track_data[rule]->data;

	while (p->mode != 3 || !((--cmd) & 0x80)) p++;

	return tile + ToTileIndexDiff(p->tileoffs) - TileOffsByDiagDir(*dir = p->attr);
}

struct AiRailPathFindData {
	TileIndex tile;
	TileIndex tile2;
	int count;
	bool flag;
};

static bool AiEnumFollowTrack(TileIndex tile, AiRailPathFindData *a, int track, uint length)
{
	if (a->flag) return true;

	if (length > 20 || tile == a->tile) {
		a->flag = true;
		return true;
	}

	if (DistanceMax(tile, a->tile2) < 4) a->count++;

	return false;
}

static bool AiDoFollowTrack(const Player* p)
{
	AiRailPathFindData arpfd;

	arpfd.tile = _players_ai[p->index].start_tile_a;
	arpfd.tile2 = _players_ai[p->index].cur_tile_a;
	arpfd.flag = false;
	arpfd.count = 0;
	FollowTrack(_players_ai[p->index].cur_tile_a + TileOffsByDiagDir(_players_ai[p->index].cur_dir_a), TRANSPORT_RAIL, 0, ReverseDiagDir(_players_ai[p->index].cur_dir_a),
		(TPFEnumProc*)AiEnumFollowTrack, NULL, &arpfd);
	return arpfd.count > 8;
}

struct AiRailFinder {
	TileIndex final_tile;
	DiagDirection final_dir;
	byte depth;
	byte recursive_mode;
	DiagDirection cur_best_dir;
	DiagDirection best_dir;
	byte cur_best_depth;
	byte best_depth;
	uint cur_best_dist;
	const byte *best_ptr;
	uint best_dist;
	TileIndex cur_best_tile, best_tile;
	TileIndex bridge_end_tile;
	Player *player;
};

static const byte _ai_table_15[4][8] = {
	{0, 0, 4, 3, 3, 1, 128 + 0, 64},
	{1, 1, 2, 0, 4, 2, 128 + 1, 65},
	{0, 2, 2, 3, 5, 1, 128 + 2, 66},
	{1, 3, 5, 0, 3, 2, 128 + 3, 67}
};

static const byte _dir_table_1[] = { 3, 9, 12, 6};
static const byte _dir_table_2[] = {12, 6,  3, 9};


static bool AiIsTileBanned(const Player* p, TileIndex tile, byte val)
{
	int i;

	for (i = 0; i != _players_ai[p->index].banned_tile_count; i++) {
		if (_players_ai[p->index].banned_tiles[i] == tile && _players_ai[p->index].banned_val[i] == val) {
			return true;
		}
	}
	return false;
}

static void AiBanTile(Player* p, TileIndex tile, byte val)
{
	uint i;

	for (i = lengthof(_players_ai[p->index].banned_tiles) - 1; i != 0; i--) {
		_players_ai[p->index].banned_tiles[i] = _players_ai[p->index].banned_tiles[i - 1];
		_players_ai[p->index].banned_val[i] = _players_ai[p->index].banned_val[i - 1];
	}

	_players_ai[p->index].banned_tiles[0] = tile;
	_players_ai[p->index].banned_val[0] = val;

	if (_players_ai[p->index].banned_tile_count != lengthof(_players_ai[p->index].banned_tiles)) {
		_players_ai[p->index].banned_tile_count++;
	}
}

static void AiBuildRailRecursive(AiRailFinder *arf, TileIndex tile, DiagDirection dir);

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
	arf->cur_best_dist = UINT_MAX;
	arf->cur_best_depth = 0xff;

	return better;
}

static inline void AiCheckBuildRailBridgeHere(AiRailFinder *arf, TileIndex tile, const byte *p)
{
	Slope tileh;
	uint z;
	bool flag;

	DiagDirection dir2 = (DiagDirection)(p[0] & 3);

	tileh = GetTileSlope(tile, &z);
	if (tileh == _dir_table_1[dir2] || (tileh == SLOPE_FLAT && z != 0)) {
		TileIndex tile_new = tile;

		// Allow bridges directly over bottom tiles
		flag = z == 0;
		for (;;) {
			TileType type;

			if ((TileIndexDiff)tile_new < -TileOffsByDiagDir(dir2)) return; // Wraping around map, no bridge possible!
			tile_new = TILE_MASK(tile_new + TileOffsByDiagDir(dir2));
			type = GetTileType(tile_new);

			if (type == MP_CLEAR || type == MP_TREES || GetTileSlope(tile_new, NULL) != SLOPE_FLAT) {
				if (!flag) return;
				break;
			}
			if (type != MP_WATER && type != MP_RAILWAY && type != MP_ROAD) return;
			flag = true;
		}

		// Is building a (rail)bridge possible at this place (type doesn't matter)?
		if (CmdFailed(DoCommand(tile_new, tile, 0 | _players_ai[arf->player->index].railtype_to_use << 8, DC_AUTO, CMD_BUILD_BRIDGE))) {
			return;
		}
		AiBuildRailRecursive(arf, tile_new, dir2);

		// At the bottom depth, check if the new path is better than the old one.
		if (arf->depth == 1) {
			if (AiCheckRailPathBetter(arf, p)) arf->bridge_end_tile = tile_new;
		}
	}
}

static inline void AiCheckBuildRailTunnelHere(AiRailFinder *arf, TileIndex tile, const byte *p)
{
	uint z;

	if (GetTileSlope(tile, &z) == _dir_table_2[p[0] & 3] && z != 0) {
		CommandCost cost = DoCommand(tile, _players_ai[arf->player->index].railtype_to_use, 0, DC_AUTO, CMD_BUILD_TUNNEL);

		if (CmdSucceeded(cost) && cost.GetCost() <= (arf->player->player_money >> 4)) {
			AiBuildRailRecursive(arf, _build_tunnel_endtile, (DiagDirection)(p[0] & 3));
			if (arf->depth == 1) AiCheckRailPathBetter(arf, p);
		}
	}
}


static void AiBuildRailRecursive(AiRailFinder *arf, TileIndex tile, DiagDirection dir)
{
	const byte *p;

	tile = TILE_MASK(tile + TileOffsByDiagDir(dir));

	// Reached destination?
	if (tile == arf->final_tile) {
		if (arf->final_dir != ReverseDiagDir(dir)) {
			if (arf->recursive_mode != 2) arf->recursive_mode = 1;
		} else if (arf->recursive_mode != 2) {
			arf->recursive_mode = 2;
			arf->cur_best_depth = arf->depth;
		} else {
			if (arf->depth < arf->cur_best_depth) arf->cur_best_depth = arf->depth;
		}
		return;
	}

	// Depth too deep?
	if (arf->depth >= 4) {
		uint dist = DistanceMaxPlusManhattan(tile, arf->final_tile);

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
	if (GetTileZ(tile) == 0) {
		p += 6;
	} else {
		do {
			// Make sure the tile is not in the list of banned tiles and that a rail can be built here.
			if (!AiIsTileBanned(arf->player, tile, p[0]) &&
					CmdSucceeded(DoCommand(tile, _players_ai[arf->player->index].railtype_to_use, p[0], DC_AUTO | DC_NO_WATER | DC_NO_RAIL_OVERLAP, CMD_BUILD_SINGLE_RAIL))) {
				AiBuildRailRecursive(arf, tile, (DiagDirection)p[1]);
			}

			// At the bottom depth?
			if (arf->depth == 1) AiCheckRailPathBetter(arf, p);

			p += 2;
		} while (!(p[0] & 0x80));
	}

	AiCheckBuildRailBridgeHere(arf, tile, p);
	AiCheckBuildRailTunnelHere(arf, tile, p + 1);

	arf->depth--;
}


static const byte _dir_table_3[] = {0x25, 0x2A, 0x19, 0x16};

static void AiBuildRailConstruct(Player *p)
{
	AiRailFinder arf;
	int i;

	// Check too much lookahead?
	if (AiDoFollowTrack(p)) {
		_players_ai[p->index].state_counter = (Random()&0xE)+6; // Destruct this amount of blocks
		_players_ai[p->index].state_mode = 1; // Start destruct

		// Ban this tile and don't reach it for a while.
		AiBanTile(p, _players_ai[p->index].cur_tile_a, FindFirstBit(GetRailTrackStatus(_players_ai[p->index].cur_tile_a)));
		return;
	}

	// Setup recursive finder and call it.
	arf.player = p;
	arf.final_tile = _players_ai[p->index].cur_tile_b;
	arf.final_dir = _players_ai[p->index].cur_dir_b;
	arf.depth = 0;
	arf.recursive_mode = 0;
	arf.best_ptr = NULL;
	arf.cur_best_dist = (uint)-1;
	arf.cur_best_depth = 0xff;
	arf.best_dist = (uint)-1;
	arf.best_depth = 0xff;
	arf.cur_best_tile = 0;
	arf.best_tile = 0;
	AiBuildRailRecursive(&arf, _players_ai[p->index].cur_tile_a, _players_ai[p->index].cur_dir_a);

	// Reached destination?
	if (arf.recursive_mode == 2 && arf.cur_best_depth == 0) {
		_players_ai[p->index].state_mode = 255;
		return;
	}

	// Didn't find anything to build?
	if (arf.best_ptr == NULL) {
		// Terraform some
		for (i = 0; i != 5; i++) {
			AiDoTerraformLand(_players_ai[p->index].cur_tile_a, _players_ai[p->index].cur_dir_a, 3, 0);
		}

		if (++_players_ai[p->index].state_counter == 21) {
			_players_ai[p->index].state_counter = 40;
			_players_ai[p->index].state_mode = 1;

			// Ban this tile
			AiBanTile(p, _players_ai[p->index].cur_tile_a, FindFirstBit(GetRailTrackStatus(_players_ai[p->index].cur_tile_a)));
		}
		return;
	}

	_players_ai[p->index].cur_tile_a += TileOffsByDiagDir(_players_ai[p->index].cur_dir_a);

	if (arf.best_ptr[0] & 0x80) {
		TileIndex t1 = _players_ai[p->index].cur_tile_a;
		TileIndex t2 = arf.bridge_end_tile;

		int32 bridge_len = GetTunnelBridgeLength(t1, t2);

		DiagDirection dir = (TileX(t1) == TileX(t2) ? DIAGDIR_SE : DIAGDIR_SW);
		Track track = AxisToTrack(DiagDirToAxis(dir));

		if (t2 < t1) dir = ReverseDiagDir(dir);

		/* try to build a long rail instead of bridge... */
		bool fail = false;
		CommandCost cost;
		TileIndex t = t1;

		/* try to build one rail on each tile - can't use CMD_BUILD_RAILROAD_TRACK now, it can build one part of track without failing */
		do {
			cost = DoCommand(t, _players_ai[p->index].railtype_to_use, track, DC_AUTO | DC_NO_WATER, CMD_BUILD_SINGLE_RAIL);
			/* do not allow building over existing track */
			if (CmdFailed(cost) || IsTileType(t, MP_RAILWAY)) {
				fail = true;
				break;
			}
			t += TileOffsByDiagDir(dir);
		} while (t != t2);

		/* can we build long track? */
		if (!fail) cost = DoCommand(t1, t2, _players_ai[p->index].railtype_to_use | (track << 4), DC_AUTO | DC_NO_WATER, CMD_BUILD_RAILROAD_TRACK);

		if (!fail && CmdSucceeded(cost) && cost.GetCost() <= p->player_money) {
			DoCommand(t1, t2, _players_ai[p->index].railtype_to_use | (track << 4), DC_AUTO | DC_NO_WATER | DC_EXEC, CMD_BUILD_RAILROAD_TRACK);
		} else {

			/* Figure out which (rail)bridge type to build
			 * start with best bridge, then go down to worse and worse bridges
			 * unnecessary to check for worst bridge (i=0), since AI will always build that. */
			int i;
			for (i = MAX_BRIDGES - 1; i != 0; i--) {
				if (CheckBridge_Stuff(i, bridge_len)) {
					CommandCost cost = DoCommand(t1, t2, i | (_players_ai[p->index].railtype_to_use << 8), DC_AUTO, CMD_BUILD_BRIDGE);
					if (CmdSucceeded(cost) && cost.GetCost() < (p->player_money >> 1) && cost.GetCost() < ((p->player_money + _economy.max_loan - p->current_loan) >> 5)) break;
				}
			}

			/* Build it */
			DoCommand(t1, t2, i | (_players_ai[p->index].railtype_to_use << 8), DC_AUTO | DC_EXEC, CMD_BUILD_BRIDGE);
		}

		_players_ai[p->index].cur_tile_a = t2;
		_players_ai[p->index].state_counter = 0;
	} else if (arf.best_ptr[0] & 0x40) {
		// tunnel
		DoCommand(_players_ai[p->index].cur_tile_a, _players_ai[p->index].railtype_to_use, 0, DC_AUTO | DC_EXEC, CMD_BUILD_TUNNEL);
		_players_ai[p->index].cur_tile_a = _build_tunnel_endtile;
		_players_ai[p->index].state_counter = 0;
	} else {
		// rail
		_players_ai[p->index].cur_dir_a = (DiagDirection)(arf.best_ptr[1] & 3);
		DoCommand(_players_ai[p->index].cur_tile_a, _players_ai[p->index].railtype_to_use, arf.best_ptr[0],
			DC_EXEC | DC_AUTO | DC_NO_WATER | DC_NO_RAIL_OVERLAP, CMD_BUILD_SINGLE_RAIL);
		_players_ai[p->index].state_counter = 0;
	}

	if (arf.best_tile != 0) {
		for (i = 0; i != 2; i++) {
			AiDoTerraformLand(arf.best_tile, arf.best_dir, 3, 0);
		}
	}
}

static bool AiRemoveTileAndGoForward(Player *p)
{
	byte b;
	int bit;
	const byte *ptr;
	TileIndex tile = _players_ai[p->index].cur_tile_a;
	TileIndex tilenew;

	if (IsTileType(tile, MP_TUNNELBRIDGE)) {
		if (IsTunnel(tile)) {
			// Clear the tunnel and continue at the other side of it.
			if (CmdFailed(DoCommand(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR)))
				return false;
			_players_ai[p->index].cur_tile_a = TILE_MASK(_build_tunnel_endtile - TileOffsByDiagDir(_players_ai[p->index].cur_dir_a));
			return true;
		} else { // IsBridge(tile)
			// Check if the bridge points in the right direction.
			// This is not really needed the first place AiRemoveTileAndGoForward is called.
			if (DiagDirToAxis(GetTunnelBridgeDirection(tile)) != (_players_ai[p->index].cur_dir_a & 1)) return false;

			tile = GetOtherBridgeEnd(tile);

			tilenew = TILE_MASK(tile - TileOffsByDiagDir(_players_ai[p->index].cur_dir_a));
			// And clear the bridge.
			if (CmdFailed(DoCommand(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR)))
				return false;
			_players_ai[p->index].cur_tile_a = tilenew;
			return true;
		}
	}

	// Find the railtype at the position. Quit if no rail there.
	b = GetRailTrackStatus(tile) & _dir_table_3[_players_ai[p->index].cur_dir_a];
	if (b == 0) return false;

	// Convert into a bit position that CMD_REMOVE_SINGLE_RAIL expects.
	bit = FindFirstBit(b);

	// Then remove and signals if there are any.
	if (IsTileType(tile, MP_RAILWAY) &&
			GetRailTileType(tile) == RAIL_TILE_SIGNALS) {
		DoCommand(tile, 0, 0, DC_EXEC, CMD_REMOVE_SIGNALS);
	}

	// And also remove the rail.
	if (CmdFailed(DoCommand(tile, 0, bit, DC_EXEC, CMD_REMOVE_SINGLE_RAIL)))
		return false;

	// Find the direction at the other edge of the rail.
	ptr = _ai_table_15[ReverseDiagDir(_players_ai[p->index].cur_dir_a)];
	while (ptr[0] != bit) ptr += 2;
	_players_ai[p->index].cur_dir_a = ReverseDiagDir((DiagDirection)ptr[1]);

	// And then also switch tile.
	_players_ai[p->index].cur_tile_a = TILE_MASK(_players_ai[p->index].cur_tile_a - TileOffsByDiagDir(_players_ai[p->index].cur_dir_a));

	return true;
}


static void AiBuildRailDestruct(Player *p)
{
	// Decrease timeout.
	if (!--_players_ai[p->index].state_counter) {
		_players_ai[p->index].state_mode = 2;
		_players_ai[p->index].state_counter = 0;
	}

	// Don't do anything if the destination is already reached.
	if (_players_ai[p->index].cur_tile_a == _players_ai[p->index].start_tile_a) return;

	AiRemoveTileAndGoForward(p);
}


static void AiBuildRail(Player *p)
{
	switch (_players_ai[p->index].state_mode) {
		case 0: // Construct mode, build new rail.
			AiBuildRailConstruct(p);
			break;

		case 1: // Destruct mode, destroy the rail currently built.
			AiBuildRailDestruct(p);
			break;

		case 2: {
			uint i;

			// Terraform some and then try building again.
			for (i = 0; i != 4; i++) {
				AiDoTerraformLand(_players_ai[p->index].cur_tile_a, _players_ai[p->index].cur_dir_a, 3, 0);
			}

			if (++_players_ai[p->index].state_counter == 4) {
				_players_ai[p->index].state_counter = 0;
				_players_ai[p->index].state_mode = 0;
			}
		}

		default: break;
	}
}

static void AiStateBuildRail(Player *p)
{
	int num;
	AiBuildRec *aib;
	byte cmd;
	TileIndex tile;
	DiagDirection dir;

	// time out?
	if (++_players_ai[p->index].timeout_counter == 1388) {
		_players_ai[p->index].state = AIS_DELETE_RAIL_BLOCKS;
		return;
	}

	// Currently building a rail between two points?
	if (_players_ai[p->index].state_mode != 255) {
		AiBuildRail(p);

		// Alternate between edges
		Swap(_players_ai[p->index].start_tile_a, _players_ai[p->index].start_tile_b);
		Swap(_players_ai[p->index].cur_tile_a,   _players_ai[p->index].cur_tile_b);
		Swap(_players_ai[p->index].start_dir_a,  _players_ai[p->index].start_dir_b);
		Swap(_players_ai[p->index].cur_dir_a,    _players_ai[p->index].cur_dir_b);
		return;
	}

	// Now, find two new points to build between
	num = _players_ai[p->index].num_build_rec;
	aib = &_players_ai[p->index].src;

	for (;;) {
		cmd = aib->buildcmd_a;
		aib->buildcmd_a = 255;
		if (cmd != 255) break;

		cmd = aib->buildcmd_b;
		aib->buildcmd_b = 255;
		if (cmd != 255) break;

		aib++;
		if (--num == 0) {
			_players_ai[p->index].state = AIS_BUILD_RAIL_VEH;
			_players_ai[p->index].state_counter = 0; // timeout
			return;
		}
	}

	// Find first edge to build from.
	tile = AiGetEdgeOfDefaultRailBlock(aib->cur_building_rule, aib->use_tile, cmd & 3, &dir);
	_players_ai[p->index].start_tile_a = tile;
	_players_ai[p->index].cur_tile_a = tile;
	_players_ai[p->index].start_dir_a = dir;
	_players_ai[p->index].cur_dir_a = dir;
	DoCommand(TILE_MASK(tile + TileOffsByDiagDir(dir)), 0, (dir & 1) ? 1 : 0, DC_EXEC, CMD_REMOVE_SINGLE_RAIL);

	assert(TILE_MASK(tile) != 0xFF00);

	// Find second edge to build to
	aib = (&_players_ai[p->index].src) + ((cmd >> 4) & 0xF);
	tile = AiGetEdgeOfDefaultRailBlock(aib->cur_building_rule, aib->use_tile, (cmd >> 2) & 3, &dir);
	_players_ai[p->index].start_tile_b = tile;
	_players_ai[p->index].cur_tile_b = tile;
	_players_ai[p->index].start_dir_b = dir;
	_players_ai[p->index].cur_dir_b = dir;
	DoCommand(TILE_MASK(tile + TileOffsByDiagDir(dir)), 0, (dir & 1) ? 1 : 0, DC_EXEC, CMD_REMOVE_SINGLE_RAIL);

	assert(TILE_MASK(tile) != 0xFF00);

	// And setup state.
	_players_ai[p->index].state_mode = 2;
	_players_ai[p->index].state_counter = 0;
	_players_ai[p->index].banned_tile_count = 0;
}

static StationID AiGetStationIdByDef(TileIndex tile, int id)
{
	const AiDefaultBlockData *p = _default_rail_track_data[id]->data;
	while (p->mode != 1) p++;
	return GetStationIndex(TILE_ADD(tile, ToTileIndexDiff(p->tileoffs)));
}

static EngineID AiFindBestWagon(CargoID cargo, RailType railtype)
{
	EngineID best_veh_index = INVALID_ENGINE;
	EngineID i;
	uint16 best_capacity = 0;
	uint16 best_speed    = 0;
	uint speed;

	FOR_ALL_ENGINEIDS_OF_TYPE(i, VEH_TRAIN) {
		const RailVehicleInfo *rvi = RailVehInfo(i);
		const Engine* e = GetEngine(i);

		if (!IsCompatibleRail(rvi->railtype, railtype) ||
				rvi->railveh_type != RAILVEH_WAGON ||
				!HasBit(e->player_avail, _current_player)) {
			continue;
		}

		if (rvi->cargo_type != cargo) continue;

		/* max_speed of 0 indicates no speed limit */
		speed = rvi->max_speed == 0 ? 0xFFFF : rvi->max_speed;

		if (rvi->capacity >= best_capacity && speed >= best_speed) {
			best_capacity = rvi->capacity;
			best_speed    = best_speed;
			best_veh_index = i;
		}
	}

	return best_veh_index;
}

static void AiStateBuildRailVeh(Player *p)
{
	const AiDefaultBlockData *ptr;
	TileIndex tile;
	EngineID veh;
	int i;
	CargoID cargo;
	CommandCost cost;
	Vehicle *v;
	VehicleID loco_id;

	ptr = _default_rail_track_data[_players_ai[p->index].src.cur_building_rule]->data;
	while (ptr->mode != 0) ptr++;

	tile = TILE_ADD(_players_ai[p->index].src.use_tile, ToTileIndexDiff(ptr->tileoffs));


	cargo = _players_ai[p->index].cargo_type;
	for (i = 0;;) {
		if (_players_ai[p->index].wagon_list[i] == INVALID_VEHICLE) {
			veh = AiFindBestWagon(cargo, _players_ai[p->index].railtype_to_use);
			/* veh will return INVALID_ENGINE if no suitable wagon is available.
			 * We shall treat this in the same way as having no money */
			if (veh == INVALID_ENGINE) goto handle_nocash;
			cost = DoCommand(tile, veh, 0, DC_EXEC, CMD_BUILD_RAIL_VEHICLE);
			if (CmdFailed(cost)) goto handle_nocash;
			_players_ai[p->index].wagon_list[i] = _new_vehicle_id;
			_players_ai[p->index].wagon_list[i + 1] = INVALID_VEHICLE;
			return;
		}
		if (cargo == CT_MAIL) cargo = CT_PASSENGERS;
		if (++i == _players_ai[p->index].num_wagons * 2 - 1) break;
	}

	// Which locomotive to build?
	veh = AiChooseTrainToBuild(_players_ai[p->index].railtype_to_use, p->player_money, cargo != CT_PASSENGERS ? 1 : 0, tile);
	if (veh == INVALID_ENGINE) {
handle_nocash:
		// after a while, if AI still doesn't have cash, get out of this block by selling the wagons.
		if (++_players_ai[p->index].state_counter == 1000) {
			for (i = 0; _players_ai[p->index].wagon_list[i] != INVALID_VEHICLE; i++) {
				cost = DoCommand(tile, _players_ai[p->index].wagon_list[i], 0, DC_EXEC, CMD_SELL_RAIL_WAGON);
				assert(CmdSucceeded(cost));
			}
			_players_ai[p->index].state = AIS_0;
		}
		return;
	}

	// Try to build the locomotive
	cost = DoCommand(tile, veh, 0, DC_EXEC, CMD_BUILD_RAIL_VEHICLE);
	assert(CmdSucceeded(cost));
	loco_id = _new_vehicle_id;

	// Sell a vehicle if the train is double headed.
	v = GetVehicle(loco_id);
	if (v->Next() != NULL) {
		i = _players_ai[p->index].wagon_list[_players_ai[p->index].num_wagons * 2 - 2];
		_players_ai[p->index].wagon_list[_players_ai[p->index].num_wagons * 2 - 2] = INVALID_VEHICLE;
		DoCommand(tile, i, 0, DC_EXEC, CMD_SELL_RAIL_WAGON);
	}

	// Move the wagons onto the train
	for (i = 0; _players_ai[p->index].wagon_list[i] != INVALID_VEHICLE; i++) {
		DoCommand(tile, _players_ai[p->index].wagon_list[i] | (loco_id << 16), 0, DC_EXEC, CMD_MOVE_RAIL_VEHICLE);
	}

	for (i = 0; _players_ai[p->index].order_list_blocks[i] != 0xFF; i++) {
		const AiBuildRec* aib = &_players_ai[p->index].src + _players_ai[p->index].order_list_blocks[i];
		bool is_pass = (
			_players_ai[p->index].cargo_type == CT_PASSENGERS ||
			_players_ai[p->index].cargo_type == CT_MAIL ||
			(_opt.landscape == LT_TEMPERATE && _players_ai[p->index].cargo_type == CT_VALUABLES)
		);
		Order order;

		order.type = OT_GOTO_STATION;
		order.flags = 0;
		order.dest = AiGetStationIdByDef(aib->use_tile, aib->cur_building_rule);

		if (!is_pass && i == 1) order.flags |= OFB_UNLOAD;
		if (_players_ai[p->index].num_want_fullload != 0 && (is_pass || i == 0))
			order.flags |= OFB_FULL_LOAD;

		DoCommand(0, loco_id + (i << 16), PackOrder(&order), DC_EXEC, CMD_INSERT_ORDER);
	}

	DoCommand(0, loco_id, 0, DC_EXEC, CMD_START_STOP_TRAIN);

	DoCommand(0, loco_id, _ai_service_interval, DC_EXEC, CMD_CHANGE_SERVICE_INT);

	if (_players_ai[p->index].num_want_fullload != 0) _players_ai[p->index].num_want_fullload--;

	if (--_players_ai[p->index].num_loco_to_build != 0) {
//		_players_ai[p->index].loco_id = INVALID_VEHICLE;
		_players_ai[p->index].wagon_list[0] = INVALID_VEHICLE;
	} else {
		_players_ai[p->index].state = AIS_0;
	}
}

static void AiStateDeleteRailBlocks(Player *p)
{
	const AiBuildRec* aib = &_players_ai[p->index].src;
	uint num = _players_ai[p->index].num_build_rec;

	do {
		const AiDefaultBlockData* b;

		if (aib->cur_building_rule == 255) continue;
		for (b = _default_rail_track_data[aib->cur_building_rule]->data; b->mode != 4; b++) {
			DoCommand(TILE_ADD(aib->use_tile, ToTileIndexDiff(b->tileoffs)), 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
		}
	} while (++aib, --num);

	_players_ai[p->index].state = AIS_0;
}

static bool AiCheckRoadResources(TileIndex tile, const AiDefaultBlockData *p, byte cargo)
{
	uint values[NUM_CARGO];
	int rad;

	if (_patches.modified_catchment) {
		rad = CA_TRUCK; // Same as CA_BUS at the moment?
	} else { // change that at some point?
		rad = 4;
	}

	for (;; p++) {
		if (p->mode == 4) {
			return true;
		} else if (p->mode == 1) {
			TileIndex tile2 = TILE_ADD(tile, ToTileIndexDiff(p->tileoffs));

			if (cargo & 0x80) {
				GetProductionAroundTiles(values, tile2, 1, 1, rad);
				return values[cargo & 0x7F] != 0;
			} else {
				GetAcceptanceAroundTiles(values, tile2, 1, 1, rad);
				return (values[cargo]&~7) != 0;
			}
		}
	}
}

static bool _want_road_truck_station;
static CommandCost AiDoBuildDefaultRoadBlock(TileIndex tile, const AiDefaultBlockData *p, byte flag);

// Returns rule and cost
static int AiFindBestDefaultRoadBlock(TileIndex tile, byte direction, byte cargo, CommandCost *cost)
{
	int i;
	const AiDefaultRoadBlock *p;

	_want_road_truck_station = (cargo & 0x7F) != CT_PASSENGERS;

	for (i = 0; (p = _road_default_block_data[i]) != NULL; i++) {
		if (p->dir == direction) {
			*cost = AiDoBuildDefaultRoadBlock(tile, p->data, 0);
			if (CmdSucceeded(*cost) && AiCheckRoadResources(tile, p->data, cargo))
				return i;
		}
	}

	return -1;
}

static CommandCost AiDoBuildDefaultRoadBlock(TileIndex tile, const AiDefaultBlockData *p, byte flag)
{
	CommandCost ret;
	CommandCost total_cost(EXPENSES_CONSTRUCTION);
	Town *t = NULL;
	int rating = 0;
	int roadflag = 0;

	for (;p->mode != 4;p++) {
		TileIndex c = TILE_MASK(tile + ToTileIndexDiff(p->tileoffs));

		_cleared_town = NULL;

		if (p->mode == 2) {
			if (IsNormalRoadTile(c) &&
					(GetRoadBits(c, ROADTYPE_ROAD) & p->attr) != 0) {
				roadflag |= 2;

				// all bits are already built?
				if ((GetRoadBits(c, ROADTYPE_ROAD) & p->attr) == p->attr) continue;
			}

			ret = DoCommand(c, p->attr, 0, flag | DC_AUTO | DC_NO_WATER, CMD_BUILD_ROAD);
			if (CmdFailed(ret)) return CMD_ERROR;
			total_cost.AddCost(ret);

			continue;
		}

		if (p->mode == 0) {
			// Depot
			ret = DoCommand(c, p->attr, 0, flag | DC_AUTO | DC_NO_WATER | DC_AI_BUILDING, CMD_BUILD_ROAD_DEPOT);
			goto clear_town_stuff;
		} else if (p->mode == 1) {
			if (_want_road_truck_station) {
				// Truck station
				ret = DoCommand(c, p->attr, ROADTYPES_ROAD << 2 | ROADSTOP_TRUCK, flag | DC_AUTO | DC_NO_WATER | DC_AI_BUILDING, CMD_BUILD_ROAD_STOP);
			} else {
				// Bus station
				ret = DoCommand(c, p->attr, ROADTYPES_ROAD << 2 | ROADSTOP_BUS, flag | DC_AUTO | DC_NO_WATER | DC_AI_BUILDING, CMD_BUILD_ROAD_STOP);
			}
clear_town_stuff:;

			if (CmdFailed(ret)) return CMD_ERROR;
			total_cost.AddCost(ret);

			if (_cleared_town != NULL) {
				if (t != NULL && t != _cleared_town) return CMD_ERROR;
				t = _cleared_town;
				rating += _cleared_town_rating;
			}
		} else if (p->mode == 3) {
			if (flag & DC_EXEC) continue;

			if (GetTileSlope(c, NULL) != SLOPE_FLAT) return CMD_ERROR;

			if (!IsNormalRoadTile(c)) {
				ret = DoCommand(c, 0, 0, flag | DC_AUTO | DC_NO_WATER | DC_AI_BUILDING, CMD_LANDSCAPE_CLEAR);
				if (CmdFailed(ret)) return CMD_ERROR;
			}

		}
	}

	if (!_want_road_truck_station && !(roadflag & 2)) return CMD_ERROR;

	if (!(flag & DC_EXEC)) {
		if (t != NULL && rating > t->ratings[_current_player]) return CMD_ERROR;
	}
	return total_cost;
}

// Make sure the blocks are not too close to each other
static bool AiCheckBlockDistances(Player *p, TileIndex tile)
{
	const AiBuildRec* aib = &_players_ai[p->index].src;
	uint num = _players_ai[p->index].num_build_rec;

	do {
		if (aib->cur_building_rule != 255) {
			if (DistanceManhattan(aib->use_tile, tile) < 9) return false;
		}
	} while (++aib, --num);

	return true;
}


static void AiStateBuildDefaultRoadBlocks(Player *p)
{
	uint i;
	int j;
	AiBuildRec *aib;
	int rule;
	CommandCost cost;

	// time out?
	if (++_players_ai[p->index].timeout_counter == 1388) {
		_players_ai[p->index].state = AIS_DELETE_RAIL_BLOCKS;
		return;
	}

	// do the following 8 times
	for (i = 0; i != 8; i++) {
		// check if we can build the default track
		aib = &_players_ai[p->index].src;
		j = _players_ai[p->index].num_build_rec;
		do {
			// this item has already been built?
			if (aib->cur_building_rule != 255) continue;

			// adjust the coordinate randomly,
			// to make sure that we find a position.
			aib->use_tile = AdjustTileCoordRandomly(aib->spec_tile, aib->rand_rng);

			// check if the road can be built there.
			rule = AiFindBestDefaultRoadBlock(
				aib->use_tile, aib->direction, aib->cargo, &cost
			);

			if (rule == -1) {
				// cannot build, terraform after a while
				if (_players_ai[p->index].state_counter >= 600) {
					AiDoTerraformLand(aib->use_tile, (DiagDirection)(Random() & 3), 3, (int8)_players_ai[p->index].state_mode);
				}
				// also try the other terraform direction
				if (++_players_ai[p->index].state_counter >= 1000) {
					_players_ai[p->index].state_counter = 0;
					_players_ai[p->index].state_mode = -_players_ai[p->index].state_mode;
				}
			} else if (CheckPlayerHasMoney(cost) && AiCheckBlockDistances(p, aib->use_tile)) {
				CommandCost r;

				// player has money, build it.
				aib->cur_building_rule = rule;

				r = AiDoBuildDefaultRoadBlock(
					aib->use_tile,
					_road_default_block_data[rule]->data,
					DC_EXEC | DC_NO_TOWN_RATING
				);
				assert(CmdSucceeded(r));
			}
		} while (++aib, --j);
	}

	// check if we're done with all of them
	aib = &_players_ai[p->index].src;
	j = _players_ai[p->index].num_build_rec;
	do {
		if (aib->cur_building_rule == 255) return;
	} while (++aib, --j);

	// yep, all are done. switch state to the rail building state.
	_players_ai[p->index].state = AIS_BUILD_ROAD;
	_players_ai[p->index].state_mode = 255;
}

struct AiRoadFinder {
	TileIndex final_tile;
	DiagDirection final_dir;
	byte depth;
	byte recursive_mode;
	DiagDirection cur_best_dir;
	DiagDirection best_dir;
	byte cur_best_depth;
	byte best_depth;
	uint cur_best_dist;
	const byte *best_ptr;
	uint best_dist;
	TileIndex cur_best_tile, best_tile;
	TileIndex bridge_end_tile;
	Player *player;
};

struct AiRoadEnum {
	TileIndex dest;
	TileIndex best_tile;
	int best_track;
	uint best_dist;
};

static const DiagDirection _dir_by_track[] = {
	DIAGDIR_NE, DIAGDIR_SE, DIAGDIR_NE, DIAGDIR_SE, DIAGDIR_SW, DIAGDIR_SE,
	DIAGDIR_NE, DIAGDIR_NE,
	DIAGDIR_SW, DIAGDIR_NW, DIAGDIR_NW, DIAGDIR_SW, DIAGDIR_NW, DIAGDIR_NE,
};

static void AiBuildRoadRecursive(AiRoadFinder *arf, TileIndex tile, DiagDirection dir);

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


static bool AiEnumFollowRoad(TileIndex tile, AiRoadEnum *a, int track, uint length)
{
	uint dist = DistanceManhattan(tile, a->dest);

	if (dist <= a->best_dist) {
		TileIndex tile2 = TILE_MASK(tile + TileOffsByDiagDir(_dir_by_track[track]));

		if (IsNormalRoadTile(tile2)) {
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
	TileIndex tile;
	DiagDirection dir = _players_ai[p->index].cur_dir_a;
	uint32 bits;

	are.dest = _players_ai[p->index].cur_tile_b;
	tile = TILE_MASK(_players_ai[p->index].cur_tile_a + TileOffsByDiagDir(dir));

	if (IsRoadStopTile(tile) || IsTileDepotType(tile, TRANSPORT_ROAD)) return false;
	bits = TrackStatusToTrackdirBits(GetTileTrackStatus(tile, TRANSPORT_ROAD, ROADTYPES_ROAD)) & _ai_road_table_and[dir];
	if (bits == 0) return false;

	are.best_dist = (uint)-1;

	uint i;
	FOR_EACH_SET_BIT(i, bits) {
		FollowTrack(tile, 0x1000 | TRANSPORT_ROAD, ROADTYPES_ROAD, (DiagDirection)_dir_by_track[i], (TPFEnumProc*)AiEnumFollowRoad, NULL, &are);
	}

	if (DistanceManhattan(tile, are.dest) <= are.best_dist) return false;

	if (are.best_dist == 0) return true;

	_players_ai[p->index].cur_tile_a = are.best_tile;
	_players_ai[p->index].cur_dir_a = _dir_by_track[are.best_track];
	return false;
}


static bool AiBuildRoadHelper(TileIndex tile, int flags, int type)
{
	static const RoadBits _road_bits[] = {
		ROAD_X,
		ROAD_Y,
		ROAD_NW | ROAD_NE,
		ROAD_SW | ROAD_SE,
		ROAD_NW | ROAD_SW,
		ROAD_SE | ROAD_NE
	};
	return CmdSucceeded(DoCommand(tile, _road_bits[type], 0, flags, CMD_BUILD_ROAD));
}

static inline void AiCheckBuildRoadBridgeHere(AiRoadFinder *arf, TileIndex tile, const byte *p)
{
	Slope tileh;
	uint z;
	bool flag;

	DiagDirection dir2 = (DiagDirection)(p[0] & 3);

	tileh = GetTileSlope(tile, &z);
	if (tileh == _dir_table_1[dir2] || (tileh == SLOPE_FLAT && z != 0)) {
		TileIndex tile_new = tile;

		// Allow bridges directly over bottom tiles
		flag = z == 0;
		for (;;) {
			TileType type;

			if ((TileIndexDiff)tile_new < -TileOffsByDiagDir(dir2)) return; // Wraping around map, no bridge possible!
			tile_new = TILE_MASK(tile_new + TileOffsByDiagDir(dir2));
			type = GetTileType(tile_new);

			if (type == MP_CLEAR || type == MP_TREES || GetTileSlope(tile_new, NULL) != SLOPE_FLAT) {
				// Allow a bridge if either we have a tile that's water, rail or street,
				// or if we found an up tile.
				if (!flag) return;
				break;
			}
			if (type != MP_WATER && type != MP_RAILWAY && type != MP_ROAD) return;
			flag = true;
		}

		// Is building a (rail)bridge possible at this place (type doesn't matter)?
		if (CmdFailed(DoCommand(tile_new, tile, ((0x80 | ROADTYPES_ROAD) << 8), DC_AUTO, CMD_BUILD_BRIDGE)))
			return;
		AiBuildRoadRecursive(arf, tile_new, dir2);

		// At the bottom depth, check if the new path is better than the old one.
		if (arf->depth == 1) {
			if (AiCheckRoadPathBetter(arf, p)) arf->bridge_end_tile = tile_new;
		}
	}
}

static inline void AiCheckBuildRoadTunnelHere(AiRoadFinder *arf, TileIndex tile, const byte *p)
{
	uint z;

	if (GetTileSlope(tile, &z) == _dir_table_2[p[0] & 3] && z != 0) {
		CommandCost cost = DoCommand(tile, 0x200, 0, DC_AUTO, CMD_BUILD_TUNNEL);

		if (CmdSucceeded(cost) && cost.GetCost() <= (arf->player->player_money >> 4)) {
			AiBuildRoadRecursive(arf, _build_tunnel_endtile, (DiagDirection)(p[0] & 3));
			if (arf->depth == 1)  AiCheckRoadPathBetter(arf, p);
		}
	}
}



static void AiBuildRoadRecursive(AiRoadFinder *arf, TileIndex tile, DiagDirection dir)
{
	const byte *p;

	tile = TILE_MASK(tile + TileOffsByDiagDir(dir));

	// Reached destination?
	if (tile == arf->final_tile) {
		if (ReverseDiagDir(arf->final_dir) == dir) {
			arf->recursive_mode = 2;
			arf->cur_best_depth = arf->depth;
		}
		return;
	}

	// Depth too deep?
	if (arf->depth >= 4) {
		uint dist = DistanceMaxPlusManhattan(tile, arf->final_tile);
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
	if (GetTileZ(tile) == 0) {
		p += 6;
	} else {
		do {
			// Make sure that a road can be built here.
			if (AiBuildRoadHelper(tile, DC_AUTO | DC_NO_WATER | DC_AI_BUILDING, p[0])) {
				AiBuildRoadRecursive(arf, tile, (DiagDirection)p[1]);
			}

			// At the bottom depth?
			if (arf->depth == 1) AiCheckRoadPathBetter(arf, p);

			p += 2;
		} while (!(p[0] & 0x80));
	}

	AiCheckBuildRoadBridgeHere(arf, tile, p);
	AiCheckBuildRoadTunnelHere(arf, tile, p + 1);

	arf->depth--;
}


static void AiBuildRoadConstruct(Player *p)
{
	AiRoadFinder arf;
	int i;
	TileIndex tile;

	// Reached destination?
	if (AiCheckRoadFinished(p)) {
		_players_ai[p->index].state_mode = 255;
		return;
	}

	// Setup recursive finder and call it.
	arf.player = p;
	arf.final_tile = _players_ai[p->index].cur_tile_b;
	arf.final_dir = _players_ai[p->index].cur_dir_b;
	arf.depth = 0;
	arf.recursive_mode = 0;
	arf.best_ptr = NULL;
	arf.cur_best_dist = (uint)-1;
	arf.cur_best_depth = 0xff;
	arf.best_dist = (uint)-1;
	arf.best_depth =  0xff;
	arf.cur_best_tile = 0;
	arf.best_tile = 0;
	AiBuildRoadRecursive(&arf, _players_ai[p->index].cur_tile_a, _players_ai[p->index].cur_dir_a);

	// Reached destination?
	if (arf.recursive_mode == 2 && arf.cur_best_depth == 0) {
		_players_ai[p->index].state_mode = 255;
		return;
	}

	// Didn't find anything to build?
	if (arf.best_ptr == NULL) {
		// Terraform some
do_some_terraform:
		for (i = 0; i != 5; i++)
			AiDoTerraformLand(_players_ai[p->index].cur_tile_a, _players_ai[p->index].cur_dir_a, 3, 0);

		if (++_players_ai[p->index].state_counter == 21) {
			_players_ai[p->index].state_mode = 1;

			_players_ai[p->index].cur_tile_a = TILE_MASK(_players_ai[p->index].cur_tile_a + TileOffsByDiagDir(_players_ai[p->index].cur_dir_a));
			_players_ai[p->index].cur_dir_a = ReverseDiagDir(_players_ai[p->index].cur_dir_a);
			_players_ai[p->index].state_counter = 0;
		}
		return;
	}

	tile = TILE_MASK(_players_ai[p->index].cur_tile_a + TileOffsByDiagDir(_players_ai[p->index].cur_dir_a));

	if (arf.best_ptr[0] & 0x80) {
		TileIndex t1 = tile;
		TileIndex t2 = arf.bridge_end_tile;

		int32 bridge_len = GetTunnelBridgeLength(t1, t2);

		Axis axis = (TileX(t1) == TileX(t2) ? AXIS_Y : AXIS_X);

		/* try to build a long road instead of bridge - CMD_BUILD_LONG_ROAD has to fail if it couldn't build at least one piece! */
 		CommandCost cost = DoCommand(t2, t1, (t2 < t1 ? 1 : 2) | (axis << 2) | (ROADTYPE_ROAD << 3), DC_AUTO | DC_NO_WATER, CMD_BUILD_LONG_ROAD);

		if (CmdSucceeded(cost) && cost.GetCost() <= p->player_money) {
			DoCommand(t2, t1, (t2 < t1 ? 1 : 2) | (axis << 2) | (ROADTYPE_ROAD << 3), DC_AUTO | DC_EXEC | DC_NO_WATER, CMD_BUILD_LONG_ROAD);
		} else {
			int i;

			/* Figure out what (road)bridge type to build
			 * start with best bridge, then go down to worse and worse bridges
			 * unnecessary to check for worse bridge (i=0), since AI will always build that */
			for (i = MAX_BRIDGES - 1; i != 0; i--) {
				if (CheckBridge_Stuff(i, bridge_len)) {
					CommandCost cost = DoCommand(t1, t2, i + ((0x80 | ROADTYPES_ROAD) << 8), DC_AUTO, CMD_BUILD_BRIDGE);
					if (CmdSucceeded(cost) && cost.GetCost() < (p->player_money >> 1) && cost.GetCost() < ((p->player_money + _economy.max_loan - p->current_loan) >> 5)) break;
				}
			}

			/* Build it */
			DoCommand(t1, t2, i + ((0x80 | ROADTYPES_ROAD) << 8), DC_AUTO | DC_EXEC, CMD_BUILD_BRIDGE);
		}

		_players_ai[p->index].cur_tile_a = t2;
		_players_ai[p->index].state_counter = 0;
	} else if (arf.best_ptr[0] & 0x40) {
		// tunnel
		DoCommand(tile, 0x200, 0, DC_AUTO | DC_EXEC, CMD_BUILD_TUNNEL);
		_players_ai[p->index].cur_tile_a = _build_tunnel_endtile;
		_players_ai[p->index].state_counter = 0;
	} else {
		// road
		if (!AiBuildRoadHelper(tile, DC_EXEC | DC_AUTO | DC_NO_WATER | DC_AI_BUILDING, arf.best_ptr[0]))
			goto do_some_terraform;

		_players_ai[p->index].cur_dir_a = (DiagDirection)(arf.best_ptr[1] & 3);
		_players_ai[p->index].cur_tile_a = tile;
		_players_ai[p->index].state_counter = 0;
	}

	if (arf.best_tile != 0) {
		for (i = 0; i != 2; i++)
			AiDoTerraformLand(arf.best_tile, arf.best_dir, 3, 0);
	}
}


static void AiBuildRoad(Player *p)
{
	if (_players_ai[p->index].state_mode < 1) {
		// Construct mode, build new road.
		AiBuildRoadConstruct(p);
	} else if (_players_ai[p->index].state_mode == 1) {
		// Destruct mode, not implemented for roads.
		_players_ai[p->index].state_mode = 2;
		_players_ai[p->index].state_counter = 0;
	} else if (_players_ai[p->index].state_mode == 2) {
		uint i;

		// Terraform some and then try building again.
		for (i = 0; i != 4; i++) {
			AiDoTerraformLand(_players_ai[p->index].cur_tile_a, _players_ai[p->index].cur_dir_a, 3, 0);
		}

		if (++_players_ai[p->index].state_counter == 4) {
			_players_ai[p->index].state_counter = 0;
			_players_ai[p->index].state_mode = 0;
		}
	}
}

static TileIndex AiGetRoadBlockEdge(byte rule, TileIndex tile, DiagDirection *dir)
{
	const AiDefaultBlockData *p = _road_default_block_data[rule]->data;
	while (p->mode != 1) p++;
	*dir = p->attr;
	return TILE_ADD(tile, ToTileIndexDiff(p->tileoffs));
}


static void AiStateBuildRoad(Player *p)
{
	int num;
	AiBuildRec *aib;
	byte cmd;
	TileIndex tile;
	DiagDirection dir;

	// time out?
	if (++_players_ai[p->index].timeout_counter == 1388) {
		_players_ai[p->index].state = AIS_DELETE_ROAD_BLOCKS;
		return;
	}

	// Currently building a road between two points?
	if (_players_ai[p->index].state_mode != 255) {
		AiBuildRoad(p);

		// Alternate between edges
		Swap(_players_ai[p->index].start_tile_a, _players_ai[p->index].start_tile_b);
		Swap(_players_ai[p->index].cur_tile_a,   _players_ai[p->index].cur_tile_b);
		Swap(_players_ai[p->index].start_dir_a,  _players_ai[p->index].start_dir_b);
		Swap(_players_ai[p->index].cur_dir_a,    _players_ai[p->index].cur_dir_b);

		return;
	}

	// Now, find two new points to build between
	num = _players_ai[p->index].num_build_rec;
	aib = &_players_ai[p->index].src;

	for (;;) {
		cmd = aib->buildcmd_a;
		aib->buildcmd_a = 255;
		if (cmd != 255) break;

		aib++;
		if (--num == 0) {
			_players_ai[p->index].state = AIS_BUILD_ROAD_VEHICLES;
			return;
		}
	}

	// Find first edge to build from.
	tile = AiGetRoadBlockEdge(aib->cur_building_rule, aib->use_tile, &dir);
	_players_ai[p->index].start_tile_a = tile;
	_players_ai[p->index].cur_tile_a = tile;
	_players_ai[p->index].start_dir_a = dir;
	_players_ai[p->index].cur_dir_a = dir;

	// Find second edge to build to
	aib = (&_players_ai[p->index].src) + (cmd & 0xF);
	tile = AiGetRoadBlockEdge(aib->cur_building_rule, aib->use_tile, &dir);
	_players_ai[p->index].start_tile_b = tile;
	_players_ai[p->index].cur_tile_b = tile;
	_players_ai[p->index].start_dir_b = dir;
	_players_ai[p->index].cur_dir_b = dir;

	// And setup state.
	_players_ai[p->index].state_mode = 2;
	_players_ai[p->index].state_counter = 0;
	_players_ai[p->index].banned_tile_count = 0;
}

static StationID AiGetStationIdFromRoadBlock(TileIndex tile, int id)
{
	const AiDefaultBlockData *p = _road_default_block_data[id]->data;
	while (p->mode != 1) p++;
	return GetStationIndex(TILE_ADD(tile, ToTileIndexDiff(p->tileoffs)));
}

static void AiStateBuildRoadVehicles(Player *p)
{
	const AiDefaultBlockData *ptr;
	TileIndex tile;
	VehicleID loco_id;
	EngineID veh;
	uint i;

	ptr = _road_default_block_data[_players_ai[p->index].src.cur_building_rule]->data;
	for (; ptr->mode != 0; ptr++) {}
	tile = TILE_ADD(_players_ai[p->index].src.use_tile, ToTileIndexDiff(ptr->tileoffs));

	veh = AiChooseRoadVehToBuild(_players_ai[p->index].cargo_type, p->player_money, tile);
	if (veh == INVALID_ENGINE) {
		_players_ai[p->index].state = AIS_0;
		return;
	}

	if (CmdFailed(DoCommand(tile, veh, 0, DC_EXEC, CMD_BUILD_ROAD_VEH))) return;

	loco_id = _new_vehicle_id;

	if (GetVehicle(loco_id)->cargo_type != _players_ai[p->index].cargo_type) {
		/* Cargo type doesn't match, so refit it */
		if (CmdFailed(DoCommand(tile, loco_id, _players_ai[p->index].cargo_type, DC_EXEC, CMD_REFIT_ROAD_VEH))) {
			/* Refit failed... sell the vehicle */
			DoCommand(tile, loco_id, 0, DC_EXEC, CMD_SELL_ROAD_VEH);
			return;
		}
	}

	for (i = 0; _players_ai[p->index].order_list_blocks[i] != 0xFF; i++) {
		const AiBuildRec* aib = &_players_ai[p->index].src + _players_ai[p->index].order_list_blocks[i];
		bool is_pass = (
			_players_ai[p->index].cargo_type == CT_PASSENGERS ||
			_players_ai[p->index].cargo_type == CT_MAIL ||
			(_opt.landscape == LT_TEMPERATE && _players_ai[p->index].cargo_type == CT_VALUABLES)
		);
		Order order;

		order.type = OT_GOTO_STATION;
		order.flags = 0;
		order.dest = AiGetStationIdFromRoadBlock(aib->use_tile, aib->cur_building_rule);

		if (!is_pass && i == 1) order.flags |= OFB_UNLOAD;
		if (_players_ai[p->index].num_want_fullload != 0 && (is_pass || i == 0))
			order.flags |= OFB_FULL_LOAD;

		DoCommand(0, loco_id + (i << 16), PackOrder(&order), DC_EXEC, CMD_INSERT_ORDER);
	}

	DoCommand(0, loco_id, 0, DC_EXEC, CMD_START_STOP_ROADVEH);
	DoCommand(0, loco_id, _ai_service_interval, DC_EXEC, CMD_CHANGE_SERVICE_INT);

	if (_players_ai[p->index].num_want_fullload != 0) _players_ai[p->index].num_want_fullload--;
	if (--_players_ai[p->index].num_loco_to_build == 0) _players_ai[p->index].state = AIS_0;
}

static void AiStateDeleteRoadBlocks(Player *p)
{
	const AiBuildRec* aib = &_players_ai[p->index].src;
	uint num = _players_ai[p->index].num_build_rec;

	do {
		const AiDefaultBlockData* b;

		if (aib->cur_building_rule == 255) continue;
		for (b = _road_default_block_data[aib->cur_building_rule]->data; b->mode != 4; b++) {
			if (b->mode > 1) continue;
			DoCommand(TILE_ADD(aib->use_tile, ToTileIndexDiff(b->tileoffs)), 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
		}
	} while (++aib, --num);

	_players_ai[p->index].state = AIS_0;
}


static void AiStateAirportStuff(Player *p)
{
	const Station* st;
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
		aib = &_players_ai[p->index].src + i;

		FOR_ALL_STATIONS(st) {
			// Is this an airport?
			if (!(st->facilities & FACIL_AIRPORT)) continue;

			// Do we own the airport? (Oilrigs aren't owned, though.)
			if (st->owner != OWNER_NONE && st->owner != _current_player) continue;

			AirportFTAClass::Flags flags = st->Airport()->flags;

			if (!(flags & (_players_ai[p->index].build_kind == 1 && i == 0 ? AirportFTAClass::HELICOPTERS : AirportFTAClass::AIRPLANES))) {
				continue;
			}

			// Dismiss airports too far away.
			if (DistanceMax(st->airport_tile, aib->spec_tile) > aib->rand_rng)
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
			 * IsHangarTile() makes no sense. --pasky */
			if (!(flags & AirportFTAClass::AIRPLANES)) {
				/* Heliports should have maybe own rulesets but
				 * OTOH we don't want AI to pick them up when
				 * looking for a suitable airport type to build.
				 * So any of rules 0 or 1 would do for now. The
				 * original rule number was 2 but that's a bug
				 * because we have no such rule. */
				rule = 1;
			} else {
				rule = IsHangarTile(st->airport_tile);
			}

			aib->cur_building_rule = rule;
			aib->use_tile = st->airport_tile;
			break;
		}
	} while (++i != _players_ai[p->index].num_build_rec);

	_players_ai[p->index].state = AIS_BUILD_DEFAULT_AIRPORT_BLOCKS;
	_players_ai[p->index].state_mode = 255;
	_players_ai[p->index].state_counter = 0;
}

static CommandCost AiDoBuildDefaultAirportBlock(TileIndex tile, const AiDefaultBlockData *p, byte flag)
{
	uint32 avail_airports = GetValidAirports();
	CommandCost ret,total_cost(EXPENSES_CONSTRUCTION);

	for (; p->mode == 0; p++) {
		if (!HasBit(avail_airports, p->attr)) return CMD_ERROR;
		ret = DoCommand(TILE_MASK(tile + ToTileIndexDiff(p->tileoffs)), p->attr, 0, flag | DC_AUTO | DC_NO_WATER, CMD_BUILD_AIRPORT);
		if (CmdFailed(ret)) return CMD_ERROR;
		total_cost.AddCost(ret);
	}

	return total_cost;
}

static bool AiCheckAirportResources(TileIndex tile, const AiDefaultBlockData *p, byte cargo)
{
	uint values[NUM_CARGO];

	for (; p->mode == 0; p++) {
		TileIndex tile2 = TILE_ADD(tile, ToTileIndexDiff(p->tileoffs));
		const AirportFTAClass* airport = GetAirport(p->attr);
		uint w = airport->size_x;
		uint h = airport->size_y;
		uint rad = _patches.modified_catchment ? airport->catchment : (uint)CA_UNMODIFIED;

		if (cargo & 0x80) {
			GetProductionAroundTiles(values, tile2, w, h, rad);
			return values[cargo & 0x7F] != 0;
		} else {
			GetAcceptanceAroundTiles(values, tile2, w, h, rad);
			return values[cargo] >= 8;
		}
	}
	return true;
}

static int AiFindBestDefaultAirportBlock(TileIndex tile, byte cargo, byte heli, CommandCost *cost)
{
	const AiDefaultBlockData *p;
	uint i;

	for (i = 0; (p = _airport_default_block_data[i]) != NULL; i++) {
		// If we are doing a helicopter service, avoid building
		// airports where they can't land.
		if (heli && !(GetAirport(p->attr)->flags & AirportFTAClass::HELICOPTERS)) continue;

		*cost = AiDoBuildDefaultAirportBlock(tile, p, 0);
		if (CmdSucceeded(*cost) && AiCheckAirportResources(tile, p, cargo))
			return i;
	}
	return -1;
}

static void AiStateBuildDefaultAirportBlocks(Player *p)
{
	int i, j;
	AiBuildRec *aib;
	int rule;
	CommandCost cost;

	// time out?
	if (++_players_ai[p->index].timeout_counter == 1388) {
		_players_ai[p->index].state = AIS_0;
		return;
	}

	// do the following 8 times
	i = 8;
	do {
		// check if we can build the default
		aib = &_players_ai[p->index].src;
		j = _players_ai[p->index].num_build_rec;
		do {
			// this item has already been built?
			if (aib->cur_building_rule != 255) continue;

			// adjust the coordinate randomly,
			// to make sure that we find a position.
			aib->use_tile = AdjustTileCoordRandomly(aib->spec_tile, aib->rand_rng);

			// check if the aircraft stuff can be built there.
			rule = AiFindBestDefaultAirportBlock(aib->use_tile, aib->cargo, _players_ai[p->index].build_kind, &cost);

//			SetRedErrorSquare(aib->use_tile);

			if (rule == -1) {
				// cannot build, terraform after a while
				if (_players_ai[p->index].state_counter >= 600) {
					AiDoTerraformLand(aib->use_tile, (DiagDirection)(Random() & 3), 3, (int8)_players_ai[p->index].state_mode);
				}
				// also try the other terraform direction
				if (++_players_ai[p->index].state_counter >= 1000) {
					_players_ai[p->index].state_counter = 0;
					_players_ai[p->index].state_mode = -_players_ai[p->index].state_mode;
				}
			} else if (CheckPlayerHasMoney(cost) && AiCheckBlockDistances(p, aib->use_tile)) {
				// player has money, build it.
				CommandCost r;

				aib->cur_building_rule = rule;

				r = AiDoBuildDefaultAirportBlock(
					aib->use_tile,
					_airport_default_block_data[rule],
					DC_EXEC | DC_NO_TOWN_RATING
				);
				assert(CmdSucceeded(r));
			}
		} while (++aib, --j);
	} while (--i);

	// check if we're done with all of them
	aib = &_players_ai[p->index].src;
	j = _players_ai[p->index].num_build_rec;
	do {
		if (aib->cur_building_rule == 255) return;
	} while (++aib, --j);

	// yep, all are done. switch state.
	_players_ai[p->index].state = AIS_BUILD_AIRCRAFT_VEHICLES;
}

static StationID AiGetStationIdFromAircraftBlock(TileIndex tile, int id)
{
	const AiDefaultBlockData *p = _airport_default_block_data[id];
	while (p->mode != 1) p++;
	return GetStationIndex(TILE_ADD(tile, ToTileIndexDiff(p->tileoffs)));
}

static void AiStateBuildAircraftVehicles(Player *p)
{
	const AiDefaultBlockData *ptr;
	TileIndex tile;
	EngineID veh;
	int i;
	VehicleID loco_id;

	ptr = _airport_default_block_data[_players_ai[p->index].src.cur_building_rule];
	for (; ptr->mode != 0; ptr++) {}

	tile = TILE_ADD(_players_ai[p->index].src.use_tile, ToTileIndexDiff(ptr->tileoffs));

	/* determine forbidden aircraft bits */
	byte forbidden = 0;
	for (i = 0; _players_ai[p->index].order_list_blocks[i] != 0xFF; i++) {
		const AiBuildRec *aib = (&_players_ai[p->index].src) + _players_ai[p->index].order_list_blocks[i];
		const Station *st = GetStationByTile(aib->use_tile);

		if (st == NULL || !(st->facilities & FACIL_AIRPORT)) continue;

		AirportFTAClass::Flags flags = st->Airport()->flags;
		if (!(flags & AirportFTAClass::AIRPLANES)) forbidden |= AIR_CTOL | AIR_FAST; // no planes for heliports / oil rigs
		if (flags & AirportFTAClass::SHORT_STRIP) forbidden |= AIR_FAST; // no fast planes for small airports
	}

	veh = AiChooseAircraftToBuild(p->player_money, forbidden);
	if (veh == INVALID_ENGINE) return;

	/* XXX - Have the AI pick the hangar terminal in an airport. Eg get airport-type
	 * and offset to the FIRST depot because the AI picks the st->xy tile */
	tile += ToTileIndexDiff(GetStationByTile(tile)->Airport()->airport_depots[0]);
	if (CmdFailed(DoCommand(tile, veh, 0, DC_EXEC, CMD_BUILD_AIRCRAFT))) return;
	loco_id = _new_vehicle_id;

	for (i = 0; _players_ai[p->index].order_list_blocks[i] != 0xFF; i++) {
		AiBuildRec *aib = (&_players_ai[p->index].src) + _players_ai[p->index].order_list_blocks[i];
		bool is_pass = (_players_ai[p->index].cargo_type == CT_PASSENGERS || _players_ai[p->index].cargo_type == CT_MAIL);
		Order order;

		order.type = OT_GOTO_STATION;
		order.flags = 0;
		order.dest = AiGetStationIdFromAircraftBlock(aib->use_tile, aib->cur_building_rule);

		if (!is_pass && i == 1) order.flags |= OFB_UNLOAD;
		if (_players_ai[p->index].num_want_fullload != 0 && (is_pass || i == 0))
			order.flags |= OFB_FULL_LOAD;

		DoCommand(0, loco_id + (i << 16), PackOrder(&order), DC_EXEC, CMD_INSERT_ORDER);
	}

	DoCommand(0, loco_id, 0, DC_EXEC, CMD_START_STOP_AIRCRAFT);

	DoCommand(0, loco_id, _ai_service_interval, DC_EXEC, CMD_CHANGE_SERVICE_INT);

	if (_players_ai[p->index].num_want_fullload != 0) _players_ai[p->index].num_want_fullload--;

	if (--_players_ai[p->index].num_loco_to_build == 0) _players_ai[p->index].state = AIS_0;
}

static void AiStateCheckShipStuff(Player *p)
{
	/* Ships are not implemented in this (broken) AI */
}

static void AiStateBuildDefaultShipBlocks(Player *p)
{
	/* Ships are not implemented in this (broken) AI */
}

static void AiStateDoShipStuff(Player *p)
{
	/* Ships are not implemented in this (broken) AI */
}

static void AiStateSellVeh(Player *p)
{
	Vehicle *v = _players_ai[p->index].cur_veh;

	if (v->owner == _current_player) {
		if (v->type == VEH_TRAIN) {

			if (!IsTileDepotType(v->tile, TRANSPORT_RAIL) || v->u.rail.track != 0x80 || !(v->vehstatus&VS_STOPPED)) {
				if (v->current_order.type != OT_GOTO_DEPOT)
					DoCommand(0, v->index, 0, DC_EXEC, CMD_SEND_TRAIN_TO_DEPOT);
				goto going_to_depot;
			}

			// Sell whole train
			DoCommand(v->tile, v->index, 1, DC_EXEC, CMD_SELL_RAIL_WAGON);

		} else if (v->type == VEH_ROAD) {
			if (!v->IsStoppedInDepot()) {
				if (v->current_order.type != OT_GOTO_DEPOT)
					DoCommand(0, v->index, 0, DC_EXEC, CMD_SEND_ROADVEH_TO_DEPOT);
				goto going_to_depot;
			}

			DoCommand(0, v->index, 0, DC_EXEC, CMD_SELL_ROAD_VEH);
		} else if (v->type == VEH_AIRCRAFT) {
			if (!v->IsStoppedInDepot()) {
				if (v->current_order.type != OT_GOTO_DEPOT)
					DoCommand(0, v->index, 0, DC_EXEC, CMD_SEND_AIRCRAFT_TO_HANGAR);
				goto going_to_depot;
			}

			DoCommand(0, v->index, 0, DC_EXEC, CMD_SELL_AIRCRAFT);
		} else if (v->type == VEH_SHIP) {
			/* Ships are not implemented in this (broken) AI */
		}
	}

	goto return_to_loop;
going_to_depot:;
	if (++_players_ai[p->index].state_counter <= 832) return;

	if (v->current_order.type == OT_GOTO_DEPOT) {
		v->current_order.type = OT_DUMMY;
		v->current_order.flags = 0;
		InvalidateWindow(WC_VEHICLE_VIEW, v->index);
	}
return_to_loop:;
	_players_ai[p->index].state = AIS_VEH_LOOP;
}

static void AiStateRemoveStation(Player *p)
{
	// Remove stations that aren't in use by any vehicle
	const Order *ord;
	const Station *st;
	TileIndex tile;

	// Go to this state when we're done.
	_players_ai[p->index].state = AIS_1;

	// Get a list of all stations that are in use by a vehicle
	byte *in_use = MallocT<byte>(GetMaxStationIndex() + 1);
	memset(in_use, 0, GetMaxStationIndex() + 1);
	FOR_ALL_ORDERS(ord) {
		if (ord->type == OT_GOTO_STATION) in_use[ord->dest] = 1;
	}

	// Go through all stations and delete those that aren't in use
	FOR_ALL_STATIONS(st) {
		if (st->owner == _current_player && !in_use[st->index] &&
				( (st->bus_stops != NULL && (tile = st->bus_stops->xy) != 0) ||
					(st->truck_stops != NULL && (tile = st->truck_stops->xy)) != 0 ||
					(tile = st->train_tile) != 0 ||
					(tile = st->dock_tile) != 0 ||
					(tile = st->airport_tile) != 0)) {
			DoCommand(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
		}
	}

	free(in_use);
}

static void AiRemovePlayerRailOrRoad(Player *p, TileIndex tile)
{
	TrackBits rails;

	if (IsTileType(tile, MP_RAILWAY)) {
		if (!IsTileOwner(tile, _current_player)) return;

		if (IsPlainRailTile(tile)) {
is_rail_crossing:;
			rails = GetRailTrackStatus(tile);

			if (rails == TRACK_BIT_HORZ || rails == TRACK_BIT_VERT) return;

			if (rails & TRACK_BIT_3WAY_NE) {
pos_0:
				if ((GetRailTrackStatus(TILE_MASK(tile - TileDiffXY(1, 0))) & TRACK_BIT_3WAY_SW) == 0) {
					_players_ai[p->index].cur_dir_a = DIAGDIR_NE;
					_players_ai[p->index].cur_tile_a = tile;
					_players_ai[p->index].state = AIS_REMOVE_SINGLE_RAIL_TILE;
					return;
				}
			}

			if (rails & TRACK_BIT_3WAY_SE) {
pos_1:
				if ((GetRailTrackStatus(TILE_MASK(tile + TileDiffXY(0, 1))) & TRACK_BIT_3WAY_NW) == 0) {
					_players_ai[p->index].cur_dir_a = DIAGDIR_SE;
					_players_ai[p->index].cur_tile_a = tile;
					_players_ai[p->index].state = AIS_REMOVE_SINGLE_RAIL_TILE;
					return;
				}
			}

			if (rails & TRACK_BIT_3WAY_SW) {
pos_2:
				if ((GetRailTrackStatus(TILE_MASK(tile + TileDiffXY(1, 0))) & TRACK_BIT_3WAY_NE) == 0) {
					_players_ai[p->index].cur_dir_a = DIAGDIR_SW;
					_players_ai[p->index].cur_tile_a = tile;
					_players_ai[p->index].state = AIS_REMOVE_SINGLE_RAIL_TILE;
					return;
				}
			}

			if (rails & TRACK_BIT_3WAY_NW) {
pos_3:
				if ((GetRailTrackStatus(TILE_MASK(tile - TileDiffXY(0, 1))) & TRACK_BIT_3WAY_SE) == 0) {
					_players_ai[p->index].cur_dir_a = DIAGDIR_NW;
					_players_ai[p->index].cur_tile_a = tile;
					_players_ai[p->index].state = AIS_REMOVE_SINGLE_RAIL_TILE;
					return;
				}
			}
		} else {
			static const byte _depot_bits[] = {0x19, 0x16, 0x25, 0x2A};

			DiagDirection dir = GetRailDepotDirection(tile);

			if (GetRailTrackStatus(tile + TileOffsByDiagDir(dir)) & _depot_bits[dir])
				return;

			DoCommand(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
		}
	} else if (IsTileType(tile, MP_ROAD)) {
		if (!IsTileOwner(tile, _current_player)) return;

		if (IsLevelCrossing(tile)) goto is_rail_crossing;

		if (IsRoadDepot(tile)) {
			DiagDirection dir;
			TileIndex t;

			// Check if there are any stations around.
			t = tile + TileDiffXY(-1, 0);
			if (IsTileType(t, MP_STATION) && IsTileOwner(t, _current_player)) return;

			t = tile + TileDiffXY(1, 0);
			if (IsTileType(t, MP_STATION) && IsTileOwner(t, _current_player)) return;

			t = tile + TileDiffXY(0, -1);
			if (IsTileType(t, MP_STATION) && IsTileOwner(t, _current_player)) return;

			t = tile + TileDiffXY(0, 1);
			if (IsTileType(t, MP_STATION) && IsTileOwner(t, _current_player)) return;

			dir = GetRoadDepotDirection(tile);

			DoCommand(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
			DoCommand(
				TILE_MASK(tile + TileOffsByDiagDir(dir)),
				DiagDirToRoadBits(ReverseDiagDir(dir)),
				0,
				DC_EXEC,
				CMD_REMOVE_ROAD);
		}
	} else if (IsTileType(tile, MP_TUNNELBRIDGE)) {
		if (!IsTileOwner(tile, _current_player) ||
				!IsBridge(tile) ||
				GetTunnelBridgeTransportType(tile) != TRANSPORT_RAIL) {
			return;
		}

		rails = TRACK_BIT_NONE;

		switch (GetTunnelBridgeDirection(tile)) {
			default:
			case DIAGDIR_NE: goto pos_2;
			case DIAGDIR_SE: goto pos_3;
			case DIAGDIR_SW: goto pos_0;
			case DIAGDIR_NW: goto pos_1;
		}
	}
}

static void AiStateRemoveTrack(Player *p)
{
	/* Was 1000 for standard 8x8 maps. */
	int num = MapSizeX() * 4;

	do {
		TileIndex tile = ++_players_ai[p->index].state_counter;

		// Iterated all tiles?
		if (tile >= MapSize()) {
			_players_ai[p->index].state = AIS_REMOVE_STATION;
			return;
		}

		// Remove player stuff in that tile
		AiRemovePlayerRailOrRoad(p, tile);
		if (_players_ai[p->index].state != AIS_REMOVE_TRACK) return;
	} while (--num);
}

static void AiStateRemoveSingleRailTile(Player *p)
{
	// Remove until we can't remove more.
	if (!AiRemoveTileAndGoForward(p)) _players_ai[p->index].state = AIS_REMOVE_TRACK;
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
		p->bankrupt_timeout -= 8;
		if (p->bankrupt_timeout > 0) return;
		p->bankrupt_timeout = 0;
		DeleteWindowById(WC_BUY_COMPANY, _current_player);
		if (IsLocalPlayer()) {
			AskExitToGameMenu();
			return;
		}
		if (IsHumanPlayer(_current_player)) return;
	}

	if (p->bankrupt_asked == 255) return;

	{
		uint asked = p->bankrupt_asked;
		Player *pp, *best_pl = NULL;
		int32 best_val = -1;

		// Ask the guy with the highest performance hist.
		FOR_ALL_PLAYERS(pp) {
			if (pp->is_active &&
					!(asked & 1) &&
					pp->bankrupt_asked == 0 &&
					best_val < pp->old_economy[1].performance_history) {
				best_val = pp->old_economy[1].performance_history;
				best_pl = pp;
			}
			asked >>= 1;
		}

		// Asked all players?
		if (best_val == -1) {
			p->bankrupt_asked = 255;
			return;
		}

		SetBit(p->bankrupt_asked, best_pl->index);

		if (best_pl->index == _local_player) {
			p->bankrupt_timeout = 4440;
			ShowBuyCompanyDialog(_current_player);
			return;
		}
		if (IsHumanPlayer(best_pl->index)) return;

		// Too little money for computer to buy it?
		if (best_pl->player_money >> 1 >= p->bankrupt_value) {
			// Computer wants to buy it.
			PlayerID old_p = _current_player;
			_current_player = best_pl->index;
			DoCommand(0, old_p, 0, DC_EXEC, CMD_BUY_COMPANY);
			_current_player = old_p;
		}
	}
}

static void AiAdjustLoan(const Player* p)
{
	Money base = AiGetBasePrice(p);

	if (p->player_money > base * 1400) {
		// Decrease loan
		if (p->current_loan != 0) {
			DoCommand(0, 0, 0, DC_EXEC, CMD_DECREASE_LOAN);
		}
	} else if (p->player_money < base * 500) {
		// Increase loan
		if (p->current_loan < _economy.max_loan &&
				p->num_valid_stat_ent >= 2 &&
				-(p->old_economy[0].expenses + p->old_economy[1].expenses) < base * 60) {
			DoCommand(0, 0, 0, DC_EXEC, CMD_INCREASE_LOAN);
		}
	}
}

static void AiBuildCompanyHQ(Player *p)
{
	TileIndex tile;

	if (p->location_of_house == 0 &&
			p->last_build_coordinate != 0) {
		tile = AdjustTileCoordRandomly(p->last_build_coordinate, 8);
		DoCommand(tile, 0, 0, DC_EXEC | DC_AUTO | DC_NO_WATER, CMD_BUILD_COMPANY_HQ);
	}
}


void AiDoGameLoop(Player *p)
{
	if (p->bankrupt_asked != 0) {
		AiHandleTakeover(p);
		return;
	}

	// Ugly hack to make sure the service interval of the AI is good, not looking
	//  to the patch-setting
	// Also, it takes into account the setting if the service-interval is in days
	//  or in %
	_ai_service_interval = _patches.servint_ispercent ? 80 : 180;

	if (IsHumanPlayer(_current_player)) return;

	AiAdjustLoan(p);
	AiBuildCompanyHQ(p);

#if 0
	{
		static byte old_state = 99;
		static bool hasdots = false;
		char *_ai_state_names[] = {
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

		if (_players_ai[p->index].state != old_state) {
			if (hasdots)
				printf("\n");
			hasdots=false;
			printf("AiState: %s\n", _ai_state_names[old_state=_players_ai[p->index].state]);
		} else {
			printf(".");
			hasdots=true;
		}
	}
#endif

	_ai_actions[_players_ai[p->index].state](p);
}


static const SaveLoad _player_ai_desc[] = {
	    SLE_VAR(PlayerAI, state,             SLE_UINT8),
	    SLE_VAR(PlayerAI, tick,              SLE_UINT8),
	SLE_CONDVAR(PlayerAI, state_counter,     SLE_FILE_U16 | SLE_VAR_U32,  0, 12),
	SLE_CONDVAR(PlayerAI, state_counter,     SLE_UINT32,                 13, SL_MAX_VERSION),
	    SLE_VAR(PlayerAI, timeout_counter,   SLE_UINT16),

	    SLE_VAR(PlayerAI, state_mode,        SLE_UINT8),
	    SLE_VAR(PlayerAI, banned_tile_count, SLE_UINT8),
	    SLE_VAR(PlayerAI, railtype_to_use,   SLE_UINT8),

	    SLE_VAR(PlayerAI, cargo_type,        SLE_UINT8),
	    SLE_VAR(PlayerAI, num_wagons,        SLE_UINT8),
	    SLE_VAR(PlayerAI, build_kind,        SLE_UINT8),
	    SLE_VAR(PlayerAI, num_build_rec,     SLE_UINT8),
	    SLE_VAR(PlayerAI, num_loco_to_build, SLE_UINT8),
	    SLE_VAR(PlayerAI, num_want_fullload, SLE_UINT8),

	    SLE_VAR(PlayerAI, route_type_mask,   SLE_UINT8),

	SLE_CONDVAR(PlayerAI, start_tile_a,      SLE_FILE_U16 | SLE_VAR_U32,  0,  5),
	SLE_CONDVAR(PlayerAI, start_tile_a,      SLE_UINT32,                  6, SL_MAX_VERSION),
	SLE_CONDVAR(PlayerAI, cur_tile_a,        SLE_FILE_U16 | SLE_VAR_U32,  0,  5),
	SLE_CONDVAR(PlayerAI, cur_tile_a,        SLE_UINT32,                  6, SL_MAX_VERSION),
	    SLE_VAR(PlayerAI, start_dir_a,       SLE_UINT8),
	    SLE_VAR(PlayerAI, cur_dir_a,         SLE_UINT8),

	SLE_CONDVAR(PlayerAI, start_tile_b,      SLE_FILE_U16 | SLE_VAR_U32,  0,  5),
	SLE_CONDVAR(PlayerAI, start_tile_b,      SLE_UINT32,                  6, SL_MAX_VERSION),
	SLE_CONDVAR(PlayerAI, cur_tile_b,        SLE_FILE_U16 | SLE_VAR_U32,  0,  5),
	SLE_CONDVAR(PlayerAI, cur_tile_b,        SLE_UINT32,                  6, SL_MAX_VERSION),
	    SLE_VAR(PlayerAI, start_dir_b,       SLE_UINT8),
	    SLE_VAR(PlayerAI, cur_dir_b,         SLE_UINT8),

	    SLE_REF(PlayerAI, cur_veh,           REF_VEHICLE),

	    SLE_ARR(PlayerAI, wagon_list,        SLE_UINT16, 9),
	    SLE_ARR(PlayerAI, order_list_blocks, SLE_UINT8, 20),
	    SLE_ARR(PlayerAI, banned_tiles,      SLE_UINT16, 16),

	SLE_CONDNULL(64, 2, SL_MAX_VERSION),
	SLE_END()
};

static const SaveLoad _player_ai_build_rec_desc[] = {
	SLE_CONDVAR(AiBuildRec, spec_tile,         SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(AiBuildRec, spec_tile,         SLE_UINT32,                 6, SL_MAX_VERSION),
	SLE_CONDVAR(AiBuildRec, use_tile,          SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(AiBuildRec, use_tile,          SLE_UINT32,                 6, SL_MAX_VERSION),
	    SLE_VAR(AiBuildRec, rand_rng,          SLE_UINT8),
	    SLE_VAR(AiBuildRec, cur_building_rule, SLE_UINT8),
	    SLE_VAR(AiBuildRec, unk6,              SLE_UINT8),
	    SLE_VAR(AiBuildRec, unk7,              SLE_UINT8),
	    SLE_VAR(AiBuildRec, buildcmd_a,        SLE_UINT8),
	    SLE_VAR(AiBuildRec, buildcmd_b,        SLE_UINT8),
	    SLE_VAR(AiBuildRec, direction,         SLE_UINT8),
	    SLE_VAR(AiBuildRec, cargo,             SLE_UINT8),
	SLE_END()
};


void SaveLoad_AI(PlayerID id)
{
	PlayerAI *pai = &_players_ai[id];
	SlObject(pai, _player_ai_desc);
	for (int i = 0; i != pai->num_build_rec; i++) {
		SlObject(&pai->src + i, _player_ai_build_rec_desc);
	}
}
