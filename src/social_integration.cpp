/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file social_integration.cpp Base implementation of social integration support. */

#include "stdafx.h"

#include "social_integration.h"
#include "3rdparty/openttd_social_integration_api/openttd_social_integration_api.h"

#include "debug.h"
#include "fileio_func.h"
#include "library_loader.h"
#include "rev.h"
#include "string_func.h"
#include "signature.h"

#include "safeguards.h"

/**
 * Container to track information per plugin.
 */
class InternalSocialIntegrationPlugin {
public:
	InternalSocialIntegrationPlugin(const std::string &filename, const std::string &basepath) : library(nullptr), external(basepath)
	{
		openttd_info.openttd_version = _openttd_revision;

		if (!ValidateSignatureFile(fmt::format("{}.sig", filename))) {
			external.state = SocialIntegrationPlugin::INVALID_SIGNATURE;
			return;
		}

		this->library = std::make_unique<LibraryLoader>(filename);
	}

	OpenTTD_SocialIntegration_v1_PluginInfo plugin_info = {}; ///< Information supplied by plugin.
	OpenTTD_SocialIntegration_v1_PluginApi plugin_api = {}; ///< API supplied by plugin.
	OpenTTD_SocialIntegration_v1_OpenTTDInfo openttd_info = {}; ///< Information supplied by OpenTTD.

	std::unique_ptr<LibraryLoader> library = nullptr; ///< Library handle.

	SocialIntegrationPlugin external; ///< Information of the plugin to be used by other parts of our codebase.
};

static std::vector<std::unique_ptr<InternalSocialIntegrationPlugin>> _plugins; ///< List of loaded plugins.
static std::set<std::string> _loaded_social_platform; ///< List of Social Platform plugins already loaded. Used to prevent loading a plugin for the same Social Platform twice.

/** Helper for scanning for files with SocialIntegration as extension */
class SocialIntegrationFileScanner : FileScanner {
public:
	void Scan()
	{
#ifdef _WIN32
		std::string extension = "-social.dll";
#elif defined(__APPLE__)
		std::string extension = "-social.dylib";
#else
		std::string extension = "-social.so";
#endif

		this->FileScanner::Scan(extension.c_str(), SOCIAL_INTEGRATION_DIR, false);
	}

	bool AddFile(const std::string &filename, size_t basepath_length, const std::string &) override
	{
		std::string basepath = filename.substr(basepath_length);
		Debug(misc, 1, "[Social Integration: {}] Loading ...", basepath);

		auto &plugin = _plugins.emplace_back(std::make_unique<InternalSocialIntegrationPlugin>(filename, basepath));

		/* Validation failed, so no library was loaded. */
		if (plugin->library == nullptr) {
			return false;
		}

		if (plugin->library->HasError()) {
			plugin->external.state = SocialIntegrationPlugin::FAILED;

			Debug(misc, 0, "[Social Integration: {}] Failed to load library: {}", basepath, plugin->library->GetLastError());
			return false;
		}

		OpenTTD_SocialIntegration_v1_GetInfo getinfo_func = plugin->library->GetFunction("SocialIntegration_v1_GetInfo");
		if (plugin->library->HasError()) {
			plugin->external.state = SocialIntegrationPlugin::UNSUPPORTED_API;

			Debug(misc, 0, "[Social Integration: {}] Failed to find symbol SocialPlugin_v1_GetInfo: {}", basepath, plugin->library->GetLastError());
			return false;
		}

		OpenTTD_SocialIntegration_v1_Init init_func = plugin->library->GetFunction("SocialIntegration_v1_Init");
		if (plugin->library->HasError()) {
			plugin->external.state = SocialIntegrationPlugin::UNSUPPORTED_API;

			Debug(misc, 0, "[Social Integration: {}] Failed to find symbol SocialPlugin_v1_Init: {}", basepath, plugin->library->GetLastError());
			return false;
		}

		getinfo_func(&plugin->plugin_info);
		/* Setup the information for the outside world to see. */
		plugin->external.social_platform = plugin->plugin_info.social_platform;
		plugin->external.name = plugin->plugin_info.name;
		plugin->external.version = plugin->plugin_info.version;

		/* Lowercase the string for comparison. */
		std::string lc_social_platform = plugin->plugin_info.social_platform;
		strtolower(lc_social_platform);

		/* Prevent more than one plugin for a certain Social Platform to be loaded, as that never ends well. */
		if (_loaded_social_platform.find(lc_social_platform) != _loaded_social_platform.end()) {
			plugin->external.state = SocialIntegrationPlugin::DUPLICATE;

			Debug(misc, 0, "[Social Integration: {}] Another plugin for {} is already loaded", basepath, plugin->plugin_info.social_platform);
			return false;
		}
		_loaded_social_platform.insert(lc_social_platform);

		auto state = init_func(&plugin->plugin_api, &plugin->openttd_info);
		switch (state) {
			case OTTD_SOCIAL_INTEGRATION_V1_INIT_SUCCESS:
				plugin->external.state = SocialIntegrationPlugin::RUNNING;

				Debug(misc, 1, "[Social Integration: {}] Loaded for {}: {} ({})", basepath, plugin->plugin_info.social_platform, plugin->plugin_info.name, plugin->plugin_info.version);
				return true;

			case OTTD_SOCIAL_INTEGRATION_V1_INIT_FAILED:
				plugin->external.state = SocialIntegrationPlugin::FAILED;

				Debug(misc, 0, "[Social Integration: {}] Failed to initialize", basepath);
				return false;

			case OTTD_SOCIAL_INTEGRATION_V1_INIT_PLATFORM_NOT_RUNNING:
				plugin->external.state = SocialIntegrationPlugin::PLATFORM_NOT_RUNNING;

				Debug(misc, 1, "[Social Integration: {}] Failed to initialize: {} is not running", basepath, plugin->plugin_info.social_platform);
				return false;

			default:
				NOT_REACHED();
		}
	}
};

