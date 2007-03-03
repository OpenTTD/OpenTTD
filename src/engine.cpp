/* $Id$ */

/** @file engine.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "table/strings.h"
#include "engine.h"
#include "gfx.h"
#include "player.h"
#include "command.h"
#include "vehicle.h"
#include "news.h"
#include "saveload.h"
#include "variables.h"
#include "train.h"
#include "aircraft.h"
#include "newgrf_cargo.h"
#include "date.h"
#include "table/engines.h"

EngineInfo _engine_info[TOTAL_NUM_ENGINES];
RailVehicleInfo _rail_vehicle_info[NUM_TRAIN_ENGINES];
ShipVehicleInfo _ship_vehicle_info[NUM_SHIP_ENGINES];
AircraftVehicleInfo _aircraft_vehicle_info[NUM_AIRCRAFT_ENGINES];
RoadVehicleInfo _road_vehicle_info[NUM_ROAD_ENGINES];

enum {
	YEAR_ENGINE_AGING_STOPS = 2050,
};


void ShowEnginePreviewWindow(EngineID engine);

void DeleteCustomEngineNames(void)
{
	uint i;
	StringID old;

	for (i = 0; i != TOTAL_NUM_ENGINES; i++) {
		old = _engine_name_strings[i];
		_engine_name_strings[i] = i + STR_8000_KIRBY_PAUL_TANK_STEAM;
		DeleteName(old);
	}

	_vehicle_design_names &= ~1;
}

void LoadCustomEngineNames(void)
{
	/* XXX: not done */
	DEBUG(misc, 1, "LoadCustomEngineNames: not done");
}

static void SetupEngineNames(void)
{
	StringID *name;

	for (name = _engine_name_strings; name != endof(_engine_name_strings); name++)
		*name = STR_SV_EMPTY;

	DeleteCustomEngineNames();
	LoadCustomEngineNames();
}


static void CalcEngineReliability(Engine *e)
{
	uint age = e->age;

	if (age < e->duration_phase_1) {
		uint start = e->reliability_start;
		e->reliability = age * (e->reliability_max - start) / e->duration_phase_1 + start;
	} else if ((age -= e->duration_phase_1) < e->duration_phase_2 || _patches.never_expire_vehicles) {
		/* We are at the peak of this engines life. It will have max reliability.
		 * This is also true if the engines never expire. They will not go bad over time */
		e->reliability = e->reliability_max;
	} else if ((age -= e->duration_phase_2) < e->duration_phase_3) {
		uint max = e->reliability_max;
		e->reliability = (int)age * (int)(e->reliability_final - max) / e->duration_phase_3 + max;
	} else {
		/* time's up for this engine.
		 * We will now completely retire this design */
		e->player_avail = 0;
		e->reliability = e->reliability_final;
		/* Kick this engine out of the lists */
		AddRemoveEngineFromAutoreplaceAndBuildWindows(e->type);
	}
	InvalidateWindowClasses(WC_BUILD_VEHICLE); // Update to show the new reliability
	InvalidateWindowClasses(WC_REPLACE_VEHICLE);
}

void AddTypeToEngines(void)
{
	Engine* e = _engines;

	do e->type = VEH_Train;    while (++e < &_engines[ROAD_ENGINES_INDEX]);
	do e->type = VEH_Road;     while (++e < &_engines[SHIP_ENGINES_INDEX]);
	do e->type = VEH_Ship;     while (++e < &_engines[AIRCRAFT_ENGINES_INDEX]);
	do e->type = VEH_Aircraft; while (++e < &_engines[TOTAL_NUM_ENGINES]);
}

