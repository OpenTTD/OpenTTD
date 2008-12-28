/* $Id$ */

/** @file default.cpp The original AI. */

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
#include "../../command_func.h"
#include "../../town.h"
#include "../../industry.h"
#include "../../pathfind.h"
#include "../../airport.h"
#include "../../variables.h"
#include "../../bridge.h"
#include "../../date_func.h"
#include "../../tunnelbridge_map.h"
#include "../../window_func.h"
#include "../../vehicle_func.h"
#include "../../functions.h"
#include "../../saveload.h"
#include "../../company_func.h"
#include "../../company_base.h"
#include "../../settings_type.h"
#include "default.h"
#include "../../tunnelbridge.h"
#include "../../order_func.h"

#include "../../table/ai_rail.h"

// remove some day perhaps?
static uint _ai_service_interval;
CompanyAI _companies_ai[MAX_COMPANIES];

typedef void AiStateAction(Company *c);

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


static void AiCase0(Company *c)
{
	_companies_ai[c->index].state = AIS_REMOVE_TRACK;
	_companies_ai[c->index].state_counter = 0;
}

static void AiCase1(Company *c)
{
	_companies_ai[c->index].cur_veh = NULL;
	_companies_ai[c->index].state = AIS_VEH_LOOP;
}

static void AiStateVehLoop(Company *c)
{
	Vehicle *v;
	uint index;

	index = (_companies_ai[c->index].cur_veh == NULL) ? 0 : _companies_ai[c->index].cur_veh->index + 1;

	FOR_ALL_VEHICLES_FROM(v, index) {
		if (v->owner != _current_company) continue;

		if ((v->type == VEH_TRAIN && v->subtype == 0) ||
				v->type == VEH_ROAD ||
				(v->type == VEH_AIRCRAFT && IsNormalAircraft(v)) ||
				v->type == VEH_SHIP) {
			/* replace engine? */
			if (v->type == VEH_TRAIN && v->engine_type < 3 &&
					(_price.build_railvehicle >> 3) < c->money) {
				_companies_ai[c->index].state = AIS_VEH_CHECK_REPLACE_VEHICLE;
				_companies_ai[c->index].cur_veh = v;
				return;
			}

			/* not profitable? */
			if (v->age >= 730 &&
					v->profit_last_year < _price.station_value * 5 * 256 &&
					v->profit_this_year < _price.station_value * 5 * 256) {
				_companies_ai[c->index].state_counter = 0;
				_companies_ai[c->index].state = AIS_SELL_VEHICLE;
				_companies_ai[c->index].cur_veh = v;
				return;
			}

			/* not reliable? */
			if (v->age >= v->max_age || (
						v->age != 0 &&
						GetEngine(v->engine_type)->reliability < 35389
					)) {
				_companies_ai[c->index].state = AIS_VEH_CHECK_REPLACE_VEHICLE;
				_companies_ai[c->index].cur_veh = v;
				return;
			}
		}
	}

	_companies_ai[c->index].state = AIS_WANT_NEW_ROUTE;
	_companies_ai[c->index].state_counter = 0;
}

static EngineID AiChooseTrainToBuild(RailType railtype, Money money, byte flag, TileIndex tile)
{
	EngineID best_veh_index = INVALID_ENGINE;
	byte best_veh_score = 0;
	const Engine *e;

	FOR_ALL_ENGINES_OF_TYPE(e, VEH_TRAIN) {
		EngineID i = e->index;
		const RailVehicleInfo *rvi = &e->u.rail;

		if (!IsCompatibleRail(rvi->railtype, railtype) ||
				rvi->railveh_type == RAILVEH_WAGON ||
				(rvi->railveh_type == RAILVEH_MULTIHEAD && flag & 1) ||
				!HasBit(e->company_avail, _current_company) ||
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
	const Engine *e;

	FOR_ALL_ENGINES_OF_TYPE(e, VEH_ROAD) {
		EngineID i = e->index;
		const RoadVehicleInfo *rvi = &e->u.road;

		if (!HasBit(e->company_avail, _current_company) || e->reliability < 0x8A3D) {
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
	const Engine *e;

	FOR_ALL_ENGINES_OF_TYPE(e, VEH_AIRCRAFT) {
		EngineID i = e->index;
		const AircraftVehicleInfo *avi = &e->u.air;

		if (!HasBit(e->company_avail, _current_company) || e->reliability < 0x8A3D) {
			continue;
		}

		if ((avi->subtype & forbidden) != 0) continue;

		CommandCost ret = DoCommand(0, i, 0, DC_QUERY_COST, CMD_BUILD_AIRCRAFT);
		if (CmdSucceeded(ret) && ret.GetCost() <= money && ret.GetCost() >= best_veh_cost) {
			best_veh_cost = ret.GetCost();
			best_veh_index = i;
		}
	}

	return best_veh_index;
}

static Money AiGetBasePrice(const Company *c)
{
	Money base = _price.station_value;

	// adjust base price when more expensive vehicles are available
	switch (_companies_ai[c->index].railtype_to_use) {
		default: NOT_REACHED();
		case RAILTYPE_RAIL:     break;
		case RAILTYPE_ELECTRIC: break;
		case RAILTYPE_MONO:     base = (base * 3) >> 1; break;
		case RAILTYPE_MAGLEV:   base *= 2; break;
	}

	return base;
}

static EngineID AiChooseRoadVehToReplaceWith(const Company *c, const Vehicle *v)
{
	Money avail_money = c->money + v->value;
	return AiChooseRoadVehToBuild(v->cargo_type, avail_money, v->tile);
}

static EngineID AiChooseAircraftToReplaceWith(const Company *c, const Vehicle *v)
{
	Money avail_money = c->money + v->value;

	/* determine forbidden aircraft bits */
	byte forbidden = 0;
	const Order *o;

	FOR_VEHICLE_ORDERS(v, o) {
		if (!o->IsValid()) continue;
		if (!IsValidStationID(o->GetDestination())) continue;
		const Station *st = GetStation(o->GetDestination());
		if (!(st->facilities & FACIL_AIRPORT)) continue;

		AirportFTAClass::Flags flags = st->Airport()->flags;
		if (!(flags & AirportFTAClass::AIRPLANES)) forbidden |= AIR_CTOL | AIR_FAST; // no planes for heliports / oil rigs
		if (flags & AirportFTAClass::SHORT_STRIP) forbidden |= AIR_FAST; // no fast planes for small airports
	}

	return AiChooseAircraftToBuild(
		avail_money, forbidden
	);
}

static EngineID AiChooseTrainToReplaceWith(const Company *c, const Vehicle *v)
{
	Money avail_money = c->money + v->value;
	const Vehicle *u = v;
	int num = 0;

	while (++num, u->Next() != NULL) {
		u = u->Next();
	}

	// XXX: check if a wagon
	return AiChooseTrainToBuild(v->u.rail.railtype, avail_money, 0, v->tile);
}

static EngineID AiChooseShipToReplaceWith(const Company *c, const Vehicle *v)
{
	/* Ships are not implemented in this (broken) AI */
	return INVALID_ENGINE;
}

static void AiHandleGotoDepot(Company *c, int cmd)
{
	if (!_companies_ai[c->index].cur_veh->current_order.IsType(OT_GOTO_DEPOT))
		DoCommand(0, _companies_ai[c->index].cur_veh->index, 0, DC_EXEC, cmd);

	if (++_companies_ai[c->index].state_counter <= 1387) {
		_companies_ai[c->index].state = AIS_VEH_DO_REPLACE_VEHICLE;
		return;
	}

	if (_companies_ai[c->index].cur_veh->current_order.IsType(OT_GOTO_DEPOT)) {
		_companies_ai[c->index].cur_veh->current_order.MakeDummy();
		InvalidateWindow(WC_VEHICLE_VIEW, _companies_ai[c->index].cur_veh->index);
	}
}

static void AiRestoreVehicleOrders(Vehicle *v, BackuppedOrders *bak)
{
	if (bak->order == NULL) return;

	for (uint i = 0; !bak->order[i].IsType(OT_NOTHING); i++) {
		if (!DoCommandP(0, v->index + (i << 16), bak->order[i].Pack(), CMD_INSERT_ORDER | CMD_NO_TEST_IF_IN_NETWORK))
			break;
	}
}

static void AiHandleReplaceTrain(Company *c)
{
	const Vehicle* v = _companies_ai[c->index].cur_veh;
	BackuppedOrders orderbak;
	EngineID veh;

	// wait until the vehicle reaches the depot.
	if (!IsRailDepotTile(v->tile) || v->u.rail.track != TRACK_BIT_DEPOT || !(v->vehstatus & VS_STOPPED)) {
		AiHandleGotoDepot(c, CMD_SEND_TRAIN_TO_DEPOT);
		return;
	}

	veh = AiChooseTrainToReplaceWith(c, v);
	if (veh != INVALID_ENGINE) {
		TileIndex tile;

		BackupVehicleOrders(v, &orderbak);
		tile = v->tile;

		if (CmdSucceeded(DoCommand(0, v->index, 2, DC_EXEC, CMD_SELL_RAIL_WAGON)) &&
				CmdSucceeded(DoCommand(tile, veh, 0, DC_EXEC, CMD_BUILD_RAIL_VEHICLE))) {
			VehicleID veh = _new_vehicle_id;
			AiRestoreVehicleOrders(GetVehicle(veh), &orderbak);
			DoCommand(0, veh, 0, DC_EXEC, CMD_START_STOP_VEHICLE);

			DoCommand(0, veh, _ai_service_interval, DC_EXEC, CMD_CHANGE_SERVICE_INT);
		}
	}
}

static void AiHandleReplaceRoadVeh(Company *c)
{
	const Vehicle* v = _companies_ai[c->index].cur_veh;
	BackuppedOrders orderbak;
	EngineID veh;

	if (!v->IsStoppedInDepot()) {
		AiHandleGotoDepot(c, CMD_SEND_ROADVEH_TO_DEPOT);
		return;
	}

	veh = AiChooseRoadVehToReplaceWith(c, v);
	if (veh != INVALID_ENGINE) {
		TileIndex tile;

		BackupVehicleOrders(v, &orderbak);
		tile = v->tile;

		if (CmdSucceeded(DoCommand(0, v->index, 0, DC_EXEC, CMD_SELL_ROAD_VEH)) &&
				CmdSucceeded(DoCommand(tile, veh, 0, DC_EXEC, CMD_BUILD_ROAD_VEH))) {
			VehicleID veh = _new_vehicle_id;

			AiRestoreVehicleOrders(GetVehicle(veh), &orderbak);
			DoCommand(0, veh, 0, DC_EXEC, CMD_START_STOP_VEHICLE);
			DoCommand(0, veh, _ai_service_interval, DC_EXEC, CMD_CHANGE_SERVICE_INT);
		}
	}
}

static void AiHandleReplaceAircraft(Company *c)
{
	const Vehicle* v = _companies_ai[c->index].cur_veh;
	BackuppedOrders orderbak;
	EngineID veh;

	if (!v->IsStoppedInDepot()) {
		AiHandleGotoDepot(c, CMD_SEND_AIRCRAFT_TO_HANGAR);
		return;
	}

	veh = AiChooseAircraftToReplaceWith(c, v);
	if (veh != INVALID_ENGINE) {
		TileIndex tile;

		BackupVehicleOrders(v, &orderbak);
		tile = v->tile;

		if (CmdSucceeded(DoCommand(0, v->index, 0, DC_EXEC, CMD_SELL_AIRCRAFT)) &&
				CmdSucceeded(DoCommand(tile, veh, 0, DC_EXEC, CMD_BUILD_AIRCRAFT))) {
			VehicleID veh = _new_vehicle_id;
			AiRestoreVehicleOrders(GetVehicle(veh), &orderbak);
			DoCommand(0, veh, 0, DC_EXEC, CMD_START_STOP_VEHICLE);

			DoCommand(0, veh, _ai_service_interval, DC_EXEC, CMD_CHANGE_SERVICE_INT);
		}
	}
}

static void AiHandleReplaceShip(Company *c)
{
	/* Ships are not implemented in this (broken) AI */
}

typedef EngineID CheckReplaceProc(const Company *c, const Vehicle* v);

static CheckReplaceProc* const _veh_check_replace_proc[] = {
	AiChooseTrainToReplaceWith,
	AiChooseRoadVehToReplaceWith,
	AiChooseShipToReplaceWith,
	AiChooseAircraftToReplaceWith,
};

typedef void DoReplaceProc(Company *c);
static DoReplaceProc* const _veh_do_replace_proc[] = {
	AiHandleReplaceTrain,
	AiHandleReplaceRoadVeh,
	AiHandleReplaceShip,
	AiHandleReplaceAircraft
};

static void AiStateCheckReplaceVehicle(Company *c)
{
	const Vehicle* v = _companies_ai[c->index].cur_veh;

	if (!v->IsValid() ||
			v->owner != _current_company ||
			v->type > VEH_SHIP ||
			_veh_check_replace_proc[v->type - VEH_TRAIN](c, v) == INVALID_ENGINE) {
		_companies_ai[c->index].state = AIS_VEH_LOOP;
	} else {
		_companies_ai[c->index].state_counter = 0;
		_companies_ai[c->index].state = AIS_VEH_DO_REPLACE_VEHICLE;
	}
}

static void AiStateDoReplaceVehicle(Company *c)
{
	const Vehicle* v = _companies_ai[c->index].cur_veh;

	_companies_ai[c->index].state = AIS_VEH_LOOP;
	// vehicle is not owned by the company anymore, something went very wrong.
	if (!v->IsValid() || v->owner != _current_company) return;
	_veh_do_replace_proc[v->type - VEH_TRAIN](c);
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

static bool AiCheckIfRouteIsGood(Company *c, FoundRoute *fr, byte bitmask)
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

		if (st->owner != _current_company) continue;
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

	if (_companies_ai[c->index].route_type_mask != 0 &&
			!(_companies_ai[c->index].route_type_mask & bitmask) &&
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
		if (from->ratings[_current_company] < -100 ||
				to->ratings[_current_company] < -100) {
			return false;
		}
	} else {
		const Industry* i = (const Industry*)fr->from;

		if (i->last_month_pct_transported[fr->cargo != i->produced_cargo[0]] > 0x99 ||
				i->last_month_production[fr->cargo != i->produced_cargo[0]] == 0) {
			return false;
		}
	}

	_companies_ai[c->index].route_type_mask |= bitmask;
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

static void AiWantLongIndustryRoute(Company *c)
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

	if (!AiCheckIfRouteIsGood(c, &fr, 1)) return;

	// Fill the source field
	_companies_ai[c->index].dst.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.to);
	_companies_ai[c->index].src.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.from);

	_companies_ai[c->index].src.use_tile = 0;
	_companies_ai[c->index].src.rand_rng = 9;
	_companies_ai[c->index].src.cur_building_rule = 0xFF;
	_companies_ai[c->index].src.unk6 = 1;
	_companies_ai[c->index].src.unk7 = 0;
	_companies_ai[c->index].src.buildcmd_a = 0x24;
	_companies_ai[c->index].src.buildcmd_b = 0xFF;
	_companies_ai[c->index].src.direction = AiGetDirectionBetweenTiles(
		_companies_ai[c->index].src.spec_tile,
		_companies_ai[c->index].dst.spec_tile
	);
	_companies_ai[c->index].src.cargo = fr.cargo | 0x80;

	// Fill the dest field

	_companies_ai[c->index].dst.use_tile = 0;
	_companies_ai[c->index].dst.rand_rng = 9;
	_companies_ai[c->index].dst.cur_building_rule = 0xFF;
	_companies_ai[c->index].dst.unk6 = 1;
	_companies_ai[c->index].dst.unk7 = 0;
	_companies_ai[c->index].dst.buildcmd_a = 0x34;
	_companies_ai[c->index].dst.buildcmd_b = 0xFF;
	_companies_ai[c->index].dst.direction = AiGetDirectionBetweenTiles(
		_companies_ai[c->index].dst.spec_tile,
		_companies_ai[c->index].src.spec_tile
	);
	_companies_ai[c->index].dst.cargo = fr.cargo;

	// Fill middle field 1
	_companies_ai[c->index].mid1.spec_tile = AiGetPctTileBetween(
		_companies_ai[c->index].src.spec_tile,
		_companies_ai[c->index].dst.spec_tile,
		0x55
	);
	_companies_ai[c->index].mid1.use_tile = 0;
	_companies_ai[c->index].mid1.rand_rng = 6;
	_companies_ai[c->index].mid1.cur_building_rule = 0xFF;
	_companies_ai[c->index].mid1.unk6 = 2;
	_companies_ai[c->index].mid1.unk7 = 1;
	_companies_ai[c->index].mid1.buildcmd_a = 0x30;
	_companies_ai[c->index].mid1.buildcmd_b = 0xFF;
	_companies_ai[c->index].mid1.direction = _companies_ai[c->index].src.direction;
	_companies_ai[c->index].mid1.cargo = fr.cargo;

	// Fill middle field 2
	_companies_ai[c->index].mid2.spec_tile = AiGetPctTileBetween(
		_companies_ai[c->index].src.spec_tile,
		_companies_ai[c->index].dst.spec_tile,
		0xAA
	);
	_companies_ai[c->index].mid2.use_tile = 0;
	_companies_ai[c->index].mid2.rand_rng = 6;
	_companies_ai[c->index].mid2.cur_building_rule = 0xFF;
	_companies_ai[c->index].mid2.unk6 = 2;
	_companies_ai[c->index].mid2.unk7 = 1;
	_companies_ai[c->index].mid2.buildcmd_a = 0xFF;
	_companies_ai[c->index].mid2.buildcmd_b = 0xFF;
	_companies_ai[c->index].mid2.direction = _companies_ai[c->index].dst.direction;
	_companies_ai[c->index].mid2.cargo = fr.cargo;

	// Fill common fields
	_companies_ai[c->index].cargo_type = fr.cargo;
	_companies_ai[c->index].num_wagons = 3;
	_companies_ai[c->index].build_kind = 2;
	_companies_ai[c->index].num_build_rec = 4;
	_companies_ai[c->index].num_loco_to_build = 2;
	_companies_ai[c->index].num_want_fullload = 2;
	_companies_ai[c->index].wagon_list[0] = INVALID_VEHICLE;
	_companies_ai[c->index].order_list_blocks[0] = 0;
	_companies_ai[c->index].order_list_blocks[1] = 1;
	_companies_ai[c->index].order_list_blocks[2] = 255;

	_companies_ai[c->index].state = AIS_BUILD_DEFAULT_RAIL_BLOCKS;
	_companies_ai[c->index].state_mode = UCHAR_MAX;
	_companies_ai[c->index].state_counter = 0;
	_companies_ai[c->index].timeout_counter = 0;
}

static void AiWantMediumIndustryRoute(Company *c)
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

	if (!AiCheckIfRouteIsGood(c, &fr, 1)) return;

	// Fill the source field
	_companies_ai[c->index].src.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.from);
	_companies_ai[c->index].src.use_tile = 0;
	_companies_ai[c->index].src.rand_rng = 9;
	_companies_ai[c->index].src.cur_building_rule = 0xFF;
	_companies_ai[c->index].src.unk6 = 1;
	_companies_ai[c->index].src.unk7 = 0;
	_companies_ai[c->index].src.buildcmd_a = 0x10;
	_companies_ai[c->index].src.buildcmd_b = 0xFF;
	_companies_ai[c->index].src.direction = AiGetDirectionBetweenTiles(
		GET_TOWN_OR_INDUSTRY_TILE(fr.from),
		GET_TOWN_OR_INDUSTRY_TILE(fr.to)
	);
	_companies_ai[c->index].src.cargo = fr.cargo | 0x80;

	// Fill the dest field
	_companies_ai[c->index].dst.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.to);
	_companies_ai[c->index].dst.use_tile = 0;
	_companies_ai[c->index].dst.rand_rng = 9;
	_companies_ai[c->index].dst.cur_building_rule = 0xFF;
	_companies_ai[c->index].dst.unk6 = 1;
	_companies_ai[c->index].dst.unk7 = 0;
	_companies_ai[c->index].dst.buildcmd_a = 0xFF;
	_companies_ai[c->index].dst.buildcmd_b = 0xFF;
	_companies_ai[c->index].dst.direction = AiGetDirectionBetweenTiles(
		GET_TOWN_OR_INDUSTRY_TILE(fr.to),
		GET_TOWN_OR_INDUSTRY_TILE(fr.from)
	);
	_companies_ai[c->index].dst.cargo = fr.cargo;

	// Fill common fields
	_companies_ai[c->index].cargo_type = fr.cargo;
	_companies_ai[c->index].num_wagons = 3;
	_companies_ai[c->index].build_kind = 1;
	_companies_ai[c->index].num_build_rec = 2;
	_companies_ai[c->index].num_loco_to_build = 1;
	_companies_ai[c->index].num_want_fullload = 1;
	_companies_ai[c->index].wagon_list[0] = INVALID_VEHICLE;
	_companies_ai[c->index].order_list_blocks[0] = 0;
	_companies_ai[c->index].order_list_blocks[1] = 1;
	_companies_ai[c->index].order_list_blocks[2] = 255;
	_companies_ai[c->index].state = AIS_BUILD_DEFAULT_RAIL_BLOCKS;
	_companies_ai[c->index].state_mode = UCHAR_MAX;
	_companies_ai[c->index].state_counter = 0;
	_companies_ai[c->index].timeout_counter = 0;
}

