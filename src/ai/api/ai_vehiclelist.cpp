/* $Id$ */

/** @file ai_vehiclelist.cpp Implementation of AIVehicleList and friends. */

#include "ai_vehiclelist.hpp"
#include "ai_group.hpp"
#include "ai_station.hpp"
#include "ai_vehicle.hpp"
#include "../../company_func.h"
#include "../../vehicle_base.h"

AIVehicleList::AIVehicleList()
{
	const Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		if (v->owner == _current_company && v->IsPrimaryVehicle()) this->AddItem(v->index);
	}
}

AIVehicleList_Station::AIVehicleList_Station(StationID station_id)
{
	if (!AIStation::IsValidStation(station_id)) return;

	const Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		if (v->owner == _current_company && v->IsPrimaryVehicle()) {
			const Order *order;

			FOR_VEHICLE_ORDERS(v, order) {
				if (order->IsType(OT_GOTO_STATION) && order->GetDestination() == station_id) {
					this->AddItem(v->index);
					break;
				}
			}
		}
	}
}

AIVehicleList_SharedOrders::AIVehicleList_SharedOrders(VehicleID vehicle_id)
{
	if (!AIVehicle::IsValidVehicle(vehicle_id)) return;

	for (const Vehicle *v = GetVehicle(vehicle_id)->FirstShared(); v != NULL; v = v->NextShared()) {
		this->AddItem(v->index);
	}
}

AIVehicleList_Group::AIVehicleList_Group(GroupID group_id)
{
	if (!AIGroup::IsValidGroup((AIGroup::GroupID)group_id)) return;

	const Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		if (v->owner == _current_company && v->IsPrimaryVehicle()) {
			if (v->group_id == group_id) this->AddItem(v->index);
		}
	}
}

AIVehicleList_DefaultGroup::AIVehicleList_DefaultGroup(AIVehicle::VehicleType vehicle_type)
{
	if (vehicle_type < AIVehicle::VT_RAIL || vehicle_type > AIVehicle::VT_AIR) return;

	const Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		if (v->owner == _current_company && v->IsPrimaryVehicle()) {
			if (v->type == vehicle_type && v->group_id == AIGroup::GROUP_DEFAULT) this->AddItem(v->index);
		}
	}
}
