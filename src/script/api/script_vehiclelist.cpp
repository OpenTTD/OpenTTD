/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_vehiclelist.cpp Implementation of ScriptVehicleList and friends. */

#include "../../stdafx.h"
#include "script_vehiclelist.hpp"
#include "script_group.hpp"
#include "script_map.hpp"
#include "script_station.hpp"
#include "../../depot_map.h"
#include "../../vehicle_base.h"
#include "../../train.h"
#include "../../core/backup_type.hpp"
#include <../squirrel/sqvm.h>

#include "../../safeguards.h"

ScriptVehicleList::ScriptVehicleList(HSQUIRRELVM vm)
{
	EnforceDeityOrCompanyModeValid_Void();

	int nparam = sq_gettop(vm) - 1;
	if (nparam >= 1) {
		/* Make sure the filter function is really a function, and not any
		 * other type. It's parameter 2 for us, but for the user it's the
		 * first parameter they give. */
		SQObjectType valuator_type = sq_gettype(vm, 2);
		if (valuator_type != OT_CLOSURE && valuator_type != OT_NATIVECLOSURE) {
			throw sq_throwerror(vm, "parameter 1 has an invalid type (expected function)");
		}

		/* Push the function to call */
		sq_push(vm, 2);
	}

	/* Don't allow docommand from a Valuator, as we can't resume in
	 * mid C++-code. */
	bool backup_allow = ScriptObject::GetAllowDoCommand();
	ScriptObject::SetAllowDoCommand(false);

	/* Limit the total number of ops that can be consumed by a filter operation, if a filter function is present */
	SQInteger new_ops_error_threshold = vm->_ops_till_suspend_error_threshold;
	if (nparam >= 1 && vm->_ops_till_suspend_error_threshold == INT64_MIN) {
		new_ops_error_threshold = vm->_ops_till_suspend - MAX_VALUATE_OPS;
		vm->_ops_till_suspend_error_label = "vehicle filter function";
	}
	AutoRestoreBackup ops_error_threshold_backup(vm->_ops_till_suspend_error_threshold, new_ops_error_threshold);

	bool is_deity = ScriptCompanyMode::IsDeity();
	CompanyID owner = ScriptObject::GetCompany();
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (v->owner != owner && !is_deity) continue;
		if (!v->IsPrimaryVehicle() && !(v->type == VEH_TRAIN && ::Train::From(v)->IsFreeWagon())) continue;

		if (nparam < 1) {
			/* No filter, just add the item. */
			this->AddItem(v->index);
			continue;
		}

		/* Push the root table as instance object, this is what squirrel does for meta-functions. */
		sq_pushroottable(vm);
		/* Push all arguments for the valuator function. */
		sq_pushinteger(vm, v->index);
		for (int i = 0; i < nparam - 1; i++) {
			sq_push(vm, i + 3);
		}

		/* Call the function. Squirrel pops all parameters and pushes the return value. */
		if (SQ_FAILED(sq_call(vm, nparam + 1, SQTrue, SQTrue))) {
			ScriptObject::SetAllowDoCommand(backup_allow);
			throw sq_throwerror(vm, "failed to run filter");
		}

		/* Retrieve the return value */
		switch (sq_gettype(vm, -1)) {
			case OT_BOOL: {
				SQBool add;
				sq_getbool(vm, -1, &add);
				if (add) this->AddItem(v->index);
				break;
			}

			default: {
				ScriptObject::SetAllowDoCommand(backup_allow);
				throw sq_throwerror(vm, "return value of filter is not valid (not bool)");
			}
		}

		/* Pop the return value. */
		sq_poptop(vm);
	}

	if (nparam >= 1) {
		/* Pop the filter function */
		sq_poptop(vm);
	}

	ScriptObject::SetAllowDoCommand(backup_allow);
}

ScriptVehicleList_Station::ScriptVehicleList_Station(StationID station_id)
{
	EnforceDeityOrCompanyModeValid_Void();
	if (!ScriptBaseStation::IsValidBaseStation(station_id)) return;

	bool is_deity = ScriptCompanyMode::IsDeity();
	CompanyID owner = ScriptObject::GetCompany();
	for (const Vehicle *v : Vehicle::Iterate()) {
		if ((v->owner == owner || is_deity) && v->IsPrimaryVehicle()) {
			for (const Order *order : v->Orders()) {
				if ((order->IsType(OT_GOTO_STATION) || order->IsType(OT_GOTO_WAYPOINT)) && order->GetDestination() == station_id) {
					this->AddItem(v->index);
					break;
				}
			}
		}
	}
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
	CompanyID owner = ScriptObject::GetCompany();
	for (const Vehicle *v : Vehicle::Iterate()) {
		if ((v->owner == owner || is_deity) && v->IsPrimaryVehicle() && v->type == type) {
			for (const Order *order : v->Orders()) {
				if (order->IsType(OT_GOTO_DEPOT) && order->GetDestination() == dest) {
					this->AddItem(v->index);
					break;
				}
			}
		}
	}
}

ScriptVehicleList_SharedOrders::ScriptVehicleList_SharedOrders(VehicleID vehicle_id)
{
	if (!ScriptVehicle::IsPrimaryVehicle(vehicle_id)) return;

	for (const Vehicle *v = Vehicle::Get(vehicle_id)->FirstShared(); v != nullptr; v = v->NextShared()) {
		this->AddItem(v->index);
	}
}

ScriptVehicleList_Group::ScriptVehicleList_Group(GroupID group_id)
{
	EnforceCompanyModeValid_Void();
	if (!ScriptGroup::IsValidGroup((ScriptGroup::GroupID)group_id)) return;

	CompanyID owner = ScriptObject::GetCompany();
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (v->owner == owner && v->IsPrimaryVehicle()) {
			if (v->group_id == group_id) this->AddItem(v->index);
		}
	}
}

ScriptVehicleList_DefaultGroup::ScriptVehicleList_DefaultGroup(ScriptVehicle::VehicleType vehicle_type)
{
	EnforceCompanyModeValid_Void();
	if (vehicle_type < ScriptVehicle::VT_RAIL || vehicle_type > ScriptVehicle::VT_AIR) return;

	CompanyID owner = ScriptObject::GetCompany();
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (v->owner == owner && v->IsPrimaryVehicle()) {
			if (v->type == (::VehicleType)vehicle_type && v->group_id == ScriptGroup::GROUP_DEFAULT) this->AddItem(v->index);
		}
	}
}
