/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "string.h"
#include "strings.h"
#include "engine.h"
#include "newgrf_callbacks.h"
#include "newgrf_engine.h"
#include "sprite.h"
#include "variables.h"
#include "train.h"

// TODO: We don't support cargo-specific wagon overrides. Pretty exotic... ;-) --pasky

typedef struct WagonOverride {
	byte *train_id;
	int trains;
	SpriteGroup *group;
} WagonOverride;

typedef struct WagonOverrides {
	int overrides_count;
	WagonOverride *overrides;
} WagonOverrides;

static WagonOverrides _engine_wagon_overrides[TOTAL_NUM_ENGINES];

void SetWagonOverrideSprites(EngineID engine, SpriteGroup *group, byte *train_id,
	int trains)
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
static SpriteGroup *engine_custom_sprites[TOTAL_NUM_ENGINES][NUM_GLOBAL_CID];

void SetCustomEngineSprites(EngineID engine, byte cargo, SpriteGroup *group)
{
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
	}
}

static int MapOldSubType(const Vehicle *v)
{
	if (v->type != VEH_Train) return v->subtype;
	if (IsTrainEngine(v)) return 0;
	if (IsFreeWagon(v)) return 4;
	return 2;
}

static int VehicleSpecificProperty(const Vehicle *v, byte var) {
	switch (v->type) {
		case VEH_Train:
			switch (var) {
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
			switch (var) {
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
			switch (var) {
				// case 0x62: XXX Need to convert from ottd to ttdp state
				case 0x63: return v->u.air.targetairport;
				// case 0x66: XXX
			}
			break;
	}

	DEBUG(grf, 1)("Unhandled vehicle property 0x%x (var 0x%x), type 0x%x", var, var + 0x80, v->type);

	return -1;
}

typedef SpriteGroup *(*resolve_callback)(const SpriteGroup *spritegroup,
	const Vehicle *veh, uint16 callback_info, void *resolve_func); /* XXX data pointer used as function pointer */

static const SpriteGroup* ResolveVehicleSpriteGroup(const SpriteGroup *spritegroup,
	const Vehicle *veh, uint16 callback_info, resolve_callback resolve_func)
{
	if (spritegroup == NULL)
		return NULL;

	//debug("spgt %d", spritegroup->type);
	switch (spritegroup->type) {
		case SGT_REAL:
		case SGT_CALLBACK:
			return spritegroup;

		case SGT_DETERMINISTIC: {
			const DeterministicSpriteGroup *dsg = &spritegroup->g.determ;
			const SpriteGroup *target;
			int value = -1;

			//debug("[%p] Having fun resolving variable %x", veh, dsg->variable);
			if (dsg->variable == 0x0C) {
				/* Callback ID */
				value = callback_info & 0xFF;
			} else if (dsg->variable == 0x10) {
				value = (callback_info >> 8) & 0xFF;
			} else if ((dsg->variable >> 6) == 0) {
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
						target = dsg->ranges[0].group;
					} else {
						target = dsg->default_group;
					}
					return resolve_func(target, NULL, callback_info, resolve_func);
				}

				if (dsg->var_scope == VSG_SCOPE_PARENT) {
					/* First engine in the vehicle chain */
					if (veh->type == VEH_Train)
						veh = GetFirstVehicleInChain(veh);
				}

				if (dsg->variable == 0x40 || dsg->variable == 0x41) {
					if (veh->type == VEH_Train) {
						const Vehicle *u = GetFirstVehicleInChain(veh);
						byte chain_before = 0, chain_after = 0;

						while (u != veh) {
							chain_before++;
							if (dsg->variable == 0x41 && u->engine_type != veh->engine_type)
								chain_before = 0;
							u = u->next;
						}
						while (u->next != NULL && (dsg->variable == 0x40 || u->next->engine_type == veh->engine_type)) {
							chain_after++;
							u = u->next;
						};

						value = chain_before | chain_after << 8
						        | (chain_before + chain_after) << 16;
					} else {
						value = 1; /* 1 vehicle in the chain */
					}

				} else {
					// TTDPatch runs on little-endian arch;
					// Variable is 0x80 + offset in TTD's vehicle structure
					switch (dsg->variable - 0x80) {
#define veh_prop(id_, value_) case (id_): value = (value_); break
						veh_prop(0x00, veh->type);
						veh_prop(0x01, MapOldSubType(veh));
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
						veh_prop(0x1A, veh->x_pos);
						veh_prop(0x1B, veh->x_pos & 0xFF);
						veh_prop(0x1C, veh->y_pos);
						veh_prop(0x1D, veh->y_pos & 0xFF);
						veh_prop(0x1E, veh->z_pos);
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
						veh_prop(0x5A, veh->next == NULL ? INVALID_VEHICLE : veh->next->index);
						veh_prop(0x5C, veh->value);
						veh_prop(0x5D, veh->value & 0xFFFFFF);
						veh_prop(0x5E, veh->value & 0xFFFF);
						veh_prop(0x5F, veh->value & 0xFF);
						veh_prop(0x60, veh->string_id);
						veh_prop(0x61, veh->string_id & 0xFF);

						veh_prop(0x72, 0); // XXX Refit cycle currently unsupported
						veh_prop(0x7A, veh->random_bits);
						veh_prop(0x7B, veh->waiting_triggers);
#undef veh_prop

						// Handle vehicle specific properties.
						default: value = VehicleSpecificProperty(veh, dsg->variable - 0x80); break;
					}
				}
			}

			target = value != -1 ? EvalDeterministicSpriteGroup(dsg, value) : dsg->default_group;
			//debug("Resolved variable %x: %d, %p", dsg->variable, value, callback);
			return resolve_func(target, veh, callback_info, resolve_func);
		}

		case SGT_RANDOMIZED: {
			const RandomizedSpriteGroup *rsg = &spritegroup->g.random;

			if (veh == NULL) {
				/* Purchase list of something. Show the first one. */
				assert(rsg->num_groups > 0);
				//debug("going for %p: %d", rsg->groups[0], rsg->groups[0].type);
				return resolve_func(rsg->groups[0], NULL, callback_info, resolve_func);
			}

			if (rsg->var_scope == VSG_SCOPE_PARENT) {
				/* First engine in the vehicle chain */
				if (veh->type == VEH_Train)
					veh = GetFirstVehicleInChain(veh);
			}

			return resolve_func(EvalRandomizedSpriteGroup(rsg, veh->random_bits), veh, callback_info, resolve_func);
		}

		default:
			error("I don't know how to handle such a spritegroup %d!", spritegroup->type);
			return NULL;
	}
}

