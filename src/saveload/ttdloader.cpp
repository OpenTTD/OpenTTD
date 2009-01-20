/* $Id$ */

/** @file oldloader.cpp Loading of old TTD(patch) savegames. */

#include "../stdafx.h"
#include "../openttd.h"
#include "../station_map.h"
#include "../town.h"
#include "../industry.h"
#include "../company_func.h"
#include "../company_base.h"
#include "../aircraft.h"
#include "../roadveh.h"
#include "../ship.h"
#include "../train.h"
#include "../signs_base.h"
#include "../debug.h"
#include "../depot_base.h"
#include "../newgrf_config.h"
#include "../ai/ai.hpp"
#include "../zoom_func.h"
#include "../functions.h"
#include "../date_func.h"
#include "../vehicle_func.h"
#include "../variables.h"
#include "../strings_func.h"
#include "../effectvehicle_base.h"
#include "../string_func.h"
#include "../core/mem_func.hpp"
#include "../core/alloc_type.hpp"

#include "table/strings.h"

#include "oldloader.h"
#include "saveload_internal.h"


extern SavegameType _savegame_type;
extern uint32 _ttdp_version;
static bool   _read_ttdpatch_flags;

static uint8  *_old_map3;

void FixOldMapArray()
{
	/* _old_map3 is moved to _m::m3 and _m::m4 */
	for (TileIndex t = 0; t < OLD_MAP_SIZE; t++) {
		_m[t].m3 = _old_map3[t * 2];
		_m[t].m4 = _old_map3[t * 2 + 1];
	}

	for (TileIndex t = 0; t < OLD_MAP_SIZE; t++) {
		switch (GetTileType(t)) {
			case MP_STATION:
				_m[t].m4 = 0; // We do not understand this TTDP station mapping (yet)
				switch (_m[t].m5) {
					/* We have drive through stops at a totally different place */
					case 0x53: case 0x54: _m[t].m5 += 170 - 0x53; break; // Bus drive through
					case 0x57: case 0x58: _m[t].m5 += 168 - 0x57; break; // Truck drive through
					case 0x55: case 0x56: _m[t].m5 += 170 - 0x55; break; // Bus tram stop
					case 0x59: case 0x5A: _m[t].m5 += 168 - 0x59; break; // Truck tram stop
					default: break;
				}
				break;

			case MP_RAILWAY:
				/* We save presignals different from TTDPatch, convert them */
				if (GB(_m[t].m5, 6, 2) == 1) { // RAIL_TILE_SIGNALS
					/* This byte is always zero in TTD for this type of tile */
					if (_m[t].m4) { // Convert the presignals to our own format
						_m[t].m4 = (_m[t].m4 >> 1) & 7;
					}
				}
				/* TTDPatch stores PBS things in L6 and all elsewhere; so we'll just
				 * clear it for ourselves and let OTTD's rebuild PBS itself */
				_m[t].m4 &= 0xF; // Only keep the lower four bits; upper four is PBS
				break;

			case MP_WATER:
				/* if water class == 3, make river there */
				if (GB(_m[t].m3, 0, 2) == 3) {
					SetTileType(t, MP_WATER);
					SetTileOwner(t, OWNER_WATER);
					_m[t].m2 = 0;
					_m[t].m3 = 2; // WATER_CLASS_RIVER
					_m[t].m4 = Random();
					_m[t].m5 = 0;
				}
				break;

			default:
				break;
		}
	}

	/* Some old TTD(Patch) savegames could have buoys at tile 0
	 * (without assigned station struct) */
	MemSetT(&_m[0], 0);
	SetTileType(0, MP_WATER);
	SetTileOwner(0, OWNER_WATER);
}

extern uint32 GetOldTownName(uint32 townnameparts, byte old_town_name_type);

void FixOldTowns()
{
	Town *town;

	/* Convert town-names if needed */
	FOR_ALL_TOWNS(town) {
		if (IsInsideMM(town->townnametype, 0x20C1, 0x20C3)) {
			town->townnametype = SPECSTR_TOWNNAME_ENGLISH + _settings_game.game_creation.town_name;
			town->townnameparts = GetOldTownName(town->townnameparts, _settings_game.game_creation.town_name);
		}
	}
}

StringID *_old_vehicle_names = NULL;

void FixOldVehicles()
{
	Vehicle *v;

	FOR_ALL_VEHICLES(v) {
		/* For some reason we need to correct for this */
		switch (v->spritenum) {
			case 0xfd: break;
			case 0xff: v->spritenum = 0xfe; break;
			default:   v->spritenum >>= 1; break;
		}

		/* Vehicle-subtype is different in TTD(Patch) */
		if (v->type == VEH_EFFECT) v->subtype = v->subtype >> 1;

		v->name = CopyFromOldName(_old_vehicle_names[v->index]);

		/* We haven't used this bit for stations for ages */
		if (v->type == VEH_ROAD &&
				v->u.road.state != RVSB_IN_DEPOT &&
				v->u.road.state != RVSB_WORMHOLE) {
			ClrBit(v->u.road.state, RVS_IS_STOPPING);
		}

		/* The subtype should be 0, but it sometimes isn't :( */
		if (v->type == VEH_ROAD || v->type == VEH_SHIP) v->subtype = 0;

		/* Sometimes primary vehicles would have a nothing (invalid) order
		 * or vehicles that could not have an order would still have a
		 * (loading) order which causes assertions and the like later on.
		 */
		if (!IsCompanyBuildableVehicleType(v) ||
				(v->IsPrimaryVehicle() && v->current_order.IsType(OT_NOTHING))) {
			v->current_order.MakeDummy();
		}

		/* Shared orders are fixed in AfterLoadVehicles now */
	}
}


/* The savegames has some hard-coded pointers, because it always enters the same
 *  piece of memory.. we don't.. so we need to remap ;)
 * Old Towns are 94 bytes big
 * Old Orders are 2 bytes big */
#define REMAP_TOWN_IDX(x) ((x) - (0x0459154 - 0x0458EF0)) / 94
#define REMAP_ORDER_IDX(x) ((x) - (0x045AB08 - 0x0458EF0)) / 2

extern TileIndex *_animated_tile_list;
extern uint _animated_tile_count;
extern char *_old_name_array;

static byte   _old_vehicle_multiplier;
static uint32 _old_town_index;
static uint16 _old_string_id;
static uint16 _old_string_id_2;
static uint16 _old_extra_chunk_nums;