void StartupEngines(void)
{
	Engine *e;
	const EngineInfo *ei;
	/* Aging of vehicles stops, so account for that when starting late */
	const Date aging_date = min(_date, ConvertYMDToDate(YEAR_ENGINE_AGING_STOPS, 0, 1));

	SetupEngineNames();

	for (e = _engines, ei = _engine_info; e != endof(_engines); e++, ei++) {
		uint32 r;

		e->age = 0;
		e->flags = 0;
		e->player_avail = 0;

		/* The magic value of 729 days below comes from the NewGRF spec. If the
		 * base intro date is before 1922 then the random number of days is not
		 * added. */
		r = Random();
		e->intro_date = ei->base_intro <= ConvertYMDToDate(1922, 0, 1) ? ei->base_intro : (Date)GB(r, 0, 9) + ei->base_intro;
		if (e->intro_date <= _date) {
			e->age = (aging_date - e->intro_date) >> 5;
			e->player_avail = (byte)-1;
			e->flags |= ENGINE_AVAILABLE;
		}

		e->reliability_start = GB(r, 16, 14) + 0x7AE0;
		r = Random();
		e->reliability_max   = GB(r,  0, 14) + 0xBFFF;
		e->reliability_final = GB(r, 16, 14) + 0x3FFF;

		r = Random();
		e->duration_phase_1 = GB(r, 0, 5) + 7;
		e->duration_phase_2 = GB(r, 5, 4) + ei->base_life * 12 - 96;
		e->duration_phase_3 = GB(r, 9, 7) + 120;

		e->reliability_spd_dec = (ei->unk2&0x7F) << 2;

		/* my invented flag for something that is a wagon */
		if (ei->unk2 & 0x80) {
			e->age = 0xFFFF;
		} else {
			CalcEngineReliability(e);
		}

		e->lifelength = ei->lifelength + _patches.extend_vehicle_life;

		/* prevent certain engines from ever appearing. */
		if (!HASBIT(ei->climates, _opt.landscape)) {
			e->flags |= ENGINE_AVAILABLE;
			e->player_avail = 0;
		}

		/* This sets up type for the engine
		 * It is needed if you want to ask the engine what type it is
		 * It should hopefully be the same as when you ask a vehicle what it is
		 * but using this, you can ask what type an engine number is
		 * even if it is not a vehicle (yet)*/
	}
}

static void AcceptEnginePreview(EngineID eid, PlayerID player)
{
	Engine *e = GetEngine(eid);

	SETBIT(e->player_avail, player);
	if (e->type == VEH_Train) {
		const RailVehicleInfo *rvi = RailVehInfo(eid);
		Player *p = GetPlayer(player);

		assert(rvi->railtype < RAILTYPE_END);
		SETBIT(p->avail_railtypes, rvi->railtype);
	}

	e->preview_player = INVALID_PLAYER;
	if (player == _local_player) {
		AddRemoveEngineFromAutoreplaceAndBuildWindows(e->type);
	}
}

static PlayerID GetBestPlayer(PlayerID pp)
{
	const Player *p;
	int32 best_hist;
	PlayerID best_player;
	uint mask = 0;

	do {
		best_hist = -1;
		best_player = PLAYER_SPECTATOR;
		FOR_ALL_PLAYERS(p) {
			if (p->is_active && p->block_preview == 0 && !HASBIT(mask, p->index) &&
					p->old_economy[0].performance_history > best_hist) {
				best_hist = p->old_economy[0].performance_history;
				best_player = p->index;
			}
		}

		if (best_player == PLAYER_SPECTATOR) return PLAYER_SPECTATOR;

		SETBIT(mask, best_player);
	} while (pp--, pp != 0);

	return best_player;
}

void EnginesDailyLoop(void)
{
	EngineID i;

	if (_cur_year >= YEAR_ENGINE_AGING_STOPS) return;

	for (i = 0; i != lengthof(_engines); i++) {
		Engine *e = &_engines[i];

		if (e->flags & ENGINE_EXCLUSIVE_PREVIEW) {
			if (e->flags & ENGINE_OFFER_WINDOW_OPEN) {
				if (e->preview_player != 0xFF && !--e->preview_wait) {
					e->flags &= ~ENGINE_OFFER_WINDOW_OPEN;
					DeleteWindowById(WC_ENGINE_PREVIEW, i);
					e->preview_player++;
				}
			} else if (e->preview_player != 0xFF) {
				PlayerID best_player = GetBestPlayer(e->preview_player);

				if (best_player == PLAYER_SPECTATOR) {
					e->preview_player = INVALID_PLAYER;
					continue;
				}

				if (!IsHumanPlayer(best_player)) {
					/* XXX - TTDBUG: TTD has a bug here ???? */
					AcceptEnginePreview(i, best_player);
				} else {
					e->flags |= ENGINE_OFFER_WINDOW_OPEN;
					e->preview_wait = 20;
					if (IsInteractivePlayer(best_player)) ShowEnginePreviewWindow(i);
				}
			}
		}
	}
}

