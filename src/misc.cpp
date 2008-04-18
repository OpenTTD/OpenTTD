/* $Id$ */

/** @file misc.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "currency.h"
#include "landscape.h"
#include "news_func.h"
#include "saveload.h"
#include "vehicle_gui.h"
#include "variables.h"
#include "cheat_func.h"
#include "ai/ai.h"
#include "newgrf_house.h"
#include "cargotype.h"
#include "group.h"
#include "viewport_func.h"
#include "economy_func.h"
#include "zoom_func.h"
#include "functions.h"
#include "map_func.h"
#include "date_func.h"
#include "vehicle_func.h"
#include "texteff.hpp"
#include "string_func.h"
#include "gfx_func.h"
#include "core/alloc_type.hpp"

#include "table/strings.h"
#include "table/sprites.h"

char _name_array[512][32];
extern TileIndex _cur_tileloop_tile;

void InitializeVehicles();
void InitializeWaypoints();
void InitializeDepots();
void InitializeEngines();
void InitializeOrders();
void InitializeClearLand();
void InitializeRailGui();
void InitializeRoadGui();
void InitializeAirportGui();
void InitializeDockGui();
void InitializeIndustries();
void InitializeTowns();
void InitializeTrees();
void InitializeSigns();
void InitializeStations();
void InitializeCargoPackets();
static void InitializeNameMgr();
void InitializePlayers();
void InitializeCheats();
void InitializeNPF();

void InitializeGame(int mode, uint size_x, uint size_y)
{
	AllocateMap(size_x, size_y);

	SetObjectToPlace(SPR_CURSOR_ZZZ, PAL_NONE, VHM_NONE, WC_MAIN_WINDOW, 0);

	_pause_game = 0;
	_fast_forward = 0;
	_tick_counter = 0;
	_realtime_tick = 0;
	_date_fract = 0;
	_cur_tileloop_tile = 0;

	if ((mode & IG_DATE_RESET) == IG_DATE_RESET) {
		SetDate(ConvertYMDToDate(_patches.starting_year, 0, 1));
	}

	InitializeEngines();
	InitializeVehicles();
	InitializeWaypoints();
	InitializeDepots();
	InitializeOrders();
	InitializeGroup();

	InitNewsItemStructs();
	InitializeLandscape();
	InitializeClearLand();
	InitializeRailGui();
	InitializeRoadGui();
	InitializeAirportGui();
	InitializeDockGui();
	InitializeTowns();
	InitializeTrees();
	InitializeSigns();
	InitializeStations();
	InitializeCargoPackets();
	InitializeIndustries();
	InitializeBuildingCounts();

	InitializeNameMgr();
	InitializeVehiclesGuiList();
	InitializeTrains();
	InitializeNPF();

	AI_Initialize();
	InitializePlayers();
	InitializeCheats();

	InitTextEffects();
	InitChatMessage();
	InitializeAnimatedTiles();

	InitializeLandscapeVariables(false);

	ResetObjectToPlace();
}

bool IsCustomName(StringID id)
{
	return GB(id, 11, 5) == 15;
}


static void InitializeNameMgr()
{
	memset(_name_array, 0, sizeof(_name_array));
}

/* Copy and convert old custom names to UTF-8 */
char *CopyFromOldName(StringID id)
{
	if (!IsCustomName(id)) return NULL;

	if (CheckSavegameVersion(37)) {
		/* Old names were 32 characters long, so 128 characters should be
		 * plenty to allow for expansion when converted to UTF-8. */
		char tmp[128];
		const char *strfrom = _name_array[GB(id, 0, 9)];
		char *strto = tmp;

		for (; *strfrom != '\0'; strfrom++) {
			WChar c = (byte)*strfrom;

			/* Map from non-ISO8859-15 characters to UTF-8. */
			switch (c) {
				case 0xA4: c = 0x20AC; break; // Euro
				case 0xA6: c = 0x0160; break; // S with caron
				case 0xA8: c = 0x0161; break; // s with caron
				case 0xB4: c = 0x017D; break; // Z with caron
				case 0xB8: c = 0x017E; break; // z with caron
				case 0xBC: c = 0x0152; break; // OE ligature
				case 0xBD: c = 0x0153; break; // oe ligature
				case 0xBE: c = 0x0178; break; // Y with diaresis
				default: break;
			}

			/* Check character will fit into our buffer. */
			if (strto + Utf8CharLen(c) > lastof(tmp)) break;

			strto += Utf8Encode(strto, c);
		}

		/* Terminate the new string and copy it back to the name array */
		*strto = '\0';

		return strdup(tmp);
	} else {
		/* Name will already be in UTF-8. */
		return strdup(_name_array[GB(id, 0, 9)]);
	}
}

