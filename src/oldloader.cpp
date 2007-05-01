/* $Id$ */

/** @file oldloader.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "station_map.h"
#include "table/strings.h"
#include "functions.h"
#include "map.h"
#include "town.h"
#include "industry.h"
#include "station.h"
#include "economy.h"
#include "player.h"
#include "engine.h"
#include "vehicle.h"
#include "aircraft.h"
#include "roadveh.h"
#include "ship.h"
#include "train.h"
#include "signs.h"
#include "debug.h"
#include "depot.h"
#include "network/network.h"
#include "ai/ai.h"
#include "date.h"

enum {
	HEADER_SIZE = 49,
	BUFFER_SIZE = 4096,

	OLD_MAP_SIZE = 256 * 256
};

struct LoadgameState {
	FILE *file;

	uint chunk_size;

	bool decoding;
	byte decode_char;

	uint buffer_count;
	uint buffer_cur;
	byte buffer[BUFFER_SIZE];

	uint total_read;
	bool failed;
};

/* OldChunk-Type */
enum OldChunkType {
	OC_SIMPLE    = 0,
	OC_NULL      = 1,
	OC_CHUNK     = 2,
	OC_ASSERT    = 3,
	/* 8 bytes allocated (256 max) */

	OC_VAR_I8    = 1 << 8,
	OC_VAR_U8    = 2 << 8,
	OC_VAR_I16   = 3 << 8,
	OC_VAR_U16   = 4 << 8,
	OC_VAR_I32   = 5 << 8,
	OC_VAR_U32   = 6 << 8,
	OC_VAR_I64   = 7 << 8,
	/* 8 bytes allocated (256 max) */

	OC_FILE_I8   = 1 << 16,
	OC_FILE_U8   = 2 << 16,
	OC_FILE_I16  = 3 << 16,
	OC_FILE_U16  = 4 << 16,
	OC_FILE_I32  = 5 << 16,
	OC_FILE_U32  = 6 << 16,
	/* 8 bytes allocated (256 max) */

	OC_INT8      = OC_VAR_I8   | OC_FILE_I8,
	OC_UINT8     = OC_VAR_U8   | OC_FILE_U8,
	OC_INT16     = OC_VAR_I16  | OC_FILE_I16,
	OC_UINT16    = OC_VAR_U16  | OC_FILE_U16,
	OC_INT32     = OC_VAR_I32  | OC_FILE_I32,
	OC_UINT32    = OC_VAR_U32  | OC_FILE_U32,

	OC_TILE      = OC_VAR_U32  | OC_FILE_U16,

	OC_END       = 0 ///< End of the whole chunk, all 32bits set to zero
};

DECLARE_ENUM_AS_BIT_SET(OldChunkType);

typedef bool OldChunkProc(LoadgameState *ls, int num);

struct OldChunks {
	OldChunkType type;   ///< Type of field
	uint32 amount;       ///< Amount of fields

	void *ptr;           ///< Pointer where to save the data (may only be set if offset is 0)
	uint offset;         ///< Offset from basepointer (may only be set if ptr is NULL)
	OldChunkProc *proc;  ///< Pointer to function that is called with OC_CHUNK
};

/* If it fails, check lines above.. */
assert_compile(sizeof(TileIndex) == 4);

static uint32 _bump_assert_value;
static bool   _read_ttdpatch_flags;

static OldChunkType GetOldChunkType(OldChunkType type)     {return (OldChunkType)GB(type, 0, 8);}
static OldChunkType GetOldChunkVarType(OldChunkType type)  {return (OldChunkType)(GB(type, 8, 8) << 8);}
static OldChunkType GetOldChunkFileType(OldChunkType type) {return (OldChunkType)(GB(type, 16, 8) << 16);}

static inline byte CalcOldVarLen(OldChunkType type)
{
	static const byte type_mem_size[] = {0, 1, 1, 2, 2, 4, 4, 8};
	byte length = GB(type, 8, 8);
	assert(length != 0 && length < lengthof(type_mem_size));
	return type_mem_size[length];
}

/**
 *
 * Reads a byte from a file (do not call yourself, use ReadByte())
 *
 */
static byte ReadByteFromFile(LoadgameState *ls)
{
	/* To avoid slow reads, we read BUFFER_SIZE of bytes per time
	and just return a byte per time */
	if (ls->buffer_cur >= ls->buffer_count) {
		/* Read some new bytes from the file */
		int count = (int)fread(ls->buffer, 1, BUFFER_SIZE, ls->file);

		/* We tried to read, but there is nothing in the file anymore.. */
		if (count == 0) {
			DEBUG(oldloader, 0, "Read past end of file, loading failed");
			ls->failed = true;
		}

		ls->buffer_count = count;
		ls->buffer_cur   = 0;
	}

	return ls->buffer[ls->buffer_cur++];
}

/**
 *
 * Reads a byte from the buffer and decompress if needed
 *
 */
static byte ReadByte(LoadgameState *ls)
{
	/* Old savegames have a nice compression algorithm (RLE)
	which means that we have a chunk, which starts with a length
	byte. If that byte is negative, we have to repeat the next byte
	that many times ( + 1). Else, we need to read that amount of bytes.
	Works pretty good if you have many zero's behind eachother */

	if (ls->chunk_size == 0) {
		/* Read new chunk */
		int8 new_byte = ReadByteFromFile(ls);

		if (new_byte < 0) {
			/* Repeat next char for new_byte times */
			ls->decoding    = true;
			ls->decode_char = ReadByteFromFile(ls);
			ls->chunk_size  = -new_byte + 1;
		} else {
			ls->decoding    = false;
			ls->chunk_size  = new_byte + 1;
		}
	}

	ls->total_read++;
	ls->chunk_size--;

	return ls->decoding ? ls->decode_char : ReadByteFromFile(ls);
}

static inline uint16 ReadUint16(LoadgameState *ls)
{
	byte x = ReadByte(ls);
	return x | ReadByte(ls) << 8;
}

static inline uint32 ReadUint32(LoadgameState *ls)
{
	uint16 x = ReadUint16(ls);
	return x | ReadUint16(ls) << 16;
}

/**
 *
 * Loads a chunk from the old savegame
 *
 */
