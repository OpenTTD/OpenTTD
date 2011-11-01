/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file newgrf_engine.h Functions for NewGRF engines. */

#ifndef NEWGRF_ENGINE_H
#define NEWGRF_ENGINE_H

#include "direction_type.h"
#include "newgrf_callbacks.h"
#include "newgrf_properties.h"
#include "vehicle_type.h"
#include "engine_type.h"
#include "gfx_type.h"

static const uint TRAININFO_DEFAULT_VEHICLE_WIDTH   = 29;
static const uint ROADVEHINFO_DEFAULT_VEHICLE_WIDTH = 28;
static const uint VEHICLEINFO_FULL_VEHICLE_WIDTH    = 32;

void SetWagonOverrideSprites(EngineID engine, CargoID cargo, const struct SpriteGroup *group, EngineID *train_id, uint trains);
const SpriteGroup *GetWagonOverrideSpriteSet(EngineID engine, CargoID cargo, EngineID overriding_engine);
void SetCustomEngineSprites(EngineID engine, byte cargo, const struct SpriteGroup *group);
SpriteID GetCustomEngineSprite(EngineID engine, const Vehicle *v, Direction direction);
SpriteID GetRotorOverrideSprite(EngineID engine, const struct Aircraft *v, bool info_view);
#define GetCustomRotorSprite(v, i) GetRotorOverrideSprite(v->engine_type, v, i)
#define GetCustomRotorIcon(et) GetRotorOverrideSprite(et, NULL, true)

/* Forward declaration of GRFFile, to avoid unnecessary inclusion of newgrf.h
 * elsewhere... */
struct GRFFile;

void SetEngineGRF(EngineID engine, const struct GRFFile *file);

uint16 GetVehicleCallback(CallbackID callback, uint32 param1, uint32 param2, EngineID engine, const Vehicle *v);
uint16 GetVehicleCallbackParent(CallbackID callback, uint32 param1, uint32 param2, EngineID engine, const Vehicle *v, const Vehicle *parent);
bool UsesWagonOverride(const Vehicle *v);
#define GetCustomVehicleSprite(v, direction) GetCustomEngineSprite(v->engine_type, v, direction)
#define GetCustomVehicleIcon(et, direction) GetCustomEngineSprite(et, NULL, direction)

/* Handler to Evaluate callback 36. If the callback fails (i.e. most of the
 * time) orig_value is returned */
uint GetVehicleProperty(const Vehicle *v, PropertyID property, uint orig_value);
uint GetEngineProperty(EngineID engine, PropertyID property, uint orig_value);

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

void UnloadWagonOverrides(Engine *e);

uint ListPositionOfEngine(EngineID engine);
void AlterVehicleListOrder(EngineID engine, EngineID target);
void CommitVehicleListOrderChanges();

EngineID GetNewEngineID(const GRFFile *file, VehicleType type, uint16 internal_id);

#endif /* NEWGRF_ENGINE_H */
