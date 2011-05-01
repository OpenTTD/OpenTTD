/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_controller.hpp The controller of the AI. */

#ifndef AI_CONTROLLER_HPP
#define AI_CONTROLLER_HPP

#include "../../core/string_compare_type.hpp"
#include <map>

/**
 * The Controller, the class each AI should extend. It creates the AI, makes
 *  sure the logic kicks in correctly, and that GetTick() has a valid value.
 */
class AIController {
	friend class AIScanner;
	friend class AIInstance;

public:
	/** Get the name of this class to identify it towards squirrel. */
	static const char *GetClassName() { return "AIController"; }

	/**
	 * Initializer of the AIController.
	 */
	AIController();

	/**
	 * Destructor of the AIController.
	 */
	~AIController();

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
	 * @note Use AILog.Info/Warning/Error instead of 'print'.
	 */
	static void Print(bool error_msg, const char *message);

private:
	typedef std::map<const char *, const char *, StringCompare> LoadedLibraryList; ///< The type for loaded libraries.

	uint ticks;                       ///< The amount of ticks we're sleeping.
	LoadedLibraryList loaded_library; ///< The libraries we loaded.
	int loaded_library_count;         ///< The amount of libraries.

	/**
	 * Register all classes that are known inside the NoAI API.
	 */
	void RegisterClasses();

	/**
	 * Check if a library is already loaded. If found, fake_class_name is filled
	 *  with the fake class name as given via AddLoadedLibrary. If not found,
	 *  next_number is set to the next number available for the fake namespace.
	 * @param library_name The library to check if already loaded.
	 * @param next_number The next available number for a library if not already loaded.
	 * @param fake_class_name The name the library has if already loaded.
	 * @param fake_class_name_len The maximum length of fake_class_name.
	 * @return True if the library is already loaded.
	 */
	bool LoadedLibrary(const char *library_name, int *next_number, char *fake_class_name, int fake_class_name_len);

	/**
	 * Add a library as loaded.
	 */
	void AddLoadedLibrary(const char *library_name, const char *fake_class_name);
};

#endif /* AI_CONTROLLER_HPP */
