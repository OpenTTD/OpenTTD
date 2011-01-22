/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_gamesettings.hpp Everything to read game settings. */

#ifndef AI_GAMESETTINGS_HPP
#define AI_GAMESETTINGS_HPP

#include "ai_vehicle.hpp"

/**
 * Class that handles all game settings related functions.
 *
 * @note AIGameSettings::IsValid and AIGameSettings::GetValue are functions
 *       that rely on the settings as OpenTTD stores them in savegame and
 *       openttd.cfg. No guarantees can be given on the long term validity,
 *       consistency and stability of the names, values and value ranges.
 *       Using these settings can be dangerous and could cause issues in
 *       future versions. To make sure that a setting still exists in the
 *       current version you have to run AIGameSettings::IsValid before
 *       accessing it.
 *
 * @note The names of the setting for AIGameSettings::IsValid and
 *       AIGameSettings::GetValue are the same ones as those that are shown by
 *       the list_settings command in the in-game console. Settings that are
 *       string based are NOT supported and AIGAmeSettings::IsValid will return
 *       false for them. These settings will not be supported either because
 *       they have no relevance for the AI (default client names, server IPs,
 *       etc.).
 */
class AIGameSettings : public AIObject {
public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIGameSettings"; }

	/**
	 * Is the given game setting a valid setting for this instance of OpenTTD?
	 * @param setting The setting to check for existence.
	 * @warning Results of this function are not governed by the API. This means
	 *          that a setting that previously existed can be gone or has
	 *          changed its name.
	 * @note Results achieved in the past offer no gurantee for the future.
	 * @return True if and only if the setting is valid.
	 */
	static bool IsValid(const char *setting);

	/**
	 * Gets the value of the game setting.
	 * @param setting The setting to get the value of.
	 * @pre IsValid(setting).
	 * @warning Results of this function are not governed by the API. This means
	 *          that the value of settings may be out of the expected range. It
	 *          also means that a setting that previously existed can be gone or
	 *          has changed its name/characteristics.
	 * @note Results achieved in the past offer no gurantee for the future.
	 * @return The value for the setting.
	 */
	static int32 GetValue(const char *setting);

	/**
	 * Checks whether the given vehicle-type is disabled for AIs.
	 * @param vehicle_type The vehicle-type to check.
	 * @return True if the vehicle-type is disabled.
	 */
	static bool IsDisabledVehicleType(AIVehicle::VehicleType vehicle_type);
};

#endif /* AI_GAMESETTINGS_HPP */
