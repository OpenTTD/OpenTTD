/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "currency.h"
#include "functions.h"
#include "news.h"
#include "player.h"
#include "string.h"
#include "table/strings.h"
#include "table/sprites.h"
#include "map.h"
#include "vehicle.h"
#include "saveload.h"
#include "engine.h"
#include "vehicle_gui.h"
#include "variables.h"
#include "ai/ai.h"
#include "table/landscape_const.h"
#include "date.h"

char _name_array[512][32];

#ifndef MERSENNE_TWISTER

#ifdef RANDOM_DEBUG
#include "network/network_data.h"
uint32 DoRandom(int line, const char *file)
#else // RANDOM_DEBUG
uint32 Random(void)
#endif // RANDOM_DEBUG
{

uint32 s;
uint32 t;

#ifdef RANDOM_DEBUG
	if (_networking && (DEREF_CLIENT(0)->status != STATUS_INACTIVE || !_network_server))
		printf("Random [%d/%d] %s:%d\n",_frame_counter, _current_player, file, line);
#endif

	s = _random_seeds[0][0];
	t = _random_seeds[0][1];
	_random_seeds[0][0] = s + ROR(t ^ 0x1234567F, 7) + 1;
	return _random_seeds[0][1] = ROR(s, 3) - 1;
}
#endif // MERSENNE_TWISTER

#if defined(RANDOM_DEBUG) && !defined(MERSENNE_TWISTER)
uint DoRandomRange(uint max, int line, const char *file)
{
	return GB(DoRandom(line, file), 0, 16) * max >> 16;
}
#else
uint RandomRange(uint max)
{
	return GB(Random(), 0, 16) * max >> 16;
}
#endif


uint32 InteractiveRandom(void)
{
	uint32 t = _random_seeds[1][1];
	uint32 s = _random_seeds[1][0];
	_random_seeds[1][0] = s + ROR(t ^ 0x1234567F, 7) + 1;
	return _random_seeds[1][1] = ROR(s, 3) - 1;
}

uint InteractiveRandomRange(uint max)
{
	return GB(InteractiveRandom(), 0, 16) * max >> 16;
}

void InitializeVehicles(void);
void InitializeWaypoints(void);
void InitializeDepots(void);
void InitializeEngines(void);
void InitializeOrders(void);
void InitializeClearLand(void);
void InitializeRailGui(void);
void InitializeRoadGui(void);
void InitializeAirportGui(void);
void InitializeDockGui(void);
void InitializeIndustries(void);
void InitializeMainGui(void);
void InitializeLandscape(void);
void InitializeTowns(void);
void InitializeTrees(void);
void InitializeSigns(void);
void InitializeStations(void);
static void InitializeNameMgr(void);
void InitializePlayers(void);
static void InitializeCheats(void);
void InitializeNPF(void);