static void AiWantShortIndustryRoute(Company *c)
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

	if (!AiCheckIfRouteIsGood(c, &fr, 1)) return;

	// Fill the source field
	_companies_ai[c->index].src.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.from);
	_companies_ai[c->index].src.use_tile = 0;
	_companies_ai[c->index].src.rand_rng = 9;
	_companies_ai[c->index].src.cur_building_rule = 0xFF;
	_companies_ai[c->index].src.unk6 = 1;
	_companies_ai[c->index].src.unk7 = 0;
	_companies_ai[c->index].src.buildcmd_a = 0x10;
	_companies_ai[c->index].src.buildcmd_b = 0xFF;
	_companies_ai[c->index].src.direction = AiGetDirectionBetweenTiles(
		GET_TOWN_OR_INDUSTRY_TILE(fr.from),
		GET_TOWN_OR_INDUSTRY_TILE(fr.to)
	);
	_companies_ai[c->index].src.cargo = fr.cargo | 0x80;

	// Fill the dest field
	_companies_ai[c->index].dst.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.to);
	_companies_ai[c->index].dst.use_tile = 0;
	_companies_ai[c->index].dst.rand_rng = 9;
	_companies_ai[c->index].dst.cur_building_rule = 0xFF;
	_companies_ai[c->index].dst.unk6 = 1;
	_companies_ai[c->index].dst.unk7 = 0;
	_companies_ai[c->index].dst.buildcmd_a = 0xFF;
	_companies_ai[c->index].dst.buildcmd_b = 0xFF;
	_companies_ai[c->index].dst.direction = AiGetDirectionBetweenTiles(
		GET_TOWN_OR_INDUSTRY_TILE(fr.to),
		GET_TOWN_OR_INDUSTRY_TILE(fr.from)
	);
	_companies_ai[c->index].dst.cargo = fr.cargo;

	// Fill common fields
	_companies_ai[c->index].cargo_type = fr.cargo;
	_companies_ai[c->index].num_wagons = 2;
	_companies_ai[c->index].build_kind = 1;
	_companies_ai[c->index].num_build_rec = 2;
	_companies_ai[c->index].num_loco_to_build = 1;
	_companies_ai[c->index].num_want_fullload = 1;
	_companies_ai[c->index].wagon_list[0] = INVALID_VEHICLE;
	_companies_ai[c->index].order_list_blocks[0] = 0;
	_companies_ai[c->index].order_list_blocks[1] = 1;
	_companies_ai[c->index].order_list_blocks[2] = 255;
	_companies_ai[c->index].state = AIS_BUILD_DEFAULT_RAIL_BLOCKS;
	_companies_ai[c->index].state_mode = UCHAR_MAX;
	_companies_ai[c->index].state_counter = 0;
	_companies_ai[c->index].timeout_counter = 0;
}

static void AiWantMailRoute(Company *c)
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
	if (!AiCheckIfRouteIsGood(c, &fr, 1)) return;

	// Fill the source field
	_companies_ai[c->index].src.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.from);
	_companies_ai[c->index].src.use_tile = 0;
	_companies_ai[c->index].src.rand_rng = 7;
	_companies_ai[c->index].src.cur_building_rule = 0xFF;
	_companies_ai[c->index].src.unk6 = 1;
	_companies_ai[c->index].src.unk7 = 0;
	_companies_ai[c->index].src.buildcmd_a = 0x24;
	_companies_ai[c->index].src.buildcmd_b = 0xFF;
	_companies_ai[c->index].src.direction = AiGetDirectionBetweenTiles(
		GET_TOWN_OR_INDUSTRY_TILE(fr.from),
		GET_TOWN_OR_INDUSTRY_TILE(fr.to)
	);
	_companies_ai[c->index].src.cargo = fr.cargo;

	// Fill the dest field
	_companies_ai[c->index].dst.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.to);
	_companies_ai[c->index].dst.use_tile = 0;
	_companies_ai[c->index].dst.rand_rng = 7;
	_companies_ai[c->index].dst.cur_building_rule = 0xFF;
	_companies_ai[c->index].dst.unk6 = 1;
	_companies_ai[c->index].dst.unk7 = 0;
	_companies_ai[c->index].dst.buildcmd_a = 0x34;
	_companies_ai[c->index].dst.buildcmd_b = 0xFF;
	_companies_ai[c->index].dst.direction = AiGetDirectionBetweenTiles(
		GET_TOWN_OR_INDUSTRY_TILE(fr.to),
		GET_TOWN_OR_INDUSTRY_TILE(fr.from)
	);
	_companies_ai[c->index].dst.cargo = fr.cargo;

	// Fill middle field 1
	_companies_ai[c->index].mid1.spec_tile = AiGetPctTileBetween(
		GET_TOWN_OR_INDUSTRY_TILE(fr.from),
		GET_TOWN_OR_INDUSTRY_TILE(fr.to),
		0x55
	);
	_companies_ai[c->index].mid1.use_tile = 0;
	_companies_ai[c->index].mid1.rand_rng = 6;
	_companies_ai[c->index].mid1.cur_building_rule = 0xFF;
	_companies_ai[c->index].mid1.unk6 = 2;
	_companies_ai[c->index].mid1.unk7 = 1;
	_companies_ai[c->index].mid1.buildcmd_a = 0x30;
	_companies_ai[c->index].mid1.buildcmd_b = 0xFF;
	_companies_ai[c->index].mid1.direction = _companies_ai[c->index].src.direction;
	_companies_ai[c->index].mid1.cargo = fr.cargo;

	// Fill middle field 2
	_companies_ai[c->index].mid2.spec_tile = AiGetPctTileBetween(
		GET_TOWN_OR_INDUSTRY_TILE(fr.from),
		GET_TOWN_OR_INDUSTRY_TILE(fr.to),
		0xAA
	);
	_companies_ai[c->index].mid2.use_tile = 0;
	_companies_ai[c->index].mid2.rand_rng = 6;
	_companies_ai[c->index].mid2.cur_building_rule = 0xFF;
	_companies_ai[c->index].mid2.unk6 = 2;
	_companies_ai[c->index].mid2.unk7 = 1;
	_companies_ai[c->index].mid2.buildcmd_a = 0xFF;
	_companies_ai[c->index].mid2.buildcmd_b = 0xFF;
	_companies_ai[c->index].mid2.direction = _companies_ai[c->index].dst.direction;
	_companies_ai[c->index].mid2.cargo = fr.cargo;

	// Fill common fields
	_companies_ai[c->index].cargo_type = fr.cargo;
	_companies_ai[c->index].num_wagons = 3;
	_companies_ai[c->index].build_kind = 2;
	_companies_ai[c->index].num_build_rec = 4;
	_companies_ai[c->index].num_loco_to_build = 2;
	_companies_ai[c->index].num_want_fullload = 0;
	_companies_ai[c->index].wagon_list[0] = INVALID_VEHICLE;
	_companies_ai[c->index].order_list_blocks[0] = 0;
	_companies_ai[c->index].order_list_blocks[1] = 1;
	_companies_ai[c->index].order_list_blocks[2] = 255;
	_companies_ai[c->index].state = AIS_BUILD_DEFAULT_RAIL_BLOCKS;
	_companies_ai[c->index].state_mode = UCHAR_MAX;
	_companies_ai[c->index].state_counter = 0;
	_companies_ai[c->index].timeout_counter = 0;
}

