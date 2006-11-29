/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "variables.h"
#include "debug.h"
#include "functions.h"
#include "engine.h"
#include "train.h"
#include "player.h"
#include "station.h"
#include "airport.h"
#include "newgrf.h"
#include "newgrf_callbacks.h"
#include "newgrf_engine.h"
#include "newgrf_station.h"
#include "newgrf_spritegroup.h"
#include "newgrf_cargo.h"
#include "date.h"



/* Default cargo classes */
static const uint16 _cargo_classes[NUM_GLOBAL_CID] = {
	CC_PASSENGERS,
	CC_BULK,
	CC_MAIL,
	CC_LIQUID,
	CC_PIECE_GOODS,
	CC_EXPRESS,
	CC_BULK,
	CC_PIECE_GOODS,
	CC_BULK,
	CC_PIECE_GOODS,
	CC_ARMOURED,
	CC_PIECE_GOODS,
	CC_REFRIGERATED | CC_EXPRESS,
	CC_REFRIGERATED | CC_EXPRESS,
	CC_BULK,
	CC_LIQUID,
	CC_LIQUID,
	CC_BULK,
	CC_PIECE_GOODS,
	CC_PIECE_GOODS,
	CC_EXPRESS,
	CC_BULK,
	CC_LIQUID,
	CC_BULK,
	CC_PIECE_GOODS,
	CC_LIQUID,
	CC_PIECE_GOODS,
	CC_PIECE_GOODS,
	CC_NOAVAILABLE,
	CC_NOAVAILABLE,
	CC_NOAVAILABLE,
};

int _traininfo_vehicle_pitch = 0;
int _traininfo_vehicle_width = 29;

typedef struct WagonOverride {
	byte *train_id;
	int trains;
	CargoID cargo;
	const SpriteGroup *group;
} WagonOverride;

typedef struct WagonOverrides {
	int overrides_count;
	WagonOverride *overrides;
} WagonOverrides;

static WagonOverrides _engine_wagon_overrides[TOTAL_NUM_ENGINES];

void SetWagonOverrideSprites(EngineID engine, CargoID cargo, const SpriteGroup *group, byte *train_id, int trains)
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
	wo->cargo = cargo;
	wo->trains = trains;
	wo->train_id = malloc(trains);
	memcpy(wo->train_id, train_id, trains);
}

static const SpriteGroup *GetWagonOverrideSpriteSet(EngineID engine, CargoID cargo, byte overriding_engine)
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
			if (wo->train_id[j] == overriding_engine && (wo->cargo == cargo || wo->cargo == GC_DEFAULT)) return wo->group;
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
static const GRFFile *_engine_grf[TOTAL_NUM_ENGINES];

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


/**
 * Tie a GRFFile entry to an engine, to allow us to retrieve GRF parameters
 * etc during a game.
 * @param engine Engine ID to tie the GRFFile to.
 * @param file   Pointer of GRFFile to tie.
 */
void SetEngineGRF(EngineID engine, const GRFFile *file)
{
	assert(engine < TOTAL_NUM_ENGINES);
	_engine_grf[engine] = file;
}


/**
 * Retrieve the GRFFile tied to an engine
 * @param engine Engine ID to retrieve.
 * @return Pointer to GRFFile.
 */
const GRFFile *GetEngineGRF(EngineID engine)
{
	assert(engine < TOTAL_NUM_ENGINES);
	return _engine_grf[engine];
}


/**
 * Retrieve the GRF ID of the GRFFile tied to an engine
 * @param engine Engine ID to retrieve.
 * @return 32 bit GRFID value.
 */
uint32 GetEngineGRFID(EngineID engine)
{
	assert(engine < TOTAL_NUM_ENGINES);
	return _engine_grf[engine]->grfid;
}


static int MapOldSubType(const Vehicle *v)
{
	if (v->type != VEH_Train) return v->subtype;
	if (IsTrainEngine(v)) return 0;
	if (IsFreeWagon(v)) return 4;
	return 2;
}


