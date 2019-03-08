/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_vehicle.cpp Implementation of ScriptVehicle. */

#include "../../stdafx.h"
#include "script_engine.hpp"
#include "script_cargo.hpp"
#include "script_gamesettings.hpp"
#include "script_group.hpp"
#include "../script_instance.hpp"
#include "../../string_func.h"
#include "../../strings_func.h"
#include "../../command_func.h"
#include "../../roadveh.h"
#include "../../train.h"
#include "../../vehicle_func.h"
#include "../../aircraft.h"
#include "table/strings.h"

#include "../../safeguards.h"

/* static */ bool ScriptVehicle::IsValidVehicle(VehicleID vehicle_id)
{
	const Vehicle *v = ::Vehicle::GetIfValid(vehicle_id);
	return v != NULL && (v->owner == ScriptObject::GetCompany() || ScriptObject::GetCompany() == OWNER_DEITY) && (v->IsPrimaryVehicle() || (v->type == VEH_TRAIN && ::Train::From(v)->IsFreeWagon()));
}

/* static */ ScriptCompany::CompanyID ScriptVehicle::GetOwner(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return ScriptCompany::COMPANY_INVALID;

	return static_cast<ScriptCompany::CompanyID>((int)::Vehicle::Get(vehicle_id)->owner);
}

/* static */ int32 ScriptVehicle::GetNumWagons(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return -1;

	int num = 1;

	const Train *v = ::Train::GetIfValid(vehicle_id);
	if (v != NULL) {
		while ((v = v->GetNextUnit()) != NULL) num++;
	}

	return num;
}

/* static */ int ScriptVehicle::GetLength(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return -1;

	const Vehicle *v = ::Vehicle::Get(vehicle_id);
	return v->IsGroundVehicle() ? v->GetGroundVehicleCache()->cached_total_length : -1;
}

/* static */ VehicleID ScriptVehicle::_BuildVehicleInternal(TileIndex depot, EngineID engine_id, CargoID cargo)
{
	EnforcePrecondition(VEHICLE_INVALID, ScriptObject::GetCompany() != OWNER_DEITY);
	EnforcePrecondition(VEHICLE_INVALID, ScriptEngine::IsBuildable(engine_id));
	EnforcePrecondition(VEHICLE_INVALID, cargo == CT_INVALID || ScriptCargo::IsValidCargo(cargo));

	::VehicleType type = ::Engine::Get(engine_id)->type;

	EnforcePreconditionCustomError(VEHICLE_INVALID, !ScriptGameSettings::IsDisabledVehicleType((ScriptVehicle::VehicleType)type), ScriptVehicle::ERR_VEHICLE_BUILD_DISABLED);

	if (!ScriptObject::DoCommand(depot, engine_id | (cargo << 24), 0, ::GetCmdBuildVeh(type), NULL, &ScriptInstance::DoCommandReturnVehicleID)) return VEHICLE_INVALID;

	/* In case of test-mode, we return VehicleID 0 */
	return 0;
}

/* static */ VehicleID ScriptVehicle::BuildVehicle(TileIndex depot, EngineID engine_id)
{
	return _BuildVehicleInternal(depot, engine_id, CT_INVALID);
}

/* static */ VehicleID ScriptVehicle::BuildVehicleWithRefit(TileIndex depot, EngineID engine_id, CargoID cargo)
{
	EnforcePrecondition(VEHICLE_INVALID, ScriptCargo::IsValidCargo(cargo));
	return _BuildVehicleInternal(depot, engine_id, cargo);
}

/* static */ int ScriptVehicle::GetBuildWithRefitCapacity(TileIndex depot, EngineID engine_id, CargoID cargo)
{
	if (!ScriptEngine::IsBuildable(engine_id)) return -1;
	if (!ScriptCargo::IsValidCargo(cargo)) return -1;

	::VehicleType type = ::Engine::Get(engine_id)->type;

	CommandCost res = ::DoCommand(depot, engine_id | (cargo << 24), 0, DC_QUERY_COST, ::GetCmdBuildVeh(type));
	return res.Succeeded() ? _returned_refit_capacity : -1;
}

/* static */ VehicleID ScriptVehicle::CloneVehicle(TileIndex depot, VehicleID vehicle_id, bool share_orders)
{
	EnforcePrecondition(false, ScriptObject::GetCompany() != OWNER_DEITY);
	EnforcePrecondition(false, IsValidVehicle(vehicle_id));

	if (!ScriptObject::DoCommand(depot, vehicle_id, share_orders, CMD_CLONE_VEHICLE, NULL, &ScriptInstance::DoCommandReturnVehicleID)) return VEHICLE_INVALID;

	/* In case of test-mode, we return VehicleID 0 */
	return 0;
}

