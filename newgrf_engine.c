/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "variables.h"
#include "debug.h"
#include "functions.h"
#include "engine.h"
#include "train.h"
#include "player.h"
#include "newgrf_callbacks.h"
#include "newgrf_engine.h"
#include "newgrf_station.h"
#include "newgrf_spritegroup.h"

// TODO: We don't support cargo-specific wagon overrides. Pretty exotic... ;-) --pasky

typedef struct WagonOverride {
	byte *train_id;
	int trains;
	const SpriteGroup *group;
} WagonOverride;

typedef struct WagonOverrides {
	int overrides_count;
	WagonOverride *overrides;
} WagonOverrides;

static WagonOverrides _engine_wagon_overrides[TOTAL_NUM_ENGINES];

void SetWagonOverrideSprites(EngineID engine, const SpriteGroup *group, byte *train_id, int trains)
{
	WagonOverrides *wos;
	WagonOverride *wo;

	wos = &_engine_wagon_overrides[engine];
	wos->overrides_count++;
	wos->overrides = realloc(wos->overrides,
		wos->overrides_count * sizeof(*wos->overrides));

	wo = &wos->overrides[wos->overrides_count - 1];
	/* FIXME: If we are replacing an override, release original SpriteGroup
	 * to prevent leaks. But first we need to refcount the SpriteGroup.
	 * --pasky */
	wo->group = group;
	wo->trains = trains;
	wo->train_id = malloc(trains);
	memcpy(wo->train_id, train_id, trains);
}

static const SpriteGroup *GetWagonOverrideSpriteSet(EngineID engine, byte overriding_engine)
{
	const WagonOverrides *wos = &_engine_wagon_overrides[engine];
	int i;

	// XXX: This could turn out to be a timesink on profiles. We could
	// always just dedicate 65535 bytes for an [engine][train] trampoline
	// for O(1). Or O(logMlogN) and searching binary tree or smt. like
	// that. --pasky

	for (i = 0; i < wos->overrides_count; i++) {
		const WagonOverride *wo = &wos->overrides[i];
		int j;

		for (j = 0; j < wo->trains; j++) {
			if (wo->train_id[j] == overriding_engine)
				return wo->group;
		}
	}
	return NULL;
}

/**
 * Unload all wagon override sprite groups.
 */
void UnloadWagonOverrides(void)
{
	WagonOverrides *wos;
	WagonOverride *wo;
	EngineID engine;
	int i;

	for (engine = 0; engine < TOTAL_NUM_ENGINES; engine++) {
		wos = &_engine_wagon_overrides[engine];
		for (i = 0; i < wos->overrides_count; i++) {
			wo = &wos->overrides[i];
			wo->group = NULL;
			free(wo->train_id);
		}
		free(wos->overrides);
		wos->overrides_count = 0;
		wos->overrides = NULL;
	}
}

// 0 - 28 are cargos, 29 is default, 30 is the advert (purchase list)
// (It isn't and shouldn't be like this in the GRF files since new cargo types
// may appear in future - however it's more convenient to store it like this in
// memory. --pasky)
static const SpriteGroup *engine_custom_sprites[TOTAL_NUM_ENGINES][NUM_GLOBAL_CID];
static uint32 _engine_grf[TOTAL_NUM_ENGINES];

void SetCustomEngineSprites(EngineID engine, byte cargo, const SpriteGroup *group)
{
	assert(engine < TOTAL_NUM_ENGINES);
	if (engine_custom_sprites[engine][cargo] != NULL) {
		DEBUG(grf, 6)("SetCustomEngineSprites: engine `%d' cargo `%d' already has group -- replacing.", engine, cargo);
	}
	engine_custom_sprites[engine][cargo] = group;
}

/**
 * Unload all engine sprite groups.
 */
void UnloadCustomEngineSprites(void)
{
	EngineID engine;
	CargoID cargo;

	for (engine = 0; engine < TOTAL_NUM_ENGINES; engine++) {
		for (cargo = 0; cargo < NUM_GLOBAL_CID; cargo++) {
			engine_custom_sprites[engine][cargo] = NULL;
		}
		_engine_grf[engine] = 0;
	}
}