static void AiWantPassengerRoute(Company *c)
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
	if (!AiCheckIfRouteIsGood(c, &fr, 1)) return;

	// Fill the source field
	_companies_ai[c->index].src.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.from);
	_companies_ai[c->index].src.use_tile = 0;
	_companies_ai[c->index].src.rand_rng = 7;
	_companies_ai[c->index].src.cur_building_rule = 0xFF;
	_companies_ai[c->index].src.unk6 = 1;
	_companies_ai[c->index].src.unk7 = 0;
	_companies_ai[c->index].src.buildcmd_a = 0x10;
	_companies_ai[c->index].src.buildcmd_b = 0xFF;
	_companies_ai[c->index].src.direction = AiGetDirectionBetweenTiles(
		GET_TOWN_OR_INDUSTRY_TILE(fr.from),
		GET_TOWN_OR_INDUSTRY_TILE(fr.to)
	);
	_companies_ai[c->index].src.cargo = fr.cargo;

	// Fill the dest field
	_companies_ai[c->index].dst.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.to);
	_companies_ai[c->index].dst.use_tile = 0;
	_companies_ai[c->index].dst.rand_rng = 7;
	_companies_ai[c->index].dst.cur_building_rule = 0xFF;
	_companies_ai[c->index].dst.unk6 = 1;
	_companies_ai[c->index].dst.unk7 = 0;
	_companies_ai[c->index].dst.buildcmd_a = 0xFF;
	_companies_ai[c->index].dst.buildcmd_b = 0xFF;
	_companies_ai[c->index].dst.direction = AiGetDirectionBetweenTiles(
		GET_TOWN_OR_INDUSTRY_TILE(fr.to),
		GET_TOWN_OR_INDUSTRY_TILE(fr.from)
	);
	_companies_ai[c->index].dst.cargo = fr.cargo;

	// Fill common fields
	_companies_ai[c->index].cargo_type = fr.cargo;
	_companies_ai[c->index].num_wagons = 2;
	_companies_ai[c->index].build_kind = 1;
	_companies_ai[c->index].num_build_rec = 2;
	_companies_ai[c->index].num_loco_to_build = 1;
	_companies_ai[c->index].num_want_fullload = 0;
	_companies_ai[c->index].wagon_list[0] = INVALID_VEHICLE;
	_companies_ai[c->index].order_list_blocks[0] = 0;
	_companies_ai[c->index].order_list_blocks[1] = 1;
	_companies_ai[c->index].order_list_blocks[2] = 255;
	_companies_ai[c->index].state = AIS_BUILD_DEFAULT_RAIL_BLOCKS;
	_companies_ai[c->index].state_mode = UCHAR_MAX;
	_companies_ai[c->index].state_counter = 0;
	_companies_ai[c->index].timeout_counter = 0;
}

static void AiWantTrainRoute(Company *c)
{
	uint16 r = GB(Random(), 0, 16);

	_companies_ai[c->index].railtype_to_use = GetBestRailtype(c->index);

	if (r > 0xD000) {
		AiWantLongIndustryRoute(c);
	} else if (r > 0x6000) {
		AiWantMediumIndustryRoute(c);
	} else if (r > 0x1000) {
		AiWantShortIndustryRoute(c);
	} else if (r > 0x800) {
		AiWantPassengerRoute(c);
	} else {
		AiWantMailRoute(c);
	}
}

static void AiWantLongRoadIndustryRoute(Company *c)
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

	if (!AiCheckIfRouteIsGood(c, &fr, 2)) return;

	// Fill the source field
	_companies_ai[c->index].src.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.from);
	_companies_ai[c->index].src.use_tile = 0;
	_companies_ai[c->index].src.rand_rng = 9;
	_companies_ai[c->index].src.cur_building_rule = 0xFF;
	_companies_ai[c->index].src.buildcmd_a = 1;
	_companies_ai[c->index].src.direction = 0;
	_companies_ai[c->index].src.cargo = fr.cargo | 0x80;

	// Fill the dest field
	_companies_ai[c->index].dst.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.to);
	_companies_ai[c->index].dst.use_tile = 0;
	_companies_ai[c->index].dst.rand_rng = 9;
	_companies_ai[c->index].dst.cur_building_rule = 0xFF;
	_companies_ai[c->index].dst.buildcmd_a = 0xFF;
	_companies_ai[c->index].dst.direction = 0;
	_companies_ai[c->index].dst.cargo = fr.cargo;

	// Fill common fields
	_companies_ai[c->index].cargo_type = fr.cargo;
	_companies_ai[c->index].num_build_rec = 2;
	_companies_ai[c->index].num_loco_to_build = 5;
	_companies_ai[c->index].num_want_fullload = 5;

//	_companies_ai[c->index].loco_id = INVALID_VEHICLE;
	_companies_ai[c->index].order_list_blocks[0] = 0;
	_companies_ai[c->index].order_list_blocks[1] = 1;
	_companies_ai[c->index].order_list_blocks[2] = 255;

	_companies_ai[c->index].state = AIS_BUILD_DEFAULT_ROAD_BLOCKS;
	_companies_ai[c->index].state_mode = UCHAR_MAX;
	_companies_ai[c->index].state_counter = 0;
	_companies_ai[c->index].timeout_counter = 0;
}

static void AiWantMediumRoadIndustryRoute(Company *c)
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

	if (!AiCheckIfRouteIsGood(c, &fr, 2)) return;

	// Fill the source field
	_companies_ai[c->index].src.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.from);
	_companies_ai[c->index].src.use_tile = 0;
	_companies_ai[c->index].src.rand_rng = 9;
	_companies_ai[c->index].src.cur_building_rule = 0xFF;
	_companies_ai[c->index].src.buildcmd_a = 1;
	_companies_ai[c->index].src.direction = 0;
	_companies_ai[c->index].src.cargo = fr.cargo | 0x80;

	// Fill the dest field
	_companies_ai[c->index].dst.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.to);
	_companies_ai[c->index].dst.use_tile = 0;
	_companies_ai[c->index].dst.rand_rng = 9;
	_companies_ai[c->index].dst.cur_building_rule = 0xFF;
	_companies_ai[c->index].dst.buildcmd_a = 0xFF;
	_companies_ai[c->index].dst.direction = 0;
	_companies_ai[c->index].dst.cargo = fr.cargo;

	// Fill common fields
	_companies_ai[c->index].cargo_type = fr.cargo;
	_companies_ai[c->index].num_build_rec = 2;
	_companies_ai[c->index].num_loco_to_build = 3;
	_companies_ai[c->index].num_want_fullload = 3;

//	_companies_ai[c->index].loco_id = INVALID_VEHICLE;
	_companies_ai[c->index].order_list_blocks[0] = 0;
	_companies_ai[c->index].order_list_blocks[1] = 1;
	_companies_ai[c->index].order_list_blocks[2] = 255;

	_companies_ai[c->index].state = AIS_BUILD_DEFAULT_ROAD_BLOCKS;
	_companies_ai[c->index].state_mode = UCHAR_MAX;
	_companies_ai[c->index].state_counter = 0;
	_companies_ai[c->index].timeout_counter = 0;
}

static void AiWantLongRoadPassengerRoute(Company *c)
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

	if (!AiCheckIfRouteIsGood(c, &fr, 2)) return;

	// Fill the source field
	_companies_ai[c->index].src.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.to);
	_companies_ai[c->index].src.use_tile = 0;
	_companies_ai[c->index].src.rand_rng = 10;
	_companies_ai[c->index].src.cur_building_rule = 0xFF;
	_companies_ai[c->index].src.buildcmd_a = 1;
	_companies_ai[c->index].src.direction = 0;
	_companies_ai[c->index].src.cargo = CT_PASSENGERS;

	// Fill the dest field
	_companies_ai[c->index].dst.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.from);
	_companies_ai[c->index].dst.use_tile = 0;
	_companies_ai[c->index].dst.rand_rng = 10;
	_companies_ai[c->index].dst.cur_building_rule = 0xFF;
	_companies_ai[c->index].dst.buildcmd_a = 0xFF;
	_companies_ai[c->index].dst.direction = 0;
	_companies_ai[c->index].dst.cargo = CT_PASSENGERS;

	// Fill common fields
	_companies_ai[c->index].cargo_type = CT_PASSENGERS;
	_companies_ai[c->index].num_build_rec = 2;
	_companies_ai[c->index].num_loco_to_build = 4;
	_companies_ai[c->index].num_want_fullload = 0;

//	_companies_ai[c->index].loco_id = INVALID_VEHICLE;
	_companies_ai[c->index].order_list_blocks[0] = 0;
	_companies_ai[c->index].order_list_blocks[1] = 1;
	_companies_ai[c->index].order_list_blocks[2] = 255;

	_companies_ai[c->index].state = AIS_BUILD_DEFAULT_ROAD_BLOCKS;
	_companies_ai[c->index].state_mode = UCHAR_MAX;
	_companies_ai[c->index].state_counter = 0;
	_companies_ai[c->index].timeout_counter = 0;
}

static void AiWantPassengerRouteInsideTown(Company *c)
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

	if (!AiCheckIfRouteIsGood(c, &fr, 2)) return;

	// Fill the source field
	_companies_ai[c->index].src.spec_tile = t->xy;
	_companies_ai[c->index].src.use_tile = 0;
	_companies_ai[c->index].src.rand_rng = 10;
	_companies_ai[c->index].src.cur_building_rule = 0xFF;
	_companies_ai[c->index].src.buildcmd_a = 1;
	_companies_ai[c->index].src.direction = 0;
	_companies_ai[c->index].src.cargo = CT_PASSENGERS;

	// Fill the dest field
	_companies_ai[c->index].dst.spec_tile = t->xy;
	_companies_ai[c->index].dst.use_tile = 0;
	_companies_ai[c->index].dst.rand_rng = 10;
	_companies_ai[c->index].dst.cur_building_rule = 0xFF;
	_companies_ai[c->index].dst.buildcmd_a = 0xFF;
	_companies_ai[c->index].dst.direction = 0;
	_companies_ai[c->index].dst.cargo = CT_PASSENGERS;

	// Fill common fields
	_companies_ai[c->index].cargo_type = CT_PASSENGERS;
	_companies_ai[c->index].num_build_rec = 2;
	_companies_ai[c->index].num_loco_to_build = 2;
	_companies_ai[c->index].num_want_fullload = 0;

//	_companies_ai[c->index].loco_id = INVALID_VEHICLE;
	_companies_ai[c->index].order_list_blocks[0] = 0;
	_companies_ai[c->index].order_list_blocks[1] = 1;
	_companies_ai[c->index].order_list_blocks[2] = 255;

	_companies_ai[c->index].state = AIS_BUILD_DEFAULT_ROAD_BLOCKS;
	_companies_ai[c->index].state_mode = UCHAR_MAX;
	_companies_ai[c->index].state_counter = 0;
	_companies_ai[c->index].timeout_counter = 0;
}

static void AiWantRoadRoute(Company *c)
{
	uint16 r = GB(Random(), 0, 16);

	if (r > 0x4000) {
		AiWantLongRoadIndustryRoute(c);
	} else if (r > 0x2000) {
		AiWantMediumRoadIndustryRoute(c);
	} else if (r > 0x1000) {
		AiWantLongRoadPassengerRoute(c);
	} else {
		AiWantPassengerRouteInsideTown(c);
	}
}

static void AiWantPassengerAircraftRoute(Company *c)
{
	FoundRoute fr;
	int i;

	/* Get aircraft that would be bought for this route
	 * (probably, as conditions may change before the route is fully built,
	 * like running out of money and having to select different aircraft, etc ...) */
	EngineID veh = AiChooseAircraftToBuild(c->money, _companies_ai[c->index].build_kind != 0 ? AIR_CTOL : 0);

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
	if (!AiCheckIfRouteIsGood(c, &fr, 4)) return;


	// Fill the source field
	_companies_ai[c->index].src.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.to);
	_companies_ai[c->index].src.use_tile = 0;
	_companies_ai[c->index].src.rand_rng = 12;
	_companies_ai[c->index].src.cur_building_rule = 0xFF;
	_companies_ai[c->index].src.cargo = fr.cargo;

	// Fill the dest field
	_companies_ai[c->index].dst.spec_tile = GET_TOWN_OR_INDUSTRY_TILE(fr.from);
	_companies_ai[c->index].dst.use_tile = 0;
	_companies_ai[c->index].dst.rand_rng = 12;
	_companies_ai[c->index].dst.cur_building_rule = 0xFF;
	_companies_ai[c->index].dst.cargo = fr.cargo;

	// Fill common fields
	_companies_ai[c->index].cargo_type = fr.cargo;
	_companies_ai[c->index].build_kind = 0;
	_companies_ai[c->index].num_build_rec = 2;
	_companies_ai[c->index].num_loco_to_build = 1;
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
	_companies_ai[c->index].num_want_fullload = Chance16(1, 5); // 20% chance
