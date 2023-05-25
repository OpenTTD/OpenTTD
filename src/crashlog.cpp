/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file crashlog.cpp Implementation of generic function to be called to log a crash */

#include "stdafx.h"
#include "crashlog.h"
#include "gamelog.h"
#include "timer/timer_game_calendar.h"
#include "map_func.h"
#include "rev.h"
#include "strings_func.h"
#include "blitter/factory.hpp"
#include "base_media_base.h"
#include "music/music_driver.hpp"
#include "sound/sound_driver.hpp"
#include "video/video_driver.hpp"
#include "saveload/saveload.h"
#include "screenshot.h"
#include "gfx_func.h"
#include "network/network.h"
#include "network/network_survey.h"
#include "language.h"
#include "fontcache.h"
#include "news_gui.h"

#include "ai/ai_info.hpp"
#include "game/game.hpp"
#include "game/game_info.hpp"
#include "company_base.h"
#include "company_func.h"
#include "3rdparty/fmt/chrono.h"

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

/* static */ std::string CrashLog::message{ "<none>" };

void CrashLog::LogCompiler(std::back_insert_iterator<std::string> &output_iterator) const
{
	fmt::format_to(output_iterator, " Compiler: "
#if defined(_MSC_VER)
			"MSVC {}", _MSC_VER
#elif defined(__ICC) && defined(__GNUC__)
			"ICC {} (GCC {}.{}.{} mode)", __ICC,  __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__
#elif defined(__ICC)
			"ICC {}", __ICC
#elif defined(__GNUC__)
			"GCC {}.{}.{}", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__
#elif defined(__WATCOMC__)
			"WatcomC {}", __WATCOMC__
#else
			"<unknown>"
#endif
			);
#if defined(__VERSION__)
	fmt::format_to(output_iterator, " \"" __VERSION__ "\"\n\n");
#else
	fmt::format_to(output_iterator, "\n\n");
#endif
}

/* virtual */ void CrashLog::LogRegisters(std::back_insert_iterator<std::string> &output_iterator) const
{
	/* Stub implementation; not all OSes support this. */
}

/* virtual */ void CrashLog::LogModules(std::back_insert_iterator<std::string> &output_iterator) const
{
	/* Stub implementation; not all OSes support this. */
}

/**
 * Writes OpenTTD's version to the buffer.
 * @param output_iterator Iterator to write the output to.
 */
void CrashLog::LogOpenTTDVersion(std::back_insert_iterator<std::string> &output_iterator) const
{
	fmt::format_to(output_iterator,
			"OpenTTD version:\n"
			" Version:    {} ({})\n"
			" NewGRF ver: {:08x}\n"
			" Bits:       {}\n"
			" Endian:     {}\n"
			" Dedicated:  {}\n"
			" Build date: {}\n\n",
			_openttd_revision,
			_openttd_revision_modified,
			_openttd_newgrf_version,
#ifdef POINTER_IS_64BIT
			64,
#else
			32,
#endif
#if (TTD_ENDIAN == TTD_LITTLE_ENDIAN)
			"little",
#else
			"big",
#endif
#ifdef DEDICATED
			"yes",
#else
			"no",
#endif
			_openttd_build_date
	);
}

/**
 * Writes the (important) configuration settings to the buffer.
 * E.g. graphics set, sound set, blitter and AIs.
 * @param output_iterator Iterator to write the output to.
 */
