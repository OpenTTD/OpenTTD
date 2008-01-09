/* $Id$ */

/** @file engine.h */

#ifndef ENGINE_H
#define ENGINE_H

#include "oldpool.h"
#include "rail_type.h"
#include "cargo_type.h"
#include "vehicle_type.h"
#include "command_type.h"
#include "gfx_type.h"
#include "date_type.h"
#include "sound_type.h"

enum RailVehicleTypes {
	RAILVEH_SINGLEHEAD,  ///< indicates a "standalone" locomotive
	RAILVEH_MULTIHEAD,   ///< indicates a combination of two locomotives
	RAILVEH_WAGON,       ///< simple wagon, not motorized
};

enum EngineClass {
	EC_STEAM,
	EC_DIESEL,
	EC_ELECTRIC,
	EC_MONORAIL,
	EC_MAGLEV,
};

struct RailVehicleInfo {
	byte image_index;
	RailVehicleTypes railveh_type;
	byte base_cost;
	RailTypeByte railtype;
	uint16 max_speed;
	uint16 power;
	uint16 weight;
	byte running_cost_base;
	byte running_cost_class;
	EngineClass engclass;           ///< Class of engine for this vehicle
	byte capacity;
	CargoID cargo_type;
	byte ai_rank;
	uint16 pow_wag_power;
	byte pow_wag_weight;
	byte visual_effect; // NOTE: this is not 100% implemented yet, at the moment it is only used as a 'fallback' value
	                    //       for when the 'powered wagon' callback fails. But it should really also determine what
	                    //       kind of visual effect to generate for a vehicle (default, steam, diesel, electric).
	                    //       Same goes for the callback result, which atm is only used to check if a wagon is powered.
	byte shorten_factor;   ///< length on main map for this type is 8 - shorten_factor
	byte tractive_effort;  ///< Tractive effort coefficient
	byte user_def_data;    ///< Property 0x25: "User-defined bit mask" Used only for (very few) NewGRF vehicles
};

struct ShipVehicleInfo {
	byte image_index;
	byte base_cost;
	uint16 max_speed;
	CargoID cargo_type;
	uint16 capacity;
	byte running_cost;
	SoundFxByte sfx;
	bool refittable;
};

/* AircraftVehicleInfo subtypes, bitmask type.
 * If bit 0 is 0 then it is a helicopter, otherwise it is a plane
 * in which case bit 1 tells us whether it's a big(fast) plane or not */
enum {
	AIR_HELI = 0,
	AIR_CTOL = 1, ///< Conventional Take Off and Landing, i.e. planes
	AIR_FAST = 2
};

struct AircraftVehicleInfo {
	byte image_index;
	byte base_cost;
	byte running_cost;
	byte subtype;
	SoundFxByte sfx;
	byte acceleration;
	uint16 max_speed;
	byte mail_capacity;
	uint16 passenger_capacity;
};

struct RoadVehicleInfo {
	byte image_index;
	byte base_cost;
	byte running_cost;
	SoundFxByte sfx;
	byte max_speed;
	byte capacity;
	CargoID cargo_type;
};

/** Information about a vehicle
 *  @see table/engines.h
 */
struct EngineInfo {
	Date base_intro;
	Year lifelength;
	Year base_life;
	byte unk2;         ///< flag for carriage(bit 7) and decay speed(bits0..6)
	byte load_amount;
	byte climates;
	uint32 refit_mask;
	byte refit_cost;
	byte misc_flags;
	byte callbackmask;
	byte retire_early; ///< Number of years early to retire vehicle
};

struct Engine {
	Date intro_date;
	Date age;
	uint16 reliability;
	uint16 reliability_spd_dec;
	uint16 reliability_start, reliability_max, reliability_final;
	uint16 duration_phase_1, duration_phase_2, duration_phase_3;
	byte lifelength;
	byte flags;
	PlayerByte preview_player;
	byte preview_wait;
	byte player_avail;
	VehicleType type; ///< type, ie VEH_ROAD, VEH_TRAIN, etc.
};

/**
 * EngineInfo.misc_flags is a bitmask, with the following values
 */
