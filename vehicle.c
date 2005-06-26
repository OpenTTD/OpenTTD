#include "stdafx.h"
#include "openttd.h"
#include "spritecache.h"
#include "table/sprites.h"
#include "table/strings.h"
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

#define INVALID_COORD (-0x8000)
#define GEN_HASH(x,y) (((x & 0x1F80)>>7) + ((y & 0xFC0)))

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

	FOR_ALL_VEHICLES_FROM(v, start_item)
		v->index = start_item++;
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

void VehicleInTheWayErrMsg(Vehicle *v)
{
	StringID id;

	(id = STR_8803_TRAIN_IN_THE_WAY,v->type == VEH_Train) ||
	(id = STR_9000_ROAD_VEHICLE_IN_THE_WAY,v->type == VEH_Road) ||
	(id = STR_A015_AIRCRAFT_IN_THE_WAY,v->type == VEH_Aircraft) ||
		(id = STR_980E_SHIP_IN_THE_WAY, true);

	_error_message = id;
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

	if (v->tile != ti->tile || v->z_pos != ti->z || v->type == VEH_Disaster)
		return NULL;

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

	FindLandscapeHeightByTile(&ti, tile);
	ti.z = z + Correct_Z(ti.tileh);

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

void VehiclePositionChanged(Vehicle *v)
{
	int img = v->cur_image;
	const SpriteDimension *sd;
	Point pt = RemapCoords(v->x_pos + v->x_offs, v->y_pos + v->y_offs, v->z_pos);

	sd = GetSpriteDimension(img);

	pt.x += sd->xoffs;
	pt.y += sd->yoffs;

	UpdateVehiclePosHash(v, pt.x, pt.y);

	v->left_coord = pt.x;
	v->top_coord = pt.y;
	v->right_coord = pt.x + sd->xsize + 2;
	v->bottom_coord = pt.y + sd->ysize + 2;
}

// Called after load to update coordinates
void AfterLoadVehicles(void)
{
	Vehicle *v;

	FOR_ALL_VEHICLES(v) {
		v->first = NULL;
		if (v->type != 0) {
			v->left_coord = INVALID_COORD;
			VehiclePositionChanged(v);

			if (v->type == VEH_Train && (v->subtype == TS_Front_Engine || v->subtype == TS_Free_Car))
				TrainConsistChanged(v);
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
	/* random_bits is used to pick out a random sprite for vehicles
	    which are technical the same (newgrf stuff).
	   Because RandomRange() results in desyncs, and because it does
	    not really matter that one client has other visual vehicles than
	    the other, it can be InteractiveRandomRange() without any problem
	*/
	v->random_bits = InteractiveRandomRange(256);
	return v;
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

Vehicle *AllocateVehicle(void)
{
	/* See note by ForceAllocateSpecialVehicle() why we skip the
	 * first blocks */
	Vehicle *v;

	FOR_ALL_VEHICLES_FROM(v, (1 << _vehicle_pool.block_size_bits) * BLOCKS_FOR_SPECIAL_VEHICLES) {
		if (v->type == 0)
			return InitializeVehicle(v);
	}

	/* Check if we can add a block to the pool */
	if (AddBlockToPool(&_vehicle_pool))
		return AllocateVehicle();

	return NULL;
}

void *VehicleFromPos(TileIndex tile, void *data, VehicleFromPosProc *proc)
{
	int x,y,x2,y2;
	VehicleID veh;
	Point pt = RemapCoords(TileX(tile) * 16, TileY(tile) * 16, 0);

	x2 = ((pt.x + 104) & 0x1F80) >> 7;
	x  = ((pt.x - 174) & 0x1F80) >> 7;

	y2 = ((pt.y + 56) & 0xFC0);
	y  = ((pt.y - 294) & 0xFC0);

	for(;;) {
		int xb = x;
		for(;;) {
			veh = _vehicle_position_hash[ (x+y)&0xFFFF ];
			while (veh != INVALID_VEHICLE) {
				Vehicle *v = GetVehicle(veh);
				void *a;

				if ((a = proc(v, data)) != NULL)
					return a;
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



void UpdateVehiclePosHash(Vehicle *v, int x, int y)
{
	VehicleID *old_hash, *new_hash;
	int old_x = v->left_coord;
	int old_y = v->top_coord;
	Vehicle *u;

	new_hash = (x == INVALID_COORD) ? NULL : &_vehicle_position_hash[GEN_HASH(x,y)];
	old_hash = (old_x == INVALID_COORD) ? NULL : &_vehicle_position_hash[GEN_HASH(old_x, old_y)];

	if (old_hash == new_hash)
		return;

	/* remove from hash table? */
	if (old_hash != NULL) {
		Vehicle *last = NULL;
		int idx = *old_hash;
		while ((u = GetVehicle(idx)) != v) {
			idx = u->next_hash;
			assert(idx != INVALID_VEHICLE);
			last = u;
		}

		if (last == NULL)
			*old_hash = v->next_hash;
		else
			last->next_hash = v->next_hash;
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
		if (v->first->subtype == TS_Front_Engine) return v->first;

		DEBUG(misc, 0) ("v->first cache faulty. We shouldn't be here, rebuilding cache!");
	}

	/* It is the fact (currently) that newly built vehicles do not have
	* their ->first pointer set. When this is the case, go up to the
	* first engine and set the pointers correctly. Also the first pointer
	* is not saved in a savegame, so this has to be fixed up after loading */

	/* Find the 'locomotive' or the first wagon in a chain */
	while ((u = GetPrevVehicleInChain_bruteforce(v)) != NULL) v = u;

	/* Set the first pointer of all vehicles in that chain to the first wagon */
	if (v->subtype == TS_Front_Engine)
		for (u = (Vehicle *)v; u != NULL; u = u->next) u->first = (Vehicle *)v;

	return (Vehicle*)v;
}

int CountVehiclesInChain(Vehicle *v)
{
	int count = 0;
	do count++; while ( (v=v->next) != NULL);
	return count;
}

void DeleteVehicle(Vehicle *v)
{
	DeleteName(v->string_id);
	v->type = 0;
	UpdateVehiclePosHash(v, INVALID_COORD, 0);
	v->next_hash = INVALID_VEHICLE;

	if (v->orders != NULL)
		DeleteVehicleOrders(v);
}

void DeleteVehicleChain(Vehicle *v)
{
	do {
		Vehicle *u = v;
		v = v->next;
		DeleteVehicle(u);
	} while (v != NULL);
}


void Aircraft_Tick(Vehicle *v);
void RoadVeh_Tick(Vehicle *v);
void Ship_Tick(Vehicle *v);
void Train_Tick(Vehicle *v);
static void EffectVehicle_Tick(Vehicle *v);
void DisasterVehicle_Tick(Vehicle *v);

VehicleTickProc *_vehicle_tick_procs[] = {
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

	FOR_ALL_VEHICLES(v) {
		if (v->type != 0)
			_vehicle_tick_procs[v->type - 0x10](v);
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

/** Check if a given vehicle (type) can be refitted to a given cargo
 * @param *v vehicle to check
 * @param cid_to check refit to this cargo-type
 * @return true if it is possible, false otherwise
 */
bool CanRefitTo(const Vehicle *v, CargoID cid_to)
{
	CargoID cid = _global_cargo_id[_opt_ptr->landscape][cid_to];

	if (cid == GC_INVALID) return false;

	if (_engine_refit_masks[v->engine_type]) {
		if (!HASBIT(_engine_refit_masks[v->engine_type], cid)) return false;
	} else {
		/* If we are talking about normal vehicles (no newgrf), you can only refit engines */
		if (v->type == VEH_Train && (RailVehInfo(v->engine_type)->flags & RVI_WAGON)) return false;
		if (!HASBIT(_default_refitmasks[v->type - VEH_Train], cid)) return false;
	}

	return true;
}

static void DoDrawVehicle(Vehicle *v)
{
	uint32 image = v->cur_image;

	if (v->vehstatus & VS_DISASTER) {
		image |= 0x3224000;
	} else if (v->vehstatus & VS_DEFPAL) {
		image |= (v->vehstatus & VS_CRASHED) ? PALETTE_CRASH : SPRITE_PALETTE(PLAYER_SPRITE_COLOR(v->owner));
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

	for(;;) {
		xb = x;
		for(;;) {
			veh = _vehicle_position_hash[ (x+y)&0xFFFF ];
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
	v->cur_image = SPR_CHIMNEY_SMOKE_0 + (r & 7);
	v->progress = (r >> 16) & 7;
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
			v->spritenum = (InteractiveRandom() & 3) + 1;
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
		if (IsTileType(tile, MP_INDUSTRY) &&
				_map5[tile] == 0xA2) {
			AddAnimatedTile(tile);
		}
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

Vehicle *CheckClickOnVehicle(ViewPort *vp, int x, int y)
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

	if (v->breakdown_ctr != 0 || (v->vehstatus & VS_STOPPED) != 0 ||
			v->cur_speed < 5 || _game_mode == GM_MENU)
				return;

	r = Random();

	/* increase chance of failure */
	chance = v->breakdown_chance + 1;
	if (CHANCE16I(1,25,r)) chance += 25;
	v->breakdown_chance = min(255, chance);

	/* calculate reliability value to use in comparison */
	rel = v->reliability;
	if (v->type == VEH_Ship) rel += 0x6666;

	/* disabled breakdowns? */
	if (_opt.diff.vehicle_breakdowns < 1)
		return;

	/* reduced breakdowns? */
	if (_opt.diff.vehicle_breakdowns == 1) rel += 0x6666;

	/* check if to break down */
	if (_breakdown_chance[(uint)min(rel, 0xffff) >> 10] <= v->breakdown_chance) {
		v->breakdown_ctr = (byte)(((r >> 16) & 0x3F) + 0x3F);
		v->breakdown_delay = (byte)(((r >> 24) & 0x7F) | 0x80);
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
	if (v->owner != _local_player)
		return;

	// Do not show getting-old message if autorenew is active
	if (_patches.autorenew)
		return;

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

extern int32 EstimateTrainCost(const RailVehicleInfo *rvi);
extern int32 EstimateRoadVehCost(EngineID engine_type);
extern int32 EstimateShipCost(EngineID engine_type);
extern int32 EstimateAircraftCost(EngineID engine_type);
extern int32 CmdRefitRailVehicle(int x, int y, uint32 flags, uint32 p1, uint32 p2);
extern int32 CmdRefitShip(int x, int y, uint32 flags, uint32 p1, uint32 p2);
extern int32 CmdRefitAircraft(int x, int y, uint32 flags, uint32 p1, uint32 p2);

/** Replaces a vehicle (used to be called autorenew).
 * @param x,y unused
 * @param p1 index of vehicle being replaced
 * @param p2 various bitstuffed elements
 * - p2 = (bit  0-15) - new engine type for the vehicle (p2 & 0xFFFF)
 * - p2 = (bit 16-31) - money the player wants to have left after replacement counted in 100.000 (100K) (p2 >> 16)
 */
int32 CmdReplaceVehicle(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	/* makesvariables to inform about how much money the player wants to have left after replacing
	 and which engine to replace with out of p2.
	 the first 16 bit is the money. The last 5 digits (all 0) were removed when sent, so we add them again.
	 This way the max is 6553 millions and it is more than the 32 bit that is stored in _patches
	 This is a nice way to send 32 bit and only use 16 bit
	 the last 8 bit is the engine. The 8 bits in front of the engine is free so it have room for 16 bit engine entries */
	EngineID new_engine_type = (p2 & 0xFFFF);
	uint32 autorefit_money = (p2 >> 16) * 100000;
	Vehicle *v, *u, *first;
	int cost, build_cost, rear_engine_cost = 0;
	EngineID old_engine_type;

	if (!IsVehicleIndex(p1)) return CMD_ERROR;

	v = u = GetVehicle(p1);

	old_engine_type = v->engine_type;

	/* First we make sure that it's a valid type the user requested
	 * check that it's an engine that is in the engine array */
	if (!IsEngineIndex(new_engine_type)) return CMD_ERROR;

	// check that the new vehicle type is the same as the original one
	if (v->type != GetEngine(new_engine_type)->type) return CMD_ERROR;

	// check that it's the vehicle's owner that requested the replace
	if (!CheckOwnership(v->owner)) return CMD_ERROR;

	// makes sure that we do not replace a plane with a helicopter or vise versa
	if (v->type == VEH_Aircraft) {
		if (HASBIT(AircraftVehInfo(old_engine_type)->subtype, 0) != HASBIT(AircraftVehInfo(new_engine_type)->subtype, 0)) return CMD_ERROR;
	}

	// makes sure that the player can actually buy the new engine. Renewing is still allowed to outdated engines
	if (!HASBIT(GetEngine(new_engine_type)->player_avail, v->owner) && old_engine_type != new_engine_type) return CMD_ERROR;

	switch (v->type) {
		case VEH_Train:    build_cost = EstimateTrainCost(RailVehInfo(new_engine_type)); break;
		case VEH_Road:     build_cost = EstimateRoadVehCost(new_engine_type);            break;
		case VEH_Ship:     build_cost = EstimateShipCost(new_engine_type);               break;
		case VEH_Aircraft: build_cost = EstimateAircraftCost(new_engine_type);           break;
		default: return CMD_ERROR;
	}

	/* In a rare situation, when 2 clients are connected to 1 company and have the same
	    settings, a vehicle can be replaced twice.. check if this is the situation here */
	if (old_engine_type == new_engine_type && v->age == 0) return CMD_ERROR;

	if ( v->type == VEH_Train ) {
		first = GetFirstVehicleInChain(v);
		u = GetLastVehicleInChain(v);
		if ( RailVehInfo(new_engine_type)->flags & RVI_MULTIHEAD )
			build_cost = build_cost >> 1;   //multiheaded engines have EstimateTrainCost() for both engines

		if ( old_engine_type != new_engine_type ) {

			// prevent that the rear engine can get replaced to something else than the front engine
			if ( v->u.rail.first_engine != INVALID_VEHICLE && RailVehInfo(old_engine_type)->flags & RVI_MULTIHEAD && RailVehInfo(old_engine_type)->flags ) {
				if ( first->engine_type != new_engine_type ) return CMD_ERROR;
			}

			// checks if the engine is the first one
			if ( v->u.rail.first_engine == INVALID_VEHICLE ) {
				if ( RailVehInfo(new_engine_type)->flags & RVI_MULTIHEAD ) {
					if ( u->engine_type == old_engine_type && v->next != NULL) {
						rear_engine_cost = build_cost - u->value;
					} else {
						rear_engine_cost = build_cost;
					}
				} else {
					if ( u->engine_type == old_engine_type && RailVehInfo(old_engine_type)->flags & RVI_MULTIHEAD) {
						if (v->next != NULL) rear_engine_cost = -(int32)u->value;
					}
				}
			 }
		}
	}

	/* Check if there is money for the upgrade.. if not, give a nice news-item
	    (that is needed, because this CMD is called automaticly) */
	if ( GetPlayer(v->owner)->money64 < (int32)(autorefit_money + build_cost + rear_engine_cost - v->value)) {
		if (( _local_player == v->owner ) && ( v->unitnumber != 0 )) {  //v->unitnumber = 0 for train cars
			int message;
			SetDParam(0, v->unitnumber);
			switch (v->type) {
				case VEH_Train:    message = STR_TRAIN_AUTORENEW_FAILED;       break;
				case VEH_Road:     message = STR_ROADVEHICLE_AUTORENEW_FAILED; break;
				case VEH_Ship:     message = STR_SHIP_AUTORENEW_FAILED;        break;
				case VEH_Aircraft: message = STR_AIRCRAFT_AUTORENEW_FAILED;    break;
				// This should never happen
				default: message = 0; break;
			}

			AddNewsItem(message, NEWS_FLAGS(NM_SMALL, NF_VIEWPORT|NF_VEHICLE, NT_ADVICE, 0), v->index, 0);
		}

		return CMD_ERROR;
	}
	cost = build_cost - v->value + rear_engine_cost;


	if (flags & DC_EXEC) {
		/* We do not really buy a new vehicle, we upgrade the old one */
		const Engine* e = GetEngine(new_engine_type);

		v->reliability = e->reliability;
		v->reliability_spd_dec = e->reliability_spd_dec;
		v->age = 0;

		v->date_of_last_service = _date;
		v->build_year = _cur_year;

		v->value = build_cost;

		if (v->engine_type != new_engine_type) {
			byte sprite = v->spritenum;
			byte cargo_type = v->cargo_type;
			v->engine_type = new_engine_type;
			v->max_age = e->lifelength * 366;

			/* Update limits of the vehicle (for when upgraded) */
			switch (v->type) {
			case VEH_Train:
				{
				const RailVehicleInfo *rvi = RailVehInfo(new_engine_type);
				const RailVehicleInfo *rvi2 = RailVehInfo(old_engine_type);
				byte capacity = rvi2->capacity;
				Vehicle *first = GetFirstVehicleInChain(v);

				//if (v->owner == _local_player) InvalidateWindowClasses(WC_TRAINS_LIST);
				/* rvi->image_index is the new sprite for the engine. Adding +1 makes the engine head the other way
				if it is a multiheaded engine (rear engine)
				(rvi->flags & RVI_MULTIHEAD && sprite - rvi2->image_index) is true if the engine is heading the other way, otherwise 0*/
				v->spritenum = rvi->image_index + (( rvi->flags & RVI_MULTIHEAD && sprite - rvi2->image_index) ? 1 : 0);

				// turn the last engine in a multiheaded train if needed
				if ( v->next == NULL && v->u.rail.first_engine != INVALID_VEHICLE && rvi->flags & RVI_MULTIHEAD && v->spritenum == rvi->image_index )
					v->spritenum++;

				v->cargo_type = rvi->cargo_type;
				v->cargo_cap = rvi->capacity;
				v->max_speed = rvi->max_speed;

				v->u.rail.railtype = e->railtype;

				// 0x0100 means that we skip the check for being stopped inside the depot
				// since we do not stop it for autorefitting
				if (v->cargo_type != cargo_type && capacity) {
					// BUG: somehow v->index is not transfered properly
					//CmdRefitRailVehicle(v->x_pos, v->y_pos, DC_EXEC, v->index , cargo_type + 0x0100 );
					v->cargo_type = cargo_type; // workaround, but it do not check the refit table
				} else {
					v->cargo_type = rvi->cargo_type;
				}
#if 0
// we disable this because they can crash the game. They will be fixed at a later date
				if ( rvi2->flags & RVI_MULTIHEAD && !(rvi->flags & RVI_MULTIHEAD) &&  v->index == first->index) {
					if (old_engine_type == u->engine_type ) {
						Vehicle *w;

						u = GetLastVehicleInChain(v);
						w = GetPrevVehicleInChain(u);
						w->next = NULL;
						DeleteVehicle(u);
					}
				}

				if ( rvi->flags & RVI_MULTIHEAD && rvi2->flags & RVI_MULTIHEAD &&  v->index == first->index ) {
					CmdReplaceVehicle(x, y, flags, u->index, p2);
				}

				if ( rvi->flags & RVI_MULTIHEAD && !(rvi2->flags & RVI_MULTIHEAD) &&  v->index == first->index ) {
					if ( old_engine_type != u->engine_type ) {
						Vehicle *w;
						if ( (w=AllocateVehicle()) != NULL ) {
							AddRearEngineToMultiheadedTrain(v,w, false);
							u->next = w;
						}
					}
				}
#endif

				// recalculate changed train values
				TrainConsistChanged(first);
				InvalidateWindowClasses(WC_TRAINS_LIST);
				UpdateTrainAcceleration(first);
				break;
				}
			case VEH_Road:
				{
				const RoadVehicleInfo *rvi = RoadVehInfo(new_engine_type);

				v->spritenum = rvi->image_index;
				v->cargo_type = rvi->cargo_type;
				v->cargo_cap = rvi->capacity;
				v->max_speed = rvi->max_speed;
				InvalidateWindowClasses(WC_ROADVEH_LIST);
				break;
				}
			case VEH_Ship:
				{
				const ShipVehicleInfo *svi = ShipVehInfo(new_engine_type);

				v->spritenum = svi->image_index;
				v->cargo_type = svi->cargo_type;
				v->cargo_cap = svi->capacity;
				v->max_speed = svi->max_speed;

				// 0x0100 means that we skip the check for being stopped inside the depot
				// since we do not stop it for autorefitting
				if (v->cargo_type != cargo_type)
					CmdRefitShip(v->x_pos, v->y_pos, DC_EXEC, v->index , cargo_type + 0x0100 );
				InvalidateWindowClasses(WC_SHIPS_LIST);
				break;
				}
			case VEH_Aircraft:
				{
				const AircraftVehicleInfo *avi = AircraftVehInfo(new_engine_type);
				Vehicle *u;

				v->max_speed = avi->max_speed;
				v->acceleration = avi->acceleration;
				v->spritenum = avi->image_index;

					if ( cargo_type == CT_PASSENGERS ) {
						v->cargo_cap = avi->passenger_capacity;
						u = v->next;
						u->cargo_cap = avi->mail_capacity;
					} else {
						// 0x0100 means that we skip the check for being stopped inside the hangar
						// since we do not stop it for autorefitting
						CmdRefitAircraft(v->x_pos, v->y_pos, DC_EXEC, v->index , cargo_type + 0x0100 );
					}
				InvalidateWindowClasses(WC_AIRCRAFT_LIST);
				break;
				}
			default: return CMD_ERROR;
			}
			// makes sure that the cargo is still valid compared to new capacity
			if (v->cargo_count != 0) {
				if ( v->cargo_type != cargo_type )
					v->cargo_count = 0;
				else if ( v->cargo_count > v->cargo_cap )
					v->cargo_count = v->cargo_cap;
			}
		}
		InvalidateWindow(WC_REPLACE_VEHICLE, v->type);
		ResortVehicleLists();
		InvalidateWindow(WC_VEHICLE_DETAILS, v->index);
	}
	//needs to be down here because refitting will change SET_EXPENSES_TYPE if called
	SET_EXPENSES_TYPE(EXPENSES_NEW_VEHICLES);

	return cost;
}

void MaybeReplaceVehicle(Vehicle *v)
{
	uint32 new_engine_and_autoreplace_money;

	if (v->owner != _local_player)
		return;
	// uncomment next line if you want to see what engine type just entered a depot
	//printf("engine type: %d\n", v->engine_type);

	// A vehicle is autorenewed when it it gets the amount of months
	//  give by _patches.autorenew_months away for his max age.
	//  Standard is -6, meaning 6 months before his max age
	//  It can be any value between -12 and 12.
	//  Here it also checks if the vehicles is listed for replacement
	if (!_patches.autorenew || v->age - v->max_age < (_patches.autorenew_months * 30)) {  //replace if engine is too old
		if (_autoreplace_array[v->engine_type] == v->engine_type && v->type != VEH_Train) //updates to a new model
			return;
	}
	/* Now replace the vehicle */
	_current_player = v->owner;

	/* makes the variable to inform about how much money the player wants to have left after replacing
	 and which engine to replace with
	 the first 16 bit is the money. Since we know the last 5 digits is 0, they are thrown away.
	 This way the max is 6553 millions and it is more than the 32 bit that is stored in _patches
	 This is a nice way to send 32 bit and only use 16 bit
	 the last 8 bit is the engine. The 8 bits in front of the engine is free so it have room for 16 bit engine entries */
	new_engine_and_autoreplace_money = ((_patches.autorenew_money / 100000) << 16) + _autoreplace_array[v->engine_type];

	assert(v->type == GetEngine(_autoreplace_array[v->engine_type])->type);

	if ( v->type != VEH_Train ) {
		DoCommandP(v->tile, v->index, new_engine_and_autoreplace_money, NULL, CMD_REPLACE_VEHICLE | CMD_SHOW_NO_ERROR);
	} else {
	// checks if any of the engines in the train are either old or listed for replacement
		do {
			if ( v->engine_type != _autoreplace_array[v->engine_type] || (_patches.autorenew && (v->age - v->max_age) > (_patches.autorenew_months * 30))) {
				new_engine_and_autoreplace_money = (new_engine_and_autoreplace_money & 0xFFFF0000) + _autoreplace_array[v->engine_type]; // sets the new engine replacement type
				DoCommandP(v->tile, v->index, new_engine_and_autoreplace_money, NULL, CMD_REPLACE_VEHICLE | CMD_SHOW_NO_ERROR);
			}
		} while ((v=v->next) != NULL);
	}
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
bool GetNewVehiclePos(Vehicle *v, GetNewVehiclePosResult *gp)
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

byte GetDirectionTowards(Vehicle *v, int x, int y)
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

	switch(v->type)
	{
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
	UnitID unit_num = 0;
	Vehicle *u;

restart:
	unit_num++;
	FOR_ALL_VEHICLES(u) {
		if (u->type == type && u->owner == _current_player &&
		    unit_num == u->unitnumber)
					goto restart;
	}
	return unit_num;
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
	SLE_CONDARR(NullStruct,null,SLE_FILE_U8 | SLE_VAR_NULL, 2, 2, 255), /* 2 */
	SLE_CONDARR(NullStruct,null,SLE_FILE_U32 | SLE_VAR_NULL, 2, 2, 255), /* 8 */

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

	// reserve extra space in savegame here. (currently 13 bytes)
	SLE_CONDARR(NullStruct,null,SLE_FILE_U8 | SLE_VAR_NULL, 13, 2, 255),

	SLE_END()
};

static const SaveLoad _roadveh_desc[] = {
	SLE_WRITEBYTE(Vehicle,type,VEH_Road, 1), // Road type. VEH_Road in mem, 1 in file.
	SLE_INCLUDEX(0, INC_VEHICLE_COMMON),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleRoad,state),					SLE_UINT8),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleRoad,frame),					SLE_UINT8),
	SLE_VARX(offsetof(Vehicle,u)+offsetof(VehicleRoad,unk2),					SLE_UINT16),
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

		/* Old savegames used 'last_station_visited = 0xFF', should be 0xFFFF */
		if (_sl.version < 5 && v->last_station_visited == 0xFF)
			v->last_station_visited = 0xFFFF;

		if (_sl.version < 5) {
			/* Convert the current_order.type (which is a mix of type and flags, because
			    in those versions, they both were 4 bits big) to type and flags */
			v->current_order.flags = (v->current_order.type & 0xF0) >> 4;
			v->current_order.type  =  v->current_order.type & 0x0F;
		}
	}

	/* Check for shared order-lists (we now use pointers for that) */
	if (_sl.full_version < 0x502) {
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

	if (_sl.full_version < 0x0B01)
		_vehicle_id_ctr_day = 0;
}

const ChunkHandler _veh_chunk_handlers[] = {
	{ 'VEHS', Save_VEHS, Load_VEHS, CH_SPARSE_ARRAY | CH_LAST},
};
