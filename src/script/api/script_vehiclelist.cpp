/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file script_vehiclelist.cpp Implementation of ScriptVehicleList and friends. */

#include "../../stdafx.h"
#include "script_vehiclelist.hpp"
#include "script_group.hpp"
#include "script_map.hpp"
#include "script_station.hpp"
#include "script_waypoint.hpp"
#include "../../depot_map.h"
#include "../../vehicle_base.h"
#include "../../vehiclelist_func.h"
#include "../../train.h"

#include "../../safeguards.h"

ScriptVehicleList::ScriptVehicleList(HSQUIRRELVM vm)
{
	EnforceDeityOrCompanyModeValid_Void();

	bool is_deity = ScriptCompanyMode::IsDeity();
	::CompanyID owner = ScriptObject::GetCompany();

	ScriptList::FillList<Vehicle>(vm, this,
		[is_deity, owner](const Vehicle *v) {
			return (is_deity || v->owner == owner) && (v->IsPrimaryVehicle() || (v->type == VEH_TRAIN && ::Train::From(v)->IsFreeWagon()));
		}
	);
}

ScriptVehicleList_Station::ScriptVehicleList_Station(HSQUIRRELVM vm)
{
	EnforceDeityOrCompanyModeValid_Void();

	int nparam = sq_gettop(vm) - 1;

	if (nparam < 1 || nparam > 2) throw sq_throwerror(vm, "wrong number of parameters");

	SQInteger sqstationid;
	if (SQ_FAILED(sq_getinteger(vm, 2, &sqstationid))) {
		throw sq_throwerror(vm, "parameter 1 must be an integer");
	}
	StationID station_id = static_cast<StationID>(sqstationid);
	if (!ScriptBaseStation::IsValidBaseStation(station_id)) return;

	bool is_deity = ScriptCompanyMode::IsDeity();
	::CompanyID owner = ScriptObject::GetCompany();
	::VehicleType type = VEH_INVALID;

	if (nparam == 2) {
		SQInteger sqtype;
		if (SQ_FAILED(sq_getinteger(vm, 3, &sqtype))) {
			throw sq_throwerror(vm, "parameter 2 must be an integer");
		}
		if (sqtype < ScriptVehicle::VT_RAIL || sqtype > ScriptVehicle::VT_AIR) return;
		type = static_cast<::VehicleType>(sqtype);
	}

	FindVehiclesWithOrder(
		[is_deity, owner, type](const Vehicle *v) { return (is_deity || v->owner == owner) && (type == VEH_INVALID || v->type == type); },
		[station_id](const Order *order) { return (order->IsType(OT_GOTO_STATION) || order->IsType(OT_GOTO_WAYPOINT)) && order->GetDestination() == station_id; },
		[this](const Vehicle *v) { this->AddItem(v->index.base()); }
	);
}

ScriptVehicleList_Waypoint::ScriptVehicleList_Waypoint(StationID waypoint_id)
{
	EnforceDeityOrCompanyModeValid_Void();
	if (!ScriptWaypoint::IsValidWaypoint(waypoint_id)) return;

	bool is_deity = ScriptCompanyMode::IsDeity();
	::CompanyID owner = ScriptObject::GetCompany();

	FindVehiclesWithOrder(
		[is_deity, owner](const Vehicle *v) { return is_deity || v->owner == owner; },
		[waypoint_id](const Order *order) { return order->IsType(OT_GOTO_WAYPOINT) && order->GetDestination() == waypoint_id; },
		[this](const Vehicle *v) { this->AddItem(v->index.base()); }
	);
}

ScriptVehicleList_Depot::ScriptVehicleList_Depot(TileIndex tile)
{
	EnforceDeityOrCompanyModeValid_Void();
	if (!ScriptMap::IsValidTile(tile)) return;

	DestinationID dest;
	VehicleType type;

	switch (GetTileType(tile)) {
		case MP_STATION: // Aircraft
			if (!IsAirport(tile)) return;
			type = VEH_AIRCRAFT;
			dest = GetStationIndex(tile);
			break;

		case MP_RAILWAY:
			if (!IsRailDepot(tile)) return;
			type = VEH_TRAIN;
			dest = GetDepotIndex(tile);
			break;

		case MP_ROAD:
			if (!IsRoadDepot(tile)) return;
			type = VEH_ROAD;
			dest = GetDepotIndex(tile);
			break;

		case MP_WATER:
			if (!IsShipDepot(tile)) return;
			type = VEH_SHIP;
			dest = GetDepotIndex(tile);
			break;

		default: // No depot
			return;
	}

	bool is_deity = ScriptCompanyMode::IsDeity();
	::CompanyID owner = ScriptObject::GetCompany();

	FindVehiclesWithOrder(
		[is_deity, owner, type](const Vehicle *v) { return (is_deity || v->owner == owner) && v->type == type; },
		[dest](const Order *order) { return order->IsType(OT_GOTO_DEPOT) && order->GetDestination() == dest; },
		[this](const Vehicle *v) { this->AddItem(v->index.base()); }
	);
}

ScriptVehicleList_SharedOrders::ScriptVehicleList_SharedOrders(VehicleID vehicle_id)
{
	if (!ScriptVehicle::IsPrimaryVehicle(vehicle_id)) return;

	for (const Vehicle *v = Vehicle::Get(vehicle_id)->FirstShared(); v != nullptr; v = v->NextShared()) {
		this->AddItem(v->index.base());
	}
}

ScriptVehicleList_Group::ScriptVehicleList_Group(GroupID group_id)
{
	EnforceCompanyModeValid_Void();
	if (!ScriptGroup::IsValidGroup(group_id)) return;

	::CompanyID owner = ScriptObject::GetCompany();

	ScriptList::FillList<Vehicle>(this,
		[owner](const Vehicle *v) { return v->owner == owner && v->IsPrimaryVehicle(); },
		[group_id](const Vehicle *v) { return v->group_id == group_id; }
	);
}

ScriptVehicleList_DefaultGroup::ScriptVehicleList_DefaultGroup(ScriptVehicle::VehicleType vehicle_type)
{
	EnforceCompanyModeValid_Void();
	if (vehicle_type < ScriptVehicle::VT_RAIL || vehicle_type > ScriptVehicle::VT_AIR) return;

	::CompanyID owner = ScriptObject::GetCompany();

	ScriptList::FillList<Vehicle>(this,
		[owner](const Vehicle *v) { return v->owner == owner && v->IsPrimaryVehicle(); },
		[vehicle_type](const Vehicle *v) { return v->type == (::VehicleType)vehicle_type && v->group_id == ScriptGroup::GROUP_DEFAULT; }
	);
}
