/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file loader.cpp Loader code for social plugins. */

#include "../../stdafx.h"
#include "../network_func.h"
#include "loader.h"

void callback(const char* serverName) {
	NetworkClientConnectGame(serverName, COMPANY_SPECTATOR);
}

SocialPlatformLoader::SocialPlatformLoader() {
#if !defined(__EMSCRIPTEN__)
	LoadSocialPlatforms(this->plugins);
#endif

	for (SocialPlatformPlugin plugin : plugins) {
		plugin.initialize(callback, &plugin.userdata);
	}
}

void SocialPlatformLoader::Shutdown() {
	for (SocialPlatformPlugin plugin : plugins) {
		plugin.shutdown(plugin.userdata);
	}
}

void SocialPlatformLoader::RunDispatch() {
	for (SocialPlatformPlugin plugin : plugins) {
		plugin.dispatch(plugin.userdata);
	}
}

void SocialPlatformLoader::NewState(OTTD_Social_Event event, void* parameter) {
	for (SocialPlatformPlugin plugin : plugins) {
		plugin.newState(event, parameter, plugin.userdata);
	}
}

SocialPlatformLoader* SocialPlatformLoader::GetInstance() {
	static SocialPlatformLoader* loader = nullptr;

	if (loader == nullptr) {
		loader = new SocialPlatformLoader();
	}

	return loader;
}
