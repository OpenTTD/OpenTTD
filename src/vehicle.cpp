/* $Id$ */

/** @file vehicle.cpp Base implementations of all vehicles. */

#include "stdafx.h"
#include "gui.h"
#include "openttd.h"
#include "debug.h"
#include "roadveh.h"
#include "ship.h"
#include "spritecache.h"
#include "landscape.h"
#include "timetable.h"
#include "viewport_func.h"
#include "news_func.h"
#include "command_func.h"
#include "company_func.h"
#include "vehicle_gui.h"
#include "train.h"
#include "aircraft.h"
#include "newgrf_engine.h"
#include "newgrf_sound.h"
#include "newgrf_station.h"
#include "group.h"
#include "group_gui.h"
#include "strings_func.h"
#include "zoom_func.h"
#include "functions.h"
#include "date_func.h"
#include "window_func.h"
#include "vehicle_func.h"
#include "autoreplace_func.h"
#include "autoreplace_gui.h"
#include "oldpool_func.h"
#include "ai/ai.hpp"
#include "core/smallmap_type.hpp"
#include "depot_func.h"
#include "settings_type.h"
#include "network/network.h"

#include "table/sprites.h"
#include "table/strings.h"

#define GEN_HASH(x, y) ((GB((y), 6, 6) << 6) + GB((x), 7, 6))

VehicleID _vehicle_id_ctr_day;
const Vehicle *_place_clicked_vehicle;
VehicleID _new_vehicle_id;
uint16 _returned_refit_capacity;


/* Initialize the vehicle-pool */
DEFINE_OLD_POOL_GENERIC(Vehicle, Vehicle)

/** Function to tell if a vehicle needs to be autorenewed
 * @param *c The vehicle owner
 * @return true if the vehicle is old enough for replacement
 */
bool Vehicle::NeedsAutorenewing(const Company *c) const
{
	/* We can always generate the Company pointer when we have the vehicle.
	 * However this takes time and since the Company pointer is often present
	 * when this function is called then it's faster to pass the pointer as an
	 * argument rather than finding it again. */
	assert(c == GetCompany(this->owner));

	if (!c->engine_renew) return false;
	if (this->age - this->max_age < (c->engine_renew_months * 30)) return false;
	if (this->age == 0) return false; // rail cars don't age and lacks a max age

	return true;
}

void VehicleServiceInDepot(Vehicle *v)
{
	v->date_of_last_service = _date;
	v->breakdowns_since_last_service = 0;
	v->reliability = GetEngine(v->engine_type)->reliability;
	InvalidateWindow(WC_VEHICLE_DETAILS, v->index); // ensure that last service date and reliability are updated
}

bool Vehicle::NeedsServicing() const
{
	if (this->vehstatus & (VS_STOPPED | VS_CRASHED)) return false;

	if (_settings_game.order.no_servicing_if_no_breakdowns && _settings_game.difficulty.vehicle_breakdowns == 0) {
		/* Vehicles set for autoreplacing needs to go to a depot even if breakdowns are turned off.
		 * Note: If servicing is enabled, we postpone replacement till next service. */
		return EngineHasReplacementForCompany(GetCompany(this->owner), this->engine_type, this->group_id);
	}

	return _settings_game.vehicle.servint_ispercent ?
		(this->reliability < GetEngine(this->engine_type)->reliability * (100 - this->service_interval) / 100) :
		(this->date_of_last_service + this->service_interval < _date);
}

bool Vehicle::NeedsAutomaticServicing() const
{
	if (_settings_game.order.gotodepot && VehicleHasDepotOrders(this)) return false;
	if (this->current_order.IsType(OT_LOADING))            return false;
	if (this->current_order.IsType(OT_GOTO_DEPOT) && this->current_order.GetDepotOrderType() != ODTFB_SERVICE) return false;
	return NeedsServicing();
}

/**
 * Displays a "NewGrf Bug" error message for a engine, and pauses the game if not networking.
 * @param engine The engine that caused the problem
 * @param part1  Part 1 of the error message, taking the grfname as parameter 1
 * @param part2  Part 2 of the error message, taking the engine as parameter 2
 * @param bug_type Flag to check and set in grfconfig
 * @param critical Shall the "OpenTTD might crash"-message be shown when the player tries to unpause?
 */
void ShowNewGrfVehicleError(EngineID engine, StringID part1, StringID part2, GRFBugs bug_type, bool critical)
{
	const Engine *e = GetEngine(engine);
	uint32 grfid = e->grffile->grfid;
	GRFConfig *grfconfig = GetGRFConfig(grfid);

	if (!HasBit(grfconfig->grf_bugs, bug_type)) {
		SetBit(grfconfig->grf_bugs, bug_type);
		SetDParamStr(0, grfconfig->name);
		SetDParam(1, engine);
		ShowErrorMessage(part2, part1, 0, 0);
		if (!_networking) _pause_game = (critical ? -1 : 1);
	}

	/* debug output */
	char buffer[512];

	SetDParamStr(0, grfconfig->name);
	GetString(buffer, part1, lastof(buffer));
	DEBUG(grf, 0, "%s", buffer + 3);

	SetDParam(1, engine);
	GetString(buffer, part2, lastof(buffer));
	DEBUG(grf, 0, "%s", buffer + 3);
}

StringID VehicleInTheWayErrMsg(const Vehicle *v)
{
	switch (v->type) {
		case VEH_TRAIN:    return STR_8803_TRAIN_IN_THE_WAY;
		case VEH_ROAD:     return STR_9000_ROAD_VEHICLE_IN_THE_WAY;
		case VEH_AIRCRAFT: return STR_A015_AIRCRAFT_IN_THE_WAY;
		default:           return STR_980E_SHIP_IN_THE_WAY;
	}
}

static Vehicle *EnsureNoVehicleProcZ(Vehicle *v, void *data)
{
	byte z = *(byte*)data;

	if (v->type == VEH_DISASTER || (v->type == VEH_AIRCRAFT && v->subtype == AIR_SHADOW)) return NULL;
	if (v->z_pos > z) return NULL;

	_error_message = VehicleInTheWayErrMsg(v);
	return v;
}

bool EnsureNoVehicleOnGround(TileIndex tile)
{
	byte z = GetTileMaxZ(tile);
	return !HasVehicleOnPos(tile, &z, &EnsureNoVehicleProcZ);
}

/** Procedure called for every vehicle found in tunnel/bridge in the hash map */
static Vehicle *GetVehicleTunnelBridgeProc(Vehicle *v, void *data)
{
	if (v->type != VEH_TRAIN && v->type != VEH_ROAD && v->type != VEH_SHIP) return NULL;
	if (v == (const Vehicle *)data) return NULL;

	_error_message = VehicleInTheWayErrMsg(v);
	return v;
}

/**
 * Finds vehicle in tunnel / bridge
 * @param tile first end
 * @param endtile second end
 * @param ignore Ignore this vehicle when searching
 * @return true if the bridge has a vehicle
 */
bool HasVehicleOnTunnelBridge(TileIndex tile, TileIndex endtile, const Vehicle *ignore)
{
	return HasVehicleOnPos(tile, (void *)ignore, &GetVehicleTunnelBridgeProc) ||
			HasVehicleOnPos(endtile, (void *)ignore, &GetVehicleTunnelBridgeProc);
}


Vehicle::Vehicle()
{
	this->type               = VEH_INVALID;
	this->coord.left         = INVALID_COORD;
	this->group_id           = DEFAULT_GROUP;
	this->fill_percent_te_id = INVALID_TE_ID;
	this->first              = this;
	this->colourmap          = PAL_NONE;
}

/**
 * Get a value for a vehicle's random_bits.
 * @return A random value from 0 to 255.
 */
byte VehicleRandomBits()
{
	return GB(Random(), 0, 8);
}


/* static */ bool Vehicle::AllocateList(Vehicle **vl, int num)
{
	if (!Vehicle::CanAllocateItem(num)) return false;
	if (vl == NULL) return true;

	uint counter = _Vehicle_pool.first_free_index;

	for (int i = 0; i != num; i++) {
		vl[i] = new (AllocateRaw(counter)) InvalidVehicle();
		counter++;
	}

	return true;
}

/* Size of the hash, 6 = 64 x 64, 7 = 128 x 128. Larger sizes will (in theory) reduce hash
 * lookup times at the expense of memory usage. */
const int HASH_BITS = 7;
const int HASH_SIZE = 1 << HASH_BITS;
const int HASH_MASK = HASH_SIZE - 1;
const int TOTAL_HASH_SIZE = 1 << (HASH_BITS * 2);
const int TOTAL_HASH_MASK = TOTAL_HASH_SIZE - 1;

