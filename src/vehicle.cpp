/* $Id$ */

/** @file vehicle.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "road_map.h"
#include "roadveh.h"
#include "ship.h"
#include "spritecache.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "functions.h"
#include "landscape.h"
#include "map.h"
#include "tile.h"
#include "vehicle.h"
#include "gfx.h"
#include "viewport.h"
#include "news.h"
#include "command.h"
#include "saveload.h"
#include "player.h"
#include "engine.h"
#include "sound.h"
#include "debug.h"
#include "vehicle_gui.h"
#include "depot.h"
#include "station.h"
#include "rail.h"
#include "train.h"
#include "aircraft.h"
#include "industry_map.h"
#include "station_map.h"
#include "water_map.h"
#include "network/network.h"
#include "yapf/yapf.h"
#include "date.h"
#include "newgrf_callbacks.h"
#include "newgrf_engine.h"
#include "newgrf_sound.h"
#include "helpers.hpp"
#include "economy.h"

#define INVALID_COORD (-0x8000)
#define GEN_HASH(x, y) ((GB((y), 6, 6) << 6) + GB((x), 7, 6))


/* Tables used in vehicle.h to find the right command for a certain vehicle type */
const uint32 _veh_build_proc_table[] = {
	CMD_BUILD_RAIL_VEHICLE,
	CMD_BUILD_ROAD_VEH,
	CMD_BUILD_SHIP,
	CMD_BUILD_AIRCRAFT,
};
const uint32 _veh_sell_proc_table[] = {
	CMD_SELL_RAIL_WAGON,
	CMD_SELL_ROAD_VEH,
	CMD_SELL_SHIP,
	CMD_SELL_AIRCRAFT,
};

const uint32 _veh_refit_proc_table[] = {
	CMD_REFIT_RAIL_VEHICLE,
	CMD_REFIT_ROAD_VEH,
	CMD_REFIT_SHIP,
	CMD_REFIT_AIRCRAFT,
};

const uint32 _send_to_depot_proc_table[] = {
	CMD_SEND_TRAIN_TO_DEPOT,
	CMD_SEND_ROADVEH_TO_DEPOT,
	CMD_SEND_SHIP_TO_DEPOT,
	CMD_SEND_AIRCRAFT_TO_HANGAR,
};


enum {
	BLOCKS_FOR_SPECIAL_VEHICLES   = 2, ///< Blocks needed for special vehicles
};

/**
 * Called if a new block is added to the vehicle-pool
 */
static void VehiclePoolNewBlock(uint start_item)
{
	Vehicle *v;

	/* We don't use FOR_ALL here, because FOR_ALL skips invalid items.
	 * TODO - This is just a temporary stage, this will be removed. */
	for (v = GetVehicle(start_item); v != NULL; v = (v->index + 1U < GetVehiclePoolSize()) ? GetVehicle(v->index + 1) : NULL) {
		v->index = start_item++;
		v = new (v) InvalidVehicle();
	}
}

/* Initialize the vehicle-pool */
DEFINE_OLD_POOL(Vehicle, Vehicle, VehiclePoolNewBlock, NULL)

void VehicleServiceInDepot(Vehicle *v)
{
	v->date_of_last_service = _date;
	v->breakdowns_since_last_service = 0;
	v->reliability = GetEngine(v->engine_type)->reliability;
	InvalidateWindow(WC_VEHICLE_DETAILS, v->index); // ensure that last service date and reliability are updated
}

bool VehicleNeedsService(const Vehicle *v)
{
	if (v->vehstatus & VS_CRASHED)
		return false; // Crashed vehicles don't need service anymore

	if (_patches.no_servicing_if_no_breakdowns && _opt.diff.vehicle_breakdowns == 0) {
		return EngineHasReplacementForPlayer(GetPlayer(v->owner), v->engine_type);  /* Vehicles set for autoreplacing needs to go to a depot even if breakdowns are turned off */
	}

	return _patches.servint_ispercent ?
		(v->reliability < GetEngine(v->engine_type)->reliability * (100 - v->service_interval) / 100) :
		(v->date_of_last_service + v->service_interval < _date);
}

StringID VehicleInTheWayErrMsg(const Vehicle* v)
{
	switch (v->type) {
		case VEH_TRAIN:    return STR_8803_TRAIN_IN_THE_WAY;
		case VEH_ROAD:     return STR_9000_ROAD_VEHICLE_IN_THE_WAY;
		case VEH_AIRCRAFT: return STR_A015_AIRCRAFT_IN_THE_WAY;
		default:           return STR_980E_SHIP_IN_THE_WAY;
	}
}

static void *EnsureNoVehicleProc(Vehicle *v, void *data)
{
	if (v->tile != *(const TileIndex*)data || v->type == VEH_DISASTER)
		return NULL;

	_error_message = VehicleInTheWayErrMsg(v);
	return v;
}

bool EnsureNoVehicle(TileIndex tile)
{
	return VehicleFromPos(tile, &tile, EnsureNoVehicleProc) == NULL;
}

static void *EnsureNoVehicleProcZ(Vehicle *v, void *data)
{
	const TileInfo *ti = (const TileInfo*)data;

	if (v->tile != ti->tile || v->type == VEH_DISASTER) return NULL;
	if (v->z_pos > ti->z) return NULL;

	_error_message = VehicleInTheWayErrMsg(v);
	return v;
}


bool EnsureNoVehicleOnGround(TileIndex tile)
{
	TileInfo ti;

	ti.tile = tile;
	ti.z = GetTileMaxZ(tile);
	return VehicleFromPos(tile, &ti, EnsureNoVehicleProcZ) == NULL;
}

Vehicle *FindVehicleOnTileZ(TileIndex tile, byte z)
{
	TileInfo ti;

	ti.tile = tile;
	ti.z = z;

	return (Vehicle*)VehicleFromPos(tile, &ti, EnsureNoVehicleProcZ);
}

Vehicle *FindVehicleBetween(TileIndex from, TileIndex to, byte z, bool without_crashed)
{
	int x1 = TileX(from);
	int y1 = TileY(from);
	int x2 = TileX(to);
	int y2 = TileY(to);
	Vehicle *veh;

	/* Make sure x1 < x2 or y1 < y2 */
	if (x1 > x2 || y1 > y2) {
		Swap(x1, x2);
		Swap(y1, y2);
	}
	FOR_ALL_VEHICLES(veh) {
		if (without_crashed && (veh->vehstatus & VS_CRASHED) != 0) continue;
		if ((veh->type == VEH_TRAIN || veh->type == VEH_ROAD) && (z == 0xFF || veh->z_pos == z)) {
			if ((veh->x_pos >> 4) >= x1 && (veh->x_pos >> 4) <= x2 &&
					(veh->y_pos >> 4) >= y1 && (veh->y_pos >> 4) <= y2) {
				return veh;
			}
		}
	}
	return NULL;
}


static void UpdateVehiclePosHash(Vehicle* v, int x, int y);

void VehiclePositionChanged(Vehicle *v)
{
	int img = v->cur_image;
	Point pt = RemapCoords(v->x_pos + v->x_offs, v->y_pos + v->y_offs, v->z_pos);
	const Sprite* spr = GetSprite(img);

	pt.x += spr->x_offs;
	pt.y += spr->y_offs;

	UpdateVehiclePosHash(v, pt.x, pt.y);

	v->left_coord = pt.x;
	v->top_coord = pt.y;
	v->right_coord = pt.x + spr->width + 2;
	v->bottom_coord = pt.y + spr->height + 2;
}

/** Called after load to update coordinates */
void AfterLoadVehicles()
{
	Vehicle *v;

	FOR_ALL_VEHICLES(v) {
		v->UpdateDeltaXY(v->direction);

		v->first = NULL;
		if (v->type == VEH_TRAIN) v->u.rail.first_engine = INVALID_ENGINE;
	}

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_TRAIN && (IsFrontEngine(v) || IsFreeWagon(v)))
			TrainConsistChanged(v);
	}

	FOR_ALL_VEHICLES(v) {
		switch (v->type) {
			case VEH_TRAIN: v->cur_image = GetTrainImage(v, v->direction); break;
			case VEH_ROAD: v->cur_image = GetRoadVehImage(v, v->direction); break;
			case VEH_SHIP: v->cur_image = GetShipImage(v, v->direction); break;
			case VEH_AIRCRAFT:
				if (IsNormalAircraft(v)) {
					v->cur_image = GetAircraftImage(v, v->direction);

					/* The plane's shadow will have the same image as the plane */
					Vehicle *shadow = v->next;
					shadow->cur_image = v->cur_image;

					/* In the case of a helicopter we will update the rotor sprites */
					if (v->subtype == AIR_HELICOPTER) {
						Vehicle *rotor = shadow->next;
						rotor->cur_image = GetRotorImage(v);
					}

					UpdateAircraftCache(v);
				}
				break;
			default: break;
		}

		v->left_coord = INVALID_COORD;
		VehiclePositionChanged(v);
	}
}

static Vehicle *InitializeVehicle(Vehicle *v)
{
	VehicleID index = v->index;
	memset(v, 0, sizeof(Vehicle));
	v->index = index;

	assert(v->orders == NULL);

	v = new (v) InvalidVehicle();
	v->left_coord = INVALID_COORD;
	v->first = NULL;
	v->next = NULL;
	v->next_hash = NULL;
	v->string_id = 0;
	v->next_shared = NULL;
	v->prev_shared = NULL;
	v->depot_list  = NULL;
	v->random_bits = 0;
	return v;
}

/**
 * Get a value for a vehicle's random_bits.
 * @return A random value from 0 to 255.
 */
byte VehicleRandomBits()
{
	return GB(Random(), 0, 8);
}

Vehicle *ForceAllocateSpecialVehicle()
{
	/* This stays a strange story.. there should always be room for special
	 * vehicles (special effects all over the map), but with 65k of vehicles
	 * is this realistic to double-check for that? For now we just reserve
	 * BLOCKS_FOR_SPECIAL_VEHICLES times block_size vehicles that may only
	 * be used for special vehicles.. should work nicely :) */

	Vehicle *v;

	/* We don't use FOR_ALL here, because FOR_ALL skips invalid items.
	 * TODO - This is just a temporary stage, this will be removed. */
	for (v = GetVehicle(0); v != NULL; v = (v->index + 1U < GetVehiclePoolSize()) ? GetVehicle(v->index + 1) : NULL) {
		/* No more room for the special vehicles, return NULL */
		if (v->index >= (1 << Vehicle_POOL_BLOCK_SIZE_BITS) * BLOCKS_FOR_SPECIAL_VEHICLES)
			return NULL;

		if (!IsValidVehicle(v)) return InitializeVehicle(v);
	}

	return NULL;
}

/**
 * finds a free vehicle in the memory or allocates a new one
 * returns a pointer to the first free vehicle or NULL if all vehicles are in use
 * *skip_vehicles is an offset to where in the array we should begin looking
 * this is to avoid looping though the same vehicles more than once after we learned that they are not free
 * this feature is used by AllocateVehicles() since it need to allocate more than one and when
 * another block is added to _Vehicle_pool, since we only do that when we know it's already full
 */
static Vehicle *AllocateSingleVehicle(VehicleID *skip_vehicles)
{
	/* See note by ForceAllocateSpecialVehicle() why we skip the
	 * first blocks */
	Vehicle *v;
	const int offset = (1 << Vehicle_POOL_BLOCK_SIZE_BITS) * BLOCKS_FOR_SPECIAL_VEHICLES;

	/* We don't use FOR_ALL here, because FOR_ALL skips invalid items.
	 * @todo - This is just a temporary stage, this will be removed. */
	if (*skip_vehicles < (_Vehicle_pool.total_items - offset)) { // make sure the offset in the array is not larger than the array itself
		for (v = GetVehicle(offset + *skip_vehicles); v != NULL; v = (v->index + 1U < GetVehiclePoolSize()) ? GetVehicle(v->index + 1) : NULL) {
			(*skip_vehicles)++;
			if (!IsValidVehicle(v)) return InitializeVehicle(v);
		}
	}

	/* Check if we can add a block to the pool */
	if (AddBlockToPool(&_Vehicle_pool))
		return AllocateSingleVehicle(skip_vehicles);

	return NULL;
}


Vehicle *AllocateVehicle()
{
	VehicleID counter = 0;
	return AllocateSingleVehicle(&counter);
}


/** Allocates a lot of vehicles and frees them again
 * @param vl pointer to an array of vehicles to get allocated. Can be NULL if the vehicles aren't needed (makes it test only)
 * @param num number of vehicles to allocate room for
 * @return true if there is room to allocate all the vehicles
 */
bool AllocateVehicles(Vehicle **vl, int num)
{
	int i;
	Vehicle *v;
	VehicleID counter = 0;

	for (i = 0; i != num; i++) {
		v = AllocateSingleVehicle(&counter);
		if (v == NULL) {
			return false;
		}
		if (vl != NULL) {
			vl[i] = v;
		}
	}

	return true;
}


static Vehicle *_vehicle_position_hash[0x1000];