/* Calculate constants that depend on the landscape type. */
void InitializeLandscapeVariables(bool only_constants)
{
	if (only_constants) return;

	for (CargoID i = 0; i < NUM_CARGO; i++) {
		_cargo_payment_rates[i] = GetCargo(i)->initial_payment;
		_cargo_payment_rates_frac[i] = 0;
	}
}

static void Load_NAME()
{
	int index;

	while ((index = SlIterateArray()) != -1) {
		SlArray(_name_array[index], SlGetFieldLength(), SLE_UINT8);
	}
}

static const SaveLoadGlobVarList _date_desc[] = {
	SLEG_CONDVAR(_date,                   SLE_FILE_U16 | SLE_VAR_I32,  0,  30),
	SLEG_CONDVAR(_date,                   SLE_INT32,                  31, SL_MAX_VERSION),
	    SLEG_VAR(_date_fract,             SLE_UINT16),
	    SLEG_VAR(_tick_counter,           SLE_UINT16),
	    SLEG_VAR(_vehicle_id_ctr_day,     SLE_UINT16),
	    SLEG_VAR(_age_cargo_skip_counter, SLE_UINT8),
	SLE_CONDNULL(1, 0, 45),
	SLEG_CONDVAR(_cur_tileloop_tile,      SLE_FILE_U16 | SLE_VAR_U32,  0, 5),
	SLEG_CONDVAR(_cur_tileloop_tile,      SLE_UINT32,                  6, SL_MAX_VERSION),
	    SLEG_VAR(_disaster_delay,         SLE_UINT16),
	    SLEG_VAR(_station_tick_ctr,       SLE_UINT16),
	    SLEG_VAR(_random.state[0],        SLE_UINT32),
	    SLEG_VAR(_random.state[1],        SLE_UINT32),
	SLEG_CONDVAR(_cur_town_ctr,           SLE_FILE_U8  | SLE_VAR_U32,  0, 9),
	SLEG_CONDVAR(_cur_town_ctr,           SLE_UINT32,                 10, SL_MAX_VERSION),
	    SLEG_VAR(_cur_player_tick_index,  SLE_FILE_U8  | SLE_VAR_U32),
	    SLEG_VAR(_next_competitor_start,  SLE_FILE_U16 | SLE_VAR_U32),
	    SLEG_VAR(_trees_tick_ctr,         SLE_UINT8),
	SLEG_CONDVAR(_pause_game,             SLE_UINT8,                   4, SL_MAX_VERSION),
	SLEG_CONDVAR(_cur_town_iter,          SLE_UINT32,                 11, SL_MAX_VERSION),
	    SLEG_END()
};

/* Save load date related variables as well as persistent tick counters
 * XXX: currently some unrelated stuff is just put here */
static void SaveLoad_DATE()
{
	SlGlobList(_date_desc);
}


static const SaveLoadGlobVarList _view_desc[] = {
	SLEG_CONDVAR(_saved_scrollpos_x,    SLE_FILE_I16 | SLE_VAR_I32, 0, 5),
	SLEG_CONDVAR(_saved_scrollpos_x,    SLE_INT32,                  6, SL_MAX_VERSION),
	SLEG_CONDVAR(_saved_scrollpos_y,    SLE_FILE_I16 | SLE_VAR_I32, 0, 5),
	SLEG_CONDVAR(_saved_scrollpos_y,    SLE_INT32,                  6, SL_MAX_VERSION),
	    SLEG_VAR(_saved_scrollpos_zoom, SLE_UINT8),
	    SLEG_END()
};

static void SaveLoad_VIEW()
{
	SlGlobList(_view_desc);
}

static uint32 _map_dim_x;
static uint32 _map_dim_y;

static const SaveLoadGlobVarList _map_dimensions[] = {
	SLEG_CONDVAR(_map_dim_x, SLE_UINT32, 6, SL_MAX_VERSION),
	SLEG_CONDVAR(_map_dim_y, SLE_UINT32, 6, SL_MAX_VERSION),
	    SLEG_END()
};

static void Save_MAPS()
{
	_map_dim_x = MapSizeX();
	_map_dim_y = MapSizeY();
	SlGlobList(_map_dimensions);
}

static void Load_MAPS()
{
	SlGlobList(_map_dimensions);
	AllocateMap(_map_dim_x, _map_dim_y);
}

enum {
	MAP_SL_BUF_SIZE = 4096
};

static void Load_MAPT()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	for (TileIndex i = 0; i != size;) {
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) _m[i++].type_height = buf[j];
	}
}

static void Save_MAPT()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	SlSetLength(size);
	for (TileIndex i = 0; i != size;) {
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) buf[j] = _m[i++].type_height;
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
	}
}