void CrashLog::LogConfiguration(std::back_insert_iterator<std::string> &output_iterator) const
{
	fmt::format_to(output_iterator,
			"Configuration:\n"
			" Blitter:      {}\n"
			" Graphics set: {} ({})\n"
			" Language:     {}\n"
			" Music driver: {}\n"
			" Music set:    {} ({})\n"
			" Network:      {}\n"
			" Sound driver: {}\n"
			" Sound set:    {} ({})\n"
			" Video driver: {}\n\n",
			BlitterFactory::GetCurrentBlitter() == nullptr ? "none" : BlitterFactory::GetCurrentBlitter()->GetName(),
			BaseGraphics::GetUsedSet() == nullptr ? "none" : BaseGraphics::GetUsedSet()->name,
			BaseGraphics::GetUsedSet() == nullptr ? UINT32_MAX : BaseGraphics::GetUsedSet()->version,
			_current_language == nullptr ? "none" : _current_language->file,
			MusicDriver::GetInstance() == nullptr ? "none" : MusicDriver::GetInstance()->GetName(),
			BaseMusic::GetUsedSet() == nullptr ? "none" : BaseMusic::GetUsedSet()->name,
			BaseMusic::GetUsedSet() == nullptr ? UINT32_MAX : BaseMusic::GetUsedSet()->version,
			_networking ? (_network_server ? "server" : "client") : "no",
			SoundDriver::GetInstance() == nullptr ? "none" : SoundDriver::GetInstance()->GetName(),
			BaseSounds::GetUsedSet() == nullptr ? "none" : BaseSounds::GetUsedSet()->name,
			BaseSounds::GetUsedSet() == nullptr ? UINT32_MAX : BaseSounds::GetUsedSet()->version,
			VideoDriver::GetInstance() == nullptr ? "none" : VideoDriver::GetInstance()->GetInfoString()
	);

	fmt::format_to(output_iterator,
			"Fonts:\n"
			" Small:  {}\n"
			" Medium: {}\n"
			" Large:  {}\n"
			" Mono:   {}\n\n",
			FontCache::Get(FS_SMALL)->GetFontName(),
			FontCache::Get(FS_NORMAL)->GetFontName(),
			FontCache::Get(FS_LARGE)->GetFontName(),
			FontCache::Get(FS_MONO)->GetFontName()
	);

	fmt::format_to(output_iterator, "AI Configuration (local: {}) (current: {}):\n", _local_company, _current_company);
	for (const Company *c : Company::Iterate()) {
		if (c->ai_info == nullptr) {
			fmt::format_to(output_iterator, " {:2}: Human\n", c->index);
		} else {
			fmt::format_to(output_iterator, " {:2}: {} (v{})\n", (int)c->index, c->ai_info->GetName(), c->ai_info->GetVersion());
		}
	}

	if (Game::GetInfo() != nullptr) {
		fmt::format_to(output_iterator, " GS: {} (v{})\n", Game::GetInfo()->GetName(), Game::GetInfo()->GetVersion());
	}
	fmt::format_to(output_iterator, "\n");
}

/**
 * Writes information (versions) of the used libraries.
 * @param output_iterator Iterator to write the output to.
 */
void CrashLog::LogLibraries(std::back_insert_iterator<std::string> &output_iterator) const
{
	fmt::format_to(output_iterator, "Libraries:\n");

#ifdef WITH_ALLEGRO
	fmt::format_to(output_iterator, " Allegro:    {}\n", allegro_id);
#endif /* WITH_ALLEGRO */

#ifdef WITH_FONTCONFIG
	int version = FcGetVersion();
	fmt::format_to(output_iterator, " FontConfig: {}.{}.{}\n", version / 10000, (version / 100) % 100, version % 100);
#endif /* WITH_FONTCONFIG */

#ifdef WITH_FREETYPE
	FT_Library library;
	int major, minor, patch;
	FT_Init_FreeType(&library);
	FT_Library_Version(library, &major, &minor, &patch);
	FT_Done_FreeType(library);
	fmt::format_to(output_iterator, " FreeType:   {}.{}.{}\n", major, minor, patch);
#endif /* WITH_FREETYPE */

#if defined(WITH_HARFBUZZ)
	fmt::format_to(output_iterator, " HarfBuzz:   {}\n", hb_version_string());
#endif /* WITH_HARFBUZZ */

#if defined(WITH_ICU_I18N)
	/* 4 times 0-255, separated by dots (.) and a trailing '\0' */
	char buf[4 * 3 + 3 + 1];
	UVersionInfo ver;
	u_getVersion(ver);
	u_versionToString(ver, buf);
	fmt::format_to(output_iterator, " ICU i18n:   {}\n", buf);
#endif /* WITH_ICU_I18N */

#ifdef WITH_LIBLZMA
	fmt::format_to(output_iterator, " LZMA:       {}\n", lzma_version_string());
#endif

#ifdef WITH_LZO
	fmt::format_to(output_iterator, " LZO:        {}\n", lzo_version_string());
#endif

#ifdef WITH_PNG
	fmt::format_to(output_iterator, " PNG:        {}\n", png_get_libpng_ver(nullptr));
#endif /* WITH_PNG */

#ifdef WITH_SDL
	const SDL_version *sdl_v = SDL_Linked_Version();
	fmt::format_to(output_iterator, " SDL1:       {}.{}.{}\n", sdl_v->major, sdl_v->minor, sdl_v->patch);
#elif defined(WITH_SDL2)
	SDL_version sdl2_v;
	SDL_GetVersion(&sdl2_v);
	fmt::format_to(output_iterator, " SDL2:       {}.{}.{}\n", sdl2_v.major, sdl2_v.minor, sdl2_v.patch);
#endif

#ifdef WITH_ZLIB
	fmt::format_to(output_iterator, " Zlib:       {}\n", zlibVersion());
#endif

#ifdef WITH_CURL
	auto *curl_v = curl_version_info(CURLVERSION_NOW);
	fmt::format_to(output_iterator, " Curl:       {}\n", curl_v->version);
	if (curl_v->ssl_version != nullptr) {
		fmt::format_to(output_iterator, " Curl SSL:   {}\n", curl_v->ssl_version);
	} else {
		fmt::format_to(output_iterator, " Curl SSL:   none\n");
	}
#endif

	fmt::format_to(output_iterator, "\n");
}