static void ReadTTDPatchFlags()
{
	if (_read_ttdpatch_flags) return;

	_read_ttdpatch_flags = true;

	/* TTDPatch misuses _old_map3 for flags.. read them! */
	_old_vehicle_multiplier = _old_map3[0];
	/* Somehow.... there was an error in some savegames, so 0 becomes 1
	and 1 becomes 2. The rest of the values are okay */
	if (_old_vehicle_multiplier < 2) _old_vehicle_multiplier++;

	_old_vehicle_names = MallocT<StringID>(_old_vehicle_multiplier * 850);

	/* TTDPatch increases the Vehicle-part in the middle of the game,
	so if the multipler is anything else but 1, the assert fails..
	bump the assert value so it doesn't!
	(1 multipler == 850 vehicles
	1 vehicle   == 128 bytes */
	_bump_assert_value = (_old_vehicle_multiplier - 1) * 850 * 128;

	for (uint i = 0; i < 17; i++) { // check tile 0, too
		if (_old_map3[i] != 0) _savegame_type = SGT_TTDP1;
	}

	/* Check if we have a modern TTDPatch savegame (has extra data all around) */
	if (memcmp(&_old_map3[0x1FFFA], "TTDp", 4) == 0) _savegame_type = SGT_TTDP2;

	_old_extra_chunk_nums = _old_map3[_savegame_type == SGT_TTDP2 ? 0x1FFFE : 0x2];

	/* Clean the misused places */
	for (uint i = 0;       i < 17;      i++) _old_map3[i] = 0;
	for (uint i = 0x1FE00; i < 0x20000; i++) _old_map3[i] = 0;

	if (_savegame_type == SGT_TTDP2) DEBUG(oldloader, 2, "Found TTDPatch game");

	DEBUG(oldloader, 3, "Vehicle-multiplier is set to %d (%d vehicles)", _old_vehicle_multiplier, _old_vehicle_multiplier * 850);
}

static const OldChunks town_chunk[] = {
	OCL_SVAR(   OC_TILE, Town, xy ),
	OCL_NULL( 2 ),         ///< population,        no longer in use
	OCL_SVAR( OC_UINT16, Town, townnametype ),
	OCL_SVAR( OC_UINT32, Town, townnameparts ),
	OCL_SVAR(  OC_UINT8, Town, grow_counter ),
	OCL_NULL( 1 ),         ///< sort_index,        no longer in use
	OCL_NULL( 4 ),         ///< sign-coordinates,  no longer in use
	OCL_NULL( 2 ),         ///< namewidth,         no longer in use
	OCL_SVAR( OC_UINT16, Town, flags12 ),
	OCL_NULL( 10 ),        ///< radius,            no longer in use

	OCL_SVAR( OC_UINT16, Town, ratings[0] ),
	OCL_SVAR( OC_UINT16, Town, ratings[1] ),
	OCL_SVAR( OC_UINT16, Town, ratings[2] ),
	OCL_SVAR( OC_UINT16, Town, ratings[3] ),
	OCL_SVAR( OC_UINT16, Town, ratings[4] ),
	OCL_SVAR( OC_UINT16, Town, ratings[5] ),
	OCL_SVAR( OC_UINT16, Town, ratings[6] ),
	OCL_SVAR( OC_UINT16, Town, ratings[7] ),

	OCL_SVAR( OC_FILE_U32 | OC_VAR_U16, Town, have_ratings ),
	OCL_SVAR( OC_FILE_U32 | OC_VAR_U16, Town, statues ),
	OCL_NULL( 2 ),         ///< num_houses,        no longer in use
	OCL_SVAR(  OC_UINT8, Town, time_until_rebuild ),
	OCL_SVAR(  OC_UINT8, Town, growth_rate ),

	OCL_SVAR( OC_UINT16, Town, new_max_pass ),
	OCL_SVAR( OC_UINT16, Town, new_max_mail ),
	OCL_SVAR( OC_UINT16, Town, new_act_pass ),
	OCL_SVAR( OC_UINT16, Town, new_act_mail ),
	OCL_SVAR( OC_UINT16, Town, max_pass ),
	OCL_SVAR( OC_UINT16, Town, max_mail ),
	OCL_SVAR( OC_UINT16, Town, act_pass ),
	OCL_SVAR( OC_UINT16, Town, act_mail ),

	OCL_SVAR(  OC_UINT8, Town, pct_pass_transported ),
	OCL_SVAR(  OC_UINT8, Town, pct_mail_transported ),

	OCL_SVAR( OC_UINT16, Town, new_act_food ),
	OCL_SVAR( OC_UINT16, Town, new_act_water ),
	OCL_SVAR( OC_UINT16, Town, act_food ),
	OCL_SVAR( OC_UINT16, Town, act_water ),

	OCL_SVAR(  OC_UINT8, Town, road_build_months ),
	OCL_SVAR(  OC_UINT8, Town, fund_buildings_months ),

	OCL_NULL( 8 ),         ///< some junk at the end of the record

	OCL_END()
};
static bool LoadOldTown(LoadgameState *ls, int num)
{
	Town *t = new (num) Town();
	if (!LoadChunk(ls, t, town_chunk)) return false;

	if (t->xy == 0) t->xy = INVALID_TILE;

	return true;
}

static uint16 _old_order;
static const OldChunks order_chunk[] = {
	OCL_VAR ( OC_UINT16,   1, &_old_order ),
	OCL_END()
};

static bool LoadOldOrder(LoadgameState *ls, int num)
{
	if (!LoadChunk(ls, NULL, order_chunk)) return false;

	new (num) Order(UnpackOldOrder(_old_order));

	/* Relink the orders to eachother (in TTD(Patch) the orders for one
	vehicle are behind eachother, with an invalid order (OT_NOTHING) as indication that
	it is the last order */
	if (num > 0 && GetOrder(num)->IsValid())
		GetOrder(num - 1)->next = GetOrder(num);

	return true;
}

static bool LoadOldAnimTileList(LoadgameState *ls, int num)
{
	/* This is sligthly hackish - we must load a chunk into an array whose
	 * address isn't static, but instead pointed to by _animated_tile_list.
	 * To achieve that, create an OldChunks list on the stack on the fly.
	 * The list cannot be static because the value of _animated_tile_list
	 * can change between calls. */

	const OldChunks anim_chunk[] = {
		OCL_VAR (   OC_TILE, 256, _animated_tile_list ),
		OCL_END ()
	};

	if (!LoadChunk(ls, NULL, anim_chunk)) return false;

	/* Update the animated tile counter by counting till the first zero in the array */
	for (_animated_tile_count = 0; _animated_tile_count < 256; _animated_tile_count++) {
		if (_animated_tile_list[_animated_tile_count] == 0) break;
	}

	return true;
}

static const OldChunks depot_chunk[] = {
	OCL_SVAR(   OC_TILE, Depot, xy ),
	OCL_VAR ( OC_UINT32,   1, &_old_town_index ),
	OCL_END()
};

static bool LoadOldDepot(LoadgameState *ls, int num)
{
	Depot *d = new (num) Depot();
	if (!LoadChunk(ls, d, depot_chunk)) return false;

	if (d->xy != 0) {
		GetDepot(num)->town_index = REMAP_TOWN_IDX(_old_town_index);
	} else {
		d->xy = INVALID_TILE;
	}

	return true;
}

