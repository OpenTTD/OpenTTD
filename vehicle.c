/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "road_map.h"
#include "roadveh.h"
#include "ship.h"
#include "spritecache.h"
#include "table/sprites.h"
#include "table/strings.h"
#include "functions.h"
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
#include "network.h"
#include "yapf/yapf.h"
#include "date.h"
#include "newgrf_engine.h"
#include "newgrf_sound.h"

#define INVALID_COORD (-0x8000)
#define GEN_HASH(x, y) ((GB((y), 6, 6) << 6) + GB((x), 7, 6))

/*
 * These command macros are used to call vehicle type specific commands with non type specific commands
 * it should be used like: DoCommandP(x, y, p1, p2, flags, CMD_STARTSTOP_VEH(v->type))
 * that line will start/stop a vehicle nomatter what type it is
 * VEH_Train is used as an offset because the vehicle type values doesn't start with 0
 */

#define CMD_BUILD_VEH(x) _veh_build_proc_table[ x - VEH_Train]
#define CMD_SELL_VEH(x)  _veh_sell_proc_table [ x - VEH_Train]
#define CMD_REFIT_VEH(x) _veh_refit_proc_table[ x - VEH_Train]

static const uint32 _veh_build_proc_table[] = {
	CMD_BUILD_RAIL_VEHICLE,
	CMD_BUILD_ROAD_VEH,
	CMD_BUILD_SHIP,
	CMD_BUILD_AIRCRAFT,
};
static const uint32 _veh_sell_proc_table[] = {
	CMD_SELL_RAIL_WAGON,
	CMD_SELL_ROAD_VEH,
	CMD_SELL_SHIP,
	CMD_SELL_AIRCRAFT,
};

static const uint32 _veh_refit_proc_table[] = {
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
	for (v = GetVehicle(start_item); v != NULL; v = (v->index + 1U < GetVehiclePoolSize()) ? GetVehicle(v->index + 1) : NULL) v->index = start_item++;
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
		return false; /* Crashed vehicles don't need service anymore */

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
		case VEH_Train:    return STR_8803_TRAIN_IN_THE_WAY;
		case VEH_Road:     return STR_9000_ROAD_VEHICLE_IN_THE_WAY;
		case VEH_Aircraft: return STR_A015_AIRCRAFT_IN_THE_WAY;
		default:           return STR_980E_SHIP_IN_THE_WAY;
	}
}

static void *EnsureNoVehicleProc(Vehicle *v, void *data)
{
	if (v->tile != *(const TileIndex*)data || v->type == VEH_Disaster)
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
	const TileInfo *ti = data;

	if (v->tile != ti->tile || v->type == VEH_Disaster) return NULL;
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

	return VehicleFromPos(tile, &ti, EnsureNoVehicleProcZ);
}

