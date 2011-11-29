/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_gamesettings.cpp Implementation of ScriptGameSettings. */

#include "../../stdafx.h"
#include "script_gamesettings.hpp"
#include "../../settings_internal.h"

/* static */ bool ScriptGameSettings::IsValid(const char *setting)
{
	uint i;
	const SettingDesc *sd = GetSettingFromName(setting, &i);
	return sd != NULL && sd->desc.cmd != SDT_STRING;
}

/* static */ int32 ScriptGameSettings::GetValue(const char *setting)
{
	if (!IsValid(setting)) return -1;

	uint i;
	const SettingDesc *sd = GetSettingFromName(setting, &i);

	void *ptr = GetVariableAddress(&_settings_game, &sd->save);
	if (sd->desc.cmd == SDT_BOOLX) return *(bool*)ptr;

	return (int32)ReadValue(ptr, sd->save.conv);
}

/* static */ bool ScriptGameSettings::IsDisabledVehicleType(ScriptVehicle::VehicleType vehicle_type)
{
	switch (vehicle_type) {
		case ScriptVehicle::VT_RAIL:  return _settings_game.ai.ai_disable_veh_train;
		case ScriptVehicle::VT_ROAD:  return _settings_game.ai.ai_disable_veh_roadveh;
		case ScriptVehicle::VT_WATER: return _settings_game.ai.ai_disable_veh_ship;
		case ScriptVehicle::VT_AIR:   return _settings_game.ai.ai_disable_veh_aircraft;
		default:                       return true;
	}
}