static int32 _old_price;
static uint16 _old_price_frac;
static const OldChunks price_chunk[] = {
	OCL_VAR (  OC_INT32,   1, &_old_price ),
	OCL_VAR ( OC_UINT16,   1, &_old_price_frac ),
	OCL_END()
};

static bool LoadOldPrice(LoadgameState *ls, int num)
{
	if (!LoadChunk(ls, NULL, price_chunk)) return false;

	/* We use a struct to store the prices, but they are ints in a row..
	so just access the struct as an array of int32's */
	((Money*)&_price)[num] = _old_price;
	_price_frac[num] = _old_price_frac;

	return true;
}

static const OldChunks cargo_payment_rate_chunk[] = {
	OCL_VAR (  OC_INT32,   1, &_old_price ),
	OCL_VAR ( OC_UINT16,   1, &_old_price_frac ),

	OCL_NULL( 2 ),         ///< Junk
	OCL_END()
};

static bool LoadOldCargoPaymentRate(LoadgameState *ls, int num)
{
	if (!LoadChunk(ls, NULL, cargo_payment_rate_chunk)) return false;

	_cargo_payment_rates[num] = -_old_price;
	_cargo_payment_rates_frac[num] = _old_price_frac;

	return true;
}

static uint   _current_station_id;
static uint16 _waiting_acceptance;
static uint8  _cargo_source;
static uint8  _cargo_days;

static const OldChunks goods_chunk[] = {
	OCL_VAR ( OC_UINT16, 1,          &_waiting_acceptance ),
	OCL_SVAR(  OC_UINT8, GoodsEntry, days_since_pickup ),
	OCL_SVAR(  OC_UINT8, GoodsEntry, rating ),
	OCL_VAR (  OC_UINT8, 1,          &_cargo_source ),
	OCL_VAR (  OC_UINT8, 1,          &_cargo_days ),
	OCL_SVAR(  OC_UINT8, GoodsEntry, last_speed ),
	OCL_SVAR(  OC_UINT8, GoodsEntry, last_age ),

	OCL_END()
};

static bool LoadOldGood(LoadgameState *ls, int num)
{
	Station *st = GetStation(_current_station_id);
	GoodsEntry *ge = &st->goods[num];
	bool ret = LoadChunk(ls, ge, goods_chunk);
	if (!ret) return false;

	SB(ge->acceptance_pickup, GoodsEntry::ACCEPTANCE, 1, HasBit(_waiting_acceptance, 15));
	SB(ge->acceptance_pickup, GoodsEntry::PICKUP, 1, _cargo_source != 0xFF);
	if (GB(_waiting_acceptance, 0, 12) != 0) {
		CargoPacket *cp = new CargoPacket();
		cp->source          = (_cargo_source == 0xFF) ? INVALID_STATION : _cargo_source;
		cp->count           = GB(_waiting_acceptance, 0, 12);
		cp->days_in_transit = _cargo_days;
		ge->cargo.Append(cp);
	}
	return ret;
}

static const OldChunks station_chunk[] = {
	OCL_SVAR(   OC_TILE, Station, xy ),
	OCL_VAR ( OC_UINT32,   1, &_old_town_index ),

	OCL_NULL( 4 ), ///< bus/lorry tile
	OCL_SVAR(   OC_TILE, Station, train_tile ),
	OCL_SVAR(   OC_TILE, Station, airport_tile ),
	OCL_SVAR(   OC_TILE, Station, dock_tile ),
	OCL_SVAR(  OC_UINT8, Station, trainst_w ),

	OCL_NULL( 1 ),         ///< sort-index, no longer in use
	OCL_NULL( 2 ),         ///< sign-width, no longer in use

	OCL_VAR ( OC_UINT16,   1, &_old_string_id ),

	OCL_NULL( 4 ),         ///< sign left/top, no longer in use

	OCL_SVAR( OC_UINT16, Station, had_vehicle_of_type ),

	OCL_CHUNK( 12, LoadOldGood ),

	OCL_SVAR(  OC_UINT8, Station, time_since_load ),
	OCL_SVAR(  OC_UINT8, Station, time_since_unload ),
	OCL_SVAR(  OC_UINT8, Station, delete_ctr ),
	OCL_SVAR(  OC_UINT8, Station, owner ),
	OCL_SVAR(  OC_UINT8, Station, facilities ),
	OCL_SVAR(  OC_UINT8, Station, airport_type ),
	/* Bus/truck status, no longer in use
	 * Blocked months
	 * Unknown
	 */
	OCL_NULL( 4 ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_U32, Station, airport_flags ),
	OCL_NULL( 2 ),         ///< last_vehicle. now last_vehicle_type

	OCL_NULL( 4 ),         ///< Junk at end of chunk

	OCL_END()
};
static bool LoadOldStation(LoadgameState *ls, int num)
{
	Station *st = new (num) Station();
	_current_station_id = num;

	if (!LoadChunk(ls, st, station_chunk))
		return false;

	if (st->xy != 0) {
		st->town    = GetTown(REMAP_TOWN_IDX(_old_town_index));
		st->string_id = RemapOldStringID(_old_string_id);
	} else {
		st->xy = INVALID_TILE;
	}

	return true;
}

static const OldChunks industry_chunk[] = {
	OCL_SVAR(   OC_TILE, Industry, xy ),
	OCL_VAR ( OC_UINT32,   1, &_old_town_index ),
	OCL_SVAR(  OC_UINT8, Industry, width ),
	OCL_SVAR(  OC_UINT8, Industry, height ),
	OCL_NULL( 2 ),  ///< used to be industry's produced_cargo

	OCL_SVAR( OC_UINT16, Industry, produced_cargo_waiting[0] ),
	OCL_SVAR( OC_UINT16, Industry, produced_cargo_waiting[1] ),

	OCL_SVAR(  OC_UINT8, Industry, production_rate[0] ),
	OCL_SVAR(  OC_UINT8, Industry, production_rate[1] ),

	OCL_NULL( 3 ),  ///< used to be industry's accepts_cargo

	OCL_SVAR(  OC_UINT8, Industry, prod_level ),

	OCL_SVAR( OC_UINT16, Industry, this_month_production[0] ),
	OCL_SVAR( OC_UINT16, Industry, this_month_production[1] ),
	OCL_SVAR( OC_UINT16, Industry, this_month_transported[0] ),
	OCL_SVAR( OC_UINT16, Industry, this_month_transported[1] ),

	OCL_SVAR(  OC_UINT8, Industry, last_month_pct_transported[0] ),
	OCL_SVAR(  OC_UINT8, Industry, last_month_pct_transported[1] ),

	OCL_SVAR( OC_UINT16, Industry, last_month_production[0] ),
	OCL_SVAR( OC_UINT16, Industry, last_month_production[1] ),
	OCL_SVAR( OC_UINT16, Industry, last_month_transported[0] ),
	OCL_SVAR( OC_UINT16, Industry, last_month_transported[1] ),

	OCL_SVAR(  OC_UINT8, Industry, type ),
	OCL_SVAR(  OC_UINT8, Industry, owner ),
	OCL_SVAR(  OC_UINT8, Industry, random_color ),
	OCL_SVAR( OC_FILE_U8 | OC_VAR_I32, Industry, last_prod_year ),
	OCL_SVAR( OC_UINT16, Industry, counter ),
	OCL_SVAR(  OC_UINT8, Industry, was_cargo_delivered ),

	OCL_NULL( 9 ), ///< Random junk at the end of this chunk

	OCL_END()
};