static const SpriteGroup *GetVehicleSpriteGroup(EngineID engine, const Vehicle *v)
{
	const SpriteGroup *group;
	CargoID cargo = GC_PURCHASE;

	if (v != NULL) {
		cargo = _global_cargo_id[_opt.landscape][v->cargo_type];
		assert(cargo != GC_INVALID);
	}

	group = engine_custom_sprites[engine][cargo];

	if (v != NULL && v->type == VEH_Train) {
		const SpriteGroup *overset = GetWagonOverrideSpriteSet(engine, v->u.rail.first_engine);

		if (overset != NULL) group = overset;
	}

	return group;
}

int GetCustomEngineSprite(EngineID engine, const Vehicle* v, Direction direction)
{
	const SpriteGroup *group;
	const RealSpriteGroup *rsg;
	CargoID cargo = GC_PURCHASE;
	byte loaded = 0;
	bool in_motion = 0;
	int totalsets, spriteset;
	int r;

	if (v != NULL) {
		int capacity = v->cargo_cap;

		cargo = _global_cargo_id[_opt.landscape][v->cargo_type];
		assert(cargo != GC_INVALID);

		if (capacity == 0) capacity = 1;
		loaded = (v->cargo_count * 100) / capacity;

		if (v->type == VEH_Train) {
			in_motion = GetFirstVehicleInChain(v)->current_order.type != OT_LOADING;
		} else {
			in_motion = v->current_order.type != OT_LOADING;
		}
	}

	group = GetVehicleSpriteGroup(engine, v);
	group = ResolveVehicleSpriteGroup(group, v, 0, (resolve_callback) ResolveVehicleSpriteGroup);

	if (group == NULL && cargo != GC_DEFAULT) {
		// This group is empty but perhaps there'll be a default one.
		group = ResolveVehicleSpriteGroup(engine_custom_sprites[engine][GC_DEFAULT], v, 0,
		                                (resolve_callback) ResolveVehicleSpriteGroup);
	}

	if (group == NULL)
		return 0;

	assert(group->type == SGT_REAL);
	rsg = &group->g.real;

	if (!rsg->sprites_per_set) {
		// This group is empty. This function users should therefore
		// look up the sprite number in _engine_original_sprites.
		return 0;
	}

	assert(rsg->sprites_per_set <= 8);
	direction %= rsg->sprites_per_set;

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

	r = (in_motion ? rsg->loaded[spriteset]->g.result.result : rsg->loading[spriteset]->g.result.result) + direction;
	return r;
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
	CargoID cargo = GC_DEFAULT;
	uint16 callback_info = callback | (param1 << 8); // XXX Temporary conversion between new and old format.

	if (v != NULL)
		cargo = _global_cargo_id[_opt.landscape][v->cargo_type];

	group = engine_custom_sprites[engine][cargo];

	if (v != NULL && v->type == VEH_Train) {
		const SpriteGroup *overset = GetWagonOverrideSpriteSet(engine, v->u.rail.first_engine);

		if (overset != NULL) group = overset;
	}

	group = ResolveVehicleSpriteGroup(group, v, callback_info, (resolve_callback) ResolveVehicleSpriteGroup);

	if (group == NULL && cargo != GC_DEFAULT) {
		// This group is empty but perhaps there'll be a default one.
		group = ResolveVehicleSpriteGroup(engine_custom_sprites[engine][GC_DEFAULT], v, callback_info,
		                                (resolve_callback) ResolveVehicleSpriteGroup);
	}

	if (group == NULL || group->type != SGT_CALLBACK)
		return CALLBACK_FAILED;

	return group->g.callback.result;
}