void *VehicleFromPos(TileIndex tile, void *data, VehicleFromPosProc *proc)
{
	Point pt = RemapCoords(TileX(tile) * TILE_SIZE, TileY(tile) * TILE_SIZE, 0);

	/* The hash area to scan */
	const int xl = GB(pt.x - 174, 7, 6);
	const int xu = GB(pt.x + 104, 7, 6);
	const int yl = GB(pt.y - 294, 6, 6) << 6;
	const int yu = GB(pt.y +  56, 6, 6) << 6;

	int x;
	int y;

	for (y = yl;; y = (y + (1 << 6)) & (0x3F << 6)) {
		for (x = xl;; x = (x + 1) & 0x3F) {
			Vehicle *v = _vehicle_position_hash[(x + y) & 0xFFFF];

			while (v != NULL) {
				void* a = proc(v, data);

				if (a != NULL) return a;
				v = v->next_hash;
			}

			if (x == xu) break;
		}

		if (y == yu) break;
	}
	return NULL;
}


static void UpdateVehiclePosHash(Vehicle* v, int x, int y)
{
	Vehicle **old_hash, **new_hash;
	int old_x = v->left_coord;
	int old_y = v->top_coord;

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
	memset(_vehicle_position_hash, 0, sizeof(_vehicle_position_hash));
}

void InitializeVehicles()
{
	uint i;

	/* Clean the vehicle pool, and reserve enough blocks
	 *  for the special vehicles, plus one for all the other
	 *  vehicles (which is increased on-the-fly) */
	CleanPool(&_Vehicle_pool);
	AddBlockToPool(&_Vehicle_pool);
	for (i = 0; i < BLOCKS_FOR_SPECIAL_VEHICLES; i++) {
		AddBlockToPool(&_Vehicle_pool);
	}

	ResetVehiclePosHash();
}

Vehicle *GetLastVehicleInChain(Vehicle *v)
{
	while (v->next != NULL) v = v->next;
	return v;
}

/** Finds the previous vehicle in a chain, by a brute force search.
 * This old function is REALLY slow because it searches through all vehicles to
 * find the previous vehicle, but if v->first has not been set, then this function
 * will need to be used to find the previous one. This function should never be
 * called by anything but GetFirstVehicleInChain
 */
static Vehicle *GetPrevVehicleInChain_bruteforce(const Vehicle *v)
{
	Vehicle *u;

	FOR_ALL_VEHICLES(u) if (u->type == VEH_TRAIN && u->next == v) return u;

	return NULL;
}

/** Find the previous vehicle in a chain, by using the v->first cache.
 * While this function is fast, it cannot be used in the GetFirstVehicleInChain
 * function, otherwise you'll end up in an infinite loop call
 */
Vehicle *GetPrevVehicleInChain(const Vehicle *v)
{
	Vehicle *u;
	assert(v != NULL);

	u = GetFirstVehicleInChain(v);

	/* Check to see if this is the first */
	if (v == u) return NULL;

	for (; u->next != v; u = u->next) assert(u->next != NULL);

	return u;
}

/** Finds the first vehicle in a chain.
 * This function reads out the v->first cache. Should the cache be dirty,
 * it determines the first vehicle in a chain, and updates the cache.
 */
Vehicle *GetFirstVehicleInChain(const Vehicle *v)
{
	Vehicle* u;

	assert(v != NULL);
	assert(v->type == VEH_TRAIN);

	if (v->first != NULL) {
		if (IsFrontEngine(v->first) || IsFreeWagon(v->first)) return v->first;

		DEBUG(misc, 0, "v->first cache faulty. We shouldn't be here, rebuilding cache!");
	}

	/* It is the fact (currently) that newly built vehicles do not have
	 * their ->first pointer set. When this is the case, go up to the
	 * first engine and set the pointers correctly. Also the first pointer
	 * is not saved in a savegame, so this has to be fixed up after loading */

	/* Find the 'locomotive' or the first wagon in a chain */
	while ((u = GetPrevVehicleInChain_bruteforce(v)) != NULL) v = u;

	/* Set the first pointer of all vehicles in that chain to the first wagon */
	if (IsFrontEngine(v) || IsFreeWagon(v))
		for (u = (Vehicle *)v; u != NULL; u = u->next) u->first = (Vehicle *)v;

	return (Vehicle*)v;
}

uint CountVehiclesInChain(const Vehicle* v)
{
	uint count = 0;
	do count++; while ((v = v->next) != NULL);
	return count;
}

/** Check if a vehicle is counted in num_engines in each player struct
 * @param *v Vehicle to test
 * @return true if the vehicle is counted in num_engines
 */
bool IsEngineCountable(const Vehicle *v)
{
	switch (v->type) {
		case VEH_AIRCRAFT: return IsNormalAircraft(v); // don't count plane shadows and helicopter rotors
		case VEH_TRAIN:
			return !IsArticulatedPart(v) && // tenders and other articulated parts
			(!IsMultiheaded(v) || IsTrainEngine(v)); // rear parts of multiheaded engines
		case VEH_ROAD:
		case VEH_SHIP:
			return true;
		default: return false; // Only count player buildable vehicles
	}
}

void DestroyVehicle(Vehicle *v)
{
	if (IsValidStationID(v->last_station_visited)) {
		GetStation(v->last_station_visited)->loading_vehicles.remove(v);
	}

	if (IsEngineCountable(v)) {
		GetPlayer(v->owner)->num_engines[v->engine_type]--;
		if (v->owner == _local_player) InvalidateAutoreplaceWindow(v->engine_type);
	}

	DeleteVehicleNews(v->index, INVALID_STRING_ID);

	DeleteName(v->string_id);
	if (v->type == VEH_ROAD) ClearSlot(v);

	if (v->type != VEH_TRAIN || (v->type == VEH_TRAIN && (IsFrontEngine(v) || IsFreeWagon(v)))) {
		InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
	}

	UpdateVehiclePosHash(v, INVALID_COORD, 0);
	v->next_hash = NULL;
	if (IsPlayerBuildableVehicleType(v)) DeleteVehicleOrders(v);

	/* Now remove any artic part. This will trigger an other
	 *  destroy vehicle, which on his turn can remove any
	 *  other artic parts. */
	if (EngineHasArticPart(v)) DeleteVehicle(v->next);
}

void DeleteVehicleChain(Vehicle *v)
{
	do {
		Vehicle *u = v;
		v = GetNextVehicle(v);
		DeleteVehicle(u);
	} while (v != NULL);
}


void Aircraft_Tick(Vehicle *v);
void RoadVeh_Tick(Vehicle *v);
void Ship_Tick(Vehicle *v);
void Train_Tick(Vehicle *v);
static void EffectVehicle_Tick(Vehicle *v);
void DisasterVehicle_Tick(Vehicle *v);

/** head of the linked list to tell what vehicles that visited a depot in a tick */
static Vehicle* _first_veh_in_depot_list;

/** Adds a vehicle to the list of vehicles, that visited a depot this tick
 * @param *v vehicle to add
 */
void VehicleEnteredDepotThisTick(Vehicle *v)
{
	/* we need to set v->leave_depot_instantly as we have no control of it's contents at this time */
	if (HASBIT(v->current_order.flags, OFB_HALT_IN_DEPOT) && !HASBIT(v->current_order.flags, OFB_PART_OF_ORDERS) && v->current_order.type == OT_GOTO_DEPOT) {
		/* we keep the vehicle in the depot since the user ordered it to stay */
		v->leave_depot_instantly = false;
	} else {
		/* the vehicle do not plan on stopping in the depot, so we stop it to ensure that it will not reserve the path
		 * out of the depot before we might autoreplace it to a different engine. The new engine would not own the reserved path
		 * we store that we stopped the vehicle, so autoreplace can start it again */
		v->vehstatus |= VS_STOPPED;
		v->leave_depot_instantly = true;
	}

	if (_first_veh_in_depot_list == NULL) {
		_first_veh_in_depot_list = v;
	} else {
		Vehicle *w = _first_veh_in_depot_list;
		while (w->depot_list != NULL) w = w->depot_list;
		w->depot_list = v;
	}
}

typedef void VehicleTickProc(Vehicle*);
static VehicleTickProc* _vehicle_tick_procs[] = {
	Train_Tick,
	RoadVeh_Tick,
	Ship_Tick,
	Aircraft_Tick,
	EffectVehicle_Tick,
	DisasterVehicle_Tick,
};

void CallVehicleTicks()
{
	Vehicle *v;

#ifdef ENABLE_NETWORK
	/* hotfix for desync problem:
	 *  for MP games invalidate the YAPF cache every tick to keep it exactly the same on the server and all clients */
	if (_networking) {
		YapfNotifyTrackLayoutChange(INVALID_TILE, INVALID_TRACK);
	}
#endif //ENABLE_NETWORK

	_first_veh_in_depot_list = NULL; // now we are sure it's initialized at the start of each tick

	FOR_ALL_VEHICLES(v) {
		_vehicle_tick_procs[v->type](v);

		switch (v->type) {
			case VEH_TRAIN:
			case VEH_ROAD:
			case VEH_AIRCRAFT:
			case VEH_SHIP:
				if (v->type == VEH_TRAIN && IsTrainWagon(v)) continue;
				if (v->type == VEH_AIRCRAFT && v->subtype != AIR_HELICOPTER) continue;

				v->motion_counter += (v->direction & 1) ? (v->cur_speed * 3) / 4 : v->cur_speed;
				/* Play a running sound if the motion counter passes 256 (Do we not skip sounds?) */
				if (GB(v->motion_counter, 0, 8) < v->cur_speed) PlayVehicleSound(v, VSE_RUNNING);

				/* Play an alterate running sound every 16 ticks */
				if (GB(v->tick_counter, 0, 4) == 0) PlayVehicleSound(v, v->cur_speed > 0 ? VSE_RUNNING_16 : VSE_STOPPED_16);
		}
	}

	/* now we handle all the vehicles that entered a depot this tick */
	v = _first_veh_in_depot_list;
	while (v != NULL) {
		Vehicle *w = v->depot_list;
		v->depot_list = NULL; // it should always be NULL at the end of each tick
		MaybeReplaceVehicle(v, false, true);
		v = w;
	}
}

static bool CanFillVehicle_FullLoadAny(Vehicle *v)
{
	uint32 full = 0, not_full = 0;
	bool keep_loading = false;
	const GoodsEntry *ge = GetStation(v->last_station_visited)->goods;

	/* special handling of aircraft */

	/* if the aircraft carries passengers and is NOT full, then
	 *continue loading, no matter how much mail is in */
	if (v->type == VEH_AIRCRAFT &&
			IsCargoInClass(v->cargo_type, CC_PASSENGERS) &&
			v->cargo_cap != v->cargo_count) {
		return true;
	}

	/* patch should return "true" to continue loading, i.e. when there is no cargo type that is fully loaded. */
	do {
		/* Should never happen, but just in case future additions change this */
		assert(v->cargo_type<32);

		if (v->cargo_cap != 0) {
			uint32 mask = 1 << v->cargo_type;

			if (v->cargo_cap == v->cargo_count) {
				full |= mask;
			} else if (GB(ge[v->cargo_type].waiting_acceptance, 0, 12) > 0 ||
					(HASBIT(v->vehicle_flags, VF_CARGO_UNLOADING) && (ge[v->cargo_type].waiting_acceptance & 0x8000))) {
				/* If there is any cargo waiting, or this vehicle is still unloading
				 * and the station accepts the cargo, don't leave the station. */
				keep_loading = true;
			} else {
				not_full |= mask;
			}
		}
	} while ((v = v->next) != NULL);

	/* continue loading if there is a non full cargo type and no cargo type that is full */
	return keep_loading || (not_full && (full & ~not_full) == 0);
}


bool CanFillVehicle(Vehicle *front_v)
{
	TileIndex tile = front_v->tile;

	assert(IsTileType(tile, MP_STATION) ||
			(front_v->type == VEH_SHIP && (
				IsTileType(TILE_ADDXY(tile,  1,  0), MP_STATION) ||
				IsTileType(TILE_ADDXY(tile, -1,  0), MP_STATION) ||
				IsTileType(TILE_ADDXY(tile,  0,  1), MP_STATION) ||
				IsTileType(TILE_ADDXY(tile,  0, -1), MP_STATION) ||
				IsTileType(TILE_ADDXY(tile, -2,  0), MP_STATION)
			)));

	bool full_load = front_v->current_order.flags & OF_FULL_LOAD;

	/* If patch is active, use alternative CanFillVehicle-function */
	if (_patches.full_load_any && full_load) return CanFillVehicle_FullLoadAny(front_v);

	Vehicle *v = front_v;
	do {
		if (HASBIT(v->vehicle_flags, VF_CARGO_UNLOADING) || (full_load && v->cargo_count != v->cargo_cap)) return true;
	} while ((v = v->next) != NULL);

	return !HASBIT(front_v->vehicle_flags, VF_LOADING_FINISHED);
}

/** Check if a given engine type can be refitted to a given cargo
 * @param engine_type Engine type to check
 * @param cid_to check refit to this cargo-type
 * @return true if it is possible, false otherwise
 */
bool CanRefitTo(EngineID engine_type, CargoID cid_to)
{
	return HASBIT(EngInfo(engine_type)->refit_mask, cid_to);
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
			if (HASBIT(refit_mask, cid)) return cid;
		}
	}

	return CT_INVALID;
}

