/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file engine_base.h Base class for engines. */

#ifndef ENGINE_BASE_H
#define ENGINE_BASE_H

#include "engine_type.h"
#include "vehicle_type.h"
#include "core/enum_type.hpp"
#include "core/pool_type.hpp"
#include "newgrf_commons.h"
#include "timer/timer_game_calendar.h"

struct WagonOverride {
	std::vector<EngineID> engines;
	CargoType cargo;
	const SpriteGroup *group;
};

/** Flags used client-side in the purchase/autorenew engine list. */
enum class EngineDisplayFlag : uint8_t {
	HasVariants, ///< Set if engine has variants.
	IsFolded, ///< Set if display of variants should be folded (hidden).
	Shaded, ///< Set if engine should be masked.
};

using EngineDisplayFlags = EnumBitSet<EngineDisplayFlag, uint8_t>;

typedef Pool<Engine, EngineID, 64> EnginePool;
extern EnginePool _engine_pool;

class Engine : public EnginePool::PoolItem<&_engine_pool> {
public:
	CompanyMask company_avail{}; ///< Bit for each company whether the engine is available for that company.
	CompanyMask company_hidden{}; ///< Bit for each company whether the engine is normally hidden in the build gui for that company.
	CompanyMask preview_asked{}; ///< Bit for each company which has already been offered a preview.

	std::string name{}; ///< Custom name of engine.

	TimerGameCalendar::Date intro_date{}; ///< Date of introduction of the engine.
	int32_t age = 0; ///< Age of the engine in months.

	uint16_t reliability = 0; ///< Current reliability of the engine.
	uint16_t reliability_spd_dec = 0; ///< Speed of reliability decay between services (per day).
	uint16_t reliability_start = 0; ///< Initial reliability of the engine.
	uint16_t reliability_max = 0; ///< Maximal reliability of the engine.
	uint16_t reliability_final = 0; ///< Final reliability of the engine.
	uint16_t duration_phase_1 = 0; ///< First reliability phase in months, increasing reliability from #reliability_start to #reliability_max.
	uint16_t duration_phase_2 = 0; ///< Second reliability phase in months, keeping #reliability_max.
	uint16_t duration_phase_3 = 0; ///< Third reliability phase in months, decaying to #reliability_final.
	EngineFlags flags{}; ///< Flags of the engine. @see EngineFlags

	CompanyID preview_company = CompanyID::Invalid();  ///< Company which is currently being offered a preview \c CompanyID::Invalid() means no company.
	uint8_t preview_wait = 0; ///< Daily countdown timer for timeout of offering the engine to the #preview_company company.
	uint8_t original_image_index = 0; ///< Original vehicle image index, thus the image index of the overridden vehicle
	VehicleType type = VEH_INVALID; ///< %Vehicle type, ie #VEH_ROAD, #VEH_TRAIN, etc.

	EngineDisplayFlags display_flags{}; ///< NOSAVE client-side-only display flags for build engine list.
	EngineID display_last_variant = EngineID::Invalid(); ///< NOSAVE client-side-only last variant selected.
	EngineInfo info{};

	uint16_t list_position = 0;

	/* NewGRF related data */
	CargoGRFFileProps grf_prop{}; ///< Link to NewGRF
	std::vector<WagonOverride> overrides{};
	std::vector<BadgeID> badges{};

private:
	/* Vehicle-type specific information. */
	std::variant<std::monostate, RailVehicleInfo, RoadVehicleInfo, ShipVehicleInfo, AircraftVehicleInfo> vehicle_info{};

public:
	Engine() {}
	Engine(VehicleType type, uint16_t local_id);
	bool IsEnabled() const;

	/**
	 * Determines the default cargo type of an engine.
	 *
	 * Usually a valid cargo is returned, even though the vehicle has zero capacity, and can therefore not carry anything. But the cargotype is still used
	 * for livery selection etc..
	 *
	 * Vehicles with INVALID_CARGO as default cargo are usually not available, but it can appear as default cargo of articulated parts.
	 *
	 * @return The default cargo type.
	 * @see CanCarryCargo
	 */
	CargoType GetDefaultCargoType() const
	{
		return this->info.cargo_type;
	}

	uint DetermineCapacity(const Vehicle *v, uint16_t *mail_capacity = nullptr) const;

	bool CanCarryCargo() const;

	/**
	 * Determines the default cargo capacity of an engine for display purposes.
	 *
	 * For planes carrying both passenger and mail this is the passenger capacity.
	 * For multiheaded engines this is the capacity of both heads.
	 * For articulated engines use GetCapacityOfArticulatedParts
	 *
	 * @param mail_capacity returns secondary cargo (mail) capacity of aircraft
	 * @return The default capacity
	 * @see GetDefaultCargoType
	 */
	uint GetDisplayDefaultCapacity(uint16_t *mail_capacity = nullptr) const
	{
		return this->DetermineCapacity(nullptr, mail_capacity);
	}

	Money GetRunningCost() const;
	Money GetCost() const;
	uint GetDisplayMaxSpeed() const;
	uint GetPower() const;
	uint GetDisplayWeight() const;
	uint GetDisplayMaxTractiveEffort() const;
	TimerGameCalendar::Date GetLifeLengthInDays() const;
	uint16_t GetRange() const;
	StringID GetAircraftTypeText() const;

