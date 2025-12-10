/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file engine.cpp Base for all engine handling. */

#include "stdafx.h"
#include "core/container_func.hpp"
#include "company_func.h"
#include "command_func.h"
#include "news_func.h"
#include "aircraft.h"
#include "newgrf.h"
#include "newgrf_engine.h"
#include "strings_func.h"
#include "core/random_func.hpp"
#include "window_func.h"
#include "autoreplace_gui.h"
#include "string_func.h"
#include "ai/ai.hpp"
#include "core/pool_func.hpp"
#include "engine_gui.h"
#include "engine_func.h"
#include "engine_base.h"
#include "company_base.h"
#include "vehicle_func.h"
#include "articulated_vehicles.h"
#include "error.h"
#include "engine_base.h"
#include "timer/timer.h"
#include "timer/timer_game_tick.h"
#include "timer/timer_game_calendar.h"

#include "table/strings.h"
#include "table/engines.h"

#include "safeguards.h"

EnginePool _engine_pool("Engine");
INSTANTIATE_POOL_METHODS(Engine)

EngineOverrideManager _engine_mngr;

/**
 * Year that engine aging stops. Engines will not reduce in reliability
 * and no more engines will be introduced
 */
static TimerGameCalendar::Year _year_engine_aging_stops;

/** Number of engines of each vehicle type in original engine data */
const uint8_t _engine_counts[4] = {
	lengthof(_orig_rail_vehicle_info),
	lengthof(_orig_road_vehicle_info),
	lengthof(_orig_ship_vehicle_info),
	lengthof(_orig_aircraft_vehicle_info),
};

/** Offset of the first engine of each vehicle type in original engine data */
const uint8_t _engine_offsets[4] = {
	0,
	lengthof(_orig_rail_vehicle_info),
	lengthof(_orig_rail_vehicle_info) + lengthof(_orig_road_vehicle_info),
	lengthof(_orig_rail_vehicle_info) + lengthof(_orig_road_vehicle_info) + lengthof(_orig_ship_vehicle_info),
};

static_assert(lengthof(_orig_rail_vehicle_info) + lengthof(_orig_road_vehicle_info) + lengthof(_orig_ship_vehicle_info) + lengthof(_orig_aircraft_vehicle_info) == lengthof(_orig_engine_info));

Engine::Engine(VehicleType type, uint16_t local_id)
{
	this->type = type;
	this->grf_prop.local_id = local_id;
	this->list_position = local_id;
	this->preview_company = CompanyID::Invalid();
	this->display_last_variant = EngineID::Invalid();

	/* Check if this base engine is within the original engine data range */
	if (local_id >= _engine_counts[type]) {
		/* Initialise default type-specific information. */
		switch (type) {
			case VEH_TRAIN: this->vehicle_info.emplace<RailVehicleInfo>(); break;
			case VEH_ROAD: this->vehicle_info.emplace<RoadVehicleInfo>(); break;
			case VEH_SHIP: this->vehicle_info.emplace<ShipVehicleInfo>(); break;
			case VEH_AIRCRAFT: this->vehicle_info.emplace<AircraftVehicleInfo>(); break;
			default: break;
		}
		/* Set model life to maximum to make wagons available */
		this->info.base_life = TimerGameCalendar::Year{0xFF};
		/* Aircraft must have CT_INVALID as default, as there is no property */
		this->info.cargo_type = INVALID_CARGO;
		this->info.cargo_label = (type == VEH_AIRCRAFT) ? CT_INVALID : CT_PASSENGERS;
		/* Set cargo aging period to the default value. */
		this->info.cargo_age_period = Ticks::CARGO_AGING_TICKS;
		/* Not a variant */
		this->info.variant_id = EngineID::Invalid();
		return;
	}

	/* Copy the original engine info for this slot */
	this->info = _orig_engine_info[_engine_offsets[type] + local_id];

	/* Copy the original engine data for this slot */
	switch (type) {
		default: NOT_REACHED();

		case VEH_TRAIN: {
			RailVehicleInfo &rvi = this->vehicle_info.emplace<RailVehicleInfo>(_orig_rail_vehicle_info[local_id]);
			this->original_image_index = rvi.image_index;
			this->info.string_id = STR_VEHICLE_NAME_TRAIN_ENGINE_RAIL_KIRBY_PAUL_TANK_STEAM + local_id;

			/* Set the default model life of original wagons to "infinite" */
			if (rvi.railveh_type == RAILVEH_WAGON) this->info.base_life = TimerGameCalendar::Year{0xFF};

			break;
		}

		case VEH_ROAD: {
			RoadVehicleInfo &rvi = this->vehicle_info.emplace<RoadVehicleInfo>(_orig_road_vehicle_info[local_id]);
			this->original_image_index = rvi.image_index;
			this->info.string_id = STR_VEHICLE_NAME_ROAD_VEHICLE_MPS_REGAL_BUS + local_id;
			break;
		}

		case VEH_SHIP: {
			ShipVehicleInfo &svi = this->vehicle_info.emplace<ShipVehicleInfo>(_orig_ship_vehicle_info[local_id]);
			this->original_image_index = svi.image_index;
			this->info.string_id = STR_VEHICLE_NAME_SHIP_MPS_OIL_TANKER + local_id;
			break;
		}

		case VEH_AIRCRAFT: {
			AircraftVehicleInfo &avi = this->vehicle_info.emplace<AircraftVehicleInfo>(_orig_aircraft_vehicle_info[local_id]);
			this->original_image_index = avi.image_index;
			this->info.string_id = STR_VEHICLE_NAME_AIRCRAFT_SAMPSON_U52 + local_id;
			break;
		}
	}
}

/**
 * Checks whether the engine is a valid (non-articulated part of an) engine.
 * @return true if enabled
 */
bool Engine::IsEnabled() const
{
	return this->info.string_id != STR_NEWGRF_INVALID_ENGINE && this->info.climates.Test(_settings_game.game_creation.landscape);
}

/**
 * Retrieve the GRF ID of the NewGRF the engine is tied to.
 * This is the GRF providing the Action 3.
 * @return GRF ID of the associated NewGRF.
 */
uint32_t Engine::GetGRFID() const
{
	const GRFFile *file = this->GetGRF();
	return file == nullptr ? 0 : file->grfid;
}

/**
 * Determines whether an engine can carry something.
 * A vehicle cannot carry anything if its capacity is zero, or none of the possible cargoes is available in the climate.
 * @return true if the vehicle can carry something.
 */
bool Engine::CanCarryCargo() const
{
	/* For engines that can appear in a consist (i.e. rail vehicles and (articulated) road vehicles), a capacity
	 * of zero is a special case, to define the vehicle to not carry anything. The default cargotype is still used
	 * for livery selection etc.
	 * Note: Only the property is tested. A capacity callback returning 0 does not have the same effect.
	 */
	switch (this->type) {
		case VEH_TRAIN:
			if (this->VehInfo<RailVehicleInfo>().capacity == 0) return false;
			break;

		case VEH_ROAD:
			if (this->VehInfo<RoadVehicleInfo>().capacity == 0) return false;
			break;

		case VEH_SHIP:
		case VEH_AIRCRAFT:
			break;

		default: NOT_REACHED();
	}
	return IsValidCargoType(this->GetDefaultCargoType());
}


