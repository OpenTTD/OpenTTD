/* $Id$ */

/** @file ai_engine.cpp Implementation of AIEngine. */

#include "ai_engine.hpp"
#include "ai_cargo.hpp"
#include "../../company_func.h"
#include "../../strings_func.h"
#include "../../vehicle_func.h"
#include "../../settings_type.h"
#include "../../rail.h"
#include "../../engine_base.h"
#include "../../articulated_vehicles.h"
#include "table/strings.h"

/* static */ bool AIEngine::IsValidEngine(EngineID engine_id)
{
	return ::IsEngineIndex(engine_id) && HasBit(::GetEngine(engine_id)->company_avail, _current_company);
}

/* static */ char *AIEngine::GetName(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return NULL;

	static const int len = 64;
	char *engine_name = MallocT<char>(len);

	::SetDParam(0, engine_id);
	::GetString(engine_name, STR_ENGINE_NAME, &engine_name[len - 1]);
	return engine_name;
}

/* static */ CargoID AIEngine::GetCargoType(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return CT_INVALID;

	const Engine *e = ::GetEngine(engine_id);
	if (!e->CanCarryCargo()) return CT_INVALID;

	return e->GetDefaultCargoType();
}

/* static */ bool AIEngine::CanRefitCargo(EngineID engine_id, CargoID cargo_id)
{
	if (!IsValidEngine(engine_id)) return false;
	if (!AICargo::IsValidCargo(cargo_id)) return false;

	if (GetCargoType(engine_id) == cargo_id) return true;
	if (cargo_id == CT_MAIL && ::GetEngine(engine_id)->type == VEH_AIRCRAFT) return true;
	if (::GetEngine(engine_id)->type == VEH_SHIP && !ShipVehInfo(engine_id)->refittable) return false;
	return ::CanRefitTo(engine_id, cargo_id);
}

/* static */ bool AIEngine::CanPullCargo(EngineID engine_id, CargoID cargo_id)
{
	if (!IsValidEngine(engine_id)) return false;
	if (GetVehicleType(engine_id) != AIVehicle::VT_RAIL) return false;
	if (!AICargo::IsValidCargo(cargo_id)) return false;

	return (::RailVehInfo(engine_id)->ai_passenger_only != 1) || AICargo::HasCargoClass(cargo_id, AICargo::CC_PASSENGERS);
}


/* static */ int32 AIEngine::GetCapacity(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return -1;

	const Engine *e = ::GetEngine(engine_id);
	switch (e->type) {
		case VEH_ROAD:
		case VEH_TRAIN: {
			uint16 *capacities = GetCapacityOfArticulatedParts(engine_id, e->type);
			for (CargoID c = 0; c < NUM_CARGO; c++) {
				if (capacities[c] == 0) continue;
				return capacities[c];
			}
			return -1;
		} break;

		case VEH_SHIP:
		case VEH_AIRCRAFT:
			return e->GetDisplayDefaultCapacity();
			break;

		default: NOT_REACHED();
	}
}

/* static */ int32 AIEngine::GetReliability(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return -1;
	if (GetVehicleType(engine_id) == AIVehicle::VT_RAIL && IsWagon(engine_id)) return -1;

	return (::GetEngine(engine_id)->reliability * 100 >> 16);
}

/* static */ int32 AIEngine::GetMaxSpeed(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return -1;

	const Engine *e = ::GetEngine(engine_id);
	int32 max_speed = e->GetDisplayMaxSpeed(); // km-ish/h
	if (e->type == VEH_AIRCRAFT) max_speed /= _settings_game.vehicle.plane_speed;
	return max_speed;
}

/* static */ Money AIEngine::GetPrice(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return -1;

	return ::GetEngine(engine_id)->GetCost();
}

/* static */ int32 AIEngine::GetMaxAge(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return -1;
	if (GetVehicleType(engine_id) == AIVehicle::VT_RAIL && IsWagon(engine_id)) return -1;

	return ::GetEngine(engine_id)->lifelength * DAYS_IN_LEAP_YEAR;
}

/* static */ Money AIEngine::GetRunningCost(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return -1;

	return ::GetEngine(engine_id)->GetRunningCost();
}

