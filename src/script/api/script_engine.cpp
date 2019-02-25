/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_engine.cpp Implementation of ScriptEngine. */

#include "../../stdafx.h"
#include "script_engine.hpp"
#include "script_cargo.hpp"
#include "../../company_base.h"
#include "../../strings_func.h"
#include "../../rail.h"
#include "../../engine_base.h"
#include "../../engine_func.h"
#include "../../articulated_vehicles.h"
#include "table/strings.h"

#include "../../safeguards.h"

/* static */ bool ScriptEngine::IsValidEngine(EngineID engine_id)
{
	const Engine *e = ::Engine::GetIfValid(engine_id);
	if (e == NULL || !e->IsEnabled()) return false;

	/* AIs have only access to engines they can purchase or still have in use.
	 * Deity has access to all engined that will be or were available ever. */
	CompanyID company = ScriptObject::GetCompany();
	return company == OWNER_DEITY || ::IsEngineBuildable(engine_id, e->type, company) || ::Company::Get(company)->group_all[e->type].num_engines[engine_id] > 0;
}

/* static */ bool ScriptEngine::IsBuildable(EngineID engine_id)
{
	const Engine *e = ::Engine::GetIfValid(engine_id);
	return e != NULL && ::IsEngineBuildable(engine_id, e->type, ScriptObject::GetCompany());
}

/* static */ char *ScriptEngine::GetName(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return NULL;

	::SetDParam(0, engine_id);
	return GetString(STR_ENGINE_NAME);
}

/* static */ CargoID ScriptEngine::GetCargoType(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return CT_INVALID;

	CargoArray cap = ::GetCapacityOfArticulatedParts(engine_id);

	CargoID most_cargo = CT_INVALID;
	uint amount = 0;
	for (CargoID cid = 0; cid < NUM_CARGO; cid++) {
		if (cap[cid] > amount) {
			amount = cap[cid];
			most_cargo = cid;
		}
	}

	return most_cargo;
}

/* static */ bool ScriptEngine::CanRefitCargo(EngineID engine_id, CargoID cargo_id)
{
	if (!IsValidEngine(engine_id)) return false;
	if (!ScriptCargo::IsValidCargo(cargo_id)) return false;

	return HasBit(::GetUnionOfArticulatedRefitMasks(engine_id, true), cargo_id);
}

/* static */ bool ScriptEngine::CanPullCargo(EngineID engine_id, CargoID cargo_id)
{
	if (!IsValidEngine(engine_id)) return false;
	if (GetVehicleType(engine_id) != ScriptVehicle::VT_RAIL) return false;
	if (!ScriptCargo::IsValidCargo(cargo_id)) return false;

	return (::RailVehInfo(engine_id)->ai_passenger_only != 1) || ScriptCargo::HasCargoClass(cargo_id, ScriptCargo::CC_PASSENGERS);
}


/* static */ int32 ScriptEngine::GetCapacity(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return -1;

	const Engine *e = ::Engine::Get(engine_id);
	switch (e->type) {
		case VEH_ROAD:
		case VEH_TRAIN: {
			CargoArray capacities = GetCapacityOfArticulatedParts(engine_id);
			for (CargoID c = 0; c < NUM_CARGO; c++) {
				if (capacities[c] == 0) continue;
				return capacities[c];
			}
			return -1;
		}

		case VEH_SHIP:
		case VEH_AIRCRAFT:
			return e->GetDisplayDefaultCapacity();

		default: NOT_REACHED();
	}
}

/* static */ int32 ScriptEngine::GetReliability(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return -1;
	if (GetVehicleType(engine_id) == ScriptVehicle::VT_RAIL && IsWagon(engine_id)) return -1;

	return ::ToPercent16(::Engine::Get(engine_id)->reliability);
}

/* static */ int32 ScriptEngine::GetMaxSpeed(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return -1;

	const Engine *e = ::Engine::Get(engine_id);
	int32 max_speed = e->GetDisplayMaxSpeed(); // km-ish/h
	if (e->type == VEH_AIRCRAFT) max_speed /= _settings_game.vehicle.plane_speed;
	return max_speed;
}

/* static */ Money ScriptEngine::GetPrice(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return -1;

	return ::Engine::Get(engine_id)->GetCost();
}

/* static */ int32 ScriptEngine::GetMaxAge(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return -1;
	if (GetVehicleType(engine_id) == ScriptVehicle::VT_RAIL && IsWagon(engine_id)) return -1;

	return ::Engine::Get(engine_id)->GetLifeLengthInDays();
}

/* static */ Money ScriptEngine::GetRunningCost(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return -1;

	return ::Engine::Get(engine_id)->GetRunningCost();
}