static const SpriteGroup *heli_rotor_custom_sprites[NUM_AIRCRAFT_ENGINES];

/** Load a rotor override sprite group for an aircraft */
void SetRotorOverrideSprites(EngineID engine, const SpriteGroup *group)
{
	assert(engine >= AIRCRAFT_ENGINES_INDEX);
	assert(engine < AIRCRAFT_ENGINES_INDEX + NUM_AIRCRAFT_ENGINES);

	if (heli_rotor_custom_sprites[engine - AIRCRAFT_ENGINES_INDEX] != NULL) {
		DEBUG(grf, 6)("SetRotorOverrideSprites: engine `%d' already has group -- replacing.", engine);
	}
	heli_rotor_custom_sprites[engine - AIRCRAFT_ENGINES_INDEX] = group;
}

/** Unload all rotor override sprite groups */
void UnloadRotorOverrideSprites(void)
{
	EngineID engine;

	/* Starting at AIRCRAFT_ENGINES_INDEX may seem pointless, but it means
	 * the context of EngineID is correct */
	for (engine = AIRCRAFT_ENGINES_INDEX; engine < AIRCRAFT_ENGINES_INDEX + NUM_AIRCRAFT_ENGINES; engine++) {
		heli_rotor_custom_sprites[engine - AIRCRAFT_ENGINES_INDEX] = NULL;
	}
}

void SetEngineGRF(EngineID engine, uint32 grfid)
{
	assert(engine < TOTAL_NUM_ENGINES);
	_engine_grf[engine] = grfid;
}

uint32 GetEngineGRFID(EngineID engine)
{
	assert(engine < TOTAL_NUM_ENGINES);
	return _engine_grf[engine];
}


static int MapOldSubType(const Vehicle *v)
{
	if (v->type != VEH_Train) return v->subtype;
	if (IsTrainEngine(v)) return 0;
	if (IsFreeWagon(v)) return 4;
	return 2;
}


/* Vehicle Resolver Functions */
static inline const Vehicle *GRV(const ResolverObject *object)
{
	return object->scope == VSG_SCOPE_SELF ? object->vehicle.self : object->vehicle.parent;
}


static uint32 VehicleGetRandomBits(const ResolverObject *object)
{
	return GRV(object) == NULL ? 0 : GRV(object)->random_bits;
}


static uint32 VehicleGetTriggers(const ResolverObject *object)
{
	return GRV(object) == NULL ? 0 : GRV(object)->waiting_triggers;
}


static void VehicleSetTriggers(const ResolverObject *object, int triggers)
{
	/* Evil cast to get around const-ness. This used to be achieved by an
	 * innocent looking function pointer cast... Currently I cannot see a
	 * way of avoiding this without removing consts deep within gui code.
	 */
	Vehicle *v = (Vehicle*)GRV(object);

	/* This function must only be called when processing triggers -- any
	 * other time is an error. */
	assert(object->trigger != 0);

	if (v != NULL) v->waiting_triggers = triggers;
}


