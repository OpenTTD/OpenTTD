/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file survey.cpp Functions to survey the current game / system, for crashlog and network-survey. */

#include "stdafx.h"

#include "survey.h"

#include "settings_table.h"
#include "network/network.h"
#include "rev.h"
#include "settings_type.h"
#include "timer/timer_game_tick.h"

#include "currency.h"
#include "fontcache.h"
#include "language.h"

#include "ai/ai_info.hpp"
#include "game/game.hpp"
#include "game/game_info.hpp"

#include "music/music_driver.hpp"
#include "sound/sound_driver.hpp"
#include "video/video_driver.hpp"

#include "base_media_base.h"
#include "blitter/factory.hpp"

#ifdef WITH_ALLEGRO
#	include <allegro.h>
#endif /* WITH_ALLEGRO */
#ifdef WITH_FONTCONFIG
#	include <fontconfig/fontconfig.h>
#endif /* WITH_FONTCONFIG */
#ifdef WITH_PNG
	/* pngconf.h, included by png.h doesn't like something in the
	 * freetype headers. As such it's not alphabetically sorted. */
#	include <png.h>
#endif /* WITH_PNG */
#ifdef WITH_FREETYPE
#	include <ft2build.h>
#	include FT_FREETYPE_H
#endif /* WITH_FREETYPE */
#ifdef WITH_HARFBUZZ
#	include <hb.h>
#endif /* WITH_HARFBUZZ */
#ifdef WITH_ICU_I18N
#	include <unicode/uversion.h>
#endif /* WITH_ICU_I18N */
#ifdef WITH_LIBLZMA
#	include <lzma.h>
#endif
#ifdef WITH_LZO
#include <lzo/lzo1x.h>
#endif
#if defined(WITH_SDL) || defined(WITH_SDL2)
#	include <SDL.h>
#endif /* WITH_SDL || WITH_SDL2 */
#ifdef WITH_ZLIB
# include <zlib.h>
#endif
#ifdef WITH_CURL
# include <curl/curl.h>
#endif

#include "safeguards.h"

NLOHMANN_JSON_SERIALIZE_ENUM(GRFStatus, {
	{GRFStatus::GCS_UNKNOWN, "unknown"},
	{GRFStatus::GCS_DISABLED, "disabled"},
	{GRFStatus::GCS_NOT_FOUND, "not found"},
	{GRFStatus::GCS_INITIALISED, "initialised"},
	{GRFStatus::GCS_ACTIVATED, "activated"},
})

/** Lookup table to convert a VehicleType to a string. */
static const std::string _vehicle_type_to_string[] = {
	"train",
	"roadveh",
	"ship",
	"aircraft",
};

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
 * @param skip_if_default If true, skip any settings that are on their default value.
 */
static void SurveySettingsTable(nlohmann::json &survey, const SettingTable &table, void *object, bool skip_if_default)
{
	for (auto &desc : table) {
		const SettingDesc *sd = GetSettingDesc(desc);
		/* Skip any old settings we no longer save/load. */
		if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;

		auto name = sd->GetName();
		if (skip_if_default && sd->IsDefaultValue(object)) continue;
		survey[name] = sd->FormatValue(object);
	}
}

/**
 * Convert settings to JSON.
 *
 * @param survey The JSON object.
 */
void SurveySettings(nlohmann::json &survey, bool skip_if_default)
{
	SurveySettingsTable(survey, _misc_settings, nullptr, skip_if_default);
#if defined(_WIN32) && !defined(DEDICATED)
	SurveySettingsTable(survey, _win32_settings, nullptr, skip_if_default);
#endif
	for (auto &table : GenericSettingTables()) {
		SurveySettingsTable(survey, table, &_settings_game, skip_if_default);
	}
	SurveySettingsTable(survey, _currency_settings, &_custom_currency, skip_if_default);
	SurveySettingsTable(survey, _company_settings, &_settings_client.company, skip_if_default);
}

/**
 * Convert compiler information to JSON.
 *
 * @param survey The JSON object.
 */
