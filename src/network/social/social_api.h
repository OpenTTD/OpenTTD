/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file social_api.h Defines the plug-in interface for social platforms. */

#ifndef NETWORK_SOCIAL_API_H
#define NETWORK_SOCIAL_API_H

#ifdef __cplusplus
extern "C" {
#endif

struct OTTD_Social_Event_Server_Joined_Data {
	// Name of the server as shown to players.
	const char* server_name;

	// String representation of the invite code or IP address.
	const char* connection_string;
};

enum OTTD_Social_Event {
	// Called when the player has entered the main menu.
	// Parameter: N/A
	OTTD_SOCIAL_EVENT_MENU,

	// Called when the player loads a map in single player mode.
	// Parameter: N/A
	OTTD_SOCIAL_EVENT_SINGLE_PLAYER,

	// Called during server join.
	// Parameter: Relevant struct (OTTD_Social_Event_Server_Joined_Data*)
	OTTD_SOCIAL_EVENT_SERVER_JOINED,

	// Called during company allegiance changes.
	// Parameter Company name (const char *)
	// NULL if the player is just spectating.
	OTTD_SOCIAL_EVENT_COMPANY_CHANGED,
};

// Callback provided by OpenTTD for the implementation to allow joining a game.
typedef void (* OTTD_Social_JoinCallback)(const char* serverName);

// Initializes the plugin.
// The plugin is free to initialize the memory pointed to at the given address with any structure it needs to keep data around.
// The plugin loader will keep track of this memory for the plugin. It remains valid until the shutdown function is called.
//
// The callback function is a static reference to a function, however, the plugin should leverage its user data to keep track of it.
typedef bool (* OTTD_Social_Initialize)(OTTD_Social_JoinCallback callback, void **userdata);

// Called by the plugin loader to indicate that OpenTTD is currently shutting down.
// The plugin is responsible for freeing its user data, if it provided or used any.
typedef void (* OTTD_Social_Shutdown)(void *userdata);

// Called during the game loop to allow any plugin to pump its messages, if needed.
typedef void (* OTTD_Social_Dispatch)(void *userdata);

// Called when the game's state changes.
//
// The data pointed to by parameter is only valid during the duration of this function.
typedef void (* OTTD_Social_NewState)(OTTD_Social_Event event, void* parameter, void *userdata);

#ifdef __cplusplus
}
#endif

#endif
