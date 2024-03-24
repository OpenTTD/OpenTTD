/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_controller.hpp The controller of the script. */

#ifndef SCRIPT_CONTROLLER_HPP
#define SCRIPT_CONTROLLER_HPP

#include "script_types.hpp"

/**
 * The Controller, the class each Script should extend. It creates the Script,
 *  makes sure the logic kicks in correctly, and that #GetTick() has a valid
 *  value.
 *
 * When starting a new game, or when loading a game, OpenTTD tries to match a
 *  script that matches to the specified version as close as possible. It tries
 *  (from first to last, stopping as soon as the attempt succeeds)
 *
 *  - load the latest version of the same script that supports loading data from
 *    the saved version (the version of saved data must be equal or greater
 *    than ScriptInfo::MinVersionToLoad),
 *  - load the latest version of the same script (ignoring version requirements),
 *  - (for AIs) load a random AI, and finally
 *  - (for AIs) load the dummy AI.
 *
 * After determining the script to use, starting it is done as follows
 *
 * - An instance is constructed of the class derived from ScriptController
 *   (class name is retrieved from ScriptInfo::CreateInstance).
 * - If there is script data available in the loaded game and if the data is
 *   loadable according to ScriptInfo::MinVersionToLoad, #Load is called with the
 *   data from the loaded game.
 * - Finally, #Start is called to start execution of the script.
 *
 * See also https://wiki.openttd.org/en/Development/Script/Save%20and%20Load for more details.
 *
 * @api ai game
 */
class ScriptController {
	friend class AIScanner;
	friend class ScriptInstance;

public:
#ifndef DOXYGEN_API
	/**
	 * Initializer of the ScriptController.
	 * @param company The company this Script is normally serving.
	 */
	ScriptController(CompanyID company);

#else
	/**
	 * This function is called to start your script. Your script starts here. If you
	 *   return from this function, your script dies, so make sure that doesn't
	 *   happen.
	 * @note Cannot be called from within your script.
	 */
	void Start();

	/**
	 * Save the state of the script.
	 *
	 * By implementing this function, you can store some data inside the savegame.
	 *   The function should return a table with the information you want to store.
	 *   You can only store:
	 *
	 *   - integers,
	 *   - strings,
	 *   - arrays (max. 25 levels deep),
	 *   - tables (max. 25 levels deep),
	 *   - booleans, and
	 *   - nulls.
	 *
	 * In particular, instances of classes can't be saved including
	 *   ScriptList. Such a list should be converted to an array or table on
	 *   save and converted back on load.
	 *
	 * The function is called as soon as the user saves the game,
	 *   independently of other activities of the script. The script is not
	 *   notified of the call. To avoid race-conditions between #Save and the
	 *   other script code, change variables directly after a #Sleep, it is
	 *   very unlikely, to get interrupted at that point in the execution.
	 * See also https://wiki.openttd.org/en/Development/Script/Save%20and%20Load for more details.
	 *
	 * @note No other information is saved than the table returned by #Save.
	 *   For example all pending events are lost as soon as the game is loaded.
	 *
	 * @return Data of the script that should be stored in the save game.
	 */
	table Save();

	/**
	 * Load saved data just before calling #Start.
	 * The function is only called when there is data to load.
	 * @param version Version number of the script that created the \a data.
	 * @param data Data that was saved (return value of #Save).
	 */
	void Load(int version, table data);
#endif /* DOXYGEN_API */

	/**
	 * Find at which tick your script currently is.
	 * @return returns the current tick.
	 */
	static uint GetTick();

	/**
	 * Get the number of operations the script may still execute this tick.
	 * @return The amount of operations left to execute.
	 * @note This number can go negative when certain uninteruptable
	 *   operations are executed. The amount of operations that you go
	 *   over the limit will be deducted from the next tick you would
	 *   be allowed to run.
	 */
	static int GetOpsTillSuspend();

	/**
	 * Get the value of one of your settings you set via info.nut.
	 * @param name The name of the setting.
	 * @return the value for the setting, or -1 if the setting is not known.
	 */
	static int GetSetting(const std::string &name);

	/**
	 * Get the OpenTTD version of this executable. The version is formatted
	 * with the bits having the following meaning:
	 * 24-31 major version + 16.
	 * 20-23 minor version.
	 *    19 1 if it is a release, 0 if it is not.
	 *  0-18 revision number; 0 when the revision is unknown.
	 * You have to subtract 16 from the major version to get the correct
	 * value.
	 *
	 * Prior to OpenTTD 12, the bits have the following meaning:
	 * 28-31 major version.
	 * 24-27 minor version.
	 * 20-23 build.
	 *    19 1 if it is a release, 0 if it is not.
	 *  0-18 revision number; 0 when the revision is unknown.
	 *
	 * @return The version in newgrf format.
	 */
	static uint GetVersion();

	/**
	 * Change the minimum amount of time the script should be put in suspend mode
	 *   when you execute a command. Normally in SP this is 1, and in MP it is
	 *   what ever delay the server has been programmed to delay commands
	 *   (normally between 1 and 5). To give a more 'real' effect to your script,
	 *   you can control that number here.
	 * @param ticks The minimum amount of ticks to wait.
	 * @pre Ticks should be positive. Too big values will influence performance of the script.
	 * @note If the number is lower than the MP setting, the MP setting wins.
	 */
	static void SetCommandDelay(int ticks);

	/**
	 * Sleep for X ticks. The code continues after this line when the X script ticks
	 *   are passed. Mind that an script tick is different from in-game ticks and
	 *   differ per script speed.
	 * @param ticks the ticks to wait
	 * @pre ticks > 0.
	 * @post the value of GetTick() will be changed exactly 'ticks' in value after
	 *   calling this.
	 */
	static void Sleep(int ticks);

	/**
	 * Break execution of the script when script developer tools are active. For
	 * other users, nothing will happen when you call this function. To resume
	 * the script, you have to click on the continue button in the AI debug
	 * window. It is not recommended to leave calls to this function in scripts
	 * that you publish or upload to bananas.
	 * @param message to print in the AI debug window when the break occurs.
	 * @note gui.ai_developer_tools setting must be enabled or the break is
	 * ignored.
	 */
	static void Break(const std::string &message);

	/**
	 * When Squirrel triggers a print, this function is called.
	 *  Squirrel calls this when 'print' is used, or when the script made an error.
	 * @param error_msg If true, it is a Squirrel error message.
	 * @param message The message Squirrel logged.
	 * @note Use ScriptLog.Info/Warning/Error instead of 'print'.
	 */
	static void Print(bool error_msg, const std::string &message);

	/**
	 * Import a library.
	 * @param library The name of the library to import. The name should be composed as ScriptInfo::GetCategory() + "." +
	 * ScriptInfo::CreateInstance().
	 * @param class_name Under which name you want it to be available (or "" if you just want the returning object).
	 * @param version Which version you want specifically.
	 * @return The loaded library object. If class_name is set, it is also available (under the scope of the import) under that name.
	 * @note This command can be called from the global space, and does not need an instance.
	 */
	static HSQOBJECT Import(const std::string &library, const std::string &class_name, int version);

private:
	typedef std::map<std::string, std::string, CaseInsensitiveComparator> LoadedLibraryList; ///< The type for loaded libraries.

	uint ticks;                       ///< The amount of ticks we're sleeping.
	LoadedLibraryList loaded_library; ///< The libraries we loaded.
	int loaded_library_count;         ///< The amount of libraries.

	/**
	 * Register all classes that are known inside the script API.
	 */
	void RegisterClasses();
};

#endif /* SCRIPT_CONTROLLER_HPP */
