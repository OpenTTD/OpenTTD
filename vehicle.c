/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
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

#define INVALID_COORD (-0x8000)
#define GEN_HASH(x,y) (((x & 0x1F80)>>7) + ((y & 0xFC0)))

/*
 *	These command macros are used to call vehicle type specific commands with non type specific commands
 *	it should be used like: DoCommandP(x, y, p1, p2, flags, CMD_STARTSTOP_VEH(v->type))
 *	that line will start/stop a vehicle nomatter what type it is
 *	VEH_Train is used as an offset because the vehicle type values doesn't start with 0
 */

#define CMD_BUILD_VEH(x)		   _veh_build_proc_table[ x - VEH_Train]
#define CMD_SELL_VEH(x)				_veh_sell_proc_table[ x - VEH_Train]
#define CMD_REFIT_VEH(x)		   _veh_refit_proc_table[ x - VEH_Train]

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
	0,	// road vehicles can't be refitted
	CMD_REFIT_SHIP,
	CMD_REFIT_AIRCRAFT,
};


enum {
	/* Max vehicles: 64000 (512 * 125) */
	VEHICLES_POOL_BLOCK_SIZE_BITS = 9,       /* In bits, so (1 << 9) == 512 */
	VEHICLES_POOL_MAX_BLOCKS      = 125,

	BLOCKS_FOR_SPECIAL_VEHICLES   = 2, //! Blocks needed for special vehicles
};

/**
 * Called if a new block is added to the vehicle-pool
 */
static void VehiclePoolNewBlock(uint start_item)
{
	Vehicle *v;

	FOR_ALL_VEHICLES_FROM(v, start_item) v->index = start_item++;
}

/* Initialize the vehicle-pool */
MemoryPool _vehicle_pool = { "Vehicle", VEHICLES_POOL_MAX_BLOCKS, VEHICLES_POOL_BLOCK_SIZE_BITS, sizeof(Vehicle), &VehiclePoolNewBlock, 0, 0, NULL };

void VehicleServiceInDepot(Vehicle *v)
{
	v->date_of_last_service = _date;
	v->breakdowns_since_last_service = 0;
	v->reliability = GetEngine(v->engine_type)->reliability;
}

bool VehicleNeedsService(const Vehicle *v)
{
	if (_patches.no_servicing_if_no_breakdowns && _opt.diff.vehicle_breakdowns == 0)
		return false;

	if (v->vehstatus & VS_CRASHED)
		return false; /* Crashed vehicles don't need service anymore */

	return _patches.servint_ispercent ?
		(v->reliability < GetEngine(v->engine_type)->reliability * (100 - v->service_interval) / 100) :
		(v->date_of_last_service + v->service_interval < _date);
}

void VehicleInTheWayErrMsg(const Vehicle* v)
{
	switch (v->type) {
		case VEH_Train:    _error_message = STR_8803_TRAIN_IN_THE_WAY;        break;
		case VEH_Road:     _error_message = STR_9000_ROAD_VEHICLE_IN_THE_WAY; break;
		case VEH_Aircraft: _error_message = STR_A015_AIRCRAFT_IN_THE_WAY;     break;
		default:           _error_message = STR_980E_SHIP_IN_THE_WAY;         break;
	}
}

static void *EnsureNoVehicleProc(Vehicle *v, void *data)
{
	if (v->tile != *(const TileIndex*)data || v->type == VEH_Disaster)
		return NULL;

	VehicleInTheWayErrMsg(v);
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
	if (!IS_INT_INSIDE(ti->z - v->z_pos, 0, 8 + 1)) return NULL;

	VehicleInTheWayErrMsg(v);
	return v;
}

static inline uint Correct_Z(uint tileh)
{
	// needs z correction for slope-type graphics that have the NORTHERN tile lowered
	// 1, 2, 3, 4, 5, 6 and 7
	return CorrectZ(tileh) ? 8 : 0;
}

uint GetCorrectTileHeight(TileIndex tile)
{
	return Correct_Z(GetTileSlope(tile, NULL));
}