/** Accept an engine prototype. XXX - it is possible that the top-player
 * changes while you are waiting to accept the offer? Then it becomes invalid
 * @param tile unused
 * @param p1 engine-prototype offered
 * @param p2 unused
 */
int32 CmdWantEnginePreview(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	Engine *e;

	if (!IsEngineIndex(p1)) return CMD_ERROR;
	e = GetEngine(p1);
	if (GetBestPlayer(e->preview_player) != _current_player) return CMD_ERROR;

	if (flags & DC_EXEC) AcceptEnginePreview(p1, _current_player);

	return 0;
}

/* Determine if an engine type is a wagon (and not a loco) */
static bool IsWagon(EngineID index)
{
	return index < NUM_TRAIN_ENGINES && RailVehInfo(index)->railveh_type == RAILVEH_WAGON;
}

static void NewVehicleAvailable(Engine *e)
{
	Vehicle *v;
	Player *p;
	EngineID index = e - _engines;

	/* In case the player didn't build the vehicle during the intro period,
	 * prevent that player from getting future intro periods for a while. */
	if (e->flags & ENGINE_EXCLUSIVE_PREVIEW) {
		FOR_ALL_PLAYERS(p) {
			uint block_preview = p->block_preview;

			if (!HASBIT(e->player_avail, p->index)) continue;

			/* We assume the user did NOT build it.. prove me wrong ;) */
			p->block_preview = 20;

			FOR_ALL_VEHICLES(v) {
				if (v->type == VEH_Train || v->type == VEH_Road || v->type == VEH_Ship ||
						(v->type == VEH_Aircraft && IsNormalAircraft(v))) {
					if (v->owner == p->index && v->engine_type == index) {
						/* The user did prove me wrong, so restore old value */
						p->block_preview = block_preview;
						break;
					}
				}
			}
		}
	}

	e->flags = (e->flags & ~ENGINE_EXCLUSIVE_PREVIEW) | ENGINE_AVAILABLE;
	AddRemoveEngineFromAutoreplaceAndBuildWindows(e->type);

	/* Now available for all players */
	e->player_avail = (byte)-1;

	/* Do not introduce new rail wagons */
	if (IsWagon(index)) return;

	if (index < NUM_TRAIN_ENGINES) {
		/* maybe make another rail type available */
		RailType railtype = RailVehInfo(index)->railtype;
		assert(railtype < RAILTYPE_END);
		FOR_ALL_PLAYERS(p) {
			if (p->is_active) SETBIT(p->avail_railtypes, railtype);
		}
	}
	AddNewsItem(index, NEWS_FLAGS(NM_CALLBACK, 0, NT_NEW_VEHICLES, DNC_VEHICLEAVAIL), 0, 0);
}

void EnginesMonthlyLoop(void)
{
	Engine *e;

	if (_cur_year < YEAR_ENGINE_AGING_STOPS) {
		for (e = _engines; e != endof(_engines); e++) {
			/* Age the vehicle */
			if (e->flags & ENGINE_AVAILABLE && e->age != 0xFFFF) {
				e->age++;
				CalcEngineReliability(e);
			}

			if (!(e->flags & ENGINE_AVAILABLE) && _date >= (e->intro_date + 365)) {
				/* Introduce it to all players */
				NewVehicleAvailable(e);
			} else if (!(e->flags & (ENGINE_AVAILABLE|ENGINE_EXCLUSIVE_PREVIEW)) && _date >= e->intro_date) {
				/* Introduction date has passed.. show introducing dialog to one player. */
				e->flags |= ENGINE_EXCLUSIVE_PREVIEW;

				/* Do not introduce new rail wagons */
				if (!IsWagon(e - _engines))
					e->preview_player = (PlayerID)1; // Give to the player with the highest rating.
			}
		}
	}
}

/** Rename an engine.
 * @param tile unused
 * @param p1 engine ID to rename
 * @param p2 unused
 */