/* TTDP style aircraft movement states for GRF Action 2 Var 0xE2 */
enum {
	AMS_TTDP_HANGAR,
	AMS_TTDP_TO_HANGAR,
	AMS_TTDP_TO_PAD1,
	AMS_TTDP_TO_PAD2,
	AMS_TTDP_TO_PAD3,
	AMS_TTDP_TO_ENTRY_2_AND_3,
	AMS_TTDP_TO_ENTRY_2_AND_3_AND_H,
	AMS_TTDP_TO_JUNCTION,
	AMS_TTDP_LEAVE_RUNWAY,
	AMS_TTDP_TO_INWAY,
	AMS_TTDP_TO_RUNWAY,
	AMS_TTDP_TO_OUTWAY,
	AMS_TTDP_WAITING,
	AMS_TTDP_TAKEOFF,
	AMS_TTDP_TO_TAKEOFF,
	AMS_TTDP_CLIMBING,
	AMS_TTDP_FLIGHT_APPROACH,
	AMS_TTDP_UNUSED_0x11,
	AMS_TTDP_FLIGHT_TO_TOWER,
	AMS_TTDP_UNUSED_0x13,
	AMS_TTDP_FLIGHT_FINAL,
	AMS_TTDP_FLIGHT_DESCENT,
	AMS_TTDP_BRAKING,
	AMS_TTDP_HELI_TAKEOFF_AIRPORT,
	AMS_TTDP_HELI_TO_TAKEOFF_AIRPORT,
	AMS_TTDP_HELI_LAND_AIRPORT,
	AMS_TTDP_HELI_TAKEOFF_HELIPORT,
	AMS_TTDP_HELI_TO_TAKEOFF_HELIPORT,
	AMS_TTDP_HELI_LAND_HELIPORT,
};


/**
 * Map OTTD aircraft movement states to TTDPatch style movement states
 * (VarAction 2 Variable 0xE2)
 */
static byte MapAircraftMovementState(const Vehicle *v)
{
	const Station *st = GetStation(v->u.air.targetairport);
	byte amdflag = GetAirportMovingData(st->airport_type, v->u.air.pos)->flag;

	switch (v->u.air.state) {
		case HANGAR:
			/* The international airport is a special case as helicopters can land in
			 * front of the hanger. Helicopters also change their air.state to
			 * AMED_HELI_LOWER some time before actually descending. */

			/* This condition only occurs for helicopters, during descent,
			 * to a landing by the hanger of an international airport. */
			if (amdflag & AMED_HELI_LOWER) return AMS_TTDP_HELI_LAND_AIRPORT;

			/* This condition only occurs for helicopters, before starting descent,
			 * to a landing by the hanger of an international airport. */
			if (amdflag & AMED_SLOWTURN) return AMS_TTDP_FLIGHT_TO_TOWER;

			// The final two conditions apply to helicopters or aircraft.
			/* Has reached hanger? */
			if (amdflag & AMED_EXACTPOS) return AMS_TTDP_HANGAR;

			// Still moving towards hanger.
			return AMS_TTDP_TO_HANGAR;

		case TERM1:
			if (amdflag & AMED_EXACTPOS) return AMS_TTDP_TO_PAD1;
			return AMS_TTDP_TO_JUNCTION;

		case TERM2:
			if (amdflag & AMED_EXACTPOS) return AMS_TTDP_TO_PAD2;
			return AMS_TTDP_TO_ENTRY_2_AND_3_AND_H;

		case TERM3:
		case TERM4:
		case TERM5:
		case TERM6:
		case TERM7:
		case TERM8:
			/* TTDPatch only has 3 terminals, so treat these states the same */
			if (amdflag & AMED_EXACTPOS) return AMS_TTDP_TO_PAD3;
			return AMS_TTDP_TO_ENTRY_2_AND_3_AND_H;

		case HELIPAD1:
		case HELIPAD2:
		case HELIPAD3:
		case HELIPAD4: // Will only occur for helicopters.
			if (amdflag & AMED_HELI_LOWER) return AMS_TTDP_HELI_LAND_AIRPORT; // Descending.
			if (amdflag & AMED_SLOWTURN)   return AMS_TTDP_FLIGHT_TO_TOWER;   // Still hasn't started descent.
			return AMS_TTDP_TO_JUNCTION; // On the ground.

		case TAKEOFF: // Moving to takeoff position.
			return AMS_TTDP_TO_OUTWAY;

		case STARTTAKEOFF: // Accelerating down runway.
			return AMS_TTDP_TAKEOFF;

		case ENDTAKEOFF: // Ascent
			return AMS_TTDP_CLIMBING;

		case HELITAKEOFF: // Helicopter is moving to take off position.
			switch (st->airport_type) {
				case AT_SMALL:
				case AT_LARGE:
				case AT_METROPOLITAN:
				case AT_INTERNATIONAL:
				case AT_COMMUTER:
				case AT_INTERCON:
				/* Note, Helidepot and Helistation are treated as airports as
				 * helicopters are taking off from ground level. */
				case AT_HELIDEPOT:
				case AT_HELISTATION:
					if (amdflag & AMED_HELI_RAISE) return AMS_TTDP_HELI_TAKEOFF_AIRPORT;
					return AMS_TTDP_TO_JUNCTION;

				case AT_HELIPORT:
				case AT_OILRIG:
					return AMS_TTDP_HELI_TAKEOFF_HELIPORT;

				default:
					return AMS_TTDP_HELI_TAKEOFF_AIRPORT;
			}

		case FLYING:
			return AMS_TTDP_FLIGHT_TO_TOWER;

		case LANDING: // Descent
			return AMS_TTDP_FLIGHT_DESCENT;

		case ENDLANDING: // On the runway braking
			if (amdflag & AMED_BRAKE) return AMS_TTDP_BRAKING;
			// Landed - moving off runway
			return AMS_TTDP_TO_INWAY;

		case HELILANDING:
		case HELIENDLANDING: // Helicoptor is decending.
			if (amdflag & AMED_HELI_LOWER) {
				switch (st->airport_type) {
					case AT_HELIPORT:
					case AT_OILRIG:
						return AMS_TTDP_HELI_LAND_HELIPORT;

					default:
						/* Note, Helidepot and Helistation are treated as airports as
						 * helicopters are landing at ground level. */
						return AMS_TTDP_HELI_LAND_AIRPORT;
				}
			}
			return AMS_TTDP_FLIGHT_TO_TOWER;

		default:
			return AMS_TTDP_HANGAR;
	}
}