/* static */ bool ScriptVehicle::_MoveWagonInternal(VehicleID source_vehicle_id, int source_wagon, bool move_attached_wagons, int dest_vehicle_id, int dest_wagon)
{
	EnforcePrecondition(false, ScriptObject::GetCompany() != OWNER_DEITY);
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

	return ScriptObject::DoCommand(0, v->index | (move_attached_wagons ? 1 : 0) << 20, w == NULL ? ::INVALID_VEHICLE : w->index, CMD_MOVE_RAIL_VEHICLE);
}

/* static */ bool ScriptVehicle::MoveWagon(VehicleID source_vehicle_id, int source_wagon, int dest_vehicle_id, int dest_wagon)
{
	return _MoveWagonInternal(source_vehicle_id, source_wagon, false, dest_vehicle_id, dest_wagon);
}

/* static */ bool ScriptVehicle::MoveWagonChain(VehicleID source_vehicle_id, int source_wagon, int dest_vehicle_id, int dest_wagon)
{
	return _MoveWagonInternal(source_vehicle_id, source_wagon, true, dest_vehicle_id, dest_wagon);
}

/* static */ int ScriptVehicle::GetRefitCapacity(VehicleID vehicle_id, CargoID cargo)
{
	if (!IsValidVehicle(vehicle_id)) return -1;
	if (!ScriptCargo::IsValidCargo(cargo)) return -1;

	CommandCost res = ::DoCommand(0, vehicle_id, cargo, DC_QUERY_COST, GetCmdRefitVeh(::Vehicle::Get(vehicle_id)));
	return res.Succeeded() ? _returned_refit_capacity : -1;
}

/* static */ bool ScriptVehicle::RefitVehicle(VehicleID vehicle_id, CargoID cargo)
{
	EnforcePrecondition(false, ScriptObject::GetCompany() != OWNER_DEITY);
	EnforcePrecondition(false, IsValidVehicle(vehicle_id) && ScriptCargo::IsValidCargo(cargo));

	return ScriptObject::DoCommand(0, vehicle_id, cargo, GetCmdRefitVeh(::Vehicle::Get(vehicle_id)));
}


/* static */ bool ScriptVehicle::SellVehicle(VehicleID vehicle_id)
{
	EnforcePrecondition(false, ScriptObject::GetCompany() != OWNER_DEITY);
	EnforcePrecondition(false, IsValidVehicle(vehicle_id));

	const Vehicle *v = ::Vehicle::Get(vehicle_id);
	return ScriptObject::DoCommand(0, vehicle_id | (v->type == VEH_TRAIN ? 1 : 0) << 20, 0, GetCmdSellVeh(v));
}

/* static */ bool ScriptVehicle::_SellWagonInternal(VehicleID vehicle_id, int wagon, bool sell_attached_wagons)
{
	EnforcePrecondition(false, ScriptObject::GetCompany() != OWNER_DEITY);
	EnforcePrecondition(false, IsValidVehicle(vehicle_id) && wagon < GetNumWagons(vehicle_id));
	EnforcePrecondition(false, ::Vehicle::Get(vehicle_id)->type == VEH_TRAIN);

	const Train *v = ::Train::Get(vehicle_id);
	while (wagon-- > 0) v = v->GetNextUnit();

	return ScriptObject::DoCommand(0, v->index | (sell_attached_wagons ? 1 : 0) << 20, 0, CMD_SELL_VEHICLE);
}

/* static */ bool ScriptVehicle::SellWagon(VehicleID vehicle_id, int wagon)
{
	return _SellWagonInternal(vehicle_id, wagon, false);
}

/* static */ bool ScriptVehicle::SellWagonChain(VehicleID vehicle_id, int wagon)
{
	return _SellWagonInternal(vehicle_id, wagon, true);
}

/* static */ bool ScriptVehicle::SendVehicleToDepot(VehicleID vehicle_id)
{
	EnforcePrecondition(false, ScriptObject::GetCompany() != OWNER_DEITY);
	EnforcePrecondition(false, IsValidVehicle(vehicle_id));

	return ScriptObject::DoCommand(0, vehicle_id, 0, GetCmdSendToDepot(::Vehicle::Get(vehicle_id)));
}

/* static */ bool ScriptVehicle::SendVehicleToDepotForServicing(VehicleID vehicle_id)
{
	EnforcePrecondition(false, ScriptObject::GetCompany() != OWNER_DEITY);
	EnforcePrecondition(false, IsValidVehicle(vehicle_id));

	return ScriptObject::DoCommand(0, vehicle_id | DEPOT_SERVICE, 0, GetCmdSendToDepot(::Vehicle::Get(vehicle_id)));
}