int32 CmdRenameEngine(TileIndex tile, uint32 flags, uint32 p1, uint32 p2)
{
	StringID str;

	if (!IsEngineIndex(p1) || _cmd_text[0] == '\0') return CMD_ERROR;

	str = AllocateNameUnique(_cmd_text, 0);
	if (str == 0) return CMD_ERROR;

	if (flags & DC_EXEC) {
		StringID old_str = _engine_name_strings[p1];
		_engine_name_strings[p1] = str;
		DeleteName(old_str);
		_vehicle_design_names |= 3;
		MarkWholeScreenDirty();
	} else {
		DeleteName(str);
	}

	return 0;
}


/*
 * returns true if an engine is valid, of the specified type, and buildable by
 * the given player, false otherwise
 *
 * engine = index of the engine to check
 * type   = the type the engine should be of (VEH_xxx)
 * player = index of the player
 */
bool IsEngineBuildable(EngineID engine, byte type, PlayerID player)
{
	const Engine *e;

	/* check if it's an engine that is in the engine array */
	if (!IsEngineIndex(engine)) return false;

	e = GetEngine(engine);

	/* check if it's an engine of specified type */
	if (e->type != type) return false;

	/* check if it's available */
	if (!HASBIT(e->player_avail, player)) return false;

	return true;
}

/************************************************************************
 * Engine Replacement stuff
 ************************************************************************/

static void EngineRenewPoolNewBlock(uint start_item);

DEFINE_OLD_POOL(EngineRenew, EngineRenew, EngineRenewPoolNewBlock, NULL)

static void EngineRenewPoolNewBlock(uint start_item)
{
	EngineRenew *er;

	/* We don't use FOR_ALL here, because FOR_ALL skips invalid items.
	 *  TODO - This is just a temporary stage, this will be removed. */
	for (er = GetEngineRenew(start_item); er != NULL; er = (er->index + 1U < GetEngineRenewPoolSize()) ? GetEngineRenew(er->index + 1U) : NULL) {
		er->index = start_item++;
		er->from = INVALID_ENGINE;
	}
}


static EngineRenew *AllocateEngineRenew(void)
{
	EngineRenew *er;

	/* We don't use FOR_ALL here, because FOR_ALL skips invalid items.
	 *  TODO - This is just a temporary stage, this will be removed. */
	for (er = GetEngineRenew(0); er != NULL; er = (er->index + 1U < GetEngineRenewPoolSize()) ? GetEngineRenew(er->index + 1U) : NULL) {
		if (IsValidEngineRenew(er)) continue;

		er->to = INVALID_ENGINE;
		er->next = NULL;
		return er;
	}

	/* Check if we can add a block to the pool */
	if (AddBlockToPool(&_EngineRenew_pool)) return AllocateEngineRenew();

	return NULL;
}

/**
 * Retrieves the EngineRenew that specifies the replacement of the given
 * engine type from the given renewlist */
static EngineRenew *GetEngineReplacement(EngineRenewList erl, EngineID engine)
{
	EngineRenew *er = (EngineRenew *)erl;

	while (er) {
		if (er->from == engine) return er;
		er = er->next;
	}
	return NULL;
}

void RemoveAllEngineReplacement(EngineRenewList *erl)
{
	EngineRenew *er = (EngineRenew *)(*erl);
	EngineRenew *next;

	while (er) {
		next = er->next;
		DeleteEngineRenew(er);
		er = next;
	}
	*erl = NULL; // Empty list
}

EngineID EngineReplacement(EngineRenewList erl, EngineID engine)
{
	const EngineRenew *er = GetEngineReplacement(erl, engine);
	return er == NULL ? INVALID_ENGINE : er->to;
}

int32 AddEngineReplacement(EngineRenewList *erl, EngineID old_engine, EngineID new_engine, uint32 flags)
{
	EngineRenew *er;

	/* Check if the old vehicle is already in the list */
	er = GetEngineReplacement(*erl, old_engine);
	if (er != NULL) {
		if (flags & DC_EXEC) er->to = new_engine;
		return 0;
	}

	er = AllocateEngineRenew();
	if (er == NULL) return CMD_ERROR;

	if (flags & DC_EXEC) {
		er->from = old_engine;
		er->to = new_engine;

		/* Insert before the first element */
		er->next = (EngineRenew *)(*erl);
		*erl = (EngineRenewList)er;
	}

	return 0;
}