static uint32 VehicleGetVariable(const ResolverObject *object, byte variable, byte parameter)
{
	const Vehicle *v = GRV(object);

	if (v == NULL) {
		/* Vehicle does not exist, so we're in a purchase list */
		switch (variable) {
			case 0x43: return _current_player; /* Owner information */
			case 0x46: return 0;               /* Motion counter */
			case 0xC4: return _cur_year;       /* Build year */
			case 0xDA: return INVALID_VEHICLE; /* Next vehicle */
			default:   return -1;
		}
	}

	/* Calculated vehicle parameters */
	switch (variable) {
		case 0x40: /* Get length of consist */
		case 0x41: /* Get length of same consecutive wagons */
			if (v->type != VEH_Train) return 1;

			{
				const Vehicle* u;
				byte chain_before = 0;
				byte chain_after  = 0;

				for (u = GetFirstVehicleInChain(v); u != v; u = u->next) {
					chain_before++;
					if (variable == 0x41 && u->engine_type != v->engine_type) chain_before = 0;
				}

				while (u->next != NULL && (variable == 0x40 || u->next->engine_type == v->engine_type)) {
					chain_after++;
					u = u->next;
				}

				return chain_before | chain_after << 8 | (chain_before + chain_after) << 16;
			}

		case 0x43: /* Player information */
			return v->owner;

		case 0x46: /* Motion counter */
			return 0;
	}

	/* General vehicle properties */
	switch (variable - 0x80) {
		case 0x00: return v->type;
		case 0x01: return MapOldSubType(v);
		case 0x04: return v->index;
		case 0x05: return v->index & 0xFF;
		case 0x0A: return PackOrder(&v->current_order);
		case 0x0B: return PackOrder(&v->current_order) & 0xFF;
		case 0x0C: return v->num_orders;
		case 0x0D: return v->cur_order_index;
		case 0x10: return v->load_unload_time_rem;
		case 0x11: return v->load_unload_time_rem & 0xFF;
		case 0x12: return v->date_of_last_service;
		case 0x13: return v->date_of_last_service & 0xFF;
		case 0x14: return v->service_interval;
		case 0x15: return v->service_interval & 0xFF;
		case 0x16: return v->last_station_visited;
		case 0x17: return v->tick_counter;
		case 0x18: return v->max_speed;
		case 0x19: return v->max_speed & 0xFF;
		case 0x1A: return v->x_pos;
		case 0x1B: return v->x_pos & 0xFF;
		case 0x1C: return v->y_pos;
		case 0x1D: return v->y_pos & 0xFF;
		case 0x1E: return v->z_pos;
		case 0x1F: return v->direction;
		case 0x28: return v->cur_image;
		case 0x29: return v->cur_image & 0xFF;
		case 0x32: return v->vehstatus;
		case 0x33: return v->vehstatus;
		case 0x34: return v->cur_speed;
		case 0x35: return v->cur_speed & 0xFF;
		case 0x36: return v->subspeed;
		case 0x37: return v->acceleration;
		case 0x39: return v->cargo_type;
		case 0x3A: return v->cargo_cap;
		case 0x3B: return v->cargo_cap & 0xFF;
		case 0x3C: return v->cargo_count;
		case 0x3D: return v->cargo_count & 0xFF;
		case 0x3E: return v->cargo_source;
		case 0x3F: return v->cargo_days;
		case 0x40: return v->age;
		case 0x41: return v->age & 0xFF;
		case 0x42: return v->max_age;
		case 0x43: return v->max_age & 0xFF;
		case 0x44: return v->build_year;
		case 0x45: return v->unitnumber;
		case 0x46: return v->engine_type;
		case 0x47: return v->engine_type & 0xFF;
		case 0x48: return v->spritenum;
		case 0x49: return v->day_counter;
		case 0x4A: return v->breakdowns_since_last_service;
		case 0x4B: return v->breakdown_ctr;
		case 0x4C: return v->breakdown_delay;
		case 0x4D: return v->breakdown_chance;
		case 0x4E: return v->reliability;
		case 0x4F: return v->reliability & 0xFF;
		case 0x50: return v->reliability_spd_dec;
		case 0x51: return v->reliability_spd_dec & 0xFF;
		case 0x52: return v->profit_this_year;
		case 0x53: return v->profit_this_year & 0xFFFFFF;
		case 0x54: return v->profit_this_year & 0xFFFF;
		case 0x55: return v->profit_this_year & 0xFF;
		case 0x56: return v->profit_last_year;
		case 0x57: return v->profit_last_year & 0xFF;
		case 0x58: return v->profit_last_year;
		case 0x59: return v->profit_last_year & 0xFF;
		case 0x5A: return v->next == NULL ? INVALID_VEHICLE : v->next->index;
		case 0x5C: return v->value;
		case 0x5D: return v->value & 0xFFFFFF;
		case 0x5E: return v->value & 0xFFFF;
		case 0x5F: return v->value & 0xFF;
		case 0x60: return v->string_id;
		case 0x61: return v->string_id & 0xFF;
		case 0x72: return 0; // XXX Refit cycle
		case 0x7A: return v->random_bits;
		case 0x7B: return v->waiting_triggers;
	}

	/* Vehicle specific properties */
	switch (v->type) {
		case VEH_Train:
			switch (variable - 0x80) {
				case 0x62: return v->u.rail.track;
				case 0x66: return v->u.rail.railtype;
				case 0x73: return v->u.rail.cached_veh_length;
				case 0x74: return v->u.rail.cached_power;
				case 0x75: return v->u.rail.cached_power & 0xFFFFFF;
				case 0x76: return v->u.rail.cached_power & 0xFFFF;
				case 0x77: return v->u.rail.cached_power & 0xFF;
				case 0x7C: return v->first->index;
				case 0x7D: return v->first->index & 0xFF;
			}
			break;

		case VEH_Road:
			switch (variable - 0x80) {
				case 0x62: return v->u.road.state;
				case 0x64: return v->u.road.blocked_ctr;
				case 0x65: return v->u.road.blocked_ctr & 0xFF;
				case 0x66: return v->u.road.overtaking;
				case 0x67: return v->u.road.overtaking_ctr;
				case 0x68: return v->u.road.crashed_ctr;
				case 0x69: return v->u.road.crashed_ctr & 0xFF;
			}
			break;

		case VEH_Aircraft:
			switch (variable - 0x80) {
				// case 0x62: XXX Need to convert from ottd to ttdp state
				case 0x63: return v->u.air.targetairport;
				// case 0x66: XXX
			}
			break;
	}

	DEBUG(grf, 1)("Unhandled vehicle property 0x%X, type 0x%X", variable, v->type);

	return -1;
}