/* TTDP style aircraft movement action for GRF Action 2 Var 0xE6 */
enum {
	AMA_TTDP_IN_HANGAR,
	AMA_TTDP_ON_PAD1,
	AMA_TTDP_ON_PAD2,
	AMA_TTDP_ON_PAD3,
	AMA_TTDP_HANGAR_TO_PAD1,
	AMA_TTDP_HANGAR_TO_PAD2,
	AMA_TTDP_HANGAR_TO_PAD3,
	AMA_TTDP_LANDING_TO_PAD1,
	AMA_TTDP_LANDING_TO_PAD2,
	AMA_TTDP_LANDING_TO_PAD3,
	AMA_TTDP_PAD1_TO_HANGAR,
	AMA_TTDP_PAD2_TO_HANGAR,
	AMA_TTDP_PAD3_TO_HANGAR,
	AMA_TTDP_PAD1_TO_TAKEOFF,
	AMA_TTDP_PAD2_TO_TAKEOFF,
	AMA_TTDP_PAD3_TO_TAKEOFF,
	AMA_TTDP_HANGAR_TO_TAKOFF,
	AMA_TTDP_LANDING_TO_HANGAR,
	AMA_TTDP_IN_FLIGHT,
};


/**
 * Map OTTD aircraft movement states to TTDPatch style movement actions
 * (VarAction 2 Variable 0xE6)
 * This is not fully supported yet but it's enough for Planeset.
 */
