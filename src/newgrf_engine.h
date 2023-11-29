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
	bool rotor_in_gui;       ///< Helicopter rotor is drawn in GUI.

	/**
	 * Scope resolver of a single vehicle.
	 * @param ro Surrounding resolver.
	 * @param engine_type Engine type
	 * @param v %Vehicle being resolved.
	 * @param rotor_in_gui Helicopter rotor is drawn in GUI.
	 */
	VehicleScopeResolver(ResolverObject &ro, EngineID engine_type, const Vehicle *v, bool rotor_in_gui)
		: ScopeResolver(ro), v(v), self_type(engine_type), rotor_in_gui(rotor_in_gui)
	{
	}

	void SetVehicle(const Vehicle *v) { this->v = v; }

	uint32_t GetRandomBits() const override;
	uint32_t GetVariable(byte variable, [[maybe_unused]] uint32_t parameter, bool *available) const override;
	uint32_t GetTriggers() const override;
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

	VehicleResolverObject(EngineID engine_type, const Vehicle *v, WagonOverride wagon_override, bool rotor_in_gui = false,
			CallbackID callback = CBID_NO_CALLBACK, uint32_t callback_param1 = 0, uint32_t callback_param2 = 0);

	ScopeResolver *GetScope(VarSpriteGroupScope scope = VSG_SCOPE_SELF, byte relative = 0) override;

	const SpriteGroup *ResolveReal(const RealSpriteGroup *group) const override;

	GrfSpecFeature GetFeature() const override;
	uint32_t GetDebugID() const override;
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
#define GetCustomVehicleIcon(et, direction, image_type, result) GetCustomEngineSprite(et, nullptr, direction, image_type, result)

void GetRotorOverrideSprite(EngineID engine, const struct Aircraft *v, EngineImageType image_type, VehicleSpriteSeq *result);
#define GetCustomRotorSprite(v, image_type, result) GetRotorOverrideSprite(v->engine_type, v, image_type, result)
#define GetCustomRotorIcon(et, image_type, result) GetRotorOverrideSprite(et, nullptr, image_type, result)

/* Forward declaration of GRFFile, to avoid unnecessary inclusion of newgrf.h
 * elsewhere... */
struct GRFFile;

void SetEngineGRF(EngineID engine, const struct GRFFile *file);

uint16_t GetVehicleCallback(CallbackID callback, uint32_t param1, uint32_t param2, EngineID engine, const Vehicle *v);
uint16_t GetVehicleCallbackParent(CallbackID callback, uint32_t param1, uint32_t param2, EngineID engine, const Vehicle *v, const Vehicle *parent);
bool UsesWagonOverride(const Vehicle *v);

/* Handler to Evaluate callback 36. If the callback fails (i.e. most of the
 * time) orig_value is returned */
int GetVehicleProperty(const Vehicle *v, PropertyID property, int orig_value, bool is_signed = false);
int GetEngineProperty(EngineID engine, PropertyID property, int orig_value, const Vehicle *v = nullptr, bool is_signed = false);

enum class BuildProbabilityType {
	Reversed = 0,
};

bool TestVehicleBuildProbability(Vehicle *v, EngineID engine, BuildProbabilityType type);

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

void AlterVehicleListOrder(EngineID engine, uint target);
void CommitVehicleListOrderChanges();

EngineID GetNewEngineID(const GRFFile *file, VehicleType type, uint16_t internal_id);

void FillNewGRFVehicleCache(const Vehicle *v);

#endif /* NEWGRF_ENGINE_H */
