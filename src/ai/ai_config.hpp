/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_config.hpp AIConfig stores the configuration settings of every AI. */

#ifndef AI_CONFIG_HPP
#define AI_CONFIG_HPP

#include <map>
#include "ai_info.hpp"
#include "../core/string_compare_type.hpp"
#include "../company_type.h"

class AIConfig {
private:
	typedef std::map<const char *, int, StringCompare> SettingValueList;

public:
	AIConfig() :
		name(NULL),
		version(-1),
		info(NULL),
		config_list(NULL),
		is_random_ai(false)
	{}
	AIConfig(const AIConfig *config);
	~AIConfig();

	/**
	 * Set another AI to be loaded in this slot.
	 * @param name The name of the AI.
	 * @param version The version of the AI to load, or -1 of latest.
	 * @param is_random Is the AI chosen randomly?
	 */
	void ChangeAI(const char *name, int version = -1, bool is_random = false);

	/**
	 * When ever the AI Scanner is reloaded, all infos become invalid. This
	 *  function tells AIConfig about this.
	 * @return True if the reset was successfull, false if the AI was no longer
	 *  found.
	 */
	bool ResetInfo();

	/**
	 * Get the AIInfo linked to this AIConfig.
	 */
	class AIInfo *GetInfo() const;

	/**
	 * Get the config list for this AIConfig.
	 */
	const AIConfigItemList *GetConfigList();

	/**
	 * Get the config of a company.
	 */
	static AIConfig *GetConfig(CompanyID company, bool forceNewgameSetting = false);

	/**
	 * Get the value of a setting for this config. It might fallback to his
	 *  'info' to find the default value (if not set or if not-custom difficulty
	 *  level).
	 * @return The (default) value of the setting, or -1 if the setting was not
	 *  found.
	 */
	int GetSetting(const char *name) const;

	/**
	 * Set the value of a setting for this config.
	 */
	void SetSetting(const char *name, int value);

	/**
	 * Reset all settings to their default value.
	 */
	void ResetSettings();

	/**
	 * Randomize all settings the AI requested to be randomized.
	 */
	void AddRandomDeviation();

	/**
	 * Is this config attached to an AI?
	 */
	bool HasAI() const;

	/**
	 * Is the current AI a randomly chosen AI?
	 */
	bool IsRandomAI() const;

	/**
	 * Get the name of the AI.
	 */
	const char *GetName() const;

	/**
	 * Get the version of the AI.
	 */
	int GetVersion() const;

	/**
	 * Convert a string which is stored in the config file or savegames to
	 *  custom settings of this AI.
	 */
	void StringToSettings(const char *value);

	/**
	 * Convert the custom settings to a string that can be stored in the config
	 *  file or savegames.
	 */
	void SettingsToString(char *string, size_t size) const;

private:
	const char *name;
	int version;
	class AIInfo *info;
	SettingValueList settings;
	AIConfigItemList *config_list;
	bool is_random_ai;
};

#endif /* AI_CONFIG_HPP */
