#include "stdafx.h"
#include "ttd.h"
#include "engine.h"
#include "table/engines.h"
#include "player.h"
#include "command.h"
#include "vehicle.h"
#include "news.h"
#include "saveload.h"

#define UPDATE_PLAYER_RAILTYPE(e,p) if ((byte)(e->railtype + 1) > p->max_railtype) p->max_railtype = e->railtype + 1;

enum {
	ENGINE_AVAILABLE = 1,
	ENGINE_INTRODUCING = 2,
	ENGINE_PREVIEWING = 4,
};

/* This maps per-landscape cargo ids to globally unique cargo ids usable ie. in
 * the custom GRF files. It is basically just a transcribed table from
 * TTDPatch's newgrf.txt. */
byte _global_cargo_id[NUM_LANDSCAPE][NUM_CARGO] = {
	/* LT_NORMAL */ {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 12 },
	/* LT_HILLY */  {  0,  1,  2,  3,  4,  5,  6,  7, 28, 11, 10, 12 },
	/* LT_DESERT */ {  0, 16,  2,  3, 13,  5,  6,  7, 14, 15, 10, 12 },
	/* LT_CANDY */  {  0, 17,  2, 18, 19, 20, 21, 22, 23, 24, 25, 26 },
	// 27 is paper in temperate climate in TTDPatch
	// Following can be renumbered:
	// 29 is the default cargo for the purpose of spritesets
	// 30 is the purchase list image (the equivalent of 0xff) for the purpose of spritesets
};

/* These two arrays provide a reverse mapping. */
byte _local_cargo_id_ctype[NUM_CID] = {
	CT_PASSENGERS, CT_COAL, CT_MAIL, CT_OIL, CT_LIVESTOCK, CT_GOODS, CT_GRAIN, CT_WOOD, // 0-7
	CT_IRON_ORE, CT_STEEL, CT_VALUABLES, CT_PAPER, CT_FOOD, CT_FRUIT, CT_COPPER_ORE, CT_WATER, // 8-15
	CT_RUBBER, CT_SUGAR, CT_TOYS, CT_BATTERIES, CT_CANDY, CT_TOFFEE, CT_COLA, CT_COTTON_CANDY, // 16-23
	CT_BUBBLES, CT_PLASTIC, CT_FIZZY_DRINKS, CT_PAPER /* unsup. */, CT_HILLY_UNUSED // 24-28
};

/* LT'th bit is set of the particular landscape if cargo available there.
 * 1: LT_NORMAL, 2: LT_HILLY, 4: LT_DESERT, 8: LT_CANDY */
byte _local_cargo_id_landscape[NUM_CID] = {
	15, 3, 15, 7, 3, 7, 7, 7, 1, 1, 7, 2, 7, // 0-12
	4, 4, 4, 4, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 1, 2, // 13-28
};

void ShowEnginePreviewWindow(int engine);

void DeleteCustomEngineNames()
{
	uint i;
	StringID old;

	for(i=0; i!=TOTAL_NUM_ENGINES; i++) {
		old = _engine_name_strings[i];
		_engine_name_strings[i] = i + STR_8000_KIRBY_PAUL_TANK_STEAM;
		DeleteName(old);
	}

	_vehicle_design_names &= ~1;
}

void LoadCustomEngineNames()
{
	// XXX: not done */
	DEBUG(misc, 1) ("LoadCustomEngineNames: not done");
}

static void SetupEngineNames()
{
	uint i;

	for(i=0; i!=TOTAL_NUM_ENGINES; i++)
		_engine_name_strings[i] = STR_SV_EMPTY;

	DeleteCustomEngineNames();
	LoadCustomEngineNames();
}

