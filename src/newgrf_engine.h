/* $Id$ */

#ifndef NEWGRF_ENGINE_H
#define NEWGRF_ENGINE_H

#include "newgrf.h"
#include "direction.h"
#include "newgrf_cargo.h"

/** @file newgrf_engine.h
 */

extern int _traininfo_vehicle_pitch;
extern int _traininfo_vehicle_width;


void SetWagonOverrideSprites(EngineID engine, CargoID cargo, const struct SpriteGroup *group, byte *train_id, int trains);
void SetCustomEngineSprites(EngineID engine, byte cargo, const struct SpriteGroup *group);
void SetRotorOverrideSprites(EngineID engine, const struct SpriteGroup *group);
SpriteID GetCustomEngineSprite(EngineID engine, const Vehicle* v, Direction direction);
SpriteID GetRotorOverrideSprite(EngineID engine, const Vehicle* v, bool info_view);
#define GetCustomRotorSprite(v, i) GetRotorOverrideSprite(v->engine_type, v, i)
#define GetCustomRotorIcon(et) GetRotorOverrideSprite(et, NULL, true)

/* Forward declaration of GRFFile, to avoid unnecessary inclusion of newgrf.h
 * elsewhere... */
struct GRFFile;

void SetEngineGRF(EngineID engine, const struct GRFFile *file);
const struct GRFFile *GetEngineGRF(EngineID engine);
uint32 GetEngineGRFID(EngineID engine);

uint16 GetVehicleCallback(uint16 callback, uint32 param1, uint32 param2, EngineID engine, const Vehicle *v);
uint16 GetVehicleCallbackParent(uint16 callback, uint32 param1, uint32 param2, EngineID engine, const Vehicle *v, const Vehicle *parent);
bool UsesWagonOverride(const Vehicle *v);
#define GetCustomVehicleSprite(v, direction) GetCustomEngineSprite(v->engine_type, v, direction)
#define GetCustomVehicleIcon(et, direction) GetCustomEngineSprite(et, NULL, direction)

typedef enum VehicleTrigger {
	VEHICLE_TRIGGER_NEW_CARGO     = 1,
	// Externally triggered only for the first vehicle in chain
	VEHICLE_TRIGGER_DEPOT         = 2,
	// Externally triggered only for the first vehicle in chain, only if whole chain is empty
	VEHICLE_TRIGGER_EMPTY         = 4,
	// Not triggered externally (called for the whole chain if we got NEW_CARGO)
	VEHICLE_TRIGGER_ANY_NEW_CARGO = 8,
} VehicleTrigger;
void TriggerVehicle(Vehicle *veh, VehicleTrigger trigger);

void SetCustomEngineName(EngineID engine, StringID name);
StringID GetCustomEngineName(EngineID engine);

void UnloadWagonOverrides();
void UnloadRotorOverrideSprites();
void UnloadCustomEngineSprites();
void UnloadCustomEngineNames();

void ResetEngineListOrder();
EngineID GetRailVehAtPosition(EngineID pos);
uint16 ListPositionOfEngine(EngineID engine);
void AlterRailVehListOrder(EngineID engine, EngineID target);

#endif /* NEWGRF_ENGINE_H */