/* Resolution of the hash, 0 = 1*1 tile, 1 = 2*2 tiles, 2 = 4*4 tiles, etc.
 * Profiling results show that 0 is fastest. */
const int HASH_RES = 0;

static Vehicle *_new_vehicle_position_hash[TOTAL_HASH_SIZE];

static Vehicle *VehicleFromHash(int xl, int yl, int xu, int yu, void *data, VehicleFromPosProc *proc, bool find_first)
{
	for (int y = yl; ; y = (y + (1 << HASH_BITS)) & (HASH_MASK << HASH_BITS)) {
		for (int x = xl; ; x = (x + 1) & HASH_MASK) {
			Vehicle *v = _new_vehicle_position_hash[(x + y) & TOTAL_HASH_MASK];
			for (; v != NULL; v = v->next_new_hash) {
				Vehicle *a = proc(v, data);
				if (find_first && a != NULL) return a;
			}
			if (x == xu) break;
		}
		if (y == yu) break;
	}

	return NULL;
}


/**
 * Helper function for FindVehicleOnPos/HasVehicleOnPos.
 * @note Do not call this function directly!
 * @param x    The X location on the map
 * @param y    The Y location on the map
 * @param data Arbitrary data passed to proc
 * @param proc The proc that determines whether a vehicle will be "found".
 * @param find_first Whether to return on the first found or iterate over
 *                   all vehicles
 * @return the best matching or first vehicle (depending on find_first).
 */
static Vehicle *VehicleFromPosXY(int x, int y, void *data, VehicleFromPosProc *proc, bool find_first)
{
	const int COLL_DIST = 6;

	/* Hash area to scan is from xl,yl to xu,yu */
	int xl = GB((x - COLL_DIST) / TILE_SIZE, HASH_RES, HASH_BITS);
	int xu = GB((x + COLL_DIST) / TILE_SIZE, HASH_RES, HASH_BITS);
	int yl = GB((y - COLL_DIST) / TILE_SIZE, HASH_RES, HASH_BITS) << HASH_BITS;
	int yu = GB((y + COLL_DIST) / TILE_SIZE, HASH_RES, HASH_BITS) << HASH_BITS;

	return VehicleFromHash(xl, yl, xu, yu, data, proc, find_first);
}

/**
 * Find a vehicle from a specific location. It will call proc for ALL vehicles
 * on the tile and YOU must make SURE that the "best one" is stored in the
 * data value and is ALWAYS the same regardless of the order of the vehicles
 * where proc was called on!
 * When you fail to do this properly you create an almost untraceable DESYNC!
 * @note The return value of proc will be ignored.
 * @note Use this when you have the intention that all vehicles
 *       should be iterated over.
 * @param x    The X location on the map
 * @param y    The Y location on the map
 * @param data Arbitrary data passed to proc
 * @param proc The proc that determines whether a vehicle will be "found".
 */
void FindVehicleOnPosXY(int x, int y, void *data, VehicleFromPosProc *proc)
{
	VehicleFromPosXY(x, y, data, proc, false);
}

/**
 * Checks whether a vehicle in on a specific location. It will call proc for
 * vehicles until it returns non-NULL.
 * @note Use FindVehicleOnPosXY when you have the intention that all vehicles
 *       should be iterated over.
 * @param x    The X location on the map
 * @param y    The Y location on the map
 * @param data Arbitrary data passed to proc
 * @param proc The proc that determines whether a vehicle will be "found".
 * @return True if proc returned non-NULL.
 */
bool HasVehicleOnPosXY(int x, int y, void *data, VehicleFromPosProc *proc)
{
	return VehicleFromPosXY(x, y, data, proc, true) != NULL;
}

/**
 * Helper function for FindVehicleOnPos/HasVehicleOnPos.
 * @note Do not call this function directly!
 * @param tile The location on the map
 * @param data Arbitrary data passed to proc
 * @param proc The proc that determines whether a vehicle will be "found".
 * @param find_first Whether to return on the first found or iterate over
 *                   all vehicles
 * @return the best matching or first vehicle (depending on find_first).
 */
static Vehicle *VehicleFromPos(TileIndex tile, void *data, VehicleFromPosProc *proc, bool find_first)
{
	int x = GB(TileX(tile), HASH_RES, HASH_BITS);
	int y = GB(TileY(tile), HASH_RES, HASH_BITS) << HASH_BITS;

	Vehicle *v = _new_vehicle_position_hash[(x + y) & TOTAL_HASH_MASK];
	for (; v != NULL; v = v->next_new_hash) {
		if (v->tile != tile) continue;

		Vehicle *a = proc(v, data);
		if (find_first && a != NULL) return a;
	}

	return NULL;
}

/**
 * Find a vehicle from a specific location. It will call proc for ALL vehicles
 * on the tile and YOU must make SURE that the "best one" is stored in the
 * data value and is ALWAYS the same regardless of the order of the vehicles
 * where proc was called on!
 * When you fail to do this properly you create an almost untraceable DESYNC!
 * @note The return value of proc will be ignored.
 * @note Use this when you have the intention that all vehicles
 *       should be iterated over.
 * @param tile The location on the map
 * @param data Arbitrary data passed to proc
 * @param proc The proc that determines whether a vehicle will be "found".
 */
void FindVehicleOnPos(TileIndex tile, void *data, VehicleFromPosProc *proc)
{
	VehicleFromPos(tile, data, proc, false);
}

/**
 * Checks whether a vehicle in on a specific location. It will call proc for
 * vehicles until it returns non-NULL.
 * @note Use FindVehicleOnPos when you have the intention that all vehicles
 *       should be iterated over.
 * @param tile The location on the map
 * @param data Arbitrary data passed to proc
 * @param proc The proc that determines whether a vehicle will be "found".
 * @return True if proc returned non-NULL.
 */
bool HasVehicleOnPos(TileIndex tile, void *data, VehicleFromPosProc *proc)
{
	return VehicleFromPos(tile, data, proc, true) != NULL;
}


static void UpdateNewVehiclePosHash(Vehicle *v, bool remove)
{
	Vehicle **old_hash = v->old_new_hash;
	Vehicle **new_hash;

	if (remove) {
		new_hash = NULL;
	} else {
		int x = GB(TileX(v->tile), HASH_RES, HASH_BITS);
		int y = GB(TileY(v->tile), HASH_RES, HASH_BITS) << HASH_BITS;
		new_hash = &_new_vehicle_position_hash[(x + y) & TOTAL_HASH_MASK];
	}

	if (old_hash == new_hash) return;

	/* Remove from the old position in the hash table */
	if (old_hash != NULL) {
		Vehicle *last = NULL;
		Vehicle *u = *old_hash;
		while (u != v) {
			last = u;
			u = u->next_new_hash;
			assert(u != NULL);
		}

		if (last == NULL) {
			*old_hash = v->next_new_hash;
		} else {
			last->next_new_hash = v->next_new_hash;
		}
	}

	/* Insert vehicle at beginning of the new position in the hash table */
	if (new_hash != NULL) {
		v->next_new_hash = *new_hash;
		*new_hash = v;
		assert(v != v->next_new_hash);
	}

	/* Remember current hash position */
	v->old_new_hash = new_hash;
}

static Vehicle *_vehicle_position_hash[0x1000];

static void UpdateVehiclePosHash(Vehicle *v, int x, int y)
{
	UpdateNewVehiclePosHash(v, x == INVALID_COORD);

	Vehicle **old_hash, **new_hash;
	int old_x = v->coord.left;
	int old_y = v->coord.top;

	new_hash = (x == INVALID_COORD) ? NULL : &_vehicle_position_hash[GEN_HASH(x, y)];
	old_hash = (old_x == INVALID_COORD) ? NULL : &_vehicle_position_hash[GEN_HASH(old_x, old_y)];

	if (old_hash == new_hash) return;

	/* remove from hash table? */
	if (old_hash != NULL) {
		Vehicle *last = NULL;
		Vehicle *u = *old_hash;
		while (u != v) {
			last = u;
			u = u->next_hash;
			assert(u != NULL);
		}

		if (last == NULL) {
			*old_hash = v->next_hash;
		} else {
			last->next_hash = v->next_hash;
		}
	}

	/* insert into hash table? */
	if (new_hash != NULL) {
		v->next_hash = *new_hash;
		*new_hash = v;
	}
}

void ResetVehiclePosHash()
{
	Vehicle *v;
	FOR_ALL_VEHICLES(v) { v->old_new_hash = NULL; }
	memset(_vehicle_position_hash, 0, sizeof(_vehicle_position_hash));
	memset(_new_vehicle_position_hash, 0, sizeof(_new_vehicle_position_hash));
}