/* static */ int32 AIEngine::GetPower(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return -1;
	if (GetVehicleType(engine_id) != AIVehicle::VT_RAIL) return -1;
	if (IsWagon(engine_id)) return -1;

	return ::GetEngine(engine_id)->GetPower();
}

/* static */ int32 AIEngine::GetWeight(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return -1;
	if (GetVehicleType(engine_id) != AIVehicle::VT_RAIL) return -1;

	return ::GetEngine(engine_id)->GetDisplayWeight();
}

/* static */ int32 AIEngine::GetMaxTractiveEffort(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return -1;
	if (GetVehicleType(engine_id) != AIVehicle::VT_RAIL) return -1;
	if (IsWagon(engine_id)) return -1;

	return ::GetEngine(engine_id)->GetDisplayMaxTractiveEffort();
}

/* static */ AIVehicle::VehicleType AIEngine::GetVehicleType(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return AIVehicle::VT_INVALID;

	switch (::GetEngine(engine_id)->type) {
		case VEH_ROAD:     return AIVehicle::VT_ROAD;
		case VEH_TRAIN:    return AIVehicle::VT_RAIL;
		case VEH_SHIP:     return AIVehicle::VT_WATER;
		case VEH_AIRCRAFT: return AIVehicle::VT_AIR;
		default: NOT_REACHED();
	}
}

/* static */ bool AIEngine::IsWagon(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return false;
	if (GetVehicleType(engine_id) != AIVehicle::VT_RAIL) return false;

	return ::RailVehInfo(engine_id)->power == 0;
}

/* static */ bool AIEngine::CanRunOnRail(EngineID engine_id, AIRail::RailType track_rail_type)
{
	if (!IsValidEngine(engine_id)) return false;
	if (GetVehicleType(engine_id) != AIVehicle::VT_RAIL) return false;
	if (!AIRail::IsRailTypeAvailable(track_rail_type)) return false;

	return ::IsCompatibleRail((::RailType)::RailVehInfo(engine_id)->railtype, (::RailType)track_rail_type);
}

/* static */ bool AIEngine::HasPowerOnRail(EngineID engine_id, AIRail::RailType track_rail_type)
{
	if (!IsValidEngine(engine_id)) return false;
	if (GetVehicleType(engine_id) != AIVehicle::VT_RAIL) return false;
	if (!AIRail::IsRailTypeAvailable(track_rail_type)) return false;

	return ::HasPowerOnRail((::RailType)::RailVehInfo(engine_id)->railtype, (::RailType)track_rail_type);
}

/* static */ AIRoad::RoadType AIEngine::GetRoadType(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return AIRoad::ROADTYPE_INVALID;
	if (GetVehicleType(engine_id) != AIVehicle::VT_ROAD) return AIRoad::ROADTYPE_INVALID;

	return HasBit(::EngInfo(engine_id)->misc_flags, EF_ROAD_TRAM) ? AIRoad::ROADTYPE_TRAM : AIRoad::ROADTYPE_ROAD;
}

/* static */ AIRail::RailType AIEngine::GetRailType(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return AIRail::RAILTYPE_INVALID;
	if (GetVehicleType(engine_id) != AIVehicle::VT_RAIL) return AIRail::RAILTYPE_INVALID;

	return (AIRail::RailType)(uint)::RailVehInfo(engine_id)->railtype;
}

/* static */ bool AIEngine::IsArticulated(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return false;
	if (GetVehicleType(engine_id) != AIVehicle::VT_ROAD && GetVehicleType(engine_id) != AIVehicle::VT_RAIL) return false;

	return CountArticulatedParts(engine_id, true) != 0;
}

/* static */ AIAirport::PlaneType AIEngine::GetPlaneType(EngineID engine_id)
{
	if (!IsValidEngine(engine_id)) return AIAirport::PT_INVALID;
	if (GetVehicleType(engine_id) != AIVehicle::VT_AIR) return AIAirport::PT_INVALID;

	return (AIAirport::PlaneType)::AircraftVehInfo(engine_id)->subtype;
}
