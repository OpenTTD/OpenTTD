#include "stdafx.h"
#include "ttd.h"
#include "table/strings.h"
#include "engine.h"
#include "table/engines.h"
#include "player.h"
#include "command.h"
#include "vehicle.h"
#include "news.h"
#include "saveload.h"
#include "sprite.h"

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
		// time's up for this engine
		// make it either available to all players (if never_expire_vehicles is enabled and if it was available earlier)
		// or disable this engine completely
		e->player_avail = (_patches.never_expire_vehicles && e->player_avail)? -1 : 0;
		e->reliability = e->reliability_final;
	}
}

void AddTypeToEngines()
{
	Engine *e;
	uint32 counter = 0;

	for(e=_engines; e != endof(_engines); e++, counter++) {

		e->type = VEH_Train;
		if 	(counter >= ROAD_ENGINES_INDEX) {
			e->type = VEH_Road;
			if 	(counter >= SHIP_ENGINES_INDEX) {
				e->type = VEH_Ship;
				if 	(counter >= AIRCRAFT_ENGINES_INDEX) {
					e->type = VEH_Aircraft;
					if 	(counter >= TOTAL_NUM_ENGINES) {
						e->type = VEH_Special;
					}
				}
			}
		}
	}
}

void StartupEngines()
{
	Engine *e;
	const EngineInfo *ei;
	uint32 r, counter = 0;

	SetupEngineNames();

	for(e=_engines, ei=_engine_info; e != endof(_engines); e++, ei++, counter++) {

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

		/* This sets up type for the engine
		   It is needed if you want to ask the engine what type it is
		   It should hopefully be the same as when you ask a vehicle what it is
		   but using this, you can ask what type an engine number is
		   even if it is not a vehicle (yet)*/
	}

	AdjustAvailAircraft();
}

uint32 _engine_refit_masks[256];


// TODO: We don't support cargo-specific wagon overrides. Pretty exotic... ;-) --pasky

struct WagonOverride {
	byte *train_id;
	int trains;
	struct SpriteGroup group;
};

static struct WagonOverrides {
	int overrides_count;
	struct WagonOverride *overrides;
} _engine_wagon_overrides[256];

void SetWagonOverrideSprites(byte engine, struct SpriteGroup *group,
                             byte *train_id, int trains)
{
	struct WagonOverrides *wos;
	struct WagonOverride *wo;

	wos = &_engine_wagon_overrides[engine];
	wos->overrides_count++;
	wos->overrides = realloc(wos->overrides,
	                         wos->overrides_count * sizeof(struct WagonOverride));

	wo = &wos->overrides[wos->overrides_count - 1];
	/* FIXME: If we are replacing an override, release original SpriteGroup
	 * to prevent leaks. But first we need to refcount the SpriteGroup.
	 * --pasky */
	wo->group = *group;
	wo->trains = trains;
	wo->train_id = malloc(trains);
	memcpy(wo->train_id, train_id, trains);
}

static struct SpriteGroup *GetWagonOverrideSpriteSet(byte engine, byte overriding_engine)
{
	struct WagonOverrides *wos = &_engine_wagon_overrides[engine];
	int i;

	// XXX: This could turn out to be a timesink on profiles. We could
	// always just dedicate 65535 bytes for an [engine][train] trampoline
	// for O(1). Or O(logMlogN) and searching binary tree or smt. like
	// that. --pasky

	for (i = 0; i < wos->overrides_count; i++) {
		struct WagonOverride *wo = &wos->overrides[i];
		int j;

		for (j = 0; j < wo->trains; j++) {
			if (wo->train_id[j] == overriding_engine)
				return &wo->group;
		}
	}
	return NULL;
}


byte _engine_original_sprites[256];
// 0 - 28 are cargos, 29 is default, 30 is the advert (purchase list)
// (It isn't and shouldn't be like this in the GRF files since new cargo types
// may appear in future - however it's more convenient to store it like this in
// memory. --pasky)
static struct SpriteGroup _engine_custom_sprites[256][NUM_CID];

void SetCustomEngineSprites(byte engine, byte cargo, struct SpriteGroup *group)
{
	/* FIXME: If we are replacing an override, release original SpriteGroup
	 * to prevent leaks. But first we need to refcount the SpriteGroup.
	 * --pasky */
	_engine_custom_sprites[engine][cargo] = *group;
}

