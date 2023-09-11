/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_railtypelist.cpp Implementation of ScriptRailTypeList and friends. */

#include "../../stdafx.h"
#include "script_railtypelist.hpp"
#include "script_error.hpp"
#include "../../rail.h"

#include "../../safeguards.h"

ScriptRailTypeList::ScriptRailTypeList()
{
	EnforceDeityOrCompanyModeValid_Void();
	for (RailType rt = RAILTYPE_BEGIN; rt != RAILTYPE_END; rt++) {
		if (ScriptCompanyMode::IsDeity() || ::HasRailTypeAvail(ScriptObject::GetCompany(), rt)) this->AddItem(rt);
	}
}