/* static */ int32 ScriptEngine::GetPower(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return -1;
	if (GetVehicleType(engine_id) != ScriptVehicle::VT_RAIL && GetVehicleType(engine_id) != ScriptVehicle::VT_ROAD) return -1;
	if (IsWagon(engine_id)) return -1;

	return ::Engine::Get(engine_id)->GetPower();
}

/* static */ int32 ScriptEngine::GetWeight(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return -1;
	if (GetVehicleType(engine_id) != ScriptVehicle::VT_RAIL && GetVehicleType(engine_id) != ScriptVehicle::VT_ROAD) return -1;

	return ::Engine::Get(engine_id)->GetDisplayWeight();
}

/* static */ int32 ScriptEngine::GetMaxTractiveEffort(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return -1;
	if (GetVehicleType(engine_id) != ScriptVehicle::VT_RAIL && GetVehicleType(engine_id) != ScriptVehicle::VT_ROAD) return -1;
	if (IsWagon(engine_id)) return -1;

	return ::Engine::Get(engine_id)->GetDisplayMaxTractiveEffort();
}

/* static */ ScriptDate::Date ScriptEngine::GetDesignDate(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return ScriptDate::DATE_INVALID;

	return (ScriptDate::Date)::Engine::Get(engine_id)->intro_date;
}

/* static */ ScriptVehicle::VehicleType ScriptEngine::GetVehicleType(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return ScriptVehicle::VT_INVALID;

	switch (::Engine::Get(engine_id)->type) {
		case VEH_ROAD:     return ScriptVehicle::VT_ROAD;
		case VEH_TRAIN:    return ScriptVehicle::VT_RAIL;
		case VEH_SHIP:     return ScriptVehicle::VT_WATER;
		case VEH_AIRCRAFT: return ScriptVehicle::VT_AIR;
		default: NOT_REACHED();
	}
}

/* static */ bool ScriptEngine::IsWagon(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return false;
	if (GetVehicleType(engine_id) != ScriptVehicle::VT_RAIL) return false;

	return ::RailVehInfo(engine_id)->power == 0;
}

/* static */ bool ScriptEngine::CanRunOnRail(EngineID engine_id, ScriptRail::RailType track_rail_type)
{
	if (!IsValidEngine(engine_id)) return false;
	if (GetVehicleType(engine_id) != ScriptVehicle::VT_RAIL) return false;
	if (!ScriptRail::IsRailTypeAvailable(track_rail_type)) return false;

	return ::IsCompatibleRail((::RailType)::RailVehInfo(engine_id)->railtype, (::RailType)track_rail_type);
}

/* static */ bool ScriptEngine::HasPowerOnRail(EngineID engine_id, ScriptRail::RailType track_rail_type)
{
	if (!IsValidEngine(engine_id)) return false;
	if (GetVehicleType(engine_id) != ScriptVehicle::VT_RAIL) return false;
	if (!ScriptRail::IsRailTypeAvailable(track_rail_type)) return false;

	return ::HasPowerOnRail((::RailType)::RailVehInfo(engine_id)->railtype, (::RailType)track_rail_type);
}

/* static */ ScriptRoad::RoadType ScriptEngine::GetRoadType(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return ScriptRoad::ROADTYPE_INVALID;
	if (GetVehicleType(engine_id) != ScriptVehicle::VT_ROAD) return ScriptRoad::ROADTYPE_INVALID;

	return HasBit(::EngInfo(engine_id)->misc_flags, EF_ROAD_TRAM) ? ScriptRoad::ROADTYPE_TRAM : ScriptRoad::ROADTYPE_ROAD;
}

/* static */ ScriptRail::RailType ScriptEngine::GetRailType(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return ScriptRail::RAILTYPE_INVALID;
	if (GetVehicleType(engine_id) != ScriptVehicle::VT_RAIL) return ScriptRail::RAILTYPE_INVALID;

	return (ScriptRail::RailType)(uint)::RailVehInfo(engine_id)->railtype;
}

/* static */ bool ScriptEngine::IsArticulated(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return false;
	if (GetVehicleType(engine_id) != ScriptVehicle::VT_ROAD && GetVehicleType(engine_id) != ScriptVehicle::VT_RAIL) return false;

	return IsArticulatedEngine(engine_id);
}

/* static */ ScriptAirport::PlaneType ScriptEngine::GetPlaneType(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return ScriptAirport::PT_INVALID;
	if (GetVehicleType(engine_id) != ScriptVehicle::VT_AIR) return ScriptAirport::PT_INVALID;

	return (ScriptAirport::PlaneType)::AircraftVehInfo(engine_id)->subtype;
}

/* static */ uint ScriptEngine::GetMaximumOrderDistance(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return 0;

	switch (GetVehicleType(engine_id)) {
		case ScriptVehicle::VT_AIR:
			return ::Engine::Get(engine_id)->GetRange() * ::Engine::Get(engine_id)->GetRange();

		default:
			return 0;
	}
}