typedef struct RealSpriteGroup *(*resolve_callback)(struct SpriteGroup *spritegroup,
                                                    struct Vehicle *veh,
                                                    void *callback); /* XXX data pointer used as function pointer */

static struct RealSpriteGroup *
ResolveVehicleSpriteGroup(struct SpriteGroup *spritegroup, struct Vehicle *veh,
			  resolve_callback callback)
{
	//debug("spgt %d", spritegroup->type);
	switch (spritegroup->type) {
		case SGT_REAL:
			return &spritegroup->g.real;

		case SGT_DETERMINISTIC: {
			struct DeterministicSpriteGroup *dsg = &spritegroup->g.determ;
			struct SpriteGroup *target;
			int value = -1;

			//debug("[%p] Having fun resolving variable %x", veh, dsg->variable);

			if ((dsg->variable >> 6) == 0) {
				/* General property */
				value = GetDeterministicSpriteValue(dsg->variable);

			} else {
				/* Vehicle-specific property. */

				if (veh == NULL) {
					/* We are in a purchase list of something,
					 * and we are checking for something undefined.
					 * That means we should get the first target
					 * (NOT the default one). */
					if (dsg->num_ranges > 0) {
						target = &dsg->ranges[0].group;
					} else {
						target = dsg->default_group;
					}
					return callback(target, NULL, callback);
				}

				if (dsg->var_scope == VSG_SCOPE_PARENT) {
					/* First engine in the vehicle chain */
					if (veh->type == VEH_Train)
						veh = GetFirstVehicleInChain(veh);
				}

				if (dsg->variable == 0x40) {
					if (veh->type == VEH_Train) {
						Vehicle *u = GetFirstVehicleInChain(veh);
						byte chain_before = 0, chain_after = 0;

						while (u != veh) {
							u = u->next;
							chain_before++;
						}
						while (u->next != NULL) {
							u = u->next;
							chain_after++;
						};

						value = chain_before << 16 | chain_after << 8
						        | (chain_before + chain_after + 1);
					} else {
						value = 1; /* 1 vehicle in the chain */
					}

				} else {
					// TTDPatch runs on little-endian arch;
					// Variable is 0x80 + offset in TTD's vehicle structure
					switch (dsg->variable - 0x80) {
#define veh_prop(id_, value_) case (id_): value = (value_); break
						veh_prop(0x00, veh->type);
						veh_prop(0x01, veh->subtype);
						veh_prop(0x04, veh->index);
						veh_prop(0x05, veh->index & 0xFF);
						/* XXX? Is THIS right? */
						veh_prop(0x0A, PackOrder(&veh->current_order));
						veh_prop(0x0B, PackOrder(&veh->current_order) & 0xff);
						veh_prop(0x0C, veh->num_orders);
						veh_prop(0x0D, veh->cur_order_index);
						veh_prop(0x10, veh->load_unload_time_rem);
						veh_prop(0x11, veh->load_unload_time_rem & 0xFF);
						veh_prop(0x12, veh->date_of_last_service);
						veh_prop(0x13, veh->date_of_last_service & 0xFF);
						veh_prop(0x14, veh->service_interval);
						veh_prop(0x15, veh->service_interval & 0xFF);
						veh_prop(0x16, veh->last_station_visited);
						veh_prop(0x17, veh->tick_counter);
						veh_prop(0x18, veh->max_speed);
						veh_prop(0x19, veh->max_speed & 0xFF);
						veh_prop(0x1F, veh->direction);
						veh_prop(0x28, veh->cur_image);
						veh_prop(0x29, veh->cur_image & 0xFF);
						veh_prop(0x32, veh->vehstatus);
						veh_prop(0x33, veh->vehstatus);
						veh_prop(0x34, veh->cur_speed);
						veh_prop(0x35, veh->cur_speed & 0xFF);
						veh_prop(0x36, veh->subspeed);
						veh_prop(0x37, veh->acceleration);
						veh_prop(0x39, veh->cargo_type);
						veh_prop(0x3A, veh->cargo_cap);
						veh_prop(0x3B, veh->cargo_cap & 0xFF);
						veh_prop(0x3C, veh->cargo_count);
						veh_prop(0x3D, veh->cargo_count & 0xFF);
						veh_prop(0x3E, veh->cargo_source); // Probably useless; so what
						veh_prop(0x3F, veh->cargo_days);
						veh_prop(0x40, veh->age);
						veh_prop(0x41, veh->age & 0xFF);
						veh_prop(0x42, veh->max_age);
						veh_prop(0x43, veh->max_age & 0xFF);
						veh_prop(0x44, veh->build_year);
						veh_prop(0x45, veh->unitnumber);
						veh_prop(0x46, veh->engine_type);
						veh_prop(0x47, veh->engine_type & 0xFF);
						veh_prop(0x48, veh->spritenum);
						veh_prop(0x49, veh->day_counter);
						veh_prop(0x4A, veh->breakdowns_since_last_service);
						veh_prop(0x4B, veh->breakdown_ctr);
						veh_prop(0x4C, veh->breakdown_delay);
						veh_prop(0x4D, veh->breakdown_chance);
						veh_prop(0x4E, veh->reliability);
						veh_prop(0x4F, veh->reliability & 0xFF);
						veh_prop(0x50, veh->reliability_spd_dec);
						veh_prop(0x51, veh->reliability_spd_dec & 0xFF);
						veh_prop(0x52, veh->profit_this_year);
						veh_prop(0x53, veh->profit_this_year & 0xFFFFFF);
						veh_prop(0x54, veh->profit_this_year & 0xFFFF);
						veh_prop(0x55, veh->profit_this_year & 0xFF);
						veh_prop(0x56, veh->profit_last_year);
						veh_prop(0x57, veh->profit_last_year & 0xFF);
						veh_prop(0x58, veh->profit_last_year);
						veh_prop(0x59, veh->profit_last_year & 0xFF);
						veh_prop(0x5A, veh->next_in_chain_old);
						veh_prop(0x5B, veh->next_in_chain_old & 0xFF);
						veh_prop(0x5C, veh->value);
						veh_prop(0x5D, veh->value & 0xFFFFFF);
						veh_prop(0x5E, veh->value & 0xFFFF);
						veh_prop(0x5F, veh->value & 0xFF);
						veh_prop(0x60, veh->string_id);
						veh_prop(0x61, veh->string_id & 0xFF);
						/* 00h..07h=sub image? 40h=in tunnel; actually some kind of status
						 * aircraft: >=13h when in flight
						 * train, ship: 80h=in depot
						 * rv: 0feh=in depot */
						/* TODO veh_prop(0x62, veh->???); */

						/* TODO: The rest is per-vehicle, I hope no GRF file looks so far.
						 * But they won't let us have an easy ride so surely *some* GRF
						 * file does. So someone needs to do this too. --pasky */

#undef veh_prop
					}
				}
			}

			target = value != -1 ? EvalDeterministicSpriteGroup(dsg, value) : dsg->default_group;
			//debug("Resolved variable %x: %d, %p", dsg->variable, value, callback);
			return callback(target, veh, callback);
		}

		case SGT_RANDOMIZED: {
			struct RandomizedSpriteGroup *rsg = &spritegroup->g.random;

			if (veh == NULL) {
				/* Purchase list of something. Show the first one. */
				assert(rsg->num_groups > 0);
				//debug("going for %p: %d", rsg->groups[0], rsg->groups[0].type);
				return callback(&rsg->groups[0], NULL, callback);
			}

			if (rsg->var_scope == VSG_SCOPE_PARENT) {
				/* First engine in the vehicle chain */
				if (veh->type == VEH_Train)
					veh = GetFirstVehicleInChain(veh);
			}

			return callback(EvalRandomizedSpriteGroup(rsg, veh->random_bits), veh, callback);
		}

		default:
			error("I don't know how to handle such a spritegroup %d!", spritegroup->type);
			return NULL;
	}
}

