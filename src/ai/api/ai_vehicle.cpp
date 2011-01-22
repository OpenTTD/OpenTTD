/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_vehicle.cpp Implementation of AIVehicle. */

#include "../../stdafx.h"
#include "ai_engine.hpp"
#include "ai_cargo.hpp"
#include "ai_gamesettings.hpp"
#include "ai_group.hpp"
#include "../ai_instance.hpp"
#include "../../company_func.h"
#include "../../string_func.h"
#include "../../strings_func.h"
#include "../../command_func.h"
#include "../../roadveh.h"
#include "../../train.h"
#include "../../vehicle_func.h"
#include "table/strings.h"

/* static */ bool AIVehicle::IsValidVehicle(VehicleID vehicle_id)
{
	const Vehicle *v = ::Vehicle::GetIfValid(vehicle_id);
	return v != NULL && v->owner == _current_company && (v->IsPrimaryVehicle() || (v->type == VEH_TRAIN && ::Train::From(v)->IsFreeWagon()));
}

/* static */ int32 AIVehicle::GetNumWagons(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return -1;

	int num = 1;

	const Train *v = ::Train::GetIfValid(vehicle_id);
	if (v != NULL) {
		while ((v = v->GetNextUnit()) != NULL) num++;
	}

	return num;
}

/* static */ int AIVehicle::GetLength(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return -1;

	const Vehicle *v = ::Vehicle::Get(vehicle_id);
	return v->IsGroundVehicle() ? v->GetGroundVehicleCache()->cached_total_length : -1;
}

/* static */ VehicleID AIVehicle::BuildVehicle(TileIndex depot, EngineID engine_id)
{
	EnforcePrecondition(VEHICLE_INVALID, AIEngine::IsBuildable(engine_id));

	::VehicleType type = ::Engine::Get(engine_id)->type;

	EnforcePreconditionCustomError(VEHICLE_INVALID, !AIGameSettings::IsDisabledVehicleType((AIVehicle::VehicleType)type), AIVehicle::ERR_VEHICLE_BUILD_DISABLED);

	if (!AIObject::DoCommand(depot, engine_id, 0, ::GetCmdBuildVeh(type), NULL, &AIInstance::DoCommandReturnVehicleID)) return VEHICLE_INVALID;

	/* In case of test-mode, we return VehicleID 0 */
	return 0;
}

/* static */ VehicleID AIVehicle::CloneVehicle(TileIndex depot, VehicleID vehicle_id, bool share_orders)
{
	EnforcePrecondition(false, IsValidVehicle(vehicle_id));

	if (!AIObject::DoCommand(depot, vehicle_id, share_orders, CMD_CLONE_VEHICLE, NULL, &AIInstance::DoCommandReturnVehicleID)) return VEHICLE_INVALID;

	/* In case of test-mode, we return VehicleID 0 */
	return 0;
}

/* static */ bool AIVehicle::_MoveWagonInternal(VehicleID source_vehicle_id, int source_wagon, bool move_attached_wagons, int dest_vehicle_id, int dest_wagon)
{
	EnforcePrecondition(false, IsValidVehicle(source_vehicle_id) && source_wagon < GetNumWagons(source_vehicle_id));
	EnforcePrecondition(false, dest_vehicle_id == -1 || (IsValidVehicle(dest_vehicle_id) && dest_wagon < GetNumWagons(dest_vehicle_id)));
	EnforcePrecondition(false, ::Vehicle::Get(source_vehicle_id)->type == VEH_TRAIN);
	EnforcePrecondition(false, dest_vehicle_id == -1 || ::Vehicle::Get(dest_vehicle_id)->type == VEH_TRAIN);

	const Train *v = ::Train::Get(source_vehicle_id);
	while (source_wagon-- > 0) v = v->GetNextUnit();
	const Train *w = NULL;
	if (dest_vehicle_id != -1) {
		w = ::Train::Get(dest_vehicle_id);
		while (dest_wagon-- > 0) w = w->GetNextUnit();
	}

	return AIObject::DoCommand(0, v->index | (move_attached_wagons ? 1 : 0) << 20, w == NULL ? ::INVALID_VEHICLE : w->index, CMD_MOVE_RAIL_VEHICLE);
}