	/**
	 * Check whether the engine is hidden in the GUI for the given company.
	 * @param c Company to check.
	 * @return \c true iff the engine is hidden in the GUI for the given company.
	 */
	inline bool IsHidden(CompanyID c) const
	{
		return c < MAX_COMPANIES && this->company_hidden.Test(c);
	}

	/**
	 * Get the last display variant for an engine.
	 * @return Engine's last display variant or engine itself if no last display variant is set.
	 */
	const Engine *GetDisplayVariant() const
	{
		if (this->display_last_variant == this->index || this->display_last_variant == EngineID::Invalid()) return this;
		return Engine::Get(this->display_last_variant);
	}

	bool IsVariantHidden(CompanyID c) const;

	/**
	 * Check if the engine is a ground vehicle.
	 * @return True iff the engine is a train or a road vehicle.
	 */
	inline bool IsGroundVehicle() const
	{
		return this->type == VEH_TRAIN || this->type == VEH_ROAD;
	}

	/**
	 * Retrieve the NewGRF the engine is tied to.
	 * This is the GRF providing the Action 3.
	 * @return NewGRF associated to the engine.
	 */
	const GRFFile *GetGRF() const
	{
		return this->grf_prop.grffile;
	}

	uint32_t GetGRFID() const;

	struct EngineTypeFilter {
		VehicleType vt;

		bool operator() (size_t index) { return Engine::Get(index)->type == this->vt; }
	};

	template <typename T>
	inline T &VehInfo()
	{
		return std::get<T>(this->vehicle_info);
	}

	template <typename T>
	inline const T &VehInfo() const
	{
		return std::get<T>(this->vehicle_info);
	}

	/**
	 * Returns an iterable ensemble of all valid engines of the given type
	 * @param vt the VehicleType for engines to be valid
	 * @param from index of the first engine to consider
	 * @return an iterable ensemble of all valid engines of the given type
	 */
	static Pool::IterateWrapperFiltered<Engine, EngineTypeFilter> IterateType(VehicleType vt, size_t from = 0)
	{
		return Pool::IterateWrapperFiltered<Engine, EngineTypeFilter>(from, EngineTypeFilter{ vt });
	}
};

struct EngineIDMapping {
	uint32_t grfid = 0; ///< The GRF ID of the file the entity belongs to
	uint16_t internal_id = 0; ///< The internal ID within the GRF file
	VehicleType type{}; ///< The engine type
	uint8_t substitute_id = 0; ///< The (original) entity ID to use if this GRF is not available (currently not used)
	EngineID engine{};

	static inline uint64_t Key(uint32_t grfid, uint16_t internal_id) { return static_cast<uint64_t>(grfid) << 32 | internal_id; }

	inline uint64_t Key() const { return Key(this->grfid, this->internal_id); }

	EngineIDMapping() {}
	EngineIDMapping(uint32_t grfid, uint16_t internal_id, VehicleType type, uint8_t substitute_id, EngineID engine)
		: grfid(grfid), internal_id(internal_id), type(type), substitute_id(substitute_id), engine(engine) {}
};

/** Projection to get a unique key of an EngineIDMapping, used for sorting in EngineOverrideManager. */
struct EngineIDMappingKeyProjection {
	inline uint64_t operator()(const EngineIDMapping &eid) const { return eid.Key(); }
};

/**
 * Stores the mapping of EngineID to the internal id of newgrfs.
 * Note: This is not part of Engine, as the data in the EngineOverrideManager and the engine pool get reset in different cases.
 */
struct EngineOverrideManager {
	std::array<std::vector<EngineIDMapping>, VEH_COMPANY_END> mappings;

	void ResetToDefaultMapping();
	EngineID GetID(VehicleType type, uint16_t grf_local_id, uint32_t grfid);
	EngineID UseUnreservedID(VehicleType type, uint16_t grf_local_id, uint32_t grfid, bool static_access);
	void SetID(VehicleType type, uint16_t grf_local_id, uint32_t grfid, uint8_t substitute_id, EngineID engine);

	static bool ResetToCurrentNewGRFConfig();
};

extern EngineOverrideManager _engine_mngr;

inline const EngineInfo *EngInfo(EngineID e)
{
	return &Engine::Get(e)->info;
}

inline const RailVehicleInfo *RailVehInfo(EngineID e)
{
	return &Engine::Get(e)->VehInfo<RailVehicleInfo>();
}

inline const RoadVehicleInfo *RoadVehInfo(EngineID e)
{
	return &Engine::Get(e)->VehInfo<RoadVehicleInfo>();
}

inline const ShipVehicleInfo *ShipVehInfo(EngineID e)
{
	return &Engine::Get(e)->VehInfo<ShipVehicleInfo>();
}

inline const AircraftVehicleInfo *AircraftVehInfo(EngineID e)
{
	return &Engine::Get(e)->VehInfo<AircraftVehicleInfo>();
}

#endif /* ENGINE_BASE_H */