static void Load_MAP1()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	for (TileIndex i = 0; i != size;) {
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) _m[i++].m1 = buf[j];
	}
}

static void Save_MAP1()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	SlSetLength(size);
	for (TileIndex i = 0; i != size;) {
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) buf[j] = _m[i++].m1;
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
	}
}

static void Load_MAP2()
{
	SmallStackSafeStackAlloc<uint16, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	for (TileIndex i = 0; i != size;) {
		SlArray(buf, MAP_SL_BUF_SIZE,
			/* In those versions the m2 was 8 bits */
			CheckSavegameVersion(5) ? SLE_FILE_U8 | SLE_VAR_U16 : SLE_UINT16
		);
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) _m[i++].m2 = buf[j];
	}
}

static void Save_MAP2()
{
	SmallStackSafeStackAlloc<uint16, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	SlSetLength(size * sizeof(uint16));
	for (TileIndex i = 0; i != size;) {
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) buf[j] = _m[i++].m2;
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT16);
	}
}

static void Load_MAP3()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	for (TileIndex i = 0; i != size;) {
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) _m[i++].m3 = buf[j];
	}
}

static void Save_MAP3()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	SlSetLength(size);
	for (TileIndex i = 0; i != size;) {
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) buf[j] = _m[i++].m3;
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
	}
}

static void Load_MAP4()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	for (TileIndex i = 0; i != size;) {
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) _m[i++].m4 = buf[j];
	}
}

static void Save_MAP4()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	SlSetLength(size);
	for (TileIndex i = 0; i != size;) {
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) buf[j] = _m[i++].m4;
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
	}
}

static void Load_MAP5()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	for (TileIndex i = 0; i != size;) {
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) _m[i++].m5 = buf[j];
	}
}

static void Save_MAP5()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	SlSetLength(size);
	for (TileIndex i = 0; i != size;) {
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) buf[j] = _m[i++].m5;
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
	}
}

static void Load_MAP6()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	if (CheckSavegameVersion(42)) {
		for (TileIndex i = 0; i != size;) {
			/* 1024, otherwise we overflow on 64x64 maps! */
			SlArray(buf, 1024, SLE_UINT8);
			for (uint j = 0; j != 1024; j++) {
				_m[i++].m6 = GB(buf[j], 0, 2);
				_m[i++].m6 = GB(buf[j], 2, 2);
				_m[i++].m6 = GB(buf[j], 4, 2);
				_m[i++].m6 = GB(buf[j], 6, 2);
			}
		}
	} else {
		for (TileIndex i = 0; i != size;) {
			SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
			for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) _m[i++].m6 = buf[j];
		}
	}
}

static void Save_MAP6()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	SlSetLength(size);
	for (TileIndex i = 0; i != size;) {
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) buf[j] = _m[i++].m6;
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
	}
}

static void Load_MAP7()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	for (TileIndex i = 0; i != size;) {
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) _me[i++].m7 = buf[j];
	}
}

static void Save_MAP7()
{
	SmallStackSafeStackAlloc<byte, MAP_SL_BUF_SIZE> buf;
	TileIndex size = MapSize();

	SlSetLength(size);
	for (TileIndex i = 0; i != size;) {
		for (uint j = 0; j != MAP_SL_BUF_SIZE; j++) buf[j] = _me[i++].m7;
		SlArray(buf, MAP_SL_BUF_SIZE, SLE_UINT8);
	}
}

extern const ChunkHandler _misc_chunk_handlers[] = {
	{ 'MAPS', Save_MAPS,     Load_MAPS,     CH_RIFF },
	{ 'MAPT', Save_MAPT,     Load_MAPT,     CH_RIFF },
	{ 'MAPO', Save_MAP1,     Load_MAP1,     CH_RIFF },
	{ 'MAP2', Save_MAP2,     Load_MAP2,     CH_RIFF },
	{ 'M3LO', Save_MAP3,     Load_MAP3,     CH_RIFF },
	{ 'M3HI', Save_MAP4,     Load_MAP4,     CH_RIFF },
	{ 'MAP5', Save_MAP5,     Load_MAP5,     CH_RIFF },
	{ 'MAPE', Save_MAP6,     Load_MAP6,     CH_RIFF },
	{ 'MAP7', Save_MAP7,     Load_MAP7,     CH_RIFF },

	{ 'NAME', NULL,          Load_NAME,     CH_ARRAY},
	{ 'DATE', SaveLoad_DATE, SaveLoad_DATE, CH_RIFF},
	{ 'VIEW', SaveLoad_VIEW, SaveLoad_VIEW, CH_RIFF | CH_LAST},
};
