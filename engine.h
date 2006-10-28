/* $Id$ */

#ifndef ENGINE_H
#define ENGINE_H

/** @file engine.h */

#include "pool.h"

typedef struct RailVehicleInfo {
	byte image_index;
	byte flags; /* 1=multihead engine, 2=wagon */
	byte base_cost;
	uint16 max_speed;
	uint16 power;
	uint16 weight;
	byte running_cost_base;
	byte running_cost_class;
	byte engclass; // 0: steam, 1: diesel, 2: electric
	byte capacity;
	CargoID cargo_type;
	byte ai_rank;
	uint16 pow_wag_power;
	byte pow_wag_weight;
	byte visual_effect; // NOTE: this is not 100% implemented yet, at the moment it is only used as a 'fallback' value
	                    //       for when the 'powered wagon' callback fails. But it should really also determine what
	                    //       kind of visual effect to generate for a vehicle (default, steam, diesel, electric).
	                    //       Same goes for the callback result, which atm is only used to check if a wagon is powered.
	byte shorten_factor; // length on main map for this type is 8 - shorten_factor
	byte user_def_data; ///! Property 0x25: "User-defined bit mask" Used only for (very few) NewGRF vehicles
} RailVehicleInfo;

typedef struct ShipVehicleInfo {
	byte image_index;
	byte base_cost;
	uint16 max_speed;
	CargoID cargo_type;
	uint16 capacity;
	byte running_cost;
	byte sfx;
	byte refittable;
} ShipVehicleInfo;

// Aircraft subtypes
enum {
	AIR_CTOL = 1, // Conventional Take Off and Landing, i.e. planes
	AIR_FAST = 2
};

typedef struct AircraftVehicleInfo {
	byte image_index;
	byte base_cost;
	byte running_cost;
	byte subtype;
	byte sfx;
	byte acceleration;
	byte max_speed;
	byte mail_capacity;
	uint16 passenger_capacity;
} AircraftVehicleInfo;

typedef struct RoadVehicleInfo {
	byte image_index;
	byte base_cost;
	byte running_cost;
	byte sfx;
	byte max_speed;
	byte capacity;
	CargoID cargo_type;
} RoadVehicleInfo;

/** Information about a vehicle
 *  @see table/engines.h
 */
typedef struct EngineInfo {
	Date base_intro;
	byte unk2;              ///< Carriages have the highest bit set in this one
	Year lifelength;
	Year base_life;
	byte railtype:4;
	byte climates:4;
	uint32 refit_mask;
	byte refit_cost;
	byte misc_flags;
	byte callbackmask;
} EngineInfo;

typedef struct Engine {
	Date intro_date;
	Date age;
	uint16 reliability;
	uint16 reliability_spd_dec;
	uint16 reliability_start, reliability_max, reliability_final;
	uint16 duration_phase_1, duration_phase_2, duration_phase_3;
	byte lifelength;
	byte flags;
	byte preview_player;
	byte preview_wait;
	byte railtype;
	byte player_avail;
	byte type; // type, ie VEH_Road, VEH_Train, etc. Same as in vehicle.h
} Engine;

/**
 * EngineInfo.misc_flags is a bitmask, with the following values
 */
enum {
	EF_RAIL_TILTS = 0, ///< Rail vehicle tilts in curves (unsupported)
	EF_ROAD_TRAM  = 0, ///< Road vehicle is a tram/light rail vehicle (unsup)
	EF_USES_2CC   = 1, ///< Vehicle uses two company colours
	EF_RAIL_IS_MU = 2, ///< Rail vehicle is a multiple-unit (DMU/EMU)
};

enum {
	RVI_MULTIHEAD = 1,
	RVI_WAGON = 2,
};

enum {
	NUM_VEHICLE_TYPES = 6
};

enum {
	INVALID_ENGINE = 0xFFFF,
};

void AddTypeToEngines(void);
void StartupEngines(void);


VARDEF const uint32 _default_refitmasks[NUM_VEHICLE_TYPES];

void DrawTrainEngine(int x, int y, EngineID engine, uint32 image_ormod);
void DrawRoadVehEngine(int x, int y, EngineID engine, uint32 image_ormod);
void DrawShipEngine(int x, int y, EngineID engine, uint32 image_ormod);
void DrawAircraftEngine(int x, int y, EngineID engine, uint32 image_ormod);

void LoadCustomEngineNames(void);
void DeleteCustomEngineNames(void);

bool IsEngineBuildable(uint engine, byte type);

enum {
	NUM_NORMAL_RAIL_ENGINES = 54,
	NUM_MONORAIL_ENGINES    = 30,
	NUM_MAGLEV_ENGINES      = 32,
	NUM_TRAIN_ENGINES       = NUM_NORMAL_RAIL_ENGINES + NUM_MONORAIL_ENGINES + NUM_MAGLEV_ENGINES,
	NUM_ROAD_ENGINES        = 88,
	NUM_SHIP_ENGINES        = 11,
	NUM_AIRCRAFT_ENGINES    = 41,
	TOTAL_NUM_ENGINES       = NUM_TRAIN_ENGINES + NUM_ROAD_ENGINES + NUM_SHIP_ENGINES + NUM_AIRCRAFT_ENGINES,
	AIRCRAFT_ENGINES_INDEX  = NUM_TRAIN_ENGINES + NUM_ROAD_ENGINES + NUM_SHIP_ENGINES,
	SHIP_ENGINES_INDEX      = NUM_TRAIN_ENGINES + NUM_ROAD_ENGINES,
	ROAD_ENGINES_INDEX      = NUM_TRAIN_ENGINES,
};
VARDEF Engine _engines[TOTAL_NUM_ENGINES];
#define FOR_ALL_ENGINES(e) for (e = _engines; e != endof(_engines); e++)