/**
 * Writes the gamelog data to the buffer.
 * @param output_iterator Iterator to write the output to.
 */
void CrashLog::LogGamelog(std::back_insert_iterator<std::string> &output_iterator) const
{
	_gamelog.Print([&output_iterator](const std::string &s) {
		fmt::format_to(output_iterator, "{}\n", s);
	});
	fmt::format_to(output_iterator, "\n");
}

/**
 * Writes up to 32 recent news messages to the buffer, with the most recent first.
 * @param output_iterator Iterator to write the output to.
 */
void CrashLog::LogRecentNews(std::back_insert_iterator<std::string> &output_iterator) const
{
	fmt::format_to(output_iterator, "Recent news messages:\n");

	int i = 0;
	for (NewsItem *news = _latest_news; i < 32 && news != nullptr; news = news->prev, i++) {
		TimerGameCalendar::YearMonthDay ymd;
		TimerGameCalendar::ConvertDateToYMD(news->date, &ymd);
		fmt::format_to(output_iterator, "({}-{:02}-{:02}) StringID: {}, Type: {}, Ref1: {}, {}, Ref2: {}, {}\n",
		                   ymd.year, ymd.month + 1, ymd.day, news->string_id, news->type,
		                   news->reftype1, news->ref1, news->reftype2, news->ref2);
	}
	fmt::format_to(output_iterator, "\n");
}

/**
 * Create a timestamped filename.
 * @param ext           The extension for the filename.
 * @param with_dir      Whether to prepend the filename with the personal directory.
 * @return The filename
 */
std::string CrashLog::CreateFileName(const char *ext, bool with_dir) const
{
	static std::string crashname;

	if (crashname.empty()) {
		crashname = fmt::format("crash{:%Y%m%d%H%M%S}", fmt::gmtime(time(nullptr)));
	}
	return fmt::format("{}{}{}", with_dir ? _personal_dir : std::string{}, crashname, ext);
}

/**
 * Fill the crash log buffer with all data of a crash log.
 * @param output_iterator Iterator to write the output to.
 */
void CrashLog::FillCrashLog(std::back_insert_iterator<std::string> &output_iterator) const
{
	fmt::format_to(output_iterator, "*** OpenTTD Crash Report ***\n\n");
	fmt::format_to(output_iterator, "Crash at: {:%Y-%m-%d %H:%M:%S} (UTC)\n", fmt::gmtime(time(nullptr)));

	TimerGameCalendar::YearMonthDay ymd;
	TimerGameCalendar::ConvertDateToYMD(TimerGameCalendar::date, &ymd);
	fmt::format_to(output_iterator, "In game date: {}-{:02}-{:02} ({})\n\n", ymd.year, ymd.month + 1, ymd.day, TimerGameCalendar::date_fract);

	this->LogError(output_iterator, CrashLog::message);
	this->LogOpenTTDVersion(output_iterator);
	this->LogRegisters(output_iterator);
	this->LogStacktrace(output_iterator);
	this->LogOSVersion(output_iterator);
	this->LogCompiler(output_iterator);
	this->LogConfiguration(output_iterator);
	this->LogLibraries(output_iterator);
	this->LogModules(output_iterator);
	this->LogGamelog(output_iterator);
	this->LogRecentNews(output_iterator);

	fmt::format_to(output_iterator, "*** End of OpenTTD Crash Report ***\n");
}

/**
 * Write the crash log to a file.
 * @note The filename will be written to \c crashlog_filename.
 * @return true when the crash log was successfully written.
 */
bool CrashLog::WriteCrashLog()
{
	this->crashlog_filename = this->CreateFileName(".log");

	FILE *file = FioFOpenFile(this->crashlog_filename, "w", NO_DIRECTORY);
	if (file == nullptr) return false;

	size_t len = this->crashlog.size();
	size_t written = fwrite(this->crashlog.data(), 1, len, file);

	FioFCloseFile(file);
	return len == written;
}

