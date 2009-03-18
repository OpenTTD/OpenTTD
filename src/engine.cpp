/* $Id$ */

/** @file engine.cpp Base for all engine handling. */

#include "stdafx.h"
#include "debug.h"
#include "company_func.h"
#include "command_func.h"
#include "news_func.h"
#include "variables.h"
#include "aircraft.h"
#include "newgrf_engine.h"
#include "group.h"
#include "strings_func.h"
#include "gfx_func.h"
#include "core/random_func.hpp"
#include "window_func.h"
#include "date_func.h"
#include "autoreplace_gui.h"
#include "string_func.h"
#include "oldpool_func.h"
#include "ai/ai.hpp"
#include "vehicle_func.h"
#include "settings_type.h"

#include "table/strings.h"
#include "table/engines.h"

DEFINE_OLD_POOL_GENERIC(Engine, Engine)

EngineOverrideManager _engine_mngr;

/** Year that engine aging stops. Engines will not reduce in reliability
 * and no more engines will be introduced */
Year _year_engine_aging_stops;

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
	this->internal_id = base;
	this->list_position = base;

	/* Check if this base engine is within the original engine data range */
	if (base >= _engine_counts[type]) {
		/* Mark engine as valid anyway */
		this->info.climates = 0x80;
		/* Set model life to maximum to make wagons available */
		this->info.base_life = 0xFF;
		return;
	}

	/* Copy the original engine info for this slot */
	this->info = _orig_engine_info[_engine_offsets[type] + base];

	/* Copy the original engine data for this slot */
	switch (type) {
		default: NOT_REACHED();

		case VEH_TRAIN:
			this->u.rail = _orig_rail_vehicle_info[base];
			this->image_index = this->u.rail.image_index;
			this->info.string_id = STR_8000_KIRBY_PAUL_TANK_STEAM + base;

			/* Set the default model life of original wagons to "infinite" */
			if (this->u.rail.railveh_type == RAILVEH_WAGON) this->info.base_life = 0xFF;

			break;

		case VEH_ROAD:
			this->u.road = _orig_road_vehicle_info[base];
			this->image_index = this->u.road.image_index;
			this->info.string_id = STR_8074_MPS_REGAL_BUS + base;
			break;

		case VEH_SHIP:
			this->u.ship = _orig_ship_vehicle_info[base];
			this->image_index = this->u.ship.image_index;
			this->info.string_id = STR_80CC_MPS_OIL_TANKER + base;
			break;

		case VEH_AIRCRAFT:
			this->u.air = _orig_aircraft_vehicle_info[base];
			this->image_index = this->u.air.image_index;
			this->info.string_id = STR_80D7_SAMPSON_U52 + base;
			break;
	}
}

Engine::~Engine()
{
	UnloadWagonOverrides(this);
	free(this->name);
}

/**
 * Determines the default cargo type of an engine.
 *
 * Usually a valid cargo is returned, even though the vehicle has zero capacity, and can therefore not carry anything. But the cargotype is still used
 * for livery selection etc..
 *
 * Vehicles with CT_INVALID as default cargo are usally not available, but it can appear as default cargo of articulated parts.
 *
 * @return The default cargo type.
 * @see CanCarryCargo
 */
CargoID Engine::GetDefaultCargoType() const
{
	switch (this->type) {
		case VEH_TRAIN:
			return this->u.rail.cargo_type;

		case VEH_ROAD:
			return this->u.road.cargo_type;

		case VEH_SHIP:
			return this->u.ship.cargo_type;

		case VEH_AIRCRAFT:
			return FindFirstRefittableCargo(this->index);

		default: NOT_REACHED();
	}
}

/**
 * Determines whether an engine can carry something.
 * A vehicle cannot carry anything if its capacity is zero, or none of the possible cargos is available in the climate.
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
 * Determines the default cargo capacity of an engine for display purposes.
 *
 * For planes carrying both passenger and mail this is the passenger capacity.
 * For multiheaded engines this is the capacity of both heads.
 * For articulated engines use GetCapacityOfArticulatedParts
 *
 * @return The default capacity
 * @see GetDefaultCargoType
 */