/** Learn the price of refitting a certain engine
* @param engine_type Which engine to refit
* @return Price for refitting
*/
int32 GetRefitCost(EngineID engine_type)
{
	int32 base_cost = 0;

	switch (GetEngine(engine_type)->type) {
		case VEH_SHIP: base_cost = _price.ship_base; break;
		case VEH_ROAD: base_cost = _price.roadveh_base; break;
		case VEH_AIRCRAFT: base_cost = _price.aircraft_base; break;
		case VEH_TRAIN:
			base_cost = 2 * ((RailVehInfo(engine_type)->railveh_type == RAILVEH_WAGON) ?
							 _price.build_railwagon : _price.build_railvehicle);
			break;
		default: NOT_REACHED(); break;
	}
	return (EngInfo(engine_type)->refit_cost * base_cost) >> 10;
}

static void DoDrawVehicle(const Vehicle *v)
{
	SpriteID image = v->cur_image;
	SpriteID pal;

	if (v->vehstatus & VS_SHADOW) {
		SETBIT(image, PALETTE_MODIFIER_TRANSPARENT);
		pal = PALETTE_TO_TRANSPARENT;
	} else if (v->vehstatus & VS_DEFPAL) {
		pal = (v->vehstatus & VS_CRASHED) ? PALETTE_CRASH : GetVehiclePalette(v);
	} else {
		pal = PAL_NONE;
	}

	AddSortableSpriteToDraw(image, pal, v->x_pos + v->x_offs, v->y_pos + v->y_offs,
		v->sprite_width, v->sprite_height, v->z_height, v->z_pos);
}

void ViewportAddVehicles(DrawPixelInfo *dpi)
{
	/* The bounding rectangle */
	const int l = dpi->left;
	const int r = dpi->left + dpi->width;
	const int t = dpi->top;
	const int b = dpi->top + dpi->height;

	/* The hash area to scan */
	const int xl = GB(l - 70, 7, 6);
	const int xu = GB(r,      7, 6);
	const int yl = GB(t - 70, 6, 6) << 6;
	const int yu = GB(b,      6, 6) << 6;

	int x;
	int y;

	for (y = yl;; y = (y + (1 << 6)) & (0x3F << 6)) {
		for (x = xl;; x = (x + 1) & 0x3F) {
			const Vehicle *v = _vehicle_position_hash[(x + y) & 0xFFFF];

			while (v != NULL) {
				if (!(v->vehstatus & VS_HIDDEN) &&
						l <= v->right_coord &&
						t <= v->bottom_coord &&
						r >= v->left_coord &&
						b >= v->top_coord) {
					DoDrawVehicle(v);
				}
				v = v->next_hash;
			}

			if (x == xu) break;
		}

		if (y == yu) break;
	}
}

static void ChimneySmokeInit(Vehicle *v)
{
	uint32 r = Random();
	v->cur_image = SPR_CHIMNEY_SMOKE_0 + GB(r, 0, 3);
	v->progress = GB(r, 16, 3);
}

static void ChimneySmokeTick(Vehicle *v)
{
	if (v->progress > 0) {
		v->progress--;
	} else {
		TileIndex tile;

		BeginVehicleMove(v);

		tile = TileVirtXY(v->x_pos, v->y_pos);
		if (!IsTileType(tile, MP_INDUSTRY)) {
			EndVehicleMove(v);
			DeleteVehicle(v);
			return;
		}

		if (v->cur_image != SPR_CHIMNEY_SMOKE_7) {
			v->cur_image++;
		} else {
			v->cur_image = SPR_CHIMNEY_SMOKE_0;
		}
		v->progress = 7;
		VehiclePositionChanged(v);
		EndVehicleMove(v);
	}
}

static void SteamSmokeInit(Vehicle *v)
{
	v->cur_image = SPR_STEAM_SMOKE_0;
	v->progress = 12;
}

static void SteamSmokeTick(Vehicle *v)
{
	bool moved = false;

	BeginVehicleMove(v);

	v->progress++;

	if ((v->progress & 7) == 0) {
		v->z_pos++;
		moved = true;
	}

	if ((v->progress & 0xF) == 4) {
		if (v->cur_image != SPR_STEAM_SMOKE_4) {
			v->cur_image++;
		} else {
			EndVehicleMove(v);
			DeleteVehicle(v);
			return;
		}
		moved = true;
	}

	if (moved) {
		VehiclePositionChanged(v);
		EndVehicleMove(v);
	}
}

static void DieselSmokeInit(Vehicle *v)
{
	v->cur_image = SPR_DIESEL_SMOKE_0;
	v->progress = 0;
}

static void DieselSmokeTick(Vehicle *v)
{
	v->progress++;

	if ((v->progress & 3) == 0) {
		BeginVehicleMove(v);
		v->z_pos++;
		VehiclePositionChanged(v);
		EndVehicleMove(v);
	} else if ((v->progress & 7) == 1) {
		BeginVehicleMove(v);
		if (v->cur_image != SPR_DIESEL_SMOKE_5) {
			v->cur_image++;
			VehiclePositionChanged(v);
			EndVehicleMove(v);
		} else {
			EndVehicleMove(v);
			DeleteVehicle(v);
		}
	}
}

static void ElectricSparkInit(Vehicle *v)
{
	v->cur_image = SPR_ELECTRIC_SPARK_0;
	v->progress = 1;
}

static void ElectricSparkTick(Vehicle *v)
{
	if (v->progress < 2) {
		v->progress++;
	} else {
		v->progress = 0;
		BeginVehicleMove(v);
		if (v->cur_image != SPR_ELECTRIC_SPARK_5) {
			v->cur_image++;
			VehiclePositionChanged(v);
			EndVehicleMove(v);
		} else {
			EndVehicleMove(v);
			DeleteVehicle(v);
		}
	}
}

static void SmokeInit(Vehicle *v)
{
	v->cur_image = SPR_SMOKE_0;
	v->progress = 12;
}

static void SmokeTick(Vehicle *v)
{
	bool moved = false;

	BeginVehicleMove(v);

	v->progress++;

	if ((v->progress & 3) == 0) {
		v->z_pos++;
		moved = true;
	}

	if ((v->progress & 0xF) == 4) {
		if (v->cur_image != SPR_SMOKE_4) {
			v->cur_image++;
		} else {
			EndVehicleMove(v);
			DeleteVehicle(v);
			return;
		}
		moved = true;
	}

	if (moved) {
		VehiclePositionChanged(v);
		EndVehicleMove(v);
	}
}

static void ExplosionLargeInit(Vehicle *v)
{
	v->cur_image = SPR_EXPLOSION_LARGE_0;
	v->progress = 0;
}

static void ExplosionLargeTick(Vehicle *v)
{
	v->progress++;
	if ((v->progress & 3) == 0) {
		BeginVehicleMove(v);
		if (v->cur_image != SPR_EXPLOSION_LARGE_F) {
			v->cur_image++;
			VehiclePositionChanged(v);
			EndVehicleMove(v);
		} else {
			EndVehicleMove(v);
			DeleteVehicle(v);
		}
	}
}

static void BreakdownSmokeInit(Vehicle *v)
{
	v->cur_image = SPR_BREAKDOWN_SMOKE_0;
	v->progress = 0;
}

static void BreakdownSmokeTick(Vehicle *v)
{
	v->progress++;
	if ((v->progress & 7) == 0) {
		BeginVehicleMove(v);
		if (v->cur_image != SPR_BREAKDOWN_SMOKE_3) {
			v->cur_image++;
		} else {
			v->cur_image = SPR_BREAKDOWN_SMOKE_0;
		}
		VehiclePositionChanged(v);
		EndVehicleMove(v);
	}

	v->u.special.unk0--;
	if (v->u.special.unk0 == 0) {
		BeginVehicleMove(v);
		EndVehicleMove(v);
		DeleteVehicle(v);
	}
}

static void ExplosionSmallInit(Vehicle *v)
{
	v->cur_image = SPR_EXPLOSION_SMALL_0;
	v->progress = 0;
}

static void ExplosionSmallTick(Vehicle *v)
{
	v->progress++;
	if ((v->progress & 3) == 0) {
		BeginVehicleMove(v);
		if (v->cur_image != SPR_EXPLOSION_SMALL_B) {
			v->cur_image++;
			VehiclePositionChanged(v);
			EndVehicleMove(v);
		} else {
			EndVehicleMove(v);
			DeleteVehicle(v);
		}
	}
}

static void BulldozerInit(Vehicle *v)
{
	v->cur_image = SPR_BULLDOZER_NE;
	v->progress = 0;
	v->u.special.unk0 = 0;
	v->u.special.unk2 = 0;
}

struct BulldozerMovement {
	byte direction:2;
	byte image:2;
	byte duration:3;
};

static const BulldozerMovement _bulldozer_movement[] = {
	{ 0, 0, 4 },
	{ 3, 3, 4 },
	{ 2, 2, 7 },
	{ 0, 2, 7 },
	{ 1, 1, 3 },
	{ 2, 2, 7 },
	{ 0, 2, 7 },
	{ 1, 1, 3 },
	{ 2, 2, 7 },
	{ 0, 2, 7 },
	{ 3, 3, 6 },
	{ 2, 2, 6 },
	{ 1, 1, 7 },
	{ 3, 1, 7 },
	{ 0, 0, 3 },
	{ 1, 1, 7 },
	{ 3, 1, 7 },
	{ 0, 0, 3 },
	{ 1, 1, 7 },
	{ 3, 1, 7 }
};

static const struct {
	int8 x;
	int8 y;
} _inc_by_dir[] = {
	{ -1,  0 },
	{  0,  1 },
	{  1,  0 },
	{  0, -1 }
};

static void BulldozerTick(Vehicle *v)
{
	v->progress++;
	if ((v->progress & 7) == 0) {
		const BulldozerMovement* b = &_bulldozer_movement[v->u.special.unk0];

		BeginVehicleMove(v);

		v->cur_image = SPR_BULLDOZER_NE + b->image;

		v->x_pos += _inc_by_dir[b->direction].x;
		v->y_pos += _inc_by_dir[b->direction].y;

		v->u.special.unk2++;
		if (v->u.special.unk2 >= b->duration) {
			v->u.special.unk2 = 0;
			v->u.special.unk0++;
			if (v->u.special.unk0 == lengthof(_bulldozer_movement)) {
				EndVehicleMove(v);
				DeleteVehicle(v);
				return;
			}
		}
		VehiclePositionChanged(v);
		EndVehicleMove(v);
	}
}

static void BubbleInit(Vehicle *v)
{
	v->cur_image = SPR_BUBBLE_GENERATE_0;
	v->spritenum = 0;
	v->progress = 0;
}

struct BubbleMovement {
	int8 x:4;
	int8 y:4;
	int8 z:4;
	byte image:4;
};

#define MK(x, y, z, i) { x, y, z, i }
#define ME(i) { i, 4, 0, 0 }

static const BubbleMovement _bubble_float_sw[] = {
	MK(0, 0, 1, 0),
	MK(1, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(1, 0, 1, 2),
	ME(1)
};


static const BubbleMovement _bubble_float_ne[] = {
	MK( 0, 0, 1, 0),
	MK(-1, 0, 1, 1),
	MK( 0, 0, 1, 0),
	MK(-1, 0, 1, 2),
	ME(1)
};

static const BubbleMovement _bubble_float_se[] = {
	MK(0, 0, 1, 0),
	MK(0, 1, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 1, 1, 2),
	ME(1)
};

static const BubbleMovement _bubble_float_nw[] = {
	MK(0,  0, 1, 0),
	MK(0, -1, 1, 1),
	MK(0,  0, 1, 0),
	MK(0, -1, 1, 2),
	ME(1)
};

static const BubbleMovement _bubble_burst[] = {
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 7),
	MK(0, 0, 1, 8),
	MK(0, 0, 1, 9),
	ME(0)
};