static byte MapAircraftMovementAction(const Vehicle *v)
{
	switch (v->u.air.state) {
		case HANGAR:
			return (v->cur_speed > 0) ? AMA_TTDP_LANDING_TO_HANGAR : AMA_TTDP_IN_HANGAR;

		case TERM1:
		case HELIPAD1:
			return (v->current_order.type == OT_LOADING) ? AMA_TTDP_ON_PAD1 : AMA_TTDP_LANDING_TO_PAD1;

		case TERM2:
		case HELIPAD2:
			return (v->current_order.type == OT_LOADING) ? AMA_TTDP_ON_PAD2 : AMA_TTDP_LANDING_TO_PAD2;

		case TERM3:
		case TERM4:
		case TERM5:
		case TERM6:
		case TERM7:
		case TERM8:
		case HELIPAD3:
		case HELIPAD4:
			return (v->current_order.type == OT_LOADING) ? AMA_TTDP_ON_PAD3 : AMA_TTDP_LANDING_TO_PAD3;

		case TAKEOFF:      // Moving to takeoff position
		case STARTTAKEOFF: // Accelerating down runway
		case ENDTAKEOFF:   // Ascent
		case HELITAKEOFF:
			// TODO Need to find which terminal (or hanger) we've come from. How?
			return AMA_TTDP_PAD1_TO_TAKEOFF;

		case FLYING:
			return AMA_TTDP_IN_FLIGHT;

		case LANDING:    // Descent
		case ENDLANDING: // On the runway braking
		case HELILANDING:
		case HELIENDLANDING:
			// TODO Need to check terminal we're landing to. Is it known yet?
			return (v->current_order.type == OT_GOTO_DEPOT) ?
				AMA_TTDP_LANDING_TO_HANGAR : AMA_TTDP_LANDING_TO_PAD1;

		default:
			return AMA_TTDP_IN_HANGAR;
	}
}


/* TTDP airport types. Used to map our types to TTDPatch's */
enum {
	ATP_TTDP_SMALL,
	ATP_TTDP_LARGE,
	ATP_TTDP_HELIPORT,
	ATP_TTDP_OILRIG,
};