int32 RemoveEngineReplacement(EngineRenewList *erl, EngineID engine, uint32 flags)
{
	EngineRenew *er = (EngineRenew *)(*erl);
	EngineRenew *prev = NULL;

	while (er)
	{
		if (er->from == engine) {
			if (flags & DC_EXEC) {
				if (prev == NULL) { // First element
					/* The second becomes the new first element */
					*erl = (EngineRenewList)er->next;
				} else {
					/* Cut this element out */
					prev->next = er->next;
				}
				DeleteEngineRenew(er);
			}
			return 0;
		}
		prev = er;
		er = er->next;
	}

	return CMD_ERROR;
}

static const SaveLoad _engine_renew_desc[] = {
	SLE_VAR(EngineRenew, from, SLE_UINT16),
	SLE_VAR(EngineRenew, to,   SLE_UINT16),

	SLE_REF(EngineRenew, next, REF_ENGINE_RENEWS),

	SLE_END()
};

static void Save_ERNW(void)
{
	EngineRenew *er;

	FOR_ALL_ENGINE_RENEWS(er) {
		SlSetArrayIndex(er->index);
		SlObject(er, _engine_renew_desc);
	}
}

static void Load_ERNW(void)
{
	int index;

	while ((index = SlIterateArray()) != -1) {
		EngineRenew *er;

		if (!AddBlockIfNeeded(&_EngineRenew_pool, index))
			error("EngineRenews: failed loading savegame: too many EngineRenews");

		er = GetEngineRenew(index);
		SlObject(er, _engine_renew_desc);
	}
}

static const SaveLoad _engine_desc[] = {
	SLE_CONDVAR(Engine, intro_date,          SLE_FILE_U16 | SLE_VAR_I32,  0,  30),
	SLE_CONDVAR(Engine, intro_date,          SLE_INT32,                  31, SL_MAX_VERSION),
	SLE_CONDVAR(Engine, age,                 SLE_FILE_U16 | SLE_VAR_I32,  0,  30),
	SLE_CONDVAR(Engine, age,                 SLE_INT32,                  31, SL_MAX_VERSION),
	    SLE_VAR(Engine, reliability,         SLE_UINT16),
	    SLE_VAR(Engine, reliability_spd_dec, SLE_UINT16),
	    SLE_VAR(Engine, reliability_start,   SLE_UINT16),
	    SLE_VAR(Engine, reliability_max,     SLE_UINT16),
	    SLE_VAR(Engine, reliability_final,   SLE_UINT16),
	    SLE_VAR(Engine, duration_phase_1,    SLE_UINT16),
	    SLE_VAR(Engine, duration_phase_2,    SLE_UINT16),
	    SLE_VAR(Engine, duration_phase_3,    SLE_UINT16),

	    SLE_VAR(Engine, lifelength,          SLE_UINT8),
	    SLE_VAR(Engine, flags,               SLE_UINT8),
	    SLE_VAR(Engine, preview_player,      SLE_UINT8),
	    SLE_VAR(Engine, preview_wait,        SLE_UINT8),
	SLE_CONDNULL(1, 0, 44),
	    SLE_VAR(Engine, player_avail,        SLE_UINT8),

	/* reserve extra space in savegame here. (currently 16 bytes) */
	SLE_CONDNULL(16, 2, SL_MAX_VERSION),

	SLE_END()
};

static void Save_ENGN(void)
{
	uint i;

	for (i = 0; i != lengthof(_engines); i++) {
		SlSetArrayIndex(i);
		SlObject(&_engines[i], _engine_desc);
	}
}

static void Load_ENGN(void)
{
	int index;
	while ((index = SlIterateArray()) != -1) {
		SlObject(GetEngine(index), _engine_desc);
	}
}

static void LoadSave_ENGS(void)
{
	SlArray(_engine_name_strings, lengthof(_engine_name_strings), SLE_STRINGID);
}

extern const ChunkHandler _engine_chunk_handlers[] = {
	{ 'ENGN', Save_ENGN,     Load_ENGN,     CH_ARRAY          },
	{ 'ENGS', LoadSave_ENGS, LoadSave_ENGS, CH_RIFF           },
	{ 'ERNW', Save_ERNW,     Load_ERNW,     CH_ARRAY | CH_LAST},
};

void InitializeEngines(void)
{
	/* Clean the engine renew pool and create 1 block in it */
	CleanPool(&_EngineRenew_pool);
	AddBlockToPool(&_EngineRenew_pool);
}