static bool LoadOldIndustry(LoadgameState *ls, int num)
{
	Industry *i = new (num) Industry();
	if (!LoadChunk(ls, i, industry_chunk)) return false;

	if (i->xy != 0) {
		i->town = GetTown(REMAP_TOWN_IDX(_old_town_index));
		IncIndustryTypeCount(i->type);
	} else {
		i->xy = INVALID_TILE;
	}

	return true;
}

static CompanyID _current_company_id;
static int32 _old_yearly;

static const OldChunks _company_yearly_chunk[] = {
	OCL_VAR(  OC_INT32,   1, &_old_yearly ),
	OCL_END()
};

static bool OldCompanyYearly(LoadgameState *ls, int num)
{
	int i;
	Company *c = GetCompany(_current_company_id);

	for (i = 0; i < 13; i++) {
		if (!LoadChunk(ls, NULL, _company_yearly_chunk)) return false;

		c->yearly_expenses[num][i] = _old_yearly;
	}

	return true;
}

static const OldChunks _company_economy_chunk[] = {
	OCL_SVAR( OC_FILE_I32 | OC_VAR_I64, CompanyEconomyEntry, income ),
	OCL_SVAR( OC_FILE_I32 | OC_VAR_I64, CompanyEconomyEntry, expenses ),
	OCL_SVAR( OC_INT32,                 CompanyEconomyEntry, delivered_cargo ),
	OCL_SVAR( OC_INT32,                 CompanyEconomyEntry, performance_history ),
	OCL_SVAR( OC_FILE_I32 | OC_VAR_I64, CompanyEconomyEntry, company_value ),

	OCL_END()
};

static bool OldCompanyEconomy(LoadgameState *ls, int num)
{
	int i;
	Company *c = GetCompany(_current_company_id);

	if (!LoadChunk(ls, &c->cur_economy, _company_economy_chunk)) return false;

	/* Don't ask, but the number in TTD(Patch) are inversed to OpenTTD */
	c->cur_economy.income   = -c->cur_economy.income;
	c->cur_economy.expenses = -c->cur_economy.expenses;

	for (i = 0; i < 24; i++) {
		if (!LoadChunk(ls, &c->old_economy[i], _company_economy_chunk)) return false;

		c->old_economy[i].income   = -c->old_economy[i].income;
		c->old_economy[i].expenses = -c->old_economy[i].expenses;
	}

	return true;
}

static const OldChunks _company_chunk[] = {
	OCL_VAR ( OC_UINT16,   1, &_old_string_id ),
	OCL_SVAR( OC_UINT32, Company, name_2 ),
	OCL_SVAR( OC_UINT32, Company, face ),
	OCL_VAR ( OC_UINT16,   1, &_old_string_id_2 ),
	OCL_SVAR( OC_UINT32, Company, president_name_2 ),

	OCL_SVAR( OC_FILE_I32 | OC_VAR_I64, Company, money ),
	OCL_SVAR( OC_FILE_I32 | OC_VAR_I64, Company, current_loan ),

	OCL_SVAR(  OC_UINT8, Company, colour ),
	OCL_SVAR(  OC_UINT8, Company, money_fraction ),
	OCL_SVAR(  OC_UINT8, Company, quarters_of_bankrupcy ),
	OCL_SVAR(  OC_UINT8, Company, bankrupt_asked ),
	OCL_SVAR( OC_FILE_U32 | OC_VAR_I64, Company, bankrupt_value ),
	OCL_SVAR( OC_UINT16, Company, bankrupt_timeout ),

	OCL_SVAR( OC_UINT32, Company, cargo_types ),

	OCL_CHUNK( 3, OldCompanyYearly ),
	OCL_CHUNK( 1, OldCompanyEconomy ),

	OCL_SVAR( OC_FILE_U16 | OC_VAR_I32, Company, inaugurated_year),
	OCL_SVAR(                  OC_TILE, Company, last_build_coordinate ),
	OCL_SVAR(                 OC_UINT8, Company, num_valid_stat_ent ),

	OCL_NULL( 230 ),         // Old AI

	OCL_SVAR(  OC_UINT8, Company, block_preview ),
	OCL_NULL( 1 ),           // Old AI
	OCL_SVAR(  OC_UINT8, Company, avail_railtypes ),
	OCL_SVAR(   OC_TILE, Company, location_of_HQ ),
	OCL_SVAR(  OC_UINT8, Company, share_owners[0] ),
	OCL_SVAR(  OC_UINT8, Company, share_owners[1] ),
	OCL_SVAR(  OC_UINT8, Company, share_owners[2] ),
	OCL_SVAR(  OC_UINT8, Company, share_owners[3] ),

	OCL_NULL( 8 ), ///< junk at end of chunk

	OCL_END()
};

static bool LoadOldCompany(LoadgameState *ls, int num)
{
	Company *c = new (num) Company();

	_current_company_id = (CompanyID)num;

	if (!LoadChunk(ls, c, _company_chunk)) return false;

	if (_old_string_id == 0) {
		delete c;
		return true;
	}

	c->name_1 = RemapOldStringID(_old_string_id);
	c->president_name_1 = RemapOldStringID(_old_string_id_2);

	if (num == 0) {
		/* If the first company has no name, make sure we call it UNNAMED */
		if (c->name_1 == 0)
			c->name_1 = STR_SV_UNNAMED;
	} else {
		/* Beside some multiplayer maps (1 on 1), which we don't official support,
		 * all other companys are an AI.. mark them as such */
		c->is_ai = true;
	}

	/* Sometimes it is better to not ask.. in old scenarios, the money
	 * was always 893288 pounds. In the newer versions this is correct,
	 * but correct for those oldies
	 * Ps: this also means that if you had exact 893288 pounds, you will go back
	 * to 100000.. this is a very VERY small chance ;) */
	if (c->money == 893288) c->money = c->current_loan = 100000;

	_company_colours[num] = c->colour;
	c->inaugurated_year -= ORIGINAL_BASE_YEAR;

	return true;
}

static uint32 _old_order_ptr;
static uint16 _old_next_ptr;
static uint32 _current_vehicle_id;

