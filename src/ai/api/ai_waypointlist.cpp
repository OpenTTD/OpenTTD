/* $Id$ */

/** @file ai_waypointlist.cpp Implementation of AIWaypointList and friends. */

#include "ai_waypointlist.hpp"
#include "ai_vehicle.hpp"
#include "ai_waypoint.hpp"
#include "../../company_func.h"
#include "../../vehicle_base.h"
#include "../../waypoint.h"

AIWaypointList::AIWaypointList()
{
	const Waypoint *wp;
	FOR_ALL_WAYPOINTS(wp) {
		if (wp->owner == _current_company) this->AddItem(wp->index);
	}
}

AIWaypointList_Vehicle::AIWaypointList_Vehicle(VehicleID vehicle_id)
{
	if (!AIVehicle::IsValidVehicle(vehicle_id)) return;

	const Vehicle *v = ::GetVehicle(vehicle_id);

	for (const Order *o = v->GetFirstOrder(); o != NULL; o = o->next) {
		if (o->IsType(OT_GOTO_WAYPOINT)) this->AddItem(o->GetDestination());
	}
}