static const BubbleMovement _bubble_absorb[] = {
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(0, 0, 1, 1),
	MK(2, 1, 3, 0),
	MK(1, 1, 3, 1),
	MK(2, 1, 3, 0),
	MK(1, 1, 3, 2),
	MK(2, 1, 3, 0),
	MK(1, 1, 3, 1),
	MK(2, 1, 3, 0),
	MK(1, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(1, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(1, 0, 1, 2),
	MK(0, 0, 1, 0),
	MK(1, 0, 1, 1),
	MK(0, 0, 1, 0),
	MK(1, 0, 1, 2),
	ME(2),
	MK(0, 0, 0, 0xA),
	MK(0, 0, 0, 0xB),
	MK(0, 0, 0, 0xC),
	MK(0, 0, 0, 0xD),
	MK(0, 0, 0, 0xE),
	ME(0)
};
#undef ME
#undef MK

static const BubbleMovement * const _bubble_movement[] = {
	_bubble_float_sw,
	_bubble_float_ne,
	_bubble_float_se,
	_bubble_float_nw,
	_bubble_burst,
	_bubble_absorb,
};

static void BubbleTick(Vehicle *v)
{
	/*
	 * Warning: those effects can NOT use Random(), and have to use
	 *  InteractiveRandom(), because somehow someone forgot to save
	 *  spritenum to the savegame, and so it will cause desyncs in
	 *  multiplayer!! (that is: in ToyLand)
	 */
	uint et;
	const BubbleMovement *b;

	v->progress++;
	if ((v->progress & 3) != 0)
		return;

	BeginVehicleMove(v);

	if (v->spritenum == 0) {
		v->cur_image++;
		if (v->cur_image < SPR_BUBBLE_GENERATE_3) {
			VehiclePositionChanged(v);
			EndVehicleMove(v);
			return;
		}
		if (v->u.special.unk2 != 0) {
			v->spritenum = GB(InteractiveRandom(), 0, 2) + 1;
		} else {
			v->spritenum = 6;
		}
		et = 0;
	} else {
		et = v->engine_type + 1;
	}

	b = &_bubble_movement[v->spritenum - 1][et];

	if (b->y == 4 && b->x == 0) {
		EndVehicleMove(v);
		DeleteVehicle(v);
		return;
	}

	if (b->y == 4 && b->x == 1) {
		if (v->z_pos > 180 || CHANCE16I(1, 96, InteractiveRandom())) {
			v->spritenum = 5;
			SndPlayVehicleFx(SND_2F_POP, v);
		}
		et = 0;
	}

	if (b->y == 4 && b->x == 2) {
		TileIndex tile;

		et++;
		SndPlayVehicleFx(SND_31_EXTRACT, v);

		tile = TileVirtXY(v->x_pos, v->y_pos);
		if (IsTileType(tile, MP_INDUSTRY) && GetIndustryGfx(tile) == 0xA2) AddAnimatedTile(tile);
	}

	v->engine_type = et;
	b = &_bubble_movement[v->spritenum - 1][et];

	v->x_pos += b->x;
	v->y_pos += b->y;
	v->z_pos += b->z;
	v->cur_image = SPR_BUBBLE_0 + b->image;

	VehiclePositionChanged(v);
	EndVehicleMove(v);
}


typedef void EffectInitProc(Vehicle *v);
typedef void EffectTickProc(Vehicle *v);

static EffectInitProc * const _effect_init_procs[] = {
	ChimneySmokeInit,
	SteamSmokeInit,
	DieselSmokeInit,
	ElectricSparkInit,
	SmokeInit,
	ExplosionLargeInit,
	BreakdownSmokeInit,
	ExplosionSmallInit,
	BulldozerInit,
	BubbleInit,
};

static EffectTickProc * const _effect_tick_procs[] = {
	ChimneySmokeTick,
	SteamSmokeTick,
	DieselSmokeTick,
	ElectricSparkTick,
	SmokeTick,
	ExplosionLargeTick,
	BreakdownSmokeTick,
	ExplosionSmallTick,
	BulldozerTick,
	BubbleTick,
};


Vehicle *CreateEffectVehicle(int x, int y, int z, EffectVehicle type)
{
	Vehicle *v;

	v = ForceAllocateSpecialVehicle();
	if (v != NULL) {
		v = new (v) SpecialVehicle();
		v->subtype = type;
		v->x_pos = x;
		v->y_pos = y;
		v->z_pos = z;
		v->tile = 0;
		v->UpdateDeltaXY(INVALID_DIR);
		v->vehstatus = VS_UNCLICKABLE;

		_effect_init_procs[type](v);

		VehiclePositionChanged(v);
		BeginVehicleMove(v);
		EndVehicleMove(v);
	}
	return v;
}

Vehicle *CreateEffectVehicleAbove(int x, int y, int z, EffectVehicle type)
{
	int safe_x = clamp(x, 0, MapMaxX() * TILE_SIZE);
	int safe_y = clamp(y, 0, MapMaxY() * TILE_SIZE);
	return CreateEffectVehicle(x, y, GetSlopeZ(safe_x, safe_y) + z, type);
}

Vehicle *CreateEffectVehicleRel(const Vehicle *v, int x, int y, int z, EffectVehicle type)
{
	return CreateEffectVehicle(v->x_pos + x, v->y_pos + y, v->z_pos + z, type);
}

static void EffectVehicle_Tick(Vehicle *v)
{
	_effect_tick_procs[v->subtype](v);
}

Vehicle *CheckClickOnVehicle(const ViewPort *vp, int x, int y)
{
	Vehicle *found = NULL, *v;
	uint dist, best_dist = (uint)-1;

	if ( (uint)(x -= vp->left) >= (uint)vp->width ||
			 (uint)(y -= vp->top) >= (uint)vp->height)
				return NULL;

	x = (x << vp->zoom) + vp->virtual_left;
	y = (y << vp->zoom) + vp->virtual_top;

	FOR_ALL_VEHICLES(v) {
		if ((v->vehstatus & (VS_HIDDEN|VS_UNCLICKABLE)) == 0 &&
				x >= v->left_coord && x <= v->right_coord &&
				y >= v->top_coord && y <= v->bottom_coord) {

			dist = max(
				myabs( ((v->left_coord + v->right_coord)>>1) - x ),
				myabs( ((v->top_coord + v->bottom_coord)>>1) - y )
			);

			if (dist < best_dist) {
				found = v;
				best_dist = dist;
			}
		}
	}

	return found;
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
	uint32 r;
	int chance;

	/* decrease reliability */
	v->reliability = rel = max((rel_old = v->reliability) - v->reliability_spd_dec, 0);
	if ((rel_old >> 8) != (rel >> 8))
		InvalidateWindow(WC_VEHICLE_DETAILS, v->index);

	if (v->breakdown_ctr != 0 || v->vehstatus & VS_STOPPED ||
			v->cur_speed < 5 || _game_mode == GM_MENU) {
		return;
	}

	r = Random();

	/* increase chance of failure */
	chance = v->breakdown_chance + 1;
	if (CHANCE16I(1,25,r)) chance += 25;
	v->breakdown_chance = min(255, chance);

	/* calculate reliability value to use in comparison */
	rel = v->reliability;
	if (v->type == VEH_SHIP) rel += 0x6666;

	/* disabled breakdowns? */
	if (_opt.diff.vehicle_breakdowns < 1) return;

	/* reduced breakdowns? */
	if (_opt.diff.vehicle_breakdowns == 1) rel += 0x6666;

	/* check if to break down */
	if (_breakdown_chance[(uint)min(rel, 0xffff) >> 10] <= v->breakdown_chance) {
		v->breakdown_ctr    = GB(r, 16, 6) + 0x3F;
		v->breakdown_delay  = GB(r, 24, 7) + 0x80;
		v->breakdown_chance = 0;
	}
}

static const StringID _vehicle_type_names[4] = {
	STR_019F_TRAIN,
	STR_019C_ROAD_VEHICLE,
	STR_019E_SHIP,
	STR_019D_AIRCRAFT,
};

static void ShowVehicleGettingOld(Vehicle *v, StringID msg)
{
	if (v->owner != _local_player) return;

	/* Do not show getting-old message if autorenew is active */
	if (GetPlayer(v->owner)->engine_renew) return;

	SetDParam(0, _vehicle_type_names[v->type]);
	SetDParam(1, v->unitnumber);
	AddNewsItem(msg, NEWS_FLAGS(NM_SMALL, NF_VIEWPORT|NF_VEHICLE, NT_ADVICE, 0), v->index, 0);
}

void AgeVehicle(Vehicle *v)
{
	int age;

	if (v->age < 65535)
		v->age++;

	age = v->age - v->max_age;
	if (age == 366*0 || age == 366*1 || age == 366*2 || age == 366*3 || age == 366*4)
		v->reliability_spd_dec <<= 1;

	InvalidateWindow(WC_VEHICLE_DETAILS, v->index);

	if (age == -366) {
		ShowVehicleGettingOld(v, STR_01A0_IS_GETTING_OLD);
	} else if (age == 0) {
		ShowVehicleGettingOld(v, STR_01A1_IS_GETTING_VERY_OLD);
	} else if (age == 366*1 || age == 366*2 || age == 366*3 || age == 366*4 || age == 366*5) {
		ShowVehicleGettingOld(v, STR_01A2_IS_GETTING_VERY_OLD_AND);
	}
}

/** Starts or stops a lot of vehicles
 * @param tile Tile of the depot where the vehicles are started/stopped (only used for depots)
 * @param flags type of operation
 * @param p1 Station/Order/Depot ID (only used for vehicle list windows)
 * @param p2 bitmask
 *   - bit 0-4 Vehicle type
 *   - bit 5 false = start vehicles, true = stop vehicles
 *   - bit 6 if set, then it's a vehicle list window, not a depot and Tile is ignored in this case
 *   - bit 8-11 Vehicle List Window type (ignored unless bit 1 is set)
 */
int32 CmdMassStartStopVehicle(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle **vl = NULL;
	uint16 engine_list_length = 0;
	uint16 engine_count = 0;
	int32 return_value = CMD_ERROR;
	uint i;
	uint stop_command;
	byte vehicle_type = GB(p2, 0, 5);
	bool start_stop = HASBIT(p2, 5);
	bool vehicle_list_window = HASBIT(p2, 6);

	switch (vehicle_type) {
		case VEH_TRAIN:    stop_command = CMD_START_STOP_TRAIN;    break;
		case VEH_ROAD:     stop_command = CMD_START_STOP_ROADVEH;  break;
		case VEH_SHIP:     stop_command = CMD_START_STOP_SHIP;     break;
		case VEH_AIRCRAFT: stop_command = CMD_START_STOP_AIRCRAFT; break;
		default: return CMD_ERROR;
	}

	if (vehicle_list_window) {
		uint32 id = p1;
		uint16 window_type = p2 & VLW_MASK;

		engine_count = GenerateVehicleSortList((const Vehicle***)&vl, &engine_list_length, vehicle_type, _current_player, id, window_type);
	} else {
		/* Get the list of vehicles in the depot */
		BuildDepotVehicleList(vehicle_type, tile, &vl, &engine_list_length, &engine_count, NULL, NULL, NULL);
	}

	for (i = 0; i < engine_count; i++) {
		const Vehicle *v = vl[i];
		int32 ret;

		if (!!(v->vehstatus & VS_STOPPED) != start_stop) continue;

		if (!vehicle_list_window) {
			if (vehicle_type == VEH_TRAIN) {
				if (CheckTrainInDepot(v, false) == -1) continue;
			} else {
				if (!(v->vehstatus & VS_HIDDEN)) continue;
			}
		}

		ret = DoCommand(tile, v->index, 0, flags, stop_command);

		if (!CmdFailed(ret)) {
			return_value = 0;
			/* We know that the command is valid for at least one vehicle.
			 * If we haven't set DC_EXEC, then there is no point in continueing because it will be valid */
			if (!(flags & DC_EXEC)) break;
		}
	}

	free(vl);
	return return_value;
}

/** Sells all vehicles in a depot
 * @param tile Tile of the depot where the depot is
 * @param flags type of operation
 * @param p1 Vehicle type
 * @param p2 unused
 */
int32 CmdDepotSellAllVehicles(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle **engines = NULL;
	Vehicle **wagons = NULL;
	uint16 engine_list_length = 0;
	uint16 engine_count = 0;
	uint16 wagon_list_length = 0;
	uint16 wagon_count = 0;

	int32 cost = 0;
	uint i, sell_command, total_number_vehicles;
	byte vehicle_type = GB(p1, 0, 8);

	switch (vehicle_type) {
		case VEH_TRAIN:    sell_command = CMD_SELL_RAIL_WAGON; break;
		case VEH_ROAD:     sell_command = CMD_SELL_ROAD_VEH;   break;
		case VEH_SHIP:     sell_command = CMD_SELL_SHIP;       break;
		case VEH_AIRCRAFT: sell_command = CMD_SELL_AIRCRAFT;   break;
		default: return CMD_ERROR;
	}

	/* Get the list of vehicles in the depot */
	BuildDepotVehicleList(vehicle_type, tile, &engines, &engine_list_length, &engine_count,
						                      &wagons,  &wagon_list_length,  &wagon_count);

	total_number_vehicles = engine_count + wagon_count;
	for (i = 0; i < total_number_vehicles; i++) {
		const Vehicle *v;
		int32 ret;

		if (i < engine_count) {
			v = engines[i];
		} else {
			v = wagons[i - engine_count];
		}

		ret = DoCommand(tile, v->index, 1, flags, sell_command);

		if (!CmdFailed(ret)) cost += ret;
	}

	free(engines);
	free(wagons);
	if (cost == 0) return CMD_ERROR; // no vehicles to sell
	return cost;
}

/** Autoreplace all vehicles in the depot
 * @param tile Tile of the depot where the vehicles are
 * @param flags type of operation
 * @param p1 Type of vehicle
 * @param p2 Unused
 */
int32 CmdDepotMassAutoReplace(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle **vl = NULL;
	uint16 engine_list_length = 0;
	uint16 engine_count = 0;
	uint i, x = 0, y = 0, z = 0;
	int32 cost = 0;
	byte vehicle_type = GB(p1, 0, 8);


	if (!IsTileOwner(tile, _current_player)) return CMD_ERROR;

	/* Get the list of vehicles in the depot */
	BuildDepotVehicleList(vehicle_type, tile, &vl, &engine_list_length, &engine_count, NULL, NULL, NULL);


	for (i = 0; i < engine_count; i++) {
		Vehicle *v = vl[i];
		bool stopped = !(v->vehstatus & VS_STOPPED);
		int32 ret;

		/* Ensure that the vehicle completely in the depot */
		if (!IsVehicleInDepot(v)) continue;

		x = v->x_pos;
		y = v->y_pos;
		z = v->z_pos;

		if (stopped) {
			v->vehstatus |= VS_STOPPED; // Stop the vehicle
			v->leave_depot_instantly = true;
		}
		ret = MaybeReplaceVehicle(v, !(flags & DC_EXEC), false);

		if (!CmdFailed(ret)) {
			cost += ret;
			if (!(flags & DC_EXEC)) break;
			/* There is a problem with autoreplace and newgrf
			 * It's impossible to tell the length of a train after it's being replaced before it's actually done
			 * Because of this, we can't estimate costs due to wagon removal and we will have to always return 0 and pay manually
			 * Since we pay after each vehicle is replaced and MaybeReplaceVehicle() check if the player got enough money
			 * we should never reach a condition where the player will end up with negative money from doing this */
			SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);
			SubtractMoneyFromPlayer(ret);
		}
	}

	if (cost == 0) {
		cost = CMD_ERROR;
	} else {
		if (flags & DC_EXEC) {
			/* Display the cost animation now that DoCommandP() can't do it for us (see previous comments) */
			if (IsLocalPlayer()) ShowCostOrIncomeAnimation(x, y, z, cost);
		}
		cost = 0;
	}

	free(vl);
	return cost;
}

/** Clone a vehicle. If it is a train, it will clone all the cars too
 * @param tile tile of the depot where the cloned vehicle is build
 * @param flags type of operation
 * @param p1 the original vehicle's index
 * @param p2 1 = shared orders, else copied orders
 */
int32 CmdCloneVehicle(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v_front, *v;
	Vehicle *w_front, *w, *w_rear;
	int32 cost, total_cost = 0;
	uint32 build_argument = 2;

	if (!IsValidVehicleID(p1)) return CMD_ERROR;
	v = GetVehicle(p1);
	v_front = v;
	w = NULL;
	w_front = NULL;
	w_rear = NULL;


	/*
	 * v_front is the front engine in the original vehicle
	 * v is the car/vehicle of the original vehicle, that is currently being copied
	 * w_front is the front engine of the cloned vehicle
	 * w is the car/vehicle currently being cloned
	 * w_rear is the rear end of the cloned train. It's used to add more cars and is only used by trains
	 */

	if (!CheckOwnership(v->owner)) return CMD_ERROR;

	if (v->type == VEH_TRAIN && (!IsFrontEngine(v) || v->u.rail.crash_anim_pos >= 4400)) return CMD_ERROR;

	/* check that we can allocate enough vehicles */
	if (!(flags & DC_EXEC)) {
		int veh_counter = 0;
		do {
			veh_counter++;
		} while ((v = v->next) != NULL);

		if (!AllocateVehicles(NULL, veh_counter)) {
			return_cmd_error(STR_00E1_TOO_MANY_VEHICLES_IN_GAME);
		}
	}

	v = v_front;

	do {
		if (IsMultiheaded(v) && !IsTrainEngine(v)) {
			/* we build the rear ends of multiheaded trains with the front ones */
			continue;
		}

		cost = DoCommand(tile, v->engine_type, build_argument, flags, GetCmdBuildVeh(v));
		build_argument = 3; // ensure that we only assign a number to the first engine

		if (CmdFailed(cost)) return cost;

		total_cost += cost;

		if (flags & DC_EXEC) {
			w = GetVehicle(_new_vehicle_id);

			if (v->type == VEH_TRAIN && HASBIT(v->u.rail.flags, VRF_REVERSE_DIRECTION)) {
				SETBIT(w->u.rail.flags, VRF_REVERSE_DIRECTION);
			}

			if (v->type == VEH_TRAIN && !IsFrontEngine(v)) {
				/* this s a train car
				 * add this unit to the end of the train */
				DoCommand(0, (w_rear->index << 16) | w->index, 1, flags, CMD_MOVE_RAIL_VEHICLE);
			} else {
				/* this is a front engine or not a train. It need orders */
				w_front = w;
				w->service_interval = v->service_interval;
				DoCommand(0, (v->index << 16) | w->index, p2 & 1 ? CO_SHARE : CO_COPY, flags, CMD_CLONE_ORDER);
			}
			w_rear = w; // trains needs to know the last car in the train, so they can add more in next loop
		}
	} while (v->type == VEH_TRAIN && (v = GetNextVehicle(v)) != NULL);

	if (flags & DC_EXEC && v_front->type == VEH_TRAIN) {
		/* for trains this needs to be the front engine due to the callback function */
		_new_vehicle_id = w_front->index;
	}

	/* Take care of refitting. */
	w = w_front;
	v = v_front;

	/* Both building and refitting are influenced by newgrf callbacks, which
	 * makes it impossible to accurately estimate the cloning costs. In
	 * particular, it is possible for engines of the same type to be built with
	 * different numbers of articulated parts, so when refitting we have to
	 * loop over real vehicles first, and then the articulated parts of those
	 * vehicles in a different loop. */
	do {
		do {
			if (flags & DC_EXEC) {
				assert(w != NULL);

				if (w->cargo_type != v->cargo_type || w->cargo_subtype != v->cargo_type) {
					cost = DoCommand(0, w->index, v->cargo_type | (v->cargo_subtype << 8) | 1U << 16 , flags, GetCmdRefitVeh(v));
					if (!CmdFailed(cost)) total_cost += cost;
				}

				if (w->type == VEH_TRAIN && EngineHasArticPart(w)) {
					w = GetNextArticPart(w);
				} else {
					break;
				}
			} else {
				CargoID initial_cargo = GetEngineCargoType(v->engine_type);

				if (v->cargo_type != initial_cargo && initial_cargo != CT_INVALID) {
					total_cost += GetRefitCost(v->engine_type);
				}
			}
		} while (v->type == VEH_TRAIN && EngineHasArticPart(v) && (v = GetNextArticPart(v)) != NULL);

		if (flags & DC_EXEC) w = GetNextVehicle(w);
	} while (v->type == VEH_TRAIN && (v = GetNextVehicle(v)) != NULL);

	/* Since we can't estimate the cost of cloning a vehicle accurately we must
	 * check whether the player has enough money manually. */
	if (!CheckPlayerHasMoney(total_cost)) {
		if (flags & DC_EXEC) {
			/* The vehicle has already been bought, so now it must be sold again. */
			DoCommand(w_front->tile, w_front->index, 1, flags, GetCmdSellVeh(w_front));
		}
		return CMD_ERROR;
	}

	/* Set the expense type last as refitting will make the cost go towards
	 * running costs... */
	SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);
	return total_cost;
}


