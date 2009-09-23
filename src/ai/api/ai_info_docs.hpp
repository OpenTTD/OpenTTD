/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_info_docs.hpp Description of the functions an AI can/must provide in AIInfo. */

/* This file exists purely for doxygen purposes. */
#ifdef DOXYGEN_SKIP
/**
 * 'Abstract' class of the class AIs/AI libraries use to register themselves.
 *
 * @note This class is not part of the API. It is purely to document what
 *       AIs must or can implemented to provide information to OpenTTD to
 *       base configuring/starting/loading the AI on.
 *
 * @note The required functions are also needed for AI Libraries. As such
 *       the information here can be used for libraries, but the information
 *       will not be shown in the GUI except for error/debug messages.
 */
class AIInfo {
public:
	/**
	 * Gets the author name to be shown in the 'Available AIs' window.
	 *
	 * @return The author name of the AI.
	 * @note This function is required.
	 */
	string GetAuthor();

	/**
	 * Gets the AIs name. This is shown in the 'Available AIs' window
	 * and at all other places where the AI is mentioned, like the debug
	 * window or OpenTTD's help message. The name is used to uniquely
	 * identify an AI within OpenTTD and this name is used in savegames
	 * and the configuration file.
	 *
	 * @return The name of the AI.
	 * @note This function is required.
	 */
	string GetName();

	/**
	 * Gets a 4 ASCII character short name of the AI to uniquely
	 * identify it from other AIs. The short name is primarily
	 * used as unique identifier for the content system.
	 * The content system uses besides the short name also the
	 * MD5 checksum of all the source files to uniquely identify
	 * a specific version of the AI.
	 *
	 * The short name must consist of precisely four ASCII
	 * characters, or more precisely four non-zero bytes.
	 *
	 * @return The name of the AI.
	 * @note This function is required.
	 */
	string GetShortName();

	/**
	 * Gets the description to be shown in the 'Available AIs' window.
	 *
	 * @return The description for the AI.
	 * @note This function is required.
	 */
	string GetDescription();

	/**
	 * Gets the version of the AI. This is a number to (in theory)
	 * uniquely identify the versions of an AI. Generally the
	 * 'instance' of an AI with the highest version is chosen to
	 * be loaded.
	 *
	 * When OpenTTD finds, during starting, a duplicate AI with the
	 * same version number one is randomly chosen. So it is
	 * important that this number is regularly updated/incremented.
	 *
	 * @return The version number of the AI.
	 * @note This function is required.
	 */
	int GetVersion();

	/**
	 * Gets the lowest version of the AI that OpenTTD can still load
	 * the savegame of. In other words, from which version until this
	 * version can the AI load the savegames.
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
	 * Gets the development/release date of the AI.
	 *
	 * The intention of this is to give the user an idea how old the
	 * AI is and whether there might be a newer version.
	 *
	 * @return The development/release date for the AI.
	 * @note This function is required.
	 */
	string GetDate();

	/**
	 * Can this AI be used as random AI?
	 *
	 * The idea behind this function is to 'forbid' highly
	 * competitive or other special AIs from running in games unless
	 * the user explicitly selects the AI to be loaded. This to
	 * try to prevent users from complaining that the AI is too
	 * aggressive or does not build profitable routes.
	 *
	 * If this function does not exist OpenTTD assumes the AI can
	 * be used as random AI. As such it will be randomly chosen.
	 *
	 * @return True if the AI can be used as random AI.
	 * @note This function is optional.
	 */
	bool UseAsRandomAI();

	/**
	 * Gets the name of main class of the AI so OpenTTD knows
	 * what class to instantiate.
	 *
	 * @return The class name of the AI.
	 * @note This function is required.
	 */
	string CreateInstance();

	/**
	 * Gets the API version this AI is written for. If this function
	 * does not exist API compatability with version 0.7 is assumed.
	 * If the function returns something OpenTTD does not understand,
	 * for example a newer version or a string that is not a version,
	 * the AI will not be loaded.
	 *
	 * Although in the future we might need to make a separate
	 * compatability 'wrapper' for a specific version of OpenTTD, for
	 * example '0.7.1', we will use only the major and minor number
	 * and not the bugfix number as valid return for this function.
	 *
	 * Valid return values are:
	 * - "0.7"
	 * - "0.8"
	 *
	 * @return The version this AI is compatible with.
	 * @note This function is optional.
	 */
	string GetAPIVersion();

	/**
	 * Gets the URL to be shown in the 'this AI has crashed' message
	 * and in the 'Available AIs' window. If this function does not
	 * exist no URL will be shown.
	 *
	 * This function purely exists to redirect users of the AI to the
	 * right place on the internet to discuss the AI and report bugs
	 * of this AI.
	 *
	 * @return The URL to show.
	 * @note This function is optional.
	 */
	string GetURL();
};
#endif