void SurveyCompiler(nlohmann::json &survey)
{
#if defined(_MSC_VER)
	survey["name"] = "MSVC";
	survey["version"] = _MSC_VER;
#elif defined(__ICC) && defined(__GNUC__)
	survey["name"] = "ICC";
	survey["version"] = __ICC;
#	if defined(__GNUC__)
		survey["extra"] = fmt::format("GCC {}.{}.{} mode", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#	endif
#elif defined(__GNUC__)
	survey["name"] = "GCC";
	survey["version"] = fmt::format("{}.{}.{}", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#else
	survey["name"] = "unknown";
#endif

#if defined(__VERSION__)
	survey["extra"] = __VERSION__;
#endif
}

/**
 * Convert generic OpenTTD information to JSON.
 *
 * @param survey The JSON object.
 */
void SurveyOpenTTD(nlohmann::json &survey)
{
	survey["version"]["revision"] = std::string(_openttd_revision);
	survey["version"]["modified"] = _openttd_revision_modified;
	survey["version"]["tagged"] = _openttd_revision_tagged;
	survey["version"]["hash"] = std::string(_openttd_revision_hash);
	survey["version"]["newgrf"] = fmt::format("{:X}", _openttd_newgrf_version);
	survey["version"]["content"] = std::string(_openttd_content_version);
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
void SurveyConfiguration(nlohmann::json &survey)
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
void SurveyFont(nlohmann::json &survey)
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
void SurveyCompanies(nlohmann::json &survey)
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
 * Convert timer information to JSON.
 *
 * @param survey The JSON object.
 */
void SurveyTimers(nlohmann::json &survey)
{
	survey["ticks"] = TimerGameTick::counter;
	survey["seconds"] = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - _switch_mode_time).count();

	TimerGameCalendar::YearMonthDay ymd = TimerGameCalendar::ConvertDateToYMD(TimerGameCalendar::date);
	survey["calendar"] = fmt::format("{:04}-{:02}-{:02} ({})", ymd.year, ymd.month + 1, ymd.day, TimerGameCalendar::date_fract);
}

/**
 * Convert GRF information to JSON.
 *
 * @param survey The JSON object.
 */
void SurveyGrfs(nlohmann::json &survey)
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
void SurveyGameScript(nlohmann::json &survey)
{
	if (Game::GetInfo() == nullptr) return;

	survey = fmt::format("{}.{}", Game::GetInfo()->GetName(), Game::GetInfo()->GetVersion());
}

/**
 * Convert compiled libraries information to JSON.
 *
 * @param survey The JSON object.
 */
void SurveyLibraries(nlohmann::json &survey)
{
#ifdef WITH_ALLEGRO
	survey["allegro"] = std::string(allegro_id);
#endif /* WITH_ALLEGRO */

#ifdef WITH_FONTCONFIG
	int version = FcGetVersion();
	survey["fontconfig"] = fmt::format("{}.{}.{}", version / 10000, (version / 100) % 100, version % 100);
#endif /* WITH_FONTCONFIG */

#ifdef WITH_FREETYPE
	FT_Library library;
	int major, minor, patch;
	FT_Init_FreeType(&library);
	FT_Library_Version(library, &major, &minor, &patch);
	FT_Done_FreeType(library);
	survey["freetype"] = fmt::format("{}.{}.{}", major, minor, patch);
#endif /* WITH_FREETYPE */

#if defined(WITH_HARFBUZZ)
	survey["harfbuzz"] = hb_version_string();
#endif /* WITH_HARFBUZZ */

#if defined(WITH_ICU_I18N)
	/* 4 times 0-255, separated by dots (.) and a trailing '\0' */
	char buf[4 * 3 + 3 + 1];
	UVersionInfo ver;
	u_getVersion(ver);
	u_versionToString(ver, buf);
	survey["icu_i18n"] = buf;
#endif /* WITH_ICU_I18N */

#ifdef WITH_LIBLZMA
	survey["lzma"] = lzma_version_string();
#endif

#ifdef WITH_LZO
	survey["lzo"] = lzo_version_string();
#endif

#ifdef WITH_PNG
	survey["png"] = png_get_libpng_ver(nullptr);
#endif /* WITH_PNG */

#ifdef WITH_SDL
	const SDL_version *sdl_v = SDL_Linked_Version();
	survey["sdl"] = fmt::format("{}.{}.{}", sdl_v->major, sdl_v->minor, sdl_v->patch);
#elif defined(WITH_SDL2)
	SDL_version sdl2_v;
	SDL_GetVersion(&sdl2_v);
	survey["sdl2"] = fmt::format("{}.{}.{}", sdl2_v.major, sdl2_v.minor, sdl2_v.patch);
#endif

#ifdef WITH_ZLIB
	survey["zlib"] = zlibVersion();
#endif

#ifdef WITH_CURL
	auto *curl_v = curl_version_info(CURLVERSION_NOW);
	survey["curl"] = curl_v->version;
	survey["curl_ssl"] = curl_v->ssl_version == nullptr ? "none" : curl_v->ssl_version;
#endif
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