/* Vehicle Resolver Functions */
static inline const Vehicle *GRV(const ResolverObject *object)
{
	return object->scope == VSG_SCOPE_SELF ? object->u.vehicle.self : object->u.vehicle.parent;
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


static uint32 GetVehicleTypeInfo(EngineID engine_type)
{
	/* Bit 0  Vehicle type is available on the market
	 * Bit 1  Vehicle type is in the testing phase
	 * Bit 2  Exclusive testing offer for a human player active */
	const Engine *e = GetEngine(engine_type);
	uint32 var = 0;

	if (e->player_avail == 0xFF) SETBIT(var, 0);
	if (e->age < e->duration_phase_1) SETBIT(var, 1);
	if (e->player_avail > 0 && e->player_avail != 0xFF) SETBIT(var, 2);
	return var;
}


static uint32 GetGRFParameter(EngineID engine_type, byte parameter)
{
	const GRFFile *file = GetEngineGRF(engine_type);

	if (parameter >= file->param_end) return 0;
	return file->param[parameter];
}


static uint32 VehicleGetVariable(const ResolverObject *object, byte variable, byte parameter, bool *available)
{
	const Vehicle *v = GRV(object);

	if (v == NULL) {
		/* Vehicle does not exist, so we're in a purchase list */
		switch (variable) {
			case 0x43: return _current_player; /* Owner information */
			case 0x46: return 0;               /* Motion counter */
			case 0x48: return GetVehicleTypeInfo(object->u.vehicle.self_type); /* Vehicle Type Info */
			case 0xC4: return clamp(_cur_year, ORIGINAL_BASE_YEAR, ORIGINAL_MAX_YEAR) - ORIGINAL_BASE_YEAR; /* Build year */
			case 0xDA: return INVALID_VEHICLE; /* Next vehicle */
			case 0x7F: return GetGRFParameter(object->u.vehicle.self_type, parameter); /* Read GRF parameter */
		}

		*available = false;
		return -1;
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

				return chain_before | chain_after << 8 | (chain_before + chain_after + (variable == 0x41)) << 16;
			}

		case 0x42: { /* Consist cargo information */
			/* XXX Missing support for common refit cycle and property 25 */
			const Vehicle *u;
			byte cargo_classes = 0;
			uint common_cargo_best = 0;
			uint common_cargos[NUM_GLOBAL_CID];
			byte user_def_data = 0;
			CargoID cargo;
			CargoID common_cargo_type = GC_PASSENGERS;

			/* Reset our arrays */
			memset(common_cargos, 0, sizeof(common_cargos));

			for (u = v; u != NULL; u = u->next) {
				/* Skip empty engines */
				if (u->cargo_cap == 0) continue;
				/* Map from climate to global cargo ID */
				cargo = _global_cargo_id[_opt.landscape][u->cargo_type];
				cargo_classes |= _cargo_classes[cargo];
				common_cargos[cargo]++;
				user_def_data |= RailVehInfo(u->engine_type)->user_def_data;
			}

			/* Pick the most common cargo type */
			for (cargo = 0; cargo < NUM_GLOBAL_CID; cargo++) {
				if (common_cargos[cargo] > common_cargo_best) {
					common_cargo_best = common_cargos[cargo];
					common_cargo_type = cargo;
				}
			}

			return cargo_classes | (common_cargo_type << 8) | (user_def_data << 24);
		}

		case 0x43: /* Player information */
			return v->owner;

		case 0x44: /* Aircraft information */
			if (v->type != VEH_Aircraft) return -1;

			{
				const Vehicle *w = v->next;
				uint16 altitude = v->z_pos - w->z_pos; /* Aircraft height - shadow height */
				byte airporttype;

				switch (GetStation(v->u.air.targetairport)->airport_type) {
					/* Note, Helidepot and Helistation are treated as small airports
					 * as they are at ground level. */
					case AT_HELIDEPOT:
					case AT_HELISTATION:
					case AT_COMMUTER:
					case AT_SMALL:         airporttype = ATP_TTDP_SMALL; break;
					case AT_METROPOLITAN:
					case AT_INTERNATIONAL:
					case AT_INTERCON:
					case AT_LARGE:         airporttype = ATP_TTDP_LARGE; break;
					case AT_HELIPORT:      airporttype = ATP_TTDP_HELIPORT; break;
					case AT_OILRIG:        airporttype = ATP_TTDP_OILRIG; break;
					default:               airporttype = ATP_TTDP_LARGE; break;
				}

				return (altitude << 8) | airporttype;
			}

		case 0x46: /* Motion counter */
			return v->motion_counter;

		case 0x47: { /* Vehicle cargo info */
			/* Format: ccccwwtt
			 * tt - the cargo type transported by the vehicle,
			 *     translated if a translation table has been installed.
			 * ww - cargo unit weight in 1/16 tons, same as cargo prop. 0F.
			 * cccc - the cargo class value of the cargo transported by the vehicle.
			 */
			CargoID cid = _global_cargo_id[_opt.landscape][v->cargo_type];

			return (_cargo_classes[cid] << 16) | (_cargoc.weights[v->cargo_type] << 8) | cid;
		}

		case 0x48: return GetVehicleTypeInfo(v->engine_type); /* Vehicle Type Info */

		/* Variables which use the parameter */
		case 0x60: /* Count consist's engine ID occurance */
			if (v->type != VEH_Train) return v->engine_type == parameter;

			{
				uint count = 0;
				for (; v != NULL; v = v->next) {
					if (v->engine_type == parameter) count++;
				}
				return count;
			}

		case 0x7F: return GetGRFParameter(v->engine_type, parameter); /* Read GRF parameter */
	}

	/* General vehicle properties */
	switch (variable - 0x80) {
		case 0x00: return v->type;
		case 0x01: return MapOldSubType(v);
		case 0x04: return v->index;
		case 0x05: return GB(v->index, 8, 8);
		case 0x0A: return PackOrder(&v->current_order);
		case 0x0B: return GB(PackOrder(&v->current_order), 8, 8);
		case 0x0C: return v->num_orders;
		case 0x0D: return v->cur_order_index;
		case 0x10: return v->load_unload_time_rem;
		case 0x11: return GB(v->load_unload_time_rem, 8, 8);
		case 0x12: return max(v->date_of_last_service - DAYS_TILL_ORIGINAL_BASE_YEAR, 0);
		case 0x13: return GB(max(v->date_of_last_service - DAYS_TILL_ORIGINAL_BASE_YEAR, 0), 8, 8);
		case 0x14: return v->service_interval;
		case 0x15: return GB(v->service_interval, 8, 8);
		case 0x16: return v->last_station_visited;
		case 0x17: return v->tick_counter;
		case 0x18: return v->max_speed;
		case 0x19: return GB(v->max_speed, 8, 8);
		case 0x1A: return v->x_pos;
		case 0x1B: return GB(v->x_pos, 8, 8);
		case 0x1C: return v->y_pos;
		case 0x1D: return GB(v->y_pos, 8, 8);
		case 0x1E: return v->z_pos;
		case 0x1F: return object->info_view ? DIR_W : v->direction;
		case 0x28: return v->cur_image;
		case 0x29: return GB(v->cur_image, 8, 8);
		case 0x32: return v->vehstatus;
		case 0x33: return 0; // non-existent high byte of vehstatus
		case 0x34: return v->cur_speed;
		case 0x35: return GB(v->cur_speed, 8, 8);
		case 0x36: return v->subspeed;
		case 0x37: return v->acceleration;
		case 0x39: return v->cargo_type;
		case 0x3A: return v->cargo_cap;
		case 0x3B: return GB(v->cargo_cap, 8, 8);
		case 0x3C: return v->cargo_count;
		case 0x3D: return GB(v->cargo_count, 8, 8);
		case 0x3E: return v->cargo_source;
		case 0x3F: return v->cargo_days;
		case 0x40: return v->age;
		case 0x41: return GB(v->age, 8, 8);
		case 0x42: return v->max_age;
		case 0x43: return GB(v->max_age, 8, 8);
		case 0x44: return clamp(v->build_year, ORIGINAL_BASE_YEAR, ORIGINAL_MAX_YEAR) - ORIGINAL_BASE_YEAR;
		case 0x45: return v->unitnumber;
		case 0x46: return v->engine_type;
		case 0x47: return GB(v->engine_type, 8, 8);
		case 0x48: return v->spritenum;
		case 0x49: return v->day_counter;
		case 0x4A: return v->breakdowns_since_last_service;
		case 0x4B: return v->breakdown_ctr;
		case 0x4C: return v->breakdown_delay;
		case 0x4D: return v->breakdown_chance;
		case 0x4E: return v->reliability;
		case 0x4F: return GB(v->reliability, 8, 8);
		case 0x50: return v->reliability_spd_dec;
		case 0x51: return GB(v->reliability_spd_dec, 8, 8);
		case 0x52: return v->profit_this_year;
		case 0x53: return GB(v->profit_this_year,  8, 24);
		case 0x54: return GB(v->profit_this_year, 16, 16);
		case 0x55: return GB(v->profit_this_year, 24,  8);
		case 0x56: return v->profit_last_year;
		case 0x57: return GB(v->profit_last_year,  8, 24);
		case 0x58: return GB(v->profit_last_year, 16, 16);
		case 0x59: return GB(v->profit_last_year, 24,  8);
		case 0x5A: return v->next == NULL ? INVALID_VEHICLE : v->next->index;
		case 0x5C: return v->value;
		case 0x5D: return GB(v->value,  8, 24);
		case 0x5E: return GB(v->value, 16, 16);
		case 0x5F: return GB(v->value, 24,  8);
		case 0x60: return v->string_id;
		case 0x61: return GB(v->string_id, 8, 8);
		case 0x72: return v->cargo_subtype;
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
				case 0x75: return GB(v->u.rail.cached_power,  8, 24);
				case 0x76: return GB(v->u.rail.cached_power, 16, 16);
				case 0x77: return GB(v->u.rail.cached_power, 24,  8);
				case 0x7C: return v->first->index;
				case 0x7D: return GB(v->first->index, 8, 8);
				case 0x7F: return 0; // Used for vehicle reversing hack in TTDP
			}
			break;

		case VEH_Road:
			switch (variable - 0x80) {
				case 0x62: return v->u.road.state;
				case 0x64: return v->u.road.blocked_ctr;
				case 0x65: return GB(v->u.road.blocked_ctr, 8, 8);
				case 0x66: return v->u.road.overtaking;
				case 0x67: return v->u.road.overtaking_ctr;
				case 0x68: return v->u.road.crashed_ctr;
				case 0x69: return GB(v->u.road.crashed_ctr, 8, 8);
			}
			break;

		case VEH_Aircraft:
			switch (variable - 0x80) {
				case 0x62: return MapAircraftMovementState(v);  // Current movement state
				case 0x63: return v->u.air.targetairport;       // Airport to which the action refers
				case 0x66: return MapAircraftMovementAction(v); // Current movement action
			}
			break;
	}

	DEBUG(grf, 1)("Unhandled vehicle property 0x%X, type 0x%X", variable, v->type);

	*available = false;
	return -1;
}