static const OldChunks vehicle_train_chunk[] = {
	OCL_SVAR(  OC_UINT8, VehicleRail, track ),
	OCL_SVAR(  OC_UINT8, VehicleRail, force_proceed ),
	OCL_SVAR( OC_UINT16, VehicleRail, crash_anim_pos ),
	OCL_SVAR(  OC_UINT8, VehicleRail, railtype ),

	OCL_NULL( 5 ), ///< Junk

	OCL_END()
};

static const OldChunks vehicle_road_chunk[] = {
	OCL_SVAR(  OC_UINT8, VehicleRoad, state ),
	OCL_SVAR(  OC_UINT8, VehicleRoad, frame ),
	OCL_SVAR( OC_UINT16, VehicleRoad, blocked_ctr ),
	OCL_SVAR(  OC_UINT8, VehicleRoad, overtaking ),
	OCL_SVAR(  OC_UINT8, VehicleRoad, overtaking_ctr ),
	OCL_SVAR( OC_UINT16, VehicleRoad, crashed_ctr ),
	OCL_SVAR(  OC_UINT8, VehicleRoad, reverse_ctr ),

	OCL_NULL( 1 ), ///< Junk

	OCL_END()
};

static const OldChunks vehicle_ship_chunk[] = {
	OCL_SVAR(  OC_UINT8, VehicleShip, state ),

	OCL_NULL( 9 ), ///< Junk

	OCL_END()
};

static const OldChunks vehicle_air_chunk[] = {
	OCL_SVAR(  OC_UINT8, VehicleAir, pos ),
	OCL_SVAR(  OC_FILE_U8 | OC_VAR_U16, VehicleAir, targetairport ),
	OCL_SVAR( OC_UINT16, VehicleAir, crashed_counter ),
	OCL_SVAR(  OC_UINT8, VehicleAir, state ),

	OCL_NULL( 5 ), ///< Junk

	OCL_END()
};

static const OldChunks vehicle_effect_chunk[] = {
	OCL_SVAR( OC_UINT16, VehicleEffect, animation_state ),
	OCL_SVAR(  OC_UINT8, VehicleEffect, animation_substate ),

	OCL_NULL( 7 ), // Junk

	OCL_END()
};

static const OldChunks vehicle_disaster_chunk[] = {
	OCL_SVAR( OC_UINT16, VehicleDisaster, image_override ),
	OCL_SVAR( OC_UINT16, VehicleDisaster, big_ufo_destroyer_target ),

	OCL_NULL( 6 ), ///< Junk

	OCL_END()
};

static const OldChunks vehicle_empty_chunk[] = {
	OCL_NULL( 10 ), ///< Junk

	OCL_END()
};

static bool LoadOldVehicleUnion(LoadgameState *ls, int num)
{
	Vehicle *v = GetVehicle(_current_vehicle_id);
	uint temp = ls->total_read;
	bool res;

	switch (v->type) {
		default: NOT_REACHED();
		case VEH_INVALID : res = LoadChunk(ls, NULL,           vehicle_empty_chunk);    break;
		case VEH_TRAIN   : res = LoadChunk(ls, &v->u.rail,     vehicle_train_chunk);    break;
		case VEH_ROAD    : res = LoadChunk(ls, &v->u.road,     vehicle_road_chunk);     break;
		case VEH_SHIP    : res = LoadChunk(ls, &v->u.ship,     vehicle_ship_chunk);     break;
		case VEH_AIRCRAFT: res = LoadChunk(ls, &v->u.air,      vehicle_air_chunk);      break;
		case VEH_EFFECT  : res = LoadChunk(ls, &v->u.effect,   vehicle_effect_chunk);   break;
		case VEH_DISASTER: res = LoadChunk(ls, &v->u.disaster, vehicle_disaster_chunk); break;
	}

	/* This chunk size should always be 10 bytes */
	if (ls->total_read - temp != 10) {
		DEBUG(oldloader, 0, "Assert failed in VehicleUnion: invalid chunk size");
		return false;
	}

	return res;
}

static uint16 _cargo_count;

static const OldChunks vehicle_chunk[] = {
	OCL_SVAR(  OC_UINT8, Vehicle, subtype ),

	OCL_NULL( 2 ),         ///< Hash, calculated automatically
	OCL_NULL( 2 ),         ///< Index, calculated automatically

	OCL_VAR ( OC_UINT32,   1, &_old_order_ptr ),
	OCL_VAR ( OC_UINT16,   1, &_old_order ),

	OCL_NULL ( 1 ), ///< num_orders, now calculated
	OCL_SVAR(  OC_UINT8, Vehicle, cur_order_index ),
	OCL_SVAR(   OC_TILE, Vehicle, dest_tile ),
	OCL_SVAR( OC_UINT16, Vehicle, load_unload_time_rem ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_U32, Vehicle, date_of_last_service ),
	OCL_SVAR( OC_UINT16, Vehicle, service_interval ),
	OCL_SVAR( OC_FILE_U8 | OC_VAR_U16, Vehicle, last_station_visited ),
	OCL_SVAR(  OC_UINT8, Vehicle, tick_counter ),
	OCL_SVAR( OC_UINT16, Vehicle, max_speed ),

	OCL_SVAR( OC_FILE_U16 | OC_VAR_I32, Vehicle, x_pos ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_I32, Vehicle, y_pos ),
	OCL_SVAR(  OC_UINT8, Vehicle, z_pos ),
	OCL_SVAR(  OC_UINT8, Vehicle, direction ),
	OCL_NULL( 2 ),         ///< x_offs and y_offs, calculated automatically
	OCL_NULL( 2 ),         ///< x_extent and y_extent, calculated automatically
	OCL_NULL( 1 ),         ///< z_extent, calculated automatically

	OCL_SVAR(  OC_UINT8, Vehicle, owner ),
	OCL_SVAR(   OC_TILE, Vehicle, tile ),
	OCL_SVAR( OC_UINT16, Vehicle, cur_image ),

	OCL_NULL( 8 ),        ///< Vehicle sprite box, calculated automatically

	OCL_SVAR( OC_FILE_U16 | OC_VAR_U8, Vehicle, vehstatus ),
	OCL_SVAR( OC_UINT16, Vehicle, cur_speed ),
	OCL_SVAR(  OC_UINT8, Vehicle, subspeed ),
	OCL_SVAR(  OC_UINT8, Vehicle, acceleration ),
	OCL_SVAR(  OC_UINT8, Vehicle, progress ),

	OCL_SVAR(  OC_UINT8, Vehicle, cargo_type ),
	OCL_SVAR( OC_UINT16, Vehicle, cargo_cap ),
	OCL_VAR ( OC_UINT16, 1,       &_cargo_count ),
	OCL_VAR (  OC_UINT8, 1,       &_cargo_source ),
	OCL_VAR (  OC_UINT8, 1,       &_cargo_days ),

	OCL_SVAR( OC_FILE_U16 | OC_VAR_U32, Vehicle, age ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_U32, Vehicle, max_age ),
	OCL_SVAR( OC_FILE_U8 | OC_VAR_I32, Vehicle, build_year ),
	OCL_SVAR( OC_FILE_U8 | OC_VAR_U16, Vehicle, unitnumber ),

	OCL_SVAR( OC_UINT16, Vehicle, engine_type ),

	OCL_SVAR(  OC_UINT8, Vehicle, spritenum ),
	OCL_SVAR(  OC_UINT8, Vehicle, day_counter ),

	OCL_SVAR(  OC_UINT8, Vehicle, breakdowns_since_last_service ),
	OCL_SVAR(  OC_UINT8, Vehicle, breakdown_ctr ),
	OCL_SVAR(  OC_UINT8, Vehicle, breakdown_delay ),
	OCL_SVAR(  OC_UINT8, Vehicle, breakdown_chance ),

	OCL_SVAR( OC_UINT16, Vehicle, reliability ),
	OCL_SVAR( OC_UINT16, Vehicle, reliability_spd_dec ),

	OCL_SVAR( OC_FILE_I32 | OC_VAR_I64, Vehicle, profit_this_year ),
	OCL_SVAR( OC_FILE_I32 | OC_VAR_I64, Vehicle, profit_last_year ),

	OCL_VAR ( OC_UINT16,   1, &_old_next_ptr ),

	OCL_SVAR( OC_FILE_U32 | OC_VAR_I64, Vehicle, value ),

	OCL_VAR ( OC_UINT16,   1, &_old_string_id ),

	OCL_CHUNK( 1, LoadOldVehicleUnion ),

	OCL_NULL( 20 ), ///< Junk at end of struct (TTDPatch has some data in it)

	OCL_END()
};