/* Extend the list size for BuildDepotVehicleList() */
static inline void ExtendVehicleListSize(const Vehicle ***engine_list, uint16 *engine_list_length, uint16 step_size)
{
	*engine_list_length = min(*engine_list_length + step_size, GetMaxVehicleIndex() + 1);
	*engine_list = ReallocT(*engine_list, *engine_list_length);
}

/** Generates a list of vehicles inside a depot
 * Will enlarge allocated space for the list if they are too small, so it's ok to call with (pointer to NULL array, pointer to uninitised uint16, pointer to 0)
 * If one of the lists is not needed (say wagons when finding ships), all the pointers regarding that list should be set to NULL
 * @param type Type of vehicle
 * @param tile The tile the depot is located in
 * @param ***engine_list Pointer to a pointer to an array of vehicles in the depot (old list is freed and a new one is malloced)
 * @param *engine_list_length Allocated size of engine_list. Needs to be set to 0 when engine_list points to a NULL array
 * @param *engine_count The number of engines stored in the list
 * @param ***wagon_list Pointer to a pointer to an array of free wagons in the depot (old list is freed and a new one is malloced)
 * @param *wagon_list_length Allocated size of wagon_list. Needs to be set to 0 when wagon_list points to a NULL array
 * @param *wagon_count The number of engines stored in the list
 */
void BuildDepotVehicleList(byte type, TileIndex tile, Vehicle ***engine_list, uint16 *engine_list_length, uint16 *engine_count, Vehicle ***wagon_list, uint16 *wagon_list_length, uint16 *wagon_count)
{
	Vehicle *v;

	/* This function should never be called without an array to store results */
	assert(!(engine_list == NULL && type != VEH_TRAIN));
	assert(!(type == VEH_TRAIN && engine_list == NULL && wagon_list == NULL));

	/* Both array and the length should either be NULL to disable the list or both should not be NULL */
	assert((engine_list == NULL && engine_list_length == NULL) || (engine_list != NULL && engine_list_length != NULL));
	assert((wagon_list == NULL && wagon_list_length == NULL) || (wagon_list != NULL && wagon_list_length != NULL));

	assert(!(engine_list != NULL && engine_count == NULL));
	assert(!(wagon_list != NULL && wagon_count == NULL));

	if (engine_count != NULL) *engine_count = 0;
	if (wagon_count != NULL) *wagon_count = 0;

	switch (type) {
		case VEH_TRAIN:
			FOR_ALL_VEHICLES(v) {
				if (v->tile == tile && v->type == VEH_TRAIN && v->u.rail.track == TRACK_BIT_DEPOT) {
					if (IsFrontEngine(v)) {
						if (engine_list == NULL) continue;
						if (*engine_count == *engine_list_length) ExtendVehicleListSize((const Vehicle***)engine_list, engine_list_length, 25);
						(*engine_list)[(*engine_count)++] = v;
					} else if (IsFreeWagon(v)) {
						if (wagon_list == NULL) continue;
						if (*wagon_count == *wagon_list_length) ExtendVehicleListSize((const Vehicle***)wagon_list, wagon_list_length, 25);
						(*wagon_list)[(*wagon_count)++] = v;
					}
				}
			}
			break;

		case VEH_ROAD:
			FOR_ALL_VEHICLES(v) {
				if (v->tile == tile && v->type == VEH_ROAD && IsRoadVehInDepot(v)) {
					if (*engine_count == *engine_list_length) ExtendVehicleListSize((const Vehicle***)engine_list, engine_list_length, 25);
					(*engine_list)[(*engine_count)++] = v;
				}
			}
			break;

		case VEH_SHIP:
			FOR_ALL_VEHICLES(v) {
				if (v->tile == tile && v->type == VEH_SHIP && IsShipInDepot(v)) {
					if (*engine_count == *engine_list_length) ExtendVehicleListSize((const Vehicle***)engine_list, engine_list_length, 25);
					(*engine_list)[(*engine_count)++] = v;
				}
			}
			break;

		case VEH_AIRCRAFT:
			FOR_ALL_VEHICLES(v) {
				if (v->tile == tile &&
						v->type == VEH_AIRCRAFT && IsNormalAircraft(v) &&
						v->vehstatus & VS_HIDDEN) {
					if (*engine_count == *engine_list_length) ExtendVehicleListSize((const Vehicle***)engine_list, engine_list_length, 25);
					(*engine_list)[(*engine_count)++] = v;
				}
			}
			break;

		default: NOT_REACHED();
	}
}

