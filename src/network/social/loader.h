/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file loader.h Social plugin loader class. */

#ifndef NETWORK_SOCIAL_LOADER_H
#define NETWORK_SOCIAL_LOADER_H

#include "social_api.h"

struct SocialPlatformPlugin {
	void* handle;

	OTTD_Social_Initialize initialize;
	OTTD_Social_Shutdown shutdown;
	OTTD_Social_Dispatch dispatch;
	OTTD_Social_NewState newState;

	void* userdata;
};

class SocialPlatformLoader {
public:
	void Shutdown();
	void RunDispatch();
	void NewState(OTTD_Social_Event event, void* parameter);

	static SocialPlatformLoader* GetInstance();

private:
	SocialPlatformLoader();

	std::vector<SocialPlatformPlugin> plugins;
};

#if !defined(__EMSCRIPTEN__)
/* Defined in os/<os>/social_<os>.cpp. */
void LoadSocialPlatforms(std::vector<SocialPlatformPlugin>& plugins);
#endif

#endif