uint Engine::GetDisplayDefaultCapacity() const
{
	if (!this->CanCarryCargo()) return 0;
	switch (type) {
		case VEH_TRAIN:
			return GetEngineProperty(this->index, 0x14, this->u.rail.capacity) + (this->u.rail.railveh_type == RAILVEH_MULTIHEAD ? this->u.rail.capacity : 0);

		case VEH_ROAD:
			return GetEngineProperty(this->index, 0x0F, this->u.road.capacity);

		case VEH_SHIP:
			return GetEngineProperty(this->index, 0x0D, this->u.ship.capacity);

		case VEH_AIRCRAFT:
			return AircraftDefaultCargoCapacity(this->GetDefaultCargoType(), &this->u.air);

		default: NOT_REACHED();
	}
}

Money Engine::GetRunningCost() const
{
	switch (this->type) {
		case VEH_ROAD:
			return this->u.road.running_cost * GetPriceByIndex(this->u.road.running_cost_class) >> 8;

		case VEH_TRAIN:
			return GetEngineProperty(this->index, 0x0D, this->u.rail.running_cost) * GetPriceByIndex(this->u.rail.running_cost_class) >> 8;

		case VEH_SHIP:
			return GetEngineProperty(this->index, 0x0F, this->u.ship.running_cost) * _price.ship_running >> 8;

		case VEH_AIRCRAFT:
			return GetEngineProperty(this->index, 0x0E, this->u.air.running_cost) * _price.aircraft_running >> 8;

		default: NOT_REACHED();
	}
}

Money Engine::GetCost() const
{
	switch (this->type) {
		case VEH_ROAD:
			return GetEngineProperty(this->index, 0x11, this->u.road.cost_factor) * (_price.roadveh_base >> 3) >> 5;

		case VEH_TRAIN:
			if (this->u.rail.railveh_type == RAILVEH_WAGON) {
				return (GetEngineProperty(this->index, 0x17, this->u.rail.cost_factor) * _price.build_railwagon) >> 8;
			} else {
				return GetEngineProperty(this->index, 0x17, this->u.rail.cost_factor) * (_price.build_railvehicle >> 3) >> 5;
			}
		case VEH_SHIP:
			return GetEngineProperty(this->index, 0x0A, this->u.ship.cost_factor) * (_price.ship_base >> 3) >> 5;

		case VEH_AIRCRAFT:
			return GetEngineProperty(this->index, 0x0B, this->u.air.cost_factor) * (_price.aircraft_base >> 3) >> 5;

		default: NOT_REACHED();
	}
}

/**
 * Returns max speed for display purposes
 * @return max speed in km-ish/h
 */
uint Engine::GetDisplayMaxSpeed() const
{
	switch (this->type) {
		case VEH_TRAIN:
			return GetEngineProperty(this->index, 0x09, this->u.rail.max_speed);

		case VEH_ROAD:
			return this->u.road.max_speed / 2;

		case VEH_SHIP:
			return GetEngineProperty(this->index, 0x0B, this->u.ship.max_speed) / 2;

		case VEH_AIRCRAFT:
			return this->u.air.max_speed;

		default: NOT_REACHED();
	}
}

uint Engine::GetPower() const
{
	/* Currently only trains have 'power' */
	switch (this->type) {
		case VEH_TRAIN:
			return GetEngineProperty(this->index, 0x0B, this->u.rail.power);

		default: NOT_REACHED();
	}
}

/**
 * Returns the weight for display purposes.
 * For dual-headed train-engines this is the weight of both heads
 * @return weight in display units metric tons
 */
uint Engine::GetDisplayWeight() const
{
	/* Currently only trains have 'weight' */
	switch (this->type) {
		case VEH_TRAIN:
			return GetEngineProperty(this->index, 0x16, this->u.rail.weight) << (this->u.rail.railveh_type == RAILVEH_MULTIHEAD ? 1 : 0);

		default: NOT_REACHED();
	}
}