void InitializeGame(int mode, uint size_x, uint size_y)
{
	AllocateMap(size_x, size_y);

	AddTypeToEngines(); // make sure all engines have a type

	SetObjectToPlace(SPR_CURSOR_ZZZ, 0, 0, 0);

	_pause = 0;
	_fast_forward = 0;
	_tick_counter = 0;
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
	InitializeIndustries();
	InitializeMainGui();

	InitializeNameMgr();
	InitializeVehiclesGuiList();
	InitializeTrains();
	InitializeNPF();

	AI_Initialize();
	InitializePlayers();
	InitializeCheats();

	InitTextEffects();
	InitTextMessage();
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


static void InitializeCheats(void)
{
	memset(&_cheats, 0, sizeof(Cheats));
}


static void InitializeNameMgr(void)
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

void ConvertNameArray(void)
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

// Calculate constants that depend on the landscape type.
void InitializeLandscapeVariables(bool only_constants)
{
	const CargoTypesValues *lpd;
	uint i;
	StringID str;

	lpd = &_cargo_types_base_values[_opt.landscape];

	for (i = 0; i != NUM_CARGO; i++) {
		_cargoc.sprites[i] = lpd->sprites[i];

		str = lpd->names[i];
		_cargoc.names_s[i] = str;
		_cargoc.names_long[i] = (str += 0x40);
		_cargoc.names_short[i] = (str += 0x20);
		_cargoc.weights[i] = lpd->weights[i];

		if (!only_constants) {
			_cargo_payment_rates[i] = lpd->initial_cargo_payment[i];
			_cargo_payment_rates_frac[i] = 0;
		}

		_cargoc.transit_days_1[i] = lpd->transit_days_table_1[i];
		_cargoc.transit_days_2[i] = lpd->transit_days_table_2[i];
	}
}



int FindFirstBit(uint32 value)
{
	// The macro FIND_FIRST_BIT is better to use when your value is
	// not more than 128.
	byte i = 0;

	if (value == 0) return 0;

	if ((value & 0x0000ffff) == 0) { value >>= 16; i += 16; }
	if ((value & 0x000000ff) == 0) { value >>= 8;  i += 8;  }
	if ((value & 0x0000000f) == 0) { value >>= 4;  i += 4;  }
	if ((value & 0x00000003) == 0) { value >>= 2;  i += 2;  }
	if ((value & 0x00000001) == 0) { i += 1; }

	return i;
}


static void Save_NAME(void)
{
	int i;

	for (i = 0; i != lengthof(_name_array); ++i) {
		if (_name_array[i][0] != '\0') {
			SlSetArrayIndex(i);
			SlArray(_name_array[i], (uint)strlen(_name_array[i]), SLE_UINT8);
		}
	}
}

static void Load_NAME(void)
{
	int index;

	while ((index = SlIterateArray()) != -1) {
		SlArray(_name_array[index],SlGetFieldLength(),SLE_UINT8);
	}
}

static const SaveLoadGlobVarList _date_desc[] = {
	SLEG_CONDVAR(_date,                   SLE_FILE_U16 | SLE_VAR_I32,  0,  30),
	SLEG_CONDVAR(_date,                   SLE_INT32,                  31, SL_MAX_VERSION),
	    SLEG_VAR(_date_fract,             SLE_UINT16),
	    SLEG_VAR(_tick_counter,           SLE_UINT16),
	    SLEG_VAR(_vehicle_id_ctr_day,     SLE_UINT16),
	    SLEG_VAR(_age_cargo_skip_counter, SLE_UINT8),
	    SLEG_VAR(_avail_aircraft,         SLE_UINT8),
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
	SLEG_CONDVAR(_pause,                  SLE_UINT8,                   4, SL_MAX_VERSION),
	SLEG_CONDVAR(_cur_town_iter,          SLE_UINT32,                 11, SL_MAX_VERSION),
	    SLEG_END()
};

// Save load date related variables as well as persistent tick counters
// XXX: currently some unrelated stuff is just put here
static void SaveLoad_DATE(void)
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

static void SaveLoad_VIEW(void)
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

static void Save_MAPS(void)
{
	_map_dim_x = MapSizeX();
	_map_dim_y = MapSizeY();
	SlGlobList(_map_dimensions);
}

static void Load_MAPS(void)
{
	SlGlobList(_map_dimensions);
	AllocateMap(_map_dim_x, _map_dim_y);
}

static void Load_MAPT(void)
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

static void Save_MAPT(void)
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

static void Load_MAP1(void)
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

static void Save_MAP1(void)
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

static void Load_MAP2(void)
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

static void Save_MAP2(void)
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

static void Load_MAP3(void)
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

static void Save_MAP3(void)
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

static void Load_MAP4(void)
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

static void Save_MAP4(void)
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

static void Load_MAP5(void)
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

static void Save_MAP5(void)
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

static void Load_MAP6(void)
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

static void Save_MAP6(void)
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


static void Save_CHTS(void)
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

static void Load_CHTS(void)
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

	{ 'NAME', Save_NAME,     Load_NAME,     CH_ARRAY},
	{ 'DATE', SaveLoad_DATE, SaveLoad_DATE, CH_RIFF},
	{ 'VIEW', SaveLoad_VIEW, SaveLoad_VIEW, CH_RIFF},
	{ 'CHTS', Save_CHTS,     Load_CHTS,     CH_RIFF | CH_LAST}
};
