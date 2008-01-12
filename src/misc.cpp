/* $Id$ */

/** @file misc.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "currency.h"
#include "landscape.h"
#include "news.h"
#include "table/strings.h"
#include "table/sprites.h"
#include "saveload.h"
#include "engine.h"
#include "vehicle_gui.h"
#include "variables.h"
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


char _name_array[512][32];

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
void InitializeMainGui();
void InitializeTowns();
void InitializeTrees();
void InitializeSigns();
void InitializeStations();
void InitializeCargoPackets();
static void InitializeNameMgr();
void InitializePlayers();
static void InitializeCheats();
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
	InitializeMainGui();

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

void DeleteName(StringID id)
{
	if (IsCustomName(id)) {
		memset(_name_array[id & 0x1FF], 0, sizeof(_name_array[id & 0x1FF]));
	}
}

char *GetName(char *buff, StringID id, const char* last)
{
	return strecpy(buff, _name_array[id & ~0x600], last);
}


static void InitializeCheats()
{
	memset(&_cheats, 0, sizeof(Cheats));
}


static void InitializeNameMgr()
{
	memset(_name_array, 0, sizeof(_name_array));
}

StringID RealAllocateName(const char *name, byte skip, bool check_double)
{
	char (*free_item)[lengthof(*_name_array)] = NULL;
	char (*i)[lengthof(*_name_array)];

	for (i = _name_array; i != endof(_name_array); ++i) {
		if ((*i)[0] == '\0') {
			if (free_item == NULL) free_item = i;
		} else if (check_double && strncmp(*i, name, lengthof(*i) - 1) == 0) {
			_error_message = STR_0132_CHOSEN_NAME_IN_USE_ALREADY;
			return 0;
		}
	}

	if (free_item != NULL) {
		ttd_strlcpy(*free_item, name, lengthof(*free_item));
		return (free_item - _name_array) | 0x7800 | (skip << 8);
	} else {
		_error_message = STR_0131_TOO_MANY_NAMES_DEFINED;
		return 0;
	}
}

void ConvertNameArray()
{
	uint i;

	for (i = 0; i < lengthof(_name_array); i++) {
		const char *strfrom = _name_array[i];
		char tmp[sizeof(*_name_array)];
		char *strto = tmp;

		for (; *strfrom != '\0'; strfrom++) {
			WChar c = (byte)*strfrom;
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
			if (strto + Utf8CharLen(c) > lastof(tmp)) break;
			strto += Utf8Encode(strto, c);
		}

		/* Terminate the new string and copy it back to the name array */
		*strto = '\0';
		memcpy(_name_array[i], tmp, sizeof(*_name_array));
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


