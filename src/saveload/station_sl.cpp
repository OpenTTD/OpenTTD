/* $Id$ */

/** @file station_sl.cpp Code handling saving and loading of economy data */

#include "../stdafx.h"
#include "../station_base.h"
#include "../core/bitmath_func.hpp"
#include "../core/alloc_func.hpp"
#include "../variables.h"
#include "../newgrf_station.h"

#include "saveload.h"


void AfterLoadStations()
{
	/* Update the speclists of all stations to point to the currently loaded custom stations. */
	Station *st;
	FOR_ALL_STATIONS(st) {
		for (uint i = 0; i < st->num_specs; i++) {
			if (st->speclist[i].grfid == 0) continue;

			st->speclist[i].spec = GetCustomStationSpecByGrf(st->speclist[i].grfid, st->speclist[i].localidx, NULL);
		}

		for (CargoID c = 0; c < NUM_CARGO; c++) st->goods[c].cargo.InvalidateCache();

		StationUpdateAnimTriggers(st);
	}
}

static const SaveLoad _roadstop_desc[] = {
	SLE_VAR(RoadStop, xy,           SLE_UINT32),
	SLE_CONDNULL(1, 0, 44),
	SLE_VAR(RoadStop, status,       SLE_UINT8),
	/* Index was saved in some versions, but this is not needed */
	SLE_CONDNULL(4, 0, 8),
	SLE_CONDNULL(2, 0, 44),
	SLE_CONDNULL(1, 0, 25),

	SLE_REF(RoadStop, next,         REF_ROADSTOPS),
	SLE_CONDNULL(2, 0, 44),

	SLE_CONDNULL(4, 0, 24),
	SLE_CONDNULL(1, 25, 25),

	SLE_END()
};