bool EnsureNoVehicleZ(TileIndex tile, byte z)
{
	TileInfo ti;

	ti.tile = tile;
	ti.z = z + GetCorrectTileHeight(tile);

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
		if (v->type == VEH_Train && (IsFrontEngine(v) || IsFreeWagon(v)))
			TrainConsistChanged(v);
	}

	FOR_ALL_VEHICLES(v) {
		if (v->type != 0) {
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

	FOR_ALL_VEHICLES(v) {
		/* No more room for the special vehicles, return NULL */
		if (v->index >= (1 << _vehicle_pool.block_size_bits) * BLOCKS_FOR_SPECIAL_VEHICLES)
			return NULL;

		if (v->type == 0)
			return InitializeVehicle(v);
	}

	return NULL;
}

/*
 * finds a free vehicle in the memory or allocates a new one
 * returns a pointer to the first free vehicle or NULL if all vehicles are in use
 * *skip_vehicles is an offset to where in the array we should begin looking
 * this is to avoid looping though the same vehicles more than once after we learned that they are not free
 * this feature is used by AllocateVehicles() since it need to allocate more than one and when
 * another block is added to _vehicle_pool, since we only do that when we know it's already full
 */
static Vehicle *AllocateSingleVehicle(VehicleID *skip_vehicles)
{
	/* See note by ForceAllocateSpecialVehicle() why we skip the
	 * first blocks */
	Vehicle *v;
	const int offset = (1 << VEHICLES_POOL_BLOCK_SIZE_BITS) * BLOCKS_FOR_SPECIAL_VEHICLES;

	if (*skip_vehicles < (_vehicle_pool.total_items - offset)) {	// make sure the offset in the array is not larger than the array itself
		FOR_ALL_VEHICLES_FROM(v, offset + *skip_vehicles) {
			(*skip_vehicles)++;
			if (v->type == 0)
				return InitializeVehicle(v);
		}
	}

	/* Check if we can add a block to the pool */
	if (AddBlockToPool(&_vehicle_pool))
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
*	returns true if there is room to allocate all the vehicles
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
	int x,y,x2,y2;
	Point pt = RemapCoords(TileX(tile) * 16, TileY(tile) * 16, 0);

	x2 = ((pt.x + 104) & 0x1F80) >> 7;
	x  = ((pt.x - 174) & 0x1F80) >> 7;

	y2 = ((pt.y + 56) & 0xFC0);
	y  = ((pt.y - 294) & 0xFC0);

	for (;;) {
		int xb = x;
		for (;;) {
			VehicleID veh = _vehicle_position_hash[(x + y) & 0xFFFF];
			while (veh != INVALID_VEHICLE) {
				Vehicle *v = GetVehicle(veh);
				void *a;

				a = proc(v, data);
				if (a != NULL) return a;
				veh = v->next_hash;
			}

			if (x == x2)
				break;

			x = (x + 1) & 0x3F;
		}
		x = xb;

		if (y == y2)
			break;

		y = (y + 0x40) & ((0x3F) << 6);
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
	int i;

	/* Clean the vehicle pool, and reserve enough blocks
	 *  for the special vehicles, plus one for all the other
	 *  vehicles (which is increased on-the-fly) */
	CleanPool(&_vehicle_pool);
	AddBlockToPool(&_vehicle_pool);
	for (i = 0; i < BLOCKS_FOR_SPECIAL_VEHICLES; i++)
		AddBlockToPool(&_vehicle_pool);

	// clear it...
	memset(_vehicle_position_hash, -1, sizeof(_vehicle_position_hash));
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

	do {
		if (u->next == v) return u;
	} while ( ( u = u->next) != NULL);

	return NULL;
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

void DeleteVehicle(Vehicle *v)
{
	Vehicle *u;
	bool has_artic_part = false;

	do {
		u = v->next;
		has_artic_part = EngineHasArticPart(v);
		DeleteName(v->string_id);
		v->type = 0;
		UpdateVehiclePosHash(v, INVALID_COORD, 0);
		v->next_hash = INVALID_VEHICLE;

		if (v->orders != NULL)
			DeleteVehicleOrders(v);
		v = u;
	} while (v != NULL && has_artic_part);
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
static void MaybeReplaceVehicle(Vehicle *v);

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

	_first_veh_in_depot_list = NULL;	// now we are sure it's initialized at the start of each tick

	FOR_ALL_VEHICLES(v) {
		if (v->type != 0) {
			_vehicle_tick_procs[v->type - 0x10](v);
		}
	}

	// now we handle all the vehicles that entered a depot this tick
	v = _first_veh_in_depot_list;
	while (v != NULL) {
		Vehicle *w = v->depot_list;
		v->depot_list = NULL;	// it should always be NULL at the end of each tick
		MaybeReplaceVehicle(v);
		v = w;
	}
}

static bool CanFillVehicle_FullLoadAny(Vehicle *v)
{
	uint32 full = 0, not_full = 0;

	//special handling of aircraft

	//if the aircraft carries passengers and is NOT full, then
	//continue loading, no matter how much mail is in
	if ((v->type == VEH_Aircraft) && (v->cargo_type == CT_PASSENGERS) && (v->cargo_cap != v->cargo_count)) {
		return true;
	}

	// patch should return "true" to continue loading, i.e. when there is no cargo type that is fully loaded.
	do {
		//Should never happen, but just in case future additions change this
		assert(v->cargo_type<32);

		if (v->cargo_cap != 0) {
			uint32 mask = 1 << v->cargo_type;
			if (v->cargo_cap == v->cargo_count) full |= mask; else not_full |= mask;
		}
	} while ( (v=v->next) != NULL);

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
		if (_patches.full_load_any)
			return CanFillVehicle_FullLoadAny(v);

		do {
			if (v->cargo_count != v->cargo_cap)
				return true;
		} while ( (v=v->next) != NULL);
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
	return HASBIT(_engine_info[engine_type].refit_mask, cid) != 0;
}

static void DoDrawVehicle(const Vehicle *v)
{
	uint32 image = v->cur_image;

	if (v->vehstatus & VS_DISASTER) {
		MAKE_TRANSPARENT(image);
	} else if (v->vehstatus & VS_DEFPAL) {
		image |= (v->vehstatus & VS_CRASHED) ? PALETTE_CRASH : GetVehiclePalette(v);
	}

	AddSortableSpriteToDraw(image, v->x_pos + v->x_offs, v->y_pos + v->y_offs,
		v->sprite_width, v->sprite_height, v->z_height, v->z_pos);
}

void ViewportAddVehicles(DrawPixelInfo *dpi)
{
	int x,xb, y, x2, y2;
	VehicleID veh;
	Vehicle *v;

	x  = ((dpi->left - 70) & 0x1F80) >> 7;
	x2 = ((dpi->left + dpi->width) & 0x1F80) >> 7;

	y  = ((dpi->top - 70) & 0xFC0);
	y2 = ((dpi->top + dpi->height) & 0xFC0);

	for (;;) {
		xb = x;
		for (;;) {
			veh = _vehicle_position_hash[(x + y) & 0xFFFF];
			while (veh != INVALID_VEHICLE) {
				v = GetVehicle(veh);

				if (!(v->vehstatus & VS_HIDDEN) &&
						dpi->left <= v->right_coord &&
						dpi->top <= v->bottom_coord &&
						dpi->left + dpi->width >= v->left_coord &&
						dpi->top + dpi->height >= v->top_coord) {
					DoDrawVehicle(v);
				}
				veh = v->next_hash;
			}

			if (x == x2)
				break;
			x = (x + 1) & 0x3F;
		}
		x = xb;

		if (y == y2)
			break;
		y = (y + 0x40) & ((0x3F) << 6);
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
	MK(0,0,1,0),
	MK(1,0,1,1),
	MK(0,0,1,0),
	MK(1,0,1,2),
	ME(1)
};


static const BubbleMovement _bubble_float_ne[] = {
	MK(0,0,1,0),
	MK(-1,0,1,1),
	MK(0,0,1,0),
	MK(-1,0,1,2),
	ME(1)
};

static const BubbleMovement _bubble_float_se[] = {
	MK(0,0,1,0),
	MK(0,1,1,1),
	MK(0,0,1,0),
	MK(0,1,1,2),
	ME(1)
};

static const BubbleMovement _bubble_float_nw[] = {
	MK(0,0,1,0),
	MK(0,-1,1,1),
	MK(0,0,1,0),
	MK(0,-1,1,2),
	ME(1)
};

static const BubbleMovement _bubble_burst[] = {
	MK(0,0,1,2),
	MK(0,0,1,7),
	MK(0,0,1,8),
	MK(0,0,1,9),
	ME(0)
};

static const BubbleMovement _bubble_absorb[] = {
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(0,0,1,0),
	MK(0,0,1,2),
	MK(0,0,1,0),
	MK(0,0,1,1),
	MK(2,1,3,0),
	MK(1,1,3,1),
	MK(2,1,3,0),
	MK(1,1,3,2),
	MK(2,1,3,0),
	MK(1,1,3,1),
	MK(2,1,3,0),
	MK(1,0,1,2),
	MK(0,0,1,0),
	MK(1,0,1,1),
	MK(0,0,1,0),
	MK(1,0,1,2),
	MK(0,0,1,0),
	MK(1,0,1,1),
	MK(0,0,1,0),
	MK(1,0,1,2),
	ME(2),
	MK(0,0,0,0xA),
	MK(0,0,0,0xB),
	MK(0,0,0,0xC),
	MK(0,0,0,0xD),
	MK(0,0,0,0xE),
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
		if (IsTileType(tile, MP_INDUSTRY) && _m[tile].m5 == 0xA2) AddAnimatedTile(tile);
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
		if (v->type != 0 && (v->vehstatus & (VS_HIDDEN|VS_UNCLICKABLE)) == 0 &&
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
	3, 3, 3, 3, 3, 3, 3, 3,
	4, 4, 5, 5, 6, 6, 7, 7,
	8, 8, 9, 9, 10, 10, 11, 11,
	12, 13, 13, 13, 13, 14, 15, 16,
	17, 19, 21, 25, 28, 31, 34, 37,
	40, 44, 48, 52, 56, 60, 64, 68,
	72, 80, 90, 100, 110, 120, 130, 140,
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

/** Clone a vehicle. If it is a train, it will clone all the cars too
* @param x,y depot where the cloned vehicle is build
* @param p1 the original vehicle's index
* @param p2 1 = shared orders, else copied orders
*/
int32 CmdCloneVehicle(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v_front, *v;
	Vehicle *w_front, *w, *w_rear;
	int cost, total_cost = 0;

	if (!IsVehicleIndex(p1)) return CMD_ERROR;
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

		cost = DoCommand(x, y, v->engine_type, 1, flags, CMD_BUILD_VEH(v->type));

		if (CmdFailed(cost)) return cost;

		total_cost += cost;

		if (flags & DC_EXEC) {
			w = GetVehicle(_new_vehicle_id);

			if (v->type != VEH_Road) { // road vehicles can't be refitted
				if (v->cargo_type != w->cargo_type) {
					DoCommand(x, y, w->index, v->cargo_type, flags, CMD_REFIT_VEH(v->type));
				}
			}

			if (v->type == VEH_Train && !IsFrontEngine(v)) {
				// this s a train car
				// add this unit to the end of the train
				DoCommand(x, y, (w_rear->index << 16) | w->index, 1, flags, CMD_MOVE_RAIL_VEHICLE);
			} else {
				// this is a front engine or not a train. It need orders
				w_front = w;
				DoCommand(x, y, (v->index << 16) | w->index, p2 & 1 ? CO_SHARE : CO_COPY, flags, CMD_CLONE_ORDER);
			}
			w_rear = w;	// trains needs to know the last car in the train, so they can add more in next loop
		}
	} while (v->type == VEH_Train && (v = GetNextVehicle(v)) != NULL);

	if (flags & DC_EXEC && v_front->type == VEH_Train) {
		// _new_train_id needs to be the front engine due to the callback function
		_new_train_id = w_front->index;
	}
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
				continue;	// cargo not compatible

			if (dest->cargo_count == dest->cargo_cap)
				continue;	// the destination vehicle is already full

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

/* Replaces a vehicle (used to be called autorenew)
 * This function is only called from MaybeReplaceVehicle()
 * Must be called with _current_player set to the owner of the vehicle
 * @param w Vehicle to replace
 * @param flags is the flags to use when calling DoCommand(). Mainly DC_EXEC counts
 * @return value is cost of the replacement or CMD_ERROR
 */
static int32 ReplaceVehicle(Vehicle **w, byte flags)
{
	int32 cost;
	Vehicle *old_v = *w;
	const Player *p = GetPlayer(old_v->owner);
	EngineID new_engine_type;
	const UnitID cached_unitnumber = old_v->unitnumber;
	bool new_front = false;
	Vehicle *new_v = NULL;
	char vehicle_name[32];

	new_engine_type = EngineReplacementForPlayer(p, old_v->engine_type);
	if (new_engine_type == INVALID_ENGINE) new_engine_type = old_v->engine_type;

	cost = DoCommand(old_v->x_pos, old_v->y_pos, new_engine_type, 1, flags, CMD_BUILD_VEH(old_v->type));
	if (CmdFailed(cost)) return cost;

	if (flags & DC_EXEC) {
		new_v = GetVehicle(_new_vehicle_id);
		*w = new_v;	//we changed the vehicle, so MaybeReplaceVehicle needs to work on the new one. Now we tell it what the new one is

		/* refit if needed */
		if (new_v->type != VEH_Road) { // road vehicles can't be refitted
			if (old_v->cargo_type != new_v->cargo_type && old_v->cargo_cap != 0 && new_v->cargo_cap != 0) {// some train engines do not have cargo capacity
				DoCommand(0, 0, new_v->index, old_v->cargo_type, DC_EXEC, CMD_REFIT_VEH(new_v->type));
			}
		}


		if (old_v->type == VEH_Train && !IsFrontEngine(old_v)) {
			/* this is a railcar. We need to move the car into the train
			 * We add the new engine after the old one instead of replacing it. It will give the same result anyway when we
			 * sell the old engine in a moment
			 */
			DoCommand(0, 0, (GetPrevVehicleInChain(old_v)->index << 16) | new_v->index, 1, DC_EXEC, CMD_MOVE_RAIL_VEHICLE);
			/* Now we move the old one out of the train */
			DoCommand(0, 0, (INVALID_VEHICLE << 16) | old_v->index, 0, DC_EXEC, CMD_MOVE_RAIL_VEHICLE);
		} else {
			// copy/clone the orders
			DoCommand(0, 0, (old_v->index << 16) | new_v->index, IsOrderListShared(old_v) ? CO_SHARE : CO_COPY, DC_EXEC, CMD_CLONE_ORDER);
			new_v->cur_order_index = old_v->cur_order_index;
			ChangeVehicleViewWindow(old_v, new_v);
			new_v->profit_this_year = old_v->profit_this_year;
			new_v->profit_last_year = old_v->profit_last_year;
			new_front = true;

			new_v->current_order = old_v->current_order;
			if (old_v->type == VEH_Train && GetNextVehicle(old_v) != NULL){
				Vehicle *temp_v = GetNextVehicle(old_v);

				// move the entire train to the new engine, excluding the old engine
				if (IsMultiheaded(old_v) && temp_v == old_v->u.rail.other_multiheaded_part) {
					// we got front and rear of a multiheaded engine right after each other. We should work with the next in line instead
					temp_v = GetNextVehicle(temp_v);
				}

				if (temp_v != NULL) {
					DoCommand(0, 0, (new_v->index << 16) | temp_v->index, 1, DC_EXEC, CMD_MOVE_RAIL_VEHICLE);
				}
			}
		}
		/* We are done setting up the new vehicle. Now we move the cargo from the old one to the new one */
		MoveVehicleCargo(new_v->type == VEH_Train ? GetFirstVehicleInChain(new_v) : new_v, old_v);

		// Get the name of the old vehicle if it has a custom name.
		if ((old_v->string_id & 0xF800) != 0x7800) {
			vehicle_name[0] = '\0';
		} else {
			GetName(old_v->string_id & 0x7FF, vehicle_name);
		}
	}

	// sell the engine/ find out how much you get for the old engine
	cost += DoCommand(0, 0, old_v->index, 0, flags, CMD_SELL_VEH(old_v->type));

	if (new_front) {
		// now we assign the old unitnumber to the new vehicle
		new_v->unitnumber = cached_unitnumber;
	}

	// Transfer the name of the old vehicle.
	if ((flags & DC_EXEC) && vehicle_name[0] != '\0') {
		_cmd_text = vehicle_name;
		DoCommand(0, 0, new_v->index, 0, DC_EXEC, CMD_NAME_VEHICLE);
	}

	return cost;
}

/** replaces a vehicle if it's set for autoreplace or is too old
 * (used to be called autorenew)
 * @param v The vehicle to replace
 *	if the vehicle is a train, v needs to be the front engine
 *	return value is a pointer to the new vehicle, which is the same as the argument if nothing happened
 */
static void MaybeReplaceVehicle(Vehicle *v)
{
	Vehicle *w;
	const Player *p = GetPlayer(v->owner);
	byte flags = 0;
	int32 cost, temp_cost = 0;
	bool stopped = false;

	/* Remember the length in case we need to trim train later on
	 * If it's not a train, the value is unused */
	uint16 old_total_length = (v->type == VEH_Train) ? v->u.rail.cached_total_length : -1;

	_current_player = v->owner;

	assert(v->type == VEH_Train || v->type == VEH_Road || v->type == VEH_Ship || v->type == VEH_Aircraft);

	assert(v->vehstatus & VS_STOPPED);	// the vehicle should have been stopped in VehicleEnteredDepotThisTick() if needed

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

			if (w->type == VEH_Train && IsTrainWagon(w) && !CanRefitTo(EngineReplacementForPlayer(p, w->engine_type), w->cargo_type)) {
				// we can't replace this wagon since we can't refit the new one to the right cargo type
				continue;
			}

			/* Now replace the vehicle */
			temp_cost = ReplaceVehicle(&w, flags);

			if (flags & DC_EXEC &&
					(w->type != VEH_Train || w->u.rail.first_engine == INVALID_ENGINE)) {
				/* now we bought a new engine and sold the old one. We need to fix the
				 * pointers in order to avoid pointing to the old one for trains: these
				 * pointers should point to the front engine and not the cars
				 */
				v = w;
			}

			if (CmdFailed(temp_cost)) break;

			cost += temp_cost;
		} while (w->type == VEH_Train && (w = GetNextVehicle(w)) != NULL);

		if (!(flags & DC_EXEC) && (CmdFailed(temp_cost) || p->money64 < (int32)(cost + p->engine_renew_money) || cost == 0)) {
			if (p->money64 < (int32)(cost + p->engine_renew_money) && ( _local_player == v->owner ) && cost != 0) {
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
			_current_player = OWNER_NONE;
			return;
		}

		if (flags & DC_EXEC) {
			break;	// we are done replacing since the loop ran once with DC_EXEC
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
			DoCommand(0, 0, (INVALID_VEHICLE << 16) | temp->index, 0, DC_EXEC, CMD_MOVE_RAIL_VEHICLE);
			MoveVehicleCargo(v, temp);
			cost += DoCommand(0, 0, temp->index, 0, flags, CMD_SELL_VEH(temp->type));
		}
	}

	if (IsLocalPlayer()) ShowCostOrIncomeAnimation(v->x_pos, v->y_pos, v->z_pos, cost);

	if (stopped) v->vehstatus &= ~VS_STOPPED;
	_current_player = OWNER_NONE;
}


/** Give a custom name to your vehicle
 * @param x,y unused
 * @param p1 vehicle ID to name
 * @param p2 unused
 */
int32 CmdNameVehicle(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle *v;
	StringID str;

	if (!IsVehicleIndex(p1) || _cmd_text[0] == '\0') return CMD_ERROR;

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
 * @param x,y unused
 * @param p1 vehicle ID that is being service-interval-changed
 * @param p2 new service interval
 */
int32 CmdChangeServiceInt(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	Vehicle* v;
	uint16 serv_int = GetServiceIntervalClamped(p2); /* Double check the service interval from the user-input */

	if (serv_int != p2 || !IsVehicleIndex(p1)) return CMD_ERROR;

	v = GetVehicle(p1);

	if (v->type == 0 || !CheckOwnership(v->owner)) return CMD_ERROR;

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

static const byte _new_direction_table[9] = {
	0, 7, 6,
	1, 3, 5,
	2, 3, 4,
};

byte GetDirectionTowards(const Vehicle *v, int x, int y)
{
	byte dirdiff, dir;
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

	dirdiff = _new_direction_table[i] - dir;
	if (dirdiff == 0)
		return dir;
	return (dir+((dirdiff&7)<5?1:-1)) & 7;
}

Trackdir GetVehicleTrackdir(const Vehicle* v)
{
	if (v->vehstatus & VS_CRASHED) return 0xFF;

	switch (v->type) {
		case VEH_Train:
			if (v->u.rail.track == 0x80) /* We'll assume the train is facing outwards */
				return DiagdirToDiagTrackdir(GetDepotDirection(v->tile, TRANSPORT_RAIL)); /* Train in depot */

			if (v->u.rail.track == 0x40) /* train in tunnel, so just use his direction and assume a diagonal track */
				return DiagdirToDiagTrackdir((v->direction >> 1) & 3);

			return TrackDirectionToTrackdir(FIND_FIRST_BIT(v->u.rail.track),v->direction);

		case VEH_Ship:
			if (v->u.ship.state == 0x80)  /* Inside a depot? */
				/* We'll assume the ship is facing outwards */
				return DiagdirToDiagTrackdir(GetDepotDirection(v->tile, TRANSPORT_WATER)); /* Ship in depot */

			return TrackDirectionToTrackdir(FIND_FIRST_BIT(v->u.ship.state),v->direction);

		case VEH_Road:
			if (v->u.road.state == 254) /* We'll assume the road vehicle is facing outwards */
				return DiagdirToDiagTrackdir(GetDepotDirection(v->tile, TRANSPORT_ROAD)); /* Road vehicle in depot */

			if (IsRoadStationTile(v->tile)) /* We'll assume the road vehicle is facing outwards */
				return DiagdirToDiagTrackdir(GetRoadStationDir(v->tile)); /* Road vehicle in a station */

			return DiagdirToDiagTrackdir((v->direction >> 1) & 3);

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
	TileIndex old_tile = v->tile;
	uint32 result = _tile_type_procs[GetTileType(tile)]->vehicle_enter_tile_proc(v, tile, x, y);

	/* When vehicle_enter_tile_proc returns 8, that apparently means that
	 * we cannot enter the tile at all. In that case, don't call
	 * leave_tile. */
	if (!(result & 8) && old_tile != tile) {
		VehicleLeaveTileProc *proc = _tile_type_procs[GetTileType(old_tile)]->vehicle_leave_tile_proc;
		if (proc != NULL)
			proc(v, old_tile, x, y);
	}
	return result;
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

// XXX Temporary stub -- will be expanded
PalSpriteID GetEngineColourMap(PlayerID player)
{
	return SPRITE_PALETTE(PLAYER_SPRITE_COLOR(player));
}

// Save and load of vehicles
const SaveLoad _common_veh_desc[] = {
	SLE_VAR(Vehicle,subtype,					SLE_UINT8),

	SLE_REF(Vehicle,next,							REF_VEHICLE_OLD),
	SLE_VAR(Vehicle,string_id,				SLE_STRINGID),
	SLE_CONDVAR(Vehicle,unitnumber,				SLE_FILE_U8 | SLE_VAR_U16, 0, 7),
	SLE_CONDVAR(Vehicle,unitnumber,				SLE_UINT16, 8, 255),
	SLE_VAR(Vehicle,owner,						SLE_UINT8),
	SLE_CONDVAR(Vehicle,tile,					SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(Vehicle,tile,					SLE_UINT32, 6, 255),
	SLE_CONDVAR(Vehicle,dest_tile,		SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(Vehicle,dest_tile,		SLE_UINT32, 6, 255),

	SLE_CONDVAR(Vehicle,x_pos,				SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(Vehicle,x_pos,				SLE_UINT32, 6, 255),
	SLE_CONDVAR(Vehicle,y_pos,				SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(Vehicle,y_pos,				SLE_UINT32, 6, 255),
	SLE_VAR(Vehicle,z_pos,						SLE_UINT8),
	SLE_VAR(Vehicle,direction,				SLE_UINT8),

	SLE_VAR(Vehicle,cur_image,				SLE_UINT16),
	SLE_VAR(Vehicle,spritenum,				SLE_UINT8),
	SLE_VAR(Vehicle,sprite_width,			SLE_UINT8),
	SLE_VAR(Vehicle,sprite_height,		SLE_UINT8),
	SLE_VAR(Vehicle,z_height,					SLE_UINT8),
	SLE_VAR(Vehicle,x_offs,						SLE_INT8),
	SLE_VAR(Vehicle,y_offs,						SLE_INT8),
	SLE_VAR(Vehicle,engine_type,			SLE_UINT16),

	SLE_VAR(Vehicle,max_speed,				SLE_UINT16),
	SLE_VAR(Vehicle,cur_speed,				SLE_UINT16),
	SLE_VAR(Vehicle,subspeed,					SLE_UINT8),
	SLE_VAR(Vehicle,acceleration,			SLE_UINT8),
	SLE_VAR(Vehicle,progress,					SLE_UINT8),

	SLE_VAR(Vehicle,vehstatus,				SLE_UINT8),
	SLE_CONDVAR(Vehicle,last_station_visited, SLE_FILE_U8 | SLE_VAR_U16, 0, 4),
	SLE_CONDVAR(Vehicle,last_station_visited, SLE_UINT16, 5, 255),

	SLE_VAR(Vehicle,cargo_type,				SLE_UINT8),
	SLE_VAR(Vehicle,cargo_days,				SLE_UINT8),
	SLE_CONDVAR(Vehicle,cargo_source,			SLE_FILE_U8 | SLE_VAR_U16, 0, 6),
	SLE_CONDVAR(Vehicle,cargo_source,			SLE_UINT16, 7, 255),
	SLE_VAR(Vehicle,cargo_cap,				SLE_UINT16),
	SLE_VAR(Vehicle,cargo_count,			SLE_UINT16),

	SLE_VAR(Vehicle,day_counter,			SLE_UINT8),
	SLE_VAR(Vehicle,tick_counter,			SLE_UINT8),

	SLE_VAR(Vehicle,cur_order_index,	SLE_UINT8),
	SLE_VAR(Vehicle,num_orders,				SLE_UINT8),

	/* This next line is for version 4 and prior compatibility.. it temporarily reads
	    type and flags (which were both 4 bits) into type. Later on this is
	    converted correctly */
	SLE_CONDVARX(offsetof(Vehicle, current_order) + offsetof(Order, type),    SLE_UINT8,  0, 4),
	SLE_CONDVARX(offsetof(Vehicle, current_order) + offsetof(Order, station), SLE_FILE_U8 | SLE_VAR_U16, 0, 4),

	/* Orders for version 5 and on */
	SLE_CONDVARX(offsetof(Vehicle, current_order) + offsetof(Order, type),    SLE_UINT8,  5, 255),
	SLE_CONDVARX(offsetof(Vehicle, current_order) + offsetof(Order, flags),   SLE_UINT8,  5, 255),
	SLE_CONDVARX(offsetof(Vehicle, current_order) + offsetof(Order, station), SLE_UINT16, 5, 255),

	SLE_REF(Vehicle,orders,						REF_ORDER),

	SLE_VAR(Vehicle,age,							SLE_UINT16),
	SLE_VAR(Vehicle,max_age,					SLE_UINT16),
	SLE_VAR(Vehicle,date_of_last_service,SLE_UINT16),
	SLE_VAR(Vehicle,service_interval,	SLE_UINT16),
	SLE_VAR(Vehicle,reliability,			SLE_UINT16),
	SLE_VAR(Vehicle,reliability_spd_dec,SLE_UINT16),
	SLE_VAR(Vehicle,breakdown_ctr,		SLE_UINT8),
	SLE_VAR(Vehicle,breakdown_delay,	SLE_UINT8),
	SLE_VAR(Vehicle,breakdowns_since_last_service,	SLE_UINT8),
	SLE_VAR(Vehicle,breakdown_chance,	SLE_UINT8),
	SLE_VAR(Vehicle,build_year,				SLE_UINT8),

	SLE_VAR(Vehicle,load_unload_time_rem,	SLE_UINT16),

	SLE_VAR(Vehicle,profit_this_year,	SLE_INT32),
	SLE_VAR(Vehicle,profit_last_year,	SLE_INT32),
	SLE_VAR(Vehicle,value,						SLE_UINT32),

	SLE_VAR(Vehicle,random_bits,       SLE_UINT8),
	SLE_VAR(Vehicle,waiting_triggers,  SLE_UINT8),

	SLE_REF(Vehicle,next_shared,				REF_VEHICLE),
	SLE_REF(Vehicle,prev_shared,				REF_VEHICLE),

	// reserve extra space in savegame here. (currently 10 bytes)
	SLE_CONDARR(NullStruct,null,SLE_FILE_U8 | SLE_VAR_NULL, 10, 2, 255),

	SLE_END()
};


static const SaveLoad _train_desc[] = {
	SLE_WRITEBYTE(Vehicle,type,VEH_Train, 0), // Train type. VEH_Train in mem, 0 in file.
	SLE_INCLUDEX(0, INC_VEHICLE_COMMON),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleRail,crash_anim_pos), SLE_UINT16),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleRail,force_proceed), SLE_UINT8),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleRail,railtype), SLE_UINT8),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleRail,track), SLE_UINT8),

	SLE_CONDVARX(offsetof(Vehicle,u)+offsetof(VehicleRail,flags), SLE_UINT8, 2, 255),
	SLE_CONDVARX(offsetof(Vehicle,u)+offsetof(VehicleRail,days_since_order_progr), SLE_UINT16, 2, 255),

	SLE_CONDARR(NullStruct, null, SLE_FILE_U8 | SLE_VAR_NULL, 2, 2, 19),
	// reserve extra space in savegame here. (currently 11 bytes)
	SLE_CONDARR(NullStruct, null, SLE_FILE_U8 | SLE_VAR_NULL, 11, 2, 255),

	SLE_END()
};

static const SaveLoad _roadveh_desc[] = {
	SLE_WRITEBYTE(Vehicle,type,VEH_Road, 1), // Road type. VEH_Road in mem, 1 in file.
	SLE_INCLUDEX(0, INC_VEHICLE_COMMON),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleRoad,state),					SLE_UINT8),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleRoad,frame),					SLE_UINT8),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleRoad,blocked_ctr),		SLE_UINT16),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleRoad,overtaking),		SLE_UINT8),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleRoad,overtaking_ctr),SLE_UINT8),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleRoad,crashed_ctr),		SLE_UINT16),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleRoad,reverse_ctr),			SLE_UINT8),

	SLE_CONDREFX(offsetof(Vehicle,u)+offsetof(VehicleRoad,slot), REF_ROADSTOPS, 6, 255),
	SLE_CONDVARX(offsetof(Vehicle,u)+offsetof(VehicleRoad,slotindex), SLE_UINT8, 6, 255),
	SLE_CONDVARX(offsetof(Vehicle,u)+offsetof(VehicleRoad,slot_age), SLE_UINT8, 6, 255),
	// reserve extra space in savegame here. (currently 16 bytes)
	SLE_CONDARR(NullStruct,null,SLE_FILE_U64 | SLE_VAR_NULL, 2, 2, 255),

	SLE_END()
};