bool LoadOldVehicle(LoadgameState *ls, int num)
{
	/* Read the TTDPatch flags, because we need some info from it */
	ReadTTDPatchFlags();

	for (uint i = 0; i < _old_vehicle_multiplier; i++) {
		_current_vehicle_id = num * _old_vehicle_multiplier + i;

		/* Read the vehicle type and allocate the right vehicle */
		Vehicle *v;
		switch (ReadByte(ls)) {
			default: NOT_REACHED();
			case 0x00 /*VEH_INVALID */: v = new (_current_vehicle_id) InvalidVehicle();  break;
			case 0x10 /*VEH_TRAIN   */: v = new (_current_vehicle_id) Train();           break;
			case 0x11 /*VEH_ROAD    */: v = new (_current_vehicle_id) RoadVehicle();     break;
			case 0x12 /*VEH_SHIP    */: v = new (_current_vehicle_id) Ship();            break;
			case 0x13 /*VEH_AIRCRAFT*/: v = new (_current_vehicle_id) Aircraft();        break;
			case 0x14 /*VEH_EFFECT  */: v = new (_current_vehicle_id) EffectVehicle();   break;
			case 0x15 /*VEH_DISASTER*/: v = new (_current_vehicle_id) DisasterVehicle(); break;
		}
		if (!LoadChunk(ls, v, vehicle_chunk)) return false;

		/* This should be consistent, else we have a big problem... */
		if (v->index != _current_vehicle_id) {
			DEBUG(oldloader, 0, "Loading failed - vehicle-array is invalid");
			return false;
		}

		if (_old_order_ptr != 0 && _old_order_ptr != 0xFFFFFFFF) {
			uint old_id = REMAP_ORDER_IDX(_old_order_ptr);
			/* There is a maximum of 5000 orders in old savegames, so *if*
			 * we go over that limit something is very wrong. In that case
			 * we just assume there are no orders for the vehicle.
			 */
			if (old_id < 5000) v->orders.old = GetOrder(old_id);
		}
		v->current_order.AssignOrder(UnpackOldOrder(_old_order));

		if (_old_next_ptr != 0xFFFF) v->next = GetVehiclePoolSize() <= _old_next_ptr ? new (_old_next_ptr) InvalidVehicle() : GetVehicle(_old_next_ptr);

		_old_vehicle_names[_current_vehicle_id] = RemapOldStringID(_old_string_id);

		if (_cargo_count != 0) {
			CargoPacket *cp = new CargoPacket((_cargo_source == 0xFF) ? INVALID_STATION : _cargo_source, _cargo_count);
			cp->days_in_transit = _cargo_days;
			v->cargo.Append(cp);
		}
	}

	return true;
}

static const OldChunks sign_chunk[] = {
	OCL_VAR ( OC_UINT16, 1, &_old_string_id ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_I32, Sign, x ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_I32, Sign, y ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_I8, Sign, z ),

	OCL_NULL( 6 ),         ///< Width of sign, no longer in use

	OCL_END()
};

static bool LoadOldSign(LoadgameState *ls, int num)
{
	Sign *si = new (num) Sign();
	if (!LoadChunk(ls, si, sign_chunk)) return false;

	if (_old_string_id != 0) {
		_old_string_id = RemapOldStringID(_old_string_id);
		si->name = CopyFromOldName(_old_string_id);
		si->owner = OWNER_NONE;
	}

	return true;
}

static const OldChunks engine_chunk[] = {
	OCL_SVAR( OC_UINT16, Engine, company_avail ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_U32, Engine, intro_date ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_U32, Engine, age ),
	OCL_SVAR( OC_UINT16, Engine, reliability ),
	OCL_SVAR( OC_UINT16, Engine, reliability_spd_dec ),
	OCL_SVAR( OC_UINT16, Engine, reliability_start ),
	OCL_SVAR( OC_UINT16, Engine, reliability_max ),
	OCL_SVAR( OC_UINT16, Engine, reliability_final ),
	OCL_SVAR( OC_UINT16, Engine, duration_phase_1 ),
	OCL_SVAR( OC_UINT16, Engine, duration_phase_2 ),
	OCL_SVAR( OC_UINT16, Engine, duration_phase_3 ),

	OCL_SVAR(  OC_UINT8, Engine, lifelength ),
	OCL_SVAR(  OC_UINT8, Engine, flags ),
	OCL_SVAR(  OC_UINT8, Engine, preview_company_rank ),
	OCL_SVAR(  OC_UINT8, Engine, preview_wait ),

	OCL_NULL( 2 ), ///< railtype + junk

	OCL_END()
};

static bool LoadOldEngine(LoadgameState *ls, int num)
{
	Engine *e = GetTempDataEngine(num);
	return LoadChunk(ls, e, engine_chunk);
}

static bool LoadOldEngineName(LoadgameState *ls, int num)
{
	Engine *e = GetTempDataEngine(num);
	e->name = CopyFromOldName(RemapOldStringID(ReadUint16(ls)));
	return true;
}

