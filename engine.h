#ifndef ENGINE_H
#define ENGINE_H

#include "sprite.h"

typedef struct RailVehicleInfo {
	byte image_index;
	byte flags; /* 1=multihead engine, 2=wagon */
	byte base_cost;
	uint16 max_speed;
	uint16 power;
	byte weight;
	byte running_cost_base;
	byte engclass; // 0: steam, 1: diesel, 2: electric
	byte capacity;
	byte cargo_type;
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
	uint16 passanger_capacity;
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

typedef struct EngineInfo {
	uint16 base_intro;
	byte unk2;
	byte lifelength;
	byte base_life;
	byte railtype_climates;
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
} Engine;


enum {
	RVI_MULTIHEAD = 1,
	RVI_WAGON = 2,
};


void StartupEngines();


extern byte _global_cargo_id[NUM_LANDSCAPE][NUM_CARGO];
enum {
	CID_DEFAULT = 29,
	CID_PURCHASE = 30,
	NUM_CID = 31,
};
extern byte _local_cargo_id_ctype[NUM_CID];
extern byte _local_cargo_id_landscape[NUM_CID];

extern uint32 _engine_refit_masks[256];

extern byte _engine_original_sprites[256];
void SetWagonOverrideSprites(byte engine, struct SpriteGroup *group, byte *train_id, int trains);
void SetCustomEngineSprites(byte engine, byte cargo, struct SpriteGroup *group);
// loaded is in percents, overriding_engine 0xffff is none
int GetCustomEngineSprite(byte engine, Vehicle *v, byte direction);
#define GetCustomVehicleSprite(v, direction) GetCustomEngineSprite(v->engine_type, v, direction)
#define GetCustomVehicleIcon(et, direction) GetCustomEngineSprite(et, NULL, direction)

enum VehicleTrigger {
	VEHICLE_TRIGGER_NEW_CARGO = 1,
	// Externally triggered only for the first vehicle in chain
	VEHICLE_TRIGGER_DEPOT = 2,
	// Externally triggered only for the first vehicle in chain, only if whole chain is empty
	VEHICLE_TRIGGER_EMPTY = 4,
	// Not triggered externally (called for the whole chain if we got NEW_CARGO)
	VEHICLE_TRIGGER_ANY_NEW_CARGO = 8,
};
void TriggerVehicle(Vehicle *veh, enum VehicleTrigger trigger);

void SetCustomEngineName(int engine, char *name);
StringID GetCustomEngineName(int engine);


void DrawTrainEngine(int x, int y, int engine, uint32 image_ormod);
void DrawRoadVehEngine(int x, int y, int engine, uint32 image_ormod);
void DrawShipEngine(int x, int y, int engine, uint32 image_ormod);
void DrawAircraftEngine(int x, int y, int engine, uint32 image_ormod);

void DrawTrainEngineInfo(int engine, int x, int y, int maxw);
void DrawRoadVehEngineInfo(int engine, int x, int y, int maxw);
void DrawShipEngineInfo(int engine, int x, int y, int maxw);
void DrawAircraftEngineInfo(int engine, int x, int y, int maxw);

void AcceptEnginePreview(Engine *e, int player);

void LoadCustomEngineNames();
void DeleteCustomEngineNames();


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
#define DEREF_ENGINE(i) (&_engines[i])
VARDEF StringID _engine_name_strings[TOTAL_NUM_ENGINES];

/* Access Vehicle Data */
//#include "table/engines.h"
extern EngineInfo _engine_info[TOTAL_NUM_ENGINES];
extern RailVehicleInfo _rail_vehicle_info[NUM_TRAIN_ENGINES];
extern ShipVehicleInfo _ship_vehicle_info[NUM_SHIP_ENGINES];
extern AircraftVehicleInfo _aircraft_vehicle_info[NUM_AIRCRAFT_ENGINES];
extern RoadVehicleInfo _road_vehicle_info[NUM_ROAD_ENGINES];

static inline RailVehicleInfo *RailVehInfo(uint e)
{
	assert(e < lengthof(_rail_vehicle_info));
	return &_rail_vehicle_info[e];
}

static inline ShipVehicleInfo *ShipVehInfo(uint e)
{
	assert(e - SHIP_ENGINES_INDEX < lengthof(_ship_vehicle_info));
	return &_ship_vehicle_info[e - SHIP_ENGINES_INDEX];
}

static inline AircraftVehicleInfo *AircraftVehInfo(uint e)
{
	assert(e - AIRCRAFT_ENGINES_INDEX < lengthof(_aircraft_vehicle_info));
	return &_aircraft_vehicle_info[e - AIRCRAFT_ENGINES_INDEX];
}

static inline RoadVehicleInfo *RoadVehInfo(uint e)
{
	assert(e - ROAD_ENGINES_INDEX < lengthof(_road_vehicle_info));
	return &_road_vehicle_info[e - ROAD_ENGINES_INDEX];
}

#endif