static uint32 VehicleResolveReal(const ResolverObject *object, uint num_loaded, uint num_loading, bool *in_motion)
{
	const Vehicle *v = object->vehicle.self;
	uint totalsets;
	uint set;

	if (v == NULL) {
		*in_motion = false;
		return 0;
	}

	if (v->type == VEH_Train) {
		*in_motion = GetFirstVehicleInChain(v)->current_order.type != OT_LOADING;
	} else {
		*in_motion = v->current_order.type != OT_LOADING;
	}

	totalsets = in_motion ? num_loaded : num_loading;

	if (v->cargo_count == v->cargo_cap || totalsets == 1) {
		set = totalsets - 1;
	} else if (v->cargo_count == 0 || totalsets == 2) {
		set = 0;
	} else {
		set = v->cargo_count * (totalsets - 2) / max(1, v->cargo_cap) + 1;
	}

	return set;
}


static inline void NewVehicleResolver(ResolverObject *res, const Vehicle *v)
{
	res->GetRandomBits = &VehicleGetRandomBits;
	res->GetTriggers   = &VehicleGetTriggers;
	res->SetTriggers   = &VehicleSetTriggers;
	res->GetVariable   = &VehicleGetVariable;
	res->ResolveReal   = &VehicleResolveReal;

	res->vehicle.self   = v;
	res->vehicle.parent = (v != NULL && v->type == VEH_Train) ? GetFirstVehicleInChain(v) : v;

	res->callback        = 0;
	res->callback_param1 = 0;
	res->callback_param2 = 0;
	res->last_value      = 0;
	res->trigger         = 0;
	res->reseed          = 0;
}