static const SpriteGroup *VehicleResolveReal(const ResolverObject *object, const SpriteGroup *group)
{
	const Vehicle *v = object->u.vehicle.self;
	uint totalsets;
	uint set;
	bool in_motion;

	if (v == NULL) return group->g.real.loading[0];

	if (v->type == VEH_Train) {
		in_motion = GetFirstVehicleInChain(v)->current_order.type != OT_LOADING;
	} else {
		in_motion = v->current_order.type != OT_LOADING;
	}

	totalsets = in_motion ? group->g.real.num_loaded : group->g.real.num_loading;

	if (v->cargo_count == v->cargo_cap || totalsets == 1) {
		set = totalsets - 1;
	} else if (v->cargo_count == 0 || totalsets == 2) {
		set = 0;
	} else {
		set = v->cargo_count * (totalsets - 2) / max(1, v->cargo_cap) + 1;
	}

	return in_motion ? group->g.real.loaded[set] : group->g.real.loading[set];
}


static inline void NewVehicleResolver(ResolverObject *res, EngineID engine_type, const Vehicle *v)
{
	res->GetRandomBits = &VehicleGetRandomBits;
	res->GetTriggers   = &VehicleGetTriggers;
	res->SetTriggers   = &VehicleSetTriggers;
	res->GetVariable   = &VehicleGetVariable;
	res->ResolveReal   = &VehicleResolveReal;

	res->u.vehicle.self   = v;
	res->u.vehicle.parent = (v != NULL && v->type == VEH_Train) ? GetFirstVehicleInChain(v) : v;

	res->u.vehicle.self_type = engine_type;

	res->info_view = false;

	res->callback        = 0;
	res->callback_param1 = 0;
	res->callback_param2 = 0;
	res->last_value      = 0;
	res->trigger         = 0;
	res->reseed          = 0;
}