/**
* @param sort_list list to store the list in. Either NULL or the length length_of_array tells
* @param length_of_array informs the length allocated for sort_list. This is not the same as the number of vehicles in the list. Needs to be 0 when sort_list is NULL
* @param type type of vehicle
* @param owner PlayerID of owner to generate a list for
* @param index This parameter has different meanings depending on window_type
    <ul>
      <li>VLW_STATION_LIST:  index of station to generate a list for</li>
      <li>VLW_SHARED_ORDERS: index of order to generate a list for<li>
      <li>VLW_STANDARD: not used<li>
      <li>VLW_DEPOT_LIST: TileIndex of the depot/hangar to make the list for</li>
    </ul>
* @param window_type tells what kind of window the list is for. Use the VLW flags in vehicle_gui.h
* @return the number of vehicles added to the list
*/
uint GenerateVehicleSortList(const Vehicle ***sort_list, uint16 *length_of_array, byte type, PlayerID owner, uint32 index, uint16 window_type)
{
	const byte subtype = (type != VEH_AIRCRAFT) ? (byte)Train_Front : (byte)AIR_AIRCRAFT;
	uint n = 0;
	const Vehicle *v;

	switch (window_type) {
		case VLW_STATION_LIST: {
			FOR_ALL_VEHICLES(v) {
				if (v->type == type && (
					(type == VEH_TRAIN && IsFrontEngine(v)) ||
					(type != VEH_TRAIN && v->subtype <= subtype))) {
					const Order *order;

					FOR_VEHICLE_ORDERS(v, order) {
						if (order->type == OT_GOTO_STATION && order->dest == index) {
							if (n == *length_of_array) ExtendVehicleListSize(sort_list, length_of_array, 50);
							(*sort_list)[n++] = v;
							break;
						}
					}
				}
			}
			break;
		}

		case VLW_SHARED_ORDERS: {
			FOR_ALL_VEHICLES(v) {
				/* Find a vehicle with the order in question */
				if (v->orders != NULL && v->orders->index == index) break;
			}

			if (v != NULL && v->orders != NULL && v->orders->index == index) {
				/* Only try to make the list if we found a vehicle using the order in question */
				for (v = GetFirstVehicleFromSharedList(v); v != NULL; v = v->next_shared) {
					if (n == *length_of_array) ExtendVehicleListSize(sort_list, length_of_array, 25);
					(*sort_list)[n++] = v;
				}
			}
			break;
		}

		case VLW_STANDARD: {
			FOR_ALL_VEHICLES(v) {
				if (v->type == type && v->owner == owner && (
					(type == VEH_TRAIN && IsFrontEngine(v)) ||
					(type != VEH_TRAIN && v->subtype <= subtype))) {
					/* TODO find a better estimate on the total number of vehicles for current player */
					if (n == *length_of_array) ExtendVehicleListSize(sort_list, length_of_array, GetNumVehicles()/4);
					(*sort_list)[n++] = v;
				}
			}
			break;
		}

		case VLW_DEPOT_LIST: {
			FOR_ALL_VEHICLES(v) {
				if (v->type == type && (
					(type == VEH_TRAIN && IsFrontEngine(v)) ||
					(type != VEH_TRAIN && v->subtype <= subtype))) {
					const Order *order;

					FOR_VEHICLE_ORDERS(v, order) {
						if (order->type == OT_GOTO_DEPOT && order->dest == index) {
							if (n == *length_of_array) ExtendVehicleListSize(sort_list, length_of_array, 25);
							(*sort_list)[n++] = v;
							break;
						}
					}
				}
			}
			break;
		}

		default: NOT_REACHED(); break;
	}

	if ((n + 100) < *length_of_array) {
		/* We allocated way too much for sort_list.
		 * Now we will reduce how much we allocated.
		 * We will still make it have room for 50 extra vehicles to prevent having
		 * to move the whole array if just one vehicle is added later */
		*length_of_array = n + 50;
		*sort_list = ReallocT(*sort_list, (*length_of_array) * sizeof((*sort_list)[0]));
	}

	return n;
}

/** send all vehicles of type to depots
 * @param type type of vehicle
 * @param flags the flags used for DoCommand()
 * @param service should the vehicles only get service in the depots
 * @param owner PlayerID of owner of the vehicles to send
 * @param vlw_flag tells what kind of list requested the goto depot
 * @return 0 for success and CMD_ERROR if no vehicle is able to go to depot
 */
int32 SendAllVehiclesToDepot(byte type, uint32 flags, bool service, PlayerID owner, uint16 vlw_flag, uint32 id)
{
	const Vehicle **sort_list = NULL;
	uint n, i;
	uint16 array_length = 0;

	n = GenerateVehicleSortList(&sort_list, &array_length, type, owner, id, vlw_flag);

	/* Send all the vehicles to a depot */
	for (i = 0; i < n; i++) {
		const Vehicle *v = sort_list[i];
		int32 ret = DoCommand(v->tile, v->index, (service ? 1 : 0) | DEPOT_DONT_CANCEL, flags, GetCmdSendToDepot(type));

		/* Return 0 if DC_EXEC is not set this is a valid goto depot command)
			* In this case we know that at least one vehicle can be sent to a depot
			* and we will issue the command. We can now safely quit the loop, knowing
			* it will succeed at least once. With DC_EXEC we really need to send them to the depot */
		if (!CmdFailed(ret) && !(flags & DC_EXEC)) {
			free((void*)sort_list);
			return 0;
		}
	}

	free((void*)sort_list);
	return (flags & DC_EXEC) ? 0 : CMD_ERROR;
}

bool IsVehicleInDepot(const Vehicle *v)
{
	switch (v->type) {
		case VEH_TRAIN:    return CheckTrainInDepot(v, false) != -1;
		case VEH_ROAD:     return IsRoadVehInDepot(v);
		case VEH_SHIP:     return IsShipInDepot(v);
		case VEH_AIRCRAFT: return IsAircraftInHangar(v);
		default: NOT_REACHED();
	}
	return false;
}