SpriteID GetCustomEngineSprite(EngineID engine, const Vehicle *v, Direction direction)
{
	const SpriteGroup *group;
	ResolverObject object;
	CargoID cargo = GC_PURCHASE;

	NewVehicleResolver(&object, v);

	if (v != NULL) {
		cargo = _global_cargo_id[_opt.landscape][v->cargo_type];
		assert(cargo != GC_INVALID);
	}

	group = engine_custom_sprites[engine][cargo];

	if (v != NULL && v->type == VEH_Train) {
		const SpriteGroup *overset = GetWagonOverrideSpriteSet(engine, v->u.rail.first_engine);

		if (overset != NULL) group = overset;
	}

	group = Resolve(group, &object);

	if ((group == NULL || group->type != SGT_RESULT) && cargo != GC_DEFAULT) {
		// This group is empty but perhaps there'll be a default one.
		group = Resolve(engine_custom_sprites[engine][GC_DEFAULT], &object);
	}

	if (group == NULL || group->type != SGT_RESULT) return 0;

	return group->g.result.sprite + (direction % group->g.result.num_sprites);
}


SpriteID GetRotorOverrideSprite(EngineID engine, const Vehicle *v)
{
	const SpriteGroup *group;
	ResolverObject object;

	assert(engine >= AIRCRAFT_ENGINES_INDEX);
	assert(engine < AIRCRAFT_ENGINES_INDEX + NUM_AIRCRAFT_ENGINES);

	/* Only valid for helicopters */
	assert((AircraftVehInfo(engine)->subtype & 1) == 0);

	NewVehicleResolver(&object, v);

	group = heli_rotor_custom_sprites[engine - AIRCRAFT_ENGINES_INDEX];
	group = Resolve(group, &object);

	if (group == NULL || group->type != SGT_RESULT) return 0;

	if (v == NULL) return group->g.result.sprite;

	return group->g.result.sprite + v->next->next->u.air.state;
}


/**
 * Check if a wagon is currently using a wagon override
 * @param v The wagon to check
 * @return true if it is using an override, false otherwise
 */
bool UsesWagonOverride(const Vehicle* v)
{
	assert(v->type == VEH_Train);
	return GetWagonOverrideSpriteSet(v->engine_type, v->u.rail.first_engine) != NULL;
}

/**
 * Evaluate a newgrf callback for vehicles
 * @param callback The callback to evalute
 * @param param1   First parameter of the callback
 * @param param2   Second parameter of the callback
 * @param engine   Engine type of the vehicle to evaluate the callback for
 * @param vehicle  The vehicle to evaluate the callback for, or NULL if it doesnt exist yet
 * @return The value the callback returned, or CALLBACK_FAILED if it failed
 */
uint16 GetVehicleCallback(byte callback, uint32 param1, uint32 param2, EngineID engine, const Vehicle *v)
{
	const SpriteGroup *group;
	ResolverObject object;
	CargoID cargo;

	NewVehicleResolver(&object, v);

	object.callback        = callback;
	object.callback_param1 = param1;
	object.callback_param2 = param2;

	cargo = (v == NULL) ? GC_PURCHASE : _global_cargo_id[_opt.landscape][v->cargo_type];

	group = engine_custom_sprites[engine][cargo];

	if (v != NULL && v->type == VEH_Train) {
		const SpriteGroup *overset = GetWagonOverrideSpriteSet(engine, v->u.rail.first_engine);

		if (overset != NULL) group = overset;
	}

	group = Resolve(group, &object);

	if ((group == NULL || group->type != SGT_CALLBACK) && cargo != GC_DEFAULT) {
		// This group is empty but perhaps there'll be a default one.
		group = Resolve(engine_custom_sprites[engine][GC_DEFAULT], &object);
	}

	if (group == NULL || group->type != SGT_CALLBACK)
		return CALLBACK_FAILED;

	return group->g.callback.result;
}


