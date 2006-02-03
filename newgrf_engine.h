/* $Id$ */

#ifndef NEWGRF_ENGINE_H
#define NEWGRF_ENGINE_H

/** @file newgrf_engine.h
 */

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

void UnloadWagonOverrides(void);
void UnloadCustomEngineSprites(void);
void UnloadCustomEngineNames(void);

#endif /* NEWGRF_ENGINE_H */