static void AdjustAvailAircraft()
{
	uint16 date = _date;
	byte avail = 0;
	if (date >= 12784) avail |= 2; // big airport
	if (date < 14610 || _patches.always_small_airport) avail |= 1;  // small airport
	if (date >= 15706) avail |= 4; // enable heliport

	if (avail != _avail_aircraft) {
		_avail_aircraft = avail;
		InvalidateWindow(WC_BUILD_STATION, 0);
	}
}

static void CalcEngineReliability(Engine *e)
{
	uint age = e->age;

	if (age < e->duration_phase_1) {
		uint start = e->reliability_start;
		e->reliability = age * (e->reliability_max - start) / e->duration_phase_1 + start;
	} else if ((age -= e->duration_phase_1) < e->duration_phase_2) {
		e->reliability = e->reliability_max;
	} else if ((age -= e->duration_phase_2) < e->duration_phase_3) {
		uint max = e->reliability_max;
		e->reliability = (int)age * (int)(e->reliability_final - max) / e->duration_phase_3 + max;
	} else {
		e->player_avail = _patches.never_expire_vehicles ? -1 : 0;
		e->reliability = e->reliability_final;
	}
}

void StartupEngines()
{
	Engine *e;
	const EngineInfo *ei;
	uint32 r;

	SetupEngineNames();


	for(e=_engines, ei=_engine_info; e != endof(_engines); e++,ei++) {
		e->age = 0;
		e->railtype = ei->railtype_climates >> 4;
		e->flags = 0;
		e->player_avail = 0;
		
		r = Random();
		e->intro_date = (uint16)((r & 0x1FF) + ei->base_intro);
		if (e->intro_date <= _date) {
			e->age = (_date - e->intro_date) >> 5;
			e->player_avail = (byte)-1;
			e->flags |= ENGINE_AVAILABLE;
		}

		e->reliability_start = (uint16)(((r >> 16) & 0x3fff) + 0x7AE0);
		r = Random();
		e->reliability_max = (uint16)((r & 0x3fff) + 0xbfff);
		e->reliability_final = (uint16)(((r>>16) & 0x3fff) + 0x3fff);

		r = Random();
		e->duration_phase_1 = (uint16)((r & 0x1F) + 7);
		e->duration_phase_2 = (uint16)(((r >> 5) & 0xF) + ei->base_life * 12 - 96);
		e->duration_phase_3 = (uint16)(((r >> 9) & 0x7F) + 120);

		e->reliability_spd_dec = (ei->unk2&0x7F) << 2;

		/* my invented flag for something that is a wagon */
		if (ei->unk2 & 0x80) {
			e->age = 0xFFFF;
		} else {
			CalcEngineReliability(e);
		}

		e->lifelength = ei->lifelength + _patches.extend_vehicle_life;

		// prevent certain engines from ever appearing.
		if (!HASBIT(ei->railtype_climates, _opt.landscape)) {
			e->flags |= ENGINE_AVAILABLE;
			e->player_avail = 0;
		}
	}

	AdjustAvailAircraft();
}


uint32 _engine_refit_masks[256];


// TODO: We don't support cargo-specific wagon overrides. Pretty exotic... ;-) --pasky

struct WagonOverride {
	byte *train_id;
	int trains;
	struct SpriteSuperSet superset;
};

static struct WagonOverrides {
	int overrides_count;
	struct WagonOverride *overrides;
} _engine_wagon_overrides[256];

void SetWagonOverrideSprites(byte engine, struct SpriteSuperSet *superset,
                             byte *train_id, int trains)
{
	struct WagonOverrides *wos;
	struct WagonOverride *wo;

	wos = &_engine_wagon_overrides[engine];
	wos->overrides_count++;
	wos->overrides = realloc(wos->overrides,
	                         wos->overrides_count * sizeof(struct WagonOverride));
	
	wo = &wos->overrides[wos->overrides_count - 1];
	wo->superset = *superset;
	wo->trains = trains;
	wo->train_id = malloc(trains);
	memcpy(wo->train_id, train_id, trains);
}

