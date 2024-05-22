/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_roadtypelist.cpp Implementation of ScriptRoadTypeList and friends. */

#include "../../stdafx.h"
#include "script_roadtypelist.hpp"
#include "../../road_func.h"

#include "../../safeguards.h"

ScriptRoadTypeList::ScriptRoadTypeList(ScriptRoad::RoadTramTypes rtts)
{
	EnforceDeityOrCompanyModeValid_Void();
	CompanyID owner = ScriptObject::GetCompany();
	for (RoadType rt = ROADTYPE_BEGIN; rt != ROADTYPE_END; rt++) {
		if (!HasBit(rtts, GetRoadTramType(rt))) continue;
		if (::HasRoadTypeAvail(owner, rt)) this->AddItem(rt);
	}
}
