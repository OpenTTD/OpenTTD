#include "stdafx.h"
#include "ttd.h"
#include "vehicle.h"
#include "gfx.h"
#include "assert.h"
#include "saveload.h"

extern void StartupEconomy();
extern void InitNewsItemStructs();

static uint32 _random_seed_3, _random_seed_4;

byte _name_array[512][32];

static INLINE uint32 ROR(uint32 x, int n)
{
	return (x >> n) + (x << ((sizeof(x)*8)-n));
}


uint32 Random()
{
	uint32 t = _random_seed_2;
	uint32 s = _random_seed_1;
	_random_seed_1 = s + ROR(t ^ 0x1234567F, 7);
	return _random_seed_2 = ROR(s, 3);
}

uint RandomRange(uint max)
{
	return (uint16)Random() * max >> 16;
}

uint32 InteractiveRandom()
{
	uint32 t = _random_seed_4;
	uint32 s = _random_seed_3;
	_random_seed_3 = s + ROR(t ^ 0x1234567F, 7);
	return _random_seed_4 = ROR(s, 3);
}

void memswap(void *a, void *b, size_t size) {
	void *c = alloca(size);
	memcpy(c, a, size);
	memcpy(a, b, size);
	memcpy(b, c, size);
}

void SetDate(uint date)
{
	YearMonthDay ymd;
	ConvertDayToYMD(&ymd, _date = date);
	_cur_year = ymd.year;
	_cur_month = ymd.month;
}

void InitializeClearLand();
void InitializeRail();
void InitializeRailGui();
void InitializeRoad();
void InitializeRoadGui();
void InitializeAirportGui();
void InitializeDock();
void InitializeDockGui();
void InitializeIndustries();
void InitializeLandscape();
void InitializeTowns();
void InitializeTrees();
void InitializeStations();
void InitializeNameMgr();
void InitializePlayers();
void InitializeCheats();

void GenerateLandscape();
void GenerateClearTile();

void GenerateIndustries();
void GenerateUnmovables();
void GenerateTowns();

void StartupPlayers();
void StartupEngines();
void StartupDisasters();
void GenerateTrees();

void ConvertGroundTilesIntoWaterTiles();

void InitializeGame()
{
	SetObjectToPlace(1, 0, 0, 0);

	_pause = 0;
	_fast_forward = 0;
	_tick_counter = 0;
	_date_fract = 0;
	_cur_tileloop_tile = 0;
	_vehicle_id_ctr_day = 0;

	{
		uint starting = ConvertIntDate(_patches.starting_date);
		if ( starting == (uint)-1) starting = 10958;
		SetDate(starting);
	}

	InitializeVehicles();
	_backup_orders_tile = 0;

	InitNewsItemStructs();
	InitializeLandscape();
	InitializeClearLand();
	InitializeRail();
	InitializeRailGui();
	InitializeRoad();
	InitializeRoadGui();
	InitializeAirportGui();
	InitializeDock();
	InitializeDockGui();
	InitializeTowns();
	InitializeTrees();
	InitializeStations();
	InitializeIndustries();

	InitializeNameMgr();
	InitializeVehiclesGuiList();
	InitializeTrains();

	InitializePlayers();
	InitializeCheats();

	InitTextEffects();
	InitializeAnimatedTiles();

	InitializeLandscapeVariables(false);

	ResetObjectToPlace();
}