/** Retrieve the SpriteGroup for the specified vehicle.
 * If the vehicle is not specified, the purchase list group for the engine is
 * chosen. For trains, an additional engine override lookup is performed.
 * @param engine Engine type of the vehicle.
 * @param v      The vehicle itself.
 * @returns      The selected SpriteGroup for the vehicle.
 */
static const SpriteGroup *GetVehicleSpriteGroup(EngineID engine, const Vehicle *v)
{
	const SpriteGroup *group;
	CargoID cargo;

	if (v == NULL) {
		cargo = GC_PURCHASE;
	} else {
		cargo = _global_cargo_id[_opt.landscape][v->cargo_type];
		assert(cargo != GC_INVALID);

		if (v->type == VEH_Train) {
			group = GetWagonOverrideSpriteSet(engine, cargo, v->u.rail.first_engine);

			if (group != NULL) return group;
		}
	}

	group = engine_custom_sprites[engine][cargo];
	if (group != NULL) return group;

	/* Fall back to the default set if the selected cargo type is not defined */
	return engine_custom_sprites[engine][GC_DEFAULT];
}


SpriteID GetCustomEngineSprite(EngineID engine, const Vehicle *v, Direction direction)
{
	const SpriteGroup *group;
	ResolverObject object;

	NewVehicleResolver(&object, engine, v);

	group = Resolve(GetVehicleSpriteGroup(engine, v), &object);
	if (group == NULL || group->type != SGT_RESULT) return 0;

	return group->g.result.sprite + (direction % group->g.result.num_sprites);
}


SpriteID GetRotorOverrideSprite(EngineID engine, const Vehicle *v, bool info_view)
{
	const SpriteGroup *group;
	ResolverObject object;

	assert(engine >= AIRCRAFT_ENGINES_INDEX);
	assert(engine < AIRCRAFT_ENGINES_INDEX + NUM_AIRCRAFT_ENGINES);

	/* Only valid for helicopters */
	assert(!(AircraftVehInfo(engine)->subtype & AIR_CTOL));

	NewVehicleResolver(&object, engine, v);

	object.info_view = info_view;

	group = heli_rotor_custom_sprites[engine - AIRCRAFT_ENGINES_INDEX];
	group = Resolve(group, &object);

	if (group == NULL || group->type != SGT_RESULT) return 0;

	if (v == NULL) return group->g.result.sprite;

	return group->g.result.sprite + (info_view ? 0 : (v->next->next->u.air.state % group->g.result.num_sprites));
}


/**
 * Check if a wagon is currently using a wagon override
 * @param v The wagon to check
 * @return true if it is using an override, false otherwise
 */