static const SaveLoad _ship_desc[] = {
	SLE_WRITEBYTE(Vehicle,type,VEH_Ship, 2), // Ship type. VEH_Ship in mem, 2 in file.
	SLE_INCLUDEX(0, INC_VEHICLE_COMMON),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleShip,state),				SLE_UINT8),

	// reserve extra space in savegame here. (currently 16 bytes)
	SLE_CONDARR(NullStruct,null,SLE_FILE_U64 | SLE_VAR_NULL, 2, 2, 255),

	SLE_END()
};

static const SaveLoad _aircraft_desc[] = {
	SLE_WRITEBYTE(Vehicle,type,VEH_Aircraft, 3), // Aircraft type. VEH_Aircraft in mem, 3 in file.
	SLE_INCLUDEX(0, INC_VEHICLE_COMMON),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleAir,crashed_counter),	SLE_UINT16),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleAir,pos),							SLE_UINT8),

	SLE_CONDVARX(offsetof(Vehicle,u)+offsetof(VehicleAir,targetairport),		SLE_FILE_U8 | SLE_VAR_U16, 0, 4),
	SLE_CONDVARX(offsetof(Vehicle,u)+offsetof(VehicleAir,targetairport),		SLE_UINT16, 5, 255),

	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleAir,state),						SLE_UINT8),

	SLE_CONDVARX(offsetof(Vehicle,u)+offsetof(VehicleAir,previous_pos),			SLE_UINT8, 2, 255),

	// reserve extra space in savegame here. (currently 15 bytes)
	SLE_CONDARR(NullStruct,null,SLE_FILE_U8 | SLE_VAR_NULL, 15, 2, 255),

	SLE_END()
};

