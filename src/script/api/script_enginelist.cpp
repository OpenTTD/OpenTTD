/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_enginelist.cpp Implementation of ScriptEngineList and friends. */

#include "../../stdafx.h"
#include "script_enginelist.hpp"
#include "../../engine_base.h"

#include "../../safeguards.h"

ScriptEngineList::ScriptEngineList(ScriptVehicle::VehicleType vehicle_type)
{
	Engine *e;
	FOR_ALL_ENGINES_OF_TYPE(e, (::VehicleType)vehicle_type) {
		if (ScriptObject::GetCompany() == OWNER_DEITY || HasBit(e->company_avail, ScriptObject::GetCompany())) this->AddItem(e->index);
	}
}
