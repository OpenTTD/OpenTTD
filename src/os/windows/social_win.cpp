/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file social_win.cpp Win32 backing implementation for social plugin loading. */

#include "../../stdafx.h"

#include "../../network/social/loader.h"
#include "../../fileio_func.h"

#include <thread>
#include <windows.h>

#include "../../safeguards.h"

void LoadSocialPlatforms(std::vector<SocialPlatformPlugin>& plugins) {
	std::string search_dir = FioGetDirectory(SP_BINARY_DIR, BASE_DIR);

	WIN32_FIND_DATAW find_data = {};
	HANDLE find_handle = FindFirstFileW(OTTD2FS(search_dir + "*.ots").c_str(), &find_data);
	if (find_handle != INVALID_HANDLE_VALUE) {
		do {
			std::string library_path = search_dir + FS2OTTD(find_data.cFileName);
			HMODULE library = LoadLibraryW(OTTD2FS(library_path).c_str());
			if (library == nullptr) {
				continue;
			}

			SocialPlatformPlugin plugin = {
				library,

				(OTTD_Social_Initialize)GetProcAddress(library, "OTTD_Social_Initialize"),
				(OTTD_Social_Shutdown)GetProcAddress(library, "OTTD_Social_Shutdown"),
				(OTTD_Social_Dispatch)GetProcAddress(library, "OTTD_Social_Dispatch"),
				(OTTD_Social_NewState)GetProcAddress(library, "OTTD_Social_NewState"),

				nullptr
			};

			if (plugin.initialize == nullptr || plugin.shutdown == nullptr || plugin.dispatch == nullptr || plugin.newState == nullptr) {
				FreeLibrary(library);
				continue;
			}

			plugins.push_back(plugin);
		} while (FindNextFileW(find_handle, &find_data));

		FindClose(find_handle);
	}
}