static const SaveLoad _special_desc[] = {
	SLE_WRITEBYTE(Vehicle,type,VEH_Special, 4),

	SLE_VAR(Vehicle,subtype,					SLE_UINT8),

	SLE_CONDVAR(Vehicle,tile,					SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(Vehicle,tile,					SLE_UINT32, 6, 255),

	SLE_CONDVAR(Vehicle,x_pos,				SLE_FILE_I16 | SLE_VAR_I32, 0, 5),
	SLE_CONDVAR(Vehicle,x_pos,				SLE_INT32, 6, 255),
	SLE_CONDVAR(Vehicle,y_pos,				SLE_FILE_I16 | SLE_VAR_I32, 0, 5),
	SLE_CONDVAR(Vehicle,y_pos,				SLE_INT32, 6, 255),
	SLE_VAR(Vehicle,z_pos,						SLE_UINT8),

	SLE_VAR(Vehicle,cur_image,				SLE_UINT16),
	SLE_VAR(Vehicle,sprite_width,			SLE_UINT8),
	SLE_VAR(Vehicle,sprite_height,		SLE_UINT8),
	SLE_VAR(Vehicle,z_height,					SLE_UINT8),
	SLE_VAR(Vehicle,x_offs,						SLE_INT8),
	SLE_VAR(Vehicle,y_offs,						SLE_INT8),
	SLE_VAR(Vehicle,progress,					SLE_UINT8),
	SLE_VAR(Vehicle,vehstatus,				SLE_UINT8),

	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleSpecial,unk0),	SLE_UINT16),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleSpecial,unk2),	SLE_UINT8),

	// reserve extra space in savegame here. (currently 16 bytes)
	SLE_CONDARR(NullStruct,null,SLE_FILE_U64 | SLE_VAR_NULL, 2, 2, 255),

	SLE_END()
};

