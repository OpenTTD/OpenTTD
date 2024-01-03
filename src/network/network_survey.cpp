/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file network_survey.cpp Opt-in survey part of the network protocol. */

#include "../stdafx.h"
#include "network_survey.h"
#include "settings_table.h"
#include "network.h"
#include "../debug.h"
#include "../survey.h"
#include "../3rdparty/fmt/chrono.h"
#include "../3rdparty/fmt/std.h"

#include "../safeguards.h"

extern std::string _savegame_id;

NetworkSurveyHandler _survey = {};

NLOHMANN_JSON_SERIALIZE_ENUM(NetworkSurveyHandler::Reason, {
	{NetworkSurveyHandler::Reason::PREVIEW, "preview"},
	{NetworkSurveyHandler::Reason::LEAVE, "leave"},
	{NetworkSurveyHandler::Reason::EXIT, "exit"},
	{NetworkSurveyHandler::Reason::CRASH, "crash"},
})

/**
 * Create the payload for the survey.
 *
 * @param reason The reason for sending the survey.
 * @param for_preview Whether the payload is meant for preview. This indents the result, and filters out the id/key.
 * @return std::string The JSON payload as string for the survey.
 */
std::string NetworkSurveyHandler::CreatePayload(Reason reason, bool for_preview)
{
	nlohmann::json survey;

	survey["schema"] = NETWORK_SURVEY_VERSION;
	survey["reason"] = reason;
	survey["id"] = _savegame_id;
	survey["date"] = fmt::format("{:%Y-%m-%d %H:%M:%S} (UTC)", fmt::gmtime(time(nullptr)));

#ifdef SURVEY_KEY
	/* We censor the key to avoid people trying to be "clever" and use it to send their own surveys. */
	survey["key"] = for_preview ? "(redacted)" : SURVEY_KEY;
#else
	survey["key"] = "";
#endif

	{
		auto &info = survey["info"];
		SurveyOS(info["os"]);
		SurveyOpenTTD(info["openttd"]);
		SurveyConfiguration(info["configuration"]);
		SurveyFont(info["font"]);
		SurveyCompiler(info["compiler"]);
		SurveyLibraries(info["libraries"]);
	}

	{
		auto &game = survey["game"];
		SurveyTimers(game["timers"]);
		SurveyCompanies(game["companies"]);
		SurveySettings(game["settings"], false);
		SurveyGrfs(game["grfs"]);
		SurveyGameScript(game["game_script"]);
	}

	/* For preview, we indent with 4 whitespaces to make things more readable. */
	int indent = for_preview ? 4 : -1;
	return survey.dump(indent);
}

/**
 * Transmit the survey.
 *
 * @param reason The reason for sending the survey.
 * @param blocking Whether to block until the survey is sent.
 */
void NetworkSurveyHandler::Transmit(Reason reason, bool blocking)
{
	if constexpr (!NetworkSurveyHandler::IsSurveyPossible()) {
		Debug(net, 4, "Survey: not possible to send survey; most likely due to missing JSON library at compile-time");
		return;
	}

	if (_settings_client.network.participate_survey != PS_YES) {
		Debug(net, 5, "Survey: user is not participating in survey; skipping survey");
		return;
	}

	Debug(net, 1, "Survey: sending survey results");
	NetworkHTTPSocketHandler::Connect(NetworkSurveyUriString(), this, this->CreatePayload(reason));

	if (blocking) {
		std::unique_lock<std::mutex> lock(this->mutex);
		/* Block no longer than 2 seconds. If we failed to send the survey in that time, so be it. */
		this->loaded.wait_for(lock, std::chrono::seconds(2));
	}
}

void NetworkSurveyHandler::OnFailure()
{
	Debug(net, 1, "Survey: failed to send survey results");
	this->loaded.notify_all();
}

void NetworkSurveyHandler::OnReceiveData(std::unique_ptr<char[]> data, size_t)
{
	if (data == nullptr) {
		Debug(net, 1, "Survey: survey results sent");
		this->loaded.notify_all();
	}
}