static void Save_NAME()
{
	int i;

	for (i = 0; i != lengthof(_name_array); ++i) {
		if (_name_array[i][0] != '\0') {
			SlSetArrayIndex(i);
			SlArray(_name_array[i], (uint)strlen(_name_array[i]), SLE_UINT8);
		}
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
	    SLEG_VAR(_random_seeds[0][0],     SLE_UINT32),
	    SLEG_VAR(_random_seeds[0][1],     SLE_UINT32),
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

static void Load_MAPT()
{
	uint size = MapSize();
	uint i;

	for (i = 0; i != size;) {
		byte buf[4096];
		uint j;

		SlArray(buf, lengthof(buf), SLE_UINT8);
		for (j = 0; j != lengthof(buf); j++) _m[i++].type_height = buf[j];
	}
}

static void Save_MAPT()
{
	uint size = MapSize();
	uint i;

	SlSetLength(size);
	for (i = 0; i != size;) {
		byte buf[4096];
		uint j;

		for (j = 0; j != lengthof(buf); j++) buf[j] = _m[i++].type_height;
		SlArray(buf, lengthof(buf), SLE_UINT8);
	}
}

static void Load_MAP1()
{
	uint size = MapSize();
	uint i;

	for (i = 0; i != size;) {
		byte buf[4096];
		uint j;

		SlArray(buf, lengthof(buf), SLE_UINT8);
		for (j = 0; j != lengthof(buf); j++) _m[i++].m1 = buf[j];
	}
}

static void Save_MAP1()
{
	uint size = MapSize();
	uint i;

	SlSetLength(size);
	for (i = 0; i != size;) {
		byte buf[4096];
		uint j;

		for (j = 0; j != lengthof(buf); j++) buf[j] = _m[i++].m1;
		SlArray(buf, lengthof(buf), SLE_UINT8);
	}
}

static void Load_MAP2()
{
	uint size = MapSize();
	uint i;

	for (i = 0; i != size;) {
		uint16 buf[4096];
		uint j;

		SlArray(buf, lengthof(buf),
			/* In those versions the m2 was 8 bits */
			CheckSavegameVersion(5) ? SLE_FILE_U8 | SLE_VAR_U16 : SLE_UINT16
		);
		for (j = 0; j != lengthof(buf); j++) _m[i++].m2 = buf[j];
	}
}

static void Save_MAP2()
{
	uint size = MapSize();
	uint i;

	SlSetLength(size * sizeof(_m[0].m2));
	for (i = 0; i != size;) {
		uint16 buf[4096];
		uint j;

		for (j = 0; j != lengthof(buf); j++) buf[j] = _m[i++].m2;
		SlArray(buf, lengthof(buf), SLE_UINT16);
	}
}

static void Load_MAP3()
{
	uint size = MapSize();
	uint i;

	for (i = 0; i != size;) {
		byte buf[4096];
		uint j;

		SlArray(buf, lengthof(buf), SLE_UINT8);
		for (j = 0; j != lengthof(buf); j++) _m[i++].m3 = buf[j];
	}
}

static void Save_MAP3()
{
	uint size = MapSize();
	uint i;

	SlSetLength(size);
	for (i = 0; i != size;) {
		byte buf[4096];
		uint j;

		for (j = 0; j != lengthof(buf); j++) buf[j] = _m[i++].m3;
		SlArray(buf, lengthof(buf), SLE_UINT8);
	}
}

static void Load_MAP4()
{
	uint size = MapSize();
	uint i;

	for (i = 0; i != size;) {
		byte buf[4096];
		uint j;

		SlArray(buf, lengthof(buf), SLE_UINT8);
		for (j = 0; j != lengthof(buf); j++) _m[i++].m4 = buf[j];
	}
}

static void Save_MAP4()
{
	uint size = MapSize();
	uint i;

	SlSetLength(size);
	for (i = 0; i != size;) {
		byte buf[4096];
		uint j;

		for (j = 0; j != lengthof(buf); j++) buf[j] = _m[i++].m4;
		SlArray(buf, lengthof(buf), SLE_UINT8);
	}
}

static void Load_MAP5()
{
	uint size = MapSize();
	uint i;

	for (i = 0; i != size;) {
		byte buf[4096];
		uint j;

		SlArray(buf, lengthof(buf), SLE_UINT8);
		for (j = 0; j != lengthof(buf); j++) _m[i++].m5 = buf[j];
	}
}

static void Save_MAP5()
{
	uint size = MapSize();
	uint i;

	SlSetLength(size);
	for (i = 0; i != size;) {
		byte buf[4096];
		uint j;

		for (j = 0; j != lengthof(buf); j++) buf[j] = _m[i++].m5;
		SlArray(buf, lengthof(buf), SLE_UINT8);
	}
}

static void Load_MAP6()
{
	/* Still available for loading old games */
	uint size = MapSize();
	uint i;

	if (CheckSavegameVersion(42)) {
		for (i = 0; i != size;) {
			uint8 buf[1024];
			uint j;

			SlArray(buf, lengthof(buf), SLE_UINT8);
			for (j = 0; j != lengthof(buf); j++) {
				_m[i++].m6 = GB(buf[j], 0, 2);
				_m[i++].m6 = GB(buf[j], 2, 2);
				_m[i++].m6 = GB(buf[j], 4, 2);
				_m[i++].m6 = GB(buf[j], 6, 2);
			}
		}
	} else {
		for (i = 0; i != size;) {
			byte buf[4096];
			uint j;

			SlArray(buf, lengthof(buf), SLE_UINT8);
			for (j = 0; j != lengthof(buf); j++) _m[i++].m6 = buf[j];
		}
	}
}

static void Save_MAP6()
{
	uint size = MapSize();
	uint i;

	SlSetLength(size);
	for (i = 0; i != size;) {
		uint8 buf[4096];
		uint j;

		for (j = 0; j != lengthof(buf); j++) buf[j] = _m[i++].m6;
		SlArray(buf, lengthof(buf), SLE_UINT8);
	}
}

static void Load_MAP7()
{
	uint size = MapSize();
	uint i;

	for (i = 0; i != size;) {
		uint8 buf[4096];
		uint j;

		SlArray(buf, lengthof(buf), SLE_UINT8);
		for (j = 0; j != lengthof(buf); j++) _me[i++].m7 = buf[j];
	}
}

static void Save_MAP7()
{
	uint size = MapSize();
	uint i;

	SlSetLength(size);
	for (i = 0; i != size;) {
		uint8 buf[4096];
		uint j;

		for (j = 0; j != lengthof(buf); j++) buf[j] = _me[i++].m7;
		SlArray(buf, lengthof(buf), SLE_UINT8);
	}
}

static void Save_CHTS()
{
	byte count = sizeof(_cheats)/sizeof(Cheat);
	Cheat* cht = (Cheat*) &_cheats;
	Cheat* cht_last = &cht[count];

	SlSetLength(count * 2);
	for (; cht != cht_last; cht++) {
		SlWriteByte(cht->been_used);
		SlWriteByte(cht->value);
	}
}

static void Load_CHTS()
{
	Cheat* cht = (Cheat*)&_cheats;
	uint count = SlGetFieldLength() / 2;
	uint i;

	for (i = 0; i < count; i++) {
		cht[i].been_used = (SlReadByte() != 0);
		cht[i].value     = (SlReadByte() != 0);
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

	{ 'NAME', Save_NAME,     Load_NAME,     CH_ARRAY},
	{ 'DATE', SaveLoad_DATE, SaveLoad_DATE, CH_RIFF},
	{ 'VIEW', SaveLoad_VIEW, SaveLoad_VIEW, CH_RIFF},
	{ 'CHTS', Save_CHTS,     Load_CHTS,     CH_RIFF | CH_LAST}
};