static const SaveLoad _disaster_desc[] = {
	SLE_WRITEBYTE(Vehicle,type,VEH_Disaster, 5),

	SLE_REF(Vehicle,next,							REF_VEHICLE_OLD),

	SLE_VAR(Vehicle,subtype,					SLE_UINT8),
	SLE_CONDVAR(Vehicle,tile,					SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(Vehicle,tile,					SLE_UINT32, 6, 255),
	SLE_CONDVAR(Vehicle,dest_tile,		SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLE_CONDVAR(Vehicle,dest_tile,		SLE_UINT32, 6, 255),

	SLE_CONDVAR(Vehicle,x_pos,				SLE_FILE_I16 | SLE_VAR_I32, 0, 5),
	SLE_CONDVAR(Vehicle,x_pos,				SLE_INT32, 6, 255),
	SLE_CONDVAR(Vehicle,y_pos,				SLE_FILE_I16 | SLE_VAR_I32, 0, 5),
	SLE_CONDVAR(Vehicle,y_pos,				SLE_INT32, 6, 255),
	SLE_VAR(Vehicle,z_pos,						SLE_UINT8),
	SLE_VAR(Vehicle,direction,				SLE_UINT8),

	SLE_VAR(Vehicle,x_offs,						SLE_INT8),
	SLE_VAR(Vehicle,y_offs,						SLE_INT8),
	SLE_VAR(Vehicle,sprite_width,			SLE_UINT8),
	SLE_VAR(Vehicle,sprite_height,		SLE_UINT8),
	SLE_VAR(Vehicle,z_height,					SLE_UINT8),
	SLE_VAR(Vehicle,owner,						SLE_UINT8),
	SLE_VAR(Vehicle,vehstatus,				SLE_UINT8),
	SLE_CONDVARX(offsetof(Vehicle, current_order) + offsetof(Order, station), SLE_FILE_U8 | SLE_VAR_U16, 0, 4),
	SLE_CONDVARX(offsetof(Vehicle, current_order) + offsetof(Order, station), SLE_UINT16, 5, 255),

	SLE_VAR(Vehicle,cur_image,				SLE_UINT16),
	SLE_VAR(Vehicle,age,							SLE_UINT16),

	SLE_VAR(Vehicle,tick_counter,			SLE_UINT8),

	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleDisaster,image_override),	SLE_UINT16),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleDisaster,unk2),						SLE_UINT16),

	// reserve extra space in savegame here. (currently 16 bytes)
	SLE_CONDARR(NullStruct,null,SLE_FILE_U64 | SLE_VAR_NULL, 2, 2, 255),

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
		if (v->type != 0) {
			SlSetArrayIndex(v->index);
			SlObject(v, _veh_descs[v->type - 0x10]);
		}
	}
}