/**
 * Determines capacity of a given vehicle from scratch.
 * For aircraft the main capacity is determined. Mail might be present as well.
 * @param v Vehicle of interest; nullptr in purchase list
 * @param mail_capacity returns secondary cargo (mail) capacity of aircraft
 * @return Capacity
 */
uint Engine::DetermineCapacity(const Vehicle *v, uint16_t *mail_capacity) const
{
	assert(v == nullptr || this->index == v->engine_type);
	if (mail_capacity != nullptr) *mail_capacity = 0;

	if (!this->CanCarryCargo()) return 0;

	bool new_multipliers = this->info.misc_flags.Test(EngineMiscFlag::NoDefaultCargoMultiplier);
	CargoType default_cargo = this->GetDefaultCargoType();
	CargoType cargo_type = (v != nullptr) ? v->cargo_type : default_cargo;

	if (mail_capacity != nullptr && this->type == VEH_AIRCRAFT && IsCargoInClass(cargo_type, CargoClass::Passengers)) {
		*mail_capacity = GetEngineProperty(this->index, PROP_AIRCRAFT_MAIL_CAPACITY, this->VehInfo<AircraftVehicleInfo>().mail_capacity, v);
	}

	/* Check the refit capacity callback if we are not in the default configuration, or if we are using the new multiplier algorithm. */
	if (this->info.callback_mask.Test(VehicleCallbackMask::RefitCapacity) &&
			(new_multipliers || default_cargo != cargo_type || (v != nullptr && v->cargo_subtype != 0))) {
		uint16_t callback = GetVehicleCallback(CBID_VEHICLE_REFIT_CAPACITY, 0, 0, this->index, v);
		if (callback != CALLBACK_FAILED) return callback;
	}

	/* Get capacity according to property resp. CB */
	uint capacity;
	uint extra_mail_cap = 0;
	switch (this->type) {
		case VEH_TRAIN:
			capacity = GetEngineProperty(this->index, PROP_TRAIN_CARGO_CAPACITY,        this->VehInfo<RailVehicleInfo>().capacity, v);

			/* In purchase list add the capacity of the second head. Always use the plain property for this. */
			if (v == nullptr && this->VehInfo<RailVehicleInfo>().railveh_type == RAILVEH_MULTIHEAD) capacity += this->VehInfo<RailVehicleInfo>().capacity;
			break;

		case VEH_ROAD:
			capacity = GetEngineProperty(this->index, PROP_ROADVEH_CARGO_CAPACITY,      this->VehInfo<RoadVehicleInfo>().capacity, v);
			break;

		case VEH_SHIP:
			capacity = GetEngineProperty(this->index, PROP_SHIP_CARGO_CAPACITY,         this->VehInfo<ShipVehicleInfo>().capacity, v);
			break;

		case VEH_AIRCRAFT:
			capacity = GetEngineProperty(this->index, PROP_AIRCRAFT_PASSENGER_CAPACITY, this->VehInfo<AircraftVehicleInfo>().passenger_capacity, v);
			if (!IsCargoInClass(cargo_type, CargoClass::Passengers)) {
				extra_mail_cap = GetEngineProperty(this->index, PROP_AIRCRAFT_MAIL_CAPACITY, this->VehInfo<AircraftVehicleInfo>().mail_capacity, v);
			}
			if (IsValidCargoType(GetCargoTypeByLabel(CT_MAIL))) {
				if (!new_multipliers && cargo_type == GetCargoTypeByLabel(CT_MAIL)) return capacity + extra_mail_cap;
			}
			default_cargo = GetCargoTypeByLabel(CT_PASSENGERS); // Always use 'passengers' wrt. cargo multipliers
			break;

		default: NOT_REACHED();
	}

	if (!new_multipliers) {
		/* Use the passenger multiplier for mail as well */
		capacity += extra_mail_cap;
		extra_mail_cap = 0;
	}

	/* Apply multipliers depending on cargo- and vehicletype. */
	if (new_multipliers || (this->type != VEH_SHIP && default_cargo != cargo_type)) {
		uint16_t default_multiplier = new_multipliers ? 0x100 : CargoSpec::Get(default_cargo)->multiplier;
		uint16_t cargo_multiplier = CargoSpec::Get(cargo_type)->multiplier;
		capacity *= cargo_multiplier;
		if (extra_mail_cap > 0 && IsValidCargoType(GetCargoTypeByLabel(CT_MAIL))) {
			uint mail_multiplier = CargoSpec::Get(GetCargoTypeByLabel(CT_MAIL))->multiplier;
			capacity += (default_multiplier * extra_mail_cap * cargo_multiplier + mail_multiplier / 2) / mail_multiplier;
		}
		capacity = (capacity + default_multiplier / 2) / default_multiplier;
	}

	return capacity;
}

/**
 * Return how much the running costs of this engine are.
 * @return Yearly running cost of the engine.
 */
Money Engine::GetRunningCost() const
{
	Price base_price;
	uint cost_factor;
	switch (this->type) {
		case VEH_ROAD:
			base_price = this->VehInfo<RoadVehicleInfo>().running_cost_class;
			if (base_price == INVALID_PRICE) return 0;
			cost_factor = GetEngineProperty(this->index, PROP_ROADVEH_RUNNING_COST_FACTOR, this->VehInfo<RoadVehicleInfo>().running_cost);
			break;

		case VEH_TRAIN:
			base_price = this->VehInfo<RailVehicleInfo>().running_cost_class;
			if (base_price == INVALID_PRICE) return 0;
			cost_factor = GetEngineProperty(this->index, PROP_TRAIN_RUNNING_COST_FACTOR, this->VehInfo<RailVehicleInfo>().running_cost);
			break;

		case VEH_SHIP:
			base_price = PR_RUNNING_SHIP;
			cost_factor = GetEngineProperty(this->index, PROP_SHIP_RUNNING_COST_FACTOR, this->VehInfo<ShipVehicleInfo>().running_cost);
			break;

		case VEH_AIRCRAFT:
			base_price = PR_RUNNING_AIRCRAFT;
			cost_factor = GetEngineProperty(this->index, PROP_AIRCRAFT_RUNNING_COST_FACTOR, this->VehInfo<AircraftVehicleInfo>().running_cost);
			break;

		default: NOT_REACHED();
	}

	return GetPrice(base_price, cost_factor, this->GetGRF(), -8);
}

/**
 * Return how much a new engine costs.
 * @return Cost of the engine.
 */
