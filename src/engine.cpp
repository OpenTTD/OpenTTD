/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file engine.cpp Base for all engine handling. */

#include "stdafx.h"
#include "company_func.h"
#include "command_func.h"
#include "news_func.h"
#include "aircraft.h"
#include "newgrf.h"
#include "newgrf_engine.h"
#include "strings_func.h"
#include "core/random_func.hpp"
#include "window_func.h"
#include "date_func.h"
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
static Year _year_engine_aging_stops;

/** Number of engines of each vehicle type in original engine data */
const uint8 _engine_counts[4] = {
	lengthof(_orig_rail_vehicle_info),
	lengthof(_orig_road_vehicle_info),
	lengthof(_orig_ship_vehicle_info),
	lengthof(_orig_aircraft_vehicle_info),
};

/** Offset of the first engine of each vehicle type in original engine data */
const uint8 _engine_offsets[4] = {
	0,
	lengthof(_orig_rail_vehicle_info),
	lengthof(_orig_rail_vehicle_info) + lengthof(_orig_road_vehicle_info),
	lengthof(_orig_rail_vehicle_info) + lengthof(_orig_road_vehicle_info) + lengthof(_orig_ship_vehicle_info),
};

assert_compile(lengthof(_orig_rail_vehicle_info) + lengthof(_orig_road_vehicle_info) + lengthof(_orig_ship_vehicle_info) + lengthof(_orig_aircraft_vehicle_info) == lengthof(_orig_engine_info));

const uint EngineOverrideManager::NUM_DEFAULT_ENGINES = _engine_counts[VEH_TRAIN] + _engine_counts[VEH_ROAD] + _engine_counts[VEH_SHIP] + _engine_counts[VEH_AIRCRAFT];

Engine::Engine() :
	name(NULL),
	overrides_count(0),
	overrides(NULL)
{
}

Engine::Engine(VehicleType type, EngineID base)
{
	this->type = type;
	this->grf_prop.local_id = base;
	this->list_position = base;
	this->preview_company = INVALID_COMPANY;

	/* Check if this base engine is within the original engine data range */
	if (base >= _engine_counts[type]) {
		/* Set model life to maximum to make wagons available */
		this->info.base_life = 0xFF;
		/* Set road vehicle tractive effort to the default value */
		if (type == VEH_ROAD) this->u.road.tractive_effort = 0x4C;
		/* Aircraft must have CT_INVALID as default, as there is no property */
		if (type == VEH_AIRCRAFT) this->info.cargo_type = CT_INVALID;
		/* Set visual effect to the default value */
		switch (type) {
			case VEH_TRAIN: this->u.rail.visual_effect = VE_DEFAULT; break;
			case VEH_ROAD:  this->u.road.visual_effect = VE_DEFAULT; break;
			case VEH_SHIP:  this->u.ship.visual_effect = VE_DEFAULT; break;
			default: break; // The aircraft, disasters and especially visual effects have no NewGRF configured visual effects
		}
		/* Set cargo aging period to the default value. */
		this->info.cargo_age_period = CARGO_AGING_TICKS;
		return;
	}

	/* Copy the original engine info for this slot */
	this->info = _orig_engine_info[_engine_offsets[type] + base];

	/* Copy the original engine data for this slot */
	switch (type) {
		default: NOT_REACHED();

		case VEH_TRAIN:
			this->u.rail = _orig_rail_vehicle_info[base];
			this->original_image_index = this->u.rail.image_index;
			this->info.string_id = STR_VEHICLE_NAME_TRAIN_ENGINE_RAIL_KIRBY_PAUL_TANK_STEAM + base;

			/* Set the default model life of original wagons to "infinite" */
			if (this->u.rail.railveh_type == RAILVEH_WAGON) this->info.base_life = 0xFF;

			break;

		case VEH_ROAD:
			this->u.road = _orig_road_vehicle_info[base];
			this->original_image_index = this->u.road.image_index;
			this->info.string_id = STR_VEHICLE_NAME_ROAD_VEHICLE_MPS_REGAL_BUS + base;
			break;

		case VEH_SHIP:
			this->u.ship = _orig_ship_vehicle_info[base];
			this->original_image_index = this->u.ship.image_index;
			this->info.string_id = STR_VEHICLE_NAME_SHIP_MPS_OIL_TANKER + base;
			break;

		case VEH_AIRCRAFT:
			this->u.air = _orig_aircraft_vehicle_info[base];
			this->original_image_index = this->u.air.image_index;
			this->info.string_id = STR_VEHICLE_NAME_AIRCRAFT_SAMPSON_U52 + base;
			break;
	}
}

