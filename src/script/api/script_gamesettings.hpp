/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_gamesettings.hpp Everything to read game settings. */

#ifndef SCRIPT_GAMESETTINGS_HPP
#define SCRIPT_GAMESETTINGS_HPP

#include "script_vehicle.hpp"

/**
 * Class that handles all game settings related functions.
 * @api ai game
 *
 * @note ScriptGameSettings::IsValid and ScriptGameSettings::GetValue are functions
 *       that rely on the settings as OpenTTD stores them in savegame and
 *       openttd.cfg. No guarantees can be given on the long term validity,
 *       consistency and stability of the names, values and value ranges.
 *       Using these settings can be dangerous and could cause issues in
 *       future versions. To make sure that a setting still exists in the
 *       current version you have to run ScriptGameSettings::IsValid before
 *       accessing it.
 *
 * @note The names of the setting for ScriptGameSettings::IsValid and
 *       ScriptGameSettings::GetValue are the same ones as those that are shown by
 *       the list_settings command in the in-game console. Settings that are
 *       string based are NOT supported and GameSettings::IsValid will return
 *       false for them. These settings will not be supported either because
 *       they have no relevance for the script (default client names, server IPs,
 *       etc.).
 */
class ScriptGameSettings : public ScriptObject {
public:
	/**
	 * Is the given game setting a valid setting for this instance of OpenTTD?
	 * @param setting The setting to check for existence.
	 * @warning Results of this function are not governed by the API. This means
	 *          that a setting that previously existed can be gone or has
	 *          changed its name.
	 * @note Results achieved in the past offer no guarantee for the future.
	 * @return True if and only if the setting is valid.
	 */
	static bool IsValid(const std::string &setting);

	/**
	 * Gets the value of the game setting.
	 * @param setting The setting to get the value of.
	 * @pre IsValid(setting).
	 * @warning Results of this function are not governed by the API. This means
	 *          that the value of settings may be out of the expected range. It
	 *          also means that a setting that previously existed can be gone or
	 *          has changed its name/characteristics.
	 * @note Results achieved in the past offer no guarantee for the future.
	 * @return The value for the setting.
	 */
	static SQInteger GetValue(const std::string &setting);

	/**
	 * Sets the value of the game setting.
	 * @param setting The setting to set the value of.
	 * @param value The value to set the setting to.
	 *              The value will be clamped to MIN(int32_t) .. MAX(int32_t).
	 * @pre IsValid(setting).
	 * @return True if the action succeeded.
	 * @note Results achieved in the past offer no guarantee for the future.
	 * @api -ai
	 */
	static bool SetValue(const std::string &setting, SQInteger value);

	/**
	 * Checks whether the given vehicle-type is disabled for companies.
	 * @param vehicle_type The vehicle-type to check.
	 * @return True if the vehicle-type is disabled.
	 */
	static bool IsDisabledVehicleType(ScriptVehicle::VehicleType vehicle_type);
};

#endif /* SCRIPT_GAMESETTINGS_HPP */
