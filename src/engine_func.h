/* $Id$ */

/** @file engine_func.h Functions related to engines. */

#ifndef ENGINE_H
#define ENGINE_H

#include "engine_type.h"

void SetupEngines();
void StartupEngines();

/* Original engine data counts and offsets */
extern const uint8 _engine_counts[4];
extern const uint8 _engine_offsets[4];

void DrawTrainEngine(int x, int y, EngineID engine, SpriteID pal);
void DrawRoadVehEngine(int x, int y, EngineID engine, SpriteID pal);
void DrawShipEngine(int x, int y, EngineID engine, SpriteID pal);
void DrawAircraftEngine(int x, int y, EngineID engine, SpriteID pal);

bool IsEngineBuildable(EngineID engine, VehicleType type, CompanyID company);
bool IsEngineRefittable(EngineID engine);
void SetCachedEngineCounts();
void SetYearEngineAgingStops();
void StartupOneEngine(Engine *e, Date aging_date);

uint GetTotalCapacityOfArticulatedParts(EngineID engine, VehicleType type);

#endif /* ENGINE_H */
