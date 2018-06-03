/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file engine_base.h Base class for engines. */

#ifndef ENGINE_BASE_H
#define ENGINE_BASE_H

#include "engine_type.h"
#include "vehicle_type.h"
#include "core/pool_type.hpp"
#include "newgrf_commons.h"

typedef Pool<Engine, EngineID, 64, 64000> EnginePool;
extern EnginePool _engine_pool;

struct Engine : EnginePool::PoolItem<&_engine_pool> {
	char *name;                 ///< Custom name of engine.
	Date intro_date;            ///< Date of introduction of the engine.
	Date age;
	uint16 reliability;         ///< Current reliability of the engine.
	uint16 reliability_spd_dec; ///< Speed of reliability decay between services (per day).
	uint16 reliability_start;   ///< Initial reliability of the engine.
	uint16 reliability_max;     ///< Maximal reliability of the engine.
	uint16 reliability_final;   ///< Final reliability of the engine.
	uint16 duration_phase_1;    ///< First reliability phase in months, increasing reliability from #reliability_start to #reliability_max.
	uint16 duration_phase_2;    ///< Second reliability phase in months, keeping #reliability_max.
	uint16 duration_phase_3;    ///< Third reliability phase on months, decaying to #reliability_final.
	byte flags;                 ///< Flags of the engine. @see EngineFlags
	CompanyMask preview_asked;  ///< Bit for each company which has already been offered a preview.
	CompanyByte preview_company;///< Company which is currently being offered a preview \c INVALID_COMPANY means no company.
	byte preview_wait;          ///< Daily countdown timer for timeout of offering the engine to the #preview_company company.
	CompanyMask company_avail;  ///< Bit for each company whether the engine is available for that company.
	CompanyMask company_hidden; ///< Bit for each company whether the engine is normally hidden in the build gui for that company.
	uint8 original_image_index; ///< Original vehicle image index, thus the image index of the overridden vehicle
	VehicleType type;           ///< %Vehicle type, ie #VEH_ROAD, #VEH_TRAIN, etc.

	EngineInfo info;

	union {
		RailVehicleInfo rail;
		RoadVehicleInfo road;
		ShipVehicleInfo ship;
		AircraftVehicleInfo air;
	} u;

	/* NewGRF related data */
	/**
	 * Properties related the the grf file.
	 * NUM_CARGO real cargo plus two pseudo cargo sprite groups.
	 * Used for obtaining the sprite offset of custom sprites, and for
	 * evaluating callbacks.
	 */
	GRFFilePropsBase<NUM_CARGO + 2> grf_prop;
	uint16 overrides_count;
	struct WagonOverride *overrides;
	uint16 list_position;

	Engine();
	Engine(VehicleType type, EngineID base);
	~Engine();
	bool IsEnabled() const;

	RoadTypeIdentifier GetRoadType() const;

	/**
	 * Determines the default cargo type of an engine.
	 *
	 * Usually a valid cargo is returned, even though the vehicle has zero capacity, and can therefore not carry anything. But the cargotype is still used
	 * for livery selection etc..
	 *
	 * Vehicles with CT_INVALID as default cargo are usually not available, but it can appear as default cargo of articulated parts.
	 *
	 * @return The default cargo type.
	 * @see CanCarryCargo
	 */
	CargoID GetDefaultCargoType() const
	{
		return this->info.cargo_type;
	}

	uint DetermineCapacity(const Vehicle *v, uint16 *mail_capacity = NULL) const;

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
	uint GetDisplayDefaultCapacity(uint16 *mail_capacity = NULL) const
	{
		return this->DetermineCapacity(NULL, mail_capacity);
	}

	Money GetRunningCost() const;
	Money GetCost() const;
	uint GetDisplayMaxSpeed() const;
	uint GetPower() const;
	uint GetDisplayWeight() const;
	uint GetDisplayMaxTractiveEffort() const;
	Date GetLifeLengthInDays() const;
	uint16 GetRange() const;
	StringID GetAircraftTypeText() const;

	/**
	 * Check whether the engine is hidden in the GUI for the given company.
	 * @param c Company to check.
	 * @return \c true iff the engine is hidden in the GUI for the given company.
	 */
	inline bool IsHidden(CompanyByte c) const
	{
		return c < MAX_COMPANIES && HasBit(this->company_hidden, c);
	}

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

	uint32 GetGRFID() const;
};

struct EngineIDMapping {
	uint32 grfid;          ///< The GRF ID of the file the entity belongs to
	uint16 internal_id;    ///< The internal ID within the GRF file
	VehicleTypeByte type;  ///< The engine type
	uint8  substitute_id;  ///< The (original) entity ID to use if this GRF is not available (currently not used)
};

/**
 * Stores the mapping of EngineID to the internal id of newgrfs.
 * Note: This is not part of Engine, as the data in the EngineOverrideManager and the engine pool get resetted in different cases.
 */
struct EngineOverrideManager : SmallVector<EngineIDMapping, 256> {
	static const uint NUM_DEFAULT_ENGINES; ///< Number of default entries

	void ResetToDefaultMapping();
	EngineID GetID(VehicleType type, uint16 grf_local_id, uint32 grfid);

	static bool ResetToCurrentNewGRFConfig();
};

extern EngineOverrideManager _engine_mngr;

#define FOR_ALL_ENGINES_FROM(var, start) FOR_ALL_ITEMS_FROM(Engine, engine_index, var, start)
#define FOR_ALL_ENGINES(var) FOR_ALL_ENGINES_FROM(var, 0)

#define FOR_ALL_ENGINES_OF_TYPE(e, engine_type) FOR_ALL_ENGINES(e) if (e->type == engine_type)

static inline const EngineInfo *EngInfo(EngineID e)
{
	return &Engine::Get(e)->info;
}

static inline const RailVehicleInfo *RailVehInfo(EngineID e)
{
	return &Engine::Get(e)->u.rail;
}

static inline const RoadVehicleInfo *RoadVehInfo(EngineID e)
{
	return &Engine::Get(e)->u.road;
}

static inline const ShipVehicleInfo *ShipVehInfo(EngineID e)
{
	return &Engine::Get(e)->u.ship;
}

static inline const AircraftVehicleInfo *AircraftVehInfo(EngineID e)
{
	return &Engine::Get(e)->u.air;
}

#endif /* ENGINE_BASE_H */