enum {
	EF_RAIL_TILTS = 0, ///< Rail vehicle tilts in curves
	EF_ROAD_TRAM  = 0, ///< Road vehicle is a tram/light rail vehicle
	EF_USES_2CC   = 1, ///< Vehicle uses two company colours
	EF_RAIL_IS_MU = 2, ///< Rail vehicle is a multiple-unit (DMU/EMU)
};

/**
 * Engine.flags is a bitmask, with the following values.
 */
enum {
	ENGINE_AVAILABLE         = 1, ///< This vehicle is available to everyone.
	ENGINE_EXCLUSIVE_PREVIEW = 2, ///< This vehicle is in the exclusive preview stage, either being used or being offered to a player.
	ENGINE_OFFER_WINDOW_OPEN = 4, ///< The exclusive offer window is currently open for a player.
};

enum {
	NUM_VEHICLE_TYPES = 6
};

static const EngineID INVALID_ENGINE = 0xFFFF;


void SetupEngines();
void StartupEngines();


void DrawTrainEngine(int x, int y, EngineID engine, SpriteID pal);
void DrawRoadVehEngine(int x, int y, EngineID engine, SpriteID pal);
void DrawShipEngine(int x, int y, EngineID engine, SpriteID pal);
void DrawAircraftEngine(int x, int y, EngineID engine, SpriteID pal);

void LoadCustomEngineNames();
void DeleteCustomEngineNames();

bool IsEngineBuildable(EngineID engine, byte type, PlayerID player);
CargoID GetEngineCargoType(EngineID engine);

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

static inline EngineID GetFirstEngineOfType(byte type)
{
	const EngineID start[] = {0, ROAD_ENGINES_INDEX, SHIP_ENGINES_INDEX, AIRCRAFT_ENGINES_INDEX};

	return start[type];
}

static inline EngineID GetLastEngineOfType(byte type)
{
	const EngineID end[] = {
		NUM_TRAIN_ENGINES,
		ROAD_ENGINES_INDEX + NUM_ROAD_ENGINES,
		SHIP_ENGINES_INDEX + NUM_SHIP_ENGINES,
		AIRCRAFT_ENGINES_INDEX + NUM_AIRCRAFT_ENGINES};

	return end[type];
}

VARDEF Engine _engines[TOTAL_NUM_ENGINES];
#define FOR_ALL_ENGINES(e) for (e = _engines; e != endof(_engines); e++)
#define FOR_ALL_ENGINEIDS_OF_TYPE(e, type) for (e = GetFirstEngineOfType(type); e != GetLastEngineOfType(type); e++)


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
extern const EngineInfo _orig_engine_info[TOTAL_NUM_ENGINES];
extern const RailVehicleInfo _orig_rail_vehicle_info[NUM_TRAIN_ENGINES];
extern const ShipVehicleInfo _orig_ship_vehicle_info[NUM_SHIP_ENGINES];
extern const AircraftVehicleInfo _orig_aircraft_vehicle_info[NUM_AIRCRAFT_ENGINES];
extern const RoadVehicleInfo _orig_road_vehicle_info[NUM_ROAD_ENGINES];

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

/* Engine list manipulators - current implementation is only C wrapper of CBlobT<EngineID> class (helpers.cpp) */
void EngList_Create(EngineList *el);            ///< Creates engine list
void EngList_Destroy(EngineList *el);           ///< Deallocate and destroy engine list
uint EngList_Count(const EngineList *el);       ///< Returns number of items in the engine list
void EngList_Add(EngineList *el, EngineID eid); ///< Append one item at the end of engine list
EngineID* EngList_Items(EngineList *el);        ///< Returns engine list items as C array
void EngList_RemoveAll(EngineList *el);         ///< Removes all items from engine list
typedef int CDECL EngList_SortTypeFunction(const void*, const void*); ///< argument type for EngList_Sort()
void EngList_Sort(EngineList *el, EngList_SortTypeFunction compare); ///< qsort of the engine list
void EngList_SortPartial(EngineList *el, EngList_SortTypeFunction compare, uint begin, uint num_items); ///< qsort of specified portion of the engine list

#endif /* ENGINE_H */
