/* $Id$ */

/** @file engine.h */

#ifndef ENGINE_H
#define ENGINE_H

#include "engine_type.h"

void SetupEngines();
void StartupEngines();


void DrawTrainEngine(int x, int y, EngineID engine, SpriteID pal);
void DrawRoadVehEngine(int x, int y, EngineID engine, SpriteID pal);
void DrawShipEngine(int x, int y, EngineID engine, SpriteID pal);
void DrawAircraftEngine(int x, int y, EngineID engine, SpriteID pal);

void LoadCustomEngineNames();
void DeleteCustomEngineNames();

bool IsEngineBuildable(EngineID engine, VehicleType type, PlayerID player);
CargoID GetEngineCargoType(EngineID engine);

static inline EngineID GetFirstEngineOfType(VehicleType type)
{
	const EngineID start[] = {0, ROAD_ENGINES_INDEX, SHIP_ENGINES_INDEX, AIRCRAFT_ENGINES_INDEX};

	return start[type];
}

static inline EngineID GetLastEngineOfType(VehicleType type)
{
	const EngineID end[] = {
		NUM_TRAIN_ENGINES,
		ROAD_ENGINES_INDEX + NUM_ROAD_ENGINES,
		SHIP_ENGINES_INDEX + NUM_SHIP_ENGINES,
		AIRCRAFT_ENGINES_INDEX + NUM_AIRCRAFT_ENGINES};

	return end[type];
}

extern Engine _engines[TOTAL_NUM_ENGINES];
#define FOR_ALL_ENGINES(e) for (e = _engines; e != endof(_engines); e++)
#define FOR_ALL_ENGINEIDS_OF_TYPE(e, type) for (e = GetFirstEngineOfType(type); e != GetLastEngineOfType(type); e++)


static inline Engine* GetEngine(EngineID i)
{
	assert(i < lengthof(_engines));
	return &_engines[i];
}

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