void GenerateWorld(int mode)
{
	int i;

	_generating_world = true;
	InitializeGame();
	SetObjectToPlace(1, 0, 0, 0);

	// Must start economy early because of the costs.
	StartupEconomy();

	// Don't generate landscape items when in the scenario editor.
	if (mode == 1) {
		// empty world in scenario editor
		ConvertGroundTilesIntoWaterTiles();
	} else {
		GenerateLandscape();
		GenerateClearTile();

		// only generate towns, tree and industries in newgame mode.
		if (mode == 0) {
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
	if (mode != 1) {
		for(i=0x500; i!=0; i--)
			RunTileLoop();
	}

	ResetObjectToPlace();
}

void DeleteName(StringID id)
{
	if ((id & 0xF800) == 0x7800) {
		memset(_name_array[id & 0x1FF], 0, sizeof(_name_array[id & 0x1FF]));
	}
}

byte *GetName(int id, byte *buff)
{
	byte *b;

	if (id & 0x600) {
		if (id & 0x200) {
			if (id & 0x400) {
				GetParamInt32();
				GetParamUint16();
			} else {
				GetParamUint16();
			}
		} else {
			GetParamInt32();
		}
	}

	b = _name_array[(id&~0x600)];
	while ((*buff++ = *b++) != 0);

	return buff - 1;
}


void InitializeCheats()
{
	memset(&_cheats, 0, sizeof(Cheats));
}


void InitializeNameMgr()
{
	memset(_name_array, 0, sizeof(_name_array));
}

StringID AllocateName(const byte *name, byte skip)
{
	int free_item = -1;
	const byte *names;
	byte *dst;
	int i;

	names = &_name_array[0][0];

	for(i=0; i!=512; i++,names+=sizeof(_name_array[0])) {
		if (names[0] == 0) {
			if (free_item == -1)
				free_item = i;
		} else {
			if (str_eq(names, name)) {
				_error_message = STR_0132_CHOSEN_NAME_IN_USE_ALREADY;
				return 0;
			}
		}
	}

	if (free_item < 0) {
		_error_message = STR_0131_TOO_MANY_NAMES_DEFINED;
		return 0;
	}

	dst=_name_array[free_item];

	for(i=0; (dst[i] = name[i]) != 0 && ++i != 32; ) {}
	dst[31] = 0;

	return free_item | 0x7800 | (skip << 8);
}

const TileIndexDiff _tileoffs_by_dir[4] = {
	TILE_XY(-1, 0),
	TILE_XY(0, 1),
	TILE_XY(1, 0),
	TILE_XY(0, -1),
};


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
// 1920 - 2090
// 192001 - 209012
// 19200101 - 20901231
// or if > 2090 and below 65536, treat it as a daycount
// returns -1 if no conversion was possible
uint ConvertIntDate(uint date)
{
	uint year, month = 0, day = 1;

	if (IS_INT_INSIDE(date, 1920, 2090 + 1)) {
		year = date - 1920;
	} else if (IS_INT_INSIDE(date, 192001, 209012+1)) {
		month = date % 100 - 1;
		year = date / 100 - 1920;
	} else if (IS_INT_INSIDE(date, 19200101, 20901231+1)) {
		day = date % 100; date /= 100;
		month = date % 100 - 1;
		year = date / 100 - 1920;
	} else if (IS_INT_INSIDE(date, 2091, 65536))
		return date;
	else
		return (uint)-1;

	// invalid ranges?
	if (month >= 12 || !IS_INT_INSIDE(day, 1, 31+1)) return (uint)-1;

	return ConvertYMDToDay(year, month, day);
}

typedef struct LandscapePredefVar {
	StringID names[NUM_CARGO];
	byte weights[NUM_CARGO];
	StringID sprites[NUM_CARGO];

	uint16 initial_cargo_payment[NUM_CARGO];
	byte transit_days_table_1[NUM_CARGO];
	byte transit_days_table_2[NUM_CARGO];

	byte railwagon_by_cargo[3][NUM_CARGO];

	byte road_veh_by_cargo_start[NUM_CARGO];
	byte road_veh_by_cargo_count[NUM_CARGO];
} LandscapePredefVar;

#include "table/landscape_const.h"


// Calculate constants that depend on the landscape type.
void InitializeLandscapeVariables(bool only_constants)
{
	const LandscapePredefVar *lpd;
	int i;
	StringID str;

	lpd = &_landscape_predef_var[_opt.landscape];

	memcpy(_cargoc.ai_railwagon, lpd->railwagon_by_cargo, sizeof(lpd->railwagon_by_cargo));
	memcpy(_cargoc.ai_roadveh_start, lpd->road_veh_by_cargo_start,sizeof(lpd->road_veh_by_cargo_start));
	memcpy(_cargoc.ai_roadveh_count, lpd->road_veh_by_cargo_count,sizeof(lpd->road_veh_by_cargo_count));

	for(i=0; i!=NUM_CARGO; i++) {
		_cargoc.sprites[i] = lpd->sprites[i];

		str = lpd->names[i];
		_cargoc.names_s[i] = str;
		_cargoc.names_p[i] = (str += 0x20);
		_cargoc.names_long_s[i] = (str += 0x20);
		_cargoc.names_long_p[i] = (str += 0x20);
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

// distance in Manhattan metric
uint GetTileDist(TileIndex xy1, TileIndex xy2)
{
	return myabs(GET_TILE_X(xy1) - GET_TILE_X(xy2)) +
				myabs(GET_TILE_Y(xy1) - GET_TILE_Y(xy2));
}

// maximum distance in x _or_ y
uint GetTileDist1D(TileIndex xy1, TileIndex xy2)
{
	return max(myabs(GET_TILE_X(xy1) - GET_TILE_X(xy2)),
						 myabs(GET_TILE_Y(xy1) - GET_TILE_Y(xy2)));
}

uint GetTileDist1Db(TileIndex xy1, TileIndex xy2)
{
	int a = myabs(GET_TILE_X(xy1) - GET_TILE_X(xy2));
	int b = myabs(GET_TILE_Y(xy1) - GET_TILE_Y(xy2));

	if (a > b)
		return a*2+b;
	else
		return b*2+a;
}

uint GetTileDistAdv(TileIndex xy1, TileIndex xy2)
{
	uint a = myabs(GET_TILE_X(xy1) - GET_TILE_X(xy2));
	uint b = myabs(GET_TILE_Y(xy1) - GET_TILE_Y(xy2));
	return a*a+b*b;
}

bool CheckDistanceFromEdge(TileIndex tile, uint distance)
{
	return IS_INT_INSIDE(GET_TILE_X(tile), distance, TILE_X_MAX + 1 - distance) &&
			IS_INT_INSIDE(GET_TILE_Y(tile), distance, TILE_Y_MAX + 1 - distance);
}

void OnNewDay_Train(Vehicle *v);
void OnNewDay_RoadVeh(Vehicle *v);
void OnNewDay_Aircraft(Vehicle *v);
void OnNewDay_Ship(Vehicle *v);
void OnNewDay_EffectVehicle(Vehicle *v) { /* empty */ }
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

void EnginesDailyLoop();
void DisasterDailyLoop();
void PlayersMonthlyLoop();
void EnginesMonthlyLoop();
void TownsMonthlyLoop();
void IndustryMonthlyLoop();
void StationMonthlyLoop();

void PlayersYearlyLoop();
void TrainsYearlyLoop();
void RoadVehiclesYearlyLoop();
void AircraftYearlyLoop();
void ShipsYearlyLoop();

void CheckpointsDailyLoop();


static const uint16 _autosave_months[] = {
	0, // never
	0xFFF, // every month
	0x249, // every 3 months
	0x041, // every 6 months
	0x001, // every 12 months
};

void IncreaseDate()
{
	int i,ctr,t;
	YearMonthDay ymd;

	if (_game_mode == GM_MENU) {
		_tick_counter++;
		return;
	}

	/*if the day changed, call the vehicle event but only update a part of the vehicles
		old max was i!= 12. But with that and a bigger number of vehicles (2560), per day only
		a part of it could be done, namely: function called max_size date_fract (uint16) / 885 x 12 ==>
		65536 / 885 = 74; 74x12 = 888. So max 888. Any vehicles above that were not _on_new_vehicle_day_proc'd
		eg. aged.
		So new code updates it for max vehicles.
		(NUM_VEHICLES / maximum number of times ctr is incremented before reset ) + 1 (to get last vehicles too)
		max size of _date_fract / 885 (added each tick) is number of times before ctr is reset.
		Calculation might look complicated, but compiler just replaces it with 35, so that's ok
	*/

	ctr = _vehicle_id_ctr_day;
	for(i=0; i!=(NUM_VEHICLES / ((1<<sizeof(_date_fract)*8) / 885)) + 1 && ctr != lengthof(_vehicles); i++) {
		Vehicle *v = &_vehicles[ctr++];
		if ((t=v->type) != 0)
			_on_new_vehicle_day_proc[t - 0x10](v);
	}
	_vehicle_id_ctr_day = ctr;

	/* increase day, and check if a new day is there? */
	_tick_counter++;

	if ( (_date_fract += 885) >= 885)
		return;

	/* yeah, increse day counter and call various daily loops */
	_date++;

	NetworkGameChangeDate(_date);

	_vehicle_id_ctr_day = 0;

	DisasterDailyLoop();
	CheckpointsDailyLoop();

	if (_game_mode != GM_MENU) {
		InvalidateWindowWidget(WC_STATUS_BAR, 0, 0);
		EnginesDailyLoop();
	}

	/* check if we entered a new month? */
	ConvertDayToYMD(&ymd, _date);
	if ((byte)ymd.month == _cur_month)
		return;
	_cur_month = ymd.month;

//	printf("Month %d, %X\n", ymd.month, _random_seed_1);

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

	/* check if we reached 2090, that's the maximum year. */
	if (_cur_year == 171) {
		_cur_year = 170;
		_date = 62093;
	}

	if (_patches.auto_euro)
		CheckSwitchToEuro();

	/* XXX: check if year 2050 was reached */
}

int FindFirstBit(uint32 value)
{
	// This is much faster then the one that was before here.
	//  Created by Darkvater.. blame him if it is wrong ;)
	// Btw, the macro FINDFIRSTBIT is better to use when your value is
	//  not more then 128.
	byte i = 0;
	if (value & 0xffff0000) { value >>= 16; i += 16; }
	if (value & 0x0000ff00) { value >>= 8;  i += 8; }
	if (value & 0x000000f0) { value >>= 4;  i += 4; }
	if (value & 0x0000000c) { value >>= 2;  i += 2; }
	if (value & 0x00000002) { i += 1; }
	return i;
}


extern uint SafeTileAdd(uint tile, int add, const char *exp, const char *file, int line)
{
	int x = GET_TILE_X(tile) + (signed char)(add & 0xFF);
	int y = GET_TILE_Y(tile) + ((((0x8080 + add)>>8) & 0xFF) - 0x80);

	if (x < 0 || y < 0 || x >= TILES_X || y >= TILES_Y) {
		char buf[512];

		sprintf(buf, "TILE_ADD(%s) when adding 0x%.4X and %d failed", exp, tile, add);
#if !defined(_DEBUG) || !defined(_MSC_VER)
		printf("%s\n", buf);
#else
		_assert(buf, (char*)file, line);
#endif
	}

	assert(TILE_XY(x,y) == TILE_MASK(tile + add));

	return TILE_XY(x,y);
}

static void Save_NAME()
{
	int i;
	byte *b = _name_array[0];

	for(i=0; i!=lengthof(_name_array); i++,b+=sizeof(_name_array[0])) {
		if (*b) {
			SlSetArrayIndex(i);
			SlArray(b, strlen(b), SLE_UINT8);
		}
	}
}

static void Load_NAME()
{
	int index;

	while ((index = SlIterateArray()) != -1) {
		SlArray(_name_array[index],SlGetFieldLength(),SLE_UINT8);
	}
}

static const byte _game_opt_desc[] = {
	// added a new difficulty option (town attitude) in version 4
	SLE_CONDARR(GameOptions,diff,						SLE_FILE_I16 | SLE_VAR_I32, 17, 0, 3),
	SLE_CONDARR(GameOptions,diff,						SLE_FILE_I16 | SLE_VAR_I32, 18, 4, 255),
	SLE_VAR(GameOptions,diff_level,			SLE_UINT8),
	SLE_VAR(GameOptions,currency,				SLE_UINT8),
	SLE_VAR(GameOptions,kilometers,			SLE_UINT8),
	SLE_VAR(GameOptions,town_name,			SLE_UINT8),
	SLE_VAR(GameOptions,landscape,			SLE_UINT8),
	SLE_VAR(GameOptions,snow_line,			SLE_UINT8),
	SLE_VAR(GameOptions,autosave,				SLE_UINT8),
	SLE_VAR(GameOptions,road_side,			SLE_UINT8),
	SLE_END()
};

// Save load game options
static void SaveLoad_OPTS()
{
	SlObject(&_opt, _game_opt_desc);
}


static const SaveLoadGlobVarList _date_desc[] = {
	{&_date, 										SLE_UINT16, 0, 255},
	{&_date_fract, 							SLE_UINT16, 0, 255},
	{&_tick_counter, 						SLE_UINT16, 0, 255},
	{&_vehicle_id_ctr_day, 			SLE_UINT16, 0, 255},
	{&_age_cargo_skip_counter, 	SLE_UINT8,	0, 255},
	{&_avail_aircraft, 					SLE_UINT8,	0, 255},
	{&_cur_tileloop_tile, 			SLE_UINT16, 0, 255},
	{&_disaster_delay, 					SLE_UINT16, 0, 255},
	{&_station_tick_ctr, 				SLE_UINT16, 0, 255},
	{&_random_seed_1, 					SLE_UINT32, 0, 255},
	{&_random_seed_2, 					SLE_UINT32, 0, 255},
	{&_cur_town_ctr, 						SLE_UINT8,	0, 255},
	{&_cur_player_tick_index, 	SLE_FILE_U8 | SLE_VAR_UINT, 0, 255},
	{&_next_competitor_start, 	SLE_FILE_U16 | SLE_VAR_UINT, 0, 255},
	{&_trees_tick_ctr, 					SLE_UINT8,	0, 255},
	{&_pause, 									SLE_UINT8,	4, 255},
	{NULL,											0,					0,   0}
};

// Save load date related variables as well as persistent tick counters
// XXX: currently some unrelated stuff is just put here
static void SaveLoad_DATE()
{
	SlGlobList(_date_desc);
}


static const SaveLoadGlobVarList _view_desc[] = {
	{&_saved_scrollpos_x,			SLE_FILE_I16 | SLE_VAR_INT, 0, 255},
	{&_saved_scrollpos_y,			SLE_FILE_I16 | SLE_VAR_INT, 0, 255},
	{&_saved_scrollpos_zoom,	SLE_UINT8,	0, 255},
	{NULL,										0,					0,   0}
};

static void SaveLoad_VIEW()
{
	SlGlobList(_view_desc);
}

static void SaveLoad_MAPT() {
  SlArray(_map_type_and_height, lengthof(_map_type_and_height), SLE_UINT8);
}

static void SaveLoad_MAP2() {
  SlArray(_map2, lengthof(_map2), SLE_UINT8);
}

static void SaveLoad_M3LO() {
  SlArray(_map3_lo, lengthof(_map3_lo), SLE_UINT8);
}

static void SaveLoad_M3HI() {
  SlArray(_map3_hi, lengthof(_map3_hi), SLE_UINT8);
}

static void SaveLoad_MAPO() {
  SlArray(_map_owner, lengthof(_map_owner), SLE_UINT8);
}

static void SaveLoad_MAP5() {
  SlArray(_map5, lengthof(_map5), SLE_UINT8);
}

static void SaveLoad_MAPE() {
  SlArray(_map_extra_bits, lengthof(_map_extra_bits), SLE_UINT8);
}


static void Save_CHTS()
{
	byte count = sizeof(_cheats)/sizeof(Cheat);
	Cheat* cht = (Cheat*) &_cheats;
	Cheat* cht_last = &cht[count];

	SlSetLength(count*2);
	for(; cht != cht_last; cht++) {
		SlWriteByte(cht->been_used);
		SlWriteByte(cht->value);
	}
}

static void Load_CHTS()
{
	Cheat* cht = (Cheat*) &_cheats;

	uint count = SlGetFieldLength()/2;
	for(; count; count--, cht++)
	{
		cht->been_used = (byte)SlReadByte();
		cht->value = (byte)SlReadByte();
	}
}


const ChunkHandler _misc_chunk_handlers[] = {
	{ 'MAPT', SaveLoad_MAPT, SaveLoad_MAPT, CH_RIFF },
	{ 'MAP2', SaveLoad_MAP2, SaveLoad_MAP2, CH_RIFF },
	{ 'M3LO', SaveLoad_M3LO, SaveLoad_M3LO, CH_RIFF },
	{ 'M3HI', SaveLoad_M3HI, SaveLoad_M3HI, CH_RIFF },
	{ 'MAPO', SaveLoad_MAPO, SaveLoad_MAPO, CH_RIFF },
	{ 'MAP5', SaveLoad_MAP5, SaveLoad_MAP5, CH_RIFF },
	{ 'MAPE', SaveLoad_MAPE, SaveLoad_MAPE, CH_RIFF },

	{ 'NAME', Save_NAME, Load_NAME, CH_ARRAY},
	{ 'DATE', SaveLoad_DATE, SaveLoad_DATE, CH_RIFF},
	{ 'VIEW', SaveLoad_VIEW, SaveLoad_VIEW, CH_RIFF},
	{ 'OPTS', SaveLoad_OPTS, SaveLoad_OPTS, CH_RIFF},
	{ 'CHTS', Save_CHTS, Load_CHTS, CH_RIFF | CH_LAST}
};
