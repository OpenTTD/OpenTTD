/* $Id$ */

/** @file ai_event_types.cpp Implementation of all EventTypes. */

#include "ai_event_types.hpp"
#include "../../command_type.h"
#include "../../strings_func.h"
#include "../../settings_type.h"
#include "../../rail.h"
#include "../../engine_base.h"
#include "../../articulated_vehicles.h"
#include "table/strings.h"

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
	const Engine *e = ::GetEngine(engine);
	if (!e->CanCarryCargo()) return CT_INVALID;
	return e->GetDefaultCargoType();
}

int32 AIEventEnginePreview::GetCapacity()
{
	const Engine *e = ::GetEngine(engine);
	switch (e->type) {
		case VEH_ROAD:
		case VEH_TRAIN: {
			uint16 *capacities = GetCapacityOfArticulatedParts(engine, e->type);
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

int32 AIEventEnginePreview::GetMaxSpeed()
{
	const Engine *e = ::GetEngine(engine);
	int32 max_speed = e->GetDisplayMaxSpeed(); // km-ish/h
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