void ResetVehicleColourMap()
{
	Vehicle *v;
	FOR_ALL_VEHICLES(v) { v->colourmap = PAL_NONE; }
}

/**
 * List of vehicles that should check for autoreplace this tick.
 * Mapping of vehicle -> leave depot immediatelly after autoreplace.
 */
typedef SmallMap<Vehicle *, bool, 4> AutoreplaceMap;
static AutoreplaceMap _vehicles_to_autoreplace;

void InitializeVehicles()
{
	_Vehicle_pool.CleanPool();
	_Vehicle_pool.AddBlockToPool();

	_vehicles_to_autoreplace.Reset();
	ResetVehiclePosHash();
}

Vehicle *GetLastVehicleInChain(Vehicle *v)
{
	while (v->Next() != NULL) v = v->Next();
	return v;
}

const Vehicle *GetLastVehicleInChain(const Vehicle *v)
{
	while (v->Next() != NULL) v = v->Next();
	return v;
}

uint CountVehiclesInChain(const Vehicle *v)
{
	uint count = 0;
	do count++; while ((v = v->Next()) != NULL);
	return count;
}

/** Check if a vehicle is counted in num_engines in each company struct
 * @param *v Vehicle to test
 * @return true if the vehicle is counted in num_engines
 */
bool IsEngineCountable(const Vehicle *v)
{
	switch (v->type) {
		case VEH_AIRCRAFT: return IsNormalAircraft(v); // don't count plane shadows and helicopter rotors
		case VEH_TRAIN:
			return !IsArticulatedPart(v) && // tenders and other articulated parts
			!IsRearDualheaded(v); // rear parts of multiheaded engines
		case VEH_ROAD: return IsRoadVehFront(v);
		case VEH_SHIP: return true;
		default: return false; // Only count company buildable vehicles
	}
}

void Vehicle::PreDestructor()
{
	if (CleaningPool()) return;

	if (IsValidStationID(this->last_station_visited)) {
		GetStation(this->last_station_visited)->loading_vehicles.remove(this);

		HideFillingPercent(&this->fill_percent_te_id);
	}

	if (IsEngineCountable(this)) {
		GetCompany(this->owner)->num_engines[this->engine_type]--;
		if (this->owner == _local_company) InvalidateAutoreplaceWindow(this->engine_type, this->group_id);

		DeleteGroupHighlightOfVehicle(this);
		if (IsValidGroupID(this->group_id)) GetGroup(this->group_id)->num_engines[this->engine_type]--;
		if (this->IsPrimaryVehicle()) DecreaseGroupNumVehicle(this->group_id);
	}

	if (this->type == VEH_ROAD) ClearSlot(this);
	if (this->type == VEH_AIRCRAFT && this->IsPrimaryVehicle()) {
		Station *st = GetTargetAirportIfValid(this);
		if (st != NULL) {
			const AirportFTA *layout = st->Airport()->layout;
			CLRBITS(st->airport_flags, layout[this->u.air.previous_pos].block | layout[this->u.air.pos].block);
		}
	}

	if (this->type != VEH_TRAIN || (this->type == VEH_TRAIN && (IsFrontEngine(this) || IsFreeWagon(this)))) {
		InvalidateWindowData(WC_VEHICLE_DEPOT, this->tile);
	}

	if (this->IsPrimaryVehicle()) {
		DeleteWindowById(WC_VEHICLE_VIEW, this->index);
		DeleteWindowById(WC_VEHICLE_ORDERS, this->index);
		DeleteWindowById(WC_VEHICLE_REFIT, this->index);
		DeleteWindowById(WC_VEHICLE_DETAILS, this->index);
		DeleteWindowById(WC_VEHICLE_TIMETABLE, this->index);
		InvalidateWindow(WC_COMPANY, this->owner);
	}
	InvalidateWindowClassesData(GetWindowClassForVehicleType(this->type), 0);

	this->cargo.Truncate(0);
	DeleteVehicleOrders(this);
	DeleteDepotHighlightOfVehicle(this);

	extern void StopGlobalFollowVehicle(const Vehicle *v);
	StopGlobalFollowVehicle(this);
}

Vehicle::~Vehicle()
{
	free(this->name);

	if (CleaningPool()) return;

	/* sometimes, eg. for disaster vehicles, when company bankrupts, when removing crashed/flooded vehicles,
	 * it may happen that vehicle chain is deleted when visible */
	if (!(this->vehstatus & VS_HIDDEN)) MarkSingleVehicleDirty(this);

	Vehicle *v = this->Next();
	this->SetNext(NULL);

	delete v;

	UpdateVehiclePosHash(this, INVALID_COORD, 0);
	this->next_hash = NULL;
	this->next_new_hash = NULL;

	DeleteVehicleNews(this->index, INVALID_STRING_ID);

	this->type = VEH_INVALID;
}

/** Adds a vehicle to the list of vehicles, that visited a depot this tick
 * @param *v vehicle to add
 */
void VehicleEnteredDepotThisTick(Vehicle *v)
{
	/* Vehicle should stop in the depot if it was in 'stopping' state or
	 * when the vehicle is ordered to halt in the depot. */
	_vehicles_to_autoreplace[v] = !(v->vehstatus & VS_STOPPED) &&
			(!v->current_order.IsType(OT_GOTO_DEPOT) ||
			 !(v->current_order.GetDepotActionType() & ODATFB_HALT));

	/* We ALWAYS set the stopped state. Even when the vehicle does not plan on
	 * stopping in the depot, so we stop it to ensure that it will not reserve
	 * the path out of the depot before we might autoreplace it to a different
	 * engine. The new engine would not own the reserved path we store that we
	 * stopped the vehicle, so autoreplace can start it again */
	v->vehstatus |= VS_STOPPED;
}

void CallVehicleTicks()
{
	_vehicles_to_autoreplace.Clear();

	Station *st;
	FOR_ALL_STATIONS(st) LoadUnloadStation(st);

	Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		v->Tick();

		switch (v->type) {
			default: break;

			case VEH_TRAIN:
			case VEH_ROAD:
			case VEH_AIRCRAFT:
			case VEH_SHIP:
				if (v->type == VEH_TRAIN && IsTrainWagon(v)) continue;
				if (v->type == VEH_AIRCRAFT && v->subtype != AIR_HELICOPTER) continue;
				if (v->type == VEH_ROAD && !IsRoadVehFront(v)) continue;

				v->motion_counter += (v->direction & 1) ? (v->cur_speed * 3) / 4 : v->cur_speed;
				/* Play a running sound if the motion counter passes 256 (Do we not skip sounds?) */
				if (GB(v->motion_counter, 0, 8) < v->cur_speed) PlayVehicleSound(v, VSE_RUNNING);

				/* Play an alterate running sound every 16 ticks */
				if (GB(v->tick_counter, 0, 4) == 0) PlayVehicleSound(v, v->cur_speed > 0 ? VSE_RUNNING_16 : VSE_STOPPED_16);
		}
	}

	for (AutoreplaceMap::iterator it = _vehicles_to_autoreplace.Begin(); it != _vehicles_to_autoreplace.End(); it++) {
		v = it->first;
		/* Autoreplace needs the current company set as the vehicle owner */
		_current_company = v->owner;

		/* Start vehicle if we stopped them in VehicleEnteredDepotThisTick()
		 * We need to stop them between VehicleEnteredDepotThisTick() and here or we risk that
		 * they are already leaving the depot again before being replaced. */
		if (it->second) v->vehstatus &= ~VS_STOPPED;

		/* Store the position of the effect as the vehicle pointer will become invalid later */
		int x = v->x_pos;
		int y = v->y_pos;
		int z = v->z_pos;

		const Company *c = GetCompany(_current_company);
		SubtractMoneyFromCompany(CommandCost(EXPENSES_NEW_VEHICLES, (Money)c->engine_renew_money));
		CommandCost res = DoCommand(0, v->index, 0, DC_EXEC, CMD_AUTOREPLACE_VEHICLE);
		SubtractMoneyFromCompany(CommandCost(EXPENSES_NEW_VEHICLES, -(Money)c->engine_renew_money));

		if (!IsLocalCompany()) continue;

		if (res.Succeeded()) {
			ShowCostOrIncomeAnimation(x, y, z, res.GetCost());
			continue;
		}

		StringID error_message = res.GetErrorMessage();
		if (error_message == STR_AUTOREPLACE_NOTHING_TO_DO || error_message == INVALID_STRING_ID) continue;

		if (error_message == STR_0003_NOT_ENOUGH_CASH_REQUIRES) error_message = STR_AUTOREPLACE_MONEY_LIMIT;

		StringID message;
		if (error_message == STR_TRAIN_TOO_LONG_AFTER_REPLACEMENT) {
			message = error_message;
		} else {
			message = STR_VEHICLE_AUTORENEW_FAILED;
		}

		SetDParam(0, v->index);
		SetDParam(1, error_message);
		AddNewsItem(message, NS_ADVICE, v->index, 0);
	}

	_current_company = OWNER_NONE;
}

