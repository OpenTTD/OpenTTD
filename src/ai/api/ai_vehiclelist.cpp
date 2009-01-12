/* $Id$ */

/** @file ai_vehiclelist.cpp Implementation of AIVehicleList and friends. */

#include "ai_vehiclelist.hpp"
#include "ai_station.hpp"
#include "../../openttd.h"
#include "../../company_func.h"
#include "../../vehicle_base.h"

AIVehicleList::AIVehicleList()
{
	Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		if (v->owner == _current_company && v->IsPrimaryVehicle()) this->AddItem(v->index);
	}
}

AIVehicleList_Station::AIVehicleList_Station(StationID station_id)
{
	if (!AIStation::IsValidStation(station_id)) return;

	Vehicle *v;

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