void VehicleEnterDepot(Vehicle *v)
{
	switch (v->type) {
		case VEH_TRAIN:
			InvalidateWindowClasses(WC_TRAINS_LIST);
			if (!IsFrontEngine(v)) v = GetFirstVehicleInChain(v);
			UpdateSignalsOnSegment(v->tile, GetRailDepotDirection(v->tile));
			v->load_unload_time_rem = 0;
			break;

		case VEH_ROAD:
			InvalidateWindowClasses(WC_ROADVEH_LIST);
			v->u.road.state = RVSB_IN_DEPOT;
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

	if (v->current_order.type == OT_GOTO_DEPOT) {
		Order t;

		InvalidateWindow(WC_VEHICLE_VIEW, v->index);

		t = v->current_order;
		v->current_order.type = OT_DUMMY;
		v->current_order.flags = 0;

		if (t.refit_cargo < NUM_CARGO) {
			int32 cost;

			_current_player = v->owner;
			cost = DoCommand(v->tile, v->index, t.refit_cargo | t.refit_subtype << 8, DC_EXEC, GetCmdRefitVeh(v));

			if (CmdFailed(cost)) {
				v->leave_depot_instantly = false; // We ensure that the vehicle stays in the depot
				if (v->owner == _local_player) {
					/* Notify the user that we stopped the vehicle */
					SetDParam(0, _vehicle_type_names[v->type]);
					SetDParam(1, v->unitnumber);
					AddNewsItem(STR_ORDER_REFIT_FAILED, NEWS_FLAGS(NM_SMALL, NF_VIEWPORT|NF_VEHICLE, NT_ADVICE, 0), v->index, 0);
				}
			} else if (v->owner == _local_player && cost != 0) {
				ShowCostOrIncomeAnimation(v->x_pos, v->y_pos, v->z_pos, cost);
			}
		}

		if (HASBIT(t.flags, OFB_PART_OF_ORDERS)) {
			/* Part of orders */
			if (v->type == VEH_TRAIN) v->u.rail.days_since_order_progr = 0;
			v->cur_order_index++;
		} else if (HASBIT(t.flags, OFB_HALT_IN_DEPOT)) {
			/* Force depot visit */
			v->vehstatus |= VS_STOPPED;
			if (v->owner == _local_player) {
				StringID string;

				switch (v->type) {
					case VEH_TRAIN:    string = STR_8814_TRAIN_IS_WAITING_IN_DEPOT; break;
					case VEH_ROAD:     string = STR_9016_ROAD_VEHICLE_IS_WAITING;   break;
					case VEH_SHIP:     string = STR_981C_SHIP_IS_WAITING_IN_DEPOT;  break;
					case VEH_AIRCRAFT: string = STR_A014_AIRCRAFT_IS_WAITING_IN;    break;
					default: NOT_REACHED(); string = STR_EMPTY; // Set the string to something to avoid a compiler warning
				}

				SetDParam(0, v->unitnumber);
				AddNewsItem(string, NEWS_FLAGS(NM_SMALL, NF_VIEWPORT|NF_VEHICLE, NT_ADVICE, 0), v->index, 0);
			}
		}
	}
}

/** Give a custom name to your vehicle
 * @param tile unused
 * @param flags type of operation
 * @param p1 vehicle ID to name
 * @param p2 unused
 */
int32 CmdNameVehicle(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;
	StringID str;

	if (!IsValidVehicleID(p1) || _cmd_text[0] == '\0') return CMD_ERROR;

	v = GetVehicle(p1);

	if (!CheckOwnership(v->owner)) return CMD_ERROR;

	str = AllocateNameUnique(_cmd_text, 2);
	if (str == 0) return CMD_ERROR;

	if (flags & DC_EXEC) {
		StringID old_str = v->string_id;
		v->string_id = str;
		DeleteName(old_str);
		ResortVehicleLists();
		MarkWholeScreenDirty();
	} else {
		DeleteName(str);
	}

	return 0;
}


/** Change the service interval of a vehicle
 * @param tile unused
 * @param flags type of operation
 * @param p1 vehicle ID that is being service-interval-changed
 * @param p2 new service interval
 */
int32 CmdChangeServiceInt(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle* v;
	uint16 serv_int = GetServiceIntervalClamped(p2); /* Double check the service interval from the user-input */

	if (serv_int != p2 || !IsValidVehicleID(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (!CheckOwnership(v->owner)) return CMD_ERROR;

	if (flags & DC_EXEC) {
		v->service_interval = serv_int;
		InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
	}

	return 0;
}


static Rect _old_vehicle_coords;

void BeginVehicleMove(Vehicle *v) {
	_old_vehicle_coords.left = v->left_coord;
	_old_vehicle_coords.top = v->top_coord;
	_old_vehicle_coords.right = v->right_coord;
	_old_vehicle_coords.bottom = v->bottom_coord;
}

void EndVehicleMove(Vehicle *v)
{
	MarkAllViewportsDirty(
		min(_old_vehicle_coords.left,v->left_coord),
		min(_old_vehicle_coords.top,v->top_coord),
		max(_old_vehicle_coords.right,v->right_coord)+1,
		max(_old_vehicle_coords.bottom,v->bottom_coord)+1
	);
}

/* returns true if staying in the same tile */
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

Direction GetDirectionTowards(const Vehicle* v, int x, int y)
{
	Direction dir;
	DirDiff dirdiff;
	int i = 0;

	if (y >= v->y_pos) {
		if (y != v->y_pos) i+=3;
		i+=3;
	}

	if (x >= v->x_pos) {
		if (x != v->x_pos) i++;
		i++;
	}

	dir = v->direction;

	dirdiff = DirDifference(_new_direction_table[i], dir);
	if (dirdiff == DIRDIFF_SAME) return dir;
	return ChangeDir(dir, dirdiff > DIRDIFF_REVERSE ? DIRDIFF_45LEFT : DIRDIFF_45RIGHT);
}

Trackdir GetVehicleTrackdir(const Vehicle* v)
{
	if (v->vehstatus & VS_CRASHED) return INVALID_TRACKDIR;

	switch (v->type) {
		case VEH_TRAIN:
			if (v->u.rail.track == TRACK_BIT_DEPOT) // We'll assume the train is facing outwards
				return DiagdirToDiagTrackdir(GetRailDepotDirection(v->tile)); // Train in depot

			if (v->u.rail.track == TRACK_BIT_WORMHOLE) // train in tunnel, so just use his direction and assume a diagonal track
				return DiagdirToDiagTrackdir(DirToDiagDir(v->direction));

			return TrackDirectionToTrackdir(FindFirstTrack(v->u.rail.track), v->direction);

		case VEH_SHIP:
			if (IsShipInDepot(v))
				// We'll assume the ship is facing outwards
				return DiagdirToDiagTrackdir(GetShipDepotDirection(v->tile));

			return TrackDirectionToTrackdir(FindFirstTrack(v->u.ship.state), v->direction);

		case VEH_ROAD:
			if (IsRoadVehInDepot(v)) // We'll assume the road vehicle is facing outwards
				return DiagdirToDiagTrackdir(GetRoadDepotDirection(v->tile));

			if (IsStandardRoadStopTile(v->tile)) // We'll assume the road vehicle is facing outwards
				return DiagdirToDiagTrackdir(GetRoadStopDir(v->tile)); // Road vehicle in a station

			if (IsDriveThroughStopTile(v->tile)) return DiagdirToDiagTrackdir(DirToDiagDir(v->direction));

			/* If vehicle's state is a valid track direction (vehicle is not turning around) return it */
			if (!IsReversingRoadTrackdir((Trackdir)v->u.road.state)) return (Trackdir)v->u.road.state;

			/* Vehicle is turning around, get the direction from vehicle's direction */
			return DiagdirToDiagTrackdir(DirToDiagDir(v->direction));

		/* case VEH_AIRCRAFT: case VEH_SPECIAL: case VEH_DISASTER: */
		default: return INVALID_TRACKDIR;
	}
}

/**
 * Returns some meta-data over the to be entered tile.
 * @see VehicleEnterTileStatus to see what the bits in the return value mean.
 */
uint32 VehicleEnterTile(Vehicle *v, TileIndex tile, int x, int y)
{
	return _tile_type_procs[GetTileType(tile)]->vehicle_enter_tile_proc(v, tile, x, y);
}

UnitID GetFreeUnitNumber(byte type)
{
	UnitID unit, max = 0;
	const Vehicle *u;
	static bool *cache = NULL;
	static UnitID gmax = 0;

	switch (type) {
		case VEH_TRAIN:    max = _patches.max_trains; break;
		case VEH_ROAD:     max = _patches.max_roadveh; break;
		case VEH_SHIP:     max = _patches.max_ships; break;
		case VEH_AIRCRAFT: max = _patches.max_aircraft; break;
		default: NOT_REACHED();
	}

	if (max == 0) {
		/* we can't build any of this kind of vehicle, so we just return 1 instead of looking for a free number
		 * a max of 0 will cause the following code to write to a NULL pointer
		 * We know that 1 is bigger than the max allowed vehicle number, so it's the same as returning something, that is too big
		 */
		return 1;
	}

	if (max > gmax) {
		gmax = max;
		free(cache);
		cache = MallocT<bool>(max + 1);
	}

	/* Clear the cache */
	memset(cache, 0, (max + 1) * sizeof(*cache));

	/* Fill the cache */
	FOR_ALL_VEHICLES(u) {
		if (u->type == type && u->owner == _current_player && u->unitnumber != 0 && u->unitnumber <= max)
			cache[u->unitnumber] = true;
	}

	/* Find the first unused unit number */
	for (unit = 1; unit <= max; unit++) {
		if (!cache[unit]) break;
	}

	return unit;
}


const Livery *GetEngineLivery(EngineID engine_type, PlayerID player, EngineID parent_engine_type, const Vehicle *v)
{
	const Player *p = GetPlayer(player);
	LiveryScheme scheme = LS_DEFAULT;
	CargoID cargo_type = v == NULL ? (CargoID)CT_INVALID : v->cargo_type;

	/* The default livery is always available for use, but its in_use flag determines
	 * whether any _other_ liveries are in use. */
	if (p->livery[LS_DEFAULT].in_use && (_patches.liveries == 2 || (_patches.liveries == 1 && player == _local_player))) {
		/* Determine the livery scheme to use */
		switch (GetEngine(engine_type)->type) {
			case VEH_TRAIN: {
				const RailVehicleInfo *rvi = RailVehInfo(engine_type);

				switch (rvi->railtype) {
					default: NOT_REACHED();
					case RAILTYPE_RAIL:
					case RAILTYPE_ELECTRIC:
					{
						if (cargo_type == CT_INVALID) cargo_type = rvi->cargo_type;
						if (rvi->railveh_type == RAILVEH_WAGON) {
							if (cargo_type == CT_PASSENGERS || cargo_type == CT_MAIL || cargo_type == CT_VALUABLES) {
								if (parent_engine_type == INVALID_ENGINE) {
									scheme = LS_PASSENGER_WAGON_STEAM;
								} else {
									switch (RailVehInfo(parent_engine_type)->engclass) {
										case 0: scheme = LS_PASSENGER_WAGON_STEAM; break;
										case 1: scheme = LS_PASSENGER_WAGON_DIESEL; break;
										case 2: scheme = LS_PASSENGER_WAGON_ELECTRIC; break;
									}
								}
							} else {
								scheme = LS_FREIGHT_WAGON;
							}
						} else {
							bool is_mu = HASBIT(_engine_info[engine_type].misc_flags, EF_RAIL_IS_MU);

							switch (rvi->engclass) {
								case 0: scheme = LS_STEAM; break;
								case 1: scheme = is_mu ? LS_DMU : LS_DIESEL; break;
								case 2: scheme = is_mu ? LS_EMU : LS_ELECTRIC; break;
							}
						}
						break;
					}

					case RAILTYPE_MONO: scheme = LS_MONORAIL; break;
					case RAILTYPE_MAGLEV: scheme = LS_MAGLEV; break;
				}
				break;
			}

			case VEH_ROAD: {
				const RoadVehicleInfo *rvi = RoadVehInfo(engine_type);
				if (cargo_type == CT_INVALID) cargo_type = rvi->cargo_type;
				scheme = IsCargoInClass(cargo_type, CC_PASSENGERS) ? LS_BUS : LS_TRUCK;
				break;
			}

			case VEH_SHIP: {
				const ShipVehicleInfo *svi = ShipVehInfo(engine_type);
				if (cargo_type == CT_INVALID) cargo_type = svi->cargo_type;
				scheme = IsCargoInClass(cargo_type, CC_PASSENGERS) ? LS_PASSENGER_SHIP : LS_FREIGHT_SHIP;
				break;
			}

			case VEH_AIRCRAFT: {
				const AircraftVehicleInfo *avi = AircraftVehInfo(engine_type);
				if (cargo_type == CT_INVALID) cargo_type = CT_PASSENGERS;
				switch (avi->subtype) {
					case AIR_HELI: scheme = LS_HELICOPTER; break;
					case AIR_CTOL: scheme = LS_SMALL_PLANE; break;
					case AIR_CTOL | AIR_FAST: scheme = LS_LARGE_PLANE; break;
				}
				break;
			}
		}

		/* Switch back to the default scheme if the resolved scheme is not in use */
		if (!p->livery[scheme].in_use) scheme = LS_DEFAULT;
	}

	return &p->livery[scheme];
}


static SpriteID GetEngineColourMap(EngineID engine_type, PlayerID player, EngineID parent_engine_type, const Vehicle *v)
{
	SpriteID map = PAL_NONE;

	/* Check if we should use the colour map callback */
	if (HASBIT(EngInfo(engine_type)->callbackmask, CBM_COLOUR_REMAP)) {
		uint16 callback = GetVehicleCallback(CBID_VEHICLE_COLOUR_MAPPING, 0, 0, engine_type, v);
		/* A return value of 0xC000 is stated to "use the default two-color
		 * maps" which happens to be the failure action too... */
		if (callback != CALLBACK_FAILED && callback != 0xC000) {
			map = GB(callback, 0, 14);
			/* If bit 14 is set, then the company colours are applied to the
			 * map else it's returned as-is. */
			if (!HASBIT(callback, 14)) return map;
		}
	}

	bool twocc = HASBIT(EngInfo(engine_type)->misc_flags, EF_USES_2CC);

	if (map == PAL_NONE) map = twocc ? (SpriteID)SPR_2CCMAP_BASE : (SpriteID)PALETTE_RECOLOR_START;

	const Livery *livery = GetEngineLivery(engine_type, player, parent_engine_type, v);

	map += livery->colour1;
	if (twocc) map += livery->colour2 * 16;

	return map;
}

SpriteID GetEnginePalette(EngineID engine_type, PlayerID player)
{
	return GetEngineColourMap(engine_type, player, INVALID_ENGINE, NULL);
}

SpriteID GetVehiclePalette(const Vehicle *v)
{
	if (v->type == VEH_TRAIN) {
		return GetEngineColourMap(
			(v->u.rail.first_engine != INVALID_ENGINE && (IsArticulatedPart(v) || UsesWagonOverride(v))) ?
				v->u.rail.first_engine : v->engine_type,
			v->owner, v->u.rail.first_engine, v);
	}

	return GetEngineColourMap(v->engine_type, v->owner, INVALID_ENGINE, v);
}

/** Save and load of vehicles */
extern const SaveLoad _common_veh_desc[] = {
	    SLE_VAR(Vehicle, subtype,              SLE_UINT8),

	    SLE_REF(Vehicle, next,                 REF_VEHICLE_OLD),
	    SLE_VAR(Vehicle, string_id,            SLE_STRINGID),
	SLE_CONDVAR(Vehicle, unitnumber,           SLE_FILE_U8  | SLE_VAR_U16,  0, 7),
	SLE_CONDVAR(Vehicle, unitnumber,           SLE_UINT16,                  8, SL_MAX_VERSION),
	    SLE_VAR(Vehicle, owner,                SLE_UINT8),
	SLE_CONDVAR(Vehicle, tile,                 SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	SLE_CONDVAR(Vehicle, tile,                 SLE_UINT32,                  6, SL_MAX_VERSION),
	SLE_CONDVAR(Vehicle, dest_tile,            SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	SLE_CONDVAR(Vehicle, dest_tile,            SLE_UINT32,                  6, SL_MAX_VERSION),

	SLE_CONDVAR(Vehicle, x_pos,                SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	SLE_CONDVAR(Vehicle, x_pos,                SLE_UINT32,                  6, SL_MAX_VERSION),
	SLE_CONDVAR(Vehicle, y_pos,                SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	SLE_CONDVAR(Vehicle, y_pos,                SLE_UINT32,                  6, SL_MAX_VERSION),
	    SLE_VAR(Vehicle, z_pos,                SLE_UINT8),
	    SLE_VAR(Vehicle, direction,            SLE_UINT8),

	SLE_CONDNULL(2,                                                         0, 57),
	    SLE_VAR(Vehicle, spritenum,            SLE_UINT8),
	SLE_CONDNULL(5,                                                         0, 57),
	    SLE_VAR(Vehicle, engine_type,          SLE_UINT16),

	    SLE_VAR(Vehicle, max_speed,            SLE_UINT16),
	    SLE_VAR(Vehicle, cur_speed,            SLE_UINT16),
	    SLE_VAR(Vehicle, subspeed,             SLE_UINT8),
	    SLE_VAR(Vehicle, acceleration,         SLE_UINT8),
	    SLE_VAR(Vehicle, progress,             SLE_UINT8),

	    SLE_VAR(Vehicle, vehstatus,            SLE_UINT8),
	SLE_CONDVAR(Vehicle, last_station_visited, SLE_FILE_U8  | SLE_VAR_U16,  0, 4),
	SLE_CONDVAR(Vehicle, last_station_visited, SLE_UINT16,                  5, SL_MAX_VERSION),

	    SLE_VAR(Vehicle, cargo_type,           SLE_UINT8),
	SLE_CONDVAR(Vehicle, cargo_subtype,        SLE_UINT8,                  35, SL_MAX_VERSION),
	    SLE_VAR(Vehicle, cargo_days,           SLE_UINT8),
	SLE_CONDVAR(Vehicle, cargo_source,         SLE_FILE_U8  | SLE_VAR_U16,  0, 6),
	SLE_CONDVAR(Vehicle, cargo_source,         SLE_UINT16,                  7, SL_MAX_VERSION),
	SLE_CONDVAR(Vehicle, cargo_source_xy,      SLE_UINT32,                 44, SL_MAX_VERSION),
	    SLE_VAR(Vehicle, cargo_cap,            SLE_UINT16),
	    SLE_VAR(Vehicle, cargo_count,          SLE_UINT16),

	    SLE_VAR(Vehicle, day_counter,          SLE_UINT8),
	    SLE_VAR(Vehicle, tick_counter,         SLE_UINT8),

	    SLE_VAR(Vehicle, cur_order_index,      SLE_UINT8),
	    SLE_VAR(Vehicle, num_orders,           SLE_UINT8),

	/* This next line is for version 4 and prior compatibility.. it temporarily reads
	    type and flags (which were both 4 bits) into type. Later on this is
	    converted correctly */
	SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, type), SLE_UINT8,                 0, 4),
	SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, dest), SLE_FILE_U8 | SLE_VAR_U16, 0, 4),

	/* Orders for version 5 and on */
	SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, type),  SLE_UINT8,  5, SL_MAX_VERSION),
	SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, flags), SLE_UINT8,  5, SL_MAX_VERSION),
	SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, dest),  SLE_UINT16, 5, SL_MAX_VERSION),

	/* Refit in current order */
	SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, refit_cargo),    SLE_UINT8, 36, SL_MAX_VERSION),
	SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, refit_subtype),  SLE_UINT8, 36, SL_MAX_VERSION),

	    SLE_REF(Vehicle, orders,               REF_ORDER),

	SLE_CONDVAR(Vehicle, age,                  SLE_FILE_U16 | SLE_VAR_I32,  0, 30),
	SLE_CONDVAR(Vehicle, age,                  SLE_INT32,                  31, SL_MAX_VERSION),
	SLE_CONDVAR(Vehicle, max_age,              SLE_FILE_U16 | SLE_VAR_I32,  0, 30),
	SLE_CONDVAR(Vehicle, max_age,              SLE_INT32,                  31, SL_MAX_VERSION),
	SLE_CONDVAR(Vehicle, date_of_last_service, SLE_FILE_U16 | SLE_VAR_I32,  0, 30),
	SLE_CONDVAR(Vehicle, date_of_last_service, SLE_INT32,                  31, SL_MAX_VERSION),
	SLE_CONDVAR(Vehicle, service_interval,     SLE_FILE_U16 | SLE_VAR_I32,  0, 30),
	SLE_CONDVAR(Vehicle, service_interval,     SLE_INT32,                  31, SL_MAX_VERSION),
	    SLE_VAR(Vehicle, reliability,          SLE_UINT16),
	    SLE_VAR(Vehicle, reliability_spd_dec,  SLE_UINT16),
	    SLE_VAR(Vehicle, breakdown_ctr,        SLE_UINT8),
	    SLE_VAR(Vehicle, breakdown_delay,      SLE_UINT8),
	    SLE_VAR(Vehicle, breakdowns_since_last_service, SLE_UINT8),
	    SLE_VAR(Vehicle, breakdown_chance,     SLE_UINT8),
	SLE_CONDVAR(Vehicle, build_year,           SLE_FILE_U8 | SLE_VAR_I32,  0, 30),
	SLE_CONDVAR(Vehicle, build_year,           SLE_INT32,                 31, SL_MAX_VERSION),

	    SLE_VAR(Vehicle, load_unload_time_rem, SLE_UINT16),
	SLE_CONDVAR(Vehicle, cargo_paid_for,       SLE_UINT16,                45, SL_MAX_VERSION),
	SLE_CONDVAR(Vehicle, vehicle_flags,        SLE_UINT8,                 40, SL_MAX_VERSION),

	    SLE_VAR(Vehicle, profit_this_year,     SLE_INT32),
	    SLE_VAR(Vehicle, profit_last_year,     SLE_INT32),
	SLE_CONDVAR(Vehicle, cargo_feeder_share,   SLE_INT32,                 51, SL_MAX_VERSION),
	SLE_CONDVAR(Vehicle, cargo_loaded_at_xy,   SLE_UINT32,                51, SL_MAX_VERSION),
	    SLE_VAR(Vehicle, value,                SLE_UINT32),

	    SLE_VAR(Vehicle, random_bits,          SLE_UINT8),
	    SLE_VAR(Vehicle, waiting_triggers,     SLE_UINT8),

	    SLE_REF(Vehicle, next_shared,          REF_VEHICLE),
	    SLE_REF(Vehicle, prev_shared,          REF_VEHICLE),

	/* reserve extra space in savegame here. (currently 10 bytes) */
	SLE_CONDNULL(10,                                                       2, SL_MAX_VERSION),

	SLE_END()
};


