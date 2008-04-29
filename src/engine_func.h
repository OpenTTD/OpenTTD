/* $Id$ */

/** @file engine_func.h */

#ifndef ENGINE_H
#define ENGINE_H

#include "engine_type.h"

void SetupEngines();
void StartupEngines();

Engine *GetTempDataEngine(EngineID index);
void CopyTempEngineData();

/* Original engine data counts and offsets */
extern const uint8 _engine_counts[4];
extern const uint8 _engine_offsets[4];

void DrawTrainEngine(int x, int y, EngineID engine, SpriteID pal);
void DrawRoadVehEngine(int x, int y, EngineID engine, SpriteID pal);
void DrawShipEngine(int x, int y, EngineID engine, SpriteID pal);
void DrawAircraftEngine(int x, int y, EngineID engine, SpriteID pal);

void LoadCustomEngineNames();
void DeleteCustomEngineNames();

bool IsEngineBuildable(EngineID engine, VehicleType type, PlayerID player);
CargoID GetEngineCargoType(EngineID engine);

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
