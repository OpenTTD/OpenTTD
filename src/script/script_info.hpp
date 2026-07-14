/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file script_info.hpp ScriptInfo keeps track of all information of a script, like Author, Description, ... */

#ifndef SCRIPT_INFO_HPP
#define SCRIPT_INFO_HPP

#include <squirrel.h>

#include "script_object.hpp"
#include "script_config.hpp"

/** The maximum number of operations for saving or loading the data of a script. */
static const int MAX_SL_OPS             = 100000;
/** The maximum number of operations for initial start of a script. */
static const int MAX_CONSTRUCTOR_OPS    = 100000;
/** Number of operations to create an instance of a script. */
static const int MAX_CREATEINSTANCE_OPS = 100000;
/** Number of operations to get the author and similar information. */
static const int MAX_GET_OPS            =   1000;
/** Maximum number of operations allowed for getting a particular setting. */
static const int MAX_GET_SETTING_OPS    = 100000;

/** All static information from an Script like name, version, etc. */
class ScriptInfo : public SimpleCountedObject {
public:
	/**
	 * Get the Author of the script.
	 * @return The author's name.
	 */
	const std::string &GetAuthor() const { return this->author; }

	/**
	 * Get the Name of the script.
	 * @return The script's name.
	 */
	const std::string &GetName() const { return this->name; }

	/**
	 * Get the 4 character long short name of the script.
	 * @return The short name.
	 */
	const std::string &GetShortName() const { return this->short_name; }

	/**
	 * Get the description of the script.
	 * @return The description.
	 */
	const std::string &GetDescription() const { return this->description; }

	/**
	 * Get the version of the script.
	 * @return The numeric versionof the script.
	 */
	int GetVersion() const { return this->version; }

	/**
	 * Get the last-modified date of the script.
	 * @return The date.
	 */
	const std::string &GetDate() const { return this->date; }

	/**
	 * Get the name of the instance of the script to create.
	 * @return Name of the instance.
	 */
	const std::string &GetInstanceName() const { return this->instance_name; }

	/**
	 * Get the website for this script.
	 * @return Optional URL.
	 */
	const std::string &GetURL() const { return this->url; }

	/**
	 * Get the filename of the main.nut script.
	 * @return The path to the main script.
	 */
	const std::string &GetMainScript() const { return this->main_script; }

	/**
	 * Get the filename of the tar the script is in.
	 * @return The tar file the script is in, or an empty string.
	 */
	const std::string &GetTarFile() const { return this->tar_file; }

	/**
	 * Check if a given method exists.
	 * @param name The method name to look for.
	 * @return \c true iff the method exists.
	 */
	bool CheckMethod(std::string_view name) const;

	/**
	 * Process the creation of a FileInfo object.
	 * @param vm The virtual machine to work on.
	 * @param info The metadata about the script.
	 * @return \c 0 upon success, or anything other on failure.
	 */
	static SQInteger Constructor(HSQUIRRELVM vm, ScriptInfo &info);

	/**
	 * Get the scanner which has found this ScriptInfo.
	 * @return The scanner for scripts.
	 */
	virtual class ScriptScanner *GetScanner() { return this->scanner; }

	/**
	 * Does this script have a 'GetSettings' function?
	 * @return \c true iff the script has the GetSettings function.
	 */
	bool GetSettings();

	/**
	 * Get the config list for this Script.
	 * @return The configuration list.
	 */
	const ScriptConfigItemList *GetConfigList() const;

	/**
	 * Get the description of a certain Script config option.
	 * @param name The name of the setting.
	 * @return The configuration item, or \c nullptr.
	 */
	const ScriptConfigItem *GetConfigItem(std::string_view name) const;

	/**
	 * Add a setting.
	 * @param vm The virtual machine to work on.
	 * @return \c 0 upon success, or anything other on failure.
	 */
	SQInteger AddSetting(HSQUIRRELVM vm);

	/**
	 * Add labels for a setting.
	 * @param vm The virtual machine to work on.
	 * @return \c 0 upon success, or anything other on failure.
	 */
	SQInteger AddLabels(HSQUIRRELVM vm);

	/**
	 * Get the default value for a setting.
	 * @param name The name of the setting to get the default for.
	 * @return The default value for the setting, or \c -1 when the setting does not exist.
	 * @note \c -1 can be a valid default setting.
	 */
	int GetSettingDefaultValue(const std::string &name) const;

	/**
	 * Can this script be selected by developers only?
	 * @return \c true iff the script should only be shown to script developers.
	 */
	virtual bool IsDeveloperOnly() const { return false; }

protected:
	class Squirrel *engine = nullptr; ///< Engine used to register for Squirrel.
	HSQOBJECT SQ_instance{}; ///< The Squirrel instance created for this info.
	ScriptConfigItemList config_list{}; ///< List of settings from this Script.

private:
	std::string main_script{}; ///< The full path of the script.
	std::string tar_file{}; ///< If, which tar file the script was in.
	std::string author{}; ///< Author of the script.
	std::string name{}; ///< Full name of the script.
	std::string short_name{}; ///< Short name (4 chars) which uniquely identifies the script.
	std::string description{}; ///< Small description of the script.
	std::string date{}; ///< The date the script was written at.
	std::string instance_name{}; ///< Name of the main class in the script.
	int version = 0; ///< Version of the script.
	std::string url{}; ///< URL of the script.

	class ScriptScanner *scanner = nullptr; ///< ScriptScanner object that was used to scan this script info.
};

void Script_CreateDummyInfo(HSQUIRRELVM vm, std::string_view type, std::string_view dir);
void Script_CreateDummy(HSQUIRRELVM vm, StringID string, std::string_view type);

#endif /* SCRIPT_INFO_HPP */