static inline Engine* GetEngine(EngineID i)
{
	assert(i < lengthof(_engines));
	return &_engines[i];
}

VARDEF StringID _engine_name_strings[TOTAL_NUM_ENGINES];

static inline bool IsEngineIndex(uint index)
{
	return index < TOTAL_NUM_ENGINES;
}

/* Access Vehicle Data */
//#include "table/engines.h"
extern const EngineInfo orig_engine_info[TOTAL_NUM_ENGINES];
extern const RailVehicleInfo orig_rail_vehicle_info[NUM_TRAIN_ENGINES];
extern const ShipVehicleInfo orig_ship_vehicle_info[NUM_SHIP_ENGINES];
extern const AircraftVehicleInfo orig_aircraft_vehicle_info[NUM_AIRCRAFT_ENGINES];
extern const RoadVehicleInfo orig_road_vehicle_info[NUM_ROAD_ENGINES];

extern EngineInfo _engine_info[TOTAL_NUM_ENGINES];
extern RailVehicleInfo _rail_vehicle_info[NUM_TRAIN_ENGINES];
extern ShipVehicleInfo _ship_vehicle_info[NUM_SHIP_ENGINES];
extern AircraftVehicleInfo _aircraft_vehicle_info[NUM_AIRCRAFT_ENGINES];
extern RoadVehicleInfo _road_vehicle_info[NUM_ROAD_ENGINES];

static inline const EngineInfo *EngInfo(EngineID e)
{
	assert(e < lengthof(_engine_info));
	return &_engine_info[e];
}

static inline const RailVehicleInfo* RailVehInfo(EngineID e)
{
	assert(e < lengthof(_rail_vehicle_info));
	return &_rail_vehicle_info[e];
}

static inline const ShipVehicleInfo* ShipVehInfo(EngineID e)
{
	assert(e >= SHIP_ENGINES_INDEX && e < SHIP_ENGINES_INDEX + lengthof(_ship_vehicle_info));
	return &_ship_vehicle_info[e - SHIP_ENGINES_INDEX];
}

static inline const AircraftVehicleInfo* AircraftVehInfo(EngineID e)
{
	assert(e >= AIRCRAFT_ENGINES_INDEX && e < AIRCRAFT_ENGINES_INDEX + lengthof(_aircraft_vehicle_info));
	return &_aircraft_vehicle_info[e - AIRCRAFT_ENGINES_INDEX];
}

static inline const RoadVehicleInfo* RoadVehInfo(EngineID e)
{
	assert(e >= ROAD_ENGINES_INDEX && e < ROAD_ENGINES_INDEX + lengthof(_road_vehicle_info));
	return &_road_vehicle_info[e - ROAD_ENGINES_INDEX];
}

/************************************************************************
 * Engine Replacement stuff
 ************************************************************************/

/**
 * Struct to store engine replacements. DO NOT USE outside of engine.c. Is
 * placed here so the only exception to this rule, the saveload code, can use
 * it.
 */
struct EngineRenew {
	EngineRenewID index;
	EngineID from;
	EngineID to;
	struct EngineRenew *next;
};

typedef struct EngineRenew EngineRenew;

/**
 * Memory pool for engine renew elements. DO NOT USE outside of engine.c. Is
 * placed here so the only exception to this rule, the saveload code, can use
 * it.
 */
DECLARE_POOL(EngineRenew, EngineRenew, 3, 8000)

/**
 * Check if a EngineRenew really exists.
 */
static inline bool IsValidEngineRenew(const EngineRenew *er)
{
	return er->from != INVALID_ENGINE;
}

static inline void DeleteEngineRenew(EngineRenew *er)
{
	er->from = INVALID_ENGINE;
}

#define FOR_ALL_ENGINE_RENEWS_FROM(er, start) for (er = GetEngineRenew(start); er != NULL; er = (er->index + 1U < GetEngineRenewPoolSize()) ? GetEngineRenew(er->index + 1U) : NULL) if (er->from != INVALID_ENGINE) if (IsValidEngineRenew(er))
#define FOR_ALL_ENGINE_RENEWS(er) FOR_ALL_ENGINE_RENEWS_FROM(er, 0)


/**
 * A list to group EngineRenew directives together (such as per-player).
 */
typedef EngineRenew* EngineRenewList;

/**
 * Remove all engine replacement settings for the player.
 * @param  er The renewlist for a given player.
 * @return The new renewlist for the player.
 */
void RemoveAllEngineReplacement(EngineRenewList* erl);

/**
 * Retrieve the engine replacement in a given renewlist for an original engine type.
 * @param  erl The renewlist to search in.
 * @param  engine Engine type to be replaced.
 * @return The engine type to replace with, or INVALID_ENGINE if no
 * replacement is in the list.
 */
EngineID EngineReplacement(EngineRenewList erl, EngineID engine);

/**
 * Add an engine replacement to the given renewlist.
 * @param erl The renewlist to add to.
 * @param old_engine The original engine type.
 * @param new_engine The replacement engine type.
 * @param flags The calling command flags.
 * @return 0 on success, CMD_ERROR on failure.
 */
int32 AddEngineReplacement(EngineRenewList* erl, EngineID old_engine, EngineID new_engine, uint32 flags);

/**
 * Remove an engine replacement from a given renewlist.
 * @param erl The renewlist from which to remove the replacement
 * @param engine The original engine type.
 * @param flags The calling command flags.
 * @return 0 on success, CMD_ERROR on failure.
 */
int32 RemoveEngineReplacement(EngineRenewList* erl, EngineID engine, uint32 flags);

#endif /* ENGINE_H */