/** Check if a given engine type can be refitted to a given cargo
 * @param engine_type Engine type to check
 * @param cid_to check refit to this cargo-type
 * @return true if it is possible, false otherwise
 */
bool CanRefitTo(EngineID engine_type, CargoID cid_to)
{
	return HasBit(EngInfo(engine_type)->refit_mask, cid_to);
}

/** Find the first cargo type that an engine can be refitted to.
 * @param engine_type Which engine to find cargo for.
 * @return A climate dependent cargo type. CT_INVALID is returned if not refittable.
 */
CargoID FindFirstRefittableCargo(EngineID engine_type)
{
	uint32 refit_mask = EngInfo(engine_type)->refit_mask;

	if (refit_mask != 0) {
		for (CargoID cid = 0; cid < NUM_CARGO; cid++) {
			if (HasBit(refit_mask, cid)) return cid;
		}
	}

	return CT_INVALID;
}

/** Learn the price of refitting a certain engine
 * @param engine_type Which engine to refit
 * @return Price for refitting
 */
CommandCost GetRefitCost(EngineID engine_type)
{
	Money base_cost;
	ExpensesType expense_type;
	switch (GetEngine(engine_type)->type) {
		case VEH_SHIP:
			base_cost = _price.ship_base;
			expense_type = EXPENSES_SHIP_RUN;
			break;

		case VEH_ROAD:
			base_cost = _price.roadveh_base;
			expense_type = EXPENSES_ROADVEH_RUN;
			break;

		case VEH_AIRCRAFT:
			base_cost = _price.aircraft_base;
			expense_type = EXPENSES_AIRCRAFT_RUN;
			break;

		case VEH_TRAIN:
			base_cost = 2 * ((RailVehInfo(engine_type)->railveh_type == RAILVEH_WAGON) ?
							 _price.build_railwagon : _price.build_railvehicle);
			expense_type = EXPENSES_TRAIN_RUN;
			break;

		default: NOT_REACHED();
	}
	return CommandCost(expense_type, (EngInfo(engine_type)->refit_cost * base_cost) >> 10);
}

static void DoDrawVehicle(const Vehicle *v)
{
	SpriteID image = v->cur_image;
	SpriteID pal = PAL_NONE;

	if (v->vehstatus & VS_DEFPAL) pal = (v->vehstatus & VS_CRASHED) ? PALETTE_CRASH : GetVehiclePalette(v);

	AddSortableSpriteToDraw(image, pal, v->x_pos + v->x_offs, v->y_pos + v->y_offs,
		v->x_extent, v->y_extent, v->z_extent, v->z_pos, (v->vehstatus & VS_SHADOW) != 0);
}

void ViewportAddVehicles(DrawPixelInfo *dpi)
{
	/* The bounding rectangle */
	const int l = dpi->left;
	const int r = dpi->left + dpi->width;
	const int t = dpi->top;
	const int b = dpi->top + dpi->height;

	/* The hash area to scan */
	int xl, xu, yl, yu;

	if (dpi->width + 70 < (1 << (7 + 6))) {
		xl = GB(l - 70, 7, 6);
		xu = GB(r,      7, 6);
	} else {
		/* scan whole hash row */
		xl = 0;
		xu = 0x3F;
	}

	if (dpi->height + 70 < (1 << (6 + 6))) {
		yl = GB(t - 70, 6, 6) << 6;
		yu = GB(b,      6, 6) << 6;
	} else {
		/* scan whole column */
		yl = 0;
		yu = 0x3F << 6;
	}

	for (int y = yl;; y = (y + (1 << 6)) & (0x3F << 6)) {
		for (int x = xl;; x = (x + 1) & 0x3F) {
			const Vehicle *v = _vehicle_position_hash[x + y]; // already masked & 0xFFF

			while (v != NULL) {
				if (!(v->vehstatus & VS_HIDDEN) &&
						l <= v->coord.right &&
						t <= v->coord.bottom &&
						r >= v->coord.left &&
						b >= v->coord.top) {
					DoDrawVehicle(v);
				}
				v = v->next_hash;
			}

			if (x == xu) break;
		}

		if (y == yu) break;
	}
}

Vehicle *CheckClickOnVehicle(const ViewPort *vp, int x, int y)
{
	Vehicle *found = NULL, *v;
	uint dist, best_dist = UINT_MAX;

	if ((uint)(x -= vp->left) >= (uint)vp->width || (uint)(y -= vp->top) >= (uint)vp->height) return NULL;

	x = ScaleByZoom(x, vp->zoom) + vp->virtual_left;
	y = ScaleByZoom(y, vp->zoom) + vp->virtual_top;

	FOR_ALL_VEHICLES(v) {
		if ((v->vehstatus & (VS_HIDDEN | VS_UNCLICKABLE)) == 0 &&
				x >= v->coord.left && x <= v->coord.right &&
				y >= v->coord.top && y <= v->coord.bottom) {

			dist = max(
				abs(((v->coord.left + v->coord.right) >> 1) - x),
				abs(((v->coord.top + v->coord.bottom) >> 1) - y)
			);

			if (dist < best_dist) {
				found = v;
				best_dist = dist;
			}
		}
	}

	return found;
}

void CheckVehicle32Day(Vehicle *v)
{
	if ((v->day_counter & 0x1F) != 0) return;

	uint16 callback = GetVehicleCallback(CBID_VEHICLE_32DAY_CALLBACK, 0, 0, v->engine_type, v);
	if (callback == CALLBACK_FAILED) return;
	if (HasBit(callback, 0)) TriggerVehicle(v, VEHICLE_TRIGGER_CALLBACK_32); // Trigger vehicle trigger 10
	if (HasBit(callback, 1)) v->colourmap = PAL_NONE;                         // Update colourmap via callback 2D
}

void DecreaseVehicleValue(Vehicle *v)
{
	v->value -= v->value >> 8;
	InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
}

static const byte _breakdown_chance[64] = {
	  3,   3,   3,   3,   3,   3,   3,   3,
	  4,   4,   5,   5,   6,   6,   7,   7,
	  8,   8,   9,   9,  10,  10,  11,  11,
	 12,  13,  13,  13,  13,  14,  15,  16,
	 17,  19,  21,  25,  28,  31,  34,  37,
	 40,  44,  48,  52,  56,  60,  64,  68,
	 72,  80,  90, 100, 110, 120, 130, 140,
	150, 170, 190, 210, 230, 250, 250, 250,
};

void CheckVehicleBreakdown(Vehicle *v)
{
	int rel, rel_old;

	/* decrease reliability */
	v->reliability = rel = max((rel_old = v->reliability) - v->reliability_spd_dec, 0);
	if ((rel_old >> 8) != (rel >> 8)) InvalidateWindow(WC_VEHICLE_DETAILS, v->index);

	if (v->breakdown_ctr != 0 || v->vehstatus & VS_STOPPED ||
			_settings_game.difficulty.vehicle_breakdowns < 1 ||
			v->cur_speed < 5 || _game_mode == GM_MENU) {
		return;
	}

	uint32 r = Random();

	/* increase chance of failure */
	int chance = v->breakdown_chance + 1;
	if (Chance16I(1, 25, r)) chance += 25;
	v->breakdown_chance = min(255, chance);

	/* calculate reliability value to use in comparison */
	rel = v->reliability;
	if (v->type == VEH_SHIP) rel += 0x6666;

	/* reduced breakdowns? */
	if (_settings_game.difficulty.vehicle_breakdowns == 1) rel += 0x6666;

	/* check if to break down */
	if (_breakdown_chance[(uint)min(rel, 0xffff) >> 10] <= v->breakdown_chance) {
		v->breakdown_ctr    = GB(r, 16, 6) + 0x3F;
		v->breakdown_delay  = GB(r, 24, 7) + 0x80;
		v->breakdown_chance = 0;
	}
}