Money Engine::GetCost() const
{
	Price base_price;
	uint cost_factor;
	switch (this->type) {
		case VEH_ROAD:
			base_price = PR_BUILD_VEHICLE_ROAD;
			cost_factor = GetEngineProperty(this->index, PROP_ROADVEH_COST_FACTOR, this->VehInfo<RoadVehicleInfo>().cost_factor);
			break;

		case VEH_TRAIN:
			if (this->VehInfo<RailVehicleInfo>().railveh_type == RAILVEH_WAGON) {
				base_price = PR_BUILD_VEHICLE_WAGON;
				cost_factor = GetEngineProperty(this->index, PROP_TRAIN_COST_FACTOR, this->VehInfo<RailVehicleInfo>().cost_factor);
			} else {
				base_price = PR_BUILD_VEHICLE_TRAIN;
				cost_factor = GetEngineProperty(this->index, PROP_TRAIN_COST_FACTOR, this->VehInfo<RailVehicleInfo>().cost_factor);
			}
			break;

		case VEH_SHIP:
			base_price = PR_BUILD_VEHICLE_SHIP;
			cost_factor = GetEngineProperty(this->index, PROP_SHIP_COST_FACTOR, this->VehInfo<ShipVehicleInfo>().cost_factor);
			break;

		case VEH_AIRCRAFT:
			base_price = PR_BUILD_VEHICLE_AIRCRAFT;
			cost_factor = GetEngineProperty(this->index, PROP_AIRCRAFT_COST_FACTOR, this->VehInfo<AircraftVehicleInfo>().cost_factor);
			break;

		default: NOT_REACHED();
	}

	return GetPrice(base_price, cost_factor, this->GetGRF(), -8);
}

/**
 * Returns max speed of the engine for display purposes
 * @return max speed in km-ish/h
 */
uint Engine::GetDisplayMaxSpeed() const
{
	switch (this->type) {
		case VEH_TRAIN:
			return GetEngineProperty(this->index, PROP_TRAIN_SPEED, this->VehInfo<RailVehicleInfo>().max_speed);

		case VEH_ROAD: {
			uint max_speed = GetEngineProperty(this->index, PROP_ROADVEH_SPEED, 0);
			return (max_speed != 0) ? max_speed * 2 : this->VehInfo<RoadVehicleInfo>().max_speed / 2;
		}

		case VEH_SHIP:
			return GetEngineProperty(this->index, PROP_SHIP_SPEED, this->VehInfo<ShipVehicleInfo>().max_speed) / 2;

		case VEH_AIRCRAFT: {
			uint max_speed = GetEngineProperty(this->index, PROP_AIRCRAFT_SPEED, 0);
			if (max_speed != 0) {
				return (max_speed * 128) / 10;
			}
			return this->VehInfo<AircraftVehicleInfo>().max_speed;
		}

		default: NOT_REACHED();
	}
}

/**
 * Returns the power of the engine for display
 * and sorting purposes.
 * Only trains and road vehicles have power
 * @return power in display units hp
 */
uint Engine::GetPower() const
{
	/* Only trains and road vehicles have 'power'. */
	switch (this->type) {
		case VEH_TRAIN:
			return GetEngineProperty(this->index, PROP_TRAIN_POWER, this->VehInfo<RailVehicleInfo>().power);
		case VEH_ROAD:
			return GetEngineProperty(this->index, PROP_ROADVEH_POWER, this->VehInfo<RoadVehicleInfo>().power) * 10;

		default: NOT_REACHED();
	}
}

/**
 * Returns the weight of the engine for display purposes.
 * For dual-headed train-engines this is the weight of both heads
 * @return weight in display units metric tons
 */
uint Engine::GetDisplayWeight() const
{
	/* Only trains and road vehicles have 'weight'. */
	switch (this->type) {
		case VEH_TRAIN:
			return GetEngineProperty(this->index, PROP_TRAIN_WEIGHT, this->VehInfo<RailVehicleInfo>().weight) << (this->VehInfo<RailVehicleInfo>().railveh_type == RAILVEH_MULTIHEAD ? 1 : 0);
		case VEH_ROAD:
			return GetEngineProperty(this->index, PROP_ROADVEH_WEIGHT, this->VehInfo<RoadVehicleInfo>().weight) / 4;

		default: NOT_REACHED();
	}
}

/**
 * Returns the tractive effort of the engine for display purposes.
 * For dual-headed train-engines this is the tractive effort of both heads
 * @return tractive effort in display units kN
 */
uint Engine::GetDisplayMaxTractiveEffort() const
{
	/* Only trains and road vehicles have 'tractive effort'. */
	switch (this->type) {
		case VEH_TRAIN:
			return (GROUND_ACCELERATION * this->GetDisplayWeight() * GetEngineProperty(this->index, PROP_TRAIN_TRACTIVE_EFFORT, this->VehInfo<RailVehicleInfo>().tractive_effort)) / 256;
		case VEH_ROAD:
			return (GROUND_ACCELERATION * this->GetDisplayWeight() * GetEngineProperty(this->index, PROP_ROADVEH_TRACTIVE_EFFORT, this->VehInfo<RoadVehicleInfo>().tractive_effort)) / 256;

		default: NOT_REACHED();
	}
}

/**
 * Returns the vehicle's (not model's!) life length in days.
 * @return the life length
 */
TimerGameCalendar::Date Engine::GetLifeLengthInDays() const
{
	/* Assume leap years; this gives the player a bit more than the given amount of years, but never less. */
	return TimerGameCalendar::Date{(this->info.lifelength + _settings_game.vehicle.extend_vehicle_life).base() * CalendarTime::DAYS_IN_LEAP_YEAR};
}

/**
 * Get the range of an aircraft type.
 * @return Range of the aircraft type in tiles or 0 if unlimited range.
 */
uint16_t Engine::GetRange() const
{
	switch (this->type) {
		case VEH_AIRCRAFT:
			return GetEngineProperty(this->index, PROP_AIRCRAFT_RANGE, this->VehInfo<AircraftVehicleInfo>().max_range);

		default: NOT_REACHED();
	}
}

/**
 * Get the name of the aircraft type for display purposes.
 * @return Aircraft type string.
 */
StringID Engine::GetAircraftTypeText() const
{
	switch (this->type) {
		case VEH_AIRCRAFT:
			switch (this->VehInfo<AircraftVehicleInfo>().subtype) {
				case AIR_HELI: return STR_LIVERY_HELICOPTER;
				case AIR_CTOL: return STR_LIVERY_SMALL_PLANE;
				case AIR_CTOL | AIR_FAST: return STR_LIVERY_LARGE_PLANE;
				default: NOT_REACHED();
			}

		default: NOT_REACHED();
	}
}

/**
 * Check whether the engine variant chain is hidden in the GUI for the given company.
 * @param c Company to check.
 * @return \c true iff the engine variant chain is hidden in the GUI for the given company.
 */
