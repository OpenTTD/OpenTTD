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
#ifdef ENABLE_AI

#include <map>
#include "ai_info.hpp"
#include "../core/string_compare_type.hpp"
#include "../company_type.h"

/**
 * AI settings for one company slot.
 */
class AIConfig {
private:
	/** List with name=>value pairs of all AI-specific settings */
	typedef std::map<const char *, int, StringCompare> SettingValueList;

public:
	AIConfig() :
		name(NULL),
		version(-1),
		info(NULL),
		config_list(NULL),
		is_random_ai(false)
	{}

	/**
	 * Create a new AI config that is a copy of an existing config.
	 * @param config The object to copy.
	 */
	AIConfig(const AIConfig *config);

	/** Delete an AI configuration. */
	~AIConfig();

	/**
	 * Set another AI to be loaded in this slot.
	 * @param name The name of the AI.
	 * @param version The version of the AI to load, or -1 of latest.
	 * @param force_exact_match If true try to find the exact same version
	 *   as specified. If false any compatible version is ok.
	 * @param is_random Is the AI chosen randomly?
	 */
	void ChangeAI(const char *name, int version = -1, bool force_exact_match = false, bool is_random = false);

	/**
	 * When ever the AI Scanner is reloaded, all infos become invalid. This
	 *  function tells AIConfig about this.
	 * @param force_exact_match If true try to find the exact same version
	 *   as specified. If false any version is ok.
	 * @return \c true if the reset was successful, \c false if the AI was no longer
	 *  found.
	 */
	bool ResetInfo(bool force_exact_match);

	/**
	 * Get the AIInfo linked to this AIConfig.
	 */
	class AIInfo *GetInfo() const;

	/**
	 * Get the config list for this AIConfig.
	 */
	const AIConfigItemList *GetConfigList();

	/**
	 * Where to get the config from, either default (depends on current game
	 * mode) or force either newgame or normal
	 */
	enum AISettingSource {
		AISS_DEFAULT,       ///< Get the AI config from the current game mode
		AISS_FORCE_NEWGAME, ///< Get the newgame AI config
		AISS_FORCE_GAME,    ///< Get the AI config from the current game
	};

	/**
	 * Get the config of a company.
	 */
	static AIConfig *GetConfig(CompanyID company, AISettingSource source = AISS_DEFAULT);

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
	const char *name;              ///< Name of the AI
	int version;                   ///< Version of the AI
	class AIInfo *info;            ///< AIInfo object for related to this AI version
	SettingValueList settings;     ///< List with all setting=>value pairs that are configure for this AI
	AIConfigItemList *config_list; ///< List with all settings defined by this AI
	bool is_random_ai;             ///< True if the AI in this slot was randomly chosen.
};

#endif /* ENABLE_AI */
#endif /* AI_CONFIG_HPP */
