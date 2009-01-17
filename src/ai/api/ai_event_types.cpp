/* $Id$ */

/** @file ai_event_types.cpp Implementation of all EventTypes. */

#include "ai_event_types.hpp"
#include "../../openttd.h"
#include "../../core/alloc_func.hpp"
#include "../../strings_func.h"
#include "../../roadveh.h"
#include "../../train.h"
#include "../../ship.h"
#include "../../aircraft.h"
#include "../../settings_type.h"
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
	switch (::GetEngine(engine)->type) {
		case VEH_ROAD: {
			const RoadVehicleInfo *vi = ::RoadVehInfo(engine);
			/* Internal speeds are km/h * 2 */
			return vi->max_speed / 2;
		} break;

		case VEH_TRAIN: {
			const RailVehicleInfo *vi = ::RailVehInfo(engine);
			return vi->max_speed;
		} break;

		case VEH_SHIP: {
			const ShipVehicleInfo *vi = ::ShipVehInfo(engine);
			/* Internal speeds are km/h * 2 */
			return vi->max_speed / 2;
		} break;

		case VEH_AIRCRAFT: {
			const AircraftVehicleInfo *vi = ::AircraftVehInfo(engine);
			return vi->max_speed / _settings_game.vehicle.plane_speed;
		} break;

		default: NOT_REACHED();
	}
}

Money AIEventEnginePreview::GetPrice()
{
	switch (::GetEngine(engine)->type) {
		case VEH_ROAD: {
			const RoadVehicleInfo *vi = ::RoadVehInfo(engine);
			return (_price.roadveh_base >> 3) * vi->cost_factor >> 5;
		} break;

		case VEH_TRAIN: {
			const RailVehicleInfo *vi = ::RailVehInfo(engine);
			return (_price.build_railvehicle >> 3) * vi->cost_factor >> 5;
		} break;

		case VEH_SHIP: {
			const ShipVehicleInfo *vi = ::ShipVehInfo(engine);
			return (_price.ship_base >> 3) * vi->cost_factor >> 5;
		} break;

		case VEH_AIRCRAFT: {
			const AircraftVehicleInfo *vi = ::AircraftVehInfo(engine);
			return (_price.aircraft_base >> 3) * vi->cost_factor >> 5;
		} break;

		default: NOT_REACHED();
	}
}

Money AIEventEnginePreview::GetRunningCost()
{
	/* We need to create an instance in order to obtain GetRunningCost.
	 *  This means we temporary allocate a vehicle in the pool, but
	 *  there is no other way.. */
	Vehicle *vehicle;
	switch (::GetEngine(engine)->type) {
		case VEH_ROAD: {
			vehicle = new RoadVehicle();
		} break;

		case VEH_TRAIN: {
			vehicle = new Train();
		} break;

		case VEH_SHIP: {
			vehicle = new Ship();
		} break;

		case VEH_AIRCRAFT: {
			vehicle = new Aircraft();
		} break;

		default: NOT_REACHED();
	}

	vehicle->engine_type = engine;
	Money runningCost = vehicle->GetRunningCost();
	delete vehicle;
	return runningCost >> 8;
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