void AgeVehicle(Vehicle *v)
{
	if (v->age < 65535) v->age++;

	int age = v->age - v->max_age;
	if (age == DAYS_IN_LEAP_YEAR * 0 || age == DAYS_IN_LEAP_YEAR * 1 ||
			age == DAYS_IN_LEAP_YEAR * 2 || age == DAYS_IN_LEAP_YEAR * 3 || age == DAYS_IN_LEAP_YEAR * 4) {
		v->reliability_spd_dec <<= 1;
	}

	InvalidateWindow(WC_VEHICLE_DETAILS, v->index);

	/* Don't warn about non-primary or not ours vehicles */
	if (v->Previous() != NULL || v->owner != _local_company) return;

	/* Don't warn if a renew is active */
	if (GetCompany(v->owner)->engine_renew && GetEngine(v->engine_type)->company_avail != 0) return;

	StringID str;
	if (age == -DAYS_IN_LEAP_YEAR) {
		str = STR_01A0_IS_GETTING_OLD;
	} else if (age == 0) {
		str = STR_01A1_IS_GETTING_VERY_OLD;
	} else if (age > 0 && (age % DAYS_IN_LEAP_YEAR) == 0) {
		str = STR_01A2_IS_GETTING_VERY_OLD_AND;
	} else {
		return;
	}

	SetDParam(0, v->index);
	AddNewsItem(str, NS_ADVICE, v->index, 0);
}

/**
 * Calculates how full a vehicle is.
 * @param v The Vehicle to check. For trains, use the first engine.
 * @param colour The string to show depending on if we are unloading or loading
 * @return A percentage of how full the Vehicle is.
 */
uint8 CalcPercentVehicleFilled(const Vehicle *v, StringID *colour)
{
	int count = 0;
	int max = 0;
	int cars = 0;
	int unloading = 0;
	bool loading = false;

	const Vehicle *u = v;
	const Station *st = v->last_station_visited != INVALID_STATION ? GetStation(v->last_station_visited) : NULL;

	/* Count up max and used */
	for (; v != NULL; v = v->Next()) {
		count += v->cargo.Count();
		max += v->cargo_cap;
		if (v->cargo_cap != 0 && colour != NULL) {
			unloading += HasBit(v->vehicle_flags, VF_CARGO_UNLOADING) ? 1 : 0;
			loading |= !(u->current_order.GetUnloadType() & OUFB_UNLOAD) && st->goods[v->cargo_type].days_since_pickup != 255;
			cars++;
		}
	}

	if (colour != NULL) {
		if (unloading == 0 && loading) {
			*colour = STR_PERCENT_UP;
		} else if (cars == unloading || !loading) {
			*colour = STR_PERCENT_DOWN;
		} else {
			*colour = STR_PERCENT_UP_DOWN;
		}
	}

	/* Train without capacity */
	if (max == 0) return 100;

	/* Return the percentage */
	return (count * 100) / max;
}

void VehicleEnterDepot(Vehicle *v)
{
	switch (v->type) {
		case VEH_TRAIN:
			InvalidateWindowClasses(WC_TRAINS_LIST);
			/* Clear path reservation */
			SetDepotWaypointReservation(v->tile, false);
			if (_settings_client.gui.show_track_reservation) MarkTileDirtyByTile(v->tile);

			if (!IsFrontEngine(v)) v = v->First();
			UpdateSignalsOnSegment(v->tile, INVALID_DIAGDIR, v->owner);
			v->load_unload_time_rem = 0;
			ClrBit(v->u.rail.flags, VRF_TOGGLE_REVERSE);
			TrainConsistChanged(v, true);
			break;

		case VEH_ROAD:
			InvalidateWindowClasses(WC_ROADVEH_LIST);
			if (!IsRoadVehFront(v)) v = v->First();
			break;

		case VEH_SHIP:
			InvalidateWindowClasses(WC_SHIPS_LIST);
			v->u.ship.state = TRACK_BIT_DEPOT;
			RecalcShipStuff(v);
			break;

		case VEH_AIRCRAFT:
			InvalidateWindowClasses(WC_AIRCRAFT_LIST);
			HandleAircraftEnterHangar(v);
			break;
		default: NOT_REACHED();
	}

	if (v->type != VEH_TRAIN) {
		/* Trains update the vehicle list when the first unit enters the depot and calls VehicleEnterDepot() when the last unit enters.
		 * We only increase the number of vehicles when the first one enters, so we will not need to search for more vehicles in the depot */
		InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
	}
	InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);

	v->vehstatus |= VS_HIDDEN;
	v->cur_speed = 0;

	VehicleServiceInDepot(v);

	TriggerVehicle(v, VEHICLE_TRIGGER_DEPOT);

	if (v->current_order.IsType(OT_GOTO_DEPOT)) {
		InvalidateWindow(WC_VEHICLE_VIEW, v->index);

		Order t = v->current_order;
		v->current_order.MakeDummy();

		if (t.IsRefit()) {
			_current_company = v->owner;
			CommandCost cost = DoCommand(v->tile, v->index, t.GetRefitCargo() | t.GetRefitSubtype() << 8, DC_EXEC, GetCmdRefitVeh(v));

			if (CmdFailed(cost)) {
				_vehicles_to_autoreplace[v] = false;
				if (v->owner == _local_company) {
					/* Notify the user that we stopped the vehicle */
					SetDParam(0, v->index);
					AddNewsItem(STR_ORDER_REFIT_FAILED, NS_ADVICE, v->index, 0);
				}
			} else if (v->owner == _local_company && cost.GetCost() != 0) {
				ShowCostOrIncomeAnimation(v->x_pos, v->y_pos, v->z_pos, cost.GetCost());
			}
		}

		if (t.GetDepotOrderType() & ODTFB_PART_OF_ORDERS) {
			/* Part of orders */
			UpdateVehicleTimetable(v, true);
			v->cur_order_index++;
		}
		if (t.GetDepotActionType() & ODATFB_HALT) {
			/* Force depot visit */
			v->vehstatus |= VS_STOPPED;
			if (v->owner == _local_company) {
				StringID string;

				switch (v->type) {
					case VEH_TRAIN:    string = STR_8814_TRAIN_IS_WAITING_IN_DEPOT; break;
					case VEH_ROAD:     string = STR_9016_ROAD_VEHICLE_IS_WAITING;   break;
					case VEH_SHIP:     string = STR_981C_SHIP_IS_WAITING_IN_DEPOT;  break;
					case VEH_AIRCRAFT: string = STR_A014_AIRCRAFT_IS_WAITING_IN;    break;
					default: NOT_REACHED(); string = STR_EMPTY; // Set the string to something to avoid a compiler warning
				}

				SetDParam(0, v->index);
				AddNewsItem(string, NS_ADVICE, v->index, 0);
			}
			AI::NewEvent(v->owner, new AIEventVehicleWaitingInDepot(v->index));
		}
	}
}


/**
 * Move a vehicle in the game state; that is moving it's position in
 * the position hashes and marking it's location in the viewport dirty
 * if requested.
 * @param v vehicle to move
 * @param update_viewport whether to dirty the viewport
 */
void VehicleMove(Vehicle *v, bool update_viewport)
{
	int img = v->cur_image;
	Point pt = RemapCoords(v->x_pos + v->x_offs, v->y_pos + v->y_offs, v->z_pos);
	const Sprite *spr = GetSprite(img, ST_NORMAL);

	pt.x += spr->x_offs;
	pt.y += spr->y_offs;

	UpdateVehiclePosHash(v, pt.x, pt.y);

	Rect old_coord = v->coord;
	v->coord.left   = pt.x;
	v->coord.top    = pt.y;
	v->coord.right  = pt.x + spr->width + 2;
	v->coord.bottom = pt.y + spr->height + 2;

	if (update_viewport) {
		MarkAllViewportsDirty(
			min(old_coord.left,   v->coord.left),
			min(old_coord.top,    v->coord.top),
			max(old_coord.right,  v->coord.right) + 1,
			max(old_coord.bottom, v->coord.bottom) + 1
		);
	}
}

/**
 * Marks viewports dirty where the vehicle's image is
 * In fact, it equals
 *   BeginVehicleMove(v); EndVehicleMove(v);
 * @param v vehicle to mark dirty
 * @see BeginVehicleMove()
 * @see EndVehicleMove()
 */
void MarkSingleVehicleDirty(const Vehicle *v)
{
	MarkAllViewportsDirty(v->coord.left, v->coord.top, v->coord.right + 1, v->coord.bottom + 1);
}

/**
 * Get position information of a vehicle when moving one pixel in the direction it is facing
 * @param v Vehicle to move
 * @return Position information after the move */
GetNewVehiclePosResult GetNewVehiclePos(const Vehicle *v)
{
	static const int8 _delta_coord[16] = {
		-1,-1,-1, 0, 1, 1, 1, 0, /* x */
		-1, 0, 1, 1, 1, 0,-1,-1, /* y */
	};

	int x = v->x_pos + _delta_coord[v->direction];
	int y = v->y_pos + _delta_coord[v->direction + 8];

	GetNewVehiclePosResult gp;
	gp.x = x;
	gp.y = y;
	gp.old_tile = v->tile;
	gp.new_tile = TileVirtXY(x, y);
	return gp;
}

