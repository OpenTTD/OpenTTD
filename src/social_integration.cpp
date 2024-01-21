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

#include "safeguards.h"

/**
 * Container to track information per plugin.
 */
class InternalSocialIntegrationPlugin {
public:
	InternalSocialIntegrationPlugin(const std::string &filename, const std::string &basepath) : library(filename)
	{
		openttd_info.openttd_version = _openttd_revision;
	}

	OpenTTD_SocialIntegration_v1_PluginInfo plugin_info = {}; ///< Information supplied by plugin.
	OpenTTD_SocialIntegration_v1_PluginApi plugin_api = {}; ///< API supplied by plugin.
	OpenTTD_SocialIntegration_v1_OpenTTDInfo openttd_info = {}; ///< Information supplied by OpenTTD.

	LibraryLoader library; ///< Library handle.
};

static std::vector<std::unique_ptr<InternalSocialIntegrationPlugin>> _plugins; ///< List of loaded plugins.

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

		if (plugin->library.HasError()) {
			Debug(misc, 0, "[Social Integration: {}] Failed to load library: {}", basepath, plugin->library.GetLastError());
			return false;
		}

		OpenTTD_SocialIntegration_v1_GetInfo getinfo_func = plugin->library.GetFunction("SocialIntegration_v1_GetInfo");
		if (plugin->library.HasError()) {
			Debug(misc, 0, "[Social Integration: {}] Failed to find symbol SocialPlugin_v1_GetInfo: {}", basepath, plugin->library.GetLastError());
			return false;
		}

		OpenTTD_SocialIntegration_v1_Init init_func = plugin->library.GetFunction("SocialIntegration_v1_Init");
		if (plugin->library.HasError()) {
			Debug(misc, 0, "[Social Integration: {}] Failed to find symbol SocialPlugin_v1_Init: {}", basepath, plugin->library.GetLastError());
			return false;
		}

		getinfo_func(&plugin->plugin_info);

		auto state = init_func(&plugin->plugin_api, &plugin->openttd_info);
		switch (state) {
			case OTTD_SOCIAL_INTEGRATION_V1_INIT_SUCCESS:
				Debug(misc, 1, "[Social Integration: {}] Loaded for {}: {} ({})", basepath, plugin->plugin_info.social_platform, plugin->plugin_info.name, plugin->plugin_info.version);
				return true;

			case OTTD_SOCIAL_INTEGRATION_V1_INIT_FAILED:
				Debug(misc, 0, "[Social Integration: {}] Failed to initialize", basepath);
				return false;

			case OTTD_SOCIAL_INTEGRATION_V1_INIT_PLATFORM_NOT_RUNNING:
				Debug(misc, 1, "[Social Integration: {}] Failed to initialize: {} is not running", basepath, plugin->plugin_info.social_platform);
				return false;

			default:
				NOT_REACHED();
		}
	}
};

void SocialIntegration::Initialize()
{
	SocialIntegrationFileScanner fs;
	fs.Scan();
}

void SocialIntegration::Shutdown()
{
	_plugins.clear();
}
