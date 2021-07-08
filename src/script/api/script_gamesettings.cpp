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
#include "../../settings_type.h"
#include "../../command_type.h"

#include "../../safeguards.h"

/* static */ bool ScriptGameSettings::IsValid(const char *setting)
{
	const SettingDesc *sd = GetSettingFromName(setting);
	return sd != nullptr && sd->IsIntSetting();
}

/* static */ int32 ScriptGameSettings::GetValue(const char *setting)
{
	if (!IsValid(setting)) return -1;

	const SettingDesc *sd = GetSettingFromName(setting);
	return sd->AsIntSetting()->Read(&_settings_game);
}

/* static */ bool ScriptGameSettings::SetValue(const char *setting, int value)
{
	if (!IsValid(setting)) return false;

	const SettingDesc *sd = GetSettingFromName(setting);

	if ((sd->flags & SF_NO_NETWORK_SYNC) != 0) return false;

	return ScriptObject::DoCommand(0, 0, value, CMD_CHANGE_SETTING, sd->GetName().c_str());
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