std::vector<SocialIntegrationPlugin *> SocialIntegration::GetPlugins()
{
	std::vector<SocialIntegrationPlugin *> plugins;

	for (auto &plugin : _plugins) {
		plugins.push_back(&plugin->external);
	}

	return plugins;
}

void SocialIntegration::Initialize()
{
	SocialIntegrationFileScanner fs;
	fs.Scan();
}

/**
 * Wrapper to call a function pointer of a plugin if it isn't a nullptr.
 *
 * @param plugin Plugin to call the function pointer on.
 * @param func   Function pointer to call.
 */
template <typename T, typename... Ts>
static void PluginCall(std::unique_ptr<InternalSocialIntegrationPlugin> &plugin, T func, Ts... args)
{
	if (plugin->external.state != SocialIntegrationPlugin::RUNNING) {
		return;
	}

	if (func != nullptr) {
		func(args...);
	}
}

void SocialIntegration::Shutdown()
{
	for (auto &plugin : _plugins) {
		PluginCall(plugin, plugin->plugin_api.shutdown);
	}

	_plugins.clear();
	_loaded_social_platform.clear();
}

void SocialIntegration::RunCallbacks()
{
	for (auto &plugin : _plugins) {
		if (plugin->external.state != SocialIntegrationPlugin::RUNNING) {
			continue;
		}

		if (plugin->plugin_api.run_callbacks != nullptr) {
			if (!plugin->plugin_api.run_callbacks()) {
				Debug(misc, 1, "[Social Plugin: {}] Requested to be unloaded", plugin->external.basepath);

				_loaded_social_platform.erase(plugin->plugin_info.social_platform);
				plugin->external.state = SocialIntegrationPlugin::UNLOADED;
				PluginCall(plugin, plugin->plugin_api.shutdown);
			}
		}
	}
}

void SocialIntegration::EventEnterMainMenu()
{
	for (auto &plugin : _plugins) {
		PluginCall(plugin, plugin->plugin_api.event_enter_main_menu);
	}
}

void SocialIntegration::EventEnterScenarioEditor(uint map_width, uint map_height)
{
	for (auto &plugin : _plugins) {
		PluginCall(plugin, plugin->plugin_api.event_enter_scenario_editor, map_width, map_height);
	}
}

void SocialIntegration::EventEnterSingleplayer(uint map_width, uint map_height)
{
	for (auto &plugin : _plugins) {
		PluginCall(plugin, plugin->plugin_api.event_enter_singleplayer, map_width, map_height);
	}
}

void SocialIntegration::EventEnterMultiplayer(uint map_width, uint map_height)
{
	for (auto &plugin : _plugins) {
		PluginCall(plugin, plugin->plugin_api.event_enter_multiplayer, map_width, map_height);
	}
}

void SocialIntegration::EventJoiningMultiplayer()
{
	for (auto &plugin : _plugins) {
		PluginCall(plugin, plugin->plugin_api.event_joining_multiplayer);
	}
}