bool Engine::IsVariantHidden(CompanyID c) const
{
	/* In case company is spectator. */
	if (c >= MAX_COMPANIES) return false;

	/* Shortcut if this engine is explicitly hidden. */
	if (this->IsHidden(c)) return true;

	/* Check for hidden parent variants. This is a bit convoluted as we must check hidden status of
	 * the last display variant rather than the actual parent variant. */
	const Engine *re = this;
	const Engine *ve = re->GetDisplayVariant();
	while (!(ve->IsHidden(c)) && re->info.variant_id != EngineID::Invalid()) {
		re = Engine::Get(re->info.variant_id);
		ve = re->GetDisplayVariant();
	}
	return ve->IsHidden(c);
}

/**
 * Initializes the #EngineOverrideManager with the default engines.
 */
void EngineOverrideManager::ResetToDefaultMapping()
{
	EngineID id = EngineID::Begin();
	for (VehicleType type = VEH_TRAIN; type <= VEH_AIRCRAFT; type++) {
		auto &map = this->mappings[type];
		map.clear();
		for (uint internal_id = 0; internal_id < _engine_counts[type]; internal_id++, ++id) {
			map.emplace_back(INVALID_GRFID, internal_id, type, internal_id, id);
		}
	}
}

/**
 * Looks up an EngineID in the EngineOverrideManager
 * @param type Vehicle type
 * @param grf_local_id The local id in the newgrf
 * @param grfid The GrfID that defines the scope of grf_local_id.
 *              If a newgrf overrides the engines of another newgrf, the "scope grfid" is the ID of the overridden newgrf.
 *              If dynamic_engines is disabled, all newgrf share the same ID scope identified by INVALID_GRFID.
 * @return The engine ID if present, or EngineID::Invalid() if not.
 */
EngineID EngineOverrideManager::GetID(VehicleType type, uint16_t grf_local_id, uint32_t grfid)
{
	const auto &map = this->mappings[type];
	const auto key = EngineIDMapping::Key(grfid, grf_local_id);
	auto it = std::ranges::lower_bound(map, key, std::less{}, EngineIDMappingKeyProjection{});
	if (it == std::end(map) || it->Key() != key) return EngineID::Invalid();
	return it->engine;
}

/**
 * Look for an unreserved EngineID matching the local id, and reserve it if found.
 * @param type Vehicle type
 * @param grf_local_id The local id in the newgrf
 * @param grfid The GrfID that defines the scope of grf_local_id.
 *              If a newgrf overrides the engines of another newgrf, the "scope grfid" is the ID of the overridden newgrf.
 *              If dynamic_engines is disabled, all newgrf share the same ID scope identified by INVALID_GRFID.
 * @param static_access Whether to actually reserve the EngineID.
 * @return The engine ID if present and now reserved, or EngineID::Invalid() if not.
 */
EngineID EngineOverrideManager::UseUnreservedID(VehicleType type, uint16_t grf_local_id, uint32_t grfid, bool static_access)
{
	auto &map = _engine_mngr.mappings[type];
	const auto key = EngineIDMapping::Key(INVALID_GRFID, grf_local_id);
	auto it = std::ranges::lower_bound(map, key, std::less{}, EngineIDMappingKeyProjection{});
	if (it == std::end(map) || it->Key() != key) return EngineID::Invalid();

	if (!static_access && grfid != INVALID_GRFID) {
		/* Reserve the engine slot for the new grfid. */
		it->grfid = grfid;

		/* Relocate entry to its new position in the mapping list to keep it sorted. */
		auto p = std::ranges::lower_bound(map, EngineIDMapping::Key(grfid, grf_local_id), std::less{}, EngineIDMappingKeyProjection{});
		it = Slide(it, std::next(it), p).first;
	}

	return it->engine;
}

void EngineOverrideManager::SetID(VehicleType type, uint16_t grf_local_id, uint32_t grfid, uint8_t substitute_id, EngineID engine)
{
	auto &map = this->mappings[type];
	const auto key = EngineIDMapping::Key(grfid, grf_local_id);
	auto it = std::ranges::lower_bound(map, key, std::less{}, EngineIDMappingKeyProjection{});
	if (it == std::end(map) || it->Key() != key) {
		map.emplace(it, grfid, grf_local_id, type, substitute_id, engine);
	} else {
		it->engine = engine;
	}
}

/**
 * Tries to reset the engine mapping to match the current NewGRF configuration.
 * This is only possible when there are currently no vehicles in the game.
 * @return false if resetting failed due to present vehicles.
 */
bool EngineOverrideManager::ResetToCurrentNewGRFConfig()
{
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (IsCompanyBuildableVehicleType(v)) return false;
	}

	/* Reset the engines, they will get new EngineIDs */
	_engine_mngr.ResetToDefaultMapping();
	ReloadNewGRFData();

	return true;
}

/**
 * Initialise the engine pool with the data from the original vehicles.
 */
void SetupEngines()
{
	CloseWindowByClass(WC_ENGINE_PREVIEW);
	_engine_pool.CleanPool();

	for (VehicleType type = VEH_BEGIN; type != VEH_COMPANY_END; type++) {
		const auto &mapping = _engine_mngr.mappings[type];

		/* Verify that the engine override manager has at least been set up with the default engines. */
		assert(std::size(mapping) >= _engine_counts[type]);

		for (const EngineIDMapping &eid : mapping) {
			new (eid.engine) Engine(type, eid.internal_id);
		}
	}
}

void ShowEnginePreviewWindow(EngineID engine);

/**
 * Determine whether an engine type is a wagon (and not a loco).
 * @param index %Engine getting queried.
 * @return Whether the queried engine is a wagon.
 */
static bool IsWagon(EngineID index)
{
	const Engine *e = Engine::Get(index);
	return e->type == VEH_TRAIN && e->VehInfo<RailVehicleInfo>().railveh_type == RAILVEH_WAGON;
}

/**
 * Ensure engine is not set as the last used variant for any other engine.
 * @param engine_id Engine being removed.
 * @param type      Type of engine.
 */
static void ClearLastVariant(EngineID engine_id, VehicleType type)
{
	for (Engine *e : Engine::IterateType(type)) {
		if (e->display_last_variant == engine_id) e->display_last_variant = EngineID::Invalid();
	}
}

/**
 * Update #Engine::reliability and (if needed) update the engine GUIs.
 * @param e %Engine to update.
 */