// Global variables are evil, yes, but we would end up with horribly overblown
// calling convention otherwise and this should be 100% reentrant.
static byte _vsg_random_triggers;
static byte _vsg_bits_to_reseed;

static const SpriteGroup *TriggerVehicleSpriteGroup(const SpriteGroup *spritegroup,
	Vehicle *veh, uint16 callback_info, resolve_callback resolve_func)
{
	if (spritegroup == NULL)
		return NULL;

	if (spritegroup->type == SGT_RANDOMIZED) {
		_vsg_bits_to_reseed |= RandomizedSpriteGroupTriggeredBits(
			&spritegroup->g.random,
			_vsg_random_triggers,
			&veh->waiting_triggers
		);
	}

	return ResolveVehicleSpriteGroup(spritegroup, veh, callback_info, resolve_func);
}

static void DoTriggerVehicle(Vehicle *veh, VehicleTrigger trigger, byte base_random_bits, bool first)
{
	const SpriteGroup *group;
	const RealSpriteGroup *rsg;
	byte new_random_bits;

	_vsg_random_triggers = trigger;
	_vsg_bits_to_reseed = 0;
	group = TriggerVehicleSpriteGroup(GetVehicleSpriteGroup(veh->engine_type, veh), veh, 0,
	                                  (resolve_callback) TriggerVehicleSpriteGroup);

	if (group == NULL && veh->cargo_type != GC_DEFAULT) {
		// This group turned out to be empty but perhaps there'll be a default one.
		group = TriggerVehicleSpriteGroup(engine_custom_sprites[veh->engine_type][GC_DEFAULT], veh, 0,
		                                  (resolve_callback) TriggerVehicleSpriteGroup);
	}

	if (group == NULL)
		return;

	assert(group->type == SGT_REAL);
	rsg = &group->g.real;

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

void TriggerVehicle(Vehicle *veh, VehicleTrigger trigger)
{
	if (trigger == VEHICLE_TRIGGER_DEPOT) {
		// store that the vehicle entered a depot this tick
		VehicleEnteredDepotThisTick(veh);
	}

	DoTriggerVehicle(veh, trigger, 0, true);
}

StringID _engine_custom_names[TOTAL_NUM_ENGINES];

void SetCustomEngineName(EngineID engine, StringID name)
{
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
