/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_info_docs.hpp Description of the functions an Script can/must provide in ScriptInfo. */

/* This file exists purely for doxygen purposes. */

/**
 * 'Abstract' class of the Scripts use to register themselves.
 *
 * @note This class is not part of the API. It is purely to document what
 *       Scripts must or can implemented to provide information to OpenTTD to
 *       base configuring/starting/loading the Script on.
 *
 * @note The required functions are also needed for Script Libraries, but in
 *       that case you extend ScriptLibrary. As such the information here can
 *       be used for libraries, but the information will not be shown in the
 *       GUI except for error/debug messages.
 *
 * @api ai game
 */
class ScriptInfo {
public:
	/**
	 * Gets the author name to be shown in the 'Available Scripts' window.
	 *
	 * @return The author name of the Script.
	 * @note This function is required.
	 */
	string GetAuthor();

	/**
	 * Gets the Scripts name. This is shown in the 'Available Scripts' window
	 * and at all other places where the Script is mentioned, like the debug
	 * window or OpenTTD's help message. The name is used to uniquely
	 * identify an Script within OpenTTD and this name is used in savegames
	 * and the configuration file.
	 *
	 * @return The name of the Script.
	 * @note This function is required.
	 * @note This name is not used as library name by ScriptController::Import,
	 * instead the name returned by #CreateInstance is used.
	 */
	string GetName();

	/**
	 * Gets a 4 ASCII character short name of the Script to uniquely
	 * identify it from other Scripts. The short name is primarily
	 * used as unique identifier for the content system.
	 * The content system uses besides the short name also the
	 * MD5 checksum of all the source files to uniquely identify
	 * a specific version of the Script.
	 *
	 * The short name must consist of precisely four ASCII
	 * characters, or more precisely four non-zero bytes.
	 *
	 * @return The name of the Script.
	 * @note This function is required.
	 */
	string GetShortName();

	/**
	 * Gets the description to be shown in the 'Available Scripts' window.
	 *
	 * @return The description for the Script.
	 * @note This function is required.
	 */
	string GetDescription();

	/**
	 * Gets the version of the Script. This is a number to (in theory)
	 * uniquely identify the versions of an Script. Generally the
	 * 'instance' of an Script with the highest version is chosen to
	 * be loaded.
	 *
	 * When OpenTTD finds, during starting, a duplicate Script with the
	 * same version number one is randomly chosen. So it is
	 * important that this number is regularly updated/incremented.
	 *
	 * @return The version number of the Script.
	 * @note This function is required.
	 */
	int GetVersion();

	/**
	 * Gets the lowest version of the Script that OpenTTD can still load
	 * the savegame of. In other words, from which version until this
	 * version can the Script load the savegames.
	 *
	 * If this function does not exist OpenTTD assumes it can only
	 * load savegames of this version. As such it will not upgrade
	 * to this version upon load.
	 *
	 * @return The lowest version number we load the savegame data.
	 * @note This function is optional.
	 */
	int MinVersionToLoad();

	/**
	 * Gets the development/release date of the Script.
	 *
	 * The intention of this is to give the user an idea how old the
	 * Script is and whether there might be a newer version.
	 *
	 * @return The development/release date for the Script.
	 * @note This function is required.
	 */
	string GetDate();

	/**
	 * Can this Script be used as random Script?
	 *
	 * The idea behind this function is to 'forbid' highly
	 * competitive or other special Scripts from running in games unless
	 * the user explicitly selects the Script to be loaded. This to
	 * try to prevent users from complaining that the Script is too
	 * aggressive or does not build profitable routes.
	 *
	 * If this function does not exist OpenTTD assumes the Script can
	 * be used as random Script. As such it will be randomly chosen.
	 *
	 * @return True if the Script can be used as random Script.
	 * @note This function is optional.
	 *
	 * @api -game
	 */
	bool UseAsRandomAI();

	/**
	 * Can a non-developer select Script for a new game.
	 *
	 * The idea behind this function is to 'forbid' using your script with a new
	 *  game if you for example specifically wrote it for a certain scenario.
	 *
	 * @return True if the Script can be selected from the GUI as non-developer.
	 * @note This function is optional. Default is false.
	 *
	 * @api -ai
	 */
	bool IsDeveloperOnly();

	/**
	 * Gets the name of main class of the Script so OpenTTD knows
	 * what class to instantiate. For libraries, this name is also
	 * used when other scripts import it using ScriptController::Import.
	 *
	 * @return The class name of the Script.
	 * @note This function is required.
	 */
	string CreateInstance();

	/**
	 * Gets the API version this Script is written for. If this function
	 * does not exist API compatibility with version 0.7 is assumed.
	 * If the function returns something OpenTTD does not understand,
	 * for example a newer version or a string that is not a version,
	 * the Script will not be loaded.
	 *
	 * Although in the future we might need to make a separate
	 * compatibility 'wrapper' for a specific version of OpenTTD, for
	 * example '0.7.1', we will use only the major and minor number
	 * and not the bugfix number as valid return for this function.
	 *
	 * Valid return values are:
	 * - "0.7" (for AI only)
	 * - "1.0" (for AI only)
	 * - "1.1" (for AI only)
	 * - "1.2" (for both AI and GS)
	 * - "1.3" (for both AI and GS)
	 *
	 * @return The version this Script is compatible with.
	 */
	string GetAPIVersion();