void CalcEngineReliability(Engine *e, bool new_month)
{
	/* Get source engine for reliability age. This is normally our engine unless variant reliability syncing is requested. */
	Engine *re = e;
	while (re->info.variant_id != EngineID::Invalid() && re->info.extra_flags.Test(ExtraEngineFlag::SyncReliability)) {
		re = Engine::Get(re->info.variant_id);
	}

	uint32_t age = re->age;
	if (new_month && re->index > e->index && age != INT32_MAX) age++; /* parent variant's age has not yet updated. */

	/* Check for early retirement */
	if (e->company_avail.Any() && !_settings_game.vehicle.never_expire_vehicles && e->info.base_life != 0xFF) {
		int retire_early = e->info.retire_early;
		uint retire_early_max_age = std::max(0, e->duration_phase_1 + e->duration_phase_2 - retire_early * 12);
		if (retire_early != 0 && age >= retire_early_max_age) {
			/* Early retirement is enabled and we're past the date... */
			e->company_avail = CompanyMask{};
			ClearLastVariant(e->index, e->type);
			AddRemoveEngineFromAutoreplaceAndBuildWindows(e->type);
		}
	}

	if (age < e->duration_phase_1) {
		uint start = e->reliability_start;
		e->reliability = age * (e->reliability_max - start) / e->duration_phase_1 + start;
	} else if ((age -= e->duration_phase_1) < e->duration_phase_2 || _settings_game.vehicle.never_expire_vehicles || e->info.base_life == 0xFF) {
		/* We are at the peak of this engines life. It will have max reliability.
		 * This is also true if the engines never expire. They will not go bad over time */
		e->reliability = e->reliability_max;
	} else if ((age -= e->duration_phase_2) < e->duration_phase_3) {
		uint max = e->reliability_max;
		e->reliability = (int)age * (int)(e->reliability_final - max) / e->duration_phase_3 + max;
	} else {
		/* time's up for this engine.
		 * We will now completely retire this design */
		e->company_avail = CompanyMask{};
		e->reliability = e->reliability_final;
		/* Kick this engine out of the lists */
		ClearLastVariant(e->index, e->type);
		AddRemoveEngineFromAutoreplaceAndBuildWindows(e->type);
	}

}

/** Compute the value for #_year_engine_aging_stops. */
void SetYearEngineAgingStops()
{
	/* Determine last engine aging year, default to 2050 as previously. */
	_year_engine_aging_stops = TimerGameCalendar::Year{2050};

	for (const Engine *e : Engine::Iterate()) {
		const EngineInfo *ei = &e->info;

		/* Exclude certain engines */
		if (!ei->climates.Test(_settings_game.game_creation.landscape)) continue;
		if (e->type == VEH_TRAIN && e->VehInfo<RailVehicleInfo>().railveh_type == RAILVEH_WAGON) continue;

		/* Base year ending date on half the model life */
		TimerGameCalendar::YearMonthDay ymd = TimerGameCalendar::ConvertDateToYMD(ei->base_intro + (ei->lifelength.base() * CalendarTime::DAYS_IN_LEAP_YEAR) / 2);

		_year_engine_aging_stops = std::max(_year_engine_aging_stops, ymd.year);
	}
}

/**
 * Start/initialise one engine.
 * @param e The engine to initialise.
 * @param aging_date The date used for age calculations.
 * @param seed Random seed.
 */
void StartupOneEngine(Engine *e, const TimerGameCalendar::YearMonthDay &aging_ymd, uint32_t seed)
{
	const EngineInfo *ei = &e->info;

	e->age = 0;
	e->flags = {};
	e->company_avail = CompanyMask{};
	e->company_hidden = CompanyMask{};

	/* Vehicles with the same base_intro date shall be introduced at the same time.
	 * Make sure they use the same randomisation of the date. */
	SavedRandomSeeds saved_seeds;
	SaveRandomSeeds(&saved_seeds);
	SetRandomSeed(_settings_game.game_creation.generation_seed ^ seed ^
	              ei->base_intro.base() ^
	              e->type ^
	              e->GetGRFID());
	uint32_t r = Random();

	/* Don't randomise the start-date in the first two years after gamestart to ensure availability
	 * of engines in early starting games.
	 * Note: TTDP uses fixed 1922 */
	e->intro_date = ei->base_intro <= TimerGameCalendar::ConvertYMDToDate(_settings_game.game_creation.starting_year + 2, 0, 1) ? ei->base_intro : (TimerGameCalendar::Date)GB(r, 0, 9) + ei->base_intro;
	if (e->intro_date <= TimerGameCalendar::date) {
		TimerGameCalendar::YearMonthDay intro_ymd = TimerGameCalendar::ConvertDateToYMD(e->intro_date);
		int aging_months = aging_ymd.year.base() * 12 + aging_ymd.month;
		int intro_months = intro_ymd.year.base() * 12 + intro_ymd.month;
		if (intro_ymd.day > 1) intro_months++; // Engines are introduced at the first month start at/after intro date.
		e->age = aging_months - intro_months;
		e->company_avail.Set();
		e->flags.Set(EngineFlag::Available);
	}

	/* Get parent variant index for syncing reliability via random seed. */
	const Engine *re = e;
	while (re->info.variant_id != EngineID::Invalid() && re->info.extra_flags.Test(ExtraEngineFlag::SyncReliability)) {
		re = Engine::Get(re->info.variant_id);
	}

	SetRandomSeed(_settings_game.game_creation.generation_seed ^ seed ^
	              (re->index.base() << 16) ^ (re->info.base_intro.base() << 12) ^ (re->info.decay_speed << 8) ^
	              (re->info.lifelength.base() << 4) ^ re->info.retire_early ^
	              e->type ^
	              e->GetGRFID());

	/* Base reliability defined as a percentage of UINT16_MAX. */
	const uint16_t RELIABILITY_START = UINT16_MAX * 48 / 100;
	const uint16_t RELIABILITY_MAX   = UINT16_MAX * 75 / 100;
	const uint16_t RELIABILITY_FINAL = UINT16_MAX * 25 / 100;

	static_assert(RELIABILITY_START == 0x7AE0);
	static_assert(RELIABILITY_MAX   == 0xBFFF);
	static_assert(RELIABILITY_FINAL == 0x3FFF);

	r = Random();
	/* 14 bits gives a value between 0 and 16383, which is up to an additional 25%p reliability on top of the base reliability. */
	e->reliability_start = GB(r, 16, 14) + RELIABILITY_START;
	e->reliability_max   = GB(r,  0, 14) + RELIABILITY_MAX;

	r = Random();
	e->reliability_final = GB(r, 16, 14) + RELIABILITY_FINAL;

	e->duration_phase_1 = GB(r, 0, 5) + 7;
	e->duration_phase_2 = std::max(0, int(GB(r, 5, 4)) + ei->base_life.base() * 12 - 96);
	e->duration_phase_3 = GB(r, 9, 7) + 120;

	RestoreRandomSeeds(saved_seeds);

	e->reliability_spd_dec = ei->decay_speed << 2;

	/* prevent certain engines from ever appearing. */
	if (!ei->climates.Test(_settings_game.game_creation.landscape)) {
		e->flags.Set(EngineFlag::Available);
		e->company_avail = CompanyMask{};
	}
}

/**
 * Start/initialise all our engines. Must be called whenever there are changes
 * to the NewGRF config.
 */
