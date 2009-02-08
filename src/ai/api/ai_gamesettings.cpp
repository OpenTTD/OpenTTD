/* $Id$ */

/** @file ai_gamesettings.cpp Implementation of AIGameSettings. */

#include "ai_gamesettings.hpp"
#include "../../settings_internal.h"

/* static */ bool AIGameSettings::IsValid(const char *setting)
{
	uint i;
	const SettingDesc *sd = GetSettingFromName(setting, &i);
	return sd != NULL && sd->desc.cmd != SDT_STRING;
}

/* static */ int32 AIGameSettings::GetValue(const char *setting)
{
	if (!IsValid(setting)) return -1;

	uint i;
	const SettingDesc *sd = GetSettingFromName(setting, &i);

	void *ptr = GetVariableAddress(&_settings_game, &sd->save);
	if (sd->desc.cmd == SDT_BOOLX) return *(bool*)ptr;

	return (int32)ReadValue(ptr, sd->save.conv);
}

/* static */ bool AIGameSettings::IsDisabledVehicleType(AIVehicle::VehicleType vehicle_type)
{
	switch (vehicle_type) {
		case AIVehicle::VT_RAIL:  return _settings_game.ai.ai_disable_veh_train;
		case AIVehicle::VT_ROAD:  return _settings_game.ai.ai_disable_veh_roadveh;
		case AIVehicle::VT_WATER: return _settings_game.ai.ai_disable_veh_ship;
		case AIVehicle::VT_AIR:   return _settings_game.ai.ai_disable_veh_aircraft;
		default:                       return true;
	}
}
