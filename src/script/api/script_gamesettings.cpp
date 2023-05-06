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
#include "../../settings_cmd.h"

#include "../../safeguards.h"

/* static */ bool ScriptGameSettings::IsValid(const std::string &setting)
{
	const SettingDesc *sd = GetSettingFromName(setting);
	return sd != nullptr && sd->IsIntSetting();
}

/* static */ SQInteger ScriptGameSettings::GetValue(const std::string &setting)
{
	if (!IsValid(setting)) return -1;

	const SettingDesc *sd = GetSettingFromName(setting);
	assert(sd != nullptr);
	return sd->AsIntSetting()->Read(&_settings_game);
}

/* static */ bool ScriptGameSettings::SetValue(const std::string &setting, SQInteger value)
{
	EnforceDeityOrCompanyModeValid(false);
	if (!IsValid(setting)) return false;

	const SettingDesc *sd = GetSettingFromName(setting);
	assert(sd != nullptr);

	if ((sd->flags & SF_NO_NETWORK_SYNC) != 0) return false;

	value = Clamp<SQInteger>(value, INT32_MIN, INT32_MAX);

	return ScriptObject::Command<CMD_CHANGE_SETTING>::Do(sd->GetName(), value);
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
