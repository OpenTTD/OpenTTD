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
#include "network.h"
#include "network_data.h"
#include "network_server.h"
#include "engine.h"
#include "vehicle_gui.h"
#include "variables.h"
#include "ai/ai.h"
#include "table/landscape_const.h"

extern void StartupEconomy(void);

char _name_array[512][32];

#ifndef MERSENNE_TWISTER

#ifdef RANDOM_DEBUG
#include "network_data.h"
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

void SetDate(uint date)
{
	YearMonthDay ymd;

	_date = date;
	ConvertDayToYMD(&ymd, date);
	_cur_year = ymd.year;
	_cur_month = ymd.month;
#ifdef ENABLE_NETWORK
	_network_last_advertise_frame = 0;
	_network_need_advertise = true;
#endif /* ENABLE_NETWORK */
}

void InitializeVehicles(void);
void InitializeWaypoints(void);
void InitializeDepot(void);
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

void GenerateLandscape(void);
void GenerateClearTile(void);

void GenerateIndustries(void);
void GenerateUnmovables(void);
bool GenerateTowns(void);

void StartupPlayers(void);
void StartupDisasters(void);
void GenerateTrees(void);

void ConvertGroundTilesIntoWaterTiles(void);

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
		uint starting = ConvertIntDate(_patches.starting_date);
		if (starting == (uint)-1) starting = 10958;
		SetDate(starting);
	}

	InitializeEngines();
	InitializeVehicles();
	InitializeWaypoints();
	InitializeDepot();
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

void GenerateWorld(int mode, uint size_x, uint size_y)
{
	// Make sure everything is done via OWNER_NONE
	_current_player = OWNER_NONE;

	UpdatePatches();

	_generating_world = true;
	InitializeGame(mode == GW_RANDOM ? 0 : IG_DATE_RESET, size_x, size_y);
	SetObjectToPlace(SPR_CURSOR_ZZZ, 0, 0, 0);

	// Must start economy early because of the costs.
	StartupEconomy();

	// Don't generate landscape items when in the scenario editor.
	if (mode == GW_EMPTY) {
		// empty world in scenario editor
		ConvertGroundTilesIntoWaterTiles();
	} else {
		GenerateLandscape();
		GenerateClearTile();

		// only generate towns, tree and industries in newgame mode.
		if (mode == GW_NEWGAME) {
			GenerateTowns();
			GenerateTrees();
			GenerateIndustries();
			GenerateUnmovables();
		}
	}

	// These are probably pointless when inside the scenario editor.
	StartupPlayers();
	StartupEngines();
	StartupDisasters();
	_generating_world = false;

	// No need to run the tile loop in the scenario editor.
	if (mode != GW_EMPTY) {
		uint i;

		for (i = 0; i < 0x500; i++) RunTileLoop();
	}

	ResetObjectToPlace();
}

void DeleteName(StringID id)
{
	if ((id & 0xF800) == 0x7800) {
		memset(_name_array[id & 0x1FF], 0, sizeof(_name_array[id & 0x1FF]));
	}
}