Vehicle *FindVehicleBetween(TileIndex from, TileIndex to, byte z)
{
	int x1 = TileX(from);
	int y1 = TileY(from);
	int x2 = TileX(to);
	int y2 = TileY(to);
	Vehicle *veh;

	/* Make sure x1 < x2 or y1 < y2 */
	if (x1 > x2 || y1 > y2) {
		intswap(x1,x2);
		intswap(y1,y2);
	}
	FOR_ALL_VEHICLES(veh) {
		if ((veh->type == VEH_Train || veh->type == VEH_Road) && (z==0xFF || veh->z_pos == z)) {
			if ((veh->x_pos>>4) >= x1 && (veh->x_pos>>4) <= x2 &&
					(veh->y_pos>>4) >= y1 && (veh->y_pos>>4) <= y2) {
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

// Called after load to update coordinates
void AfterLoadVehicles(void)
{
	Vehicle *v;

	FOR_ALL_VEHICLES(v) {
		v->first = NULL;
		if (v->type == VEH_Train) v->u.rail.first_engine = INVALID_ENGINE;
	}

	FOR_ALL_VEHICLES(v) {
		if (v->type == VEH_Train && (IsFrontEngine(v) || IsFreeWagon(v)))
			TrainConsistChanged(v);
	}

	FOR_ALL_VEHICLES(v) {
		switch (v->type) {
			case VEH_Train: v->cur_image = GetTrainImage(v, v->direction); break;
			case VEH_Road: v->cur_image = GetRoadVehImage(v, v->direction); break;
			case VEH_Ship: v->cur_image = GetShipImage(v, v->direction); break;
			case VEH_Aircraft:
				if (v->subtype == 0 || v->subtype == 2) {
					v->cur_image = GetAircraftImage(v, v->direction);
					if (v->next != NULL) v->next->cur_image = v->cur_image;
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

	v->left_coord = INVALID_COORD;
	v->first = NULL;
	v->next = NULL;
	v->next_hash = INVALID_VEHICLE;
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
byte VehicleRandomBits(void)
{
	return GB(Random(), 0, 8);
}

Vehicle *ForceAllocateSpecialVehicle(void)
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

/*
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
	 * TODO - This is just a temporary stage, this will be removed. */
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


Vehicle *AllocateVehicle(void)
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


static VehicleID _vehicle_position_hash[0x1000];

void *VehicleFromPos(TileIndex tile, void *data, VehicleFromPosProc *proc)
{
	Point pt = RemapCoords(TileX(tile) * TILE_SIZE, TileY(tile) * TILE_SIZE, 0);

	// The hash area to scan
	const int xl = GB(pt.x - 174, 7, 6);
	const int xu = GB(pt.x + 104, 7, 6);
	const int yl = GB(pt.y - 294, 6, 6) << 6;
	const int yu = GB(pt.y +  56, 6, 6) << 6;

	int x;
	int y;

	for (y = yl;; y = (y + (1 << 6)) & (0x3F << 6)) {
		for (x = xl;; x = (x + 1) & 0x3F) {
			VehicleID veh = _vehicle_position_hash[(x + y) & 0xFFFF];

			while (veh != INVALID_VEHICLE) {
				Vehicle *v = GetVehicle(veh);
				void* a = proc(v, data);

				if (a != NULL) return a;
				veh = v->next_hash;
			}

			if (x == xu) break;
		}

		if (y == yu) break;
	}
	return NULL;
}


static void UpdateVehiclePosHash(Vehicle* v, int x, int y)
{
	VehicleID *old_hash, *new_hash;
	int old_x = v->left_coord;
	int old_y = v->top_coord;
	Vehicle *u;

	new_hash = (x == INVALID_COORD) ? NULL : &_vehicle_position_hash[GEN_HASH(x,y)];
	old_hash = (old_x == INVALID_COORD) ? NULL : &_vehicle_position_hash[GEN_HASH(old_x, old_y)];

	if (old_hash == new_hash) return;

	/* remove from hash table? */
	if (old_hash != NULL) {
		Vehicle *last = NULL;
		VehicleID idx = *old_hash;
		while ((u = GetVehicle(idx)) != v) {
			idx = u->next_hash;
			assert(idx != INVALID_VEHICLE);
			last = u;
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
		*new_hash = v->index;
	}
}

void InitializeVehicles(void)
{
	uint i;

	/* Clean the vehicle pool, and reserve enough blocks
	 *  for the special vehicles, plus one for all the other
	 *  vehicles (which is increased on-the-fly) */
	CleanPool(&_Vehicle_pool);
	AddBlockToPool(&_Vehicle_pool);
	for (i = 0; i < BLOCKS_FOR_SPECIAL_VEHICLES; i++)
		AddBlockToPool(&_Vehicle_pool);

	for (i = 0; i < lengthof(_vehicle_position_hash); i++) {
		_vehicle_position_hash[i] = INVALID_VEHICLE;
	}
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

	FOR_ALL_VEHICLES(u) if (u->type == VEH_Train && u->next == v) return u;

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

	// Check to see if this is the first
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

	if (v->first != NULL) {
		if (IsFrontEngine(v->first) || IsFreeWagon(v->first)) return v->first;

		DEBUG(misc, 0) ("v->first cache faulty. We shouldn't be here, rebuilding cache!");
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
		case VEH_Aircraft: return (v->subtype <= 2); // don't count plane shadows and helicopter rotors
		case VEH_Train:
			return !IsArticulatedPart(v) && // tenders and other articulated parts
			(!IsMultiheaded(v) || IsTrainEngine(v)); // rear parts of multiheaded engines
		case VEH_Road:
		case VEH_Ship:
			return true;
		default: return false; // Only count player buildable vehicles
	}
}

void DestroyVehicle(Vehicle *v)
{
	if (IsEngineCountable(v)) GetPlayer(v->owner)->num_engines[v->engine_type]--;

	DeleteVehicleNews(v->index, INVALID_STRING_ID);

	DeleteName(v->string_id);
	if (v->type == VEH_Road) ClearSlot(v);

	if (v->type != VEH_Train || (v->type == VEH_Train && (IsFrontEngine(v) || IsFreeWagon(v)))) {
		InvalidateWindowData(WC_VEHICLE_DEPOT, v->tile);
	}

	UpdateVehiclePosHash(v, INVALID_COORD, 0);
	v->next_hash = INVALID_VEHICLE;
	if (v->orders != NULL) DeleteVehicleOrders(v);

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
static int32 MaybeReplaceVehicle(Vehicle *v, bool check, bool display_costs);

// head of the linked list to tell what vehicles that visited a depot in a tick
static Vehicle* _first_veh_in_depot_list;

/** Adds a vehicle to the list of vehicles, that visited a depot this tick
 * @param *v vehicle to add
 */
void VehicleEnteredDepotThisTick(Vehicle *v)
{
	// we need to set v->leave_depot_instantly as we have no control of it's contents at this time
	if (HASBIT(v->current_order.flags, OFB_HALT_IN_DEPOT) && !HASBIT(v->current_order.flags, OFB_PART_OF_ORDERS) && v->current_order.type == OT_GOTO_DEPOT) {
		// we keep the vehicle in the depot since the user ordered it to stay
		v->leave_depot_instantly = false;
	} else {
		// the vehicle do not plan on stopping in the depot, so we stop it to ensure that it will not reserve the path
		// out of the depot before we might autoreplace it to a different engine. The new engine would not own the reserved path
		// we store that we stopped the vehicle, so autoreplace can start it again
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

void CallVehicleTicks(void)
{
	Vehicle *v;

#ifdef ENABLE_NETWORK
	// hotfix for desync problem:
	//  for MP games invalidate the YAPF cache every tick to keep it exactly the same on the server and all clients
	if (_networking) {
		YapfNotifyTrackLayoutChange(0, 0);
	}
#endif //ENABLE_NETWORK

	_first_veh_in_depot_list = NULL; // now we are sure it's initialized at the start of each tick

	FOR_ALL_VEHICLES(v) {
		_vehicle_tick_procs[v->type - 0x10](v);

		switch (v->type) {
			case VEH_Train:
			case VEH_Road:
			case VEH_Aircraft:
			case VEH_Ship:
				if (v->type == VEH_Train && IsTrainWagon(v)) continue;
				if (v->type == VEH_Aircraft && v->subtype > 0) continue;

				v->motion_counter += (v->direction & 1) ? (v->cur_speed * 3) / 4 : v->cur_speed;
				/* Play a running sound if the motion counter passes 256 (Do we not skip sounds?) */
				if (GB(v->motion_counter, 0, 8) < v->cur_speed) PlayVehicleSound(v, VSE_RUNNING);

				/* Play an alterate running sound every 16 ticks */
				if (GB(v->tick_counter, 0, 4) == 0) PlayVehicleSound(v, v->cur_speed > 0 ? VSE_RUNNING_16 : VSE_STOPPED_16);
		}
	}

	// now we handle all the vehicles that entered a depot this tick
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

	//special handling of aircraft

	//if the aircraft carries passengers and is NOT full, then
	//continue loading, no matter how much mail is in
	if (v->type == VEH_Aircraft &&
			v->cargo_type == CT_PASSENGERS &&
			v->cargo_cap != v->cargo_count) {
		return true;
	}

	// patch should return "true" to continue loading, i.e. when there is no cargo type that is fully loaded.
	do {
		//Should never happen, but just in case future additions change this
		assert(v->cargo_type<32);

		if (v->cargo_cap != 0) {
			uint32 mask = 1 << v->cargo_type;

			if (v->cargo_cap == v->cargo_count) {
				full |= mask;
			} else {
				not_full |= mask;
			}
		}
	} while ((v = v->next) != NULL);

	// continue loading if there is a non full cargo type and no cargo type that is full
	return not_full && (full & ~not_full) == 0;
}

bool CanFillVehicle(Vehicle *v)
{
	TileIndex tile = v->tile;

	if (IsTileType(tile, MP_STATION) ||
			(v->type == VEH_Ship && (
				IsTileType(TILE_ADDXY(tile,  1,  0), MP_STATION) ||
				IsTileType(TILE_ADDXY(tile, -1,  0), MP_STATION) ||
				IsTileType(TILE_ADDXY(tile,  0,  1), MP_STATION) ||
				IsTileType(TILE_ADDXY(tile,  0, -1), MP_STATION) ||
				IsTileType(TILE_ADDXY(tile, -2,  0), MP_STATION)
			))) {

		// If patch is active, use alternative CanFillVehicle-function
		if (_patches.full_load_any && v->current_order.flags & OF_FULL_LOAD) return CanFillVehicle_FullLoadAny(v);

		do {
			if (v->cargo_count != v->cargo_cap) return true;
		} while ((v = v->next) != NULL);
	}
	return false;
}

/** Check if a given engine type can be refitted to a given cargo
 * @param engine_type Engine type to check
 * @param cid_to check refit to this cargo-type
 * @return true if it is possible, false otherwise
 */
bool CanRefitTo(EngineID engine_type, CargoID cid_to)
{
	CargoID cid = _global_cargo_id[_opt_ptr->landscape][cid_to];
	return HASBIT(EngInfo(engine_type)->refit_mask, cid);
}

/** Find the first cargo type that an engine can be refitted to.
 * @param engine Which engine to find cargo for.
 * @return A climate dependent cargo type. CT_INVALID is returned if not refittable.
 */
CargoID FindFirstRefittableCargo(EngineID engine_type)
{
	CargoID cid;
	uint32 refit_mask = EngInfo(engine_type)->refit_mask;

	if (refit_mask != 0) {
		for (cid = CT_PASSENGERS; cid < NUM_CARGO; cid++) {
			if (HASBIT(refit_mask, _global_cargo_id[_opt_ptr->landscape][cid])) return cid;
		}
	}

	return CT_INVALID;
}

/** Learn the price of refitting a certain engine
* @param engine Which engine to refit
* @return Price for refitting
*/
int32 GetRefitCost(EngineID engine_type)
{
	int32 base_cost = 0;

	switch (GetEngine(engine_type)->type) {
		case VEH_Ship: base_cost = _price.ship_base; break;
		case VEH_Road: base_cost = _price.roadveh_base; break;
		case VEH_Aircraft: base_cost = _price.aircraft_base; break;
		case VEH_Train:
			base_cost = 2 * ((RailVehInfo(engine_type)->flags & RVI_WAGON) ?
							 _price.build_railwagon : _price.build_railvehicle);
			break;
		default: NOT_REACHED(); break;
	}
	return (EngInfo(engine_type)->refit_cost * base_cost) >> 10;
}

static void DoDrawVehicle(const Vehicle *v)
{
	uint32 image = v->cur_image;

	if (v->vehstatus & VS_SHADOW) {
		MAKE_TRANSPARENT(image);
	} else if (v->vehstatus & VS_DEFPAL) {
		image |= (v->vehstatus & VS_CRASHED) ? PALETTE_CRASH : GetVehiclePalette(v);
	}

	AddSortableSpriteToDraw(image, v->x_pos + v->x_offs, v->y_pos + v->y_offs,
		v->sprite_width, v->sprite_height, v->z_height, v->z_pos);
}

void ViewportAddVehicles(DrawPixelInfo *dpi)
{
	// The bounding rectangle
	const int l = dpi->left;
	const int r = dpi->left + dpi->width;
	const int t = dpi->top;
	const int b = dpi->top + dpi->height;

	// The hash area to scan
	const int xl = GB(l - 70, 7, 6);
	const int xu = GB(r,      7, 6);
	const int yl = GB(t - 70, 6, 6) << 6;
	const int yu = GB(b,      6, 6) << 6;

	int x;
	int y;

	for (y = yl;; y = (y + (1 << 6)) & (0x3F << 6)) {
		for (x = xl;; x = (x + 1) & 0x3F) {
			VehicleID veh = _vehicle_position_hash[(x + y) & 0xFFFF];

			while (veh != INVALID_VEHICLE) {
				const Vehicle* v = GetVehicle(veh);

				if (!(v->vehstatus & VS_HIDDEN) &&
						l <= v->right_coord &&
						t <= v->bottom_coord &&
						r >= v->left_coord &&
						b >= v->top_coord) {
					DoDrawVehicle(v);
				}
				veh = v->next_hash;
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

typedef struct BulldozerMovement {
	byte direction:2;
	byte image:2;
	byte duration:3;
} BulldozerMovement;

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

typedef struct BubbleMovement {
	int8 x:4;
	int8 y:4;
	int8 z:4;
	byte image:4;
} BubbleMovement;

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
		v->type = VEH_Special;
		v->subtype = type;
		v->x_pos = x;
		v->y_pos = y;
		v->z_pos = z;
		v->z_height = v->sprite_width = v->sprite_height = 1;
		v->x_offs = v->y_offs = 0;
		v->tile = 0;
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
	return CreateEffectVehicle(x, y, GetSlopeZ(x, y) + z, type);
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
	if (v->type == VEH_Ship) rel += 0x6666;

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

	// Do not show getting-old message if autorenew is active
	if (GetPlayer(v->owner)->engine_renew) return;

	SetDParam(0, _vehicle_type_names[v->type - 0x10]);
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
		case VEH_Train:    stop_command = CMD_START_STOP_TRAIN;    break;
		case VEH_Road:     stop_command = CMD_START_STOP_ROADVEH;  break;
		case VEH_Ship:     stop_command = CMD_START_STOP_SHIP;     break;
		case VEH_Aircraft: stop_command = CMD_START_STOP_AIRCRAFT; break;
		default: return CMD_ERROR;
	}

	if (vehicle_list_window) {
		uint16 id = GB(p1, 0, 16);
		uint16 window_type = p2 & VLW_MASK;

		engine_count = GenerateVehicleSortList((const Vehicle***)&vl, &engine_list_length, vehicle_type, _current_player, id, id, id, window_type);
	} else {
		/* Get the list of vehicles in the depot */
		BuildDepotVehicleList(vehicle_type, tile, &vl, &engine_list_length, &engine_count, NULL, NULL, NULL);
	}

	for (i = 0; i < engine_count; i++) {
		const Vehicle *v = vl[i];
		int32 ret;

		if (!!(v->vehstatus & VS_STOPPED) != start_stop) continue;

		if (!vehicle_list_window) {
			if (vehicle_type == VEH_Train) {
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
		case VEH_Train:    sell_command = CMD_SELL_RAIL_WAGON; break;
		case VEH_Road:     sell_command = CMD_SELL_ROAD_VEH;   break;
		case VEH_Ship:     sell_command = CMD_SELL_SHIP;       break;
		case VEH_Aircraft: sell_command = CMD_SELL_AIRCRAFT;   break;
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
 * @param p1 the original vehicle's index
 * @param p2 1 = shared orders, else copied orders
 */
int32 CmdCloneVehicle(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v_front, *v;
	Vehicle *w_front, *w, *w_rear;
	int cost, total_cost = 0;
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

	if (v->type == VEH_Train && (!IsFrontEngine(v) || v->u.rail.crash_anim_pos >= 4400)) return CMD_ERROR;

	// check that we can allocate enough vehicles
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

		cost = DoCommand(tile, v->engine_type, build_argument, flags, CMD_BUILD_VEH(v->type));
		build_argument = 3; // ensure that we only assign a number to the first engine

		if (CmdFailed(cost)) return cost;

		total_cost += cost;

		if (flags & DC_EXEC) {
			w = GetVehicle(_new_vehicle_id);

			if (v->cargo_type != w->cargo_type || v->cargo_subtype != w->cargo_subtype) {
				// we can't pay for refitting because we can't estimate refitting costs for a vehicle before it's build
				// if we pay for it anyway, the cost and the estimated cost will not be the same and we will have an assert
				DoCommand(0, w->index, v->cargo_type | (v->cargo_subtype << 8), flags, CMD_REFIT_VEH(v->type));
			}
			if (v->type == VEH_Train && HASBIT(v->u.rail.flags, VRF_REVERSE_DIRECTION)) {
				SETBIT(w->u.rail.flags, VRF_REVERSE_DIRECTION);
			}

			if (v->type == VEH_Train && !IsFrontEngine(v)) {
				// this s a train car
				// add this unit to the end of the train
				DoCommand(0, (w_rear->index << 16) | w->index, 1, flags, CMD_MOVE_RAIL_VEHICLE);
			} else {
				// this is a front engine or not a train. It need orders
				w_front = w;
				w->service_interval = v->service_interval;
				DoCommand(0, (v->index << 16) | w->index, p2 & 1 ? CO_SHARE : CO_COPY, flags, CMD_CLONE_ORDER);
			}
			w_rear = w; // trains needs to know the last car in the train, so they can add more in next loop
		}
	} while (v->type == VEH_Train && (v = GetNextVehicle(v)) != NULL);

	if (flags & DC_EXEC && v_front->type == VEH_Train) {
		// for trains this needs to be the front engine due to the callback function
		_new_vehicle_id = w_front->index;
	}

	/* Set the expense type last as refitting will make the cost go towards
	 * running costs... */
	SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);
	return total_cost;
}

/*
 * move the cargo from one engine to another if possible
 */
static void MoveVehicleCargo(Vehicle *dest, Vehicle *source)
{
	Vehicle *v = dest;
	int units_moved;

	do {
		do {
			if (source->cargo_type != dest->cargo_type)
				continue; // cargo not compatible

			if (dest->cargo_count == dest->cargo_cap)
				continue; // the destination vehicle is already full

			units_moved = min(source->cargo_count, dest->cargo_cap - dest->cargo_count);
			source->cargo_count -= units_moved;
			dest->cargo_count   += units_moved;
			dest->cargo_source   = source->cargo_source;

			// copy the age of the cargo
			dest->cargo_days   = source->cargo_days;
			dest->day_counter  = source->day_counter;
			dest->tick_counter = source->tick_counter;

		} while (source->cargo_count > 0 && (dest = dest->next) != NULL);
		dest = v;
	} while ((source = source->next) != NULL);
}

static bool VerifyAutoreplaceRefitForOrders(const Vehicle *v, const EngineID engine_type)
{
	const Order *o;
	const Vehicle *u;

	if (v->type == VEH_Train) {
		u = GetFirstVehicleInChain(v);
	} else {
		u = v;
	}

	FOR_VEHICLE_ORDERS(u, o) {
		if (!(o->refit_cargo < NUM_CARGO)) continue;
		if (!CanRefitTo(v->engine_type, o->refit_cargo)) continue;
		if (!CanRefitTo(engine_type, o->refit_cargo)) return false;
	}

	return true;
}

/**
 * Function to find what type of cargo to refit to when autoreplacing
 * @param *v Original vehicle, that is being replaced
 * @param engine_type The EngineID of the vehicle that is being replaced to
 * @return The cargo type to replace to
 *    CT_NO_REFIT is returned if no refit is needed
 *    CT_INVALID is returned when both old and new vehicle got cargo capacity and refitting the new one to the old one's cargo type isn't possible
 */
static CargoID GetNewCargoTypeForReplace(Vehicle *v, EngineID engine_type)
{
	bool new_cargo_capacity = true;
	CargoID new_cargo_type = CT_INVALID;

	switch (v->type) {
		case VEH_Train:
			new_cargo_capacity = (RailVehInfo(engine_type)->capacity > 0);
			new_cargo_type     = RailVehInfo(engine_type)->cargo_type;
			break;

		case VEH_Road:
			new_cargo_capacity = (RoadVehInfo(engine_type)->capacity > 0);
			new_cargo_type     = RoadVehInfo(engine_type)->cargo_type;
			break;
		case VEH_Ship:
			new_cargo_capacity = (ShipVehInfo(engine_type)->capacity > 0);
			new_cargo_type     = ShipVehInfo(engine_type)->cargo_type;
			break;

		case VEH_Aircraft:
			/* all aircraft starts as passenger planes with cargo capacity
			 * new_cargo_capacity is always true for aircraft, which is the init value. No need to set it here */
			new_cargo_type     = CT_PASSENGERS;
			break;

		default: NOT_REACHED(); break;
	}

	if (!new_cargo_capacity) return CT_NO_REFIT; // Don't try to refit an engine with no cargo capacity

	if (v->cargo_type == new_cargo_type || CanRefitTo(engine_type, v->cargo_type)) {
		if (VerifyAutoreplaceRefitForOrders(v, engine_type)) {
			return v->cargo_type == new_cargo_type ? CT_NO_REFIT : v->cargo_type;
		} else {
			return CT_INVALID;
		}
	}
	if (v->type != VEH_Train) return CT_INVALID; // We can't refit the vehicle to carry the cargo we want

	/* Below this line it's safe to assume that the vehicle in question is a train */

	if (v->cargo_cap != 0) return CT_INVALID; // trying to replace a vehicle with cargo capacity into another one with incompatible cargo type

	/* the old engine didn't have cargo capacity, but the new one does
	 * now we will figure out what cargo the train is carrying and refit to fit this */
	v = GetFirstVehicleInChain(v);
	do {
		if (v->cargo_cap == 0) continue;
		/* Now we found a cargo type being carried on the train and we will see if it is possible to carry to this one */
		if (v->cargo_type == new_cargo_type) return CT_NO_REFIT;
		if (CanRefitTo(engine_type, v->cargo_type)) return v->cargo_type;
	} while ((v=v->next) != NULL);
	return CT_NO_REFIT; // We failed to find a cargo type on the old vehicle and we will not refit the new one
}

/* Replaces a vehicle (used to be called autorenew)
 * This function is only called from MaybeReplaceVehicle()
 * Must be called with _current_player set to the owner of the vehicle
 * @param w Vehicle to replace
 * @param flags is the flags to use when calling DoCommand(). Mainly DC_EXEC counts
 * @return value is cost of the replacement or CMD_ERROR
 */
static int32 ReplaceVehicle(Vehicle **w, byte flags, int32 total_cost)
{
	int32 cost;
	int32 sell_value;
	Vehicle *old_v = *w;
	const Player *p = GetPlayer(old_v->owner);
	EngineID new_engine_type;
	const UnitID cached_unitnumber = old_v->unitnumber;
	bool new_front = false;
	Vehicle *new_v = NULL;
	char vehicle_name[32];
	CargoID replacement_cargo_type;

	new_engine_type = EngineReplacementForPlayer(p, old_v->engine_type);
	if (new_engine_type == INVALID_ENGINE) new_engine_type = old_v->engine_type;

	replacement_cargo_type = GetNewCargoTypeForReplace(old_v, new_engine_type);

	/* check if we can't refit to the needed type, so no replace takes place to prevent the vehicle from altering cargo type */
	if (replacement_cargo_type == CT_INVALID) return 0;

	sell_value = DoCommand(0, old_v->index, 0, DC_QUERY_COST, CMD_SELL_VEH(old_v->type));

	/* We give the player a loan of the same amount as the sell value.
	 * This is needed in case he needs the income from the sale to build the new vehicle.
	 * We take it back if building fails or when we really sell the old engine */
	SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);
	SubtractMoneyFromPlayer(sell_value);

	cost = DoCommand(old_v->tile, new_engine_type, 3, flags, CMD_BUILD_VEH(old_v->type));
	if (CmdFailed(cost)) {
		SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);
		SubtractMoneyFromPlayer(-sell_value); // Take back the money we just gave the player
		return cost;
	}

	if (replacement_cargo_type != CT_NO_REFIT) cost += GetRefitCost(new_engine_type); // add refit cost

	if (flags & DC_EXEC) {
		new_v = GetVehicle(_new_vehicle_id);
		*w = new_v; //we changed the vehicle, so MaybeReplaceVehicle needs to work on the new one. Now we tell it what the new one is

		/* refit if needed */
		if (replacement_cargo_type != CT_NO_REFIT) {
			if (CmdFailed(DoCommand(0, new_v->index, replacement_cargo_type, DC_EXEC, CMD_REFIT_VEH(new_v->type)))) {
				/* Being here shows a failure, which most likely is in GetNewCargoTypeForReplace() or incorrect estimation costs */
				error("Autoreplace failed to refit. Replace engine %d to %d and refit to cargo %d", old_v->engine_type, new_v->engine_type, replacement_cargo_type);
			}
		}

		if (new_v->type == VEH_Train && HASBIT(old_v->u.rail.flags, VRF_REVERSE_DIRECTION) && !IsMultiheaded(new_v) && !(new_v->next != NULL && IsArticulatedPart(new_v->next))) {
			// we are autorenewing to a single engine, so we will turn it as the old one was turned as well
			SETBIT(new_v->u.rail.flags, VRF_REVERSE_DIRECTION);
		}

		if (old_v->type == VEH_Train && !IsFrontEngine(old_v)) {
			/* this is a railcar. We need to move the car into the train
			 * We add the new engine after the old one instead of replacing it. It will give the same result anyway when we
			 * sell the old engine in a moment
			 */
			DoCommand(0, (GetPrevVehicleInChain(old_v)->index << 16) | new_v->index, 1, DC_EXEC, CMD_MOVE_RAIL_VEHICLE);
			/* Now we move the old one out of the train */
			DoCommand(0, (INVALID_VEHICLE << 16) | old_v->index, 0, DC_EXEC, CMD_MOVE_RAIL_VEHICLE);
		} else {
			// copy/clone the orders
			DoCommand(0, (old_v->index << 16) | new_v->index, IsOrderListShared(old_v) ? CO_SHARE : CO_COPY, DC_EXEC, CMD_CLONE_ORDER);
			new_v->cur_order_index = old_v->cur_order_index;
			ChangeVehicleViewWindow(old_v, new_v);
			new_v->profit_this_year = old_v->profit_this_year;
			new_v->profit_last_year = old_v->profit_last_year;
			new_v->service_interval = old_v->service_interval;
			new_front = true;
			new_v->unitnumber = old_v->unitnumber; // use the same unit number

			new_v->current_order = old_v->current_order;
			if (old_v->type == VEH_Train && GetNextVehicle(old_v) != NULL){
				Vehicle *temp_v = GetNextVehicle(old_v);

				// move the entire train to the new engine, excluding the old engine
				if (IsMultiheaded(old_v) && temp_v == old_v->u.rail.other_multiheaded_part) {
					// we got front and rear of a multiheaded engine right after each other. We should work with the next in line instead
					temp_v = GetNextVehicle(temp_v);
				}

				if (temp_v != NULL) {
					DoCommand(0, (new_v->index << 16) | temp_v->index, 1, DC_EXEC, CMD_MOVE_RAIL_VEHICLE);
				}
			}
		}
		/* We are done setting up the new vehicle. Now we move the cargo from the old one to the new one */
		MoveVehicleCargo(new_v->type == VEH_Train ? GetFirstVehicleInChain(new_v) : new_v, old_v);

		// Get the name of the old vehicle if it has a custom name.
		if (!IsCustomName(old_v->string_id)) {
			vehicle_name[0] = '\0';
		} else {
			GetName(vehicle_name, old_v->string_id & 0x7FF, lastof(vehicle_name));
		}
	} else { // flags & DC_EXEC not set
		/* Ensure that the player will not end up having negative money while autoreplacing
		 * This is needed because the only other check is done after the income from selling the old vehicle is substracted from the cost */
		if (p->money64 < (cost + total_cost)) {
			SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);
			SubtractMoneyFromPlayer(-sell_value); // Pay back the loan
			return CMD_ERROR;
		}
	}

	/* Take back the money we just gave the player just before building the vehicle
	 * The player will get the same amount now that the sale actually takes place */
	SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);
	SubtractMoneyFromPlayer(-sell_value);

	/* sell the engine/ find out how much you get for the old engine (income is returned as negative cost) */
	cost += DoCommand(0, old_v->index, 0, flags, CMD_SELL_VEH(old_v->type));

	if (new_front) {
		/* now we assign the old unitnumber to the new vehicle */
		new_v->unitnumber = cached_unitnumber;
	}

	/* Transfer the name of the old vehicle */
	if ((flags & DC_EXEC) && vehicle_name[0] != '\0') {
		_cmd_text = vehicle_name;
		DoCommand(0, new_v->index, 0, DC_EXEC, CMD_NAME_VEHICLE);
	}

	return cost;
}

/** replaces a vehicle if it's set for autoreplace or is too old
 * (used to be called autorenew)
 * @param v The vehicle to replace
 * if the vehicle is a train, v needs to be the front engine
 * @param check Checks if the replace is valid. No action is done at all
 * @param display_costs If set, a cost animation is shown (only if check is false)
 * @return CMD_ERROR if something went wrong. Otherwise the price of the replace
 */
static int32 MaybeReplaceVehicle(Vehicle *v, bool check, bool display_costs)
{
	Vehicle *w;
	const Player *p = GetPlayer(v->owner);
	byte flags = 0;
	int32 cost, temp_cost = 0;
	bool stopped = false;

	/* Remember the length in case we need to trim train later on
	 * If it's not a train, the value is unused
	 * round up to the length of the tiles used for the train instead of the train length instead
	 * Useful when newGRF uses custom length */
	uint16 old_total_length = (v->type == VEH_Train ?
		(v->u.rail.cached_total_length + TILE_SIZE - 1) / TILE_SIZE * TILE_SIZE :
		-1
	);


	_current_player = v->owner;

	assert(v->type == VEH_Train || v->type == VEH_Road || v->type == VEH_Ship || v->type == VEH_Aircraft);

	assert(v->vehstatus & VS_STOPPED); // the vehicle should have been stopped in VehicleEnteredDepotThisTick() if needed

	if (v->leave_depot_instantly) {
		// we stopped the vehicle to do this, so we have to remember to start it again when we are done
		// we need to store this info as the engine might be replaced and lose this info
		stopped = true;
	}

	for (;;) {
		cost = 0;
		w = v;
		do {
			if (w->type == VEH_Train && IsMultiheaded(w) && !IsTrainEngine(w)) {
				/* we build the rear ends of multiheaded trains with the front ones */
				continue;
			}

			// check if the vehicle should be replaced
			if (!p->engine_renew ||
					w->age - w->max_age < (p->engine_renew_months * 30) || // replace if engine is too old
					w->max_age == 0) { // rail cars got a max age of 0
				if (!EngineHasReplacementForPlayer(p, w->engine_type)) // updates to a new model
					continue;
			}

			/* Now replace the vehicle */
			temp_cost = ReplaceVehicle(&w, flags, cost);

			if (flags & DC_EXEC &&
					(w->type != VEH_Train || w->u.rail.first_engine == INVALID_ENGINE)) {
				/* now we bought a new engine and sold the old one. We need to fix the
				 * pointers in order to avoid pointing to the old one for trains: these
				 * pointers should point to the front engine and not the cars
				 */
				v = w;
			}

			if (!CmdFailed(temp_cost)) {
				cost += temp_cost;
			}
		} while (w->type == VEH_Train && (w = GetNextVehicle(w)) != NULL);

		if (!(flags & DC_EXEC) && (p->money64 < (int32)(cost + p->engine_renew_money) || cost == 0)) {
			if (!check && p->money64 < (int32)(cost + p->engine_renew_money) && ( _local_player == v->owner ) && cost != 0) {
				StringID message;
				SetDParam(0, v->unitnumber);
				switch (v->type) {
					case VEH_Train:    message = STR_TRAIN_AUTORENEW_FAILED;       break;
					case VEH_Road:     message = STR_ROADVEHICLE_AUTORENEW_FAILED; break;
					case VEH_Ship:     message = STR_SHIP_AUTORENEW_FAILED;        break;
					case VEH_Aircraft: message = STR_AIRCRAFT_AUTORENEW_FAILED;    break;
						// This should never happen
					default: NOT_REACHED(); message = 0; break;
				}

				AddNewsItem(message, NEWS_FLAGS(NM_SMALL, NF_VIEWPORT|NF_VEHICLE, NT_ADVICE, 0), v->index, 0);
			}
			if (stopped) v->vehstatus &= ~VS_STOPPED;
			if (display_costs) _current_player = OWNER_NONE;
			return CMD_ERROR;
		}

		if (flags & DC_EXEC) {
			break; // we are done replacing since the loop ran once with DC_EXEC
		} else if (check) {
			/* It's a test only and we know that we can do this
			 * NOTE: payment for wagon removal is NOT included in this price */
			return cost;
		}
		// now we redo the loop, but this time we actually do stuff since we know that we can do it
		flags |= DC_EXEC;
	}

	/* If setting is on to try not to exceed the old length of the train with the replacement */
	if (v->type == VEH_Train && p->renew_keep_length) {
		Vehicle *temp;
		w = v;

		while (v->u.rail.cached_total_length > old_total_length) {
			// the train is too long. We will remove cars one by one from the start of the train until it's short enough
			while (w != NULL && !(RailVehInfo(w->engine_type)->flags&RVI_WAGON) ) {
				w = GetNextVehicle(w);
			}
			if (w == NULL) {
				// we failed to make the train short enough
				SetDParam(0, v->unitnumber);
				AddNewsItem(STR_TRAIN_TOO_LONG_AFTER_REPLACEMENT, NEWS_FLAGS(NM_SMALL, NF_VIEWPORT|NF_VEHICLE, NT_ADVICE, 0), v->index, 0);
				break;
			}
			temp = w;
			w = GetNextVehicle(w);
			DoCommand(0, (INVALID_VEHICLE << 16) | temp->index, 0, DC_EXEC, CMD_MOVE_RAIL_VEHICLE);
			MoveVehicleCargo(v, temp);
			cost += DoCommand(0, temp->index, 0, DC_EXEC, CMD_SELL_RAIL_WAGON);
		}
	}

	if (stopped) v->vehstatus &= ~VS_STOPPED;
	if (display_costs) {
		if (IsLocalPlayer()) ShowCostOrIncomeAnimation(v->x_pos, v->y_pos, v->z_pos, cost);
		_current_player = OWNER_NONE;
	}
	return cost;
}

/* Extend the list size for BuildDepotVehicleList() */
static inline void ExtendVehicleListSize(const Vehicle ***engine_list, uint16 *engine_list_length, uint16 step_size)
{
	*engine_list_length = min(*engine_list_length + step_size, GetVehicleArraySize());
	*engine_list = realloc((void*)*engine_list, (*engine_list_length) * sizeof((*engine_list)[0]));
}

/** Generates a list of vehicles inside a depot
 * Will enlarge allocated space for the list if they are too small, so it's ok to call with (pointer to NULL array, pointer to uninitised uint16, pointer to 0)
 * If one of the lists is not needed (say wagons when finding ships), all the pointers regarding that list should be set to NULL
 * @param Type type of vehicle
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
	assert(!(engine_list == NULL && type != VEH_Train));
	assert(!(type == VEH_Train && engine_list == NULL && wagon_list == NULL));

	/* Both array and the length should either be NULL to disable the list or both should not be NULL */
	assert((engine_list == NULL && engine_list_length == NULL) || (engine_list != NULL && engine_list_length != NULL));
	assert((wagon_list == NULL && wagon_list_length == NULL) || (wagon_list != NULL && wagon_list_length != NULL));

	assert(!(engine_list != NULL && engine_count == NULL));
	assert(!(wagon_list != NULL && wagon_count == NULL));

	if (engine_count != NULL) *engine_count = 0;
	if (wagon_count != NULL) *wagon_count = 0;

	switch (type) {
		case VEH_Train:
			FOR_ALL_VEHICLES(v) {
				if (v->tile == tile && v->type == VEH_Train && v->u.rail.track == 0x80) {
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

		case VEH_Road:
			FOR_ALL_VEHICLES(v) {
				if (v->tile == tile && v->type == VEH_Road && IsRoadVehInDepot(v)) {
					if (*engine_count == *engine_list_length) ExtendVehicleListSize((const Vehicle***)engine_list, engine_list_length, 25);
					(*engine_list)[(*engine_count)++] = v;
				}
			}
			break;

		case VEH_Ship:
			FOR_ALL_VEHICLES(v) {
				if (v->tile == tile && v->type == VEH_Ship && IsShipInDepot(v)) {
					if (*engine_count == *engine_list_length) ExtendVehicleListSize((const Vehicle***)engine_list, engine_list_length, 25);
					(*engine_list)[(*engine_count)++] = v;
				}
			}
			break;

		case VEH_Aircraft:
			FOR_ALL_VEHICLES(v) {
				if (v->tile == tile &&
						v->type == VEH_Aircraft &&
						v->subtype <= 2 &&
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
* @param station index of station to generate a list for. INVALID_STATION when not used
* @param order index of oder to generate a list for. INVALID_ORDER when not used
* @param window_type tells what kind of window the list is for. Use the VLW flags in vehicle_gui.h
* @return the number of vehicles added to the list
*/
uint GenerateVehicleSortList(const Vehicle ***sort_list, uint16 *length_of_array, byte type, PlayerID owner, StationID station, OrderID order, uint16 depot_airport_index, uint16 window_type)
{
	const uint subtype = (type != VEH_Aircraft) ? Train_Front : 2;
	uint n = 0;
	const Vehicle *v;

	switch (window_type) {
		case VLW_STATION_LIST: {
			FOR_ALL_VEHICLES(v) {
				if (v->type == type && (
					(type == VEH_Train && IsFrontEngine(v)) ||
					(type != VEH_Train && v->subtype <= subtype))) {
					const Order *order;

					FOR_VEHICLE_ORDERS(v, order) {
						if (order->type == OT_GOTO_STATION && order->dest == station) {
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
				if (v->orders != NULL && v->orders->index == order) break;
			}

			if (v != NULL && v->orders != NULL && v->orders->index == order) {
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
					(type == VEH_Train && IsFrontEngine(v)) ||
					(type != VEH_Train && v->subtype <= subtype))) {
					/* TODO find a better estimate on the total number of vehicles for current player */
					if (n == *length_of_array) ExtendVehicleListSize(sort_list, length_of_array, GetVehicleArraySize()/4);
					(*sort_list)[n++] = v;
				}
			}
			break;
		}

		case VLW_DEPOT_LIST: {
			FOR_ALL_VEHICLES(v) {
				if (v->type == type && (
					(type == VEH_Train && IsFrontEngine(v)) ||
					(type != VEH_Train && v->subtype <= subtype))) {
					const Order *order;

					FOR_VEHICLE_ORDERS(v, order) {
						if (order->type == OT_GOTO_DEPOT && order->dest == depot_airport_index) {
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
		*sort_list = realloc((void*)*sort_list, (*length_of_array) * sizeof((*sort_list)[0]));
	}

	return n;
}

/** send all vehicles of type to depots
 * @param type type of vehicle
 * @param flags the flags used for DoCommand()
 * @param service should the vehicles only get service in the depots
 * @param owner PlayerID of owner of the vehicles to send
 * @param VLW_flag tells what kind of list requested the goto depot
 * @return 0 for success and CMD_ERROR if no vehicle is able to go to depot
 */
int32 SendAllVehiclesToDepot(byte type, uint32 flags, bool service, PlayerID owner, uint16 vlw_flag, uint32 id)
{
	const Vehicle **sort_list = NULL;
	uint n, i;
	uint16 array_length = 0;

	n = GenerateVehicleSortList(&sort_list, &array_length, type, owner, id, id, id, vlw_flag);

	/* Send all the vehicles to a depot */
	for (i = 0; i < n; i++) {
		const Vehicle *v = sort_list[i];
		int32 ret = DoCommand(v->tile, v->index, service | DEPOT_DONT_CANCEL, flags, CMD_SEND_TO_DEPOT(type));

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
		case VEH_Train:    return CheckTrainInDepot(v, false) != -1;
		case VEH_Road:     return IsRoadVehInDepot(v);
		case VEH_Ship:     return IsShipInDepot(v);
		case VEH_Aircraft: return IsAircraftInHangar(v);
		default: NOT_REACHED();
	}
	return false;
}

void VehicleEnterDepot(Vehicle *v)
{
	switch (v->type) {
		case VEH_Train:
			InvalidateWindowClasses(WC_TRAINS_LIST);
			if (!IsFrontEngine(v)) v = GetFirstVehicleInChain(v);
			UpdateSignalsOnSegment(v->tile, GetRailDepotDirection(v->tile));
			v->load_unload_time_rem = 0;
			break;

		case VEH_Road:
			InvalidateWindowClasses(WC_ROADVEH_LIST);
			v->u.road.state = 254;
			break;

		case VEH_Ship:
			InvalidateWindowClasses(WC_SHIPS_LIST);
			v->u.ship.state = 0x80;
			RecalcShipStuff(v);
			break;

		case VEH_Aircraft:
			InvalidateWindowClasses(WC_AIRCRAFT_LIST);
			HandleAircraftEnterHangar(v);
			break;
		default: NOT_REACHED();
	}

	if (v->type != VEH_Train) {
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
			cost = DoCommand(v->tile, v->index, t.refit_cargo | t.refit_subtype << 8, DC_EXEC, CMD_REFIT_VEH(v->type));

			if (CmdFailed(cost)) {
				v->leave_depot_instantly = false; // We ensure that the vehicle stays in the depot
				if (v->owner == _local_player) {
					/* Notify the user that we stopped the vehicle */
					SetDParam(0, _vehicle_type_names[v->type - 0x10]);
					SetDParam(1, v->unitnumber);
					AddNewsItem(STR_ORDER_REFIT_FAILED, NEWS_FLAGS(NM_SMALL, NF_VIEWPORT|NF_VEHICLE, NT_ADVICE, 0), v->index, 0);
				}
			} else if (v->owner == _local_player && cost != 0) {
				ShowCostOrIncomeAnimation(v->x_pos, v->y_pos, v->z_pos, cost);
			}
		}

		if (HASBIT(t.flags, OFB_PART_OF_ORDERS)) {
			/* Part of orders */
			if (v->type == VEH_Train) v->u.rail.days_since_order_progr = 0;
			v->cur_order_index++;
		} else if (HASBIT(t.flags, OFB_HALT_IN_DEPOT)) {
			/* Force depot visit */
			v->vehstatus |= VS_STOPPED;
			if (v->owner == _local_player) {
				StringID string;

				switch (v->type) {
					case VEH_Train:    string = STR_8814_TRAIN_IS_WAITING_IN_DEPOT; break;
					case VEH_Road:     string = STR_9016_ROAD_VEHICLE_IS_WAITING;   break;
					case VEH_Ship:     string = STR_981C_SHIP_IS_WAITING_IN_DEPOT;  break;
					case VEH_Aircraft: string = STR_A014_AIRCRAFT_IS_WAITING_IN;    break;
					default: NOT_REACHED(); string = STR_EMPTY; // Set the string to something to avoid a compiler warning
				}

				SetDParam(0, v->unitnumber);
				AddNewsItem(string, NEWS_FLAGS(NM_SMALL, NF_VIEWPORT|NF_VEHICLE, NT_ADVICE, 0),	v->index, 0);
			}
		}
	}
}

/** Give a custom name to your vehicle
 * @param tile unused
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
bool GetNewVehiclePos(const Vehicle *v, GetNewVehiclePosResult *gp)
{
	static const int8 _delta_coord[16] = {
		-1,-1,-1, 0, 1, 1, 1, 0, /* x */
		-1, 0, 1, 1, 1, 0,-1,-1, /* y */
	};

	int x = v->x_pos + _delta_coord[v->direction];
	int y = v->y_pos + _delta_coord[v->direction + 8];

	gp->x = x;
	gp->y = y;
	gp->old_tile = v->tile;
	gp->new_tile = TileVirtXY(x, y);
	return gp->old_tile == gp->new_tile;
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
	if (v->vehstatus & VS_CRASHED) return 0xFF;

	switch (v->type) {
		case VEH_Train:
			if (v->u.rail.track == 0x80) /* We'll assume the train is facing outwards */
				return DiagdirToDiagTrackdir(GetRailDepotDirection(v->tile)); /* Train in depot */

			if (v->u.rail.track == 0x40) /* train in tunnel, so just use his direction and assume a diagonal track */
				return DiagdirToDiagTrackdir(DirToDiagDir(v->direction));

			return TrackDirectionToTrackdir(FIND_FIRST_BIT(v->u.rail.track),v->direction);

		case VEH_Ship:
			if (IsShipInDepot(v))
				/* We'll assume the ship is facing outwards */
				return DiagdirToDiagTrackdir(GetShipDepotDirection(v->tile));

			return TrackDirectionToTrackdir(FIND_FIRST_BIT(v->u.ship.state),v->direction);

		case VEH_Road:
			if (IsRoadVehInDepot(v)) /* We'll assume the road vehicle is facing outwards */
				return DiagdirToDiagTrackdir(GetRoadDepotDirection(v->tile));

			if (IsRoadStopTile(v->tile)) /* We'll assume the road vehicle is facing outwards */
				return DiagdirToDiagTrackdir(GetRoadStopDir(v->tile)); /* Road vehicle in a station */

			/* If vehicle's state is a valid track direction (vehicle is not turning around) return it */
			if ((v->u.road.state & 7) < 6) return v->u.road.state;

			/* Vehicle is turning around, get the direction from vehicle's direction */
			return DiagdirToDiagTrackdir(DirToDiagDir(v->direction));

		/* case VEH_Aircraft: case VEH_Special: case VEH_Disaster: */
		default: return 0xFF;
	}
}
/* Return value has bit 0x2 set, when the vehicle enters a station. Then,
 * result << 8 contains the id of the station entered. If the return value has
 * bit 0x8 set, the vehicle could not and did not enter the tile. Are there
 * other bits that can be set? */
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
		case VEH_Train:    max = _patches.max_trains; break;
		case VEH_Road:     max = _patches.max_roadveh; break;
		case VEH_Ship:     max = _patches.max_ships; break;
		case VEH_Aircraft: max = _patches.max_aircraft; break;
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
		cache = malloc((max + 1) * sizeof(*cache));
	}

	// Clear the cache
	memset(cache, 0, (max + 1) * sizeof(*cache));

	// Fill the cache
	FOR_ALL_VEHICLES(u) {
		if (u->type == type && u->owner == _current_player && u->unitnumber != 0 && u->unitnumber <= max)
			cache[u->unitnumber] = true;
	}

	// Find the first unused unit number
	for (unit = 1; unit <= max; unit++) {
		if (!cache[unit]) break;
	}

	return unit;
}

static PalSpriteID GetEngineColourMap(EngineID engine_type, PlayerID player, EngineID parent_engine_type, CargoID cargo_type)
{
	SpriteID map;
	const Player *p = GetPlayer(player);
	LiveryScheme scheme = LS_DEFAULT;

	/* The default livery is always available for use, but its in_use flag determines
	 * whether any _other_ liveries are in use. */
	if (p->livery[LS_DEFAULT].in_use && (_patches.liveries == 2 || (_patches.liveries == 1 && player == _local_player))) {
		/* Determine the livery scheme to use */
		switch (GetEngine(engine_type)->type) {
			case VEH_Train: {
				switch (_engine_info[engine_type].railtype) {
					case RAILTYPE_RAIL:
					case RAILTYPE_ELECTRIC:
					{
						const RailVehicleInfo *rvi = RailVehInfo(engine_type);

						if (cargo_type == CT_INVALID) cargo_type = rvi->cargo_type;
						if (rvi->flags & RVI_WAGON) {
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

			case VEH_Road: {
				const RoadVehicleInfo *rvi = RoadVehInfo(engine_type);
				if (cargo_type == CT_INVALID) cargo_type = rvi->cargo_type;
				scheme = (cargo_type == CT_PASSENGERS) ? LS_BUS : LS_TRUCK;
				break;
			}

			case VEH_Ship: {
				const ShipVehicleInfo *svi = ShipVehInfo(engine_type);
				if (cargo_type == CT_INVALID) cargo_type = svi->cargo_type;
				scheme = (cargo_type == CT_PASSENGERS) ? LS_PASSENGER_SHIP : LS_FREIGHT_SHIP;
				break;
			}

			case VEH_Aircraft: {
				const AircraftVehicleInfo *avi = AircraftVehInfo(engine_type);
				if (cargo_type == CT_INVALID) cargo_type = CT_PASSENGERS;
				switch (avi->subtype) {
					case 0: scheme = LS_HELICOPTER; break;
					case 1: scheme = LS_SMALL_PLANE; break;
					case 3: scheme = LS_LARGE_PLANE; break;
				}
				break;
			}
		}

		/* Switch back to the default scheme if the resolved scheme is not in use */
		if (!p->livery[scheme].in_use) scheme = LS_DEFAULT;
	}

	map = HASBIT(EngInfo(engine_type)->misc_flags, EF_USES_2CC) ?
		(SPR_2CCMAP_BASE + p->livery[scheme].colour1 + p->livery[scheme].colour2 * 16) :
		(PALETTE_RECOLOR_START + p->livery[scheme].colour1);

	return SPRITE_PALETTE(map << PALETTE_SPRITE_START);
}

PalSpriteID GetEnginePalette(EngineID engine_type, PlayerID player)
{
	return GetEngineColourMap(engine_type, player, INVALID_ENGINE, CT_INVALID);
}

PalSpriteID GetVehiclePalette(const Vehicle *v)
{
	if (v->type == VEH_Train) {
		return GetEngineColourMap(
			(v->u.rail.first_engine != INVALID_ENGINE && (IsArticulatedPart(v) || UsesWagonOverride(v))) ?
				v->u.rail.first_engine : v->engine_type,
			v->owner,
			v->u.rail.first_engine,
			v->cargo_type);
	}

	return GetEngineColourMap(v->engine_type, v->owner, INVALID_ENGINE, v->cargo_type);
}

// Save and load of vehicles
const SaveLoad _common_veh_desc[] = {
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

	    SLE_VAR(Vehicle, cur_image,            SLE_UINT16),
	    SLE_VAR(Vehicle, spritenum,            SLE_UINT8),
	    SLE_VAR(Vehicle, sprite_width,         SLE_UINT8),
	    SLE_VAR(Vehicle, sprite_height,        SLE_UINT8),
	    SLE_VAR(Vehicle, z_height,             SLE_UINT8),
	    SLE_VAR(Vehicle, x_offs,               SLE_INT8),
	    SLE_VAR(Vehicle, y_offs,               SLE_INT8),
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
	    SLE_VAR(Vehicle, cargo_cap,            SLE_UINT16),
	    SLE_VAR(Vehicle, cargo_count,          SLE_UINT16),

	    SLE_VAR(Vehicle, day_counter,          SLE_UINT8),
	    SLE_VAR(Vehicle, tick_counter,         SLE_UINT8),

	    SLE_VAR(Vehicle, cur_order_index,      SLE_UINT8),
	    SLE_VAR(Vehicle, num_orders,           SLE_UINT8),

	/* This next line is for version 4 and prior compatibility.. it temporarily reads
	    type and flags (which were both 4 bits) into type. Later on this is
	    converted correctly */
	SLE_CONDVARX(offsetof(Vehicle, current_order) + offsetof(Order, type), SLE_UINT8,                 0, 4),
	SLE_CONDVARX(offsetof(Vehicle, current_order) + offsetof(Order, dest), SLE_FILE_U8 | SLE_VAR_U16, 0, 4),

	/* Orders for version 5 and on */
	SLE_CONDVARX(offsetof(Vehicle, current_order) + offsetof(Order, type),  SLE_UINT8,  5, SL_MAX_VERSION),
	SLE_CONDVARX(offsetof(Vehicle, current_order) + offsetof(Order, flags), SLE_UINT8,  5, SL_MAX_VERSION),
	SLE_CONDVARX(offsetof(Vehicle, current_order) + offsetof(Order, dest),  SLE_UINT16, 5, SL_MAX_VERSION),

	/* Refit in current order */
	SLE_CONDVARX(offsetof(Vehicle, current_order) + offsetof(Order, refit_cargo),    SLE_UINT8, 36, SL_MAX_VERSION),
	SLE_CONDVARX(offsetof(Vehicle, current_order) + offsetof(Order, refit_subtype),  SLE_UINT8, 36, SL_MAX_VERSION),

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
	SLE_CONDVAR(Vehicle, load_status,          SLE_UINT8,                 40, SL_MAX_VERSION),

	    SLE_VAR(Vehicle, profit_this_year,     SLE_INT32),
	    SLE_VAR(Vehicle, profit_last_year,     SLE_INT32),
	    SLE_VAR(Vehicle, value,                SLE_UINT32),

	    SLE_VAR(Vehicle, random_bits,          SLE_UINT8),
	    SLE_VAR(Vehicle, waiting_triggers,     SLE_UINT8),

	    SLE_REF(Vehicle, next_shared,          REF_VEHICLE),
	    SLE_REF(Vehicle, prev_shared,          REF_VEHICLE),

	// reserve extra space in savegame here. (currently 10 bytes)
	SLE_CONDNULL(10,                                                       2, SL_MAX_VERSION),

	SLE_END()
};


static const SaveLoad _train_desc[] = {
	SLE_WRITEBYTE(Vehicle, type, VEH_Train, 0), // Train type. VEH_Train in mem, 0 in file.
	SLE_INCLUDEX(0, INC_VEHICLE_COMMON),
	    SLE_VARX(offsetof(Vehicle, u) + offsetof(VehicleRail, crash_anim_pos),         SLE_UINT16),
	    SLE_VARX(offsetof(Vehicle, u) + offsetof(VehicleRail, force_proceed),          SLE_UINT8),
	    SLE_VARX(offsetof(Vehicle, u) + offsetof(VehicleRail, railtype),               SLE_UINT8),
	    SLE_VARX(offsetof(Vehicle, u) + offsetof(VehicleRail, track),                  SLE_UINT8),

	SLE_CONDVARX(offsetof(Vehicle, u) + offsetof(VehicleRail, flags),                  SLE_UINT8,  2, SL_MAX_VERSION),
	SLE_CONDVARX(offsetof(Vehicle, u) + offsetof(VehicleRail, days_since_order_progr), SLE_UINT16, 2, SL_MAX_VERSION),

	SLE_CONDNULL(2, 2, 19),
	// reserve extra space in savegame here. (currently 11 bytes)
	SLE_CONDNULL(11, 2, SL_MAX_VERSION),

	SLE_END()
};

static const SaveLoad _roadveh_desc[] = {
	SLE_WRITEBYTE(Vehicle, type, VEH_Road, 1), // Road type. VEH_Road in mem, 1 in file.
	SLE_INCLUDEX(0, INC_VEHICLE_COMMON),
	    SLE_VARX(offsetof(Vehicle, u) + offsetof(VehicleRoad, state),          SLE_UINT8),
	    SLE_VARX(offsetof(Vehicle, u) + offsetof(VehicleRoad, frame),          SLE_UINT8),
	    SLE_VARX(offsetof(Vehicle, u) + offsetof(VehicleRoad, blocked_ctr),    SLE_UINT16),
	    SLE_VARX(offsetof(Vehicle, u) + offsetof(VehicleRoad, overtaking),     SLE_UINT8),
	    SLE_VARX(offsetof(Vehicle, u) + offsetof(VehicleRoad, overtaking_ctr), SLE_UINT8),
	    SLE_VARX(offsetof(Vehicle, u) + offsetof(VehicleRoad, crashed_ctr),    SLE_UINT16),
	    SLE_VARX(offsetof(Vehicle, u) + offsetof(VehicleRoad, reverse_ctr),    SLE_UINT8),

	SLE_CONDREFX(offsetof(Vehicle, u) + offsetof(VehicleRoad, slot),     REF_ROADSTOPS, 6, SL_MAX_VERSION),
	SLE_CONDNULL(1,                                                                     6, SL_MAX_VERSION),
	SLE_CONDVARX(offsetof(Vehicle, u) + offsetof(VehicleRoad, slot_age), SLE_UINT8,     6, SL_MAX_VERSION),
	// reserve extra space in savegame here. (currently 16 bytes)
	SLE_CONDNULL(16,                                                                    2, SL_MAX_VERSION),

	SLE_END()
};

static const SaveLoad _ship_desc[] = {
	SLE_WRITEBYTE(Vehicle, type, VEH_Ship, 2), // Ship type. VEH_Ship in mem, 2 in file.
	SLE_INCLUDEX(0, INC_VEHICLE_COMMON),
	SLE_VARX(offsetof(Vehicle, u) + offsetof(VehicleShip, state), SLE_UINT8),

	// reserve extra space in savegame here. (currently 16 bytes)
	SLE_CONDNULL(16, 2, SL_MAX_VERSION),

	SLE_END()
};

static const SaveLoad _aircraft_desc[] = {
	SLE_WRITEBYTE(Vehicle, type, VEH_Aircraft, 3), // Aircraft type. VEH_Aircraft in mem, 3 in file.
	SLE_INCLUDEX(0, INC_VEHICLE_COMMON),
	    SLE_VARX(offsetof(Vehicle, u) + offsetof(VehicleAir, crashed_counter), SLE_UINT16),
	    SLE_VARX(offsetof(Vehicle, u) + offsetof(VehicleAir, pos),             SLE_UINT8),

	SLE_CONDVARX(offsetof(Vehicle, u) + offsetof(VehicleAir, targetairport),   SLE_FILE_U8 | SLE_VAR_U16, 0, 4),
	SLE_CONDVARX(offsetof(Vehicle, u) + offsetof(VehicleAir, targetairport),   SLE_UINT16,                5, SL_MAX_VERSION),

	    SLE_VARX(offsetof(Vehicle, u) + offsetof(VehicleAir, state),           SLE_UINT8),

	SLE_CONDVARX(offsetof(Vehicle, u) + offsetof(VehicleAir, previous_pos),    SLE_UINT8,                 2, SL_MAX_VERSION),

	// reserve extra space in savegame here. (currently 15 bytes)
	SLE_CONDNULL(15,                                                                                      2, SL_MAX_VERSION),

	SLE_END()
};

static const SaveLoad _special_desc[] = {
	SLE_WRITEBYTE(Vehicle,type,VEH_Special, 4),

	    SLE_VAR(Vehicle, subtype,       SLE_UINT8),

	SLE_CONDVAR(Vehicle, tile,          SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(Vehicle, tile,          SLE_UINT32,                 6, SL_MAX_VERSION),

	SLE_CONDVAR(Vehicle, x_pos,         SLE_FILE_I16 | SLE_VAR_I32, 0, 5),
	SLE_CONDVAR(Vehicle, x_pos,         SLE_INT32,                  6, SL_MAX_VERSION),
	SLE_CONDVAR(Vehicle, y_pos,         SLE_FILE_I16 | SLE_VAR_I32, 0, 5),
	SLE_CONDVAR(Vehicle, y_pos,         SLE_INT32,                  6, SL_MAX_VERSION),
	    SLE_VAR(Vehicle, z_pos,         SLE_UINT8),

	    SLE_VAR(Vehicle, cur_image,     SLE_UINT16),
	    SLE_VAR(Vehicle, sprite_width,  SLE_UINT8),
	    SLE_VAR(Vehicle, sprite_height, SLE_UINT8),
	    SLE_VAR(Vehicle, z_height,      SLE_UINT8),
	    SLE_VAR(Vehicle, x_offs,        SLE_INT8),
	    SLE_VAR(Vehicle, y_offs,        SLE_INT8),
	    SLE_VAR(Vehicle, progress,      SLE_UINT8),
	    SLE_VAR(Vehicle, vehstatus,     SLE_UINT8),

	    SLE_VARX(offsetof(Vehicle, u) + offsetof(VehicleSpecial, unk0), SLE_UINT16),
	    SLE_VARX(offsetof(Vehicle, u) + offsetof(VehicleSpecial, unk2), SLE_UINT8),

	// reserve extra space in savegame here. (currently 16 bytes)
	SLE_CONDNULL(16, 2, SL_MAX_VERSION),

	SLE_END()
};

static const SaveLoad _disaster_desc[] = {
	SLE_WRITEBYTE(Vehicle, type, VEH_Disaster, 5),

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

	    SLE_VAR(Vehicle, x_offs,        SLE_INT8),
	    SLE_VAR(Vehicle, y_offs,        SLE_INT8),
	    SLE_VAR(Vehicle, sprite_width,  SLE_UINT8),
	    SLE_VAR(Vehicle, sprite_height, SLE_UINT8),
	    SLE_VAR(Vehicle, z_height,      SLE_UINT8),
	    SLE_VAR(Vehicle, owner,         SLE_UINT8),
	    SLE_VAR(Vehicle, vehstatus,     SLE_UINT8),
	SLE_CONDVARX(offsetof(Vehicle, current_order) + offsetof(Order, dest), SLE_FILE_U8 | SLE_VAR_U16, 0, 4),
	SLE_CONDVARX(offsetof(Vehicle, current_order) + offsetof(Order, dest), SLE_UINT16,                5, SL_MAX_VERSION),

	    SLE_VAR(Vehicle, cur_image,     SLE_UINT16),
	SLE_CONDVAR(Vehicle, age,           SLE_FILE_U16 | SLE_VAR_I32,  0, 30),
	SLE_CONDVAR(Vehicle, age,           SLE_INT32,                  31, SL_MAX_VERSION),
	    SLE_VAR(Vehicle, tick_counter,  SLE_UINT8),

	   SLE_VARX(offsetof(Vehicle, u) + offsetof(VehicleDisaster, image_override), SLE_UINT16),
	   SLE_VARX(offsetof(Vehicle, u) + offsetof(VehicleDisaster, unk2),           SLE_UINT16),

	// reserve extra space in savegame here. (currently 16 bytes)
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

// Will be called when the vehicles need to be saved.
static void Save_VEHS(void)
{
	Vehicle *v;
	// Write the vehicles
	FOR_ALL_VEHICLES(v) {
		SlSetArrayIndex(v->index);
		SlObject(v, _veh_descs[v->type - 0x10]);
	}
}

// Will be called when vehicles need to be loaded.
static void Load_VEHS(void)
{
	int index;
	Vehicle *v;

	while ((index = SlIterateArray()) != -1) {
		Vehicle *v;

		if (!AddBlockIfNeeded(&_Vehicle_pool, index))
			error("Vehicles: failed loading savegame: too many vehicles");

		v = GetVehicle(index);
		SlObject(v, _veh_descs[SlReadByte()]);

		/* Old savegames used 'last_station_visited = 0xFF' */
		if (CheckSavegameVersion(5) && v->last_station_visited == 0xFF)
			v->last_station_visited = INVALID_STATION;

		if (CheckSavegameVersion(5)) {
			/* Convert the current_order.type (which is a mix of type and flags, because
			 *  in those versions, they both were 4 bits big) to type and flags */
			v->current_order.flags = (v->current_order.type & 0xF0) >> 4;
			v->current_order.type  =  v->current_order.type & 0x0F;
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

const ChunkHandler _veh_chunk_handlers[] = {
	{ 'VEHS', Save_VEHS, Load_VEHS, CH_SPARSE_ARRAY | CH_LAST},
};