/* virtual */ int CrashLog::WriteCrashDump()
{
	/* Stub implementation; not all OSes support this. */
	return 0;
}

/**
 * Write the (crash) savegame to a file.
 * @note The filename will be written to \c savegame_filename.
 * @return true when the crash save was successfully made.
 */
bool CrashLog::WriteSavegame()
{
	/* If the map doesn't exist, saving will fail too. If the map got
	 * initialised, there is a big chance the rest is initialised too. */
	if (!Map::IsInitialized()) return false;

	try {
		_gamelog.Emergency();

		this->savegame_filename = this->CreateFileName(".sav");

		/* Don't do a threaded saveload. */
		return SaveOrLoad(this->savegame_filename, SLO_SAVE, DFT_GAME_FILE, NO_DIRECTORY, false) == SL_OK;
	} catch (...) {
		return false;
	}
}

/**
 * Write the (crash) screenshot to a file.
 * @note The filename will be written to \c screenshot_filename.
 * @return std::nullopt when the crash screenshot could not be made, otherwise the filename.
 */
bool CrashLog::WriteScreenshot()
{
	/* Don't draw when we have invalid screen size */
	if (_screen.width < 1 || _screen.height < 1 || _screen.dst_ptr == nullptr) return false;

	std::string filename = this->CreateFileName("", false);
	bool res = MakeScreenshot(SC_CRASHLOG, filename);
	if (res) this->screenshot_filename = _full_screenshot_path;
	return res;
}

void CrashLog::SendSurvey() const
{
	if (_game_mode == GM_NORMAL) {
		_survey.Transmit(NetworkSurveyHandler::Reason::CRASH, true);
	}
}

/**
 * Makes the crash log, writes it to a file and then subsequently tries
 * to make a crash dump and crash savegame. It uses DEBUG to write
 * information like paths to the console.
 * @return true when everything is made successfully.
 */
bool CrashLog::MakeCrashLog()
{
	/* Don't keep looping logging crashes. */
	static bool crashlogged = false;
	if (crashlogged) return false;
	crashlogged = true;

	crashlog.reserve(65536);
	auto output_iterator = std::back_inserter(crashlog);
	bool ret = true;

	fmt::print("Crash encountered, generating crash log...\n");
	this->FillCrashLog(output_iterator);
	fmt::print("{}\n", crashlog);
	fmt::print("Crash log generated.\n\n");

	fmt::print("Writing crash log to disk...\n");
	bool bret = this->WriteCrashLog();
	if (bret) {
		fmt::print("Crash log written to {}. Please add this file to any bug reports.\n\n", this->crashlog_filename);
	} else {
		fmt::print("Writing crash log failed. Please attach the output above to any bug reports.\n\n");
		ret = false;
	}

	/* Don't mention writing crash dumps because not all platforms support it. */
	int dret = this->WriteCrashDump();
	if (dret < 0) {
		fmt::print("Writing crash dump failed.\n\n");
		ret = false;
	} else if (dret > 0) {
		fmt::print("Crash dump written to {}. Please add this file to any bug reports.\n\n", this->crashdump_filename);
	}

	fmt::print("Writing crash savegame...\n");
	bret = this->WriteSavegame();
	if (bret) {
		fmt::print("Crash savegame written to {}. Please add this file and the last (auto)save to any bug reports.\n\n", this->savegame_filename);
	} else {
		ret = false;
		fmt::print("Writing crash savegame failed. Please attach the last (auto)save to any bug reports.\n\n");
	}

	fmt::print("Writing crash screenshot...\n");
	bret = this->WriteScreenshot();
	if (bret) {
		fmt::print("Crash screenshot written to {}. Please add this file to any bug reports.\n\n", this->screenshot_filename);
	} else {
		ret = false;
		fmt::print("Writing crash screenshot failed.\n\n");
	}

	this->SendSurvey();

	return ret;
}

/**
 * Sets a message for the error message handler.
 * @param message The error message of the error.
 */
/* static */ void CrashLog::SetErrorMessage(const std::string &message)
{
	CrashLog::message = message;
}

/**
 * Try to close the sound/video stuff so it doesn't keep lingering around
 * incorrect video states or so, e.g. keeping dpmi disabled.
 */
/* static */ void CrashLog::AfterCrashLogCleanup()
{
	if (MusicDriver::GetInstance() != nullptr) MusicDriver::GetInstance()->Stop();
	if (SoundDriver::GetInstance() != nullptr) SoundDriver::GetInstance()->Stop();
	if (VideoDriver::GetInstance() != nullptr) VideoDriver::GetInstance()->Stop();
}