void StartupEngines()
{
	/* Aging of vehicles stops, so account for that when starting late */
	const TimerGameCalendar::Date aging_date = std::min(TimerGameCalendar::date, TimerGameCalendar::ConvertYMDToDate(_year_engine_aging_stops, 0, 1));
	TimerGameCalendar::YearMonthDay aging_ymd = TimerGameCalendar::ConvertDateToYMD(aging_date);
	uint32_t seed = Random();

	for (Engine *e : Engine::Iterate()) {
		StartupOneEngine(e, aging_ymd, seed);
	}
	for (Engine *e : Engine::Iterate()) {
		CalcEngineReliability(e, false);
	}

	/* Update the bitmasks for the vehicle lists */
	for (Company *c : Company::Iterate()) {
		c->avail_railtypes = GetCompanyRailTypes(c->index);
		c->avail_roadtypes = GetCompanyRoadTypes(c->index);
	}

	/* Invalidate any open purchase lists */
	InvalidateWindowClassesData(WC_BUILD_VEHICLE);

	SetWindowClassesDirty(WC_BUILD_VEHICLE);
	SetWindowClassesDirty(WC_REPLACE_VEHICLE);
}

/**
 * Allows engine \a eid to be used by a company \a company.
 * @param eid The engine to enable.
 * @param company The company to allow using the engine.
 */
static void EnableEngineForCompany(EngineID eid, CompanyID company)
{
	Engine *e = Engine::Get(eid);
	Company *c = Company::Get(company);

	e->company_avail.Set(company);
	if (e->type == VEH_TRAIN) {
		c->avail_railtypes = GetCompanyRailTypes(c->index);
	} else if (e->type == VEH_ROAD) {
		c->avail_roadtypes = GetCompanyRoadTypes(c->index);
	}

	if (company == _local_company) {
		AddRemoveEngineFromAutoreplaceAndBuildWindows(e->type);

		/* Update the toolbar. */
		InvalidateWindowData(WC_MAIN_TOOLBAR, 0);
		if (e->type == VEH_ROAD) InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_ROAD);
		if (e->type == VEH_SHIP) InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_WATER);
		if (e->type == VEH_AIRCRAFT) InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_AIR);
	}
}

/**
 * Forbids engine \a eid to be used by a company \a company.
 * @param eid The engine to disable.
 * @param company The company to forbid using the engine.
 */
static void DisableEngineForCompany(EngineID eid, CompanyID company)
{
	Engine *e = Engine::Get(eid);
	Company *c = Company::Get(company);

	e->company_avail.Reset(company);
	if (e->type == VEH_TRAIN) {
		c->avail_railtypes = GetCompanyRailTypes(c->index);
	} else if (e->type == VEH_ROAD) {
		c->avail_roadtypes = GetCompanyRoadTypes(c->index);
	}

	if (company == _local_company) {
		ClearLastVariant(e->index, e->type);
		AddRemoveEngineFromAutoreplaceAndBuildWindows(e->type);
	}
}

/**
 * Company \a company accepts engine \a eid for preview.
 * @param eid Engine being accepted (is under preview).
 * @param company Current company previewing the engine.
 * @param recursion_depth Recursion depth to avoid infinite loop.
 */
static void AcceptEnginePreview(EngineID eid, CompanyID company, int recursion_depth = 0)
{
	Engine *e = Engine::Get(eid);

	e->preview_company = CompanyID::Invalid();
	e->preview_asked.Set();

	EnableEngineForCompany(eid, company);

	/* Notify preview window, that it might want to close.
	 * Note: We cannot directly close the window.
	 *       In singleplayer this function is called from the preview window, so
	 *       we have to use the GUI-scope scheduling of InvalidateWindowData.
	 */
	InvalidateWindowData(WC_ENGINE_PREVIEW, eid);

	/* Don't search for variants to include if we are 10 levels deep already. */
	if (recursion_depth >= 10) return;

	/* Find variants to be included in preview. */
	for (Engine *ve : Engine::IterateType(e->type)) {
		if (ve->index != eid && ve->info.variant_id == eid && ve->info.extra_flags.Test(ExtraEngineFlag::JoinPreview)) {
			AcceptEnginePreview(ve->index, company, recursion_depth + 1);
		}
	}
}

/**
 * Get the best company for an engine preview.
 * @param e Engine to preview.
 * @return Best company if it exists, #CompanyID::Invalid() otherwise.
 */
static CompanyID GetPreviewCompany(Engine *e)
{
	CompanyID best_company = CompanyID::Invalid();

	/* For trains the cargomask has no useful meaning, since you can attach other wagons */
	CargoTypes cargomask = e->type != VEH_TRAIN ? GetUnionOfArticulatedRefitMasks(e->index, true) : ALL_CARGOTYPES;

	int32_t best_hist = -1;
	for (const Company *c : Company::Iterate()) {
		if (c->block_preview == 0 && !e->preview_asked.Test(c->index) &&
				c->old_economy[0].performance_history > best_hist) {

			/* Check whether the company uses similar vehicles */
			for (const Vehicle *v : Vehicle::Iterate()) {
				if (v->owner != c->index || v->type != e->type) continue;
				if (!v->GetEngine()->CanCarryCargo() || !HasBit(cargomask, v->cargo_type)) continue;

				best_hist = c->old_economy[0].performance_history;
				best_company = c->index;
				break;
			}
		}
	}

	return best_company;
}

/**
 * Checks if a vehicle type is disabled for all/ai companies.
 * @param type The vehicle type which shall be checked.
 * @param ai If true, check if the type is disabled for AI companies, otherwise check if
 *           the vehicle type is disabled for human companies.
 * @return Whether or not a vehicle type is disabled.
 */
static bool IsVehicleTypeDisabled(VehicleType type, bool ai)
{
	switch (type) {
		case VEH_TRAIN:    return _settings_game.vehicle.max_trains == 0   || (ai && _settings_game.ai.ai_disable_veh_train);
		case VEH_ROAD:     return _settings_game.vehicle.max_roadveh == 0  || (ai && _settings_game.ai.ai_disable_veh_roadveh);
		case VEH_SHIP:     return _settings_game.vehicle.max_ships == 0    || (ai && _settings_game.ai.ai_disable_veh_ship);
		case VEH_AIRCRAFT: return _settings_game.vehicle.max_aircraft == 0 || (ai && _settings_game.ai.ai_disable_veh_aircraft);

		default: NOT_REACHED();
	}
}

/** Daily check to offer an exclusive engine preview to the companies. */
static const IntervalTimer<TimerGameCalendar> _calendar_engines_daily({TimerGameCalendar::DAY, TimerGameCalendar::Priority::ENGINE}, [](auto)
{
	for (Company *c : Company::Iterate()) {
		c->avail_railtypes = AddDateIntroducedRailTypes(c->avail_railtypes, TimerGameCalendar::date);
		c->avail_roadtypes = AddDateIntroducedRoadTypes(c->avail_roadtypes, TimerGameCalendar::date);
	}

	if (TimerGameCalendar::year >= _year_engine_aging_stops) return;

	for (Engine *e : Engine::Iterate()) {
		EngineID i = e->index;
		if (e->flags.Test(EngineFlag::ExclusivePreview)) {
			if (e->preview_company != CompanyID::Invalid()) {
				if (!--e->preview_wait) {
					CloseWindowById(WC_ENGINE_PREVIEW, i);
					e->preview_company = CompanyID::Invalid();
				}
			} else if (e->preview_asked.Count() < MAX_COMPANIES) {
				e->preview_company = GetPreviewCompany(e);

				if (e->preview_company == CompanyID::Invalid()) {
					e->preview_asked.Set();
					continue;
				}

				e->preview_asked.Set(e->preview_company);
				e->preview_wait = 20;
				/* AIs are intentionally not skipped for preview even if they cannot build a certain
				 * vehicle type. This is done to not give poor performing human companies an "unfair"
				 * boost that they wouldn't have gotten against other human companies. The check on
				 * the line below is just to make AIs not notice that they have a preview if they
				 * cannot build the vehicle. */
				if (!IsVehicleTypeDisabled(e->type, true)) AI::NewEvent(e->preview_company, new ScriptEventEnginePreview(i));
				if (IsInteractiveCompany(e->preview_company)) ShowEnginePreviewWindow(i);
			}
		}
	}
});