	/**
	 * Gets the URL to be shown in the 'this Script has crashed' message
	 * and in the 'Available Scripts' window. If this function does not
	 * exist no URL will be shown.
	 *
	 * This function purely exists to redirect users of the Script to the
	 * right place on the internet to discuss the Script and report bugs
	 * of this Script.
	 *
	 * @return The URL to show.
	 * @note This function is optional.
	 */
	string GetURL();

	/**
	 * Gets the settings that OpenTTD shows in the "Script Parameters" window
	 * so the user can customize the Script. This is a special function that
	 * doesn't need to return anything. Instead you can call AddSetting
	 * and AddLabels here.
	 *
	 * @note This function is optional.
	 */
	void GetSettings();

	/** Miscellaneous flags for Script settings. */
	enum ScriptConfigFlags {
		CONFIG_NONE,      ///< Normal setting.
		CONFIG_RANDOM,    ///< When randomizing the Script, pick any value between min_value and max_value (inclusive).
		CONFIG_BOOLEAN,   ///< This value is a boolean (either 0 (false) or 1 (true) ).
		CONFIG_INGAME,    ///< This setting can be changed while the Script is running.
		CONFIG_DEVELOPER, ///< This setting will only be visible when the Script development tools are active.
	};

	/**
	 * Add a user configurable setting for this Script. You can call this
	 * as many times as you have settings.
	 * @param setting_description A table with all information about a
	 *  single setting. The table should have the following name/value pairs:
	 *  - name The name of the setting, this is used in openttd.cfg to
	 *    store the current configuration of Scripts. Required.
	 *  - description A single line describing the setting. Required.
	 *  - min_value The minimum value of this setting. Required for integer
	 *    settings and not allowed for boolean settings. The value will be
	 *    clamped in the range [MIN(int32_t), MAX(int32_t)] (inclusive).
	 *  - max_value The maximum value of this setting. Required for integer
	 *    settings and not allowed for boolean settings. The value will be
	 *    clamped in the range [MIN(int32_t), MAX(int32_t)] (inclusive).
	 *  - easy_value The default value if the easy difficulty level
	 *    is selected. Required. The value will be clamped in the range
	 *    [MIN(int32_t), MAX(int32_t)] (inclusive).
	 *  - medium_value The default value if the medium difficulty level
	 *    is selected. Required. The value will be clamped in the range
	 *    [MIN(int32_t), MAX(int32_t)] (inclusive).
	 *  - hard_value The default value if the hard difficulty level
	 *    is selected. Required. The value will be clamped in the range
	 *    [MIN(int32_t), MAX(int32_t)] (inclusive).
	 *  - custom_value The default value if the custom difficulty level
	 *    is selected. Required. The value will be clamped in the range
	 *    [MIN(int32_t), MAX(int32_t)] (inclusive).
	 *  - random_deviation If this property has a nonzero value, then the
	 *    actual value of the setting in game will be randomized in the range
	 *    [user_configured_value - random_deviation, user_configured_value + random_deviation] (inclusive).
	 *    random_deviation sign is ignored and the value is clamped in the range [0, MAX(int32_t)] (inclusive).
	 *    Not allowed if the CONFIG_RANDOM flag is set, otherwise optional.
	 *  - step_size The increase/decrease of the value every time the user
	 *    clicks one of the up/down arrow buttons. Optional, default is 1.
	 *  - flags Bitmask of some flags, see ScriptConfigFlags. Required.
	 *
	 * @note This is a function provided by OpenTTD, you don't have to
	 * include it in your Script but should just call it from GetSettings.
	 */
	void AddSetting(table setting_description);

	/**
	 * Add labels for the values of a setting. Instead of a number the
	 *  user will see the corresponding name.
	 * @param setting_name The name of the setting.
	 * @param value_names A table that maps values to names. The first
	 *   character of every identifier is ignored, the second character
	 *   could be '_' to indicate the value is negative, and the rest should
	 *   be an integer of the value you define a name for. The value
	 *   is a short description of that value.
	 * To define labels for a setting named "competition_level" you could
	 * for example call it like this:
	 * AddLabels("competition_level", {_0 = "no competition", _1 = "some competition",
	 * _2 = "a lot of competition"});
	 * Another example, for a setting with a negative value:
	 * AddLabels("amount", {__1 = "less than one", _0 = "none", _1 = "more than one"});
	 *
	 * @note This is a function provided by OpenTTD, you don't have to
	 * include it in your Script but should just call it from GetSettings.
	 */
	void AddLabels(const char *setting_name, table value_names);
};