Engine::~Engine()
{
	UnloadWagonOverrides(this);
	free(this->name);
}

/**
 * Checks whether the engine is a valid (non-articulated part of an) engine.
 * @return true if enabled
 */
bool Engine::IsEnabled() const
{
	return this->info.string_id != STR_NEWGRF_INVALID_ENGINE && HasBit(this->info.climates, _settings_game.game_creation.landscape);
}

/**
 * Retrieve the GRF ID of the NewGRF the engine is tied to.
 * This is the GRF providing the Action 3.
 * @return GRF ID of the associated NewGRF.
 */
uint32 Engine::GetGRFID() const
{
	const GRFFile *file = this->GetGRF();
	return file == NULL ? 0 : file->grfid;
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
			if (this->u.rail.capacity == 0) return false;
			break;

		case VEH_ROAD:
			if (this->u.road.capacity == 0) return false;
			break;

		case VEH_SHIP:
		case VEH_AIRCRAFT:
			break;

		default: NOT_REACHED();
	}
	return this->GetDefaultCargoType() != CT_INVALID;
}


/**
 * Determines capacity of a given vehicle from scratch.
 * For aircraft the main capacity is determined. Mail might be present as well.
 * @param v Vehicle of interest; NULL in purchase list
 * @param mail_capacity returns secondary cargo (mail) capacity of aircraft
 * @return Capacity
 */