static struct SpriteSuperSet *GetWagonOverrideSpriteSet(byte engine, byte overriding_engine)
{
	struct WagonOverrides *wos = &_engine_wagon_overrides[engine];
	int i;

	// XXX: This could turn out to be a timesink on profiles. We could always just
	// dedicate 65535 bytes for an [engine][train] trampoline.

	for (i = 0; i < wos->overrides_count; i++) {
		struct WagonOverride *wo = &wos->overrides[i];
		int j;

		for (j = 0; j < wo->trains; j++) {
			if (wo->train_id[j] == overriding_engine)
				return &wo->superset;
		}
	}
	return NULL;
}


byte _engine_original_sprites[256];
// 0 - 28 are cargos, 29 is default, 30 is the advert (purchase list)
// (It isn't and shouldn't be like this in the GRF files since new cargo types
// may appear in future - however it's more convenient to store it like this in
// memory. --pasky)
static struct SpriteSuperSet _engine_custom_sprites[256][NUM_CID];

void SetCustomEngineSprites(byte engine, byte cargo, struct SpriteSuperSet *superset)
{
	assert(superset->sprites_per_set == 4 || superset->sprites_per_set == 8);
	_engine_custom_sprites[engine][cargo] = *superset;
}

int GetCustomEngineSprite(byte engine, uint16 overriding_engine, byte cargo,
                          byte loaded, byte in_motion, byte direction)
{
	struct SpriteSuperSet *superset = &_engine_custom_sprites[engine][cargo];
	int totalsets, spriteset;
	int r;

	if (overriding_engine != 0xffff) {
		struct SpriteSuperSet *overset;

		overset = GetWagonOverrideSpriteSet(engine, overriding_engine);
		if (overset) superset = overset;
	}
	
	if (!superset->sprites_per_set && cargo != 29) {
		// This superset is empty but perhaps there'll be a default one.
		superset = &_engine_custom_sprites[engine][29];
	}

	if (!superset->sprites_per_set) {
		// This superset is empty. This function users should therefore
		// look up the sprite number in _engine_original_sprites.
		return 0;
	}

	direction %= 8;
	if (superset->sprites_per_set == 4)
		direction %= 4;

	totalsets = in_motion ? superset->loaded_count : superset->loading_count;

	// My aim here is to make it possible to visually determine absolutely
	// empty and totally full vehicles. --pasky
	if (loaded == 100 || totalsets == 1) { // full
		spriteset = totalsets - 1;
	} else if (loaded == 0 || totalsets == 2) { // empty
		spriteset = 0;
	} else { // something inbetween
		spriteset = loaded * (totalsets - 2) / 100 + 1;
		// correct possible rounding errors
		if (!spriteset)
			spriteset = 1;
		else if (spriteset == totalsets - 1)
			spriteset--;
	}

	r = (in_motion ? superset->loaded[spriteset] : superset->loading[spriteset]) + direction;
	return r;
}


static char *_engine_custom_names[256];

void SetCustomEngineName(int engine, char *name)
{
	_engine_custom_names[engine] = strdup(name);
}

StringID GetCustomEngineName(int engine)
{
	if (!_engine_custom_names[engine])
		return _engine_name_strings[engine];
	strcpy(_userstring, _engine_custom_names[engine]);
	return STR_SPEC_USERSTRING;
}


void AcceptEnginePreview(Engine *e, int player)
{
	Player *p;

	SETBIT(e->player_avail, player);

	p = DEREF_PLAYER(player);
	
	UPDATE_PLAYER_RAILTYPE(e,p);

	e->preview_player = 0xFF;
	InvalidateWindowClasses(WC_BUILD_VEHICLE);
}