// Will be called when vehicles need to be loaded.
static void Load_VEHS(void)
{
	int index;
	Vehicle *v;

	while ((index = SlIterateArray()) != -1) {
		Vehicle *v;

		if (!AddBlockIfNeeded(&_vehicle_pool, index))
			error("Vehicles: failed loading savegame: too many vehicles");

		v = GetVehicle(index);
		SlObject(v, _veh_descs[SlReadByte()]);

		/* Old savegames used 'last_station_visited = 0xFF' */
		if (CheckSavegameVersion(5) && v->last_station_visited == 0xFF)
			v->last_station_visited = INVALID_STATION;

		if (CheckSavegameVersion(5)) {
			/* Convert the current_order.type (which is a mix of type and flags, because
			    in those versions, they both were 4 bits big) to type and flags */
			v->current_order.flags = (v->current_order.type & 0xF0) >> 4;
			v->current_order.type  =  v->current_order.type & 0x0F;
		}
	}

	/* Check for shared order-lists (we now use pointers for that) */
	if (CheckSavegameVersionOldStyle(5, 2)) {
		FOR_ALL_VEHICLES(v) {
			Vehicle *u;

			if (v->type == 0)
				continue;

			FOR_ALL_VEHICLES_FROM(u, v->index + 1) {
				if (u->type == 0)
					continue;

				/* If a vehicle has the same orders, add the link to eachother
				    in both vehicles */
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
