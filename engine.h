/* $Id$ */

#ifndef ENGINE_H
#define ENGINE_H

/** @file engine.h
  */

#include "sprite.h"

typedef struct RailVehicleInfo {
	byte image_index;
	byte flags; /* 1=multihead engine, 2=wagon */
	byte base_cost;
	uint16 max_speed;
	uint16 power;
	uint16 weight;
	byte running_cost_base;
	byte engclass; // 0: steam, 1: diesel, 2: electric
	byte capacity;
	byte cargo_type;
	byte callbackmask; // see CallbackMask enum
	uint16 pow_wag_power;
	byte pow_wag_weight;
	byte visual_effect; // NOTE: this is not 100% implemented yet, at the moment it is only used as a 'fallback' value
	                    //       for when the 'powered wagon' callback fails. But it should really also determine what
	                    //       kind of visual effect to generate for a vehicle (default, steam, diesel, electric).
	                    //       Same goes for the callback result, which atm is only used to check if a wagon is powered.
	byte shorten_factor;	// length on main map for this type is 8 - shorten_factor
} RailVehicleInfo;

typedef struct ShipVehicleInfo {
	byte image_index;
	byte base_cost;
	uint16 max_speed;
	byte cargo_type;
	uint16 capacity;
	byte running_cost;
	byte sfx;
	byte refittable;
} ShipVehicleInfo;

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
	byte cargo_type;
} RoadVehicleInfo;

/** Information about a vehicle
  * @see table/engines.h
  */
typedef struct EngineInfo {
	uint16 base_intro;
	byte unk2;              ///< Carriages have the highest bit set in this one
	byte lifelength;
	byte base_life;
	byte railtype:4;
	byte climates:4;
	uint32 refit_mask;
} EngineInfo;

typedef struct Engine {
	uint16 intro_date;
	uint16 age;
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
	byte type;				// type, ie VEH_Road, VEH_Train, etc. Same as in vehicle.h
} Engine;


enum {
	RVI_MULTIHEAD = 1,
	RVI_WAGON = 2,
};

enum {
	NUM_VEHICLE_TYPES = 6
};

void AddTypeToEngines(void);
void StartupEngines(void);

enum GlobalCargo {
	GC_PASSENGERS   =   0,
	GC_COAL         =   1,
	GC_MAIL         =   2,
	GC_OIL          =   3,
	GC_LIVESTOCK    =   4,
	GC_GOODS        =   5,
	GC_GRAIN        =   6, // GC_WHEAT / GC_MAIZE
	GC_WOOD         =   7,
	GC_IRON_ORE     =   8,
	GC_STEEL        =   9,
	GC_VALUABLES    =  10, // GC_GOLD / GC_DIAMONDS
	GC_PAPER        =  11,
	GC_FOOD         =  12,
	GC_FRUIT        =  13,
	GC_COPPER_ORE   =  14,
	GC_WATER        =  15,
	GC_RUBBER       =  16,
	GC_SUGAR        =  17,
	GC_TOYS         =  18,
	GC_BATTERIES    =  19,
	GC_CANDY        =  20,
	GC_TOFFEE       =  21,
	GC_COLA         =  22,
	GC_COTTON_CANDY =  23,
	GC_BUBBLES      =  24,
	GC_PLASTIC      =  25,
	GC_FIZZY_DRINKS =  26,
	GC_PAPER_TEMP   =  27,
	GC_UNDEFINED    =  28, // undefined; unused slot in arctic climate
	GC_DEFAULT      =  29,
	GC_PURCHASE     =  30,
	GC_INVALID      = 255,
	NUM_GLOBAL_CID  =  31
};

// This enum lists the implemented callbacks
// Use as argument for the GetCallBackResult function (see comments there)
enum CallbackID {
	// Powered wagons, if the result is lower as 0x40 then the wagon is powered
	// TODO: interpret the rest of the result, aka "visual effects"
	CBID_WAGON_POWER = 0x10,

	// Vehicle length, returns the amount of 1/8's the vehicle is shorter
	// only for train vehicles
	CBID_VEH_LENGTH = 0x11,

	// Refit capacity, the passed vehicle needs to have its ->cargo_type set to
	// the cargo we are refitting to, returns the new cargo capacity
	CBID_REFIT_CAP = 0x15,