void EnginesDailyLoop()
{
	Engine *e;
	int i,num;
	Player *p;
	uint mask;
	int32 best_hist;
	int best_player;

	if (_cur_year >= 130)
		return;

	for(e=_engines,i=0; i!=TOTAL_NUM_ENGINES; e++,i++) {
		if (e->flags & ENGINE_INTRODUCING) {
			if (e->flags & ENGINE_PREVIEWING) {
				if (!--e->preview_wait) {
					e->flags &= ~ENGINE_PREVIEWING;
					DeleteWindowById(WC_ENGINE_PREVIEW, i);
					e->preview_player++;
				}	
			} else if (e->preview_player != 0xFF) {
				num = e->preview_player;
				mask = 0;
				do {
					best_hist = -1;
					best_player = -1;
					FOR_ALL_PLAYERS(p) {
						if (p->is_active && p->block_preview == 0 && !HASBIT(mask,p->index) && 
								p->old_economy[0].performance_history > best_hist) {
							best_hist = p->old_economy[0].performance_history;
							best_player = p->index;
						}
					}
					if (best_player == -1) {
						e->preview_player = 0xFF;
						goto next_engine;
					}
					mask |= (1 << best_player);
				} while (--num != 0);
				
				if (!IS_HUMAN_PLAYER(best_player)) {
					/* TTDBUG: TTD has a bug here */
					AcceptEnginePreview(e, best_player);
				} else {
					e->flags |= ENGINE_PREVIEWING;
					e->preview_wait = 20;
					if (IS_INTERACTIVE_PLAYER(best_player)) {
						ShowEnginePreviewWindow(i);					
					}
				}
			}
		}
		next_engine:;
	}
}

int32 CmdWantEnginePreview(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	if (flags & DC_EXEC) {
		AcceptEnginePreview(&_engines[p1], _current_player);
	}
	return 0;
}

void NewVehicleAvailable(Engine *e)
{
	Vehicle *v;
	Player *p;
	int index = e - _engines;

	// In case the player didn't build the vehicle during the intro period,
	// prevent that player from getting future intro periods for a while.
	if (e->flags&ENGINE_INTRODUCING) {
		FOR_ALL_PLAYERS(p) {
			if (!HASBIT(e->player_avail,p->index))
				continue;
		
			for(v=_vehicles;;) {
				if (v->type == VEH_Train || v->type == VEH_Road || v->type == VEH_Ship ||
						(v->type == VEH_Aircraft && v->subtype <= 2)) {
					if (v->owner == p->index && v->engine_type == index) break;
				}
				if (++v == endof(_vehicles)) {
					p->block_preview = 20;
					break;
				}
			}
		}
	}

	// Now available for all players
	e->player_avail = (byte)-1;
	FOR_ALL_PLAYERS(p) {
		if (p->is_active)
			UPDATE_PLAYER_RAILTYPE(e,p);
	}

	e->flags = (e->flags & ~ENGINE_INTRODUCING) | ENGINE_AVAILABLE;
	
	if ((byte)index < NUM_TRAIN_ENGINES) {
		AddNewsItem(index, NEWS_FLAGS(NM_CALLBACK, 0, NT_NEW_VEHICLES, DNC_TRAINAVAIL), 0, 0);
	} else if ((byte)index < NUM_TRAIN_ENGINES + NUM_ROAD_ENGINES) {
		AddNewsItem(index, NEWS_FLAGS(NM_CALLBACK, 0, NT_NEW_VEHICLES, DNC_ROADAVAIL), 0, 0);
	} else if ((byte)index < NUM_TRAIN_ENGINES + NUM_ROAD_ENGINES + NUM_SHIP_ENGINES) {
		AddNewsItem(index, NEWS_FLAGS(NM_CALLBACK, 0, NT_NEW_VEHICLES, DNC_SHIPAVAIL), 0, 0);
	} else {
		AddNewsItem(index, NEWS_FLAGS(NM_CALLBACK, 0, NT_NEW_VEHICLES, DNC_AIRCRAFTAVAIL), 0, 0);
	}

	InvalidateWindowClasses(WC_BUILD_VEHICLE);
}