static const Direction _new_direction_table[] = {
	DIR_N , DIR_NW, DIR_W ,
	DIR_NE, DIR_SE, DIR_SW,
	DIR_E , DIR_SE, DIR_S
};

Direction GetDirectionTowards(const Vehicle *v, int x, int y)
{
	int i = 0;

	if (y >= v->y_pos) {
		if (y != v->y_pos) i += 3;
		i += 3;
	}

	if (x >= v->x_pos) {
		if (x != v->x_pos) i++;
		i++;
	}

	Direction dir = v->direction;

	DirDiff dirdiff = DirDifference(_new_direction_table[i], dir);
	if (dirdiff == DIRDIFF_SAME) return dir;
	return ChangeDir(dir, dirdiff > DIRDIFF_REVERSE ? DIRDIFF_45LEFT : DIRDIFF_45RIGHT);
}

Trackdir GetVehicleTrackdir(const Vehicle *v)
{
	if (v->vehstatus & VS_CRASHED) return INVALID_TRACKDIR;

	switch (v->type) {
		case VEH_TRAIN:
			if (v->u.rail.track == TRACK_BIT_DEPOT) // We'll assume the train is facing outwards
				return DiagDirToDiagTrackdir(GetRailDepotDirection(v->tile)); // Train in depot

			if (v->u.rail.track == TRACK_BIT_WORMHOLE) // train in tunnel or on bridge, so just use his direction and assume a diagonal track
				return DiagDirToDiagTrackdir(DirToDiagDir(v->direction));

			return TrackDirectionToTrackdir(FindFirstTrack(v->u.rail.track), v->direction);

		case VEH_SHIP:
			if (v->IsInDepot())
				/* We'll assume the ship is facing outwards */
				return DiagDirToDiagTrackdir(GetShipDepotDirection(v->tile));

			if (v->u.ship.state == TRACK_BIT_WORMHOLE) // ship on aqueduct, so just use his direction and assume a diagonal track
				return DiagDirToDiagTrackdir(DirToDiagDir(v->direction));

			return TrackDirectionToTrackdir(FindFirstTrack(v->u.ship.state), v->direction);

		case VEH_ROAD:
			if (v->IsInDepot()) // We'll assume the road vehicle is facing outwards
				return DiagDirToDiagTrackdir(GetRoadDepotDirection(v->tile));

			if (IsStandardRoadStopTile(v->tile)) // We'll assume the road vehicle is facing outwards
				return DiagDirToDiagTrackdir(GetRoadStopDir(v->tile)); // Road vehicle in a station

			if (IsDriveThroughStopTile(v->tile)) return DiagDirToDiagTrackdir(DirToDiagDir(v->direction));

			/* If vehicle's state is a valid track direction (vehicle is not turning around) return it */
			if (!IsReversingRoadTrackdir((Trackdir)v->u.road.state)) return (Trackdir)v->u.road.state;

			/* Vehicle is turning around, get the direction from vehicle's direction */
			return DiagDirToDiagTrackdir(DirToDiagDir(v->direction));

		/* case VEH_AIRCRAFT: case VEH_EFFECT: case VEH_DISASTER: */
		default: return INVALID_TRACKDIR;
	}
}

/**
 * Call the tile callback function for a vehicle entering a tile
 * @param v    Vehicle entering the tile
 * @param tile Tile entered
 * @param x    X position
 * @param y    Y position
 * @return Some meta-data over the to be entered tile.
 * @see VehicleEnterTileStatus to see what the bits in the return value mean.
 */
VehicleEnterTileStatus VehicleEnterTile(Vehicle *v, TileIndex tile, int x, int y)
{
	return _tile_type_procs[GetTileType(tile)]->vehicle_enter_tile_proc(v, tile, x, y);
}

FreeUnitIDGenerator::FreeUnitIDGenerator(VehicleType type, CompanyID owner) : cache(NULL), maxid(0), curid(0)
{
	/* Find maximum */
	const Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		if (v->type == type && v->owner == owner) {
			this->maxid = max<UnitID>(this->maxid, v->unitnumber);
		}
	}

	if (this->maxid == 0) return;

	this->maxid++; // so there is space for last item (with v->unitnumber == maxid)
	this->maxid++; // this one will always be free (well, it will fail when there are 65535 units, so this overflows)

	this->cache = CallocT<bool>(this->maxid);

	/* Fill the cache */
	FOR_ALL_VEHICLES(v) {
		if (v->type == type && v->owner == owner) {
			this->cache[v->unitnumber] = true;
		}
	}
}

UnitID FreeUnitIDGenerator::NextID()
{
	if (this->maxid <= this->curid) return ++this->curid;

	while (this->cache[++this->curid]) { } // it will stop, we reserved more space than needed

	return this->curid;
}

UnitID GetFreeUnitNumber(VehicleType type)
{
	FreeUnitIDGenerator gen(type, _current_company);

	return gen.NextID();
}


/**
 * Check whether we can build infrastructure for the given
 * vehicle type. This to disable building stations etc. when
 * you are not allowed/able to have the vehicle type yet.
 * @param type the vehicle type to check this for
 * @return true if there is any reason why you may build
 *         the infrastructure for the given vehicle type
 */
bool CanBuildVehicleInfrastructure(VehicleType type)
{
	assert(IsCompanyBuildableVehicleType(type));

	if (!IsValidCompanyID(_local_company)) return false;
	if (_settings_client.gui.always_build_infrastructure) return true;

	UnitID max;
	switch (type) {
		case VEH_TRAIN:    max = _settings_game.vehicle.max_trains; break;
		case VEH_ROAD:     max = _settings_game.vehicle.max_roadveh; break;
		case VEH_SHIP:     max = _settings_game.vehicle.max_ships; break;
		case VEH_AIRCRAFT: max = _settings_game.vehicle.max_aircraft; break;
		default: NOT_REACHED();
	}

	/* We can build vehicle infrastructure when we may build the vehicle type */
	if (max > 0) {
		/* Can we actually build the vehicle type? */
		const Engine *e;
		FOR_ALL_ENGINES_OF_TYPE(e, type) {
			if (HasBit(e->company_avail, _local_company)) return true;
		}
		return false;
	}

	/* We should be able to build infrastructure when we have the actual vehicle type */
	const Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		if (v->owner == _local_company && v->type == type) return true;
	}

	return false;
}