//	_companies_ai[c->index].loco_id = INVALID_VEHICLE;
	_companies_ai[c->index].order_list_blocks[0] = 0;
	_companies_ai[c->index].order_list_blocks[1] = 1;
	_companies_ai[c->index].order_list_blocks[2] = 255;

	_companies_ai[c->index].state = AIS_AIRPORT_STUFF;
	_companies_ai[c->index].timeout_counter = 0;
}

static void AiWantOilRigAircraftRoute(Company *c)
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

	if (!AiCheckIfRouteIsGood(c, &fr, 4)) return;

	// Fill the source field
	_companies_ai[c->index].src.spec_tile = t->xy;
	_companies_ai[c->index].src.use_tile = 0;
	_companies_ai[c->index].src.rand_rng = 12;
	_companies_ai[c->index].src.cur_building_rule = 0xFF;
	_companies_ai[c->index].src.cargo = CT_PASSENGERS;

	// Fill the dest field
	_companies_ai[c->index].dst.spec_tile = in->xy;
	_companies_ai[c->index].dst.use_tile = 0;
	_companies_ai[c->index].dst.rand_rng = 5;
	_companies_ai[c->index].dst.cur_building_rule = 0xFF;
	_companies_ai[c->index].dst.cargo = CT_PASSENGERS;

	// Fill common fields
	_companies_ai[c->index].cargo_type = CT_PASSENGERS;
	_companies_ai[c->index].build_kind = 1;
	_companies_ai[c->index].num_build_rec = 2;
	_companies_ai[c->index].num_loco_to_build = 1;
	_companies_ai[c->index].num_want_fullload = 0;
//	_companies_ai[c->index].loco_id = INVALID_VEHICLE;
	_companies_ai[c->index].order_list_blocks[0] = 0;
	_companies_ai[c->index].order_list_blocks[1] = 1;
	_companies_ai[c->index].order_list_blocks[2] = 255;

	_companies_ai[c->index].state = AIS_AIRPORT_STUFF;
	_companies_ai[c->index].timeout_counter = 0;
}

static void AiWantAircraftRoute(Company *c)
{
	uint16 r = (uint16)Random();

	if (r >= 0x2AAA || _date < 0x3912 + DAYS_TILL_ORIGINAL_BASE_YEAR) {
		AiWantPassengerAircraftRoute(c);
	} else {
		AiWantOilRigAircraftRoute(c);
	}
}



static void AiStateWantNewRoute(Company *c)
{
	uint16 r;
	int i;

	if (c->money < AiGetBasePrice(c) * 500) {
		_companies_ai[c->index].state = AIS_0;
		return;
	}

	i = 200;
	for (;;) {
		r = (uint16)Random();

		if (_settings_game.ai.ai_disable_veh_train &&
				_settings_game.ai.ai_disable_veh_roadveh &&
				_settings_game.ai.ai_disable_veh_aircraft &&
				_settings_game.ai.ai_disable_veh_ship) {
			return;
		}

		if (r < 0x7626) {
			if (_settings_game.ai.ai_disable_veh_train) continue;
			AiWantTrainRoute(c);
		} else if (r < 0xC4EA) {
			if (_settings_game.ai.ai_disable_veh_roadveh) continue;
			AiWantRoadRoute(c);
		} else if (r < 0xD89B) {
			if (_settings_game.ai.ai_disable_veh_aircraft) continue;
			AiWantAircraftRoute(c);
		} else {
			/* Ships are not implemented in this (broken) AI */
		}

		// got a route?
		if (_companies_ai[c->index].state != AIS_WANT_NEW_ROUTE) break;

		// time out?
		if (--i == 0) {
			if (++_companies_ai[c->index].state_counter == 556) _companies_ai[c->index].state = AIS_0;
			break;
		}
	}
}

