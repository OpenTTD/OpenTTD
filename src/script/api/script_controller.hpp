/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_controller.hpp The controller of the AI. */

#ifndef SCRIPT_CONTROLLER_HPP
#define SCRIPT_CONTROLLER_HPP

#include "../../core/string_compare_type.hpp"
#include <map>

/**
 * The Controller, the class each Script should extend. It creates the Script,
 *  makes sure the logic kicks in correctly, and that GetTick() has a valid
 *  value.
 * @api ai game
 */
class ScriptController {
	friend class AIScanner;
	friend class ScriptInstance;

public:
	/**
	 * Initializer of the ScriptController.
	 */
	ScriptController();

	/**
	 * Destructor of the ScriptController.
	 */
	~ScriptController();

	/**
	 * This function is called to start your AI. Your AI starts here. If you
	 *   return from this function, your AI dies, so make sure that doesn't
	 *   happen.
	 * @note Cannot be called from within your AI.
	 */
	void Start();

	/**
	 * Find at which tick your AI currently is.
	 * @return returns the current tick.
	 */
	static uint GetTick();

	/**
	 * Get the number of operations the AI may still execute this tick.
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
	static int GetSetting(const char *name);

	/**
	 * Get the OpenTTD version of this executable. The version is formatted
	 * with the bits having the following meaning:
	 * 28-31 major version
	 * 24-27 minor version
	 * 20-23 build
	 *    19 1 if it is a release, 0 if it is not.
	 *  0-18 revision number; 0 when the revision is unknown.
	 * @return The version in newgrf format.
	 */
	static uint GetVersion();

	/**
	 * Change the minimum amount of time the AI should be put in suspend mode
	 *   when you execute a command. Normally in SP this is 1, and in MP it is
	 *   what ever delay the server has been programmed to delay commands
	 *   (normally between 1 and 5). To give a more 'real' effect to your AI,
	 *   you can control that number here.
	 * @param ticks The minimum amount of ticks to wait.
	 * @pre Ticks should be positive. Too big values will influence performance of the AI.
	 * @note If the number is lower than the MP setting, the MP setting wins.
	 */
	static void SetCommandDelay(int ticks);

	/**
	 * Sleep for X ticks. The code continues after this line when the X AI ticks
	 *   are passed. Mind that an AI tick is different from in-game ticks and
	 *   differ per AI speed.
	 * @param ticks the ticks to wait
	 * @pre ticks > 0.
	 * @post the value of GetTick() will be changed exactly 'ticks' in value after
	 *   calling this.
	 */
	static void Sleep(int ticks);

	/**
	 * When Squirrel triggers a print, this function is called.
	 *  Squirrel calls this when 'print' is used, or when the script made an error.
	 * @param error_msg If true, it is a Squirrel error message.
	 * @param message The message Squirrel logged.
	 * @note Use ScriptLog.Info/Warning/Error instead of 'print'.
	 */
	static void Print(bool error_msg, const char *message);

	/**
	 * Import a library.
	 * @param library The name of the library to import.
	 * @param class_name Under which name you want it to be available (or "" if you just want the returning object).
	 * @param version Which version you want specificly.
	 * @return The loaded library object. If class_name is set, it is also available (under the scope of the import) under that name.
	 * @note This command can be called from the global space, and does not need an instance.
	 */
	static HSQOBJECT Import(const char *library, const char *class_name, int version);

private:
	typedef std::map<const char *, const char *, StringCompare> LoadedLibraryList; ///< The type for loaded libraries.

	uint ticks;                       ///< The amount of ticks we're sleeping.
	LoadedLibraryList loaded_library; ///< The libraries we loaded.
	int loaded_library_count;         ///< The amount of libraries.

	/**
	 * Register all classes that are known inside the NoAI API.
	 */
	void RegisterClasses();
};

#endif /* SCRIPT_CONTROLLER_HPP */