/**
 * Clear the 'hidden' flag for all engines of a new company.
 * @param cid Company being created.
 */
void ClearEnginesHiddenFlagOfCompany(CompanyID cid)
{
	for (Engine *e : Engine::Iterate()) {
		e->company_hidden.Reset(cid);
	}
}

/**
 * Set the visibility of an engine.
 * @param flags Operation to perform.
 * @param engine_id Engine id..
 * @param hide Set for hidden, unset for visible.
 * @return The cost of this operation or an error.
 */
CommandCost CmdSetVehicleVisibility(DoCommandFlags flags, EngineID engine_id, bool hide)
{
	Engine *e = Engine::GetIfValid(engine_id);
	if (e == nullptr || _current_company >= MAX_COMPANIES) return CMD_ERROR;
	if (!IsEngineBuildable(e->index, e->type, _current_company)) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		e->company_hidden.Set(_current_company, hide);
		AddRemoveEngineFromAutoreplaceAndBuildWindows(e->type);
	}

	return CommandCost();
}

/**
 * Accept an engine prototype. XXX - it is possible that the top-company
 * changes while you are waiting to accept the offer? Then it becomes invalid
 * @param flags operation to perform
 * @param engine_id engine-prototype offered
 * @return the cost of this operation or an error
 */
CommandCost CmdWantEnginePreview(DoCommandFlags flags, EngineID engine_id)
{
	Engine *e = Engine::GetIfValid(engine_id);
	if (e == nullptr || !e->flags.Test(EngineFlag::ExclusivePreview) || e->preview_company != _current_company) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) AcceptEnginePreview(engine_id, _current_company);

	return CommandCost();
}

/**
 * Allow or forbid a specific company to use an engine
 * @param flags operation to perform
 * @param engine_id engine id
 * @param company_id Company to allow/forbid the use of an engine.
 * @param allow false to forbid, true to allow.
 * @return the cost of this operation or an error
 */
CommandCost CmdEngineCtrl(DoCommandFlags flags, EngineID engine_id, CompanyID company_id, bool allow)
{
	if (_current_company != OWNER_DEITY) return CMD_ERROR;

	if (!Engine::IsValidID(engine_id) || !Company::IsValidID(company_id)) return CMD_ERROR;

	if (flags.Test(DoCommandFlag::Execute)) {
		if (allow) {
			EnableEngineForCompany(engine_id, company_id);
		} else {
			DisableEngineForCompany(engine_id, company_id);
		}
	}

	return CommandCost();
}

/**
 * An engine has become available for general use.
 * Also handle the exclusive engine preview contract.
 * @param e Engine generally available as of now.
 */
static void NewVehicleAvailable(Engine *e)
{
	EngineID index = e->index;

	/* In case the company didn't build the vehicle during the intro period,
	 * prevent that company from getting future intro periods for a while. */
	if (e->flags.Test(EngineFlag::ExclusivePreview)) {
		for (Company *c : Company::Iterate()) {
			if (!e->company_avail.Test(c->index)) continue;

			/* Check the company's 'ALL_GROUP' group statistics. This only includes countable vehicles, which is fine
			 * as those are the only engines that can be given exclusive previews. */
			if (GetGroupNumEngines(c->index, ALL_GROUP, e->index) == 0) {
				/* The company did not build this engine during preview. */
				c->block_preview = 20;
			}
		}
	}

	e->flags.Reset(EngineFlag::ExclusivePreview).Set(EngineFlag::Available);
	AddRemoveEngineFromAutoreplaceAndBuildWindows(e->type);

	/* Now available for all companies */
	e->company_avail.Set();

	/* Do not introduce new rail wagons */
	if (IsWagon(index)) return;

	if (e->type == VEH_TRAIN) {
		/* maybe make another rail type available */
		assert(e->VehInfo<RailVehicleInfo>().railtypes != RailTypes{});
		RailTypes introduced = GetAllIntroducesRailTypes(e->VehInfo<RailVehicleInfo>().railtypes);
		for (Company *c : Company::Iterate()) c->avail_railtypes = AddDateIntroducedRailTypes(c->avail_railtypes | introduced, TimerGameCalendar::date);
	} else if (e->type == VEH_ROAD) {
		/* maybe make another road type available */
		assert(e->VehInfo<RoadVehicleInfo>().roadtype < ROADTYPE_END);
		for (Company *c : Company::Iterate()) c->avail_roadtypes = AddDateIntroducedRoadTypes(c->avail_roadtypes | GetRoadTypeInfo(e->VehInfo<RoadVehicleInfo>().roadtype)->introduces_roadtypes, TimerGameCalendar::date);
	}

	/* Only broadcast event if AIs are able to build this vehicle type. */
	if (!IsVehicleTypeDisabled(e->type, true)) AI::BroadcastNewEvent(new ScriptEventEngineAvailable(index));

	/* Only provide the "New Vehicle available" news paper entry, if engine can be built. */
	if (!IsVehicleTypeDisabled(e->type, false) && !e->info.extra_flags.Test(ExtraEngineFlag::NoNews)) {
		AddNewsItem(GetEncodedString(STR_NEWS_NEW_VEHICLE_NOW_AVAILABLE_WITH_TYPE,
				GetEngineCategoryName(index),
				PackEngineNameDParam(index, EngineNameContext::PreviewNews)),
			NewsType::NewVehicles, NewsStyle::Vehicle, {}, index);
	}

	/* Update the toolbar. */
	if (e->type == VEH_ROAD) InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_ROAD);
	if (e->type == VEH_SHIP) InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_WATER);
	if (e->type == VEH_AIRCRAFT) InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_AIR);

	/* Close pending preview windows */
	CloseWindowById(WC_ENGINE_PREVIEW, index);
}