/* static */ bool AIVehicle::MoveWagon(VehicleID source_vehicle_id, int source_wagon, int dest_vehicle_id, int dest_wagon)
{
	return _MoveWagonInternal(source_vehicle_id, source_wagon, false, dest_vehicle_id, dest_wagon);
}

/* static */ bool AIVehicle::MoveWagonChain(VehicleID source_vehicle_id, int source_wagon, int dest_vehicle_id, int dest_wagon)
{
	return _MoveWagonInternal(source_vehicle_id, source_wagon, true, dest_vehicle_id, dest_wagon);
}

/* static */ int AIVehicle::GetRefitCapacity(VehicleID vehicle_id, CargoID cargo)
{
	if (!IsValidVehicle(vehicle_id)) return -1;
	if (!AICargo::IsValidCargo(cargo)) return -1;

	CommandCost res = ::DoCommand(0, vehicle_id, cargo, DC_QUERY_COST, GetCmdRefitVeh(::Vehicle::Get(vehicle_id)));
	return res.Succeeded() ? _returned_refit_capacity : -1;
}

/* static */ bool AIVehicle::RefitVehicle(VehicleID vehicle_id, CargoID cargo)
{
	EnforcePrecondition(false, IsValidVehicle(vehicle_id) && AICargo::IsValidCargo(cargo));

	return AIObject::DoCommand(0, vehicle_id, cargo, GetCmdRefitVeh(::Vehicle::Get(vehicle_id)));
}


/* static */ bool AIVehicle::SellVehicle(VehicleID vehicle_id)
{
	EnforcePrecondition(false, IsValidVehicle(vehicle_id));

	const Vehicle *v = ::Vehicle::Get(vehicle_id);
	return AIObject::DoCommand(0, vehicle_id | (v->type == VEH_TRAIN ? 1 : 0) << 20, 0, GetCmdSellVeh(v));
}

/* static */ bool AIVehicle::_SellWagonInternal(VehicleID vehicle_id, int wagon, bool sell_attached_wagons)
{
	EnforcePrecondition(false, IsValidVehicle(vehicle_id) && wagon < GetNumWagons(vehicle_id));
	EnforcePrecondition(false, ::Vehicle::Get(vehicle_id)->type == VEH_TRAIN);

	const Train *v = ::Train::Get(vehicle_id);
	while (wagon-- > 0) v = v->GetNextUnit();

	return AIObject::DoCommand(0, v->index | (sell_attached_wagons ? 1 : 0) << 20, 0, CMD_SELL_VEHICLE);
}

/* static */ bool AIVehicle::SellWagon(VehicleID vehicle_id, int wagon)
{
	return _SellWagonInternal(vehicle_id, wagon, false);
}

/* static */ bool AIVehicle::SellWagonChain(VehicleID vehicle_id, int wagon)
{
	return _SellWagonInternal(vehicle_id, wagon, true);
}

/* static */ bool AIVehicle::SendVehicleToDepot(VehicleID vehicle_id)
{
	EnforcePrecondition(false, IsValidVehicle(vehicle_id));

	return AIObject::DoCommand(0, vehicle_id, 0, GetCmdSendToDepot(::Vehicle::Get(vehicle_id)));
}

/* static */ bool AIVehicle::SendVehicleToDepotForServicing(VehicleID vehicle_id)
{
	EnforcePrecondition(false, IsValidVehicle(vehicle_id));

	return AIObject::DoCommand(0, vehicle_id | DEPOT_SERVICE, 0, GetCmdSendToDepot(::Vehicle::Get(vehicle_id)));
}

/* static */ bool AIVehicle::IsInDepot(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return false;
	return ::Vehicle::Get(vehicle_id)->IsInDepot();
}

/* static */ bool AIVehicle::IsStoppedInDepot(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return false;
	return ::Vehicle::Get(vehicle_id)->IsStoppedInDepot();
}

/* static */ bool AIVehicle::StartStopVehicle(VehicleID vehicle_id)
{
	EnforcePrecondition(false, IsValidVehicle(vehicle_id));

	return AIObject::DoCommand(0, vehicle_id, 0, CMD_START_STOP_VEHICLE);
}