static bool LoadChunk(LoadgameState *ls, void *base, const OldChunks *chunks)
{
	const OldChunks *chunk = chunks;
	byte *base_ptr = (byte*)base;

	while (chunk->type != OC_END) {
		byte* ptr = (byte*)chunk->ptr;
		uint i;

		for (i = 0; i < chunk->amount; i++) {
			if (ls->failed) return false;

			/* Handle simple types */
			if (GetOldChunkType(chunk->type) != 0) {
				switch (GetOldChunkType(chunk->type)) {
					/* Just read the byte and forget about it */
					case OC_NULL: ReadByte(ls); break;

					case OC_CHUNK:
						/* Call function, with 'i' as parameter to tell which item we
						 * are going to read */
						if (!chunk->proc(ls, i)) return false;
						break;

					case OC_ASSERT:
						DEBUG(oldloader, 4, "Assert point: 0x%X / 0x%X", ls->total_read, chunk->offset + _bump_assert_value);
						if (ls->total_read != chunk->offset + _bump_assert_value) ls->failed = true;
					default: break;
				}
			} else {
				uint32 res = 0;

				/* Reading from the file: bits 16 to 23 have the FILE type */
				switch (GetOldChunkFileType(chunk->type)) {
					case OC_FILE_I8:  res = (int8)ReadByte(ls); break;
					case OC_FILE_U8:  res = ReadByte(ls); break;
					case OC_FILE_I16: res = (int16)ReadUint16(ls); break;
					case OC_FILE_U16: res = ReadUint16(ls); break;
					case OC_FILE_I32: res = (int32)ReadUint32(ls); break;
					case OC_FILE_U32: res = ReadUint32(ls); break;
					default: NOT_REACHED();
				}

				/* Sanity check */
				assert(base_ptr != NULL || chunk->ptr != NULL);

				/* Writing to the var: bits 8 to 15 have the VAR type */
				if (chunk->ptr == NULL) ptr = base_ptr + chunk->offset;

				/* Write the data */
				switch (GetOldChunkVarType(chunk->type)) {
					case OC_VAR_I8: *(int8  *)ptr = GB(res, 0, 8); break;
					case OC_VAR_U8: *(uint8 *)ptr = GB(res, 0, 8); break;
					case OC_VAR_I16:*(int16 *)ptr = GB(res, 0, 16); break;
					case OC_VAR_U16:*(uint16*)ptr = GB(res, 0, 16); break;
					case OC_VAR_I32:*(int32 *)ptr = res; break;
					case OC_VAR_U32:*(uint32*)ptr = res; break;
					case OC_VAR_I64:*(int64 *)ptr = res; break;
					default: NOT_REACHED();
				}

				/* Increase pointer base for arrays when looping */
				if (chunk->amount > 1 && chunk->ptr != NULL) ptr += CalcOldVarLen(chunk->type);
			}
		}

		chunk++;
	}

	return true;
}

/**
 *
 * Initialize some data before reading
 *
 */
static void InitLoading(LoadgameState *ls)
{
	ls->chunk_size   = 0;
	ls->total_read   = 0;
	ls->failed       = false;

	ls->decoding     = false;
	ls->decode_char  = 0;

	ls->buffer_cur   = 0;
	ls->buffer_count = 0;
	memset(ls->buffer, 0, BUFFER_SIZE);

	_bump_assert_value = 0;

	_read_ttdpatch_flags = false;
}


/*
 * Begin -- Stuff to fix the savegames to be OpenTTD compatible
 */

extern uint32 GetOldTownName(uint32 townnameparts, byte old_town_name_type);

static void FixOldTowns()
{
	Town *town;

	/* Convert town-names if needed */
	FOR_ALL_TOWNS(town) {
		if (IS_INT_INSIDE(town->townnametype, 0x20C1, 0x20C3)) {
			town->townnametype = SPECSTR_TOWNNAME_ENGLISH + _opt.town_name;
			town->townnameparts = GetOldTownName(town->townnameparts, _opt.town_name);
		}
	}
}

static void FixOldStations()
{
	Station *st;

	FOR_ALL_STATIONS(st) {
		/* Check if we need to swap width and height for the station */
		if (st->train_tile != 0 && GetRailStationAxis(st->train_tile) != AXIS_X) {
			Swap(st->trainst_w, st->trainst_h);
		}
	}
}

static void FixOldVehicles()
{
	/* Check for shared orders, and link them correctly */
	Vehicle* v;

	FOR_ALL_VEHICLES(v) {
		Vehicle *u;

		FOR_ALL_VEHICLES_FROM(u, v->index + 1) {
			/* If a vehicle has the same orders, add the link to eachother
			 * in both vehicles */
			if (v->orders == u->orders) {
				v->next_shared = u;
				u->prev_shared = v;
				break;
			}
		}
	}
}

/*
 * End -- Stuff to fix the savegames to be OpenTTD compatible
 */


/* Help:
 *  - OCL_SVAR: load 'type' to offset 'offset' in a struct of type 'base', which must also
 *       be given via base in LoadChunk() as real pointer
 *  - OCL_VAR: load 'type' to a global var
 *  - OCL_END: every struct must end with this
 *  - OCL_NULL: read 'amount' of bytes and send them to /dev/null or something
 *  - OCL_CHUNK: load an other proc to load a part of the savegame, 'amount' times
 *  - OCL_ASSERT: to check if we are really at the place we expect to be.. because old savegames are too binary to be sure ;)
 */
#define OCL_SVAR(type, base, offset)         { type,          1, NULL,    (uint)cpp_offsetof(base, offset), NULL }
#define OCL_VAR(type, amount, pointer)       { type,     amount, pointer, 0,                      NULL }
#define OCL_END()                                   { OC_END,        0, NULL,    0,                      NULL }
#define OCL_NULL(amount)                            { OC_NULL,  amount, NULL,    0,                      NULL }
#define OCL_CHUNK(amount, proc)                     { OC_CHUNK, amount, NULL,    0,                      proc }
#define OCL_ASSERT(size)                            { OC_ASSERT,     1, NULL, size,                      NULL }

/* The savegames has some hard-coded pointers, because it always enters the same
    piece of memory.. we don't.. so we need to remap ;)
   Old Towns are 94 bytes big
   Old Orders are 2 bytes big */
#define REMAP_TOWN_IDX(x) ((x) - (0x0459154 - 0x0458EF0)) / 94
#define REMAP_ORDER_IDX(x) ((x) - (0x045AB08 - 0x0458EF0)) / 2

extern TileIndex _animated_tile_list[256];
extern char _name_array[512][32];

static byte   _old_vehicle_multiplier;
static uint8  _old_map3[OLD_MAP_SIZE * 2];
static bool   _new_ttdpatch_format;
static uint32 _old_town_index;
static uint16 _old_string_id;
static uint16 _old_string_id_2;
static uint16 _old_extra_chunk_nums;