static bool AiCheckTrackResources(TileIndex tile, const AiDefaultBlockData *p, byte cargo)
{
	uint rad = (_settings_game.station.modified_catchment) ? CA_TRAIN : CA_UNMODIFIED;

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
		if (t != NULL && rating > t->ratings[_current_company]) {
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
	CompanyID old_company;
	uint32 r;
	Slope slope;
	uint h;

	old_company = _current_company;
	_current_company = OWNER_NONE;

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

	_current_company = old_company;
}

static void AiStateBuildDefaultRailBlocks(Company *c)
{
	uint i;
	int j;
	AiBuildRec *aib;
	int rule;
	CommandCost cost;

	// time out?
	if (++_companies_ai[c->index].timeout_counter == 1388) {
		_companies_ai[c->index].state = AIS_DELETE_RAIL_BLOCKS;
		return;
	}

	// do the following 8 times
	for (i = 0; i < 8; i++) {
		// check if we can build the default track
		aib = &_companies_ai[c->index].src;
		j = _companies_ai[c->index].num_build_rec;
		do {
			// this item has already been built?
			if (aib->cur_building_rule != 255) continue;

			// adjust the coordinate randomly,
			// to make sure that we find a position.
			aib->use_tile = AdjustTileCoordRandomly(aib->spec_tile, aib->rand_rng);

			// check if the track can be build there.
			rule = AiBuildDefaultRailTrack(aib->use_tile,
				_companies_ai[c->index].build_kind, _companies_ai[c->index].num_wagons,
				aib->unk6, aib->unk7,
				aib->direction, aib->cargo,
				_companies_ai[c->index].railtype_to_use,
				&cost
			);

			if (rule == -1) {
				// cannot build, terraform after a while
				if (_companies_ai[c->index].state_counter >= 600) {
					AiDoTerraformLand(aib->use_tile, (DiagDirection)(Random() & 3), 3, (int8)_companies_ai[c->index].state_mode);
				}
				// also try the other terraform direction
				if (++_companies_ai[c->index].state_counter >= 1000) {
					_companies_ai[c->index].state_counter = 0;
					_companies_ai[c->index].state_mode = -_companies_ai[c->index].state_mode;
				}
			} else if (CheckCompanyHasMoney(cost)) {
				// company has money, build it.
				aib->cur_building_rule = rule;

				AiDoBuildDefaultRailTrack(
					aib->use_tile,
					_default_rail_track_data[rule]->data,
					_companies_ai[c->index].railtype_to_use,
					DC_EXEC | DC_NO_TOWN_RATING
				);
			}
		} while (++aib, --j);
	}

	// check if we're done with all of them
	aib = &_companies_ai[c->index].src;
	j = _companies_ai[c->index].num_build_rec;
	do {
		if (aib->cur_building_rule == 255) return;
	} while (++aib, --j);

	// yep, all are done. switch state to the rail building state.
	_companies_ai[c->index].state = AIS_BUILD_RAIL;
	_companies_ai[c->index].state_mode = 255;
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

static bool AiDoFollowTrack(const Company *c)
{
	AiRailPathFindData arpfd;

	arpfd.tile = _companies_ai[c->index].start_tile_a;
	arpfd.tile2 = _companies_ai[c->index].cur_tile_a;
	arpfd.flag = false;
	arpfd.count = 0;
	FollowTrack(_companies_ai[c->index].cur_tile_a + TileOffsByDiagDir(_companies_ai[c->index].cur_dir_a), PATHFIND_FLAGS_NONE, TRANSPORT_RAIL, 0, ReverseDiagDir(_companies_ai[c->index].cur_dir_a),
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
	Company *company;
};

static const byte _ai_table_15[4][8] = {
	{0, 0, 4, 3, 3, 1, 128 + 0, 64},
	{1, 1, 2, 0, 4, 2, 128 + 1, 65},
	{0, 2, 2, 3, 5, 1, 128 + 2, 66},
	{1, 3, 5, 0, 3, 2, 128 + 3, 67}
};


static bool AiIsTileBanned(const Company *c, TileIndex tile, byte val)
{
	int i;

	for (i = 0; i != _companies_ai[c->index].banned_tile_count; i++) {
		if (_companies_ai[c->index].banned_tiles[i] == tile && _companies_ai[c->index].banned_val[i] == val) {
			return true;
		}
	}
	return false;
}

static void AiBanTile(Company *c, TileIndex tile, byte val)
{
	uint i;

	for (i = lengthof(_companies_ai[c->index].banned_tiles) - 1; i != 0; i--) {
		_companies_ai[c->index].banned_tiles[i] = _companies_ai[c->index].banned_tiles[i - 1];
		_companies_ai[c->index].banned_val[i] = _companies_ai[c->index].banned_val[i - 1];
	}

	_companies_ai[c->index].banned_tiles[0] = tile;
	_companies_ai[c->index].banned_val[0] = val;

	if (_companies_ai[c->index].banned_tile_count != lengthof(_companies_ai[c->index].banned_tiles)) {
		_companies_ai[c->index].banned_tile_count++;
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
	if (tileh == InclinedSlope(ReverseDiagDir(dir2)) || (tileh == SLOPE_FLAT && z != 0)) {
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
		if (CmdFailed(DoCommand(tile_new, tile, _companies_ai[arf->company->index].railtype_to_use << 8 | TRANSPORT_RAIL << 15, DC_AUTO, CMD_BUILD_BRIDGE))) {
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

	if (GetTileSlope(tile, &z) == InclinedSlope((DiagDirection)(p[0] & 3)) && z != 0) {
		CommandCost cost = DoCommand(tile, _companies_ai[arf->company->index].railtype_to_use, 0, DC_AUTO, CMD_BUILD_TUNNEL);

		if (CmdSucceeded(cost) && cost.GetCost() <= (arf->company->money >> 4)) {
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
			if (!AiIsTileBanned(arf->company, tile, p[0]) &&
					CmdSucceeded(DoCommand(tile, _companies_ai[arf->company->index].railtype_to_use, p[0], DC_AUTO | DC_NO_WATER | DC_NO_RAIL_OVERLAP, CMD_BUILD_SINGLE_RAIL))) {
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


static void AiBuildRailConstruct(Company *c)
{
	AiRailFinder arf;
	int i;

	// Check too much lookahead?
	if (AiDoFollowTrack(c)) {
		_companies_ai[c->index].state_counter = (Random()&0xE)+6; // Destruct this amount of blocks
		_companies_ai[c->index].state_mode = 1; // Start destruct

		// Ban this tile and don't reach it for a while.
		AiBanTile(c, _companies_ai[c->index].cur_tile_a, FindFirstBit(GetRailTrackStatus(_companies_ai[c->index].cur_tile_a)));
		return;
	}

	// Setup recursive finder and call it.
	arf.company = c;
	arf.final_tile = _companies_ai[c->index].cur_tile_b;
	arf.final_dir = _companies_ai[c->index].cur_dir_b;
	arf.depth = 0;
	arf.recursive_mode = 0;
	arf.best_ptr = NULL;
	arf.cur_best_dist = UINT_MAX;
	arf.cur_best_depth = 0xff;
	arf.best_dist = UINT_MAX;
	arf.best_depth = 0xff;
	arf.cur_best_tile = 0;
	arf.best_tile = 0;
	AiBuildRailRecursive(&arf, _companies_ai[c->index].cur_tile_a, _companies_ai[c->index].cur_dir_a);

	// Reached destination?
	if (arf.recursive_mode == 2 && arf.cur_best_depth == 0) {
		_companies_ai[c->index].state_mode = 255;
		return;
	}

	// Didn't find anything to build?
	if (arf.best_ptr == NULL) {
		// Terraform some
		for (i = 0; i != 5; i++) {
			AiDoTerraformLand(_companies_ai[c->index].cur_tile_a, _companies_ai[c->index].cur_dir_a, 3, 0);
		}

		if (++_companies_ai[c->index].state_counter == 21) {
			_companies_ai[c->index].state_counter = 40;
			_companies_ai[c->index].state_mode = 1;

			// Ban this tile
			AiBanTile(c, _companies_ai[c->index].cur_tile_a, FindFirstBit(GetRailTrackStatus(_companies_ai[c->index].cur_tile_a)));
		}
		return;
	}

	_companies_ai[c->index].cur_tile_a += TileOffsByDiagDir(_companies_ai[c->index].cur_dir_a);

	if (arf.best_ptr[0] & 0x80) {
		TileIndex t1 = _companies_ai[c->index].cur_tile_a;
		TileIndex t2 = arf.bridge_end_tile;

		int32 bridge_len = GetTunnelBridgeLength(t1, t2);

		DiagDirection dir = (TileX(t1) == TileX(t2) ? DIAGDIR_SE : DIAGDIR_SW);
		Track track = DiagDirToDiagTrack(dir);

		if (t2 < t1) dir = ReverseDiagDir(dir);

		/* try to build a long rail instead of bridge... */
		bool fail = false;
		CommandCost cost;
		TileIndex t = t1;

		/* try to build one rail on each tile - can't use CMD_BUILD_RAILROAD_TRACK now, it can build one part of track without failing */
		do {
			cost = DoCommand(t, _companies_ai[c->index].railtype_to_use, track, DC_AUTO | DC_NO_WATER, CMD_BUILD_SINGLE_RAIL);
			/* do not allow building over existing track */
			if (CmdFailed(cost) || IsTileType(t, MP_RAILWAY)) {
				fail = true;
				break;
			}
			t += TileOffsByDiagDir(dir);
		} while (t != t2);

		/* can we build long track? */
		if (!fail) cost = DoCommand(t1, t2, _companies_ai[c->index].railtype_to_use | (track << 4), DC_AUTO | DC_NO_WATER, CMD_BUILD_RAILROAD_TRACK);

		if (!fail && CmdSucceeded(cost) && cost.GetCost() <= c->money) {
			DoCommand(t1, t2, _companies_ai[c->index].railtype_to_use | (track << 4), DC_AUTO | DC_NO_WATER | DC_EXEC, CMD_BUILD_RAILROAD_TRACK);
		} else {

			/* Figure out which (rail)bridge type to build
			 * start with best bridge, then go down to worse and worse bridges
			 * unnecessary to check for worst bridge (i=0), since AI will always build that. */
			int i;
			for (i = MAX_BRIDGES - 1; i != 0; i--) {
				if (CheckBridge_Stuff(i, bridge_len)) {
					CommandCost cost = DoCommand(t1, t2, i | _companies_ai[c->index].railtype_to_use << 8 | TRANSPORT_RAIL << 15, DC_AUTO, CMD_BUILD_BRIDGE);
					if (CmdSucceeded(cost) && cost.GetCost() < (c->money >> 1) && cost.GetCost() < ((c->money + _economy.max_loan - c->current_loan) >> 5)) break;
				}
			}

			/* Build it */
			DoCommand(t1, t2, i | _companies_ai[c->index].railtype_to_use << 8 | TRANSPORT_RAIL << 15, DC_AUTO | DC_EXEC, CMD_BUILD_BRIDGE);
		}

		_companies_ai[c->index].cur_tile_a = t2;
		_companies_ai[c->index].state_counter = 0;
	} else if (arf.best_ptr[0] & 0x40) {
		// tunnel
		DoCommand(_companies_ai[c->index].cur_tile_a, _companies_ai[c->index].railtype_to_use, 0, DC_AUTO | DC_EXEC, CMD_BUILD_TUNNEL);
		_companies_ai[c->index].cur_tile_a = _build_tunnel_endtile;
		_companies_ai[c->index].state_counter = 0;
	} else {
		// rail
		_companies_ai[c->index].cur_dir_a = (DiagDirection)(arf.best_ptr[1] & 3);
		DoCommand(_companies_ai[c->index].cur_tile_a, _companies_ai[c->index].railtype_to_use, arf.best_ptr[0],
			DC_EXEC | DC_AUTO | DC_NO_WATER | DC_NO_RAIL_OVERLAP, CMD_BUILD_SINGLE_RAIL);
		_companies_ai[c->index].state_counter = 0;
	}

	if (arf.best_tile != 0) {
		for (i = 0; i != 2; i++) {
			AiDoTerraformLand(arf.best_tile, arf.best_dir, 3, 0);
		}
	}
}

static bool AiRemoveTileAndGoForward(Company *c)
{
	const byte *ptr;
	TileIndex tile = _companies_ai[c->index].cur_tile_a;
	TileIndex tilenew;

	if (IsTileType(tile, MP_TUNNELBRIDGE)) {
		if (IsTunnel(tile)) {
			// Clear the tunnel and continue at the other side of it.
			if (CmdFailed(DoCommand(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR)))
				return false;
			_companies_ai[c->index].cur_tile_a = TILE_MASK(_build_tunnel_endtile - TileOffsByDiagDir(_companies_ai[c->index].cur_dir_a));
			return true;
		} else { // IsBridge(tile)
			// Check if the bridge points in the right direction.
			// This is not really needed the first place AiRemoveTileAndGoForward is called.
			if (DiagDirToAxis(GetTunnelBridgeDirection(tile)) != (_companies_ai[c->index].cur_dir_a & 1)) return false;

			tile = GetOtherBridgeEnd(tile);

			tilenew = TILE_MASK(tile - TileOffsByDiagDir(_companies_ai[c->index].cur_dir_a));
			// And clear the bridge.
			if (CmdFailed(DoCommand(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR)))
				return false;
			_companies_ai[c->index].cur_tile_a = tilenew;
			return true;
		}
	}

	// Find the railtype at the position. Quit if no rail there.
	TrackBits bits = GetRailTrackStatus(tile) & DiagdirReachesTracks(ReverseDiagDir(_companies_ai[c->index].cur_dir_a));
	if (bits == TRACK_BIT_NONE) return false;

	// Convert into a bit position that CMD_REMOVE_SINGLE_RAIL expects.
	Track track = FindFirstTrack(bits);

	// Then remove and signals if there are any.
	if (IsTileType(tile, MP_RAILWAY) &&
			GetRailTileType(tile) == RAIL_TILE_SIGNALS) {
		DoCommand(tile, 0, 0, DC_EXEC, CMD_REMOVE_SIGNALS);
	}

	// And also remove the rail.
	if (CmdFailed(DoCommand(tile, 0, track, DC_EXEC, CMD_REMOVE_SINGLE_RAIL)))
		return false;

	// Find the direction at the other edge of the rail.
	ptr = _ai_table_15[ReverseDiagDir(_companies_ai[c->index].cur_dir_a)];
	while (ptr[0] != track) ptr += 2;
	_companies_ai[c->index].cur_dir_a = ReverseDiagDir((DiagDirection)ptr[1]);

	// And then also switch tile.
	_companies_ai[c->index].cur_tile_a = TILE_MASK(_companies_ai[c->index].cur_tile_a - TileOffsByDiagDir(_companies_ai[c->index].cur_dir_a));

	return true;
}


static void AiBuildRailDestruct(Company *c)
{
	// Decrease timeout.
	if (!--_companies_ai[c->index].state_counter) {
		_companies_ai[c->index].state_mode = 2;
		_companies_ai[c->index].state_counter = 0;
	}

	// Don't do anything if the destination is already reached.
	if (_companies_ai[c->index].cur_tile_a == _companies_ai[c->index].start_tile_a) return;

	AiRemoveTileAndGoForward(c);
}


static void AiBuildRail(Company *c)
{
	switch (_companies_ai[c->index].state_mode) {
		case 0: // Construct mode, build new rail.
			AiBuildRailConstruct(c);
			break;

		case 1: // Destruct mode, destroy the rail currently built.
			AiBuildRailDestruct(c);
			break;

		case 2: {
			uint i;

			// Terraform some and then try building again.
			for (i = 0; i != 4; i++) {
				AiDoTerraformLand(_companies_ai[c->index].cur_tile_a, _companies_ai[c->index].cur_dir_a, 3, 0);
			}

			if (++_companies_ai[c->index].state_counter == 4) {
				_companies_ai[c->index].state_counter = 0;
				_companies_ai[c->index].state_mode = 0;
			}
		}

		default: break;
	}
}

static void AiStateBuildRail(Company *c)
{
	int num;
	AiBuildRec *aib;
	byte cmd;
	TileIndex tile;
	DiagDirection dir;

	// time out?
	if (++_companies_ai[c->index].timeout_counter == 1388) {
		_companies_ai[c->index].state = AIS_DELETE_RAIL_BLOCKS;
		return;
	}

	// Currently building a rail between two points?
	if (_companies_ai[c->index].state_mode != 255) {
		AiBuildRail(c);

		// Alternate between edges
		Swap(_companies_ai[c->index].start_tile_a, _companies_ai[c->index].start_tile_b);
		Swap(_companies_ai[c->index].cur_tile_a,   _companies_ai[c->index].cur_tile_b);
		Swap(_companies_ai[c->index].start_dir_a,  _companies_ai[c->index].start_dir_b);
		Swap(_companies_ai[c->index].cur_dir_a,    _companies_ai[c->index].cur_dir_b);
		return;
	}

	// Now, find two new points to build between
	num = _companies_ai[c->index].num_build_rec;
	aib = &_companies_ai[c->index].src;

	for (;;) {
		cmd = aib->buildcmd_a;
		aib->buildcmd_a = 255;
		if (cmd != 255) break;

		cmd = aib->buildcmd_b;
		aib->buildcmd_b = 255;
		if (cmd != 255) break;

		aib++;
		if (--num == 0) {
			_companies_ai[c->index].state = AIS_BUILD_RAIL_VEH;
			_companies_ai[c->index].state_counter = 0; // timeout
			return;
		}
	}

	// Find first edge to build from.
	tile = AiGetEdgeOfDefaultRailBlock(aib->cur_building_rule, aib->use_tile, cmd & 3, &dir);
	_companies_ai[c->index].start_tile_a = tile;
	_companies_ai[c->index].cur_tile_a = tile;
	_companies_ai[c->index].start_dir_a = dir;
	_companies_ai[c->index].cur_dir_a = dir;
	DoCommand(TILE_MASK(tile + TileOffsByDiagDir(dir)), 0, (dir & 1) ? 1 : 0, DC_EXEC, CMD_REMOVE_SINGLE_RAIL);

	assert(TILE_MASK(tile) != 0xFF00);

	// Find second edge to build to
	aib = (&_companies_ai[c->index].src) + ((cmd >> 4) & 0xF);
	tile = AiGetEdgeOfDefaultRailBlock(aib->cur_building_rule, aib->use_tile, (cmd >> 2) & 3, &dir);
	_companies_ai[c->index].start_tile_b = tile;
	_companies_ai[c->index].cur_tile_b = tile;
	_companies_ai[c->index].start_dir_b = dir;
	_companies_ai[c->index].cur_dir_b = dir;
	DoCommand(TILE_MASK(tile + TileOffsByDiagDir(dir)), 0, (dir & 1) ? 1 : 0, DC_EXEC, CMD_REMOVE_SINGLE_RAIL);

	assert(TILE_MASK(tile) != 0xFF00);

	// And setup state.
	_companies_ai[c->index].state_mode = 2;
	_companies_ai[c->index].state_counter = 0;
	_companies_ai[c->index].banned_tile_count = 0;
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
	uint16 best_capacity = 0;
	uint16 best_speed    = 0;
	uint speed;
	const Engine *e;

	FOR_ALL_ENGINES_OF_TYPE(e, VEH_TRAIN) {
		EngineID i = e->index;
		const RailVehicleInfo *rvi = &e->u.rail;

		if (!IsCompatibleRail(rvi->railtype, railtype) ||
				rvi->railveh_type != RAILVEH_WAGON ||
				!HasBit(e->company_avail, _current_company)) {
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

static void AiStateBuildRailVeh(Company *c)
{
	const AiDefaultBlockData *ptr;
	TileIndex tile;
	EngineID veh;
	int i;
	CargoID cargo;
	CommandCost cost;
	Vehicle *v;
	VehicleID loco_id;

	ptr = _default_rail_track_data[_companies_ai[c->index].src.cur_building_rule]->data;
	while (ptr->mode != 0) ptr++;

	tile = TILE_ADD(_companies_ai[c->index].src.use_tile, ToTileIndexDiff(ptr->tileoffs));


	cargo = _companies_ai[c->index].cargo_type;
	for (i = 0;;) {
		if (_companies_ai[c->index].wagon_list[i] == INVALID_VEHICLE) {
			veh = AiFindBestWagon(cargo, _companies_ai[c->index].railtype_to_use);
			/* veh will return INVALID_ENGINE if no suitable wagon is available.
			 * We shall treat this in the same way as having no money */
			if (veh == INVALID_ENGINE) goto handle_nocash;
			cost = DoCommand(tile, veh, 0, DC_EXEC, CMD_BUILD_RAIL_VEHICLE);
			if (CmdFailed(cost)) goto handle_nocash;
			_companies_ai[c->index].wagon_list[i] = _new_vehicle_id;
			_companies_ai[c->index].wagon_list[i + 1] = INVALID_VEHICLE;
			return;
		}
		if (cargo == CT_MAIL) cargo = CT_PASSENGERS;
		if (++i == _companies_ai[c->index].num_wagons * 2 - 1) break;
	}

	// Which locomotive to build?
	veh = AiChooseTrainToBuild(_companies_ai[c->index].railtype_to_use, c->money, cargo != CT_PASSENGERS ? 1 : 0, tile);
	if (veh == INVALID_ENGINE) {
handle_nocash:
		// after a while, if AI still doesn't have cash, get out of this block by selling the wagons.
		if (++_companies_ai[c->index].state_counter == 1000) {
			for (i = 0; _companies_ai[c->index].wagon_list[i] != INVALID_VEHICLE; i++) {
				cost = DoCommand(tile, _companies_ai[c->index].wagon_list[i], 0, DC_EXEC, CMD_SELL_RAIL_WAGON);
				assert(CmdSucceeded(cost));
			}
			_companies_ai[c->index].state = AIS_0;
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
		i = _companies_ai[c->index].wagon_list[_companies_ai[c->index].num_wagons * 2 - 2];
		_companies_ai[c->index].wagon_list[_companies_ai[c->index].num_wagons * 2 - 2] = INVALID_VEHICLE;
		DoCommand(tile, i, 0, DC_EXEC, CMD_SELL_RAIL_WAGON);
	}

	// Move the wagons onto the train
	for (i = 0; _companies_ai[c->index].wagon_list[i] != INVALID_VEHICLE; i++) {
		DoCommand(tile, _companies_ai[c->index].wagon_list[i] | (loco_id << 16), 0, DC_EXEC, CMD_MOVE_RAIL_VEHICLE);
	}

	for (i = 0; _companies_ai[c->index].order_list_blocks[i] != 0xFF; i++) {
		const AiBuildRec* aib = &_companies_ai[c->index].src + _companies_ai[c->index].order_list_blocks[i];
		bool is_pass = (
			_companies_ai[c->index].cargo_type == CT_PASSENGERS ||
			_companies_ai[c->index].cargo_type == CT_MAIL ||
			(_settings_game.game_creation.landscape == LT_TEMPERATE && _companies_ai[c->index].cargo_type == CT_VALUABLES)
		);
		Order order;

		order.MakeGoToStation(AiGetStationIdByDef(aib->use_tile, aib->cur_building_rule));

		if (!is_pass && i == 1) order.SetUnloadType(OUFB_UNLOAD);
		if (_companies_ai[c->index].num_want_fullload != 0 && (is_pass || i == 0))
			order.SetLoadType(OLFB_FULL_LOAD);

		DoCommand(0, loco_id + (i << 16), order.Pack(), DC_EXEC, CMD_INSERT_ORDER);
	}

	DoCommand(0, loco_id, 0, DC_EXEC, CMD_START_STOP_VEHICLE);

	DoCommand(0, loco_id, _ai_service_interval, DC_EXEC, CMD_CHANGE_SERVICE_INT);

	if (_companies_ai[c->index].num_want_fullload != 0) _companies_ai[c->index].num_want_fullload--;

	if (--_companies_ai[c->index].num_loco_to_build != 0) {
//		_companies_ai[c->index].loco_id = INVALID_VEHICLE;
		_companies_ai[c->index].wagon_list[0] = INVALID_VEHICLE;
	} else {
		_companies_ai[c->index].state = AIS_0;
	}
}

static void AiStateDeleteRailBlocks(Company *c)
{
	const AiBuildRec* aib = &_companies_ai[c->index].src;
	uint num = _companies_ai[c->index].num_build_rec;

	do {
		const AiDefaultBlockData* b;

		if (aib->cur_building_rule == 255) continue;
		for (b = _default_rail_track_data[aib->cur_building_rule]->data; b->mode != 4; b++) {
			DoCommand(TILE_ADD(aib->use_tile, ToTileIndexDiff(b->tileoffs)), 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
		}
	} while (++aib, --num);

	_companies_ai[c->index].state = AIS_0;
}

static bool AiCheckRoadResources(TileIndex tile, const AiDefaultBlockData *p, byte cargo)
{
	uint values[NUM_CARGO];
	int rad;

	if (_settings_game.station.modified_catchment) {
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
		if (t != NULL && rating > t->ratings[_current_company]) return CMD_ERROR;
	}
	return total_cost;
}

// Make sure the blocks are not too close to each other
static bool AiCheckBlockDistances(Company *c, TileIndex tile)
{
	const AiBuildRec* aib = &_companies_ai[c->index].src;
	uint num = _companies_ai[c->index].num_build_rec;

	do {
		if (aib->cur_building_rule != 255) {
			if (DistanceManhattan(aib->use_tile, tile) < 9) return false;
		}
	} while (++aib, --num);

	return true;
}


static void AiStateBuildDefaultRoadBlocks(Company *c)
{
	uint i;
	int j;
	AiBuildRec *aib;
	int rule;
	CommandCost cost;

	// time out?
	if (++_companies_ai[c->index].timeout_counter == 1388) {
		_companies_ai[c->index].state = AIS_DELETE_RAIL_BLOCKS;
		return;
	}

	// do the following 8 times
	for (i = 0; i != 8; i++) {
		// check if we can build the default track
		aib = &_companies_ai[c->index].src;
		j = _companies_ai[c->index].num_build_rec;
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
				if (_companies_ai[c->index].state_counter >= 600) {
					AiDoTerraformLand(aib->use_tile, (DiagDirection)(Random() & 3), 3, (int8)_companies_ai[c->index].state_mode);
				}
				// also try the other terraform direction
				if (++_companies_ai[c->index].state_counter >= 1000) {
					_companies_ai[c->index].state_counter = 0;
					_companies_ai[c->index].state_mode = -_companies_ai[c->index].state_mode;
				}
			} else if (CheckCompanyHasMoney(cost) && AiCheckBlockDistances(c, aib->use_tile)) {
				CommandCost r;

				// company has money, build it.
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
	aib = &_companies_ai[c->index].src;
	j = _companies_ai[c->index].num_build_rec;
	do {
		if (aib->cur_building_rule == 255) return;
	} while (++aib, --j);

	// yep, all are done. switch state to the rail building state.
	_companies_ai[c->index].state = AIS_BUILD_ROAD;
	_companies_ai[c->index].state_mode = 255;
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
	Company *company;
};

struct AiRoadEnum {
	TileIndex dest;
	TileIndex best_tile;
	Trackdir best_track;
	uint best_dist;
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
	arf->cur_best_dist = UINT_MAX;
	arf->cur_best_depth = 0xff;

	return better;
}


static bool AiEnumFollowRoad(TileIndex tile, AiRoadEnum *a, Trackdir track, uint length)
{
	uint dist = DistanceManhattan(tile, a->dest);

	if (dist <= a->best_dist) {
		TileIndex tile2 = TILE_MASK(tile + TileOffsByDiagDir(TrackdirToExitdir(track)));

		if (IsNormalRoadTile(tile2)) {
			a->best_dist = dist;
			a->best_tile = tile;
			a->best_track = track;
		}
	}

	return false;
}

static bool AiCheckRoadFinished(Company *c)
{
	AiRoadEnum are;
	TileIndex tile;
	DiagDirection dir = _companies_ai[c->index].cur_dir_a;

	are.dest = _companies_ai[c->index].cur_tile_b;
	tile = TILE_MASK(_companies_ai[c->index].cur_tile_a + TileOffsByDiagDir(dir));

	if (IsRoadStopTile(tile) || IsRoadDepotTile(tile)) return false;
	TrackdirBits bits = TrackStatusToTrackdirBits(GetTileTrackStatus(tile, TRANSPORT_ROAD, ROADTYPES_ROAD)) & DiagdirReachesTrackdirs(dir);
	if (bits == TRACKDIR_BIT_NONE) return false;

	are.best_dist = UINT_MAX;

	while (bits != TRACKDIR_BIT_NONE) {
		Trackdir trackdir = RemoveFirstTrackdir(&bits);
		FollowTrack(tile, PATHFIND_FLAGS_DISABLE_TILE_HASH, TRANSPORT_ROAD, ROADTYPES_ROAD, TrackdirToExitdir(trackdir), (TPFEnumProc*)AiEnumFollowRoad, NULL, &are);
	}

	if (DistanceManhattan(tile, are.dest) <= are.best_dist) return false;

	if (are.best_dist == 0) return true;

	_companies_ai[c->index].cur_tile_a = are.best_tile;
	_companies_ai[c->index].cur_dir_a = TrackdirToExitdir(are.best_track);
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
	if (tileh == InclinedSlope(ReverseDiagDir(dir2)) || (tileh == SLOPE_FLAT && z != 0)) {
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
		if (CmdFailed(DoCommand(tile_new, tile, ROADTYPES_ROAD << 8 | TRANSPORT_ROAD << 15, DC_AUTO, CMD_BUILD_BRIDGE)))
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

	if (GetTileSlope(tile, &z) == InclinedSlope((DiagDirection)(p[0] & 3)) && z != 0) {
		CommandCost cost = DoCommand(tile, 0x200, 0, DC_AUTO, CMD_BUILD_TUNNEL);

		if (CmdSucceeded(cost) && cost.GetCost() <= (arf->company->money >> 4)) {
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


static void AiBuildRoadConstruct(Company *c)
{
	AiRoadFinder arf;
	int i;
	TileIndex tile;

	// Reached destination?
	if (AiCheckRoadFinished(c)) {
		_companies_ai[c->index].state_mode = 255;
		return;
	}

	// Setup recursive finder and call it.
	arf.company = c;
	arf.final_tile = _companies_ai[c->index].cur_tile_b;
	arf.final_dir = _companies_ai[c->index].cur_dir_b;
	arf.depth = 0;
	arf.recursive_mode = 0;
	arf.best_ptr = NULL;
	arf.cur_best_dist = UINT_MAX;
	arf.cur_best_depth = 0xff;
	arf.best_dist = UINT_MAX;
	arf.best_depth =  0xff;
	arf.cur_best_tile = 0;
	arf.best_tile = 0;
	AiBuildRoadRecursive(&arf, _companies_ai[c->index].cur_tile_a, _companies_ai[c->index].cur_dir_a);

	// Reached destination?
	if (arf.recursive_mode == 2 && arf.cur_best_depth == 0) {
		_companies_ai[c->index].state_mode = 255;
		return;
	}

	// Didn't find anything to build?
	if (arf.best_ptr == NULL) {
		// Terraform some
do_some_terraform:
		for (i = 0; i != 5; i++)
			AiDoTerraformLand(_companies_ai[c->index].cur_tile_a, _companies_ai[c->index].cur_dir_a, 3, 0);

		if (++_companies_ai[c->index].state_counter == 21) {
			_companies_ai[c->index].state_mode = 1;

			_companies_ai[c->index].cur_tile_a = TILE_MASK(_companies_ai[c->index].cur_tile_a + TileOffsByDiagDir(_companies_ai[c->index].cur_dir_a));
			_companies_ai[c->index].cur_dir_a = ReverseDiagDir(_companies_ai[c->index].cur_dir_a);
			_companies_ai[c->index].state_counter = 0;
		}
		return;
	}

	tile = TILE_MASK(_companies_ai[c->index].cur_tile_a + TileOffsByDiagDir(_companies_ai[c->index].cur_dir_a));

	if (arf.best_ptr[0] & 0x80) {
		TileIndex t1 = tile;
		TileIndex t2 = arf.bridge_end_tile;

		int32 bridge_len = GetTunnelBridgeLength(t1, t2);

		Axis axis = (TileX(t1) == TileX(t2) ? AXIS_Y : AXIS_X);

		/* try to build a long road instead of bridge - CMD_BUILD_LONG_ROAD has to fail if it couldn't build at least one piece! */
		CommandCost cost = DoCommand(t2, t1, (t2 < t1 ? 1 : 2) | (axis << 2) | (ROADTYPE_ROAD << 3), DC_AUTO | DC_NO_WATER, CMD_BUILD_LONG_ROAD);

		if (CmdSucceeded(cost) && cost.GetCost() <= c->money) {
			DoCommand(t2, t1, (t2 < t1 ? 1 : 2) | (axis << 2) | (ROADTYPE_ROAD << 3), DC_AUTO | DC_EXEC | DC_NO_WATER, CMD_BUILD_LONG_ROAD);
		} else {
			int i;

			/* Figure out what (road)bridge type to build
			 * start with best bridge, then go down to worse and worse bridges
			 * unnecessary to check for worse bridge (i=0), since AI will always build that */
			for (i = MAX_BRIDGES - 1; i != 0; i--) {
				if (CheckBridge_Stuff(i, bridge_len)) {
					CommandCost cost = DoCommand(t1, t2, i | ROADTYPES_ROAD << 8 | TRANSPORT_ROAD << 15, DC_AUTO, CMD_BUILD_BRIDGE);
					if (CmdSucceeded(cost) && cost.GetCost() < (c->money >> 1) && cost.GetCost() < ((c->money + _economy.max_loan - c->current_loan) >> 5)) break;
				}
			}

			/* Build it */
			DoCommand(t1, t2, i | ROADTYPES_ROAD << 8 | TRANSPORT_ROAD << 15, DC_AUTO | DC_EXEC, CMD_BUILD_BRIDGE);
		}

		_companies_ai[c->index].cur_tile_a = t2;
		_companies_ai[c->index].state_counter = 0;
	} else if (arf.best_ptr[0] & 0x40) {
		// tunnel
		DoCommand(tile, 0x200, 0, DC_AUTO | DC_EXEC, CMD_BUILD_TUNNEL);
		_companies_ai[c->index].cur_tile_a = _build_tunnel_endtile;
		_companies_ai[c->index].state_counter = 0;
	} else {
		// road
		if (!AiBuildRoadHelper(tile, DC_EXEC | DC_AUTO | DC_NO_WATER | DC_AI_BUILDING, arf.best_ptr[0]))
			goto do_some_terraform;

		_companies_ai[c->index].cur_dir_a = (DiagDirection)(arf.best_ptr[1] & 3);
		_companies_ai[c->index].cur_tile_a = tile;
		_companies_ai[c->index].state_counter = 0;
	}

	if (arf.best_tile != 0) {
		for (i = 0; i != 2; i++)
			AiDoTerraformLand(arf.best_tile, arf.best_dir, 3, 0);
	}
}


static void AiBuildRoad(Company *c)
{
	if (_companies_ai[c->index].state_mode < 1) {
		// Construct mode, build new road.
		AiBuildRoadConstruct(c);
	} else if (_companies_ai[c->index].state_mode == 1) {
		// Destruct mode, not implemented for roads.
		_companies_ai[c->index].state_mode = 2;
		_companies_ai[c->index].state_counter = 0;
	} else if (_companies_ai[c->index].state_mode == 2) {
		uint i;

		// Terraform some and then try building again.
		for (i = 0; i != 4; i++) {
			AiDoTerraformLand(_companies_ai[c->index].cur_tile_a, _companies_ai[c->index].cur_dir_a, 3, 0);
		}

		if (++_companies_ai[c->index].state_counter == 4) {
			_companies_ai[c->index].state_counter = 0;
			_companies_ai[c->index].state_mode = 0;
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


static void AiStateBuildRoad(Company *c)
{
	int num;
	AiBuildRec *aib;
	byte cmd;
	TileIndex tile;
	DiagDirection dir;

	// time out?
	if (++_companies_ai[c->index].timeout_counter == 1388) {
		_companies_ai[c->index].state = AIS_DELETE_ROAD_BLOCKS;
		return;
	}

	// Currently building a road between two points?
	if (_companies_ai[c->index].state_mode != 255) {
		AiBuildRoad(c);

		// Alternate between edges
		Swap(_companies_ai[c->index].start_tile_a, _companies_ai[c->index].start_tile_b);
		Swap(_companies_ai[c->index].cur_tile_a,   _companies_ai[c->index].cur_tile_b);
		Swap(_companies_ai[c->index].start_dir_a,  _companies_ai[c->index].start_dir_b);
		Swap(_companies_ai[c->index].cur_dir_a,    _companies_ai[c->index].cur_dir_b);

		return;
	}

	// Now, find two new points to build between
	num = _companies_ai[c->index].num_build_rec;
	aib = &_companies_ai[c->index].src;

	for (;;) {
		cmd = aib->buildcmd_a;
		aib->buildcmd_a = 255;
		if (cmd != 255) break;

		aib++;
		if (--num == 0) {
			_companies_ai[c->index].state = AIS_BUILD_ROAD_VEHICLES;
			return;
		}
	}

	// Find first edge to build from.
	tile = AiGetRoadBlockEdge(aib->cur_building_rule, aib->use_tile, &dir);
	_companies_ai[c->index].start_tile_a = tile;
	_companies_ai[c->index].cur_tile_a = tile;
	_companies_ai[c->index].start_dir_a = dir;
	_companies_ai[c->index].cur_dir_a = dir;

	// Find second edge to build to
	aib = (&_companies_ai[c->index].src) + (cmd & 0xF);
	tile = AiGetRoadBlockEdge(aib->cur_building_rule, aib->use_tile, &dir);
	_companies_ai[c->index].start_tile_b = tile;
	_companies_ai[c->index].cur_tile_b = tile;
	_companies_ai[c->index].start_dir_b = dir;
	_companies_ai[c->index].cur_dir_b = dir;

	// And setup state.
	_companies_ai[c->index].state_mode = 2;
	_companies_ai[c->index].state_counter = 0;
	_companies_ai[c->index].banned_tile_count = 0;
}

static StationID AiGetStationIdFromRoadBlock(TileIndex tile, int id)
{
	const AiDefaultBlockData *p = _road_default_block_data[id]->data;
	while (p->mode != 1) p++;
	return GetStationIndex(TILE_ADD(tile, ToTileIndexDiff(p->tileoffs)));
}

static void AiStateBuildRoadVehicles(Company *c)
{
	const AiDefaultBlockData *ptr;
	TileIndex tile;
	VehicleID loco_id;
	EngineID veh;
	uint i;

	ptr = _road_default_block_data[_companies_ai[c->index].src.cur_building_rule]->data;
	for (; ptr->mode != 0; ptr++) {}
	tile = TILE_ADD(_companies_ai[c->index].src.use_tile, ToTileIndexDiff(ptr->tileoffs));

	veh = AiChooseRoadVehToBuild(_companies_ai[c->index].cargo_type, c->money, tile);
	if (veh == INVALID_ENGINE) {
		_companies_ai[c->index].state = AIS_0;
		return;
	}

	if (CmdFailed(DoCommand(tile, veh, 0, DC_EXEC, CMD_BUILD_ROAD_VEH))) return;

	loco_id = _new_vehicle_id;

	if (GetVehicle(loco_id)->cargo_type != _companies_ai[c->index].cargo_type) {
		/* Cargo type doesn't match, so refit it */
		if (CmdFailed(DoCommand(tile, loco_id, _companies_ai[c->index].cargo_type, DC_EXEC, CMD_REFIT_ROAD_VEH))) {
			/* Refit failed... sell the vehicle */
			DoCommand(tile, loco_id, 0, DC_EXEC, CMD_SELL_ROAD_VEH);
			return;
		}
	}

	for (i = 0; _companies_ai[c->index].order_list_blocks[i] != 0xFF; i++) {
		const AiBuildRec* aib = &_companies_ai[c->index].src + _companies_ai[c->index].order_list_blocks[i];
		bool is_pass = (
			_companies_ai[c->index].cargo_type == CT_PASSENGERS ||
			_companies_ai[c->index].cargo_type == CT_MAIL ||
			(_settings_game.game_creation.landscape == LT_TEMPERATE && _companies_ai[c->index].cargo_type == CT_VALUABLES)
		);
		Order order;

		order.MakeGoToStation(AiGetStationIdFromRoadBlock(aib->use_tile, aib->cur_building_rule));

		if (!is_pass && i == 1) order.SetUnloadType(OUFB_UNLOAD);
		if (_companies_ai[c->index].num_want_fullload != 0 && (is_pass || i == 0))
			order.SetLoadType(OLFB_FULL_LOAD);

		DoCommand(0, loco_id + (i << 16), order.Pack(), DC_EXEC, CMD_INSERT_ORDER);
	}

	DoCommand(0, loco_id, 0, DC_EXEC, CMD_START_STOP_VEHICLE);
	DoCommand(0, loco_id, _ai_service_interval, DC_EXEC, CMD_CHANGE_SERVICE_INT);

	if (_companies_ai[c->index].num_want_fullload != 0) _companies_ai[c->index].num_want_fullload--;
	if (--_companies_ai[c->index].num_loco_to_build == 0) _companies_ai[c->index].state = AIS_0;
}

static void AiStateDeleteRoadBlocks(Company *c)
{
	const AiBuildRec* aib = &_companies_ai[c->index].src;
	uint num = _companies_ai[c->index].num_build_rec;

	do {
		const AiDefaultBlockData* b;

		if (aib->cur_building_rule == 255) continue;
		for (b = _road_default_block_data[aib->cur_building_rule]->data; b->mode != 4; b++) {
			if (b->mode > 1) continue;
			DoCommand(TILE_ADD(aib->use_tile, ToTileIndexDiff(b->tileoffs)), 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
		}
	} while (++aib, --num);

	_companies_ai[c->index].state = AIS_0;
}


static void AiStateAirportStuff(Company *c)
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
		aib = &_companies_ai[c->index].src + i;

		FOR_ALL_STATIONS(st) {
			// Is this an airport?
			if (!(st->facilities & FACIL_AIRPORT)) continue;

			// Do we own the airport? (Oilrigs aren't owned, though.)
			if (st->owner != OWNER_NONE && st->owner != _current_company) continue;

			AirportFTAClass::Flags flags = st->Airport()->flags;

			/* if airport doesn't accept our kind of plane, dismiss it */
			if (!(flags & (_companies_ai[c->index].build_kind == 1 ? AirportFTAClass::HELICOPTERS : AirportFTAClass::AIRPLANES))) {
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
	} while (++i != _companies_ai[c->index].num_build_rec);

	_companies_ai[c->index].state = AIS_BUILD_DEFAULT_AIRPORT_BLOCKS;
	_companies_ai[c->index].state_mode = 255;
	_companies_ai[c->index].state_counter = 0;
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
		uint rad = _settings_game.station.modified_catchment ? airport->catchment : (uint)CA_UNMODIFIED;

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

	bool no_small = false;

	if (!heli) {
		/* do not build small airport if we have large available and we are not building heli route */
		uint valid = GetValidAirports();
		for (uint i = 0; (p = _airport_default_block_data[i]) != NULL; i++) {
			uint flags = GetAirport(p->attr)->flags;
			if (HasBit(valid, p->attr) && (flags & AirportFTAClass::AIRPLANES) && !(flags & AirportFTAClass::SHORT_STRIP)) {
				no_small = true;
				break;
			}
		}
	}

	for (uint i = 0; (p = _airport_default_block_data[i]) != NULL; i++) {
		uint flags = GetAirport(p->attr)->flags;
		/* If we are doing a helicopter service, avoid building airports where they can't land */
		if (heli && !(flags & AirportFTAClass::HELICOPTERS)) continue;
		/* Similiar with aircraft ... */
		if (!heli && !(flags & AirportFTAClass::AIRPLANES)) continue;
		/* Do not build small airport if we prefer large */
		if (no_small && (flags & AirportFTAClass::SHORT_STRIP)) continue;

		*cost = AiDoBuildDefaultAirportBlock(tile, p, 0);
		if (CmdSucceeded(*cost) && AiCheckAirportResources(tile, p, cargo))
			return i;
	}
	return -1;
}

static void AiStateBuildDefaultAirportBlocks(Company *c)
{
	int i, j;
	AiBuildRec *aib;
	int rule;
	CommandCost cost;

	// time out?
	if (++_companies_ai[c->index].timeout_counter == 1388) {
		_companies_ai[c->index].state = AIS_0;
		return;
	}

	// do the following 8 times
	i = 8;
	do {
		// check if we can build the default
		aib = &_companies_ai[c->index].src;
		j = _companies_ai[c->index].num_build_rec;
		do {
			// this item has already been built?
			if (aib->cur_building_rule != 255) continue;

			// adjust the coordinate randomly,
			// to make sure that we find a position.
			aib->use_tile = AdjustTileCoordRandomly(aib->spec_tile, aib->rand_rng);

			// check if the aircraft stuff can be built there.
			rule = AiFindBestDefaultAirportBlock(aib->use_tile, aib->cargo, _companies_ai[c->index].build_kind, &cost);

//			SetRedErrorSquare(aib->use_tile);

			if (rule == -1) {
				// cannot build, terraform after a while
				if (_companies_ai[c->index].state_counter >= 600) {
					AiDoTerraformLand(aib->use_tile, (DiagDirection)(Random() & 3), 3, (int8)_companies_ai[c->index].state_mode);
				}
				// also try the other terraform direction
				if (++_companies_ai[c->index].state_counter >= 1000) {
					_companies_ai[c->index].state_counter = 0;
					_companies_ai[c->index].state_mode = -_companies_ai[c->index].state_mode;
				}
			} else if (CheckCompanyHasMoney(cost) && AiCheckBlockDistances(c, aib->use_tile)) {
				// company has money, build it.
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
	aib = &_companies_ai[c->index].src;
	j = _companies_ai[c->index].num_build_rec;
	do {
		if (aib->cur_building_rule == 255) return;
	} while (++aib, --j);

	// yep, all are done. switch state.
	_companies_ai[c->index].state = AIS_BUILD_AIRCRAFT_VEHICLES;
}

static StationID AiGetStationIdFromAircraftBlock(TileIndex tile, int id)
{
	const AiDefaultBlockData *p = _airport_default_block_data[id];
	while (p->mode != 1) p++;
	return GetStationIndex(TILE_ADD(tile, ToTileIndexDiff(p->tileoffs)));
}

static void AiStateBuildAircraftVehicles(Company *c)
{
	const AiDefaultBlockData *ptr;
	TileIndex tile;
	EngineID veh;
	int i;
	VehicleID loco_id;

	ptr = _airport_default_block_data[_companies_ai[c->index].src.cur_building_rule];
	for (; ptr->mode != 0; ptr++) {}

	tile = TILE_ADD(_companies_ai[c->index].src.use_tile, ToTileIndexDiff(ptr->tileoffs));

	/* determine forbidden aircraft bits */
	byte forbidden = 0;
	for (i = 0; _companies_ai[c->index].order_list_blocks[i] != 0xFF; i++) {
		const AiBuildRec *aib = (&_companies_ai[c->index].src) + _companies_ai[c->index].order_list_blocks[i];
		const Station *st = GetStationByTile(aib->use_tile);

		if (st == NULL || !(st->facilities & FACIL_AIRPORT)) continue;

		AirportFTAClass::Flags flags = st->Airport()->flags;
		if (!(flags & AirportFTAClass::AIRPLANES)) forbidden |= AIR_CTOL | AIR_FAST; // no planes for heliports / oil rigs
		if (flags & AirportFTAClass::SHORT_STRIP) forbidden |= AIR_FAST; // no fast planes for small airports
	}

	veh = AiChooseAircraftToBuild(c->money, forbidden);
	if (veh == INVALID_ENGINE) return;
	if (GetStationByTile(tile)->Airport()->nof_depots == 0) return;

	/* XXX - Have the AI pick the hangar terminal in an airport. Eg get airport-type
	 * and offset to the FIRST depot because the AI picks the st->xy tile */
	tile += ToTileIndexDiff(GetStationByTile(tile)->Airport()->airport_depots[0]);
	if (CmdFailed(DoCommand(tile, veh, 0, DC_EXEC, CMD_BUILD_AIRCRAFT))) return;
	loco_id = _new_vehicle_id;

	for (i = 0; _companies_ai[c->index].order_list_blocks[i] != 0xFF; i++) {
		AiBuildRec *aib = (&_companies_ai[c->index].src) + _companies_ai[c->index].order_list_blocks[i];
		bool is_pass = (_companies_ai[c->index].cargo_type == CT_PASSENGERS || _companies_ai[c->index].cargo_type == CT_MAIL);
		Order order;

		order.MakeGoToStation(AiGetStationIdFromAircraftBlock(aib->use_tile, aib->cur_building_rule));

		if (!is_pass && i == 1) order.SetUnloadType(OUFB_UNLOAD);
		if (_companies_ai[c->index].num_want_fullload != 0 && (is_pass || i == 0))
			order.SetLoadType(OLFB_FULL_LOAD);

		DoCommand(0, loco_id + (i << 16), order.Pack(), DC_EXEC, CMD_INSERT_ORDER);
	}

	DoCommand(0, loco_id, 0, DC_EXEC, CMD_START_STOP_VEHICLE);

	DoCommand(0, loco_id, _ai_service_interval, DC_EXEC, CMD_CHANGE_SERVICE_INT);

	if (_companies_ai[c->index].num_want_fullload != 0) _companies_ai[c->index].num_want_fullload--;

	if (--_companies_ai[c->index].num_loco_to_build == 0) _companies_ai[c->index].state = AIS_0;
}

static void AiStateCheckShipStuff(Company *c)
{
	/* Ships are not implemented in this (broken) AI */
}

static void AiStateBuildDefaultShipBlocks(Company *c)
{
	/* Ships are not implemented in this (broken) AI */
}

static void AiStateDoShipStuff(Company *c)
{
	/* Ships are not implemented in this (broken) AI */
}

static void AiStateSellVeh(Company *c)
{
	Vehicle *v = _companies_ai[c->index].cur_veh;

	if (v->owner == _current_company) {
		if (v->type == VEH_TRAIN) {

			if (!IsRailDepotTile(v->tile) || v->u.rail.track != TRACK_BIT_DEPOT || !(v->vehstatus & VS_STOPPED)) {
				if (!v->current_order.IsType(OT_GOTO_DEPOT))
					DoCommand(0, v->index, 0, DC_EXEC, CMD_SEND_TRAIN_TO_DEPOT);
				goto going_to_depot;
			}

			// Sell whole train
			DoCommand(v->tile, v->index, 1, DC_EXEC, CMD_SELL_RAIL_WAGON);

		} else if (v->type == VEH_ROAD) {
			if (!v->IsStoppedInDepot()) {
				if (!v->current_order.IsType(OT_GOTO_DEPOT))
					DoCommand(0, v->index, 0, DC_EXEC, CMD_SEND_ROADVEH_TO_DEPOT);
				goto going_to_depot;
			}

			DoCommand(0, v->index, 0, DC_EXEC, CMD_SELL_ROAD_VEH);
		} else if (v->type == VEH_AIRCRAFT) {
			if (!v->IsStoppedInDepot()) {
				if (!v->current_order.IsType(OT_GOTO_DEPOT))
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
	if (++_companies_ai[c->index].state_counter <= 832) return;

	if (v->current_order.IsType(OT_GOTO_DEPOT)) {
		v->current_order.MakeDummy();
		InvalidateWindow(WC_VEHICLE_VIEW, v->index);
	}
return_to_loop:;
	_companies_ai[c->index].state = AIS_VEH_LOOP;
}

static void AiStateRemoveStation(Company *c)
{
	// Remove stations that aren't in use by any vehicle
	const Order *ord;
	const Station *st;
	TileIndex tile;

	// Go to this state when we're done.
	_companies_ai[c->index].state = AIS_1;

	// Get a list of all stations that are in use by a vehicle
	byte *in_use = MallocT<byte>(GetMaxStationIndex() + 1);
	memset(in_use, 0, GetMaxStationIndex() + 1);
	FOR_ALL_ORDERS(ord) {
		if (ord->IsType(OT_GOTO_STATION)) in_use[ord->GetDestination()] = 1;
	}

	// Go through all stations and delete those that aren't in use
	FOR_ALL_STATIONS(st) {
		if (st->owner == _current_company && !in_use[st->index] &&
				( (st->bus_stops != NULL && (tile = st->bus_stops->xy) != INVALID_TILE) ||
					(st->truck_stops != NULL && (tile = st->truck_stops->xy) != INVALID_TILE) ||
					(tile = st->train_tile) != INVALID_TILE ||
					(tile = st->dock_tile) != INVALID_TILE ||
					(tile = st->airport_tile) != INVALID_TILE)) {
			DoCommand(tile, 0, 0, DC_EXEC, CMD_LANDSCAPE_CLEAR);
		}
	}

	free(in_use);
}

static void AiRemoveCompanyRailOrRoad(Company *c, TileIndex tile)
{
	TrackBits rails;

	if (IsTileType(tile, MP_RAILWAY)) {
		if (!IsTileOwner(tile, _current_company)) return;

		if (IsPlainRailTile(tile)) {
is_rail_crossing:;
			rails = GetRailTrackStatus(tile);

			if (rails == TRACK_BIT_HORZ || rails == TRACK_BIT_VERT) return;

			if (rails & TRACK_BIT_3WAY_NE) {
pos_0:
				if ((GetRailTrackStatus(TILE_MASK(tile - TileDiffXY(1, 0))) & TRACK_BIT_3WAY_SW) == 0) {
					_companies_ai[c->index].cur_dir_a = DIAGDIR_NE;
					_companies_ai[c->index].cur_tile_a = tile;
					_companies_ai[c->index].state = AIS_REMOVE_SINGLE_RAIL_TILE;
					return;
				}
			}

			if (rails & TRACK_BIT_3WAY_SE) {
pos_1:
				if ((GetRailTrackStatus(TILE_MASK(tile + TileDiffXY(0, 1))) & TRACK_BIT_3WAY_NW) == 0) {
					_companies_ai[c->index].cur_dir_a = DIAGDIR_SE;
					_companies_ai[c->index].cur_tile_a = tile;
					_companies_ai[c->index].state = AIS_REMOVE_SINGLE_RAIL_TILE;
					return;
				}
			}

			if (rails & TRACK_BIT_3WAY_SW) {
pos_2:
				if ((GetRailTrackStatus(TILE_MASK(tile + TileDiffXY(1, 0))) & TRACK_BIT_3WAY_NE) == 0) {
					_companies_ai[c->index].cur_dir_a = DIAGDIR_SW;
					_companies_ai[c->index].cur_tile_a = tile;
					_companies_ai[c->index].state = AIS_REMOVE_SINGLE_RAIL_TILE;
					return;
				}
			}

			if (rails & TRACK_BIT_3WAY_NW) {
pos_3:
				if ((GetRailTrackStatus(TILE_MASK(tile - TileDiffXY(0, 1))) & TRACK_BIT_3WAY_SE) == 0) {
					_companies_ai[c->index].cur_dir_a = DIAGDIR_NW;
					_companies_ai[c->index].cur_tile_a = tile;
					_companies_ai[c->index].state = AIS_REMOVE_SINGLE_RAIL_TILE;
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
		if (IsLevelCrossing(tile)) goto is_rail_crossing;

		if (IsRoadDepot(tile)) {
			if (!IsTileOwner(tile, _current_company)) return;

			DiagDirection dir;
			TileIndex t;

			// Check if there are any stations around.
			t = tile + TileDiffXY(-1, 0);
			if (IsTileType(t, MP_STATION) && IsTileOwner(t, _current_company)) return;

			t = tile + TileDiffXY(1, 0);
			if (IsTileType(t, MP_STATION) && IsTileOwner(t, _current_company)) return;

			t = tile + TileDiffXY(0, -1);
			if (IsTileType(t, MP_STATION) && IsTileOwner(t, _current_company)) return;

			t = tile + TileDiffXY(0, 1);
			if (IsTileType(t, MP_STATION) && IsTileOwner(t, _current_company)) return;

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
		if (!IsTileOwner(tile, _current_company) ||
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

static void AiStateRemoveTrack(Company *c)
{
	/* Was 1000 for standard 8x8 maps. */
	int num = MapSizeX() * 4;

	do {
		TileIndex tile = ++_companies_ai[c->index].state_counter;

		// Iterated all tiles?
		if (tile >= MapSize()) {
			_companies_ai[c->index].state = AIS_REMOVE_STATION;
			return;
		}

		// Remove company stuff in that tile
		AiRemoveCompanyRailOrRoad(c, tile);
		if (_companies_ai[c->index].state != AIS_REMOVE_TRACK) return;
	} while (--num);
}

static void AiStateRemoveSingleRailTile(Company *c)
{
	// Remove until we can't remove more.
	if (!AiRemoveTileAndGoForward(c)) _companies_ai[c->index].state = AIS_REMOVE_TRACK;
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

extern void ShowBuyCompanyDialog(CompanyID company);

static void AiHandleTakeover(Company *c)
{
	if (c->bankrupt_timeout != 0) {
		c->bankrupt_timeout -= 8;
		if (c->bankrupt_timeout > 0) return;
		c->bankrupt_timeout = 0;
		DeleteWindowById(WC_BUY_COMPANY, _current_company);
		if (IsLocalCompany()) {
			AskExitToGameMenu();
			return;
		}
		if (IsHumanCompany(_current_company)) return;
	}

	if (c->bankrupt_asked == MAX_UVALUE(CompanyMask)) return;

	{
		CompanyMask asked = c->bankrupt_asked;
		Company *company, *best_company = NULL;
		int32 best_val = -1;

		// Ask the guy with the highest performance hist.
		FOR_ALL_COMPANIES(company) {
			if (!(asked & 1) &&
					company->bankrupt_asked == 0 &&
					best_val < company->old_economy[1].performance_history) {
				best_val = company->old_economy[1].performance_history;
				best_company = company;
			}
			asked >>= 1;
		}

		// Asked all companies?
		if (best_val == -1) {
			c->bankrupt_asked = MAX_UVALUE(CompanyMask);
			return;
		}

		SetBit(c->bankrupt_asked, best_company->index);

		if (best_company->index == _local_company) {
			c->bankrupt_timeout = 4440;
			ShowBuyCompanyDialog(_current_company);
			return;
		}
		if (IsHumanCompany(best_company->index)) return;

		// Too little money for computer to buy it?
		if (best_company->money >> 1 >= c->bankrupt_value) {
			// Computer wants to buy it.
			CompanyID old_company = _current_company;
			_current_company = best_company->index;
			DoCommand(0, old_company, 0, DC_EXEC, CMD_BUY_COMPANY);
			_current_company = old_company;
		}
	}
}

static void AiAdjustLoan(const Company *c)
{
	Money base = AiGetBasePrice(c);

	if (c->money > base * 1400) {
		// Decrease loan
		if (c->current_loan != 0) {
			DoCommand(0, 0, 0, DC_EXEC, CMD_DECREASE_LOAN);
		}
	} else if (c->money < base * 500) {
		// Increase loan
		if (c->current_loan < _economy.max_loan &&
				c->num_valid_stat_ent >= 2 &&
				-(c->old_economy[0].expenses + c->old_economy[1].expenses) < base * 60) {
			DoCommand(0, 0, 0, DC_EXEC, CMD_INCREASE_LOAN);
		}
	}
}

static void AiBuildCompanyHQ(Company *c)
{
	TileIndex tile;

	if (c->location_of_HQ == 0 &&
			c->last_build_coordinate != 0) {
		tile = AdjustTileCoordRandomly(c->last_build_coordinate, 8);
		DoCommand(tile, 0, 0, DC_EXEC | DC_AUTO | DC_NO_WATER, CMD_BUILD_COMPANY_HQ);
	}
}


void AiDoGameLoop(Company *c)
{
	if (c->bankrupt_asked != 0) {
		AiHandleTakeover(c);
		return;
	}

	// Ugly hack to make sure the service interval of the AI is good, not looking
	//  to the patch-setting
	// Also, it takes into account the setting if the service-interval is in days
	//  or in %
	_ai_service_interval = _settings_game.vehicle.servint_ispercent ? 80 : 180;

	if (IsHumanCompany(_current_company)) return;

	AiAdjustLoan(c);
	AiBuildCompanyHQ(c);

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

		if (_companies_ai[c->index].state != old_state) {
			if (hasdots)
				printf("\n");
			hasdots = false;
			printf("AiState: %s\n", _ai_state_names[old_state=_companies_ai[c->index].state]);
		} else {
			printf(".");
			hasdots = true;
		}
	}
#endif

	_ai_actions[_companies_ai[c->index].state](c);
}


static const SaveLoad _company_ai_desc[] = {
	    SLE_VAR(CompanyAI, state,             SLE_UINT8),
	    SLE_VAR(CompanyAI, tick,              SLE_UINT8),
	SLE_CONDVAR(CompanyAI, state_counter,     SLE_FILE_U16 | SLE_VAR_U32,  0, 12),
	SLE_CONDVAR(CompanyAI, state_counter,     SLE_UINT32,                 13, SL_MAX_VERSION),
	    SLE_VAR(CompanyAI, timeout_counter,   SLE_UINT16),

	    SLE_VAR(CompanyAI, state_mode,        SLE_UINT8),
	    SLE_VAR(CompanyAI, banned_tile_count, SLE_UINT8),
	    SLE_VAR(CompanyAI, railtype_to_use,   SLE_UINT8),

	    SLE_VAR(CompanyAI, cargo_type,        SLE_UINT8),
	    SLE_VAR(CompanyAI, num_wagons,        SLE_UINT8),
	    SLE_VAR(CompanyAI, build_kind,        SLE_UINT8),
	    SLE_VAR(CompanyAI, num_build_rec,     SLE_UINT8),
	    SLE_VAR(CompanyAI, num_loco_to_build, SLE_UINT8),
	    SLE_VAR(CompanyAI, num_want_fullload, SLE_UINT8),

	    SLE_VAR(CompanyAI, route_type_mask,   SLE_UINT8),

	SLE_CONDVAR(CompanyAI, start_tile_a,      SLE_FILE_U16 | SLE_VAR_U32,  0,  5),
	SLE_CONDVAR(CompanyAI, start_tile_a,      SLE_UINT32,                  6, SL_MAX_VERSION),
	SLE_CONDVAR(CompanyAI, cur_tile_a,        SLE_FILE_U16 | SLE_VAR_U32,  0,  5),
	SLE_CONDVAR(CompanyAI, cur_tile_a,        SLE_UINT32,                  6, SL_MAX_VERSION),
	    SLE_VAR(CompanyAI, start_dir_a,       SLE_UINT8),
	    SLE_VAR(CompanyAI, cur_dir_a,         SLE_UINT8),

	SLE_CONDVAR(CompanyAI, start_tile_b,      SLE_FILE_U16 | SLE_VAR_U32,  0,  5),
	SLE_CONDVAR(CompanyAI, start_tile_b,      SLE_UINT32,                  6, SL_MAX_VERSION),
	SLE_CONDVAR(CompanyAI, cur_tile_b,        SLE_FILE_U16 | SLE_VAR_U32,  0,  5),
	SLE_CONDVAR(CompanyAI, cur_tile_b,        SLE_UINT32,                  6, SL_MAX_VERSION),
	    SLE_VAR(CompanyAI, start_dir_b,       SLE_UINT8),
	    SLE_VAR(CompanyAI, cur_dir_b,         SLE_UINT8),

	    SLE_REF(CompanyAI, cur_veh,           REF_VEHICLE),

	    SLE_ARR(CompanyAI, wagon_list,        SLE_UINT16, 9),
	    SLE_ARR(CompanyAI, order_list_blocks, SLE_UINT8, 20),
	    SLE_ARR(CompanyAI, banned_tiles,      SLE_UINT16, 16),

	SLE_CONDNULL(64, 2, SL_MAX_VERSION),
	SLE_END()
};

static const SaveLoad _company_ai_build_rec_desc[] = {
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


void SaveLoad_AI(CompanyID company)
{
	CompanyAI *cai = &_companies_ai[company];
	SlObject(cai, _company_ai_desc);
	for (int i = 0; i != cai->num_build_rec; i++) {
		SlObject(&cai->src + i, _company_ai_build_rec_desc);
	}
}
