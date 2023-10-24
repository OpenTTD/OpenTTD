/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file social_unix.cpp Unix like backing implementation for social plugin loading. */

#include "../../stdafx.h"
#include "../../fileio_func.h"
#include "../../string_func.h"
#include "../../network/social/loader.h"

#include <dlfcn.h>
#include <dirent.h>
#include <stdio.h>

#include "../../safeguards.h"

void LoadSocialPlatforms(std::vector<SocialPlatformPlugin>& plugins) {
	std::string search_dir = FioGetDirectory(SP_BINARY_DIR, BASE_DIR);

	DIR* directory = opendir(OTTD2FS(search_dir).c_str());
	if (directory == nullptr) {
		return;
	}

	while (true) {
		struct dirent* entry = readdir(directory);
		if (entry == nullptr) {
			break;
		}

		if (!StrEndsWith(FS2OTTD(entry->d_name), ".ots")) {
			continue;
		}

		std::string library_path = search_dir + FS2OTTD(entry->d_name);
		void* library = dlopen(library_path.c_str(), 0);
		if (library == nullptr) {
			continue;
		}

		SocialPlatformPlugin plugin = {
			library,

			(OTTD_Social_Initialize)dlsym(library, "OTTD_Social_Initialize"),
			(OTTD_Social_Shutdown)dlsym(library, "OTTD_Social_Shutdown"),
			(OTTD_Social_Dispatch)dlsym(library, "OTTD_Social_Dispatch"),
			(OTTD_Social_NewState)dlsym(library, "OTTD_Social_NewState"),

			nullptr
		};

		if (plugin.initialize == nullptr || plugin.shutdown == nullptr || plugin.dispatch == nullptr || plugin.newState == nullptr) {
			dlclose(library);
			continue;
		}

		plugins.push_back(plugin);
	}

	closedir(directory);
}