static const SaveLoad _train_desc[] = {
	SLE_WRITEBYTE(Vehicle, type, VEH_TRAIN, 0), // Train type. VEH_TRAIN in mem, 0 in file.
	SLE_INCLUDEX(0, INC_VEHICLE_COMMON),
	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRail, crash_anim_pos),         SLE_UINT16),
	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRail, force_proceed),          SLE_UINT8),
	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRail, railtype),               SLE_UINT8),
	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRail, track),                  SLE_UINT8),

	SLE_CONDVARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRail, flags),                  SLE_UINT8,  2, SL_MAX_VERSION),
	SLE_CONDVARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRail, days_since_order_progr), SLE_UINT16, 2, SL_MAX_VERSION),

	SLE_CONDNULL(2, 2, 19),
	/* reserve extra space in savegame here. (currently 11 bytes) */
	SLE_CONDNULL(11, 2, SL_MAX_VERSION),

	SLE_END()
};

static const SaveLoad _roadveh_desc[] = {
	SLE_WRITEBYTE(Vehicle, type, VEH_ROAD, 1), // Road type. VEH_ROAD in mem, 1 in file.
	SLE_INCLUDEX(0, INC_VEHICLE_COMMON),
	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRoad, state),          SLE_UINT8),
	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRoad, frame),          SLE_UINT8),
	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRoad, blocked_ctr),    SLE_UINT16),
	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRoad, overtaking),     SLE_UINT8),
	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRoad, overtaking_ctr), SLE_UINT8),
	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRoad, crashed_ctr),    SLE_UINT16),
	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRoad, reverse_ctr),    SLE_UINT8),

	SLE_CONDREFX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRoad, slot),     REF_ROADSTOPS, 6, SL_MAX_VERSION),
	SLE_CONDNULL(1,                                                                     6, SL_MAX_VERSION),
	SLE_CONDVARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleRoad, slot_age), SLE_UINT8,     6, SL_MAX_VERSION),
	/* reserve extra space in savegame here. (currently 16 bytes) */
	SLE_CONDNULL(16,                                                                    2, SL_MAX_VERSION),

	SLE_END()
};

static const SaveLoad _ship_desc[] = {
	SLE_WRITEBYTE(Vehicle, type, VEH_SHIP, 2), // Ship type. VEH_SHIP in mem, 2 in file.
	SLE_INCLUDEX(0, INC_VEHICLE_COMMON),
	SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleShip, state), SLE_UINT8),

	/* reserve extra space in savegame here. (currently 16 bytes) */
	SLE_CONDNULL(16, 2, SL_MAX_VERSION),

	SLE_END()
};

static const SaveLoad _aircraft_desc[] = {
	SLE_WRITEBYTE(Vehicle, type, VEH_AIRCRAFT, 3), // Aircraft type. VEH_AIRCRAFT in mem, 3 in file.
	SLE_INCLUDEX(0, INC_VEHICLE_COMMON),
	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleAir, crashed_counter), SLE_UINT16),
	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleAir, pos),             SLE_UINT8),

	SLE_CONDVARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleAir, targetairport),   SLE_FILE_U8 | SLE_VAR_U16, 0, 4),
	SLE_CONDVARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleAir, targetairport),   SLE_UINT16,                5, SL_MAX_VERSION),

	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleAir, state),           SLE_UINT8),

	SLE_CONDVARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleAir, previous_pos),    SLE_UINT8,                 2, SL_MAX_VERSION),

	/* reserve extra space in savegame here. (currently 15 bytes) */
	SLE_CONDNULL(15,                                                                                      2, SL_MAX_VERSION),

	SLE_END()
};

static const SaveLoad _special_desc[] = {
	SLE_WRITEBYTE(Vehicle,type,VEH_SPECIAL, 4),

	    SLE_VAR(Vehicle, subtype,       SLE_UINT8),

	SLE_CONDVAR(Vehicle, tile,          SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(Vehicle, tile,          SLE_UINT32,                 6, SL_MAX_VERSION),

	SLE_CONDVAR(Vehicle, x_pos,         SLE_FILE_I16 | SLE_VAR_I32, 0, 5),
	SLE_CONDVAR(Vehicle, x_pos,         SLE_INT32,                  6, SL_MAX_VERSION),
	SLE_CONDVAR(Vehicle, y_pos,         SLE_FILE_I16 | SLE_VAR_I32, 0, 5),
	SLE_CONDVAR(Vehicle, y_pos,         SLE_INT32,                  6, SL_MAX_VERSION),
	    SLE_VAR(Vehicle, z_pos,         SLE_UINT8),

	    SLE_VAR(Vehicle, cur_image,     SLE_UINT16),
	SLE_CONDNULL(5,                                                 0, 57),
	    SLE_VAR(Vehicle, progress,      SLE_UINT8),
	    SLE_VAR(Vehicle, vehstatus,     SLE_UINT8),

	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleSpecial, unk0), SLE_UINT16),
	    SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleSpecial, unk2), SLE_UINT8),

	/* reserve extra space in savegame here. (currently 16 bytes) */
	SLE_CONDNULL(16, 2, SL_MAX_VERSION),

	SLE_END()
};

static const SaveLoad _disaster_desc[] = {
	SLE_WRITEBYTE(Vehicle, type, VEH_DISASTER, 5),

	    SLE_REF(Vehicle, next,          REF_VEHICLE_OLD),

	    SLE_VAR(Vehicle, subtype,       SLE_UINT8),
	SLE_CONDVAR(Vehicle, tile,          SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	SLE_CONDVAR(Vehicle, tile,          SLE_UINT32,                  6, SL_MAX_VERSION),
	SLE_CONDVAR(Vehicle, dest_tile,     SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	SLE_CONDVAR(Vehicle, dest_tile,     SLE_UINT32,                  6, SL_MAX_VERSION),

	SLE_CONDVAR(Vehicle, x_pos,         SLE_FILE_I16 | SLE_VAR_I32,  0, 5),
	SLE_CONDVAR(Vehicle, x_pos,         SLE_INT32,                   6, SL_MAX_VERSION),
	SLE_CONDVAR(Vehicle, y_pos,         SLE_FILE_I16 | SLE_VAR_I32,  0, 5),
	SLE_CONDVAR(Vehicle, y_pos,         SLE_INT32,                   6, SL_MAX_VERSION),
	    SLE_VAR(Vehicle, z_pos,         SLE_UINT8),
	    SLE_VAR(Vehicle, direction,     SLE_UINT8),

	SLE_CONDNULL(5,                                                  0, 57),
	    SLE_VAR(Vehicle, owner,         SLE_UINT8),
	    SLE_VAR(Vehicle, vehstatus,     SLE_UINT8),
	SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, dest), SLE_FILE_U8 | SLE_VAR_U16, 0, 4),
	SLE_CONDVARX(cpp_offsetof(Vehicle, current_order) + cpp_offsetof(Order, dest), SLE_UINT16,                5, SL_MAX_VERSION),

	    SLE_VAR(Vehicle, cur_image,     SLE_UINT16),
	SLE_CONDVAR(Vehicle, age,           SLE_FILE_U16 | SLE_VAR_I32,  0, 30),
	SLE_CONDVAR(Vehicle, age,           SLE_INT32,                  31, SL_MAX_VERSION),
	    SLE_VAR(Vehicle, tick_counter,  SLE_UINT8),

	   SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleDisaster, image_override), SLE_UINT16),
	   SLE_VARX(cpp_offsetof(Vehicle, u) + cpp_offsetof(VehicleDisaster, unk2),           SLE_UINT16),

	/* reserve extra space in savegame here. (currently 16 bytes) */
	SLE_CONDNULL(16,                                                 2, SL_MAX_VERSION),

	SLE_END()
};


static const void *_veh_descs[] = {
	_train_desc,
	_roadveh_desc,
	_ship_desc,
	_aircraft_desc,
	_special_desc,
	_disaster_desc,
};

/** Will be called when the vehicles need to be saved. */
static void Save_VEHS()
{
	Vehicle *v;
	/* Write the vehicles */
	FOR_ALL_VEHICLES(v) {
		SlSetArrayIndex(v->index);
		SlObject(v, (SaveLoad*)_veh_descs[v->type]);
	}
}

/** Will be called when vehicles need to be loaded. */
static void Load_VEHS()
{
	int index;
	Vehicle *v;

	while ((index = SlIterateArray()) != -1) {
		Vehicle *v;

		if (!AddBlockIfNeeded(&_Vehicle_pool, index))
			error("Vehicles: failed loading savegame: too many vehicles");

		v = GetVehicle(index);
		SlObject(v, (SaveLoad*)_veh_descs[SlReadByte()]);

		switch (v->type) {
			case VEH_TRAIN:    v = new (v) Train();           break;
			case VEH_ROAD:     v = new (v) RoadVehicle();     break;
			case VEH_SHIP:     v = new (v) Ship();            break;
			case VEH_AIRCRAFT: v = new (v) Aircraft();        break;
			case VEH_SPECIAL:  v = new (v) SpecialVehicle();  break;
			case VEH_DISASTER: v = new (v) DisasterVehicle(); break;
			case VEH_INVALID:  v = new (v) InvalidVehicle();  break;
		}

		/* Old savegames used 'last_station_visited = 0xFF' */
		if (CheckSavegameVersion(5) && v->last_station_visited == 0xFF)
			v->last_station_visited = INVALID_STATION;

		if (CheckSavegameVersion(5)) {
			/* Convert the current_order.type (which is a mix of type and flags, because
			 *  in those versions, they both were 4 bits big) to type and flags */
			v->current_order.flags = (v->current_order.type & 0xF0) >> 4;
			v->current_order.type.m_val &= 0x0F;
		}
	}

	/* Check for shared order-lists (we now use pointers for that) */
	if (CheckSavegameVersionOldStyle(5, 2)) {
		FOR_ALL_VEHICLES(v) {
			Vehicle *u;

			FOR_ALL_VEHICLES_FROM(u, v->index + 1) {
				/* If a vehicle has the same orders, add the link to eachother
				 *  in both vehicles */
				if (v->orders == u->orders) {
					v->next_shared = u;
					u->prev_shared = v;
					break;
				}
			}
		}
	}
}

extern const ChunkHandler _veh_chunk_handlers[] = {
	{ 'VEHS', Save_VEHS, Load_VEHS, CH_SPARSE_ARRAY | CH_LAST},
};

void Vehicle::BeginLoading()
{
	assert(IsTileType(tile, MP_STATION) || type == VEH_SHIP);

	if (this->current_order.type == OT_GOTO_STATION &&
			this->current_order.dest == this->last_station_visited) {
		/* Arriving at the ordered station.
		 * Keep the load/unload flags, as we (obviously) still need them. */
		this->current_order.flags &= OF_FULL_LOAD | OF_UNLOAD | OF_TRANSFER;

		/* Furthermore add the Non Stop flag to mark that this station
		 * is the actual destination of the vehicle, which is (for example)
		 * necessary to be known for HandleTrainLoading to determine
		 * whether the train is lost or not; not marking a train lost
		 * that arrives at random stations is bad. */
		this->current_order.flags |= OF_NON_STOP;
	} else {
		/* This is just an unordered intermediate stop */
		this->current_order.flags = 0;
	}

	current_order.type = OT_LOADING;
	GetStation(this->last_station_visited)->loading_vehicles.push_back(this);

	SET_EXPENSES_TYPE(this->GetExpenseType(true));
	VehiclePayment(this);

	InvalidateWindow(this->GetVehicleListWindowClass(), this->owner);
	InvalidateWindowWidget(WC_VEHICLE_VIEW, this->index, STATUS_BAR);
	InvalidateWindow(WC_VEHICLE_DETAILS, this->index);
	InvalidateWindow(WC_STATION_VIEW, this->last_station_visited);

	GetStation(this->last_station_visited)->MarkTilesDirty();
	this->MarkDirty();
}

void Vehicle::LeaveStation()
{
	assert(current_order.type == OT_LOADING);
	current_order.type = OT_LEAVESTATION;
	current_order.flags = 0;
	GetStation(this->last_station_visited)->loading_vehicles.remove(this);
}


void SpecialVehicle::UpdateDeltaXY(Direction direction)
{
	this->x_offs        = 0;
	this->y_offs        = 0;
	this->sprite_width  = 1;
	this->sprite_height = 1;
	this->z_height      = 1;
}