char *GetName(int id, char *buff)
{
	return strecpy(buff, _name_array[id & ~0x600], NULL);
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


#define M(a,b) ((a<<5)|b)
static const uint16 _month_date_from_year_day[] = {
M(0,1),M(0,2),M(0,3),M(0,4),M(0,5),M(0,6),M(0,7),M(0,8),M(0,9),M(0,10),M(0,11),M(0,12),M(0,13),M(0,14),M(0,15),M(0,16),M(0,17),M(0,18),M(0,19),M(0,20),M(0,21),M(0,22),M(0,23),M(0,24),M(0,25),M(0,26),M(0,27),M(0,28),M(0,29),M(0,30),M(0,31),
M(1,1),M(1,2),M(1,3),M(1,4),M(1,5),M(1,6),M(1,7),M(1,8),M(1,9),M(1,10),M(1,11),M(1,12),M(1,13),M(1,14),M(1,15),M(1,16),M(1,17),M(1,18),M(1,19),M(1,20),M(1,21),M(1,22),M(1,23),M(1,24),M(1,25),M(1,26),M(1,27),M(1,28),M(1,29),
M(2,1),M(2,2),M(2,3),M(2,4),M(2,5),M(2,6),M(2,7),M(2,8),M(2,9),M(2,10),M(2,11),M(2,12),M(2,13),M(2,14),M(2,15),M(2,16),M(2,17),M(2,18),M(2,19),M(2,20),M(2,21),M(2,22),M(2,23),M(2,24),M(2,25),M(2,26),M(2,27),M(2,28),M(2,29),M(2,30),M(2,31),
M(3,1),M(3,2),M(3,3),M(3,4),M(3,5),M(3,6),M(3,7),M(3,8),M(3,9),M(3,10),M(3,11),M(3,12),M(3,13),M(3,14),M(3,15),M(3,16),M(3,17),M(3,18),M(3,19),M(3,20),M(3,21),M(3,22),M(3,23),M(3,24),M(3,25),M(3,26),M(3,27),M(3,28),M(3,29),M(3,30),
M(4,1),M(4,2),M(4,3),M(4,4),M(4,5),M(4,6),M(4,7),M(4,8),M(4,9),M(4,10),M(4,11),M(4,12),M(4,13),M(4,14),M(4,15),M(4,16),M(4,17),M(4,18),M(4,19),M(4,20),M(4,21),M(4,22),M(4,23),M(4,24),M(4,25),M(4,26),M(4,27),M(4,28),M(4,29),M(4,30),M(4,31),
M(5,1),M(5,2),M(5,3),M(5,4),M(5,5),M(5,6),M(5,7),M(5,8),M(5,9),M(5,10),M(5,11),M(5,12),M(5,13),M(5,14),M(5,15),M(5,16),M(5,17),M(5,18),M(5,19),M(5,20),M(5,21),M(5,22),M(5,23),M(5,24),M(5,25),M(5,26),M(5,27),M(5,28),M(5,29),M(5,30),
M(6,1),M(6,2),M(6,3),M(6,4),M(6,5),M(6,6),M(6,7),M(6,8),M(6,9),M(6,10),M(6,11),M(6,12),M(6,13),M(6,14),M(6,15),M(6,16),M(6,17),M(6,18),M(6,19),M(6,20),M(6,21),M(6,22),M(6,23),M(6,24),M(6,25),M(6,26),M(6,27),M(6,28),M(6,29),M(6,30),M(6,31),
M(7,1),M(7,2),M(7,3),M(7,4),M(7,5),M(7,6),M(7,7),M(7,8),M(7,9),M(7,10),M(7,11),M(7,12),M(7,13),M(7,14),M(7,15),M(7,16),M(7,17),M(7,18),M(7,19),M(7,20),M(7,21),M(7,22),M(7,23),M(7,24),M(7,25),M(7,26),M(7,27),M(7,28),M(7,29),M(7,30),M(7,31),
M(8,1),M(8,2),M(8,3),M(8,4),M(8,5),M(8,6),M(8,7),M(8,8),M(8,9),M(8,10),M(8,11),M(8,12),M(8,13),M(8,14),M(8,15),M(8,16),M(8,17),M(8,18),M(8,19),M(8,20),M(8,21),M(8,22),M(8,23),M(8,24),M(8,25),M(8,26),M(8,27),M(8,28),M(8,29),M(8,30),
M(9,1),M(9,2),M(9,3),M(9,4),M(9,5),M(9,6),M(9,7),M(9,8),M(9,9),M(9,10),M(9,11),M(9,12),M(9,13),M(9,14),M(9,15),M(9,16),M(9,17),M(9,18),M(9,19),M(9,20),M(9,21),M(9,22),M(9,23),M(9,24),M(9,25),M(9,26),M(9,27),M(9,28),M(9,29),M(9,30),M(9,31),
M(10,1),M(10,2),M(10,3),M(10,4),M(10,5),M(10,6),M(10,7),M(10,8),M(10,9),M(10,10),M(10,11),M(10,12),M(10,13),M(10,14),M(10,15),M(10,16),M(10,17),M(10,18),M(10,19),M(10,20),M(10,21),M(10,22),M(10,23),M(10,24),M(10,25),M(10,26),M(10,27),M(10,28),M(10,29),M(10,30),
M(11,1),M(11,2),M(11,3),M(11,4),M(11,5),M(11,6),M(11,7),M(11,8),M(11,9),M(11,10),M(11,11),M(11,12),M(11,13),M(11,14),M(11,15),M(11,16),M(11,17),M(11,18),M(11,19),M(11,20),M(11,21),M(11,22),M(11,23),M(11,24),M(11,25),M(11,26),M(11,27),M(11,28),M(11,29),M(11,30),M(11,31),
};
#undef M

enum {
	ACCUM_JAN = 0,
	ACCUM_FEB = ACCUM_JAN + 31,
	ACCUM_MAR = ACCUM_FEB + 29,
	ACCUM_APR = ACCUM_MAR + 31,
	ACCUM_MAY = ACCUM_APR + 30,
	ACCUM_JUN = ACCUM_MAY + 31,
	ACCUM_JUL = ACCUM_JUN + 30,
	ACCUM_AUG = ACCUM_JUL + 31,
	ACCUM_SEP = ACCUM_AUG + 31,
	ACCUM_OCT = ACCUM_SEP + 30,
	ACCUM_NOV = ACCUM_OCT + 31,
	ACCUM_DEC = ACCUM_NOV + 30,
};

static const uint16 _accum_days_for_month[] = {
	ACCUM_JAN,ACCUM_FEB,ACCUM_MAR,ACCUM_APR,
	ACCUM_MAY,ACCUM_JUN,ACCUM_JUL,ACCUM_AUG,
	ACCUM_SEP,ACCUM_OCT,ACCUM_NOV,ACCUM_DEC,
};


void ConvertDayToYMD(YearMonthDay *ymd, uint16 date)
{
	uint yr = date / (365+365+365+366);
	uint rem = date % (365+365+365+366);
	uint x;

	yr *= 4;

	if (rem >= 366) {
		rem--;
		do {
			rem -= 365;
			yr++;
		} while (rem >= 365);
		if (rem >= 31+28) rem++;
	}

	ymd->year = yr;

	x = _month_date_from_year_day[rem];
	ymd->month = x >> 5;
	ymd->day = x & 0x1F;
}

// year is a number between 0..?
// month is a number between 0..11
// day is a number between 1..31
uint ConvertYMDToDay(uint year, uint month, uint day)
{
	uint rem;

	// day in the year
	rem = _accum_days_for_month[month] + day - 1;

	// remove feb 29 from year 1,2,3
	if (year & 3) rem += (year & 3) * 365 + (rem < 31+29);

	// base date.
	return (year >> 2) * (365+365+365+366) + rem;
}

// convert a date on the form
// 1920 - 2090 (MAX_YEAR_END_REAL)
// 192001 - 209012
// 19200101 - 20901231
// or if > 2090 and below 65536, treat it as a daycount
// returns -1 if no conversion was possible
uint ConvertIntDate(uint date)
{
	uint year, month = 0, day = 1;

	if (IS_INT_INSIDE(date, 1920, MAX_YEAR_END_REAL + 1)) {
		year = date - 1920;
	} else if (IS_INT_INSIDE(date, 192001, 209012+1)) {
		month = date % 100 - 1;
		year = date / 100 - 1920;
	} else if (IS_INT_INSIDE(date, 19200101, 20901231+1)) {
		day = date % 100; date /= 100;
		month = date % 100 - 1;
		year = date / 100 - 1920;
	} else if (IS_INT_INSIDE(date, 2091, 65536)) {
		return date;
	} else {
		return (uint)-1;
	}

	// invalid ranges?
	if (month >= 12 || !IS_INT_INSIDE(day, 1, 31+1)) return (uint)-1;

	return ConvertYMDToDay(year, month, day);
}


// Calculate constants that depend on the landscape type.
void InitializeLandscapeVariables(bool only_constants)
{
	const CargoTypesValues *lpd;
	uint i;
	StringID str;

	lpd = &_cargo_types_base_values[_opt.landscape];

	memcpy(_cargoc.ai_roadveh_start, lpd->road_veh_by_cargo_start,sizeof(lpd->road_veh_by_cargo_start));
	memcpy(_cargoc.ai_roadveh_count, lpd->road_veh_by_cargo_count,sizeof(lpd->road_veh_by_cargo_count));

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


void OnNewDay_Train(Vehicle *v);
void OnNewDay_RoadVeh(Vehicle *v);
void OnNewDay_Aircraft(Vehicle *v);
void OnNewDay_Ship(Vehicle *v);
static void OnNewDay_EffectVehicle(Vehicle *v) { /* empty */ }
void OnNewDay_DisasterVehicle(Vehicle *v);

typedef void OnNewVehicleDayProc(Vehicle *v);

static OnNewVehicleDayProc * _on_new_vehicle_day_proc[] = {
	OnNewDay_Train,
	OnNewDay_RoadVeh,
	OnNewDay_Ship,
	OnNewDay_Aircraft,
	OnNewDay_EffectVehicle,
	OnNewDay_DisasterVehicle,
};

void EnginesDailyLoop(void);
void DisasterDailyLoop(void);
void PlayersMonthlyLoop(void);
void EnginesMonthlyLoop(void);
void TownsMonthlyLoop(void);
void IndustryMonthlyLoop(void);
void StationMonthlyLoop(void);

void PlayersYearlyLoop(void);
void TrainsYearlyLoop(void);
void RoadVehiclesYearlyLoop(void);
void AircraftYearlyLoop(void);
void ShipsYearlyLoop(void);

void WaypointsDailyLoop(void);


static const uint16 _autosave_months[] = {
	0, // never
	0xFFF, // every month
	0x249, // every 3 months
	0x041, // every 6 months
	0x001, // every 12 months
};

/**
 * Runs the day_proc for every DAY_TICKS vehicle starting at daytick.
 */
static void RunVehicleDayProc(uint daytick)
{
	uint total = _vehicle_pool.total_items;
	uint i;

	for (i = daytick; i < total; i += DAY_TICKS) {
		Vehicle* v = GetVehicle(i);

		if (v->type != 0) _on_new_vehicle_day_proc[v->type - 0x10](v);
	}
}

void IncreaseDate(void)
{
	YearMonthDay ymd;

	if (_game_mode == GM_MENU) {
		_tick_counter++;
		return;
	}

	RunVehicleDayProc(_date_fract);

	/* increase day, and check if a new day is there? */
	_tick_counter++;

	_date_fract++;
	if (_date_fract < DAY_TICKS) return;
	_date_fract = 0;

	/* yeah, increse day counter and call various daily loops */
	_date++;

	TextMessageDailyLoop();

	DisasterDailyLoop();
	WaypointsDailyLoop();

	if (_game_mode != GM_MENU) {
		InvalidateWindowWidget(WC_STATUS_BAR, 0, 0);
		EnginesDailyLoop();
	}

	/* check if we entered a new month? */
	ConvertDayToYMD(&ymd, _date);
	if ((byte)ymd.month == _cur_month)
		return;
	_cur_month = ymd.month;

	/* yes, call various monthly loops */
	if (_game_mode != GM_MENU) {
		if (HASBIT(_autosave_months[_opt.autosave], _cur_month)) {
			_do_autosave = true;
			RedrawAutosave();
		}

		PlayersMonthlyLoop();
		EnginesMonthlyLoop();
		TownsMonthlyLoop();
		IndustryMonthlyLoop();
		StationMonthlyLoop();
#ifdef ENABLE_NETWORK
		if (_network_server) NetworkServerMonthlyLoop();
#endif /* ENABLE_NETWORK */
	}

	/* check if we entered a new year? */
	if ((byte)ymd.year == _cur_year)
		return;
	_cur_year = ymd.year;

	/* yes, call various yearly loops */

	PlayersYearlyLoop();
	TrainsYearlyLoop();
	RoadVehiclesYearlyLoop();
	AircraftYearlyLoop();
	ShipsYearlyLoop();
#ifdef ENABLE_NETWORK
	if (_network_server) NetworkServerYearlyLoop();
#endif /* ENABLE_NETWORK */

	/* check if we reached end of the game (31 dec 2050) */
	if (_cur_year == _patches.ending_date - MAX_YEAR_BEGIN_REAL) {
			ShowEndGameChart();
	/* check if we reached 2090 (MAX_YEAR_END_REAL), that's the maximum year. */
	} else if (_cur_year == (MAX_YEAR_END + 1)) {
		Vehicle* v;

		_cur_year = MAX_YEAR_END;
		_date = 62093;
		FOR_ALL_VEHICLES(v) {
			v->date_of_last_service -= 365; // 1 year is 365 days long
		}

		/* Because the _date wraps here, and text-messages expire by game-days, we have to clean out
		 *  all of them if the date is set back, else those messages will hang for ever */
		InitTextMessage();
	}

	if (_patches.auto_euro) CheckSwitchToEuro();

	/* XXX: check if year 2050 was reached */
}

int FindFirstBit(uint32 value)
{
	// This is much faster than the one that was before here.
	//  Created by Darkvater.. blame him if it is wrong ;)
	// Btw, the macro FINDFIRSTBIT is better to use when your value is
	//  not more than 128.
	byte i = 0;
	if (value & 0xffff0000) { value >>= 16; i += 16; }
	if (value & 0x0000ff00) { value >>= 8;  i +=  8; }
	if (value & 0x000000f0) { value >>= 4;  i +=  4; }
	if (value & 0x0000000c) { value >>= 2;  i +=  2; }
	if (value & 0x00000002) { i += 1; }
	return i;
}


static void Save_NAME(void)
{
	int i;

	for (i = 0; i != lengthof(_name_array); ++i) {
		if (_name_array[i][0] != '\0') {
			SlSetArrayIndex(i);
			SlArray(_name_array[i], strlen(_name_array[i]), SLE_UINT8);
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
	    SLEG_VAR(_date,                  SLE_UINT16),
	    SLEG_VAR(_date_fract,            SLE_UINT16),
	    SLEG_VAR(_tick_counter,          SLE_UINT16),
	    SLEG_VAR(_vehicle_id_ctr_day,    SLE_UINT16),
	    SLEG_VAR(_age_cargo_skip_counter,SLE_UINT8),
	    SLEG_VAR(_avail_aircraft,        SLE_UINT8),
	SLEG_CONDVAR(_cur_tileloop_tile,     SLE_FILE_U16 | SLE_VAR_U32, 0, 5),
	SLEG_CONDVAR(_cur_tileloop_tile,     SLE_UINT32,                 6, SL_MAX_VERSION),
	    SLEG_VAR(_disaster_delay,        SLE_UINT16),
	    SLEG_VAR(_station_tick_ctr,      SLE_UINT16),
	    SLEG_VAR(_random_seeds[0][0],    SLE_UINT32),
	    SLEG_VAR(_random_seeds[0][1],    SLE_UINT32),
	SLEG_CONDVAR(_cur_town_ctr,          SLE_FILE_U8 | SLE_VAR_U32,  0, 9),
	SLEG_CONDVAR(_cur_town_ctr,          SLE_UINT32,                10, SL_MAX_VERSION),
	    SLEG_VAR(_cur_player_tick_index, SLE_FILE_U8  | SLE_VAR_U32),
	    SLEG_VAR(_next_competitor_start, SLE_FILE_U16 | SLE_VAR_U32),
	    SLEG_VAR(_trees_tick_ctr,        SLE_UINT8),
	SLEG_CONDVAR(_pause,                 SLE_UINT8,   4, SL_MAX_VERSION),
	SLEG_CONDVAR(_cur_town_iter,         SLE_UINT32, 11, SL_MAX_VERSION),
	    SLEG_END()
};

// Save load date related variables as well as persistent tick counters
// XXX: currently some unrelated stuff is just put here
static void SaveLoad_DATE(void)
{
	SlGlobList(_date_desc);
}


static const SaveLoadGlobVarList _view_desc[] = {
	SLEG_CONDVAR(_saved_scrollpos_x,   SLE_FILE_I16 | SLE_VAR_I32, 0, 5),
	SLEG_CONDVAR(_saved_scrollpos_x,   SLE_INT32,                  6, SL_MAX_VERSION),
	SLEG_CONDVAR(_saved_scrollpos_y,   SLE_FILE_I16 | SLE_VAR_I32, 0, 5),
	SLEG_CONDVAR(_saved_scrollpos_y,   SLE_INT32,                  6, SL_MAX_VERSION),
	    SLEG_VAR(_saved_scrollpos_zoom,SLE_UINT8),
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

static void Load_MAPE(void)
{
	uint size = MapSize();
	uint i;

	for (i = 0; i != size;) {
		uint8 buf[1024];
		uint j;

		SlArray(buf, lengthof(buf), SLE_UINT8);
		for (j = 0; j != lengthof(buf); j++) {
			_m[i++].extra = GB(buf[j], 0, 2);
			_m[i++].extra = GB(buf[j], 2, 2);
			_m[i++].extra = GB(buf[j], 4, 2);
			_m[i++].extra = GB(buf[j], 6, 2);
		}
	}
}

static void Save_MAPE(void)
{
	uint size = MapSize();
	uint i;

	SlSetLength(size / 4);
	for (i = 0; i != size;) {
		uint8 buf[1024];
		uint j;

		for (j = 0; j != lengthof(buf); j++) {
			buf[j]  = _m[i++].extra << 0;
			buf[j] |= _m[i++].extra << 2;
			buf[j] |= _m[i++].extra << 4;
			buf[j] |= _m[i++].extra << 6;
		}
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
		cht[i].been_used = SlReadByte();
		cht[i].value     = SlReadByte();
	}
}


const ChunkHandler _misc_chunk_handlers[] = {
	{ 'MAPS', Save_MAPS, Load_MAPS, CH_RIFF },
	{ 'MAPT', Save_MAPT, Load_MAPT, CH_RIFF },
	{ 'MAPO', Save_MAP1, Load_MAP1, CH_RIFF },
	{ 'MAP2', Save_MAP2, Load_MAP2, CH_RIFF },
	{ 'M3LO', Save_MAP3, Load_MAP3, CH_RIFF },
	{ 'M3HI', Save_MAP4, Load_MAP4, CH_RIFF },
	{ 'MAP5', Save_MAP5, Load_MAP5, CH_RIFF },
	{ 'MAPE', Save_MAPE, Load_MAPE, CH_RIFF },

	{ 'NAME', Save_NAME, Load_NAME, CH_ARRAY},
	{ 'DATE', SaveLoad_DATE, SaveLoad_DATE, CH_RIFF},
	{ 'VIEW', SaveLoad_VIEW, SaveLoad_VIEW, CH_RIFF},
	{ 'CHTS', Save_CHTS, Load_CHTS, CH_RIFF | CH_LAST}
};