/* static */ bool AIVehicle::ReverseVehicle(VehicleID vehicle_id)
{
	EnforcePrecondition(false, IsValidVehicle(vehicle_id));
	EnforcePrecondition(false, ::Vehicle::Get(vehicle_id)->type == VEH_ROAD || ::Vehicle::Get(vehicle_id)->type == VEH_TRAIN);

	switch (::Vehicle::Get(vehicle_id)->type) {
		case VEH_ROAD: return AIObject::DoCommand(0, vehicle_id, 0, CMD_TURN_ROADVEH);
		case VEH_TRAIN: return AIObject::DoCommand(0, vehicle_id, 0, CMD_REVERSE_TRAIN_DIRECTION);
		default: NOT_REACHED();
	}
}

/* static */ bool AIVehicle::SetName(VehicleID vehicle_id, const char *name)
{
	EnforcePrecondition(false, IsValidVehicle(vehicle_id));
	EnforcePrecondition(false, !::StrEmpty(name));
	EnforcePreconditionCustomError(false, ::Utf8StringLength(name) < MAX_LENGTH_VEHICLE_NAME_CHARS, AIError::ERR_PRECONDITION_STRING_TOO_LONG);

	return AIObject::DoCommand(0, vehicle_id, 0, CMD_RENAME_VEHICLE, name);
}

/* static */ TileIndex AIVehicle::GetLocation(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return INVALID_TILE;

	const Vehicle *v = ::Vehicle::Get(vehicle_id);
	if (v->type == VEH_AIRCRAFT) {
		uint x = Clamp(v->x_pos / TILE_SIZE, 0, ::MapSizeX() - 2);
		uint y = Clamp(v->y_pos / TILE_SIZE, 0, ::MapSizeY() - 2);
		return ::TileXY(x, y);
	}

	return v->tile;
}

/* static */ EngineID AIVehicle::GetEngineType(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return INVALID_ENGINE;

	return ::Vehicle::Get(vehicle_id)->engine_type;
}

/* static */ EngineID AIVehicle::GetWagonEngineType(VehicleID vehicle_id, int wagon)
{
	if (!IsValidVehicle(vehicle_id)) return INVALID_ENGINE;
	if (wagon >= GetNumWagons(vehicle_id)) return INVALID_ENGINE;

	const Vehicle *v = ::Vehicle::Get(vehicle_id);
	if (v->type == VEH_TRAIN) {
		while (wagon-- > 0) v = ::Train::From(v)->GetNextUnit();
	}
	return v->engine_type;
}

/* static */ int32 AIVehicle::GetUnitNumber(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return -1;

	return ::Vehicle::Get(vehicle_id)->unitnumber;
}

/* static */ char *AIVehicle::GetName(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return NULL;

	static const int len = 64;
	char *vehicle_name = MallocT<char>(len);

	::SetDParam(0, vehicle_id);
	::GetString(vehicle_name, STR_VEHICLE_NAME, &vehicle_name[len - 1]);
	return vehicle_name;
}

/* static */ int32 AIVehicle::GetAge(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return -1;

	return ::Vehicle::Get(vehicle_id)->age;
}

/* static */ int32 AIVehicle::GetWagonAge(VehicleID vehicle_id, int wagon)
{
	if (!IsValidVehicle(vehicle_id)) return -1;
	if (wagon >= GetNumWagons(vehicle_id)) return -1;

	const Vehicle *v = ::Vehicle::Get(vehicle_id);
	if (v->type == VEH_TRAIN) {
		while (wagon-- > 0) v = ::Train::From(v)->GetNextUnit();
	}
	return v->age;
}

/* static */ int32 AIVehicle::GetMaxAge(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return -1;

	return ::Vehicle::Get(vehicle_id)->max_age;
}

/* static */ int32 AIVehicle::GetAgeLeft(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return -1;

	return ::Vehicle::Get(vehicle_id)->max_age - ::Vehicle::Get(vehicle_id)->age;
}

/* static */ int32 AIVehicle::GetCurrentSpeed(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return -1;

	return ::Vehicle::Get(vehicle_id)->GetDisplaySpeed(); // km-ish/h
}

