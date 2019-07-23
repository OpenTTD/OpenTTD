/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

 /** @file social_presence.cpp Base implementation of social presence support. */

#include "../stdafx.h"
#include <string>
#include "social_presence.h"
#include "social_plugin_api.h"


static bool _social_loaded = false;
static OpenTTD_SocialPluginApi _social_api{};
static OpenTTD_SocialPluginCallbacks _social_callbacks{};

static struct {
	std::string server_name;
	std::string server_cookie;
} _social_multiplayer_status;

/* Implemented by platform */
extern OpenTTD_SocialPluginInit SocialLoadPlugin();


static void Callback_handle_join_request(void *join_request_cookie, const char *friend_name)
{
	SocialHandleJoinRequest(join_request_cookie, friend_name);
}

static void Callback_cancel_join_request(void *join_request_cookie)
{
	SocialCancelJoinRequest(join_request_cookie);
}

static void Callback_join_requested_game(const char *server_cookie)
{
	SocialJoinRequestedGame(server_cookie);
}


void SocialStartup()
{
	if (_social_loaded) return;

	OpenTTD_SocialPluginInit init = SocialLoadPlugin();

	_social_callbacks.handle_join_request = Callback_handle_join_request;
	_social_callbacks.cancel_join_request = Callback_cancel_join_request;
	_social_callbacks.join_requested_game = Callback_join_requested_game;

	if (init != nullptr) {
		_social_loaded = init(OTTD_SOCIAL_PLUGIN_API_VERSION, &_social_api, &_social_callbacks) != 0;
	}
}

void SocialShutdown()
{
	if (_social_loaded) _social_api.shutdown();
	_social_loaded = false;
}

void SocialEventLoop()
{
	if (_social_loaded) _social_api.event_loop();
}

void SocialEnterSingleplayer()
{
	if (_social_loaded) _social_api.enter_singleplayer();
}

void SocialBeginEnterMultiplayer(const std::string &server_name, const std::string &server_cookie)
{
	_social_multiplayer_status.server_name = server_name;
	_social_multiplayer_status.server_cookie = server_cookie;
}

void SocialCompleteEnterMultiplayer()
{
	if (_social_loaded && !_social_multiplayer_status.server_cookie.empty()) {
		_social_api.enter_multiplayer(_social_multiplayer_status.server_name.c_str(), _social_multiplayer_status.server_cookie.c_str());
	}
}

void SocialEnterCompany(const std::string &company_name, CompanyID company_id)
{
	if (_social_loaded) _social_api.enter_company(company_name.c_str(), company_id);
}

void SocialEnterSpectate()
{
	if (_social_loaded) _social_api.enter_spectate();
}

void SocialExitGameplay()
{
	if (_social_loaded) _social_api.exit_gameplay();
}

void SocialRespondJoinRequest(void *join_request_cookie, SocialJoinRequestResponse response)
{
	OpenTTD_SocialPluginApi_JoinRequestResponse api_rsp;
	switch (response) {
		case SocialJoinRequestResponse::Ignore: api_rsp = OTTD_JRR_IGNORE; break;
		case SocialJoinRequestResponse::Accept: api_rsp = OTTD_JRR_ACCEPT; break;
		case SocialJoinRequestResponse::Reject: api_rsp = OTTD_JRR_REJECT; break;
		default: return;
	}

	if (_social_loaded) _social_api.respond_join_request(join_request_cookie, api_rsp);
}
