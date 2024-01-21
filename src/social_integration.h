/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file social_integration.h Interface definitions for game to report/respond to social integration. */

#ifndef SOCIAL_INTEGRATION_H
#define SOCIAL_INTEGRATION_H

class SocialIntegrationPlugin {
public:
	enum State {
		RUNNING, ///< The plugin is successfully loaded and running.

		FAILED, ///< The plugin failed to initialize.
		PLATFORM_NOT_RUNNING, ///< The plugin failed to initialize because the Social Platform is not running.
		UNLOADED, ///< The plugin is unloaded upon request.
		DUPLICATE, ///< Another plugin of the same Social Platform is already loaded.
		UNSUPPORTED_API, ///< The plugin does not support the current API version.
	};

	std::string basepath; ///< Base path of the plugin.

	std::string social_platform = "unknown"; ///< Social platform this plugin is for.
	std::string name = ""; ///< Name of the plugin.
	std::string version = ""; ///< Version of the plugin.

	State state = FAILED; ///< Result of the plugin's init function.

	SocialIntegrationPlugin(const std::string &basepath) : basepath(basepath) {}
};

class SocialIntegration {
public:
	/**
	 * Get the list of loaded social integration plugins.
	 */
	static std::vector<SocialIntegrationPlugin *> GetPlugins();

	/**
	 * Initialize the social integration system, loading any social integration plugins that are available.
	 */
	static void Initialize();

	/**
	 * Shutdown the social integration system, and all social integration plugins that are loaded.
	 */
	static void Shutdown();

	/**
	 * Allow any social integration library to handle their own events.
	 */
	static void RunCallbacks();

	/**
	 * Event: user entered the main menu.
	 */
	static void EventEnterMainMenu();

	/**
	 * Event: user entered the Scenario Editor.
	 */
	static void EventEnterScenarioEditor(uint map_width, uint map_height);

	/**
	 * Event: user entered a singleplayer game.
	 */
	static void EventEnterSingleplayer(uint map_width, uint map_height);

	/**
	 * Event: user entered a multiplayer game.
	 */
	static void EventEnterMultiplayer(uint map_width, uint map_height);

	/**
	 * Event: user is joining a multiplayer game.
	 */
	static void EventJoiningMultiplayer();
};

#endif /* SOCIAL_INTEGRATION_H */