void EnginesMonthlyLoop()
{
	Engine *e;

	if (_cur_year < 130) {
		for(e=_engines; e != endof(_engines); e++) {
			// Age the vehicle
			if (e->flags&ENGINE_AVAILABLE && e->age != 0xFFFF) {
				e->age++;
				CalcEngineReliability(e);
			}

			if (!(e->flags & ENGINE_AVAILABLE) && (uint16)(_date - min(_date, 365)) >= e->intro_date) {
				// Introduce it to all players
				NewVehicleAvailable(e);
			} else if (!(e->flags & (ENGINE_AVAILABLE|ENGINE_INTRODUCING)) && _date >= e->intro_date) {
				// Introduction date has passed.. show introducing dialog to one player.
				e->flags |= ENGINE_INTRODUCING;
				e->preview_player = 1; // Give to the player with the highest rating.
			}
		}
	}
	AdjustAvailAircraft();
}

int32 CmdRenameEngine(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	StringID str;

	str = AllocateName((byte*)_decode_parameters, 0);
	if (str == 0)
		return CMD_ERROR;
	
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

int GetPlayerMaxRailtype(int p)
{
	Engine *e;
	int rt = 0;
	int i;

	for(e=_engines,i=0; i!=lengthof(_engines); e++,i++) {
		if (!HASBIT(e->player_avail, p))
			continue;

		if ((i >= 27 && i < 54) || (i >= 57 && i < 84) || (i >= 89 && i < 116))
			continue;

		if (rt < e->railtype)
			rt = e->railtype;
	}

	return rt + 1;
}


static const byte _engine_desc[] = {
	SLE_VAR(Engine,intro_date,						SLE_UINT16),
	SLE_VAR(Engine,age,										SLE_UINT16),
	SLE_VAR(Engine,reliability,						SLE_UINT16),
	SLE_VAR(Engine,reliability_spd_dec,		SLE_UINT16),
	SLE_VAR(Engine,reliability_start,			SLE_UINT16),
	SLE_VAR(Engine,reliability_max,				SLE_UINT16),
	SLE_VAR(Engine,reliability_final,			SLE_UINT16),
	SLE_VAR(Engine,duration_phase_1,			SLE_UINT16),
	SLE_VAR(Engine,duration_phase_2,			SLE_UINT16),
	SLE_VAR(Engine,duration_phase_3,			SLE_UINT16),

	SLE_VAR(Engine,lifelength,						SLE_UINT8),
	SLE_VAR(Engine,flags,									SLE_UINT8),
	SLE_VAR(Engine,preview_player,				SLE_UINT8),
	SLE_VAR(Engine,preview_wait,					SLE_UINT8),
	SLE_VAR(Engine,railtype,							SLE_UINT8),
	SLE_VAR(Engine,player_avail,					SLE_UINT8),

	// reserve extra space in savegame here. (currently 16 bytes)
	SLE_CONDARR(NullStruct,null,SLE_FILE_U64 | SLE_VAR_NULL, 2, 2, 255),

	SLE_END()
};

static void Save_ENGN()
{
	Engine *e;
	int i;
	for(i=0,e=_engines; i != lengthof(_engines); i++,e++) {
		SlSetArrayIndex(i);
		SlObject(e, _engine_desc);
	}
}

static void Load_ENGN()
{
	int index;
	while ((index = SlIterateArray()) != -1) {
		SlObject(&_engines[index], _engine_desc);
	}
}

static void LoadSave_ENGS()
{
	SlArray(_engine_name_strings, lengthof(_engine_name_strings), SLE_STRINGID);
}

const ChunkHandler _engine_chunk_handlers[] = {
	{ 'ENGN', Save_ENGN, Load_ENGN, CH_ARRAY},
	{ 'ENGS', LoadSave_ENGS, LoadSave_ENGS, CH_RIFF | CH_LAST},
};