/** Monthly update of the availability, reliability, and preview offers of the engines. */
void CalendarEnginesMonthlyLoop()
{
	if (TimerGameCalendar::year < _year_engine_aging_stops) {
		bool refresh = false;
		for (Engine *e : Engine::Iterate()) {
			/* Age the vehicle */
			if (e->flags.Test(EngineFlag::Available) && e->age != INT32_MAX) {
				e->age++;
				CalcEngineReliability(e, true);
				refresh = true;
			}

			/* Do not introduce invalid engines */
			if (!e->IsEnabled()) continue;

			if (!e->flags.Test(EngineFlag::Available) && TimerGameCalendar::date >= (e->intro_date + CalendarTime::DAYS_IN_YEAR)) {
				/* Introduce it to all companies */
				NewVehicleAvailable(e);
			} else if (!e->flags.Any({EngineFlag::Available, EngineFlag::ExclusivePreview}) && TimerGameCalendar::date >= e->intro_date) {
				/* Introduction date has passed...
				 * Check if it is allowed to build this vehicle type at all
				 * based on the current game settings. If not, it does not
				 * make sense to show the preview dialog to any company. */
				if (IsVehicleTypeDisabled(e->type, false)) continue;

				/* Do not introduce new rail wagons */
				if (IsWagon(e->index)) continue;

				/* Engine has no preview */
				if (e->info.extra_flags.Test(ExtraEngineFlag::NoPreview)) continue;

				/* Show preview dialog to one of the companies. */
				e->flags.Set(EngineFlag::ExclusivePreview);
				e->preview_company = CompanyID::Invalid();
				e->preview_asked = CompanyMask{};
			}
		}

		InvalidateWindowClassesData(WC_BUILD_VEHICLE); // rebuild the purchase list (esp. when sorted by reliability)

		if (refresh) {
			SetWindowClassesDirty(WC_BUILD_VEHICLE);
			SetWindowClassesDirty(WC_REPLACE_VEHICLE);
		}
	}
}

static const IntervalTimer<TimerGameCalendar> _calendar_engines_monthly({TimerGameCalendar::MONTH, TimerGameCalendar::Priority::ENGINE}, [](auto)
{
	CalendarEnginesMonthlyLoop();
});

/**
 * Is \a name still free as name for an engine?
 * @param name New name of an engine.
 * @return \c false if the name is being used already, else \c true.
 */
static bool IsUniqueEngineName(const std::string &name)
{
	for (const Engine *e : Engine::Iterate()) {
		if (!e->name.empty() && e->name == name) return false;
	}

	return true;
}

/**
 * Rename an engine.
 * @param flags operation to perform
 * @param engine_id engine ID to rename
 * @param text the new name or an empty string when resetting to the default
 * @return the cost of this operation or an error
 */
CommandCost CmdRenameEngine(DoCommandFlags flags, EngineID engine_id, const std::string &text)
{
	Engine *e = Engine::GetIfValid(engine_id);
	if (e == nullptr) return CMD_ERROR;

	bool reset = text.empty();

	if (!reset) {
		if (Utf8StringLength(text) >= MAX_LENGTH_ENGINE_NAME_CHARS) return CMD_ERROR;
		if (!IsUniqueEngineName(text)) return CommandCost(STR_ERROR_NAME_MUST_BE_UNIQUE);
	}

	if (flags.Test(DoCommandFlag::Execute)) {
		if (reset) {
			e->name.clear();
		} else {
			e->name = text;
		}

		MarkWholeScreenDirty();
	}

	return CommandCost();
}


/**
 * Check if an engine is buildable.
 * @param engine  index of the engine to check.
 * @param type    the type the engine should be.
 * @param company index of the company.
 * @return True if an engine is valid, of the specified type, and buildable by
 *              the given company.
 */
bool IsEngineBuildable(EngineID engine, VehicleType type, CompanyID company)
{
	const Engine *e = Engine::GetIfValid(engine);

	/* check if it's an engine that is in the engine array */
	if (e == nullptr) return false;

	/* check if it's an engine of specified type */
	if (e->type != type) return false;

	/* check if it's available ... */
	if (company == OWNER_DEITY) {
		/* ... for any company (preview does not count) */
		if (!e->flags.Test(EngineFlag::Available) || e->company_avail.None()) return false;
	} else {
		/* ... for this company */
		if (!e->company_avail.Test(company)) return false;
	}

	if (!e->IsEnabled()) return false;

	if (type == VEH_TRAIN && company != OWNER_DEITY) {
		/* Check if the rail type is available to this company */
		const Company *c = Company::Get(company);
		if (!GetAllCompatibleRailTypes(e->VehInfo<RailVehicleInfo>().railtypes).Any(c->avail_railtypes)) return false;
	}
	if (type == VEH_ROAD && company != OWNER_DEITY) {
		/* Check if the road type is available to this company */
		const Company *c = Company::Get(company);
		if (!GetRoadTypeInfo(e->VehInfo<RoadVehicleInfo>().roadtype)->powered_roadtypes.Any(c->avail_roadtypes)) return false;
	}

	return true;
}

/**
 * Check if an engine is refittable.
 * Note: Likely you want to use IsArticulatedVehicleRefittable().
 * @param engine index of the engine to check.
 * @return true if the engine is refittable.
 */
bool IsEngineRefittable(EngineID engine)
{
	const Engine *e = Engine::GetIfValid(engine);

	/* check if it's an engine that is in the engine array */
	if (e == nullptr) return false;

	if (!e->CanCarryCargo()) return false;

	const EngineInfo *ei = &e->info;
	if (ei->refit_mask == 0) return false;

	/* Are there suffixes?
	 * Note: This does not mean the suffixes are actually available for every consist at any time. */
	if (ei->callback_mask.Test(VehicleCallbackMask::CargoSuffix)) return true;

	/* Is there any cargo except the default cargo? */
	CargoType default_cargo = e->GetDefaultCargoType();
	CargoTypes default_cargo_mask = 0;
	SetBit(default_cargo_mask, default_cargo);
	return IsValidCargoType(default_cargo) && ei->refit_mask != default_cargo_mask;
}

/**
 * Check for engines that have an appropriate availability.
 */
void CheckEngines()
{
	TimerGameCalendar::Date min_date{INT32_MAX};

	for (const Engine *e : Engine::Iterate()) {
		if (!e->IsEnabled()) continue;

		/* Don't consider train wagons, we need a powered engine available. */
		if (e->type == VEH_TRAIN && e->VehInfo<RailVehicleInfo>().railveh_type == RAILVEH_WAGON) continue;

		/* We have an available engine... yay! */
		if (e->flags.Test(EngineFlag::Available) && e->company_avail.Any()) return;

		/* Okay, try to find the earliest date. */
		min_date = std::min(min_date, e->info.base_intro);
	}

	if (min_date < INT32_MAX) {
		ShowErrorMessage(GetEncodedString(STR_ERROR_NO_VEHICLES_AVAILABLE_YET),
			GetEncodedString(STR_ERROR_NO_VEHICLES_AVAILABLE_YET_EXPLANATION, min_date), WL_WARNING);
	} else {
		ShowErrorMessage(GetEncodedString(STR_ERROR_NO_VEHICLES_AVAILABLE_AT_ALL),
			GetEncodedString(STR_ERROR_NO_VEHICLES_AVAILABLE_AT_ALL_EXPLANATION), WL_WARNING);
	}
}