static struct SpriteGroup *GetVehicleSpriteGroup(byte engine, Vehicle *v)
{
	struct SpriteGroup *group;
	uint16 overriding_engine = -1;
	byte cargo = CID_PURCHASE;

	if (v != NULL) {
		overriding_engine = v->type == VEH_Train ? v->u.rail.first_engine : -1;
		cargo = _global_cargo_id[_opt.landscape][v->cargo_type];
	}

	group = &_engine_custom_sprites[engine][cargo];

	if (overriding_engine != 0xffff) {
		struct SpriteGroup *overset;

		overset = GetWagonOverrideSpriteSet(engine, overriding_engine);
		if (overset) group = overset;
	}

	return group;
}

int GetCustomEngineSprite(byte engine, Vehicle *v, byte direction)
{
	struct SpriteGroup *group;
	struct RealSpriteGroup *rsg;
	byte cargo = CID_PURCHASE;
	byte loaded = 0;
	bool in_motion = 0;
	int totalsets, spriteset;
	int r;

	if (v != NULL) {
		int capacity = v->cargo_cap;

		cargo = _global_cargo_id[_opt.landscape][v->cargo_type];
		if (capacity == 0) capacity = 1;
		loaded = (v->cargo_count * 100) / capacity;
		in_motion = (v->cur_speed != 0);
	}

	group = GetVehicleSpriteGroup(engine, v);
	rsg = ResolveVehicleSpriteGroup(group, v, (resolve_callback) ResolveVehicleSpriteGroup);

	if (rsg->sprites_per_set == 0 && cargo != 29) { /* XXX magic number */
		// This group is empty but perhaps there'll be a default one.
		rsg = ResolveVehicleSpriteGroup(&_engine_custom_sprites[engine][29], v,
		                                (resolve_callback) ResolveVehicleSpriteGroup);
	}

	if (!rsg->sprites_per_set) {
		// This group is empty. This function users should therefore
		// look up the sprite number in _engine_original_sprites.
		return 0;
	}

	direction %= 8;
	if (rsg->sprites_per_set == 4)
		direction %= 4;

	totalsets = in_motion ? rsg->loaded_count : rsg->loading_count;

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

	r = (in_motion ? rsg->loaded[spriteset] : rsg->loading[spriteset]) + direction;
	return r;
}