static void ReadTTDPatchFlags()
{
	int i;

	if (_read_ttdpatch_flags) return;

	_read_ttdpatch_flags = true;

	/* TTDPatch misuses _old_map3 for flags.. read them! */
	_old_vehicle_multiplier = _old_map3[0];
	/* Somehow.... there was an error in some savegames, so 0 becomes 1
	and 1 becomes 2. The rest of the values are okay */
	if (_old_vehicle_multiplier < 2) _old_vehicle_multiplier++;

	/* TTDPatch increases the Vehicle-part in the middle of the game,
	so if the multipler is anything else but 1, the assert fails..
	bump the assert value so it doesn't!
	(1 multipler == 850 vehicles
	1 vehicle   == 128 bytes */
	_bump_assert_value = (_old_vehicle_multiplier - 1) * 850 * 128;

	/* Check if we have a modern TTDPatch savegame (has extra data all around) */
	_new_ttdpatch_format = (memcmp(&_old_map3[0x1FFFA], "TTDp", 4) == 0);

	_old_extra_chunk_nums = _old_map3[_new_ttdpatch_format ? 0x1FFFE : 0x2];

	/* Clean the misused places */
	for (i = 0;       i < 17;      i++) _old_map3[i] = 0;
	for (i = 0x1FE00; i < 0x20000; i++) _old_map3[i] = 0;

	if (_new_ttdpatch_format) DEBUG(oldloader, 2, "Found TTDPatch game");

	DEBUG(oldloader, 3, "Vehicle-multiplier is set to %d (%d vehicles)", _old_vehicle_multiplier, _old_vehicle_multiplier * 850);
}

static const OldChunks town_chunk[] = {
	OCL_SVAR(   OC_TILE, Town, xy ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_U32, Town, population ),
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

	/* XXX - This is pretty odd.. we read 32bit, but only write 8bit.. sure there is
	nothing changed ? ? */
	OCL_SVAR( OC_FILE_U32 | OC_VAR_U8, Town, have_ratings ),
	OCL_SVAR( OC_FILE_U32 | OC_VAR_U8, Town, statues ),
	OCL_SVAR( OC_UINT16, Town, num_houses ),
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
	if (!AddBlockIfNeeded(&_Town_pool, num))
		error("Towns: failed loading savegame: too many towns");

	return LoadChunk(ls, GetTown(num), town_chunk);
}

static uint16 _old_order;
static const OldChunks order_chunk[] = {
	OCL_VAR ( OC_UINT16,   1, &_old_order ),
	OCL_END()
};

static bool LoadOldOrder(LoadgameState *ls, int num)
{
	if (!AddBlockIfNeeded(&_Order_pool, num))
		error("Orders: failed loading savegame: too many orders");

	if (!LoadChunk(ls, NULL, order_chunk)) return false;

	AssignOrder(GetOrder(num), UnpackOldOrder(_old_order));

	/* Relink the orders to eachother (in TTD(Patch) the orders for one
	vehicle are behind eachother, with an invalid order (OT_NOTHING) as indication that
	it is the last order */
	if (num > 0 && GetOrder(num)->IsValid())
		GetOrder(num - 1)->next = GetOrder(num);

	return true;
}

static const OldChunks depot_chunk[] = {
	OCL_SVAR(   OC_TILE, Depot, xy ),
	OCL_VAR ( OC_UINT32,   1, &_old_town_index ),
	OCL_END()
};

