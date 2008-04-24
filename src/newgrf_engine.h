/* $Id$ */

/** @file newgrf_engine.h */

#ifndef NEWGRF_ENGINE_H
#define NEWGRF_ENGINE_H

#include "newgrf.h"
#include "direction_type.h"
#include "newgrf_cargo.h"

extern int _traininfo_vehicle_pitch;
extern int _traininfo_vehicle_width;


void SetWagonOverrideSprites(EngineID engine, CargoID cargo, const struct SpriteGroup *group, EngineID *train_id, uint trains);
const SpriteGroup *GetWagonOverrideSpriteSet(EngineID engine, CargoID cargo, EngineID overriding_engine);
void SetCustomEngineSprites(EngineID engine, byte cargo, const struct SpriteGroup *group);
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

uint16 GetVehicleCallback(CallbackID callback, uint32 param1, uint32 param2, EngineID engine, const Vehicle *v);
uint16 GetVehicleCallbackParent(CallbackID callback, uint32 param1, uint32 param2, EngineID engine, const Vehicle *v, const Vehicle *parent);
bool UsesWagonOverride(const Vehicle *v);
#define GetCustomVehicleSprite(v, direction) GetCustomEngineSprite(v->engine_type, v, direction)
#define GetCustomVehicleIcon(et, direction) GetCustomEngineSprite(et, NULL, direction)

/* Handler to Evaluate callback 36. If the callback fails (i.e. most of the
 * time) orig_value is returned */
uint GetVehicleProperty(const Vehicle *v, uint8 property, uint orig_value);
uint GetEngineProperty(EngineID engine, uint8 property, uint orig_value);

enum VehicleTrigger {
	VEHICLE_TRIGGER_NEW_CARGO     = 0x01,
	/* Externally triggered only for the first vehicle in chain */
	VEHICLE_TRIGGER_DEPOT         = 0x02,
	/* Externally triggered only for the first vehicle in chain, only if whole chain is empty */
	VEHICLE_TRIGGER_EMPTY         = 0x04,
	/* Not triggered externally (called for the whole chain if we got NEW_CARGO) */
	VEHICLE_TRIGGER_ANY_NEW_CARGO = 0x08,
	/* Externally triggered for each vehicle in chain */
	VEHICLE_TRIGGER_CALLBACK_32   = 0x10,
};
void TriggerVehicle(Vehicle *veh, VehicleTrigger trigger);

void UnloadWagonOverrides();
void UnloadCustomEngineSprites();

void ResetEngineListOrder();
uint16 ListPositionOfEngine(EngineID engine);
void AlterRailVehListOrder(EngineID engine, EngineID target);

#endif /* NEWGRF_ENGINE_H */
