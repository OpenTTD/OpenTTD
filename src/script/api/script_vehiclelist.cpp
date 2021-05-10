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

#include "../../safeguards.h"

ScriptVehicleList::ScriptVehicleList()
{
	for (const Vehicle *v : Vehicle::Iterate()) {
		if ((v->owner == ScriptObject::GetCompany() || ScriptObject::GetCompany() == OWNER_DEITY) && (v->IsPrimaryVehicle() || (v->type == VEH_TRAIN && ::Train::From(v)->IsFreeWagon()))) this->AddItem(v->index);
	}
}

ScriptVehicleList_Station::ScriptVehicleList_Station(StationID station_id)
{
	if (!ScriptBaseStation::IsValidBaseStation(station_id)) return;

	for (const Vehicle *v : Vehicle::Iterate()) {
		if ((v->owner == ScriptObject::GetCompany() || ScriptObject::GetCompany() == OWNER_DEITY) && v->IsPrimaryVehicle()) {
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

	for (const Vehicle *v : Vehicle::Iterate()) {
		if ((v->owner == ScriptObject::GetCompany() || ScriptObject::GetCompany() == OWNER_DEITY) && v->IsPrimaryVehicle() && v->type == type) {
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
	if (!ScriptVehicle::IsValidVehicle(vehicle_id)) return;

	for (const Vehicle *v = Vehicle::Get(vehicle_id)->FirstShared(); v != nullptr; v = v->NextShared()) {
		this->AddItem(v->index);
	}
}

ScriptVehicleList_Group::ScriptVehicleList_Group(GroupID group_id)
{
	if (!ScriptGroup::IsValidGroup((ScriptGroup::GroupID)group_id)) return;

	for (const Vehicle *v : Vehicle::Iterate()) {
		if (v->owner == ScriptObject::GetCompany() && v->IsPrimaryVehicle()) {
			if (v->group_id == group_id) this->AddItem(v->index);
		}
	}
}

ScriptVehicleList_DefaultGroup::ScriptVehicleList_DefaultGroup(ScriptVehicle::VehicleType vehicle_type)
{
	if (vehicle_type < ScriptVehicle::VT_RAIL || vehicle_type > ScriptVehicle::VT_AIR) return;

	for (const Vehicle *v : Vehicle::Iterate()) {
		if (v->owner == ScriptObject::GetCompany() && v->IsPrimaryVehicle()) {
			if (v->type == (::VehicleType)vehicle_type && v->group_id == ScriptGroup::GROUP_DEFAULT) this->AddItem(v->index);
		}
	}
}
