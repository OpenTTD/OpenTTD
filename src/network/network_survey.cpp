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
#include "../rev.h"
#include "../settings_type.h"
#include "../timer/timer_game_tick.h"

#include "../currency.h"
#include "../fontcache.h"
#include "../language.h"

#include "../ai/ai_info.hpp"
#include "../game/game.hpp"
#include "../game/game_info.hpp"

#include "../music/music_driver.hpp"
#include "../sound/sound_driver.hpp"
#include "../video/video_driver.hpp"

#include "../base_media_base.h"
#include "../blitter/factory.hpp"

#ifdef WITH_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#endif /* WITH_NLOHMANN_JSON */

#include "../safeguards.h"

extern std::string _savegame_id;

NetworkSurveyHandler _survey = {};

#ifdef WITH_NLOHMANN_JSON

NLOHMANN_JSON_SERIALIZE_ENUM(NetworkSurveyHandler::Reason, {
	{NetworkSurveyHandler::Reason::PREVIEW, "preview"},
	{NetworkSurveyHandler::Reason::LEAVE, "leave"},
	{NetworkSurveyHandler::Reason::EXIT, "exit"},
	{NetworkSurveyHandler::Reason::CRASH, "crash"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(GRFStatus, {
	{GRFStatus::GCS_UNKNOWN, "unknown"},
	{GRFStatus::GCS_DISABLED, "disabled"},
	{GRFStatus::GCS_NOT_FOUND, "not found"},
	{GRFStatus::GCS_INITIALISED, "initialised"},
	{GRFStatus::GCS_ACTIVATED, "activated"},
})

static const std::string _vehicle_type_to_string[] = {
	"train",
	"roadveh",
	"ship",
	"aircraft",
};

/* Defined in one of the os/ survey files. */
extern void SurveyOS(nlohmann::json &json);

/**
 * List of all the generic setting tables.
 *
 * There are a few tables that are special and not processed like the rest:
 * - _currency_settings
 * - _misc_settings
 * - _company_settings
 * - _win32_settings
 * As such, they are not part of this list.
 */
static auto &GenericSettingTables()
{
	static const SettingTable _generic_setting_tables[] = {
		_difficulty_settings,
		_economy_settings,
		_game_settings,
		_gui_settings,
		_linkgraph_settings,
		_locale_settings,
		_multimedia_settings,
		_network_settings,
		_news_display_settings,
		_pathfinding_settings,
		_script_settings,
		_world_settings,
	};
	return _generic_setting_tables;
}

/**
 * Convert a settings table to JSON.
 *
 * @param survey The JSON object.
 * @param table The settings table to convert.
 * @param object The object to get the settings from.
 */
static void SurveySettingsTable(nlohmann::json &survey, const SettingTable &table, void *object)
{
	for (auto &desc : table) {
		const SettingDesc *sd = GetSettingDesc(desc);
		/* Skip any old settings we no longer save/load. */
		if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;

		auto name = sd->GetName();
		survey[name] = sd->FormatValue(object);
	}
}

/**
 * Convert settings to JSON.
 *
 * @param survey The JSON object.
 */
static void SurveySettings(nlohmann::json &survey)
{
	SurveySettingsTable(survey, _misc_settings, nullptr);
#if defined(_WIN32) && !defined(DEDICATED)
	SurveySettingsTable(survey, _win32_settings, nullptr);
#endif
	for (auto &table : GenericSettingTables()) {
		SurveySettingsTable(survey, table, &_settings_game);
	}
	SurveySettingsTable(survey, _currency_settings, &_custom_currency);
	SurveySettingsTable(survey, _company_settings, &_settings_client.company);
}

/**
 * Convert generic OpenTTD information to JSON.
 *
 * @param survey The JSON object.
 */
static void SurveyOpenTTD(nlohmann::json &survey)
{
	survey["version"] = std::string(_openttd_revision);
	survey["newgrf_version"] = _openttd_newgrf_version;
	survey["build_date"] = std::string(_openttd_build_date);
	survey["bits"] =
#ifdef POINTER_IS_64BIT
			64
#else
			32
#endif
		;
	survey["endian"] =
#if (TTD_ENDIAN == TTD_LITTLE_ENDIAN)
			"little"
#else
			"big"
#endif
		;
	survey["dedicated_build"] =
#ifdef DEDICATED
			"yes"
#else
			"no"
#endif
		;
}

/**
 * Convert generic game information to JSON.
 *
 * @param survey The JSON object.
 */
static void SurveyConfiguration(nlohmann::json &survey)
{
	survey["network"] = _networking ? (_network_server ? "server" : "client") : "no";
	if (_current_language != nullptr) {
		survey["language"]["filename"] = _current_language->file.filename().string();
		survey["language"]["name"] = _current_language->name;
		survey["language"]["isocode"] = _current_language->isocode;
	}
	if (BlitterFactory::GetCurrentBlitter() != nullptr) {
		survey["blitter"] = BlitterFactory::GetCurrentBlitter()->GetName();
	}
	if (MusicDriver::GetInstance() != nullptr) {
		survey["music_driver"] = MusicDriver::GetInstance()->GetName();
	}
	if (SoundDriver::GetInstance() != nullptr) {
		survey["sound_driver"] = SoundDriver::GetInstance()->GetName();
	}
	if (VideoDriver::GetInstance() != nullptr) {
		survey["video_driver"] = VideoDriver::GetInstance()->GetName();
		survey["video_info"] = VideoDriver::GetInstance()->GetInfoString();
	}
	if (BaseGraphics::GetUsedSet() != nullptr) {
		survey["graphics_set"] = fmt::format("{}.{}", BaseGraphics::GetUsedSet()->name, BaseGraphics::GetUsedSet()->version);
	}
	if (BaseMusic::GetUsedSet() != nullptr) {
		survey["music_set"] = fmt::format("{}.{}", BaseMusic::GetUsedSet()->name, BaseMusic::GetUsedSet()->version);
	}
	if (BaseSounds::GetUsedSet() != nullptr) {
		survey["sound_set"] = fmt::format("{}.{}", BaseSounds::GetUsedSet()->name, BaseSounds::GetUsedSet()->version);
	}
}

/**
 * Convert font information to JSON.
 *
 * @param survey The JSON object.
 */
static void SurveyFont(nlohmann::json &survey)
{
	survey["small"] = FontCache::Get(FS_SMALL)->GetFontName();
	survey["medium"] = FontCache::Get(FS_NORMAL)->GetFontName();
	survey["large"] = FontCache::Get(FS_LARGE)->GetFontName();
	survey["mono"] = FontCache::Get(FS_MONO)->GetFontName();
}

/**
 * Convert company information to JSON.
 *
 * @param survey The JSON object.
 */
static void SurveyCompanies(nlohmann::json &survey)
{
	for (const Company *c : Company::Iterate()) {
		auto &company = survey[std::to_string(c->index)];
		if (c->ai_info == nullptr) {
			company["type"] = "human";
		} else {
			company["type"] = "ai";
			company["script"] = fmt::format("{}.{}", c->ai_info->GetName(), c->ai_info->GetVersion());
		}

		for (VehicleType type = VEH_BEGIN; type < VEH_COMPANY_END; type++) {
			uint amount = c->group_all[type].num_vehicle;
			company["vehicles"][_vehicle_type_to_string[type]] = amount;
		}

		company["infrastructure"]["road"] = c->infrastructure.GetRoadTotal();
		company["infrastructure"]["tram"] = c->infrastructure.GetTramTotal();
		company["infrastructure"]["rail"] = c->infrastructure.GetRailTotal();
		company["infrastructure"]["signal"] = c->infrastructure.signal;
		company["infrastructure"]["water"] = c->infrastructure.water;
		company["infrastructure"]["station"] = c->infrastructure.station;
		company["infrastructure"]["airport"] = c->infrastructure.airport;
	}
}

/**
 * Convert GRF information to JSON.
 *
 * @param survey The JSON object.
 */
static void SurveyGrfs(nlohmann::json &survey)
{
	for (GRFConfig *c = _grfconfig; c != nullptr; c = c->next) {
		auto grfid = fmt::format("{:08x}", BSWAP32(c->ident.grfid));
		auto &grf = survey[grfid];

		grf["md5sum"] = FormatArrayAsHex(c->ident.md5sum);
		grf["status"] = c->status;

		if ((c->palette & GRFP_GRF_MASK) == GRFP_GRF_UNSET) grf["palette"] = "unset";
		if ((c->palette & GRFP_GRF_MASK) == GRFP_GRF_DOS) grf["palette"] = "dos";
		if ((c->palette & GRFP_GRF_MASK) == GRFP_GRF_WINDOWS) grf["palette"] = "windows";
		if ((c->palette & GRFP_GRF_MASK) == GRFP_GRF_ANY) grf["palette"] = "any";

		if ((c->palette & GRFP_BLT_MASK) == GRFP_BLT_UNSET) grf["blitter"] = "unset";
		if ((c->palette & GRFP_BLT_MASK) == GRFP_BLT_32BPP) grf["blitter"] = "32bpp";

		grf["is_static"] = HasBit(c->flags, GCF_STATIC);

		std::vector<uint32_t> parameters;
		for (int i = 0; i < c->num_params; i++) {
			parameters.push_back(c->param[i]);
		}
		grf["parameters"] = parameters;
	}
}

/**
 * Convert game-script information to JSON.
 *
 * @param survey The JSON object.
 */
static void SurveyGameScript(nlohmann::json &survey)
{
	if (Game::GetInfo() == nullptr) return;

	survey = fmt::format("{}.{}", Game::GetInfo()->GetName(), Game::GetInfo()->GetVersion());
}

/**
 * Change the bytes of memory into a textual version rounded up to the biggest unit.
 *
 * For example, 16751108096 would become 16 GiB.
 *
 * @param memory The bytes of memory.
 * @return std::string A textual representation.
 */
std::string SurveyMemoryToText(uint64_t memory)
{
	memory = memory / 1024; // KiB
	memory = CeilDiv(memory, 1024); // MiB

	/* Anything above 512 MiB we represent in GiB. */
	if (memory > 512) {
		return fmt::format("{} GiB", CeilDiv(memory, 1024));
	}

	/* Anything above 64 MiB we represent in a multiplier of 128 MiB. */
	if (memory > 64) {
		return fmt::format("{} MiB", Ceil(memory, 128));
	}

	/* Anything else in a multiplier of 4 MiB. */
	return fmt::format("{} MiB", Ceil(memory, 4));
}

#endif /* WITH_NLOHMANN_JSON */

/**
 * Create the payload for the survey.
 *
 * @param reason The reason for sending the survey.
 * @param for_preview Whether the payload is meant for preview. This indents the result, and filters out the id/key.
 * @return std::string The JSON payload as string for the survey.
 */
std::string NetworkSurveyHandler::CreatePayload(Reason reason, bool for_preview)
{
#ifndef WITH_NLOHMANN_JSON
	return "";
#else
	nlohmann::json survey;

	survey["schema"] = NETWORK_SURVEY_VERSION;
	survey["reason"] = reason;
	survey["id"] = _savegame_id;

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
	}

	{
		auto &game = survey["game"];
		game["ticks"] = TimerGameTick::counter;
		game["time"] = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - _switch_mode_time).count();
		SurveyCompanies(game["companies"]);
		SurveySettings(game["settings"]);
		SurveyGrfs(game["grfs"]);
		SurveyGameScript(game["game_script"]);
	}

	/* For preview, we indent with 4 whitespaces to make things more readable. */
	int indent = for_preview ? 4 : -1;
	return survey.dump(indent);
#endif /* WITH_NLOHMANN_JSON */
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

void NetworkSurveyHandler::OnReceiveData(const char *data, size_t length)
{
	if (data == nullptr) {
		Debug(net, 1, "Survey: survey results sent");
		this->loaded.notify_all();
	}
}