/**
 * Returns the tractive effort for display purposes.
 * For dual-headed train-engines this is the tractive effort of both heads
 * @return tractive effort in display units kN
 */
uint Engine::GetDisplayMaxTractiveEffort() const
{
	/* Currently only trains have 'tractive effort' */
	switch (this->type) {
		case VEH_TRAIN:
			return (10 * this->GetDisplayWeight() * GetEngineProperty(this->index, 0x1F, this->u.rail.tractive_effort)) / 256;

		default: NOT_REACHED();
	}
}

/**
 * Initializes the EngineOverrideManager with the default engines.
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

/** Sets cached values in Company::num_vehicles and Group::num_vehicles
 */
void SetCachedEngineCounts()
{
	uint engines = GetEnginePoolSize();

	/* Set up the engine count for all companies */
	Company *c;
	FOR_ALL_COMPANIES(c) {
		free(c->num_engines);
		c->num_engines = CallocT<EngineID>(engines);
	}

	/* Recalculate */
	Group *g;
	FOR_ALL_GROUPS(g) {
		free(g->num_engines);
		g->num_engines = CallocT<EngineID>(engines);
	}

	const Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		if (!IsEngineCountable(v)) continue;

		assert(v->engine_type < engines);

		GetCompany(v->owner)->num_engines[v->engine_type]++;

		if (v->group_id == DEFAULT_GROUP) continue;

		g = GetGroup(v->group_id);
		assert(v->type == g->vehicle_type);
		assert(v->owner == g->owner);

		g->num_engines[v->engine_type]++;
	}
}

void SetupEngines()
{
	_Engine_pool.CleanPool();
	_Engine_pool.AddBlockToPool();

	assert(_engine_mngr.Length() >= _engine_mngr.NUM_DEFAULT_ENGINES);
	const EngineIDMapping *end = _engine_mngr.End();
	uint index = 0;
	for (const EngineIDMapping *eid = _engine_mngr.Begin(); eid != end; eid++, index++) {
		const Engine *e = new Engine(eid->type, eid->internal_id);
		assert(e->index == index);
	}
}


void ShowEnginePreviewWindow(EngineID engine);

/* Determine if an engine type is a wagon (and not a loco) */
static bool IsWagon(EngineID index)
{
	const Engine *e = GetEngine(index);
	return e->type == VEH_TRAIN && e->u.rail.railveh_type == RAILVEH_WAGON;
}

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
	InvalidateWindowClasses(WC_BUILD_VEHICLE); // Update to show the new reliability
	InvalidateWindowClasses(WC_REPLACE_VEHICLE);
}

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

void StartupOneEngine(Engine *e, Date aging_date)
{
	const EngineInfo *ei = &e->info;
	uint32 r;

	e->age = 0;
	e->flags = 0;
	e->company_avail = 0;

	/* The magic value of 729 days below comes from the NewGRF spec. If the
	 * base intro date is before 1922 then the random number of days is not
	 * added. */
	r = Random();
	e->intro_date = ei->base_intro <= ConvertYMDToDate(1922, 0, 1) ? ei->base_intro : (Date)GB(r, 0, 9) + ei->base_intro;
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

	e->lifelength = ei->lifelength + _settings_game.vehicle.extend_vehicle_life;

	/* prevent certain engines from ever appearing. */
	if (!HasBit(ei->climates, _settings_game.game_creation.landscape)) {
		e->flags |= ENGINE_AVAILABLE;
		e->company_avail = 0;
	}
}

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
}

