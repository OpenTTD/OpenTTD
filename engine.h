#ifndef ENGINE_H
#define ENGINE_H

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


struct SpriteGroup {
	// XXX: Would anyone ever need more than 16 spritesets? Maybe we should
	// use even less, now we take whole 8kb for custom sprites table, oh my!
	byte sprites_per_set; // means number of directions - 4 or 8
	// Loaded = in motion, loading = not moving
	// Each group contains several spritesets, for various loading stages
	byte loaded_count;
	uint16 loaded[16]; // sprite ids
	byte loading_count;
	uint16 loading[16]; // sprite ids
};

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
int GetCustomEngineSprite(byte engine, uint16 overriding_engine, byte cargo, byte loaded, byte in_motion, byte direction);
#define GetCustomVehicleSprite(v, direction) \
	GetCustomEngineSprite(v->engine_type, v->type == VEH_Train ? v->u.rail.first_engine : -1, \
	                      _global_cargo_id[_opt.landscape][v->cargo_type], \
	                      ((v->cargo_count + 1) * 100) / (v->cargo_cap + 1), \
	                      !!v->cur_speed, direction)
#define GetCustomVehicleIcon(v, direction) \
	GetCustomEngineSprite(v, -1, CID_PURCHASE, 0, 0, direction)

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
	TOTAL_NUM_ENGINES = NUM_NORMAL_RAIL_ENGINES+NUM_MONORAIL_ENGINES+NUM_MAGLEV_ENGINES+NUM_ROAD_ENGINES+NUM_SHIP_ENGINES+NUM_AIRCRAFT_ENGINES,
	AIRCRAFT_ENGINES_INDEX = NUM_TRAIN_ENGINES + NUM_ROAD_ENGINES + NUM_SHIP_ENGINES,
	SHIP_ENGINES_INDEX = NUM_TRAIN_ENGINES + NUM_ROAD_ENGINES,
	ROAD_ENGINES_INDEX = NUM_TRAIN_ENGINES,
};
VARDEF Engine _engines[TOTAL_NUM_ENGINES];
VARDEF StringID _engine_name_strings[TOTAL_NUM_ENGINES];

extern EngineInfo _engine_info[TOTAL_NUM_ENGINES];
extern RailVehicleInfo _rail_vehicle_info[];
#define ship_vehicle_info(e) _ship_vehicle_info[e - SHIP_ENGINES_INDEX]
extern ShipVehicleInfo _ship_vehicle_info[];

#endif