static const OldChunks subsidy_chunk[] = {
	OCL_SVAR(  OC_UINT8, Subsidy, cargo_type ),
	OCL_SVAR(  OC_UINT8, Subsidy, age ),
	OCL_SVAR(  OC_FILE_U8 | OC_VAR_U16, Subsidy, from ),
	OCL_SVAR(  OC_FILE_U8 | OC_VAR_U16, Subsidy, to ),

	OCL_END()
};

static inline bool LoadOldSubsidy(LoadgameState *ls, int num)
{
	return LoadChunk(ls, &_subsidies[num], subsidy_chunk);
}

static const OldChunks game_difficulty_chunk[] = {
	OCL_SVAR( OC_FILE_U16 |  OC_VAR_U8, DifficultySettings, max_no_competitors ),
	OCL_NULL( 2), // competitor_start_time
	OCL_SVAR( OC_FILE_U16 |  OC_VAR_U8, DifficultySettings, number_towns ),
	OCL_SVAR( OC_FILE_U16 |  OC_VAR_U8, DifficultySettings, number_industries ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_U32, DifficultySettings, max_loan ),
	OCL_SVAR( OC_FILE_U16 |  OC_VAR_U8, DifficultySettings, initial_interest ),
	OCL_SVAR( OC_FILE_U16 |  OC_VAR_U8, DifficultySettings, vehicle_costs ),
	OCL_SVAR( OC_FILE_U16 |  OC_VAR_U8, DifficultySettings, competitor_speed ),
	OCL_NULL( 2), // competitor_intelligence
	OCL_SVAR( OC_FILE_U16 |  OC_VAR_U8, DifficultySettings, vehicle_breakdowns ),
	OCL_SVAR( OC_FILE_U16 |  OC_VAR_U8, DifficultySettings, subsidy_multiplier ),
	OCL_SVAR( OC_FILE_U16 |  OC_VAR_U8, DifficultySettings, construction_cost ),
	OCL_SVAR( OC_FILE_U16 |  OC_VAR_U8, DifficultySettings, terrain_type ),
	OCL_SVAR( OC_FILE_U16 |  OC_VAR_U8, DifficultySettings, quantity_sea_lakes ),
	OCL_SVAR( OC_FILE_U16 |  OC_VAR_U8, DifficultySettings, economy ),
	OCL_SVAR( OC_FILE_U16 |  OC_VAR_U8, DifficultySettings, line_reverse_mode ),
	OCL_SVAR( OC_FILE_U16 |  OC_VAR_U8, DifficultySettings, disasters ),
	OCL_END()
};

static inline bool LoadOldGameDifficulty(LoadgameState *ls, int num)
{
	bool ret = LoadChunk(ls, &_settings_game.difficulty, game_difficulty_chunk);
	_settings_game.difficulty.max_loan *= 1000;
	return ret;
}


static bool LoadOldMapPart1(LoadgameState *ls, int num)
{
	uint i;

	for (i = 0; i < OLD_MAP_SIZE; i++) {
		_m[i].m1 = ReadByte(ls);
	}
	for (i = 0; i < OLD_MAP_SIZE; i++) {
		_m[i].m2 = ReadByte(ls);
	}
	for (i = 0; i < OLD_MAP_SIZE; i++) {
		_old_map3[i * 2] = ReadByte(ls);
		_old_map3[i * 2 + 1] = ReadByte(ls);
	}
	for (i = 0; i < OLD_MAP_SIZE / 4; i++) {
		byte b = ReadByte(ls);
		_m[i * 4 + 0].m6 = GB(b, 0, 2);
		_m[i * 4 + 1].m6 = GB(b, 2, 2);
		_m[i * 4 + 2].m6 = GB(b, 4, 2);
		_m[i * 4 + 3].m6 = GB(b, 6, 2);
	}

	return !ls->failed;
}

static bool LoadOldMapPart2(LoadgameState *ls, int num)
{
	uint i;

	for (i = 0; i < OLD_MAP_SIZE; i++) {
		_m[i].type_height = ReadByte(ls);
	}
	for (i = 0; i < OLD_MAP_SIZE; i++) {
		_m[i].m5 = ReadByte(ls);
	}

	return !ls->failed;
}

static bool LoadTTDPatchExtraChunks(LoadgameState *ls, int num)
{
	ReadTTDPatchFlags();

	DEBUG(oldloader, 2, "Found %d extra chunk(s)", _old_extra_chunk_nums);

	for (int i = 0; i != _old_extra_chunk_nums; i++) {
		uint16 id = ReadUint16(ls);
		uint32 len = ReadUint32(ls);

		switch (id) {
			/* List of GRFIDs, used in the savegame. 0x8004 is the new ID
			 * They are saved in a 'GRFID:4 active:1' format, 5 bytes for each entry */
			case 0x2:
			case 0x8004: {
				/* Skip the first element: TTDP hack for the Action D special variables (FFFF0000 01) */
				ReadUint32(ls); ReadByte(ls); len -= 5;

				ClearGRFConfigList(&_grfconfig);
				while (len != 0) {
					uint32 grfid = ReadUint32(ls);

					if (ReadByte(ls) == 1) {
						GRFConfig *c = CallocT<GRFConfig>(1);
						c->grfid = grfid;
						c->filename = strdup("TTDP game, no information");

						AppendToGRFConfigList(&_grfconfig, c);
						DEBUG(oldloader, 3, "TTDPatch game using GRF file with GRFID %0X", BSWAP32(c->grfid));
					}
					len -= 5;
				};

				/* Append static NewGRF configuration */
				AppendStaticGRFConfigs(&_grfconfig);
			} break;

			/* TTDPatch version and configuration */
			case 0x3:
				_ttdp_version = ReadUint32(ls);
				DEBUG(oldloader, 3, "Game saved with TTDPatch version %d.%d.%d r%d",
					GB(_ttdp_version, 24, 8), GB(_ttdp_version, 20, 4), GB(_ttdp_version, 16, 4), GB(_ttdp_version, 0, 16));
				len -= 4;
				while (len-- != 0) ReadByte(ls); // skip the configuration
				break;

			default:
				DEBUG(oldloader, 4, "Skipping unknown extra chunk %X", id);
				while (len-- != 0) ReadByte(ls);
				break;
		}
	}

	return !ls->failed;
}