static bool LoadOldDepot(LoadgameState *ls, int num)
{
	if (!AddBlockIfNeeded(&_Depot_pool, num))
		error("Depots: failed loading savegame: too many depots");

	if (!LoadChunk(ls, GetDepot(num), depot_chunk)) return false;

	if (IsValidDepot(GetDepot(num))) {
		GetDepot(num)->town_index = REMAP_TOWN_IDX(_old_town_index);
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
	((int32*)&_price)[num] = _old_price;
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

static uint8 _old_platforms;
static uint _current_station_id;

static const OldChunks goods_chunk[] = {
	OCL_SVAR( OC_UINT16, GoodsEntry, waiting_acceptance ),
	OCL_SVAR(  OC_UINT8, GoodsEntry, days_since_pickup ),
	OCL_SVAR(  OC_UINT8, GoodsEntry, rating ),
	OCL_SVAR(  OC_FILE_U8 | OC_VAR_U16, GoodsEntry, enroute_from ),
	OCL_SVAR(  OC_UINT8, GoodsEntry, enroute_time ),
	OCL_SVAR(  OC_UINT8, GoodsEntry, last_speed ),
	OCL_SVAR(  OC_UINT8, GoodsEntry, last_age ),

	OCL_END()
};

static bool LoadOldGood(LoadgameState *ls, int num)
{
	Station *st = GetStation(_current_station_id);
	return LoadChunk(ls, &st->goods[num], goods_chunk);
}

static const OldChunks station_chunk[] = {
	OCL_SVAR(   OC_TILE, Station, xy ),
	OCL_VAR ( OC_UINT32,   1, &_old_town_index ),

	OCL_NULL( 4 ), ///< bus/lorry tile
	OCL_SVAR(   OC_TILE, Station, train_tile ),
	OCL_SVAR(   OC_TILE, Station, airport_tile ),
	OCL_SVAR(   OC_TILE, Station, dock_tile ),

	OCL_VAR (  OC_UINT8,   1, &_old_platforms ),

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
	Station *st;

	if (!AddBlockIfNeeded(&_Station_pool, num))
		error("Stations: failed loading savegame: too many stations");

	st = GetStation(num);
	_current_station_id = num;

	if (!LoadChunk(ls, st, station_chunk))
		return false;

	if (st->IsValid()) {
		if (st->train_tile) {
			/* Calculate the trainst_w and trainst_h */
			uint w = GB(_old_platforms, 3, 3);
			uint h = GB(_old_platforms, 0, 3);
			st->trainst_w = w;
			st->trainst_h = h;
		}

		st->town    = GetTown(REMAP_TOWN_IDX(_old_town_index));
		st->string_id = RemapOldStringID(_old_string_id);
	}

	return true;
}

static const OldChunks industry_chunk[] = {
	OCL_SVAR(   OC_TILE, Industry, xy ),
	OCL_VAR ( OC_UINT32,   1, &_old_town_index ),
	OCL_SVAR(  OC_UINT8, Industry, width ),
	OCL_SVAR(  OC_UINT8, Industry, height ),
	OCL_SVAR(  OC_UINT8, Industry, produced_cargo[0] ),
	OCL_SVAR(  OC_UINT8, Industry, produced_cargo[1] ),

	OCL_SVAR( OC_UINT16, Industry, cargo_waiting[0] ),
	OCL_SVAR( OC_UINT16, Industry, cargo_waiting[1] ),

	OCL_SVAR(  OC_UINT8, Industry, production_rate[0] ),
	OCL_SVAR(  OC_UINT8, Industry, production_rate[1] ),

	OCL_SVAR(  OC_UINT8, Industry, accepts_cargo[0] ),
	OCL_SVAR(  OC_UINT8, Industry, accepts_cargo[1] ),
	OCL_SVAR(  OC_UINT8, Industry, accepts_cargo[2] ),

	OCL_SVAR(  OC_UINT8, Industry, prod_level ),

	OCL_SVAR( OC_UINT16, Industry, last_mo_production[0] ),
	OCL_SVAR( OC_UINT16, Industry, last_mo_production[1] ),
	OCL_SVAR( OC_UINT16, Industry, last_mo_transported[0] ),
	OCL_SVAR( OC_UINT16, Industry, last_mo_transported[1] ),

	OCL_SVAR(  OC_UINT8, Industry, pct_transported[0] ),
	OCL_SVAR(  OC_UINT8, Industry, pct_transported[1] ),

	OCL_SVAR( OC_UINT16, Industry, total_production[0] ),
	OCL_SVAR( OC_UINT16, Industry, total_production[1] ),
	OCL_SVAR( OC_UINT16, Industry, total_transported[0] ),
	OCL_SVAR( OC_UINT16, Industry, total_transported[1] ),

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
	Industry *i;

	if (!AddBlockIfNeeded(&_Industry_pool, num))
		error("Industries: failed loading savegame: too many industries");

	i = GetIndustry(num);
	if (!LoadChunk(ls, i, industry_chunk)) return false;

	if (IsValidIndustry(i)) {
		i->town = GetTown(REMAP_TOWN_IDX(_old_town_index));
	}

	return true;
}

static PlayerID _current_player_id;
static int32 _old_yearly;

static const OldChunks player_yearly_chunk[] = {
	OCL_VAR(  OC_INT32,   1, &_old_yearly ),
	OCL_END()
};

static bool OldPlayerYearly(LoadgameState *ls, int num)
{
	int i;
	Player *p = GetPlayer(_current_player_id);

	for (i = 0; i < 13; i++) {
		if (!LoadChunk(ls, NULL, player_yearly_chunk)) return false;

		p->yearly_expenses[num][i] = _old_yearly;
	}

	return true;
}

static const OldChunks player_economy_chunk[] = {
	OCL_SVAR( OC_INT32, PlayerEconomyEntry, income ),
	OCL_SVAR( OC_INT32, PlayerEconomyEntry, expenses ),
	OCL_SVAR( OC_INT32, PlayerEconomyEntry, delivered_cargo ),
	OCL_SVAR( OC_INT32, PlayerEconomyEntry, performance_history ),
	OCL_SVAR( OC_FILE_I32 | OC_VAR_I64, PlayerEconomyEntry, company_value ),

	OCL_END()
};

static bool OldPlayerEconomy(LoadgameState *ls, int num)
{
	int i;
	Player *p = GetPlayer(_current_player_id);

	if (!LoadChunk(ls, &p->cur_economy, player_economy_chunk)) return false;

	/* Don't ask, but the number in TTD(Patch) are inversed to OpenTTD */
	p->cur_economy.income   = -p->cur_economy.income;
	p->cur_economy.expenses = -p->cur_economy.expenses;

	for (i = 0; i < 24; i++) {
		if (!LoadChunk(ls, &p->old_economy[i], player_economy_chunk)) return false;

		p->old_economy[i].income   = -p->old_economy[i].income;
		p->old_economy[i].expenses = -p->old_economy[i].expenses;
	}

	return true;
}

static const OldChunks player_ai_build_rec_chunk[] = {
	OCL_SVAR(   OC_TILE, AiBuildRec, spec_tile ),
	OCL_SVAR(   OC_TILE, AiBuildRec, use_tile ),
	OCL_SVAR(  OC_UINT8, AiBuildRec, rand_rng ),
	OCL_SVAR(  OC_UINT8, AiBuildRec, cur_building_rule ),
	OCL_SVAR(  OC_UINT8, AiBuildRec, unk6 ),
	OCL_SVAR(  OC_UINT8, AiBuildRec, unk7 ),
	OCL_SVAR(  OC_UINT8, AiBuildRec, buildcmd_a ),
	OCL_SVAR(  OC_UINT8, AiBuildRec, buildcmd_b ),
	OCL_SVAR(  OC_UINT8, AiBuildRec, direction ),
	OCL_SVAR(  OC_UINT8, AiBuildRec, cargo ),

	OCL_NULL( 8 ),  ///< Junk...

	OCL_END()
};

static bool OldLoadAIBuildRec(LoadgameState *ls, int num)
{
	Player *p = GetPlayer(_current_player_id);

	switch (num) {
		case 0: return LoadChunk(ls, &p->ai.src, player_ai_build_rec_chunk);
		case 1: return LoadChunk(ls, &p->ai.dst, player_ai_build_rec_chunk);
		case 2: return LoadChunk(ls, &p->ai.mid1, player_ai_build_rec_chunk);
		case 3: return LoadChunk(ls, &p->ai.mid2, player_ai_build_rec_chunk);
	}

	return false;
}
static const OldChunks player_ai_chunk[] = {
	OCL_SVAR(  OC_UINT8, PlayerAI, state ),
	OCL_NULL( 1 ),         ///< Junk
	OCL_SVAR(  OC_UINT8, PlayerAI, state_mode ),
	OCL_SVAR( OC_UINT16, PlayerAI, state_counter ),
	OCL_SVAR( OC_UINT16, PlayerAI, timeout_counter ),

	OCL_CHUNK( 4, OldLoadAIBuildRec ),

	OCL_NULL( 20 ),        ///< More junk

	OCL_SVAR(  OC_UINT8, PlayerAI, cargo_type ),
	OCL_SVAR(  OC_UINT8, PlayerAI, num_wagons ),
	OCL_SVAR(  OC_UINT8, PlayerAI, build_kind ),
	OCL_SVAR(  OC_UINT8, PlayerAI, num_build_rec ),
	OCL_SVAR(  OC_UINT8, PlayerAI, num_loco_to_build ),
	OCL_SVAR(  OC_UINT8, PlayerAI, num_want_fullload ),

	OCL_NULL( 14 ),        ///< Oh no more junk :|

	OCL_NULL( 2 ),         ///< Loco-id, not used

	OCL_SVAR( OC_UINT16, PlayerAI, wagon_list[0] ),
	OCL_SVAR( OC_UINT16, PlayerAI, wagon_list[1] ),
	OCL_SVAR( OC_UINT16, PlayerAI, wagon_list[2] ),
	OCL_SVAR( OC_UINT16, PlayerAI, wagon_list[3] ),
	OCL_SVAR( OC_UINT16, PlayerAI, wagon_list[4] ),
	OCL_SVAR( OC_UINT16, PlayerAI, wagon_list[5] ),
	OCL_SVAR( OC_UINT16, PlayerAI, wagon_list[6] ),
	OCL_SVAR( OC_UINT16, PlayerAI, wagon_list[7] ),
	OCL_SVAR( OC_UINT16, PlayerAI, wagon_list[8] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, order_list_blocks[0] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, order_list_blocks[1] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, order_list_blocks[2] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, order_list_blocks[3] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, order_list_blocks[4] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, order_list_blocks[5] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, order_list_blocks[6] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, order_list_blocks[7] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, order_list_blocks[8] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, order_list_blocks[9] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, order_list_blocks[10] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, order_list_blocks[11] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, order_list_blocks[12] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, order_list_blocks[13] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, order_list_blocks[14] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, order_list_blocks[15] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, order_list_blocks[16] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, order_list_blocks[17] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, order_list_blocks[18] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, order_list_blocks[19] ),

	OCL_SVAR( OC_UINT16, PlayerAI, start_tile_a ),
	OCL_SVAR( OC_UINT16, PlayerAI, start_tile_b ),
	OCL_SVAR( OC_UINT16, PlayerAI, cur_tile_a ),
	OCL_SVAR( OC_UINT16, PlayerAI, cur_tile_b ),

	OCL_SVAR(  OC_UINT8, PlayerAI, start_dir_a ),
	OCL_SVAR(  OC_UINT8, PlayerAI, start_dir_b ),
	OCL_SVAR(  OC_UINT8, PlayerAI, cur_dir_a ),
	OCL_SVAR(  OC_UINT8, PlayerAI, cur_dir_b ),

	OCL_SVAR(  OC_UINT8, PlayerAI, banned_tile_count ),

	OCL_SVAR(   OC_TILE, PlayerAI, banned_tiles[0] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, banned_val[0] ),
	OCL_SVAR(   OC_TILE, PlayerAI, banned_tiles[1] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, banned_val[1] ),
	OCL_SVAR(   OC_TILE, PlayerAI, banned_tiles[2] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, banned_val[2] ),
	OCL_SVAR(   OC_TILE, PlayerAI, banned_tiles[3] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, banned_val[3] ),
	OCL_SVAR(   OC_TILE, PlayerAI, banned_tiles[4] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, banned_val[4] ),
	OCL_SVAR(   OC_TILE, PlayerAI, banned_tiles[5] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, banned_val[5] ),
	OCL_SVAR(   OC_TILE, PlayerAI, banned_tiles[6] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, banned_val[6] ),
	OCL_SVAR(   OC_TILE, PlayerAI, banned_tiles[7] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, banned_val[7] ),
	OCL_SVAR(   OC_TILE, PlayerAI, banned_tiles[8] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, banned_val[8] ),
	OCL_SVAR(   OC_TILE, PlayerAI, banned_tiles[9] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, banned_val[9] ),
	OCL_SVAR(   OC_TILE, PlayerAI, banned_tiles[10] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, banned_val[10] ),
	OCL_SVAR(   OC_TILE, PlayerAI, banned_tiles[11] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, banned_val[11] ),
	OCL_SVAR(   OC_TILE, PlayerAI, banned_tiles[12] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, banned_val[12] ),
	OCL_SVAR(   OC_TILE, PlayerAI, banned_tiles[13] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, banned_val[13] ),
	OCL_SVAR(   OC_TILE, PlayerAI, banned_tiles[14] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, banned_val[14] ),
	OCL_SVAR(   OC_TILE, PlayerAI, banned_tiles[15] ),
	OCL_SVAR(  OC_UINT8, PlayerAI, banned_val[15] ),

	OCL_SVAR(  OC_UINT8, PlayerAI, railtype_to_use ),
	OCL_SVAR(  OC_UINT8, PlayerAI, route_type_mask ),

	OCL_END()
};

static bool OldPlayerAI(LoadgameState *ls, int num)
{
	Player *p = GetPlayer(_current_player_id);

	return LoadChunk(ls, &p->ai, player_ai_chunk);
}

static const OldChunks player_chunk[] = {
	OCL_VAR ( OC_UINT16,   1, &_old_string_id ),
	OCL_SVAR( OC_UINT32, Player, name_2 ),
	OCL_SVAR( OC_UINT32, Player, face ),
	OCL_VAR ( OC_UINT16,   1, &_old_string_id_2 ),
	OCL_SVAR( OC_UINT32, Player, president_name_2 ),

	OCL_SVAR(  OC_INT32, Player, player_money ),
	OCL_SVAR(  OC_INT32, Player, current_loan ),

	OCL_SVAR(  OC_UINT8, Player, player_color ),
	OCL_SVAR(  OC_UINT8, Player, player_money_fraction ),
	OCL_SVAR(  OC_UINT8, Player, quarters_of_bankrupcy ),
	OCL_SVAR(  OC_UINT8, Player, bankrupt_asked ),
	OCL_SVAR( OC_UINT32, Player, bankrupt_value ),
	OCL_SVAR( OC_UINT16, Player, bankrupt_timeout ),

	OCL_SVAR( OC_FILE_U32 | OC_VAR_U16, Player, cargo_types ),

	OCL_CHUNK( 3, OldPlayerYearly ),
	OCL_CHUNK( 1, OldPlayerEconomy ),

	OCL_SVAR( OC_FILE_U16 | OC_VAR_I32, Player, inaugurated_year),
	OCL_SVAR(                  OC_TILE, Player, last_build_coordinate ),
	OCL_SVAR(                 OC_UINT8, Player, num_valid_stat_ent ),

	OCL_CHUNK( 1, OldPlayerAI ),

	OCL_SVAR(  OC_UINT8, Player, block_preview ),
	OCL_SVAR(  OC_UINT8, Player, ai.tick ),
	OCL_SVAR(  OC_UINT8, Player, avail_railtypes ),
	OCL_SVAR(   OC_TILE, Player, location_of_house ),
	OCL_SVAR(  OC_UINT8, Player, share_owners[0] ),
	OCL_SVAR(  OC_UINT8, Player, share_owners[1] ),
	OCL_SVAR(  OC_UINT8, Player, share_owners[2] ),
	OCL_SVAR(  OC_UINT8, Player, share_owners[3] ),

	OCL_NULL( 8 ), ///< junk at end of chunk

	OCL_END()
};

static bool LoadOldPlayer(LoadgameState *ls, int num)
{
	Player *p = GetPlayer((PlayerID)num);

	_current_player_id = (PlayerID)num;

	if (!LoadChunk(ls, p, player_chunk)) return false;

	p->name_1 = RemapOldStringID(_old_string_id);
	p->president_name_1 = RemapOldStringID(_old_string_id_2);
	p->money64 = p->player_money;

	if (num == 0) {
		/* If the first player has no name, make sure we call it UNNAMED */
		if (p->name_1 == 0)
			p->name_1 = STR_SV_UNNAMED;
	} else {
		/* Beside some multiplayer maps (1 on 1), which we don't official support,
		all other players are an AI.. mark them as such */
		p->is_ai = true;
	}

	/* Sometimes it is better to not ask.. in old scenarios, the money
	was always 893288 pounds. In the newer versions this is correct,
	but correct for those oldies
	Ps: this also means that if you had exact 893288 pounds, you will go back
	to 10000.. this is a very VERY small chance ;) */
	if (p->player_money == 893288)
		p->money64 = p->player_money = p->current_loan = 100000;

	_player_colors[num] = p->player_color;
	p->inaugurated_year -= ORIGINAL_BASE_YEAR;
	if (p->location_of_house == 0xFFFF)
		p->location_of_house = 0;

	/* State 20 for AI players is sell vehicle. Since the AI struct is not
	 * really figured out as of now, p->ai.cur_veh; needed for 'sell vehicle'
	 * is NULL and the function will crash. To fix this, just change the state
	 * to some harmless state, like 'loop vehicle'; 1 */
	if (!IsHumanPlayer((PlayerID)num) && p->ai.state == 20) p->ai.state = 1;

	if (p->is_ai && (!_networking || _network_server) && _ai.enabled)
		AI_StartNewAI(p->index);

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

static const OldChunks vehicle_special_chunk[] = {
	OCL_SVAR( OC_UINT16, VehicleSpecial, unk0 ),
	OCL_SVAR(  OC_UINT8, VehicleSpecial, unk2 ),

	OCL_NULL( 7 ), // Junk

	OCL_END()
};

static const OldChunks vehicle_disaster_chunk[] = {
	OCL_SVAR( OC_UINT16, VehicleDisaster, image_override ),
	OCL_SVAR( OC_UINT16, VehicleDisaster, unk2 ),

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

	/* We changed the offset of the vehicle types, so fix it
	 * Basically v->type -= 0x10; would suffice, but play safely */
	switch (v->type) {
		default: NOT_REACHED();
		case 0x00 /*VEH_INVALID */: v = new (v) InvalidVehicle();  res = LoadChunk(ls, NULL,           vehicle_empty_chunk);    break;
		case 0x10 /*VEH_TRAIN   */: v = new (v) Train();           res = LoadChunk(ls, &v->u.rail,     vehicle_train_chunk);    break;
		case 0x11 /*VEH_ROAD    */: v = new (v) RoadVehicle();     res = LoadChunk(ls, &v->u.road,     vehicle_road_chunk);     break;
		case 0x12 /*VEH_SHIP    */: v = new (v) Ship();            res = LoadChunk(ls, &v->u.ship,     vehicle_ship_chunk);     break;
		case 0x13 /*VEH_AIRCRAFT*/: v = new (v) Aircraft();        res = LoadChunk(ls, &v->u.air,      vehicle_air_chunk);      break;
		case 0x14 /*VEH_SPECIAL */: v = new (v) SpecialVehicle();  res = LoadChunk(ls, &v->u.special,  vehicle_special_chunk);  break;
		case 0x15 /*VEH_DISASTER*/: v = new (v) DisasterVehicle(); res = LoadChunk(ls, &v->u.disaster, vehicle_disaster_chunk); break;
	}

	/* This chunk size should always be 10 bytes */
	if (ls->total_read - temp != 10) {
		DEBUG(oldloader, 0, "Assert failed in VehicleUnion: invalid chunk size");
		return false;
	}

	return res;
}

static const OldChunks vehicle_chunk[] = {
	OCL_SVAR(  OC_UINT8, Vehicle, type ),
	OCL_SVAR(  OC_UINT8, Vehicle, subtype ),

	OCL_NULL( 2 ),         ///< Hash, calculated automatically
	OCL_NULL( 2 ),         ///< Index, calculated automatically

	OCL_VAR ( OC_UINT32,   1, &_old_order_ptr ),
	OCL_VAR ( OC_UINT16,   1, &_old_order ),

	OCL_SVAR(  OC_UINT8, Vehicle, num_orders ),
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
	OCL_NULL( 2 ),         ///< sprite_width and sprite_height, calculated automatically
	OCL_NULL( 1 ),         ///< z_height, calculated automatically

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
	OCL_SVAR( OC_UINT16, Vehicle, cargo_count ),
	OCL_SVAR( OC_FILE_U8 | OC_VAR_U16, Vehicle, cargo_source ),
	OCL_SVAR(  OC_UINT8, Vehicle, cargo_days ),

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

	OCL_SVAR(  OC_INT32, Vehicle, profit_this_year ),
	OCL_SVAR(  OC_INT32, Vehicle, profit_last_year ),

	OCL_VAR ( OC_UINT16,   1, &_old_next_ptr ),

	OCL_SVAR( OC_UINT32, Vehicle, value ),

	OCL_VAR ( OC_UINT16,   1, &_old_string_id ),

	OCL_CHUNK( 1, LoadOldVehicleUnion ),

	OCL_NULL( 20 ), ///< Junk at end of struct (TTDPatch has some data in it)

	OCL_END()
};

static bool LoadOldVehicle(LoadgameState *ls, int num)
{
	uint i;

	/* Read the TTDPatch flags, because we need some info from it */
	ReadTTDPatchFlags();

	for (i = 0; i < _old_vehicle_multiplier; i++) {
		Vehicle *v;

		_current_vehicle_id = num * _old_vehicle_multiplier + i;

		if (!AddBlockIfNeeded(&_Vehicle_pool, _current_vehicle_id))
			error("Vehicles: failed loading savegame: too many vehicles");

		v = GetVehicle(_current_vehicle_id);
		if (!LoadChunk(ls, v, vehicle_chunk)) return false;

		/* This should be consistent, else we have a big problem... */
		if (v->index != _current_vehicle_id) {
			DEBUG(oldloader, 0, "Loading failed - vehicle-array is invalid");
			return false;
		}

		if (_old_order_ptr != 0 && _old_order_ptr != 0xFFFFFFFF) {
			v->orders = GetOrder(REMAP_ORDER_IDX(_old_order_ptr));
		}
		AssignOrder(&v->current_order, UnpackOldOrder(_old_order));

		/* For some reason we need to correct for this */
		switch (v->spritenum) {
			case 0xfd: break;
			case 0xff: v->spritenum = 0xfe; break;
			default:   v->spritenum >>= 1; break;
		}

		if (_old_next_ptr != 0xFFFF) v->next = GetVehicle(_old_next_ptr);

		v->string_id = RemapOldStringID(_old_string_id);

		/* Vehicle-subtype is different in TTD(Patch) */
		if (v->type == VEH_SPECIAL) v->subtype = v->subtype >> 1;
	}

	return true;
}

static const OldChunks sign_chunk[] = {
	OCL_SVAR( OC_UINT16, Sign, str ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_I32, Sign, x ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_I32, Sign, y ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_I8, Sign, z ),

	OCL_NULL( 6 ),         ///< Width of sign, no longer in use

	OCL_END()
};

static bool LoadOldSign(LoadgameState *ls, int num)
{
	if (!AddBlockIfNeeded(&_Sign_pool, num))
		error("Signs: failed loading savegame: too many signs");

	return LoadChunk(ls, GetSign(num), sign_chunk);
}

static const OldChunks engine_chunk[] = {
	OCL_SVAR( OC_UINT16, Engine, player_avail ),
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
	OCL_SVAR(  OC_UINT8, Engine, preview_player ),
	OCL_SVAR(  OC_UINT8, Engine, preview_wait ),

	OCL_NULL( 2 ), ///< Junk

	OCL_END()
};

static bool LoadOldEngine(LoadgameState *ls, int num)
{
	if (!LoadChunk(ls, GetEngine(num), engine_chunk)) return false;

	/* Make sure wagons are marked as do-not-age */
	if ((num >= 27 && num < 54) || (num >= 57 && num < 84) || (num >= 89 && num < 116))
		GetEngine(num)->age = 0xFFFF;

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
	OCL_SVAR( OC_FILE_U16 | OC_VAR_I32, GameDifficulty, max_no_competitors ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_I32, GameDifficulty, competitor_start_time ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_I32, GameDifficulty, number_towns ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_I32, GameDifficulty, number_industries ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_I32, GameDifficulty, max_loan ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_I32, GameDifficulty, initial_interest ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_I32, GameDifficulty, vehicle_costs ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_I32, GameDifficulty, competitor_speed ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_I32, GameDifficulty, competitor_intelligence ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_I32, GameDifficulty, vehicle_breakdowns ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_I32, GameDifficulty, subsidy_multiplier ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_I32, GameDifficulty, construction_cost ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_I32, GameDifficulty, terrain_type ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_I32, GameDifficulty, quantity_sea_lakes ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_I32, GameDifficulty, economy ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_I32, GameDifficulty, line_reverse_mode ),
	OCL_SVAR( OC_FILE_U16 | OC_VAR_I32, GameDifficulty, disasters ),
	OCL_END()
};

static inline bool LoadOldGameDifficulty(LoadgameState *ls, int num)
{
	return LoadChunk(ls, &_opt.diff, game_difficulty_chunk);
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
			case 0x3: {
				uint32 ttdpv = ReadUint32(ls);
				DEBUG(oldloader, 3, "Game saved with TTDPatch version %d.%d.%d r%d", GB(ttdpv, 24, 8), GB(ttdpv, 20, 4), GB(ttdpv, 16, 4), GB(ttdpv, 0, 16));
				len -= 4;
				while (len-- != 0) ReadByte(ls); // skip the configuration
			} break;

			default:
				DEBUG(oldloader, 4, "Skipping unknown extra chunk %X", id);
				while (len-- != 0) ReadByte(ls);
				break;
		}
	}

	return !ls->failed;
}

static uint32 _old_cur_town_ctr;
static const OldChunks main_chunk[] = {
	OCL_ASSERT( 0 ),
	OCL_VAR ( OC_FILE_U16 | OC_VAR_U32, 1, &_date ),
	OCL_VAR ( OC_UINT16,   1, &_date_fract ),
	OCL_NULL( 600 ),            ///< TextEffects
	OCL_VAR ( OC_UINT32,   2, &_random_seeds[0] ),

	OCL_ASSERT( 0x264 ),
	OCL_CHUNK(  70, LoadOldTown ),
	OCL_ASSERT( 0x1C18 ),
	OCL_CHUNK(5000, LoadOldOrder ),
	OCL_ASSERT( 0x4328 ),

	OCL_VAR (   OC_TILE, 256, &_animated_tile_list[0] ),
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
	OCL_CHUNK(  8, LoadOldPlayer ),

	OCL_ASSERT( 0x547F2 ),

	OCL_CHUNK( 850, LoadOldVehicle ),

	OCL_ASSERT( 0x6F0F2 ),

	OCL_VAR (  OC_UINT8, 32 * 500, &_name_array[0] ),

	OCL_NULL( 0x2000 ),            ///< Old hash-table, no longer in use

	OCL_CHUNK( 40, LoadOldSign ),
	OCL_CHUNK(256, LoadOldEngine ),

	OCL_VAR ( OC_UINT16,    1, &_vehicle_id_ctr_day ),

	OCL_CHUNK(  8, LoadOldSubsidy ),

	OCL_VAR ( OC_FILE_U16 | OC_VAR_U32,   1, &_next_competitor_start ),
	OCL_VAR ( OC_FILE_I16 | OC_VAR_I32,   1, &_saved_scrollpos_x ),
	OCL_VAR ( OC_FILE_I16 | OC_VAR_I32,   1, &_saved_scrollpos_y ),
	OCL_VAR ( OC_FILE_U16 | OC_VAR_U8,    1, &_saved_scrollpos_zoom ),

	OCL_VAR ( OC_UINT32,    1, &_economy.max_loan ),
	OCL_VAR ( OC_UINT32,    1, &_economy.max_loan_unround ),
	OCL_VAR ( OC_FILE_U16 | OC_VAR_U32,    1, &_economy.fluct ),

	OCL_VAR ( OC_UINT16,    1, &_disaster_delay ),

	OCL_NULL( 144 ),             ///< cargo-stuff, calculated in InitializeLandscapeVariables

	OCL_VAR ( OC_UINT16,  256, &_engine_name_strings[0] ),

	OCL_NULL( 144 ),             ///< AI cargo-stuff, calculated in InitializeLandscapeVariables
	OCL_NULL( 2 ),               ///< Company indexes of players, no longer in use

	OCL_VAR ( OC_FILE_U8 | OC_VAR_U16,    1, &_station_tick_ctr ),

	OCL_VAR (  OC_UINT8,    1, &_opt.currency ),
	OCL_VAR (  OC_UINT8,    1, &_opt.units ),
	OCL_VAR ( OC_FILE_U8 | OC_VAR_U32,    1, &_cur_player_tick_index ),

	OCL_NULL( 2 ),               ///< Date stuff, calculated automatically
	OCL_NULL( 8 ),               ///< Player colors, calculated automatically

	OCL_VAR (  OC_UINT8,    1, &_economy.infl_amount ),
	OCL_VAR (  OC_UINT8,    1, &_economy.infl_amount_pr ),
	OCL_VAR (  OC_UINT8,    1, &_economy.interest_rate ),
	OCL_NULL( 1 ), // available airports
	OCL_VAR (  OC_UINT8,    1, &_opt.road_side ),
	OCL_VAR (  OC_UINT8,    1, &_opt.town_name ),

	OCL_CHUNK( 1, LoadOldGameDifficulty ),

	OCL_ASSERT( 0x77130 ),

	OCL_VAR (  OC_UINT8,    1, &_opt.diff_level ),
	OCL_VAR (  OC_UINT8,    1, &_opt.landscape ),
	OCL_VAR (  OC_UINT8,    1, &_trees_tick_ctr ),

	OCL_NULL( 1 ),               ///< Custom vehicle types yes/no, no longer used
	OCL_VAR (  OC_UINT8,    1, &_opt.snow_line ),

	OCL_NULL( 32 ),              ///< new_industry_randtable, no longer used (because of new design)
	OCL_NULL( 36 ),              ///< cargo-stuff, calculated in InitializeLandscapeVariables

	OCL_ASSERT( 0x77179 ),

	OCL_CHUNK( 1, LoadOldMapPart2 ),

	OCL_ASSERT( 0x97179 ),

	/* Below any (if available) extra chunks from TTDPatch can follow */
	OCL_CHUNK(1, LoadTTDPatchExtraChunks),

	OCL_END()
};

static bool LoadOldMain(LoadgameState *ls)
{
	int i;

	/* The first 49 is the name of the game + checksum, skip it */
	fseek(ls->file, HEADER_SIZE, SEEK_SET);

	DEBUG(oldloader, 3, "Reading main chunk...");
	/* Load the biggest chunk */
	if (!LoadChunk(ls, NULL, main_chunk)) {
		DEBUG(oldloader, 0, "Loading failed");
		return false;
	}
	DEBUG(oldloader, 3, "Done, converting game data...");

	/* Fix some general stuff */
	_opt.landscape = _opt.landscape & 0xF;

	/* Remap some pointers */
	_cur_town_ctr      = REMAP_TOWN_IDX(_old_cur_town_ctr);

	/* _old_map3 is changed in _map3_lo and _map3_hi */
	for (i = 0; i < OLD_MAP_SIZE; i++) {
		_m[i].m3 = _old_map3[i * 2];
		_m[i].m4 = _old_map3[i * 2 + 1];
	}

	for (i = 0; i < OLD_MAP_SIZE; i ++) {
		switch (GetTileType(i)) {
			case MP_RAILWAY:
				/* We save presignals different from TTDPatch, convert them */
				if (GetRailTileType(i) == RAIL_TILE_SIGNALS) {
					/* This byte is always zero in TTD for this type of tile */
					if (_m[i].m4) /* Convert the presignals to our own format */
						_m[i].m4 = (_m[i].m4 >> 1) & 7;
				}
				/* TTDPatch stores PBS things in L6 and all elsewhere; so we'll just
				 * clear it for ourselves and let OTTD's rebuild PBS itself */
				_m[i].m4 &= 0xF; /* Only keep the lower four bits; upper four is PBS */
				break;
			case MP_WATER: {
				/* TTDPatch has all tiles touching water as coast (water)-type, we don't.
				 * This is only true from a certain TTDP version, but there is no harm
				 * in checking all the time */
				Slope s = GetTileSlope(i, NULL);
				if (s == SLOPE_ENW || s == SLOPE_NWS || s == SLOPE_SEN || s == SLOPE_WSE || IsSteepSlope(s)) {
					SetTileType(i, MP_CLEAR);
					SetTileOwner(i, OWNER_NONE);
				}
			} break;
			default: break;
		}
	}

	/* Fix the game to be compatible with OpenTTD */
	FixOldTowns();
	FixOldStations();
	FixOldVehicles();

	AddTypeToEngines();

	/* We have a new difficulty setting */
	_opt.diff.town_council_tolerance = clamp(_opt.diff_level, 0, 2);

	DEBUG(oldloader, 3, "Finished converting game data");
	DEBUG(oldloader, 1, "TTD(Patch) savegame successfully converted");

	return true;
}

bool LoadOldSaveGame(const char *file)
{
	LoadgameState ls;

	DEBUG(oldloader, 3, "Trying to load a TTD(Patch) savegame");

	InitLoading(&ls);

	/* Open file */
	ls.file = fopen(file, "rb");

	if (ls.file == NULL) {
		DEBUG(oldloader, 0, "Cannot open file '%s'", file);
		return false;
	}

	/* Load the main chunk */
	if (!LoadOldMain(&ls)) return false;

	fclose(ls.file);

	_pause_game = 2;

	return true;
}

void GetOldSaveGameName(char *title, const char *path, const char *file)
{
	char filename[MAX_PATH];
	FILE *f;

	snprintf(filename, lengthof(filename), "%s" PATHSEP "%s", path, file);
	f = fopen(filename, "rb");
	title[0] = '\0';
	title[48] = '\0';

	if (f == NULL) return;

	if (fread(title, 1, 48, f) != 48) snprintf(title, 48, "Corrupt file");

	fclose(f);
}