bool UsesWagonOverride(const Vehicle* v)
{
	assert(v->type == VEH_Train);
	return GetWagonOverrideSpriteSet(v->engine_type, _global_cargo_id[_opt.landscape][v->cargo_type], v->u.rail.first_engine) != NULL;
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
uint16 GetVehicleCallback(uint16 callback, uint32 param1, uint32 param2, EngineID engine, const Vehicle *v)
{
	const SpriteGroup *group;
	ResolverObject object;

	NewVehicleResolver(&object, engine, v);

	object.callback        = callback;
	object.callback_param1 = param1;
	object.callback_param2 = param2;

	group = Resolve(GetVehicleSpriteGroup(engine, v), &object);
	if (group == NULL || group->type != SGT_CALLBACK) return CALLBACK_FAILED;

	return group->g.callback.result;
}

/**
 * Evaluate a newgrf callback for vehicles with a different vehicle for parent scope.
 * @param callback The callback to evalute
 * @param param1   First parameter of the callback
 * @param param2   Second parameter of the callback
 * @param engine   Engine type of the vehicle to evaluate the callback for
 * @param v        The vehicle to evaluate the callback for, or NULL if it doesnt exist yet
 * @param parent   The vehicle to use for parent scope
 * @return The value the callback returned, or CALLBACK_FAILED if it failed
 */
uint16 GetVehicleCallbackParent(uint16 callback, uint32 param1, uint32 param2, EngineID engine, const Vehicle *v, const Vehicle *parent)
{
	const SpriteGroup *group;
	ResolverObject object;

	NewVehicleResolver(&object, engine, v);

	object.callback        = callback;
	object.callback_param1 = param1;
	object.callback_param2 = param2;

	object.u.vehicle.parent = parent;

	group = Resolve(GetVehicleSpriteGroup(engine, v), &object);
	if (group == NULL || group->type != SGT_CALLBACK) return CALLBACK_FAILED;

	return group->g.callback.result;
}

static void DoTriggerVehicle(Vehicle *v, VehicleTrigger trigger, byte base_random_bits, bool first)
{
	const SpriteGroup *group;
	ResolverObject object;
	byte new_random_bits;

	/* We can't trigger a non-existent vehicle... */
	assert(v != NULL);

	NewVehicleResolver(&object, v->engine_type, v);

	object.trigger = trigger;

	group = Resolve(GetVehicleSpriteGroup(v->engine_type, v), &object);

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
static EngineID _engine_list_order[NUM_TRAIN_ENGINES];
static byte _engine_list_position[NUM_TRAIN_ENGINES];

void ResetEngineListOrder(void)
{
	EngineID i;

	for (i = 0; i < NUM_TRAIN_ENGINES; i++) {
		_engine_list_order[i] = i;
		_engine_list_position[i] = i;
	}
}

/**
 * Get the EngineID at position pos.
 * Used when drawing a(n unsorted) list of engines.
 * @param pos List position/
 * @return The EngineID at the requested position.
 */
EngineID GetRailVehAtPosition(EngineID pos)
{
	if (pos < NUM_TRAIN_ENGINES) return _engine_list_order[pos];
	return pos;
}

/**
 * Get the list position of an engine.
 * Used when sorting a list of engines.
 * @param engine ID of the engine.
 * @return The list position of the engine.
 */
uint16 ListPositionOfEngine(EngineID engine)
{
	if (engine < NUM_TRAIN_ENGINES) return _engine_list_position[engine];
	return engine;
}

void AlterRailVehListOrder(EngineID engine, EngineID target)
{
	EngineID i;
	bool moving = false;

	if (engine == target) return;

	// First, remove our ID from the list.
	for (i = 0; i < NUM_TRAIN_ENGINES - 1; i++) {
		if (_engine_list_order[i] == engine) moving = true;
		if (moving) _engine_list_order[i] = _engine_list_order[i + 1];
	}

	// Now, insert it again, before the target engine.
	for (i = NUM_TRAIN_ENGINES - 1; i > 0; i--) {
		_engine_list_order[i] = _engine_list_order[i - 1];
		if (_engine_list_order[i] == target) {
			_engine_list_order[i - 1] = engine;
			break;
		}
	}

	// Update the engine list position (a reverse of engine list order)
	for (i = 0; i < NUM_TRAIN_ENGINES; i++) {
		_engine_list_position[_engine_list_order[i]] = i;
	}
}