static void AcceptEnginePreview(EngineID eid, CompanyID company)
{
	Engine *e = GetEngine(eid);
	Company *c = GetCompany(company);

	SetBit(e->company_avail, company);
	if (e->type == VEH_TRAIN) {
		const RailVehicleInfo *rvi = RailVehInfo(eid);

		assert(rvi->railtype < RAILTYPE_END);
		SetBit(c->avail_railtypes, rvi->railtype);
	} else if (e->type == VEH_ROAD) {
		SetBit(c->avail_roadtypes, HasBit(EngInfo(eid)->misc_flags, EF_ROAD_TRAM) ? ROADTYPE_TRAM : ROADTYPE_ROAD);
	}

	e->preview_company_rank = 0xFF;
	if (company == _local_company) {
		AddRemoveEngineFromAutoreplaceAndBuildWindows(e->type);
	}
}

static CompanyID GetBestCompany(uint8 pp)
{
	const Company *c;
	int32 best_hist;
	CompanyID best_company;
	CompanyMask mask = 0;

	do {
		best_hist = -1;
		best_company = INVALID_COMPANY;
		FOR_ALL_COMPANIES(c) {
			if (c->block_preview == 0 && !HasBit(mask, c->index) &&
					c->old_economy[0].performance_history > best_hist) {
				best_hist = c->old_economy[0].performance_history;
				best_company = c->index;
			}
		}

		if (best_company == INVALID_COMPANY) return INVALID_COMPANY;

		SetBit(mask, best_company);
	} while (--pp != 0);

	return best_company;
}

void EnginesDailyLoop()
{
	if (_cur_year >= _year_engine_aging_stops) return;

	Engine *e;
	FOR_ALL_ENGINES(e) {
		EngineID i = e->index;
		if (e->flags & ENGINE_EXCLUSIVE_PREVIEW) {
			if (e->flags & ENGINE_OFFER_WINDOW_OPEN) {
				if (e->preview_company_rank != 0xFF && !--e->preview_wait) {
					e->flags &= ~ENGINE_OFFER_WINDOW_OPEN;
					DeleteWindowById(WC_ENGINE_PREVIEW, i);
					e->preview_company_rank++;
				}
			} else if (e->preview_company_rank != 0xFF) {
				CompanyID best_company = GetBestCompany(e->preview_company_rank);

				if (best_company == INVALID_COMPANY) {
					e->preview_company_rank = 0xFF;
					continue;
				}

				e->flags |= ENGINE_OFFER_WINDOW_OPEN;
				e->preview_wait = 20;
				AI::NewEvent(best_company, new AIEventEnginePreview(i));
				if (IsInteractiveCompany(best_company)) ShowEnginePreviewWindow(i);
			}
		}
	}
}

/** Accept an engine prototype. XXX - it is possible that the top-company
 * changes while you are waiting to accept the offer? Then it becomes invalid
 * @param tile unused
 * @param flags operation to perfom
 * @param p1 engine-prototype offered
 * @param p2 unused
 */
CommandCost CmdWantEnginePreview(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	Engine *e;

	if (!IsEngineIndex(p1)) return CMD_ERROR;
	e = GetEngine(p1);
	if (GetBestCompany(e->preview_company_rank) != _current_company) return CMD_ERROR;

	if (flags & DC_EXEC) AcceptEnginePreview(p1, _current_company);

	return CommandCost();
}

StringID GetEngineCategoryName(EngineID engine);

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
						(v->type == VEH_AIRCRAFT && IsNormalAircraft(v))) {
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
		FOR_ALL_COMPANIES(c) SetBit(c->avail_railtypes, railtype);
	} else if (e->type == VEH_ROAD) {
		/* maybe make another road type available */
		FOR_ALL_COMPANIES(c) SetBit(c->avail_roadtypes, HasBit(e->info.misc_flags, EF_ROAD_TRAM) ? ROADTYPE_TRAM : ROADTYPE_ROAD);
	}

	AI::BroadcastNewEvent(new AIEventEngineAvailable(index));

	SetDParam(0, GetEngineCategoryName(index));
	SetDParam(1, index);
	AddNewsItem(STR_NEW_VEHICLE_NOW_AVAILABLE_WITH_TYPE, NS_NEW_VEHICLES, index, 0);
}