/* static */ bool ScriptVehicle::IsInDepot(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return false;
	return ::Vehicle::Get(vehicle_id)->IsChainInDepot();
}

/* static */ bool ScriptVehicle::IsStoppedInDepot(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return false;
	return ::Vehicle::Get(vehicle_id)->IsStoppedInDepot();
}

/* static */ bool ScriptVehicle::StartStopVehicle(VehicleID vehicle_id)
{
	EnforcePrecondition(false, ScriptObject::GetCompany() != OWNER_DEITY);
	EnforcePrecondition(false, IsValidVehicle(vehicle_id));

	return ScriptObject::DoCommand(0, vehicle_id, 0, CMD_START_STOP_VEHICLE);
}

/* static */ bool ScriptVehicle::ReverseVehicle(VehicleID vehicle_id)
{
	EnforcePrecondition(false, ScriptObject::GetCompany() != OWNER_DEITY);
	EnforcePrecondition(false, IsValidVehicle(vehicle_id));
	EnforcePrecondition(false, ::Vehicle::Get(vehicle_id)->type == VEH_ROAD || ::Vehicle::Get(vehicle_id)->type == VEH_TRAIN);

	switch (::Vehicle::Get(vehicle_id)->type) {
		case VEH_ROAD: return ScriptObject::DoCommand(0, vehicle_id, 0, CMD_TURN_ROADVEH);
		case VEH_TRAIN: return ScriptObject::DoCommand(0, vehicle_id, 0, CMD_REVERSE_TRAIN_DIRECTION);
		default: NOT_REACHED();
	}
}

/* static */ bool ScriptVehicle::SetName(VehicleID vehicle_id, Text *name)
{
	CCountedPtr<Text> counter(name);

	EnforcePrecondition(false, ScriptObject::GetCompany() != OWNER_DEITY);
	EnforcePrecondition(false, IsValidVehicle(vehicle_id));
	EnforcePrecondition(false, name != NULL);
	const char *text = name->GetDecodedText();
	EnforcePreconditionEncodedText(false, text);
	EnforcePreconditionCustomError(false, ::Utf8StringLength(text) < MAX_LENGTH_VEHICLE_NAME_CHARS, ScriptError::ERR_PRECONDITION_STRING_TOO_LONG);

	return ScriptObject::DoCommand(0, vehicle_id, 0, CMD_RENAME_VEHICLE, text);
}

/* static */ TileIndex ScriptVehicle::GetLocation(VehicleID vehicle_id)
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

/* static */ EngineID ScriptVehicle::GetEngineType(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return INVALID_ENGINE;

	return ::Vehicle::Get(vehicle_id)->engine_type;
}

/* static */ EngineID ScriptVehicle::GetWagonEngineType(VehicleID vehicle_id, int wagon)
{
	if (!IsValidVehicle(vehicle_id)) return INVALID_ENGINE;
	if (wagon >= GetNumWagons(vehicle_id)) return INVALID_ENGINE;

	const Vehicle *v = ::Vehicle::Get(vehicle_id);
	if (v->type == VEH_TRAIN) {
		while (wagon-- > 0) v = ::Train::From(v)->GetNextUnit();
	}
	return v->engine_type;
}

/* static */ int32 ScriptVehicle::GetUnitNumber(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return -1;

	return ::Vehicle::Get(vehicle_id)->unitnumber;
}

/* static */ char *ScriptVehicle::GetName(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return NULL;

	::SetDParam(0, vehicle_id);
	return GetString(STR_VEHICLE_NAME);
}

/* static */ int32 ScriptVehicle::GetAge(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return -1;

	return ::Vehicle::Get(vehicle_id)->age;
}

/* static */ int32 ScriptVehicle::GetWagonAge(VehicleID vehicle_id, int wagon)
{
	if (!IsValidVehicle(vehicle_id)) return -1;
	if (wagon >= GetNumWagons(vehicle_id)) return -1;

	const Vehicle *v = ::Vehicle::Get(vehicle_id);
	if (v->type == VEH_TRAIN) {
		while (wagon-- > 0) v = ::Train::From(v)->GetNextUnit();
	}
	return v->age;
}

/* static */ int32 ScriptVehicle::GetMaxAge(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return -1;

	return ::Vehicle::Get(vehicle_id)->max_age;
}

/* static */ int32 ScriptVehicle::GetAgeLeft(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return -1;

	return ::Vehicle::Get(vehicle_id)->max_age - ::Vehicle::Get(vehicle_id)->age;
}

