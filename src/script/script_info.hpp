/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_info.hpp ScriptInfo keeps track of all information of a script, like Author, Description, ... */

#ifndef SCRIPT_INFO_HPP
#define SCRIPT_INFO_HPP

#include <squirrel.h>
#include "../misc/countedptr.hpp"

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
	ScriptInfo() :
		engine(nullptr),
		version(0),
		scanner(nullptr)
	{}

	/**
	 * Get the Author of the script.
	 */
	const std::string &GetAuthor() const { return this->author; }

	/**
	 * Get the Name of the script.
	 */
	const std::string &GetName() const { return this->name; }

	/**
	 * Get the 4 character long short name of the script.
	 */
	const std::string &GetShortName() const { return this->short_name; }

	/**
	 * Get the description of the script.
	 */
	const std::string &GetDescription() const { return this->description; }

	/**
	 * Get the version of the script.
	 */
	int GetVersion() const { return this->version; }

	/**
	 * Get the last-modified date of the script.
	 */
	const std::string &GetDate() const { return this->date; }

	/**
	 * Get the name of the instance of the script to create.
	 */
	const std::string &GetInstanceName() const { return this->instance_name; }

	/**
	 * Get the website for this script.
	 */
	const std::string &GetURL() const { return this->url; }

	/**
	 * Get the filename of the main.nut script.
	 */
	const std::string &GetMainScript() const { return this->main_script; }

	/**
	 * Get the filename of the tar the script is in.
	 */
	const std::string &GetTarFile() const { return this->tar_file; }

	/**
	 * Check if a given method exists.
	 */
	bool CheckMethod(const char *name) const;

	/**
	 * Process the creation of a FileInfo object.
	 */
	static SQInteger Constructor(HSQUIRRELVM vm, ScriptInfo *info);

	/**
	 * Get the scanner which has found this ScriptInfo.
	 */
	virtual class ScriptScanner *GetScanner() { return this->scanner; }

	/**
	 * Get the settings of the Script.
	 */
	bool GetSettings();

	/**
	 * Get the config list for this Script.
	 */
	const ScriptConfigItemList *GetConfigList() const;

	/**
	 * Get the description of a certain Script config option.
	 */
	const ScriptConfigItem *GetConfigItem(const std::string_view name) const;

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
	int GetSettingDefaultValue(const std::string &name) const;

	/**
	 * Can this script be selected by developers only?
	 */
	virtual bool IsDeveloperOnly() const { return false; }

protected:
	class Squirrel *engine;           ///< Engine used to register for Squirrel.
	HSQOBJECT SQ_instance;            ///< The Squirrel instance created for this info.
	ScriptConfigItemList config_list; ///< List of settings from this Script.

private:
	std::string main_script;      ///< The full path of the script.
	std::string tar_file;         ///< If, which tar file the script was in.
	std::string author;           ///< Author of the script.
	std::string name;             ///< Full name of the script.
	std::string short_name;       ///< Short name (4 chars) which uniquely identifies the script.
	std::string description;      ///< Small description of the script.
	std::string date;             ///< The date the script was written at.
	std::string instance_name;    ///< Name of the main class in the script.
	int version;                  ///< Version of the script.
	std::string url;              ///< URL of the script.

	class ScriptScanner *scanner; ///< ScriptScanner object that was used to scan this script info.
};

void Script_CreateDummyInfo(HSQUIRRELVM vm, const char *type, const char *dir);
void Script_CreateDummy(HSQUIRRELVM vm, StringID string, const char *type);

#endif /* SCRIPT_INFO_HPP */
