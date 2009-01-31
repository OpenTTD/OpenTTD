/* $Id$ */

/** @file ai_stationlist.cpp Implementation of AIStationList and friends. */

#include "ai_stationlist.hpp"
#include "ai_vehicle.hpp"
#include "../../company_func.h"
#include "../../station_base.h"
#include "../../vehicle_base.h"

AIStationList::AIStationList(AIStation::StationType station_type)
{
	Station *st;
	FOR_ALL_STATIONS(st) {
		if (st->owner == _current_company && (st->facilities & station_type) != 0) this->AddItem(st->index);
	}
}

AIStationList_Vehicle::AIStationList_Vehicle(VehicleID vehicle_id)
{
	if (!AIVehicle::IsValidVehicle(vehicle_id)) return;

	Vehicle *v = ::GetVehicle(vehicle_id);

	for (Order *o = v->GetFirstOrder(); o != NULL; o = o->next) {
		if (o->IsType(OT_GOTO_STATION)) this->AddItem(o->GetDestination());
	}
}