const Livery *GetEngineLivery(EngineID engine_type, CompanyID company, EngineID parent_engine_type, const Vehicle *v)
{
	const Company *c = GetCompany(company);
	LiveryScheme scheme = LS_DEFAULT;
	CargoID cargo_type = v == NULL ? (CargoID)CT_INVALID : v->cargo_type;

	/* The default livery is always available for use, but its in_use flag determines
	 * whether any _other_ liveries are in use. */
	if (c->livery[LS_DEFAULT].in_use && (_settings_client.gui.liveries == 2 || (_settings_client.gui.liveries == 1 && company == _local_company))) {
		/* Determine the livery scheme to use */
		const Engine *e = GetEngine(engine_type);
		switch (e->type) {
			default: NOT_REACHED();
			case VEH_TRAIN: {
				const RailVehicleInfo *rvi = RailVehInfo(engine_type);
				if (v != NULL && parent_engine_type != INVALID_ENGINE && (UsesWagonOverride(v) || (IsArticulatedPart(v) && rvi->railveh_type != RAILVEH_WAGON))) {
					/* Wagonoverrides use the coloir scheme of the front engine.
					 * Articulated parts use the colour scheme of the first part. (Not supported for articulated wagons) */
					engine_type = parent_engine_type;
					e = GetEngine(engine_type);
					rvi = RailVehInfo(engine_type);
					/* Note: Luckily cargo_type is not needed for engines */
				}

				if (cargo_type == CT_INVALID) cargo_type = e->GetDefaultCargoType();
				if (cargo_type == CT_INVALID) cargo_type = CT_GOODS; // The vehicle does not carry anything, let's pick some freight cargo
				if (rvi->railveh_type == RAILVEH_WAGON) {
					if (!GetCargo(cargo_type)->is_freight) {
						if (parent_engine_type == INVALID_ENGINE) {
							scheme = LS_PASSENGER_WAGON_STEAM;
						} else {
							switch (RailVehInfo(parent_engine_type)->engclass) {
								default: NOT_REACHED();
								case EC_STEAM:    scheme = LS_PASSENGER_WAGON_STEAM;    break;
								case EC_DIESEL:   scheme = LS_PASSENGER_WAGON_DIESEL;   break;
								case EC_ELECTRIC: scheme = LS_PASSENGER_WAGON_ELECTRIC; break;
								case EC_MONORAIL: scheme = LS_PASSENGER_WAGON_MONORAIL; break;
								case EC_MAGLEV:   scheme = LS_PASSENGER_WAGON_MAGLEV;   break;
							}
						}
					} else {
						scheme = LS_FREIGHT_WAGON;
					}
				} else {
					bool is_mu = HasBit(EngInfo(engine_type)->misc_flags, EF_RAIL_IS_MU);

					switch (rvi->engclass) {
						default: NOT_REACHED();
						case EC_STEAM:    scheme = LS_STEAM; break;
						case EC_DIESEL:   scheme = is_mu ? LS_DMU : LS_DIESEL;   break;
						case EC_ELECTRIC: scheme = is_mu ? LS_EMU : LS_ELECTRIC; break;
						case EC_MONORAIL: scheme = LS_MONORAIL; break;
						case EC_MAGLEV:   scheme = LS_MAGLEV; break;
					}
				}
				break;
			}

			case VEH_ROAD: {
				/* Always use the livery of the front */
				if (v != NULL && parent_engine_type != INVALID_ENGINE) {
					engine_type = parent_engine_type;
					e = GetEngine(engine_type);
					cargo_type = v->First()->cargo_type;
				}
				if (cargo_type == CT_INVALID) cargo_type = e->GetDefaultCargoType();
				if (cargo_type == CT_INVALID) cargo_type = CT_GOODS; // The vehicle does not carry anything, let's pick some freight cargo

				/* Important: Use Tram Flag of front part. Luckily engine_type refers to the front part here. */
				if (HasBit(EngInfo(engine_type)->misc_flags, EF_ROAD_TRAM)) {
					/* Tram */
					scheme = IsCargoInClass(cargo_type, CC_PASSENGERS) ? LS_PASSENGER_TRAM : LS_FREIGHT_TRAM;
				} else {
					/* Bus or truck */
					scheme = IsCargoInClass(cargo_type, CC_PASSENGERS) ? LS_BUS : LS_TRUCK;
				}
				break;
			}

			case VEH_SHIP: {
				if (cargo_type == CT_INVALID) cargo_type = e->GetDefaultCargoType();
				if (cargo_type == CT_INVALID) cargo_type = CT_GOODS; // The vehicle does not carry anything, let's pick some freight cargo
				scheme = IsCargoInClass(cargo_type, CC_PASSENGERS) ? LS_PASSENGER_SHIP : LS_FREIGHT_SHIP;
				break;
			}

			case VEH_AIRCRAFT: {
				switch (e->u.air.subtype) {
					case AIR_HELI: scheme = LS_HELICOPTER; break;
					case AIR_CTOL: scheme = LS_SMALL_PLANE; break;
					case AIR_CTOL | AIR_FAST: scheme = LS_LARGE_PLANE; break;
				}
				break;
			}
		}

		/* Switch back to the default scheme if the resolved scheme is not in use */
		if (!c->livery[scheme].in_use) scheme = LS_DEFAULT;
	}

	return &c->livery[scheme];
}


static SpriteID GetEngineColourMap(EngineID engine_type, CompanyID company, EngineID parent_engine_type, const Vehicle *v)
{
	SpriteID map = (v != NULL) ? v->colourmap : PAL_NONE;

	/* Return cached value if any */
	if (map != PAL_NONE) return map;

	/* Check if we should use the colour map callback */
	if (HasBit(EngInfo(engine_type)->callbackmask, CBM_VEHICLE_COLOUR_REMAP)) {
		uint16 callback = GetVehicleCallback(CBID_VEHICLE_COLOUR_MAPPING, 0, 0, engine_type, v);
		/* A return value of 0xC000 is stated to "use the default two-colour
		 * maps" which happens to be the failure action too... */
		if (callback != CALLBACK_FAILED && callback != 0xC000) {
			map = GB(callback, 0, 14);
			/* If bit 14 is set, then the company colours are applied to the
			 * map else it's returned as-is. */
			if (!HasBit(callback, 14)) {
				/* Update cache */
				if (v != NULL) ((Vehicle*)v)->colourmap = map;
				return map;
			}
		}
	}

	bool twocc = HasBit(EngInfo(engine_type)->misc_flags, EF_USES_2CC);

	if (map == PAL_NONE) map = twocc ? (SpriteID)SPR_2CCMAP_BASE : (SpriteID)PALETTE_RECOLOUR_START;

	const Livery *livery = GetEngineLivery(engine_type, company, parent_engine_type, v);

	map += livery->colour1;
	if (twocc) map += livery->colour2 * 16;

	/* Update cache */
	if (v != NULL) ((Vehicle*)v)->colourmap = map;
	return map;
}

SpriteID GetEnginePalette(EngineID engine_type, CompanyID company)
{
	return GetEngineColourMap(engine_type, company, INVALID_ENGINE, NULL);
}

SpriteID GetVehiclePalette(const Vehicle *v)
{
	if (v->type == VEH_TRAIN) {
		return GetEngineColourMap(v->engine_type, v->owner, v->u.rail.first_engine, v);
	} else if (v->type == VEH_ROAD) {
		return GetEngineColourMap(v->engine_type, v->owner, v->u.road.first_engine, v);
	}

	return GetEngineColourMap(v->engine_type, v->owner, INVALID_ENGINE, v);
}


void Vehicle::BeginLoading()
{
	assert(IsTileType(tile, MP_STATION) || type == VEH_SHIP);

	if (this->current_order.IsType(OT_GOTO_STATION) &&
			this->current_order.GetDestination() == this->last_station_visited) {
		current_order.MakeLoading(true);
		UpdateVehicleTimetable(this, true);

		/* Furthermore add the Non Stop flag to mark that this station
		 * is the actual destination of the vehicle, which is (for example)
		 * necessary to be known for HandleTrainLoading to determine
		 * whether the train is lost or not; not marking a train lost
		 * that arrives at random stations is bad. */
		this->current_order.SetNonStopType(ONSF_NO_STOP_AT_ANY_STATION);

	} else {
		current_order.MakeLoading(false);
	}

	GetStation(this->last_station_visited)->loading_vehicles.push_back(this);

	VehiclePayment(this);

	InvalidateWindow(GetWindowClassForVehicleType(this->type), this->owner);
	InvalidateWindowWidget(WC_VEHICLE_VIEW, this->index, VVW_WIDGET_START_STOP_VEH);
	InvalidateWindow(WC_VEHICLE_DETAILS, this->index);
	InvalidateWindow(WC_STATION_VIEW, this->last_station_visited);

	GetStation(this->last_station_visited)->MarkTilesDirty(true);
	this->MarkDirty();
}

void Vehicle::LeaveStation()
{
	assert(current_order.IsType(OT_LOADING));

	/* Only update the timetable if the vehicle was supposed to stop here. */
	if (current_order.GetNonStopType() != ONSF_STOP_EVERYWHERE) UpdateVehicleTimetable(this, false);

	current_order.MakeLeaveStation();
	Station *st = GetStation(this->last_station_visited);
	st->loading_vehicles.remove(this);

	HideFillingPercent(&this->fill_percent_te_id);

	if (this->type == VEH_TRAIN) {
		/* Trigger station animation (trains only) */
		if (IsTileType(this->tile, MP_STATION)) StationAnimationTrigger(st, this->tile, STAT_ANIM_TRAIN_DEPARTS);

		/* Try to reserve a path when leaving the station as we
		 * might not be marked as wanting a reservation, e.g.
		 * when an overlength train gets turned around in a station. */
		if (UpdateSignalsOnSegment(this->tile, TrackdirToExitdir(GetVehicleTrackdir(this)), this->owner) == SIGSEG_PBS || _settings_game.pf.reserve_paths) {
			TryPathReserve(this, true, true);
		}
	}
}


void Vehicle::HandleLoading(bool mode)
{
	switch (this->current_order.GetType()) {
		case OT_LOADING: {
			uint wait_time = max(this->current_order.wait_time - this->lateness_counter, 0);

			/* Not the first call for this tick, or still loading */
			if (mode || !HasBit(this->vehicle_flags, VF_LOADING_FINISHED) ||
					(_settings_game.order.timetabling && this->current_order_time < wait_time)) return;

			this->PlayLeaveStationSound();

			bool at_destination_station = this->current_order.GetNonStopType() != ONSF_STOP_EVERYWHERE;
			this->LeaveStation();

			/* If this was not the final order, don't remove it from the list. */
			if (!at_destination_station) return;
			break;
		}

		case OT_DUMMY: break;

		default: return;
	}

	this->cur_order_index++;
	InvalidateVehicleOrder(this, 0);
}