void EnginesMonthlyLoop()
{
	if (_cur_year < _year_engine_aging_stops) {
		Engine *e;
		FOR_ALL_ENGINES(e) {
			/* Age the vehicle */
			if (e->flags & ENGINE_AVAILABLE && e->age != 0xFFFF) {
				e->age++;
				CalcEngineReliability(e);
			}

			if (!(e->flags & ENGINE_AVAILABLE) && _date >= (e->intro_date + DAYS_IN_YEAR)) {
				/* Introduce it to all companies */
				NewVehicleAvailable(e);
			} else if (!(e->flags & (ENGINE_AVAILABLE | ENGINE_EXCLUSIVE_PREVIEW)) && _date >= e->intro_date) {
				/* Introduction date has passed.. show introducing dialog to one companies. */
				e->flags |= ENGINE_EXCLUSIVE_PREVIEW;

				/* Do not introduce new rail wagons */
				if (!IsWagon(e->index))
					e->preview_company_rank = 1; // Give to the company with the highest rating.
			}
		}
	}
}

static bool IsUniqueEngineName(const char *name)
{
	const Engine *e;

	FOR_ALL_ENGINES(e) {
		if (e->name != NULL && strcmp(e->name, name) == 0) return false;
	}

	return true;
}

/** Rename an engine.
 * @param tile unused
 * @param flags operation to perfom
 * @param p1 engine ID to rename
 * @param p2 unused
 */
CommandCost CmdRenameEngine(TileIndex tile, DoCommandFlag flags, uint32 p1, uint32 p2, const char *text)
{
	if (!IsEngineIndex(p1)) return CMD_ERROR;

	bool reset = StrEmpty(text);

	if (!reset) {
		if (strlen(text) >= MAX_LENGTH_ENGINE_NAME_BYTES) return CMD_ERROR;
		if (!IsUniqueEngineName(text)) return_cmd_error(STR_NAME_MUST_BE_UNIQUE);
	}

	if (flags & DC_EXEC) {
		Engine *e = GetEngine(p1);
		free(e->name);

		if (reset) {
			e->name = NULL;
		} else {
			e->name = strdup(text);
		}

		MarkWholeScreenDirty();
	}

	return CommandCost();
}


/** Check if an engine is buildable.
 * @param engine  index of the engine to check.
 * @param type    the type the engine should be.
 * @param company index of the company.
 * @return True if an engine is valid, of the specified type, and buildable by
 *              the given company.
 */
bool IsEngineBuildable(EngineID engine, VehicleType type, CompanyID company)
{
	/* check if it's an engine that is in the engine array */
	if (!IsEngineIndex(engine)) return false;

	const Engine *e = GetEngine(engine);

	/* check if it's an engine of specified type */
	if (e->type != type) return false;

	/* check if it's available */
	if (!HasBit(e->company_avail, company)) return false;

	if (type == VEH_TRAIN) {
		/* Check if the rail type is available to this company */
		const Company *c = GetCompany(company);
		if (!HasBit(c->avail_railtypes, RailVehInfo(engine)->railtype)) return false;
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
	/* check if it's an engine that is in the engine array */
	if (!IsEngineIndex(engine)) return false;

	const Engine *e = GetEngine(engine);

	if (e->type == VEH_SHIP && !e->u.ship.refittable) return false;

	if (!e->CanCarryCargo()) return false;

	const EngineInfo *ei = &e->info;
	if (ei->refit_mask == 0) return false;

	/* Are there suffixes?
	 * Note: This does not mean the suffixes are actually available for every consist at any time. */
	if (HasBit(ei->callbackmask, CBM_VEHICLE_CARGO_SUFFIX)) return true;

	/* Is there any cargo except the default cargo? */
	CargoID default_cargo = e->GetDefaultCargoType();
	return default_cargo != CT_INVALID && ei->refit_mask != 1U << default_cargo;
}
