/* $Id$ */

/** @file engine_func.h Functions related to engines. */

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
void SetCachedEngineCounts();

#endif /* ENGINE_H */