CommandCost Vehicle::SendToDepot(DoCommandFlag flags, DepotCommand command)
{
	if (!CheckOwnership(this->owner)) return CMD_ERROR;
	if (this->vehstatus & VS_CRASHED) return CMD_ERROR;
	if (this->IsStoppedInDepot()) return CMD_ERROR;

	if (this->current_order.IsType(OT_GOTO_DEPOT)) {
		bool halt_in_depot = this->current_order.GetDepotActionType() & ODATFB_HALT;
		if (!!(command & DEPOT_SERVICE) == halt_in_depot) {
			/* We called with a different DEPOT_SERVICE setting.
			 * Now we change the setting to apply the new one and let the vehicle head for the same depot.
			 * Note: the if is (true for requesting service == true for ordered to stop in depot)          */
			if (flags & DC_EXEC) {
				this->current_order.SetDepotOrderType(ODTF_MANUAL);
				this->current_order.SetDepotActionType(halt_in_depot ? ODATF_SERVICE_ONLY : ODATFB_HALT);
				InvalidateWindowWidget(WC_VEHICLE_VIEW, this->index, VVW_WIDGET_START_STOP_VEH);
			}
			return CommandCost();
		}

		if (command & DEPOT_DONT_CANCEL) return CMD_ERROR; // Requested no cancelation of depot orders
		if (flags & DC_EXEC) {
			/* If the orders to 'goto depot' are in the orders list (forced servicing),
			 * then skip to the next order; effectively cancelling this forced service */
			if (this->current_order.GetDepotOrderType() & ODTFB_PART_OF_ORDERS) this->cur_order_index++;

			this->current_order.MakeDummy();
			InvalidateWindowWidget(WC_VEHICLE_VIEW, this->index, VVW_WIDGET_START_STOP_VEH);
		}
		return CommandCost();
	}

	TileIndex location;
	DestinationID destination;
	bool reverse;
	static const StringID no_depot[] = {STR_883A_UNABLE_TO_FIND_ROUTE_TO, STR_9019_UNABLE_TO_FIND_LOCAL_DEPOT, STR_981A_UNABLE_TO_FIND_LOCAL_DEPOT, STR_A012_CAN_T_SEND_AIRCRAFT_TO};
	if (!this->FindClosestDepot(&location, &destination, &reverse)) return_cmd_error(no_depot[this->type]);

	if (flags & DC_EXEC) {
		if (this->current_order.IsType(OT_LOADING)) this->LeaveStation();

		this->dest_tile = location;
		this->current_order.MakeGoToDepot(destination, ODTF_MANUAL);
		if (!(command & DEPOT_SERVICE)) this->current_order.SetDepotActionType(ODATFB_HALT);
		InvalidateWindowWidget(WC_VEHICLE_VIEW, this->index, VVW_WIDGET_START_STOP_VEH);

		/* If there is no depot in front, reverse automatically (trains only) */
		if (this->type == VEH_TRAIN && reverse) DoCommand(this->tile, this->index, 0, DC_EXEC, CMD_REVERSE_TRAIN_DIRECTION);

		if (this->type == VEH_AIRCRAFT && this->u.air.state == FLYING && this->u.air.targetairport != destination) {
			/* The aircraft is now heading for a different hangar than the next in the orders */
			extern void AircraftNextAirportPos_and_Order(Vehicle *v);
			AircraftNextAirportPos_and_Order(this);
		}
	}

	return CommandCost();

}

void Vehicle::SetNext(Vehicle *next)
{
	if (this->next != NULL) {
		/* We had an old next vehicle. Update the first and previous pointers */
		for (Vehicle *v = this->next; v != NULL; v = v->Next()) {
			v->first = this->next;
		}
		this->next->previous = NULL;
	}

	this->next = next;

	if (this->next != NULL) {
		/* A new next vehicle. Update the first and previous pointers */
		if (this->next->previous != NULL) this->next->previous->next = NULL;
		this->next->previous = this;
		for (Vehicle *v = this->next; v != NULL; v = v->Next()) {
			v->first = this->first;
		}
	}
}

void Vehicle::AddToShared(Vehicle *shared_chain)
{
	assert(this->previous_shared == NULL && this->next_shared == NULL);

	if (!shared_chain->orders.list) {
		assert(shared_chain->previous_shared == NULL);
		assert(shared_chain->next_shared == NULL);
		this->orders.list = shared_chain->orders.list = new OrderList(NULL, shared_chain);
	}

	this->next_shared     = shared_chain->next_shared;
	this->previous_shared = shared_chain;

	shared_chain->next_shared = this;

	if (this->next_shared != NULL) this->next_shared->previous_shared = this;

	shared_chain->orders.list->AddVehicle(this);
}

void Vehicle::RemoveFromShared()
{
	/* Remember if we were first and the old window number before RemoveVehicle()
	 * as this changes first if needed. */
	bool were_first = (this->FirstShared() == this);
	uint32 old_window_number = (this->FirstShared()->index << 16) | (this->type << 11) | VLW_SHARED_ORDERS | this->owner;

	this->orders.list->RemoveVehicle(this);

	if (!were_first) {
		/* We are not the first shared one, so only relink our previous one. */
		this->previous_shared->next_shared = this->NextShared();
	}

	if (this->next_shared != NULL) this->next_shared->previous_shared = this->previous_shared;


	if (this->orders.list->GetNumVehicles() == 1) {
		/* When there is only one vehicle, remove the shared order list window. */
		DeleteWindowById(GetWindowClassForVehicleType(this->type), old_window_number);
		InvalidateVehicleOrder(this->FirstShared(), 0);
	} else if (were_first) {
		/* If we were the first one, update to the new first one.
		 * Note: FirstShared() is already the new first */
		InvalidateWindowData(GetWindowClassForVehicleType(this->type), old_window_number, (this->FirstShared()->index << 16) | (1 << 15));
	}

	this->next_shared     = NULL;
	this->previous_shared = NULL;
}

void StopAllVehicles()
{
	Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		/* Code ripped from CmdStartStopTrain. Can't call it, because of
		 * ownership problems, so we'll duplicate some code, for now */
		v->vehstatus |= VS_STOPPED;
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, VVW_WIDGET_START_STOP_VEH);
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
	}
}

void VehiclesYearlyLoop()
{
	Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		if (v->IsPrimaryVehicle()) {
			/* show warning if vehicle is not generating enough income last 2 years (corresponds to a red icon in the vehicle list) */
			Money profit = v->GetDisplayProfitThisYear();
			if (v->age >= 730 && profit < 0) {
				if (_settings_client.gui.vehicle_income_warn && v->owner == _local_company) {
					SetDParam(0, v->index);
					SetDParam(1, profit);
					AddNewsItem(
						STR_VEHICLE_IS_UNPROFITABLE,
						NS_ADVICE,
						v->index,
						0);
				}
				AI::NewEvent(v->owner, new AIEventVehicleUnprofitable(v->index));
			}

			v->profit_last_year = v->profit_this_year;
			v->profit_this_year = 0;
			InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
		}
	}
}


/**
 * Can this station be used by the given engine type?
 * @param engine_type the type of vehicles to test
 * @param st the station to test for
 * @return true if and only if the vehicle of the type can use this station.
 * @note For road vehicles the Vehicle is needed to determine whether it can
 *       use the station. This function will return true for road vehicles
 *       when at least one of the facilities is available.
 */
bool CanVehicleUseStation(EngineID engine_type, const Station *st)
{
	assert(IsEngineIndex(engine_type));
	const Engine *e = GetEngine(engine_type);

	switch (e->type) {
		case VEH_TRAIN:
			return (st->facilities & FACIL_TRAIN) != 0;

		case VEH_ROAD:
			/* For road vehicles we need the vehicle to know whether it can actually
			 * use the station, but if it doesn't have facilities for RVs it is
			 * certainly not possible that the station can be used. */
			return (st->facilities & (FACIL_BUS_STOP | FACIL_TRUCK_STOP)) != 0;

		case VEH_SHIP:
			return (st->facilities & FACIL_DOCK) != 0;

		case VEH_AIRCRAFT:
			return (st->facilities & FACIL_AIRPORT) != 0 &&
					(st->Airport()->flags & (e->u.air.subtype & AIR_CTOL ? AirportFTAClass::AIRPLANES : AirportFTAClass::HELICOPTERS)) != 0;

		default:
			return false;
	}
}

/**
 * Can this station be used by the given vehicle?
 * @param v the vehicle to test
 * @param st the station to test for
 * @return true if and only if the vehicle can use this station.
 */
bool CanVehicleUseStation(const Vehicle *v, const Station *st)
{
	if (v->type == VEH_ROAD) return st->GetPrimaryRoadStop(v) != NULL;

	return CanVehicleUseStation(v->engine_type, st);
}
