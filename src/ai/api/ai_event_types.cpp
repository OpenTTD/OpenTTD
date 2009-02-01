/* $Id$ */

/** @file ai_event_types.cpp Implementation of all EventTypes. */

#include "ai_event_types.hpp"
#include "../../strings_func.h"
#include "../../settings_type.h"
#include "../../aircraft.h"
#include "../../articulated_vehicles.h"
#include "table/strings.h"

bool AIEventVehicleCrashed::CloneCrashedVehicle(TileIndex depot)
{
	return false;
}

char *AIEventEnginePreview::GetName()
{
	static const int len = 64;
	char *engine_name = MallocT<char>(len);

	::SetDParam(0, engine);
	::GetString(engine_name, STR_ENGINE_NAME, &engine_name[len - 1]);
	return engine_name;
}

CargoID AIEventEnginePreview::GetCargoType()
{
	switch (::GetEngine(engine)->type) {
		case VEH_ROAD: {
			const RoadVehicleInfo *vi = ::RoadVehInfo(engine);
			return vi->cargo_type;
		} break;

		case VEH_TRAIN: {
			const RailVehicleInfo *vi = ::RailVehInfo(engine);
			return vi->cargo_type;
		} break;

		case VEH_SHIP: {
			const ShipVehicleInfo *vi = ::ShipVehInfo(engine);
			return vi->cargo_type;
		} break;

		case VEH_AIRCRAFT: {
			return CT_PASSENGERS;
		} break;

		default: NOT_REACHED();
	}
}

int32 AIEventEnginePreview::GetCapacity()
{
	switch (::GetEngine(engine)->type) {
		case VEH_ROAD:
		case VEH_TRAIN: {
			uint16 *capacities = GetCapacityOfArticulatedParts(engine, ::GetEngine(engine)->type);
			for (CargoID c = 0; c < NUM_CARGO; c++) {
				if (capacities[c] == 0) continue;
				return capacities[c];
			}
			return -1;
		} break;

		case VEH_SHIP: {
			const ShipVehicleInfo *vi = ::ShipVehInfo(engine);
			return vi->capacity;
		} break;

		case VEH_AIRCRAFT: {
			const AircraftVehicleInfo *vi = ::AircraftVehInfo(engine);
			return vi->passenger_capacity;
		} break;

		default: NOT_REACHED();
	}
}

int32 AIEventEnginePreview::GetMaxSpeed()
{
	const Engine *e = ::GetEngine(engine);
	int32 max_speed = e->GetDisplayMaxSpeed() * 16 / 10; // convert mph to km-ish/h
	if (e->type == VEH_AIRCRAFT) max_speed /= _settings_game.vehicle.plane_speed;
	return max_speed;
}

Money AIEventEnginePreview::GetPrice()
{
	return ::GetEngine(engine)->GetCost();
}

Money AIEventEnginePreview::GetRunningCost()
{
	return ::GetEngine(engine)->GetRunningCost();
}

AIVehicle::VehicleType AIEventEnginePreview::GetVehicleType()
{
	switch (::GetEngine(engine)->type) {
		case VEH_ROAD:     return AIVehicle::VT_ROAD;
		case VEH_TRAIN:    return AIVehicle::VT_RAIL;
		case VEH_SHIP:     return AIVehicle::VT_WATER;
		case VEH_AIRCRAFT: return AIVehicle::VT_AIR;
		default: NOT_REACHED();
	}
}

bool AIEventEnginePreview::AcceptPreview()
{
	return AIObject::DoCommand(0, engine, 0, CMD_WANT_ENGINE_PREVIEW);
}
