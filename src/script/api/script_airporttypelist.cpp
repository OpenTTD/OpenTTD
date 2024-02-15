/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_airporttypelist.cpp Implementation of ScriptAirportTypeList and friends. */

#include "../../stdafx.h"
#include "script_airporttypelist.hpp"
#include "script_error.hpp"
#include "../../newgrf_airport.h"

#include "../../safeguards.h"


ScriptAirportTypeList::ScriptAirportTypeList()
{
	EnforceDeityOrCompanyModeValid_Void();
	bool is_deity = ScriptCompanyMode::IsDeity();
	for (uint8_t at = 0; at < ::NUM_AIRPORTS; at++) {
		if ((is_deity && ::AirportSpec::Get(at)->enabled) || ::AirportSpec::Get(at)->IsAvailable()) this->AddItem(at);
	}
}