static void DoTriggerVehicle(Vehicle *v, VehicleTrigger trigger, byte base_random_bits, bool first)
{
	const SpriteGroup *group;
	ResolverObject object;
	CargoID cargo;
	byte new_random_bits;

	/* We can't trigger a non-existent vehicle... */
	assert(v != NULL);

	NewVehicleResolver(&object, v);

	object.trigger = trigger;

	cargo = _global_cargo_id[_opt.landscape][v->cargo_type];
	group = engine_custom_sprites[v->engine_type][cargo];

	if (v->type == VEH_Train) {
		const SpriteGroup *overset = GetWagonOverrideSpriteSet(v->engine_type, v->u.rail.first_engine);
		if (overset != NULL) group = overset;
	}

	group = Resolve(group, &object);
	if (group == NULL && v->cargo_type != GC_DEFAULT) {
		// This group is empty but perhaps there'll be a default one.
		group = Resolve(engine_custom_sprites[v->engine_type][GC_DEFAULT], &object);
	}

	/* Really return? */
	if (group == NULL) return;

	new_random_bits = Random();
	v->random_bits &= ~object.reseed;
	v->random_bits |= (first ? new_random_bits : base_random_bits) & object.reseed;

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
			DoTriggerVehicle(GetFirstVehicleInChain(v), VEHICLE_TRIGGER_ANY_NEW_CARGO, new_random_bits, false);
			break;

		case VEHICLE_TRIGGER_DEPOT:
			/* We now trigger the next vehicle in chain recursively.
			 * The random bits portions may be different for each
			 * vehicle in chain. */
			if (v->next != NULL) DoTriggerVehicle(v->next, trigger, 0, true);
			break;

		case VEHICLE_TRIGGER_EMPTY:
			/* We now trigger the next vehicle in chain
			 * recursively.  The random bits portions must be same
			 * for each vehicle in chain, so we give them all
			 * first chained vehicle's portion of random bits. */
			if (v->next != NULL) DoTriggerVehicle(v->next, trigger, first ? new_random_bits : base_random_bits, false);
			break;

		case VEHICLE_TRIGGER_ANY_NEW_CARGO:
			/* Now pass the trigger recursively to the next vehicle
			 * in chain. */
			assert(!first);
			if (v->next != NULL) DoTriggerVehicle(v->next, VEHICLE_TRIGGER_ANY_NEW_CARGO, base_random_bits, false);
			break;
	}
}

void TriggerVehicle(Vehicle *v, VehicleTrigger trigger)
{
	if (trigger == VEHICLE_TRIGGER_DEPOT) {
		// store that the vehicle entered a depot this tick
		VehicleEnteredDepotThisTick(v);
	}

	DoTriggerVehicle(v, trigger, 0, true);
}

StringID _engine_custom_names[TOTAL_NUM_ENGINES];

void SetCustomEngineName(EngineID engine, StringID name)
{
	assert(engine < lengthof(_engine_custom_names));
	_engine_custom_names[engine] = name;
}

void UnloadCustomEngineNames(void)
{
	EngineID i;
	for (i = 0; i < TOTAL_NUM_ENGINES; i++) {
		_engine_custom_names[i] = 0;
	}
}

StringID GetCustomEngineName(EngineID engine)
{
	return _engine_custom_names[engine] == 0 ? _engine_name_strings[engine] : _engine_custom_names[engine];
}

// Functions for changing the order of vehicle purchase lists
// This is currently only implemented for rail vehicles.
static EngineID engine_list_order[NUM_TRAIN_ENGINES];

void ResetEngineListOrder(void)
{
	EngineID i;

	for (i = 0; i < NUM_TRAIN_ENGINES; i++)
		engine_list_order[i] = i;
}

EngineID GetRailVehAtPosition(EngineID pos)
{
	return engine_list_order[pos];
}

void AlterRailVehListOrder(EngineID engine, EngineID target)
{
	EngineID i;
	bool moving = false;

	if (engine == target) return;

	// First, remove our ID from the list.
	for (i = 0; i < NUM_TRAIN_ENGINES - 1; i++) {
		if (engine_list_order[i] == engine)
			moving = true;
		if (moving)
			engine_list_order[i] = engine_list_order[i + 1];
	}

	// Now, insert it again, before the target engine.
	for (i = NUM_TRAIN_ENGINES - 1; i > 0; i--) {
		engine_list_order[i] = engine_list_order[i - 1];
		if (engine_list_order[i] == target) {
			engine_list_order[i - 1] = engine;
			break;
		}
	}
}
