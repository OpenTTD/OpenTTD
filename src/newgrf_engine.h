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
#include "newgrf_spritegroup.h"

/** Resolver for a vehicle scope. */
struct VehicleScopeResolver : public ScopeResolver {
	const struct Vehicle *v; ///< The vehicle being resolved.
	EngineID self_type;      ///< Type of the vehicle.
	bool info_view;          ///< Indicates if the item is being drawn in an info window.

	VehicleScopeResolver(ResolverObject &ro, EngineID engine_type, const Vehicle *v, bool info_view);

	void SetVehicle(const Vehicle *v) { this->v = v; }

	/* virtual */ uint32 GetRandomBits() const;
	/* virtual */ uint32 GetVariable(byte variable, uint32 parameter, bool *available) const;
	/* virtual */ uint32 GetTriggers() const;
};

/** Resolver for a vehicle (chain) */
struct VehicleResolverObject : public ResolverObject {
	/** Application of 'wagon overrides'. */
	enum WagonOverride {
		WO_NONE,     //!< Resolve no wagon overrides.
		WO_UNCACHED, //!< Resolve wagon overrides.
		WO_CACHED,   //!< Resolve wagon overrides using TrainCache::cached_override.
		WO_SELF,     //!< Resolve self-override (helicopter rotors and such).
	};

	VehicleScopeResolver self_scope;     ///< Scope resolver for the indicated vehicle.
	VehicleScopeResolver parent_scope;   ///< Scope resolver for its parent vehicle.

	VehicleScopeResolver relative_scope; ///< Scope resolver for an other vehicle in the chain.
	byte cached_relative_count;          ///< Relative position of the other vehicle.

	VehicleResolverObject(EngineID engine_type, const Vehicle *v, WagonOverride wagon_override, bool info_view = false,
			CallbackID callback = CBID_NO_CALLBACK, uint32 callback_param1 = 0, uint32 callback_param2 = 0);

	/* virtual */ ScopeResolver *GetScope(VarSpriteGroupScope scope = VSG_SCOPE_SELF, byte relative = 0);

	/* virtual */ const SpriteGroup *ResolveReal(const RealSpriteGroup *group) const;
};

static const uint TRAININFO_DEFAULT_VEHICLE_WIDTH   = 29;
static const uint ROADVEHINFO_DEFAULT_VEHICLE_WIDTH = 32;
static const uint VEHICLEINFO_FULL_VEHICLE_WIDTH    = 32;

struct VehicleSpriteSeq;

void SetWagonOverrideSprites(EngineID engine, CargoID cargo, const struct SpriteGroup *group, EngineID *train_id, uint trains);
const SpriteGroup *GetWagonOverrideSpriteSet(EngineID engine, CargoID cargo, EngineID overriding_engine);
void SetCustomEngineSprites(EngineID engine, byte cargo, const struct SpriteGroup *group);

void GetCustomEngineSprite(EngineID engine, const Vehicle *v, Direction direction, EngineImageType image_type, VehicleSpriteSeq *result);
#define GetCustomVehicleSprite(v, direction, image_type, result) GetCustomEngineSprite(v->engine_type, v, direction, image_type, result)
#define GetCustomVehicleIcon(et, direction, image_type, result) GetCustomEngineSprite(et, NULL, direction, image_type, result)

void GetRotorOverrideSprite(EngineID engine, const struct Aircraft *v, bool info_view, EngineImageType image_type, VehicleSpriteSeq *result);
#define GetCustomRotorSprite(v, i, image_type, result) GetRotorOverrideSprite(v->engine_type, v, i, image_type, result)
#define GetCustomRotorIcon(et, image_type, result) GetRotorOverrideSprite(et, NULL, true, image_type, result)

/* Forward declaration of GRFFile, to avoid unnecessary inclusion of newgrf.h
 * elsewhere... */
struct GRFFile;

void SetEngineGRF(EngineID engine, const struct GRFFile *file);

uint16 GetVehicleCallback(CallbackID callback, uint32 param1, uint32 param2, EngineID engine, const Vehicle *v);
uint16 GetVehicleCallbackParent(CallbackID callback, uint32 param1, uint32 param2, EngineID engine, const Vehicle *v, const Vehicle *parent);
bool UsesWagonOverride(const Vehicle *v);

/* Handler to Evaluate callback 36. If the callback fails (i.e. most of the
 * time) orig_value is returned */
uint GetVehicleProperty(const Vehicle *v, PropertyID property, uint orig_value);
uint GetEngineProperty(EngineID engine, PropertyID property, uint orig_value, const Vehicle *v = NULL);

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

void AlterVehicleListOrder(EngineID engine, uint target);
void CommitVehicleListOrderChanges();

EngineID GetNewEngineID(const GRFFile *file, VehicleType type, uint16 internal_id);

#endif /* NEWGRF_ENGINE_H */