extern TileIndex _cur_tileloop_tile;
static uint32 _old_cur_town_ctr;
static const OldChunks main_chunk[] = {
	OCL_ASSERT( 0 ),
	OCL_VAR ( OC_FILE_U16 | OC_VAR_U32, 1, &_date ),
	OCL_VAR ( OC_UINT16,   1, &_date_fract ),
	OCL_NULL( 600 ),            ///< TextEffects
	OCL_VAR ( OC_UINT32,   2, &_random.state ),

	OCL_ASSERT( 0x264 ),
	OCL_CHUNK(  70, LoadOldTown ),
	OCL_ASSERT( 0x1C18 ),
	OCL_CHUNK(5000, LoadOldOrder ),
	OCL_ASSERT( 0x4328 ),

	OCL_CHUNK( 1, LoadOldAnimTileList ),
	OCL_NULL( 4 ),              ///< old end-of-order-list-pointer, no longer in use

	OCL_CHUNK( 255, LoadOldDepot ),
	OCL_ASSERT( 0x4B26 ),

	OCL_VAR ( OC_UINT32,   1, &_old_cur_town_ctr ),
	OCL_NULL( 2 ),              ///< timer_counter, no longer in use
	OCL_NULL( 2 ),              ///< land_code,     no longer in use

	OCL_VAR ( OC_FILE_U16 | OC_VAR_U8, 1, &_age_cargo_skip_counter ),
	OCL_VAR ( OC_UINT16,   1, &_tick_counter ),
	OCL_VAR (   OC_TILE,   1, &_cur_tileloop_tile ),

	OCL_CHUNK( 49, LoadOldPrice ),
	OCL_CHUNK( 12, LoadOldCargoPaymentRate ),

	OCL_ASSERT( 0x4CBA ),

	OCL_CHUNK( 1, LoadOldMapPart1 ),

	OCL_ASSERT( 0x48CBA ),

	OCL_CHUNK(250, LoadOldStation ),
	OCL_CHUNK( 90, LoadOldIndustry ),
	OCL_CHUNK(  8, LoadOldCompany ),

	OCL_ASSERT( 0x547F2 ),

	OCL_CHUNK( 850, LoadOldVehicle ),

	OCL_ASSERT( 0x6F0F2 ),

	OCL_VAR (  OC_UINT8 | OC_DEREFERENCE_POINTER, 32 * 500, &_old_name_array ),

	OCL_NULL( 0x2000 ),            ///< Old hash-table, no longer in use

	OCL_CHUNK( 40, LoadOldSign ),
	OCL_CHUNK(256, LoadOldEngine ),

	OCL_VAR ( OC_UINT16,    1, &_vehicle_id_ctr_day ),

	OCL_CHUNK(  8, LoadOldSubsidy ),

	OCL_VAR ( OC_FILE_U16 | OC_VAR_U32,   1, &_next_competitor_start ),
	OCL_VAR ( OC_FILE_I16 | OC_VAR_I32,   1, &_saved_scrollpos_x ),
	OCL_VAR ( OC_FILE_I16 | OC_VAR_I32,   1, &_saved_scrollpos_y ),
	OCL_VAR ( OC_FILE_U16 | OC_VAR_U8,    1, &_saved_scrollpos_zoom ),

	OCL_VAR ( OC_FILE_U32 | OC_VAR_I64,   1, &_economy.max_loan ),
	OCL_VAR ( OC_FILE_U32 | OC_VAR_I64,   1, &_economy.max_loan_unround ),
	OCL_VAR (  OC_INT16,    1, &_economy.fluct ),

	OCL_VAR ( OC_UINT16,    1, &_disaster_delay ),

	OCL_NULL( 144 ),             ///< cargo-stuff, calculated in InitializeLandscapeVariables

	OCL_CHUNK(256, LoadOldEngineName ),

	OCL_NULL( 144 ),             ///< AI cargo-stuff, calculated in InitializeLandscapeVariables
	OCL_NULL( 2 ),               ///< Company indexes of companies, no longer in use

	OCL_VAR ( OC_FILE_U8 | OC_VAR_U16,    1, &_station_tick_ctr ),

	OCL_VAR (  OC_UINT8,    1, &_settings_game.locale.currency ),
	OCL_VAR (  OC_UINT8,    1, &_settings_game.locale.units ),
	OCL_VAR ( OC_FILE_U8 | OC_VAR_U32,    1, &_cur_company_tick_index ),

	OCL_NULL( 2 ),               ///< Date stuff, calculated automatically
	OCL_NULL( 8 ),               ///< Company colors, calculated automatically

	OCL_VAR (  OC_UINT8,    1, &_economy.infl_amount ),
	OCL_VAR (  OC_UINT8,    1, &_economy.infl_amount_pr ),
	OCL_VAR (  OC_UINT8,    1, &_economy.interest_rate ),
	OCL_NULL( 1 ), // available airports
	OCL_VAR (  OC_UINT8,    1, &_settings_game.vehicle.road_side ),
	OCL_VAR (  OC_UINT8,    1, &_settings_game.game_creation.town_name ),

	OCL_CHUNK( 1, LoadOldGameDifficulty ),

	OCL_ASSERT( 0x77130 ),

	OCL_VAR (  OC_UINT8,    1, &_settings_game.difficulty.diff_level ),
	OCL_VAR (  OC_UINT8,    1, &_settings_game.game_creation.landscape ),
	OCL_VAR (  OC_UINT8,    1, &_trees_tick_ctr ),

	OCL_NULL( 1 ),               ///< Custom vehicle types yes/no, no longer used
	OCL_VAR (  OC_UINT8,    1, &_settings_game.game_creation.snow_line ),

	OCL_NULL( 32 ),              ///< new_industry_randtable, no longer used (because of new design)
	OCL_NULL( 36 ),              ///< cargo-stuff, calculated in InitializeLandscapeVariables

	OCL_ASSERT( 0x77179 ),

	OCL_CHUNK( 1, LoadOldMapPart2 ),

	OCL_ASSERT( 0x97179 ),

	/* Below any (if available) extra chunks from TTDPatch can follow */
	OCL_CHUNK(1, LoadTTDPatchExtraChunks),

	OCL_END()
};

bool LoadOldMain(LoadgameState *ls)
{
	_savegame_type = SGT_TTD;
	_ttdp_version = 0;
	_read_ttdpatch_flags = false;

	DEBUG(oldloader, 3, "Reading main chunk...");
	/* Load the biggest chunk */
	SmallStackSafeStackAlloc<byte, OLD_MAP_SIZE * 2> map3;
	_old_map3 = map3.data;
	_old_vehicle_names = NULL;
	if (!LoadChunk(ls, NULL, main_chunk)) {
		DEBUG(oldloader, 0, "Loading failed");
		free(_old_vehicle_names);
		return false;
	}
	DEBUG(oldloader, 3, "Done, converting game data...");

	FixOldMapArray();

	/* Fix some general stuff */
	_settings_game.game_creation.landscape = _settings_game.game_creation.landscape & 0xF;

	/* Remap some pointers */
	_cur_town_ctr      = REMAP_TOWN_IDX(_old_cur_town_ctr);

	/* Fix the game to be compatible with OpenTTD */
	FixOldTowns();
	FixOldVehicles();

	/* We have a new difficulty setting */
	_settings_game.difficulty.town_council_tolerance = Clamp(_settings_game.difficulty.diff_level, 0, 2);

	DEBUG(oldloader, 3, "Finished converting game data");
	DEBUG(oldloader, 1, "TTD(Patch) savegame successfully converted");

	free(_old_vehicle_names);

	return true;
}