/* static */ AIVehicle::VehicleState AIVehicle::GetState(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return AIVehicle::VS_INVALID;

	const Vehicle *v = ::Vehicle::Get(vehicle_id);
	byte vehstatus = v->vehstatus;

	if (vehstatus & ::VS_CRASHED) return AIVehicle::VS_CRASHED;
	if (v->breakdown_ctr != 0) return AIVehicle::VS_BROKEN;
	if (v->IsStoppedInDepot()) return AIVehicle::VS_IN_DEPOT;
	if (vehstatus & ::VS_STOPPED) return AIVehicle::VS_STOPPED;
	if (v->current_order.IsType(OT_LOADING)) return AIVehicle::VS_AT_STATION;
	return AIVehicle::VS_RUNNING;
}

/* static */ Money AIVehicle::GetRunningCost(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return -1;

	return ::Vehicle::Get(vehicle_id)->GetRunningCost() >> 8;
}

/* static */ Money AIVehicle::GetProfitThisYear(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return -1;

	return ::Vehicle::Get(vehicle_id)->GetDisplayProfitThisYear();
}

/* static */ Money AIVehicle::GetProfitLastYear(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return -1;

	return ::Vehicle::Get(vehicle_id)->GetDisplayProfitLastYear();
}

/* static */ Money AIVehicle::GetCurrentValue(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return -1;

	return ::Vehicle::Get(vehicle_id)->value;
}

/* static */ AIVehicle::VehicleType AIVehicle::GetVehicleType(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return VT_INVALID;

	switch (::Vehicle::Get(vehicle_id)->type) {
		case VEH_ROAD:     return VT_ROAD;
		case VEH_TRAIN:    return VT_RAIL;
		case VEH_SHIP:     return VT_WATER;
		case VEH_AIRCRAFT: return VT_AIR;
		default:           return VT_INVALID;
	}
}

/* static */ AIRoad::RoadType AIVehicle::GetRoadType(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return AIRoad::ROADTYPE_INVALID;
	if (GetVehicleType(vehicle_id) != VT_ROAD) return AIRoad::ROADTYPE_INVALID;

	return (AIRoad::RoadType)(::RoadVehicle::Get(vehicle_id))->roadtype;
}

/* static */ int32 AIVehicle::GetCapacity(VehicleID vehicle_id, CargoID cargo)
{
	if (!IsValidVehicle(vehicle_id)) return -1;
	if (!AICargo::IsValidCargo(cargo)) return -1;

	uint32 amount = 0;
	for (const Vehicle *v = ::Vehicle::Get(vehicle_id); v != NULL; v = v->Next()) {
		if (v->cargo_type == cargo) amount += v->cargo_cap;
	}

	return amount;
}

/* static */ int32 AIVehicle::GetCargoLoad(VehicleID vehicle_id, CargoID cargo)
{
	if (!IsValidVehicle(vehicle_id)) return -1;
	if (!AICargo::IsValidCargo(cargo)) return -1;

	uint32 amount = 0;
	for (const Vehicle *v = ::Vehicle::Get(vehicle_id); v != NULL; v = v->Next()) {
		if (v->cargo_type == cargo) amount += v->cargo.Count();
	}

	return amount;
}

/* static */ GroupID AIVehicle::GetGroupID(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return AIGroup::GROUP_INVALID;

	return ::Vehicle::Get(vehicle_id)->group_id;
}

/* static */ bool AIVehicle::IsArticulated(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return false;
	if (GetVehicleType(vehicle_id) != VT_ROAD && GetVehicleType(vehicle_id) != VT_RAIL) return false;

	const Vehicle *v = ::Vehicle::Get(vehicle_id);
	switch (v->type) {
		case VEH_ROAD: return ::RoadVehicle::From(v)->HasArticulatedPart();
		case VEH_TRAIN: return ::Train::From(v)->HasArticulatedPart();
		default: NOT_REACHED();
	}
}

/* static */ bool AIVehicle::HasSharedOrders(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return false;

	Vehicle *v = ::Vehicle::Get(vehicle_id);
	return v->orders.list != NULL && v->orders.list->GetNumVehicles() > 1;
}

/* static */ int AIVehicle::GetReliability(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return -1;

	const Vehicle *v = ::Vehicle::Get(vehicle_id);
	return ::ToPercent16(v->reliability);
}