	CBID_ARTIC_ENGINE = 0x16,
};

// bit positions for rvi->callbackmask, indicates which callbacks are used by an engine
// (some callbacks are always used, and dont appear here)
enum CallbackMask {
	CBM_WAGON_POWER = 0,
	CBM_VEH_LENGTH = 1,
	CBM_REFIT_CAP = 3,
	CBM_ARTIC_ENGINE = 4,
};

enum {
	CALLBACK_FAILED = 0xFFFF
};

VARDEF const uint32 _default_refitmasks[NUM_VEHICLE_TYPES];
VARDEF const CargoID _global_cargo_id[NUM_LANDSCAPE][NUM_CARGO];
VARDEF const uint32 _landscape_global_cargo_mask[NUM_LANDSCAPE];
VARDEF const CargoID _local_cargo_id_ctype[NUM_GLOBAL_CID];
VARDEF const uint32 cargo_classes[16];

void SetWagonOverrideSprites(EngineID engine, struct SpriteGroup *group, byte *train_id, int trains);
void SetCustomEngineSprites(EngineID engine, byte cargo, struct SpriteGroup *group);
// loaded is in percents, overriding_engine 0xffff is none
int GetCustomEngineSprite(EngineID engine, const Vehicle *v, byte direction);
uint16 GetCallBackResult(uint16 callback_info, EngineID engine, const Vehicle *v);
bool UsesWagonOverride(const Vehicle *v);
#define GetCustomVehicleSprite(v, direction) GetCustomEngineSprite(v->engine_type, v, direction)
#define GetCustomVehicleIcon(et, direction) GetCustomEngineSprite(et, NULL, direction)

typedef enum VehicleTrigger {
	VEHICLE_TRIGGER_NEW_CARGO = 1,
	// Externally triggered only for the first vehicle in chain
	VEHICLE_TRIGGER_DEPOT = 2,
	// Externally triggered only for the first vehicle in chain, only if whole chain is empty
	VEHICLE_TRIGGER_EMPTY = 4,
	// Not triggered externally (called for the whole chain if we got NEW_CARGO)
	VEHICLE_TRIGGER_ANY_NEW_CARGO = 8,
} VehicleTrigger;
void TriggerVehicle(Vehicle *veh, VehicleTrigger trigger);

void SetCustomEngineName(EngineID engine, const char *name);
StringID GetCustomEngineName(EngineID engine);


void DrawTrainEngine(int x, int y, EngineID engine, uint32 image_ormod);
void DrawRoadVehEngine(int x, int y, EngineID engine, uint32 image_ormod);
void DrawShipEngine(int x, int y, EngineID engine, uint32 image_ormod);
void DrawAircraftEngine(int x, int y, EngineID engine, uint32 image_ormod);

void AcceptEnginePreview(Engine *e, PlayerID player);

void LoadCustomEngineNames(void);
void DeleteCustomEngineNames(void);

bool IsEngineBuildable(uint engine, byte type);

enum {
	NUM_NORMAL_RAIL_ENGINES = 54,
	NUM_MONORAIL_ENGINES = 30,
	NUM_MAGLEV_ENGINES = 32,
	NUM_TRAIN_ENGINES = NUM_NORMAL_RAIL_ENGINES + NUM_MONORAIL_ENGINES + NUM_MAGLEV_ENGINES,
	NUM_ROAD_ENGINES = 88,
	NUM_SHIP_ENGINES = 11,
	NUM_AIRCRAFT_ENGINES = 41,
	TOTAL_NUM_ENGINES = NUM_TRAIN_ENGINES + NUM_ROAD_ENGINES + NUM_SHIP_ENGINES + NUM_AIRCRAFT_ENGINES,
	AIRCRAFT_ENGINES_INDEX = NUM_TRAIN_ENGINES + NUM_ROAD_ENGINES + NUM_SHIP_ENGINES,
	SHIP_ENGINES_INDEX = NUM_TRAIN_ENGINES + NUM_ROAD_ENGINES,
	ROAD_ENGINES_INDEX = NUM_TRAIN_ENGINES,
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

void UnloadWagonOverrides(void);
void UnloadCustomEngineSprites(void);
void UnloadCustomEngineNames(void);

#endif /* ENGINE_H */