// Global variables are evil, yes, but we would end up with horribly overblown
// calling convention otherwise and this should be 100% reentrant.
static byte _vsg_random_triggers;
static byte _vsg_bits_to_reseed;

extern int _custom_sprites_base;

static struct RealSpriteGroup *
TriggerVehicleSpriteGroup(struct SpriteGroup *spritegroup, struct Vehicle *veh,
			  resolve_callback callback)
{
	if (spritegroup->type == SGT_RANDOMIZED)
		_vsg_bits_to_reseed |= RandomizedSpriteGroupTriggeredBits(&spritegroup->g.random,
		                                                         _vsg_random_triggers,
		                                                         &veh->waiting_triggers);

	return ResolveVehicleSpriteGroup(spritegroup, veh, callback);
}

static void DoTriggerVehicle(Vehicle *veh, enum VehicleTrigger trigger, byte base_random_bits, bool first)
{
	struct RealSpriteGroup *rsg;
	byte new_random_bits;

	_vsg_random_triggers = trigger;
	_vsg_bits_to_reseed = 0;
	rsg = TriggerVehicleSpriteGroup(GetVehicleSpriteGroup(veh->engine_type, veh), veh,
	                                (resolve_callback) TriggerVehicleSpriteGroup);
	if (rsg->sprites_per_set == 0 && veh->cargo_type != 29) { /* XXX magic number */
		// This group turned out to be empty but perhaps there'll be a default one.
		rsg = TriggerVehicleSpriteGroup(&_engine_custom_sprites[veh->engine_type][29], veh,
						(resolve_callback) TriggerVehicleSpriteGroup);
	}
	new_random_bits = Random();
	veh->random_bits &= ~_vsg_bits_to_reseed;
	veh->random_bits |= (first ? new_random_bits : base_random_bits) & _vsg_bits_to_reseed;

	switch (trigger) {
		case VEHICLE_TRIGGER_NEW_CARGO:
			/* All vehicles in chain get ANY_NEW_CARGO trigger now.
			 * So we call it for the first one and they will recurse. */
			/* Indexing part of vehicle random bits needs to be
			 * same for all triggered vehicles in the chain (to get
			 * all the random-cargo wagons carry the same cargo,
			 * i.e.), so we give them all the NEW_CARGO triggered
			 * vehicle's portion of random bits. */
			assert(first);
			DoTriggerVehicle(GetFirstVehicleInChain(veh), VEHICLE_TRIGGER_ANY_NEW_CARGO, new_random_bits, false);
			break;
		case VEHICLE_TRIGGER_DEPOT:
			/* We now trigger the next vehicle in chain recursively.
			 * The random bits portions may be different for each
			 * vehicle in chain. */
			if (veh->next != NULL)
				DoTriggerVehicle(veh->next, trigger, 0, true);
			break;
		case VEHICLE_TRIGGER_EMPTY:
			/* We now trigger the next vehicle in chain
			 * recursively.  The random bits portions must be same
			 * for each vehicle in chain, so we give them all
			 * first chained vehicle's portion of random bits. */
			if (veh->next != NULL)
				DoTriggerVehicle(veh->next, trigger, first ? new_random_bits : base_random_bits, false);
			break;
		case VEHICLE_TRIGGER_ANY_NEW_CARGO:
			/* Now pass the trigger recursively to the next vehicle
			 * in chain. */
			assert(!first);
			if (veh->next != NULL)
				DoTriggerVehicle(veh->next, VEHICLE_TRIGGER_ANY_NEW_CARGO, base_random_bits, false);
			break;
	}
}