/* static */ int32 ScriptVehicle::GetCurrentSpeed(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return -1;

	const ::Vehicle *v = ::Vehicle::Get(vehicle_id);
	return (v->vehstatus & (::VS_STOPPED | ::VS_CRASHED)) == 0 ? v->GetDisplaySpeed() : 0; // km-ish/h
}

/* static */ ScriptVehicle::VehicleState ScriptVehicle::GetState(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return ScriptVehicle::VS_INVALID;

	const Vehicle *v = ::Vehicle::Get(vehicle_id);
	byte vehstatus = v->vehstatus;

	if (vehstatus & ::VS_CRASHED) return ScriptVehicle::VS_CRASHED;
	if (v->breakdown_ctr != 0) return ScriptVehicle::VS_BROKEN;
	if (v->IsStoppedInDepot()) return ScriptVehicle::VS_IN_DEPOT;
	if (vehstatus & ::VS_STOPPED) return ScriptVehicle::VS_STOPPED;
	if (v->current_order.IsType(OT_LOADING)) return ScriptVehicle::VS_AT_STATION;
	return ScriptVehicle::VS_RUNNING;
}

/* static */ Money ScriptVehicle::GetRunningCost(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return -1;

	return ::Vehicle::Get(vehicle_id)->GetRunningCost() >> 8;
}

/* static */ Money ScriptVehicle::GetProfitThisYear(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return -1;

	return ::Vehicle::Get(vehicle_id)->GetDisplayProfitThisYear();
}

/* static */ Money ScriptVehicle::GetProfitLastYear(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return -1;

	return ::Vehicle::Get(vehicle_id)->GetDisplayProfitLastYear();
}

/* static */ Money ScriptVehicle::GetCurrentValue(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return -1;

	return ::Vehicle::Get(vehicle_id)->value;
}

/* static */ ScriptVehicle::VehicleType ScriptVehicle::GetVehicleType(VehicleID vehicle_id)
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

/* static */ ScriptRoad::RoadType ScriptVehicle::GetRoadType(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return ScriptRoad::ROADTYPE_INVALID;
	if (GetVehicleType(vehicle_id) != VT_ROAD) return ScriptRoad::ROADTYPE_INVALID;

	return (ScriptRoad::RoadType)(::RoadVehicle::Get(vehicle_id))->roadtype;
}

/* static */ int32 ScriptVehicle::GetCapacity(VehicleID vehicle_id, CargoID cargo)
{
	if (!IsValidVehicle(vehicle_id)) return -1;
	if (!ScriptCargo::IsValidCargo(cargo)) return -1;

	uint32 amount = 0;
	for (const Vehicle *v = ::Vehicle::Get(vehicle_id); v != NULL; v = v->Next()) {
		if (v->cargo_type == cargo) amount += v->cargo_cap;
	}

	return amount;
}

/* static */ int32 ScriptVehicle::GetCargoLoad(VehicleID vehicle_id, CargoID cargo)
{
	if (!IsValidVehicle(vehicle_id)) return -1;
	if (!ScriptCargo::IsValidCargo(cargo)) return -1;

	uint32 amount = 0;
	for (const Vehicle *v = ::Vehicle::Get(vehicle_id); v != NULL; v = v->Next()) {
		if (v->cargo_type == cargo) amount += v->cargo.StoredCount();
	}

	return amount;
}

/* static */ GroupID ScriptVehicle::GetGroupID(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return ScriptGroup::GROUP_INVALID;

	return ::Vehicle::Get(vehicle_id)->group_id;
}

/* static */ bool ScriptVehicle::IsArticulated(VehicleID vehicle_id)
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

/* static */ bool ScriptVehicle::HasSharedOrders(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return false;

	Vehicle *v = ::Vehicle::Get(vehicle_id);
	return v->orders.list != NULL && v->orders.list->GetNumVehicles() > 1;
}

/* static */ int ScriptVehicle::GetReliability(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return -1;

	const Vehicle *v = ::Vehicle::Get(vehicle_id);
	return ::ToPercent16(v->reliability);
}

/* static */ uint ScriptVehicle::GetMaximumOrderDistance(VehicleID vehicle_id)
{
	if (!IsValidVehicle(vehicle_id)) return 0;

	const ::Vehicle *v = ::Vehicle::Get(vehicle_id);
	switch (v->type) {
		case VEH_SHIP:
			return _settings_game.pf.pathfinder_for_ships != VPF_NPF ? 129 : 0;

		case VEH_AIRCRAFT:
			return ::Aircraft::From(v)->acache.cached_max_range_sqr;

		default:
			return 0;
	}
}
