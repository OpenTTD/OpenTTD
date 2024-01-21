/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_waypointlist.cpp Implementation of ScriptWaypointList and friends. */

#include "../../stdafx.h"
#include "script_waypointlist.h"
#include "script_vehicle.h"
#include "../../vehicle_base.h"
#include "../../waypoint_base.h"

#include "../../safeguards.h"

ScriptWaypointList::ScriptWaypointList(ScriptWaypoint::WaypointType waypoint_type)
{
	EnforceDeityOrCompanyModeValid_Void();

	bool is_deity = ScriptCompanyMode::IsDeity();
	CompanyID owner = ScriptObject::GetCompany();
	ScriptList::FillList<Waypoint>(this,
		[is_deity, owner, waypoint_type](const Waypoint *wp) {
			return (is_deity || wp->owner == owner || wp->owner == OWNER_NONE) && (wp->facilities & static_cast<StationFacility>(waypoint_type)) != 0;
		}
	);
}

ScriptWaypointList_Vehicle::ScriptWaypointList_Vehicle(VehicleID vehicle_id)
{
	if (!ScriptVehicle::IsPrimaryVehicle(vehicle_id)) return;

	const Vehicle *v = ::Vehicle::Get(vehicle_id);

	for (const Order *o = v->GetFirstOrder(); o != nullptr; o = o->next) {
		if (o->IsType(OT_GOTO_WAYPOINT)) this->AddItem(o->GetDestination());
	}
}