void TriggerVehicle(Vehicle *veh, enum VehicleTrigger trigger)
{
	DoTriggerVehicle(veh, trigger, 0, true);
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
	strncpy(_userstring, _engine_custom_names[engine], USERSTRING_LEN);
	_userstring[USERSTRING_LEN - 1] = '\0';
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
				if (e->preview_player != 0xFF && !--e->preview_wait) {
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

// Determine if an engine type is a wagon (and not a loco)
bool isWagon(byte index)
{
	if (index < NUM_TRAIN_ENGINES) {
		const RailVehicleInfo *rvi = &_rail_vehicle_info[index];
		if(rvi->flags & RVI_WAGON)
			return true;
	}
	return false;
}

static void NewVehicleAvailable(Engine *e)
{
	Vehicle *v;
	Player *p;
	int index = e - _engines;

	// In case the player didn't build the vehicle during the intro period,
	// prevent that player from getting future intro periods for a while.
	if (e->flags&ENGINE_INTRODUCING) {
		FOR_ALL_PLAYERS(p) {
			uint block_preview = p->block_preview;

			if (!HASBIT(e->player_avail,p->index))
				continue;

			/* We assume the user did NOT build it.. prove me wrong ;) */
			p->block_preview = 20;

			FOR_ALL_VEHICLES(v) {
				if (v->type == VEH_Train || v->type == VEH_Road || v->type == VEH_Ship ||
						(v->type == VEH_Aircraft && v->subtype <= 2)) {
					if (v->owner == p->index && v->engine_type == index) {
						/* The user did prove me wrong, so restore old value */
						p->block_preview = block_preview;
						break;
					}
				}
			}
		}
	}

	e->flags = (e->flags & ~ENGINE_INTRODUCING) | ENGINE_AVAILABLE;
	InvalidateWindowClasses(WC_BUILD_VEHICLE);

	// Now available for all players
	e->player_avail = (byte)-1;

	// Do not introduce new rail wagons
	if(isWagon(index))
		return;

	// make maglev / monorail available
	FOR_ALL_PLAYERS(p) {
		if (p->is_active)
			UPDATE_PLAYER_RAILTYPE(e,p);
	}

	if ((byte)index < NUM_TRAIN_ENGINES) {
		AddNewsItem(index, NEWS_FLAGS(NM_CALLBACK, 0, NT_NEW_VEHICLES, DNC_TRAINAVAIL), 0, 0);
	} else if ((byte)index < NUM_TRAIN_ENGINES + NUM_ROAD_ENGINES) {
		AddNewsItem(index, NEWS_FLAGS(NM_CALLBACK, 0, NT_NEW_VEHICLES, DNC_ROADAVAIL), 0, 0);
	} else if ((byte)index < NUM_TRAIN_ENGINES + NUM_ROAD_ENGINES + NUM_SHIP_ENGINES) {
		AddNewsItem(index, NEWS_FLAGS(NM_CALLBACK, 0, NT_NEW_VEHICLES, DNC_SHIPAVAIL), 0, 0);
	} else {
		AddNewsItem(index, NEWS_FLAGS(NM_CALLBACK, 0, NT_NEW_VEHICLES, DNC_AIRCRAFTAVAIL), 0, 0);
	}
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

				// Do not introduce new rail wagons
				if(!isWagon(e - _engines))
					e->preview_player = 1; // Give to the player with the highest rating.
			}
		}
	}
	AdjustAvailAircraft();
}

int32 CmdRenameEngine(int x, int y, uint32 flags, uint32 p1, uint32 p2)
{
	StringID str;

	str = AllocateNameUnique((byte*)_decode_parameters, 0);
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