uint Engine::DetermineCapacity(const Vehicle *v, uint16 *mail_capacity) const
{
	assert(v == NULL || this->index == v->engine_type);
	if (mail_capacity != NULL) *mail_capacity = 0;

	if (!this->CanCarryCargo()) return 0;

	bool new_multipliers = HasBit(this->info.misc_flags, EF_NO_DEFAULT_CARGO_MULTIPLIER);
	CargoID default_cargo = this->GetDefaultCargoType();
	CargoID cargo_type = (v != NULL) ? v->cargo_type : default_cargo;

	if (mail_capacity != NULL && this->type == VEH_AIRCRAFT && IsCargoInClass(cargo_type, CC_PASSENGERS)) {
		*mail_capacity = GetEngineProperty(this->index, PROP_AIRCRAFT_MAIL_CAPACITY, this->u.air.mail_capacity, v);
	}

	/* Check the refit capacity callback if we are not in the default configuration, or if we are using the new multiplier algorithm. */
	if (HasBit(this->info.callback_mask, CBM_VEHICLE_REFIT_CAPACITY) &&
			(new_multipliers || default_cargo != cargo_type || (v != NULL && v->cargo_subtype != 0))) {
		uint16 callback = GetVehicleCallback(CBID_VEHICLE_REFIT_CAPACITY, 0, 0, this->index, v);
		if (callback != CALLBACK_FAILED) return callback;
	}

	/* Get capacity according to property resp. CB */
	uint capacity;
	uint extra_mail_cap = 0;
	switch (this->type) {
		case VEH_TRAIN:
			capacity = GetEngineProperty(this->index, PROP_TRAIN_CARGO_CAPACITY,        this->u.rail.capacity, v);

			/* In purchase list add the capacity of the second head. Always use the plain property for this. */
			if (v == NULL && this->u.rail.railveh_type == RAILVEH_MULTIHEAD) capacity += this->u.rail.capacity;
			break;

		case VEH_ROAD:
			capacity = GetEngineProperty(this->index, PROP_ROADVEH_CARGO_CAPACITY,      this->u.road.capacity, v);
			break;

		case VEH_SHIP:
			capacity = GetEngineProperty(this->index, PROP_SHIP_CARGO_CAPACITY,         this->u.ship.capacity, v);
			break;

		case VEH_AIRCRAFT:
			capacity = GetEngineProperty(this->index, PROP_AIRCRAFT_PASSENGER_CAPACITY, this->u.air.passenger_capacity, v);
			if (!IsCargoInClass(cargo_type, CC_PASSENGERS)) {
				extra_mail_cap = GetEngineProperty(this->index, PROP_AIRCRAFT_MAIL_CAPACITY, this->u.air.mail_capacity, v);
			}
			if (!new_multipliers && cargo_type == CT_MAIL) return capacity + extra_mail_cap;
			default_cargo = CT_PASSENGERS; // Always use 'passengers' wrt. cargo multipliers
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
		uint16 default_multiplier = new_multipliers ? 0x100 : CargoSpec::Get(default_cargo)->multiplier;
		uint16 cargo_multiplier = CargoSpec::Get(cargo_type)->multiplier;
		capacity *= cargo_multiplier;
		if (extra_mail_cap > 0) {
			uint mail_multiplier = CargoSpec::Get(CT_MAIL)->multiplier;
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
			base_price = this->u.road.running_cost_class;
			if (base_price == INVALID_PRICE) return 0;
			cost_factor = GetEngineProperty(this->index, PROP_ROADVEH_RUNNING_COST_FACTOR, this->u.road.running_cost);
			break;

		case VEH_TRAIN:
			base_price = this->u.rail.running_cost_class;
			if (base_price == INVALID_PRICE) return 0;
			cost_factor = GetEngineProperty(this->index, PROP_TRAIN_RUNNING_COST_FACTOR, this->u.rail.running_cost);
			break;

		case VEH_SHIP:
			base_price = PR_RUNNING_SHIP;
			cost_factor = GetEngineProperty(this->index, PROP_SHIP_RUNNING_COST_FACTOR, this->u.ship.running_cost);
			break;

		case VEH_AIRCRAFT:
			base_price = PR_RUNNING_AIRCRAFT;
			cost_factor = GetEngineProperty(this->index, PROP_AIRCRAFT_RUNNING_COST_FACTOR, this->u.air.running_cost);
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
			cost_factor = GetEngineProperty(this->index, PROP_ROADVEH_COST_FACTOR, this->u.road.cost_factor);
			break;

		case VEH_TRAIN:
			if (this->u.rail.railveh_type == RAILVEH_WAGON) {
				base_price = PR_BUILD_VEHICLE_WAGON;
				cost_factor = GetEngineProperty(this->index, PROP_TRAIN_COST_FACTOR, this->u.rail.cost_factor);
			} else {
				base_price = PR_BUILD_VEHICLE_TRAIN;
				cost_factor = GetEngineProperty(this->index, PROP_TRAIN_COST_FACTOR, this->u.rail.cost_factor);
			}
			break;

		case VEH_SHIP:
			base_price = PR_BUILD_VEHICLE_SHIP;
			cost_factor = GetEngineProperty(this->index, PROP_SHIP_COST_FACTOR, this->u.ship.cost_factor);
			break;

		case VEH_AIRCRAFT:
			base_price = PR_BUILD_VEHICLE_AIRCRAFT;
			cost_factor = GetEngineProperty(this->index, PROP_AIRCRAFT_COST_FACTOR, this->u.air.cost_factor);
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
			return GetEngineProperty(this->index, PROP_TRAIN_SPEED, this->u.rail.max_speed);

		case VEH_ROAD: {
			uint max_speed = GetEngineProperty(this->index, PROP_ROADVEH_SPEED, 0);
			return (max_speed != 0) ? max_speed * 2 : this->u.road.max_speed / 2;
		}

		case VEH_SHIP:
			return GetEngineProperty(this->index, PROP_SHIP_SPEED, this->u.ship.max_speed) / 2;

		case VEH_AIRCRAFT: {
			uint max_speed = GetEngineProperty(this->index, PROP_AIRCRAFT_SPEED, 0);
			if (max_speed != 0) {
				return (max_speed * 128) / 10;
			}
			return this->u.air.max_speed;
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
			return GetEngineProperty(this->index, PROP_TRAIN_POWER, this->u.rail.power);
		case VEH_ROAD:
			return GetEngineProperty(this->index, PROP_ROADVEH_POWER, this->u.road.power) * 10;

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
			return GetEngineProperty(this->index, PROP_TRAIN_WEIGHT, this->u.rail.weight) << (this->u.rail.railveh_type == RAILVEH_MULTIHEAD ? 1 : 0);
		case VEH_ROAD:
			return GetEngineProperty(this->index, PROP_ROADVEH_WEIGHT, this->u.road.weight) / 4;

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
			return (10 * this->GetDisplayWeight() * GetEngineProperty(this->index, PROP_TRAIN_TRACTIVE_EFFORT, this->u.rail.tractive_effort)) / 256;
		case VEH_ROAD:
			return (10 * this->GetDisplayWeight() * GetEngineProperty(this->index, PROP_ROADVEH_TRACTIVE_EFFORT, this->u.road.tractive_effort)) / 256;

		default: NOT_REACHED();
	}
}

/**
 * Returns the vehicle's (not model's!) life length in days.
 * @return the life length
 */
Date Engine::GetLifeLengthInDays() const
{
	/* Assume leap years; this gives the player a bit more than the given amount of years, but never less. */
	return (this->info.lifelength + _settings_game.vehicle.extend_vehicle_life) * DAYS_IN_LEAP_YEAR;
}

/**
 * Get the range of an aircraft type.
 * @return Range of the aircraft type in tiles or 0 if unlimited range.
 */
uint16 Engine::GetRange() const
{
	switch (this->type) {
		case VEH_AIRCRAFT:
			return GetEngineProperty(this->index, PROP_AIRCRAFT_RANGE, this->u.air.max_range);

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
			switch (this->u.air.subtype) {
				case AIR_HELI: return STR_LIVERY_HELICOPTER;
				case AIR_CTOL: return STR_LIVERY_SMALL_PLANE;
				case AIR_CTOL | AIR_FAST: return STR_LIVERY_LARGE_PLANE;
				default: NOT_REACHED();
			}

		default: NOT_REACHED();
	}
}

/**
 * Initializes the #EngineOverrideManager with the default engines.
 */
void EngineOverrideManager::ResetToDefaultMapping()
{
	this->Clear();
	for (VehicleType type = VEH_TRAIN; type <= VEH_AIRCRAFT; type++) {
		for (uint internal_id = 0; internal_id < _engine_counts[type]; internal_id++) {
			EngineIDMapping *eid = this->Append();
			eid->type            = type;
			eid->grfid           = INVALID_GRFID;
			eid->internal_id     = internal_id;
			eid->substitute_id   = internal_id;
		}
	}
}

/**
 * Looks up an EngineID in the EngineOverrideManager
 * @param type Vehicle type
 * @param grf_local_id The local id in the newgrf
 * @param grfid The GrfID that defines the scope of grf_local_id.
 *              If a newgrf overrides the engines of another newgrf, the "scope grfid" is the ID of the overridden newgrf.
 *              If dynnamic_engines is disabled, all newgrf share the same ID scope identified by INVALID_GRFID.
 * @return The engine ID if present, or INVALID_ENGINE if not.
 */
EngineID EngineOverrideManager::GetID(VehicleType type, uint16 grf_local_id, uint32 grfid)
{
	const EngineIDMapping *end = this->End();
	EngineID index = 0;
	for (const EngineIDMapping *eid = this->Begin(); eid != end; eid++, index++) {
		if (eid->type == type && eid->grfid == grfid && eid->internal_id == grf_local_id) {
			return index;
		}
	}
	return INVALID_ENGINE;
}

/**
 * Tries to reset the engine mapping to match the current NewGRF configuration.
 * This is only possible when there are currently no vehicles in the game.
 * @return false if resetting failed due to present vehicles.
 */
bool EngineOverrideManager::ResetToCurrentNewGRFConfig()
{
	const Vehicle *v;
	FOR_ALL_VEHICLES(v) {
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
	DeleteWindowByClass(WC_ENGINE_PREVIEW);
	_engine_pool.CleanPool();

	assert(_engine_mngr.Length() >= _engine_mngr.NUM_DEFAULT_ENGINES);
	const EngineIDMapping *end = _engine_mngr.End();
	uint index = 0;
	for (const EngineIDMapping *eid = _engine_mngr.Begin(); eid != end; eid++, index++) {
		/* Assert is safe; there won't be more than 256 original vehicles
		 * in any case, and we just cleaned the pool. */
		assert(Engine::CanAllocateItem());
		const Engine *e = new Engine(eid->type, eid->internal_id);
		assert(e->index == index);
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
	return e->type == VEH_TRAIN && e->u.rail.railveh_type == RAILVEH_WAGON;
}

/**
 * Update #reliability of engine \a e, (if needed) update the engine GUIs.
 * @param e %Engine to update.
 */
static void CalcEngineReliability(Engine *e)
{
	uint age = e->age;

	/* Check for early retirement */
	if (e->company_avail != 0 && !_settings_game.vehicle.never_expire_vehicles && e->info.base_life != 0xFF) {
		int retire_early = e->info.retire_early;
		uint retire_early_max_age = max(0, e->duration_phase_1 + e->duration_phase_2 - retire_early * 12);
		if (retire_early != 0 && age >= retire_early_max_age) {
			/* Early retirement is enabled and we're past the date... */
			e->company_avail = 0;
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
		e->company_avail = 0;
		e->reliability = e->reliability_final;
		/* Kick this engine out of the lists */
		AddRemoveEngineFromAutoreplaceAndBuildWindows(e->type);
	}
	SetWindowClassesDirty(WC_BUILD_VEHICLE); // Update to show the new reliability
	SetWindowClassesDirty(WC_REPLACE_VEHICLE);
}

/** Compute the value for #_year_engine_aging_stops. */
void SetYearEngineAgingStops()
{
	/* Determine last engine aging year, default to 2050 as previously. */
	_year_engine_aging_stops = 2050;

	const Engine *e;
	FOR_ALL_ENGINES(e) {
		const EngineInfo *ei = &e->info;

		/* Exclude certain engines */
		if (!HasBit(ei->climates, _settings_game.game_creation.landscape)) continue;
		if (e->type == VEH_TRAIN && e->u.rail.railveh_type == RAILVEH_WAGON) continue;

		/* Base year ending date on half the model life */
		YearMonthDay ymd;
		ConvertDateToYMD(ei->base_intro + (ei->lifelength * DAYS_IN_LEAP_YEAR) / 2, &ymd);

		_year_engine_aging_stops = max(_year_engine_aging_stops, ymd.year);
	}
}

/**
 * Start/initialise one engine.
 * @param e The engine to initialise.
 * @param aging_date The date used for age calculations.
 */
void StartupOneEngine(Engine *e, Date aging_date)
{
	const EngineInfo *ei = &e->info;

	e->age = 0;
	e->flags = 0;
	e->company_avail = 0;
	e->company_hidden = 0;

	/* Don't randomise the start-date in the first two years after gamestart to ensure availability
	 * of engines in early starting games.
	 * Note: TTDP uses fixed 1922 */
	uint32 r = Random();
	e->intro_date = ei->base_intro <= ConvertYMDToDate(_settings_game.game_creation.starting_year + 2, 0, 1) ? ei->base_intro : (Date)GB(r, 0, 9) + ei->base_intro;
	if (e->intro_date <= _date) {
		e->age = (aging_date - e->intro_date) >> 5;
		e->company_avail = (CompanyMask)-1;
		e->flags |= ENGINE_AVAILABLE;
	}

	e->reliability_start = GB(r, 16, 14) + 0x7AE0;
	r = Random();
	e->reliability_max   = GB(r,  0, 14) + 0xBFFF;
	e->reliability_final = GB(r, 16, 14) + 0x3FFF;

	r = Random();
	e->duration_phase_1 = GB(r, 0, 5) + 7;
	e->duration_phase_2 = GB(r, 5, 4) + ei->base_life * 12 - 96;
	e->duration_phase_3 = GB(r, 9, 7) + 120;

	e->reliability_spd_dec = ei->decay_speed << 2;

	CalcEngineReliability(e);

	/* prevent certain engines from ever appearing. */
	if (!HasBit(ei->climates, _settings_game.game_creation.landscape)) {
		e->flags |= ENGINE_AVAILABLE;
		e->company_avail = 0;
	}
}

/**
 * Start/initialise all our engines. Must be called whenever there are changes
 * to the NewGRF config.
 */
void StartupEngines()
{
	Engine *e;
	/* Aging of vehicles stops, so account for that when starting late */
	const Date aging_date = min(_date, ConvertYMDToDate(_year_engine_aging_stops, 0, 1));

	FOR_ALL_ENGINES(e) {
		StartupOneEngine(e, aging_date);
	}

	/* Update the bitmasks for the vehicle lists */
	Company *c;
	FOR_ALL_COMPANIES(c) {
		c->avail_railtypes = GetCompanyRailtypes(c->index);
		c->avail_roadtypes = GetCompanyRoadtypes(c->index);
	}

	/* Invalidate any open purchase lists */
	InvalidateWindowClassesData(WC_BUILD_VEHICLE);
}

/**
 * Company \a company accepts engine \a eid for preview.
 * @param eid Engine being accepted (is under preview).
 * @param company Current company previewing the engine.
 */
static void AcceptEnginePreview(EngineID eid, CompanyID company)
{
	Engine *e = Engine::Get(eid);
	Company *c = Company::Get(company);

	SetBit(e->company_avail, company);
	if (e->type == VEH_TRAIN) {
		assert(e->u.rail.railtype < RAILTYPE_END);
		c->avail_railtypes = AddDateIntroducedRailTypes(c->avail_railtypes | GetRailTypeInfo(e->u.rail.railtype)->introduces_railtypes, _date);
	} else if (e->type == VEH_ROAD) {
		SetBit(c->avail_roadtypes, HasBit(e->info.misc_flags, EF_ROAD_TRAM) ? ROADTYPE_TRAM : ROADTYPE_ROAD);
	}

	e->preview_company = INVALID_COMPANY;
	e->preview_asked = (CompanyMask)-1;
	if (company == _local_company) {
		AddRemoveEngineFromAutoreplaceAndBuildWindows(e->type);
	}

	/* Update the toolbar. */
	if (e->type == VEH_ROAD) InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_ROAD);
	if (e->type == VEH_SHIP) InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_WATER);

	/* Notify preview window, that it might want to close.
	 * Note: We cannot directly close the window.
	 *       In singleplayer this function is called from the preview window, so
	 *       we have to use the GUI-scope scheduling of InvalidateWindowData.
	 */
	InvalidateWindowData(WC_ENGINE_PREVIEW, eid);
}

/**
 * Get the best company for an engine preview.
 * @param e Engine to preview.
 * @return Best company if it exists, #INVALID_COMPANY otherwise.
 */
static CompanyID GetPreviewCompany(Engine *e)
{
	CompanyID best_company = INVALID_COMPANY;

	/* For trains the cargomask has no useful meaning, since you can attach other wagons */
	CargoTypes cargomask = e->type != VEH_TRAIN ? GetUnionOfArticulatedRefitMasks(e->index, true) : ALL_CARGOTYPES;

	int32 best_hist = -1;
	const Company *c;
	FOR_ALL_COMPANIES(c) {
		if (c->block_preview == 0 && !HasBit(e->preview_asked, c->index) &&
				c->old_economy[0].performance_history > best_hist) {

			/* Check whether the company uses similar vehicles */
			Vehicle *v;
			FOR_ALL_VEHICLES(v) {
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
void EnginesDailyLoop()
{
	Company *c;
	FOR_ALL_COMPANIES(c) {
		c->avail_railtypes = AddDateIntroducedRailTypes(c->avail_railtypes, _date);
	}

	if (_cur_year >= _year_engine_aging_stops) return;

	Engine *e;
	FOR_ALL_ENGINES(e) {
		EngineID i = e->index;
		if (e->flags & ENGINE_EXCLUSIVE_PREVIEW) {
			if (e->preview_company != INVALID_COMPANY) {
				if (!--e->preview_wait) {
					DeleteWindowById(WC_ENGINE_PREVIEW, i);
					e->preview_company = INVALID_COMPANY;
				}
			} else if (CountBits(e->preview_asked) < MAX_COMPANIES) {
				e->preview_company = GetPreviewCompany(e);

				if (e->preview_company == INVALID_COMPANY) {
					e->preview_asked = (CompanyMask)-1;
					continue;
				}

				SetBit(e->preview_asked, e->preview_company);
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
}

/**
 * Clear the 'hidden' flag for all engines of a new company.
 * @param cid Company being created.
 */
void ClearEnginesHiddenFlagOfCompany(CompanyID cid)
{
	Engine *e;
	FOR_ALL_ENGINES(e) {
		SB(e->company_hidden, cid, 1, 0);
	}
}

/**
 * Set the visibility of an engine.
 * @param tile Unused.
 * @param flags Operation to perform.
 * @param p1 Unused.
 * @param p2 Bit 31: 0=visible, 1=hidden, other bits for the #EngineID.
 * @param text Unused.
 * @return The cost of this operation or an error.
 */
CommandCost CmdSetVehicleVisibility(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Engine *e = Engine::GetIfValid(GB(p2, 0, 31));
	if (e == NULL || _current_company >= MAX_COMPANIES) return CMD_ERROR;
	if (!IsEngineBuildable(e->index, e->type, _current_company)) return CMD_ERROR;

	if ((flags & DC_EXEC) != 0) {
		SB(e->company_hidden, _current_company, 1, GB(p2, 31, 1));
		AddRemoveEngineFromAutoreplaceAndBuildWindows(e->type);
	}

	return CommandCost();
}

/**
 * Accept an engine prototype. XXX - it is possible that the top-company
 * changes while you are waiting to accept the offer? Then it becomes invalid
 * @param tile unused
 * @param flags operation to perform
 * @param p1 engine-prototype offered
 * @param p2 unused
 * @param text unused
 * @return the cost of this operation or an error
 */
CommandCost CmdWantEnginePreview(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Engine *e = Engine::GetIfValid(p1);
	if (e == NULL || !(e->flags & ENGINE_EXCLUSIVE_PREVIEW) || e->preview_company != _current_company) return CMD_ERROR;

	if (flags & DC_EXEC) AcceptEnginePreview(p1, _current_company);

	return CommandCost();
}

/**
 * An engine has become available for general use.
 * Also handle the exclusive engine preview contract.
 * @param e Engine generally available as of now.
 */
static void NewVehicleAvailable(Engine *e)
{
	Vehicle *v;
	Company *c;
	EngineID index = e->index;

	/* In case the company didn't build the vehicle during the intro period,
	 * prevent that company from getting future intro periods for a while. */
	if (e->flags & ENGINE_EXCLUSIVE_PREVIEW) {
		FOR_ALL_COMPANIES(c) {
			uint block_preview = c->block_preview;

			if (!HasBit(e->company_avail, c->index)) continue;

			/* We assume the user did NOT build it.. prove me wrong ;) */
			c->block_preview = 20;

			FOR_ALL_VEHICLES(v) {
				if (v->type == VEH_TRAIN || v->type == VEH_ROAD || v->type == VEH_SHIP ||
						(v->type == VEH_AIRCRAFT && Aircraft::From(v)->IsNormalAircraft())) {
					if (v->owner == c->index && v->engine_type == index) {
						/* The user did prove me wrong, so restore old value */
						c->block_preview = block_preview;
						break;
					}
				}
			}
		}
	}

	e->flags = (e->flags & ~ENGINE_EXCLUSIVE_PREVIEW) | ENGINE_AVAILABLE;
	AddRemoveEngineFromAutoreplaceAndBuildWindows(e->type);

	/* Now available for all companies */
	e->company_avail = (CompanyMask)-1;

	/* Do not introduce new rail wagons */
	if (IsWagon(index)) return;

	if (e->type == VEH_TRAIN) {
		/* maybe make another rail type available */
		RailType railtype = e->u.rail.railtype;
		assert(railtype < RAILTYPE_END);
		FOR_ALL_COMPANIES(c) c->avail_railtypes = AddDateIntroducedRailTypes(c->avail_railtypes | GetRailTypeInfo(e->u.rail.railtype)->introduces_railtypes, _date);
	} else if (e->type == VEH_ROAD) {
		/* maybe make another road type available */
		FOR_ALL_COMPANIES(c) SetBit(c->avail_roadtypes, HasBit(e->info.misc_flags, EF_ROAD_TRAM) ? ROADTYPE_TRAM : ROADTYPE_ROAD);
	}

	/* Only broadcast event if AIs are able to build this vehicle type. */
	if (!IsVehicleTypeDisabled(e->type, true)) AI::BroadcastNewEvent(new ScriptEventEngineAvailable(index));

	/* Only provide the "New Vehicle available" news paper entry, if engine can be built. */
	if (!IsVehicleTypeDisabled(e->type, false)) {
		SetDParam(0, GetEngineCategoryName(index));
		SetDParam(1, index);
		AddNewsItem(STR_NEWS_NEW_VEHICLE_NOW_AVAILABLE_WITH_TYPE, NT_NEW_VEHICLES, NF_VEHICLE, NR_ENGINE, index);
	}

	/* Update the toolbar. */
	if (e->type == VEH_ROAD) InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_ROAD);
	if (e->type == VEH_SHIP) InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_WATER);

	/* Close pending preview windows */
	DeleteWindowById(WC_ENGINE_PREVIEW, index);
}

/** Monthly update of the availability, reliability, and preview offers of the engines. */
void EnginesMonthlyLoop()
{
	if (_cur_year < _year_engine_aging_stops) {
		Engine *e;
		FOR_ALL_ENGINES(e) {
			/* Age the vehicle */
			if ((e->flags & ENGINE_AVAILABLE) && e->age != MAX_DAY) {
				e->age++;
				CalcEngineReliability(e);
			}

			/* Do not introduce invalid engines */
			if (!e->IsEnabled()) continue;

			if (!(e->flags & ENGINE_AVAILABLE) && _date >= (e->intro_date + DAYS_IN_YEAR)) {
				/* Introduce it to all companies */
				NewVehicleAvailable(e);
			} else if (!(e->flags & (ENGINE_AVAILABLE | ENGINE_EXCLUSIVE_PREVIEW)) && _date >= e->intro_date) {
				/* Introduction date has passed...
				 * Check if it is allowed to build this vehicle type at all
				 * based on the current game settings. If not, it does not
				 * make sense to show the preview dialog to any company. */
				if (IsVehicleTypeDisabled(e->type, false)) continue;

				/* Do not introduce new rail wagons */
				if (IsWagon(e->index)) continue;

				/* Show preview dialog to one of the companies. */
				e->flags |= ENGINE_EXCLUSIVE_PREVIEW;
				e->preview_company = INVALID_COMPANY;
				e->preview_asked = 0;
			}
		}

		InvalidateWindowClassesData(WC_BUILD_VEHICLE); // rebuild the purchase list (esp. when sorted by reliability)
	}
}

/**
 * Is \a name still free as name for an engine?
 * @param name New name of an engine.
 * @return \c false if the name is being used already, else \c true.
 */
static bool IsUniqueEngineName(const char *name)
{
	const Engine *e;

	FOR_ALL_ENGINES(e) {
		if (e->name != NULL && strcmp(e->name, name) == 0) return false;
	}

	return true;
}

/**
 * Rename an engine.
 * @param tile unused
 * @param flags operation to perform
 * @param p1 engine ID to rename
 * @param p2 unused
 * @param text the new name or an empty string when resetting to the default
 * @return the cost of this operation or an error
 */
CommandCost CmdRenameEngine(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Engine *e = Engine::GetIfValid(p1);
	if (e == NULL) return CMD_ERROR;

	bool reset = StrEmpty(text);

	if (!reset) {
		if (Utf8StringLength(text) >= MAX_LENGTH_ENGINE_NAME_CHARS) return CMD_ERROR;
		if (!IsUniqueEngineName(text)) return_cmd_error(STR_ERROR_NAME_MUST_BE_UNIQUE);
	}

	if (flags & DC_EXEC) {
		free(e->name);

		if (reset) {
			e->name = NULL;
		} else {
			e->name = stredup(text);
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
	if (e == NULL) return false;

	/* check if it's an engine of specified type */
	if (e->type != type) return false;

	/* check if it's available ... */
	if (company == OWNER_DEITY) {
		/* ... for any company (preview does not count) */
		if (!(e->flags & ENGINE_AVAILABLE) || e->company_avail == 0) return false;
	} else {
		/* ... for this company */
		if (!HasBit(e->company_avail, company)) return false;
	}

	if (!e->IsEnabled()) return false;

	if (type == VEH_TRAIN && company != OWNER_DEITY) {
		/* Check if the rail type is available to this company */
		const Company *c = Company::Get(company);
		if (((GetRailTypeInfo(e->u.rail.railtype))->compatible_railtypes & c->avail_railtypes) == 0) return false;
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
	if (e == NULL) return false;

	if (!e->CanCarryCargo()) return false;

	const EngineInfo *ei = &e->info;
	if (ei->refit_mask == 0) return false;

	/* Are there suffixes?
	 * Note: This does not mean the suffixes are actually available for every consist at any time. */
	if (HasBit(ei->callback_mask, CBM_VEHICLE_CARGO_SUFFIX)) return true;

	/* Is there any cargo except the default cargo? */
	CargoID default_cargo = e->GetDefaultCargoType();
	CargoTypes default_cargo_mask = 0;
	SetBit(default_cargo_mask, default_cargo);
	return default_cargo != CT_INVALID && ei->refit_mask != default_cargo_mask;
}

/**
 * Check for engines that have an appropriate availability.
 */
void CheckEngines()
{
	const Engine *e;
	Date min_date = INT32_MAX;

	FOR_ALL_ENGINES(e) {
		if (!e->IsEnabled()) continue;

		/* We have an available engine... yay! */
		if ((e->flags & ENGINE_AVAILABLE) != 0 && e->company_avail != 0) return;

		/* Okay, try to find the earliest date. */
		min_date = min(min_date, e->info.base_intro);
	}

	if (min_date < INT32_MAX) {
		SetDParam(0, min_date);
		ShowErrorMessage(STR_ERROR_NO_VEHICLES_AVAILABLE_YET, STR_ERROR_NO_VEHICLES_AVAILABLE_YET_EXPLANATION, WL_WARNING);
	} else {
		ShowErrorMessage(STR_ERROR_NO_VEHICLES_AVAILABLE_AT_ALL, STR_ERROR_NO_VEHICLES_AVAILABLE_AT_ALL_EXPLANATION, WL_WARNING);
	}
}