static const SaveLoad _station_desc[] = {
	SLE_CONDVAR(Station, xy,                         SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	SLE_CONDVAR(Station, xy,                         SLE_UINT32,                  6, SL_MAX_VERSION),
	SLE_CONDNULL(4, 0, 5),  ///< bus/lorry tile
	SLE_CONDVAR(Station, train_tile,                 SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	SLE_CONDVAR(Station, train_tile,                 SLE_UINT32,                  6, SL_MAX_VERSION),
	SLE_CONDVAR(Station, airport_tile,               SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	SLE_CONDVAR(Station, airport_tile,               SLE_UINT32,                  6, SL_MAX_VERSION),
	SLE_CONDVAR(Station, dock_tile,                  SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	SLE_CONDVAR(Station, dock_tile,                  SLE_UINT32,                  6, SL_MAX_VERSION),
	    SLE_REF(Station, town,                       REF_TOWN),
	    SLE_VAR(Station, trainst_w,                  SLE_UINT8),
	SLE_CONDVAR(Station, trainst_h,                  SLE_UINT8,                   2, SL_MAX_VERSION),

	SLE_CONDNULL(1, 0, 3),  ///< alpha_order

	    SLE_VAR(Station, string_id,                  SLE_STRINGID),
	SLE_CONDSTR(Station, name,                       SLE_STR, 0,                 84, SL_MAX_VERSION),
	SLE_CONDVAR(Station, indtype,                    SLE_UINT8,                 103, SL_MAX_VERSION),
	    SLE_VAR(Station, had_vehicle_of_type,        SLE_UINT16),

	    SLE_VAR(Station, time_since_load,            SLE_UINT8),
	    SLE_VAR(Station, time_since_unload,          SLE_UINT8),
	    SLE_VAR(Station, delete_ctr,                 SLE_UINT8),
	    SLE_VAR(Station, owner,                      SLE_UINT8),
	    SLE_VAR(Station, facilities,                 SLE_UINT8),
	    SLE_VAR(Station, airport_type,               SLE_UINT8),

	SLE_CONDNULL(2, 0, 5),  ///< Truck/bus stop status
	SLE_CONDNULL(1, 0, 4),  ///< Blocked months

	SLE_CONDVAR(Station, airport_flags,              SLE_VAR_U64 | SLE_FILE_U16,  0,  2),
	SLE_CONDVAR(Station, airport_flags,              SLE_VAR_U64 | SLE_FILE_U32,  3, 45),
	SLE_CONDVAR(Station, airport_flags,              SLE_UINT64,                 46, SL_MAX_VERSION),

	SLE_CONDNULL(2, 0, 25), ///< last-vehicle
	SLE_CONDVAR(Station, last_vehicle_type,          SLE_UINT8,                  26, SL_MAX_VERSION),

	SLE_CONDNULL(2, 3, 25), ///< custom station class and id
	SLE_CONDVAR(Station, build_date,                 SLE_FILE_U16 | SLE_VAR_I32,  3, 30),
	SLE_CONDVAR(Station, build_date,                 SLE_INT32,                  31, SL_MAX_VERSION),

	SLE_CONDREF(Station, bus_stops,                  REF_ROADSTOPS,               6, SL_MAX_VERSION),
	SLE_CONDREF(Station, truck_stops,                REF_ROADSTOPS,               6, SL_MAX_VERSION),

	/* Used by newstations for graphic variations */
	SLE_CONDVAR(Station, random_bits,                SLE_UINT16,                 27, SL_MAX_VERSION),
	SLE_CONDVAR(Station, waiting_triggers,           SLE_UINT8,                  27, SL_MAX_VERSION),
	SLE_CONDVAR(Station, num_specs,                  SLE_UINT8,                  27, SL_MAX_VERSION),

	SLE_CONDLST(Station, loading_vehicles,           REF_VEHICLE,                57, SL_MAX_VERSION),

	/* reserve extra space in savegame here. (currently 32 bytes) */
	SLE_CONDNULL(32, 2, SL_MAX_VERSION),

	SLE_END()
};

static uint16 _waiting_acceptance;
static uint16 _cargo_source;
static uint32 _cargo_source_xy;
static uint16 _cargo_days;
static Money  _cargo_feeder_share;

static const SaveLoad _station_speclist_desc[] = {
	SLE_CONDVAR(StationSpecList, grfid,    SLE_UINT32, 27, SL_MAX_VERSION),
	SLE_CONDVAR(StationSpecList, localidx, SLE_UINT8,  27, SL_MAX_VERSION),

	SLE_END()
};


void SaveLoad_STNS(Station *st)
{
	static const SaveLoad _goods_desc[] = {
		SLEG_CONDVAR(            _waiting_acceptance, SLE_UINT16,                  0, 67),
		 SLE_CONDVAR(GoodsEntry, acceptance_pickup,   SLE_UINT8,                  68, SL_MAX_VERSION),
		SLE_CONDNULL(2,                                                           51, 67),
		     SLE_VAR(GoodsEntry, days_since_pickup,   SLE_UINT8),
		     SLE_VAR(GoodsEntry, rating,              SLE_UINT8),
		SLEG_CONDVAR(            _cargo_source,       SLE_FILE_U8 | SLE_VAR_U16,   0, 6),
		SLEG_CONDVAR(            _cargo_source,       SLE_UINT16,                  7, 67),
		SLEG_CONDVAR(            _cargo_source_xy,    SLE_UINT32,                 44, 67),
		SLEG_CONDVAR(            _cargo_days,         SLE_UINT8,                   0, 67),
		     SLE_VAR(GoodsEntry, last_speed,          SLE_UINT8),
		     SLE_VAR(GoodsEntry, last_age,            SLE_UINT8),
		SLEG_CONDVAR(            _cargo_feeder_share, SLE_FILE_U32 | SLE_VAR_I64, 14, 64),
		SLEG_CONDVAR(            _cargo_feeder_share, SLE_INT64,                  65, 67),
		 SLE_CONDLST(GoodsEntry, cargo.packets,       REF_CARGO_PACKET,           68, SL_MAX_VERSION),

		SLE_END()
	};


	SlObject(st, _station_desc);

	_waiting_acceptance = 0;

	uint num_cargo = CheckSavegameVersion(55) ? 12 : NUM_CARGO;
	for (CargoID i = 0; i < num_cargo; i++) {
		GoodsEntry *ge = &st->goods[i];
		SlObject(ge, _goods_desc);
		if (CheckSavegameVersion(68)) {
			SB(ge->acceptance_pickup, GoodsEntry::ACCEPTANCE, 1, HasBit(_waiting_acceptance, 15));
			if (GB(_waiting_acceptance, 0, 12) != 0) {
				/* Don't construct the packet with station here, because that'll fail with old savegames */
				CargoPacket *cp = new CargoPacket();
				/* In old versions, enroute_from used 0xFF as INVALID_STATION */
				cp->source          = (CheckSavegameVersion(7) && _cargo_source == 0xFF) ? INVALID_STATION : _cargo_source;
				cp->count           = GB(_waiting_acceptance, 0, 12);
				cp->days_in_transit = _cargo_days;
				cp->feeder_share    = _cargo_feeder_share;
				cp->source_xy       = _cargo_source_xy;
				cp->days_in_transit = _cargo_days;
				cp->feeder_share    = _cargo_feeder_share;
				SB(ge->acceptance_pickup, GoodsEntry::PICKUP, 1, 1);
				ge->cargo.Append(cp);
			}
		}
	}

	if (st->num_specs != 0) {
		/* Allocate speclist memory when loading a game */
		if (st->speclist == NULL) st->speclist = CallocT<StationSpecList>(st->num_specs);
		for (uint i = 0; i < st->num_specs; i++) {
			SlObject(&st->speclist[i], _station_speclist_desc);
		}
	}
}

static void Save_STNS()
{
	Station *st;
	/* Write the stations */
	FOR_ALL_STATIONS(st) {
		SlSetArrayIndex(st->index);
		SlAutolength((AutolengthProc*)SaveLoad_STNS, st);
	}
}

static void Load_STNS()
{
	int index;
	while ((index = SlIterateArray()) != -1) {
		Station *st = new (index) Station();

		SaveLoad_STNS(st);
	}

	/* This is to ensure all pointers are within the limits of _stations_size */
	if (_station_tick_ctr > GetMaxStationIndex()) _station_tick_ctr = 0;
}

static void Save_ROADSTOP()
{
	RoadStop *rs;

	FOR_ALL_ROADSTOPS(rs) {
		SlSetArrayIndex(rs->index);
		SlObject(rs, _roadstop_desc);
	}
}

static void Load_ROADSTOP()
{
	int index;

	while ((index = SlIterateArray()) != -1) {
		RoadStop *rs = new (index) RoadStop(INVALID_TILE);

		SlObject(rs, _roadstop_desc);
	}
}

extern const ChunkHandler _station_chunk_handlers[] = {
	{ 'STNS', Save_STNS,      Load_STNS,      CH_ARRAY },
	{ 'ROAD', Save_ROADSTOP,  Load_ROADSTOP,  CH_ARRAY | CH_LAST},
};
