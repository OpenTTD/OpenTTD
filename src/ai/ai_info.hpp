/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_info.hpp AIInfo keeps track of all information of an AI, like Author, Description, ... */

#ifndef AI_INFO_HPP
#define AI_INFO_HPP

#ifdef ENABLE_AI

#include <list>
#include "../core/smallmap_type.hpp"
#include "../script/script_info.hpp"

/** Bitmask of flags for AI settings. */
enum AIConfigFlags {
	AICONFIG_NONE    = 0x0, ///< No flags set.
	AICONFIG_RANDOM  = 0x1, ///< When randomizing the AI, pick any value between min_value and max_value when on custom difficulty setting.
	AICONFIG_BOOLEAN = 0x2, ///< This value is a boolean (either 0 (false) or 1 (true) ).
	AICONFIG_INGAME  = 0x4, ///< This setting can be changed while the AI is running.
};

typedef SmallMap<int, char *> LabelMapping; ///< Map-type used to map the setting numbers to labels.

/** Info about a single AI setting. */
struct AIConfigItem {
	const char *name;        ///< The name of the configuration setting.
	const char *description; ///< The description of the configuration setting.
	int min_value;           ///< The minimal value this configuration setting can have.
	int max_value;           ///< The maximal value this configuration setting can have.
	int custom_value;        ///< The default value on custom difficulty setting.
	int easy_value;          ///< The default value on easy difficulty setting.
	int medium_value;        ///< The default value on medium difficulty setting.
	int hard_value;          ///< The default value on hard difficulty setting.
	int random_deviation;    ///< The maximum random deviation from the default value.
	int step_size;           ///< The step size in the gui.
	AIConfigFlags flags;     ///< Flags for the configuration setting.
	LabelMapping *labels;    ///< Text labels for the integer values.
};

extern AIConfigItem _start_date_config;

typedef std::list<AIConfigItem> AIConfigItemList; ///< List of AIConfig items.

/** Base class that holds some basic information about AIs and AI libraries. */
class AIFileInfo : public ScriptFileInfo {
public:
	/**
	 * Process the creation of a FileInfo object.
	 */
	static SQInteger Constructor(HSQUIRRELVM vm, AIFileInfo *info);

protected:
	class AIScanner *base; ///< AIScanner object that was used to scan this AI (library) info.
};

/** All static information from an AI like name, version, etc. */
class AIInfo : public AIFileInfo {
public:
	static const char *GetClassName() { return "AIInfo"; }

	AIInfo();
	~AIInfo();

	/**
	 * Create an AI, using this AIInfo as start-template.
	 */
	static SQInteger Constructor(HSQUIRRELVM vm);

	/**
	 * Create a dummy-AI.
	 */
	static SQInteger DummyConstructor(HSQUIRRELVM vm);

	/**
	 * Get the settings of the AI.
	 */
	bool GetSettings();

	/**
	 * Get the config list for this AI.
	 */
	const AIConfigItemList *GetConfigList() const;

	/**
	 * Get the description of a certain ai config option.
	 */
	const AIConfigItem *GetConfigItem(const char *name) const;

	/**
	 * Check if we can start this AI.
	 */
	bool CanLoadFromVersion(int version) const;

	/**
	 * Set a setting.
	 */
	SQInteger AddSetting(HSQUIRRELVM vm);

	/**
	 * Add labels for a setting.
	 */
	SQInteger AddLabels(HSQUIRRELVM vm);

	/**
	 * Get the default value for a setting.
	 */
	int GetSettingDefaultValue(const char *name) const;

	/**
	 * Use this AI as a random AI.
	 */
	bool UseAsRandomAI() const { return this->use_as_random; }

	/**
	 * Get the API version this AI is written for.
	 */
	const char *GetAPIVersion() const { return this->api_version; }

private:
	AIConfigItemList config_list; ///< List of settings from this AI.
	int min_loadable_version;     ///< The AI can load savegame data if the version is equal or greater than this.
	bool use_as_random;           ///< Should this AI be used when the user wants a "random AI"?
	const char *api_version;      ///< API version used by this AI.
};

/** All static information from an AI library like name, version, etc. */
class AILibrary : public AIFileInfo {
public:
	AILibrary() : AIFileInfo(), category(NULL) {};
	~AILibrary();

	/**
	 * Create an AI, using this AIInfo as start-template.
	 */
	static SQInteger Constructor(HSQUIRRELVM vm);

	/**
	 * Import a library in the current AI. This function can be used by AIs
	 * by calling import.
	 * @param vm The squirrel vm of the calling AI.
	 */
	static SQInteger Import(HSQUIRRELVM vm);

	/**
	 * Get the category this library is in.
	 */
	const char *GetCategory() const { return this->category; }

private:
	const char *category; ///< The category this library is in.
};

#endif /* ENABLE_AI */
#endif /* AI_INFO_HPP */
