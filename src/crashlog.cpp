/* $Id$ */

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
#include "date_func.h"
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
#include "language.h"
#include "fontcache.h"

#include "ai/ai_info.hpp"
#include "game/game.hpp"
#include "game/game_info.hpp"
#include "company_base.h"
#include "company_func.h"

#include <time.h>

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
#if defined(WITH_ICU_LAYOUT) || defined(WITH_ICU_SORT)
#	include <unicode/uversion.h>
#endif /* WITH_ICU_SORT || WITH_ICU_LAYOUT */
#ifdef WITH_LZMA
#	include <lzma.h>
#endif
#ifdef WITH_LZO
#include <lzo/lzo1x.h>
#endif
#ifdef WITH_SDL
#	include "sdl.h"
#	include <SDL.h>
#endif /* WITH_SDL */
#ifdef WITH_ZLIB
# include <zlib.h>
#endif

#include "safeguards.h"

/* static */ const char *CrashLog::message = NULL;
/* static */ char *CrashLog::gamelog_buffer = NULL;
/* static */ const char *CrashLog::gamelog_last = NULL;

char *CrashLog::LogCompiler(char *buffer, const char *last) const
{
			buffer += seprintf(buffer, last, " Compiler: "
#if defined(_MSC_VER)
			"MSVC %d", _MSC_VER
#elif defined(__ICC) && defined(__GNUC__)
			"ICC %d (GCC %d.%d.%d mode)", __ICC,  __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__
#elif defined(__ICC)
			"ICC %d", __ICC
#elif defined(__GNUC__)
			"GCC %d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__
#elif defined(__WATCOMC__)
			"WatcomC %d", __WATCOMC__
#else
			"<unknown>"
#endif
			);
#if defined(__VERSION__)
			return buffer + seprintf(buffer, last,  " \"" __VERSION__ "\"\n\n");
#else
			return buffer + seprintf(buffer, last,  "\n\n");
#endif
}

/* virtual */ char *CrashLog::LogRegisters(char *buffer, const char *last) const
{
	/* Stub implementation; not all OSes support this. */
	return buffer;
}

/* virtual */ char *CrashLog::LogModules(char *buffer, const char *last) const
{
	/* Stub implementation; not all OSes support this. */
	return buffer;
}

/**
 * Writes OpenTTD's version to the buffer.
 * @param buffer The begin where to write at.
 * @param last   The last position in the buffer to write to.
 * @return the position of the \c '\0' character after the buffer.
 */
char *CrashLog::LogOpenTTDVersion(char *buffer, const char *last) const
{
	return buffer + seprintf(buffer, last,
			"OpenTTD version:\n"
			" Version:    %s (%d)\n"
			" NewGRF ver: %08x\n"
			" Bits:       %d\n"
			" Endian:     %s\n"
			" Dedicated:  %s\n"
			" Build date: %s\n\n",
			_openttd_revision,
			_openttd_revision_modified,
			_openttd_newgrf_version,
#ifdef _SQ64
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
 * @param buffer The begin where to write at.
 * @param last   The last position in the buffer to write to.
 * @return the position of the \c '\0' character after the buffer.
 */
char *CrashLog::LogConfiguration(char *buffer, const char *last) const
{
	buffer += seprintf(buffer, last,
			"Configuration:\n"
			" Blitter:      %s\n"
			" Graphics set: %s (%u)\n"
			" Language:     %s\n"
			" Music driver: %s\n"
			" Music set:    %s (%u)\n"
			" Network:      %s\n"
			" Sound driver: %s\n"
			" Sound set:    %s (%u)\n"
			" Video driver: %s\n\n",
			BlitterFactory::GetCurrentBlitter() == NULL ? "none" : BlitterFactory::GetCurrentBlitter()->GetName(),
			BaseGraphics::GetUsedSet() == NULL ? "none" : BaseGraphics::GetUsedSet()->name,
			BaseGraphics::GetUsedSet() == NULL ? UINT32_MAX : BaseGraphics::GetUsedSet()->version,
			_current_language == NULL ? "none" : _current_language->file,
			MusicDriver::GetInstance() == NULL ? "none" : MusicDriver::GetInstance()->GetName(),
			BaseMusic::GetUsedSet() == NULL ? "none" : BaseMusic::GetUsedSet()->name,
			BaseMusic::GetUsedSet() == NULL ? UINT32_MAX : BaseMusic::GetUsedSet()->version,
			_networking ? (_network_server ? "server" : "client") : "no",
			SoundDriver::GetInstance() == NULL ? "none" : SoundDriver::GetInstance()->GetName(),
			BaseSounds::GetUsedSet() == NULL ? "none" : BaseSounds::GetUsedSet()->name,
			BaseSounds::GetUsedSet() == NULL ? UINT32_MAX : BaseSounds::GetUsedSet()->version,
			VideoDriver::GetInstance() == NULL ? "none" : VideoDriver::GetInstance()->GetName()
	);

	buffer += seprintf(buffer, last,
			"Fonts:\n"
			" Small:  %s\n"
			" Medium: %s\n"
			" Large:  %s\n"
			" Mono:   %s\n\n",
			FontCache::Get(FS_SMALL)->GetFontName(),
			FontCache::Get(FS_NORMAL)->GetFontName(),
			FontCache::Get(FS_LARGE)->GetFontName(),
			FontCache::Get(FS_MONO)->GetFontName()
	);

	buffer += seprintf(buffer, last, "AI Configuration (local: %i):\n", (int)_local_company);
	const Company *c;
	FOR_ALL_COMPANIES(c) {
		if (c->ai_info == NULL) {
			buffer += seprintf(buffer, last, " %2i: Human\n", (int)c->index);
		} else {
			buffer += seprintf(buffer, last, " %2i: %s (v%d)\n", (int)c->index, c->ai_info->GetName(), c->ai_info->GetVersion());
		}
	}

	if (Game::GetInfo() != NULL) {
		buffer += seprintf(buffer, last, " GS: %s (v%d)\n", Game::GetInfo()->GetName(), Game::GetInfo()->GetVersion());
	}
	buffer += seprintf(buffer, last, "\n");

	return buffer;
}

/**
 * Writes information (versions) of the used libraries.
 * @param buffer The begin where to write at.
 * @param last   The last position in the buffer to write to.
 * @return the position of the \c '\0' character after the buffer.
 */
char *CrashLog::LogLibraries(char *buffer, const char *last) const
{
	buffer += seprintf(buffer, last, "Libraries:\n");

#ifdef WITH_ALLEGRO
	buffer += seprintf(buffer, last, " Allegro:    %s\n", allegro_id);
#endif /* WITH_ALLEGRO */

#ifdef WITH_FONTCONFIG
	int version = FcGetVersion();
	buffer += seprintf(buffer, last, " FontConfig: %d.%d.%d\n", version / 10000, (version / 100) % 100, version % 100);
#endif /* WITH_FONTCONFIG */

#ifdef WITH_FREETYPE
	FT_Library library;
	int major, minor, patch;
	FT_Init_FreeType(&library);
	FT_Library_Version(library, &major, &minor, &patch);
	FT_Done_FreeType(library);
	buffer += seprintf(buffer, last, " FreeType:   %d.%d.%d\n", major, minor, patch);
#endif /* WITH_FREETYPE */

#if defined(WITH_ICU_LAYOUT) || defined(WITH_ICU_SORT)
	/* 4 times 0-255, separated by dots (.) and a trailing '\0' */
	char buf[4 * 3 + 3 + 1];
	UVersionInfo ver;
	u_getVersion(ver);
	u_versionToString(ver, buf);
#ifdef WITH_ICU_SORT
	buffer += seprintf(buffer, last, " ICU i18n:   %s\n", buf);
#endif
#ifdef WITH_ICU_LAYOUT
	buffer += seprintf(buffer, last, " ICU lx:     %s\n", buf);
#endif
#endif /* WITH_ICU_SORT || WITH_ICU_LAYOUT */

#ifdef WITH_LZMA
	buffer += seprintf(buffer, last, " LZMA:       %s\n", lzma_version_string());
#endif

#ifdef WITH_LZO
	buffer += seprintf(buffer, last, " LZO:        %s\n", lzo_version_string());
#endif

#ifdef WITH_PNG
	buffer += seprintf(buffer, last, " PNG:        %s\n", png_get_libpng_ver(NULL));
#endif /* WITH_PNG */

#ifdef WITH_SDL
#ifdef DYNAMICALLY_LOADED_SDL
	if (SDL_CALL SDL_Linked_Version != NULL) {
#else
	{
#endif
		const SDL_version *v = SDL_CALL SDL_Linked_Version();
		buffer += seprintf(buffer, last, " SDL:        %d.%d.%d\n", v->major, v->minor, v->patch);
	}
#endif /* WITH_SDL */

#ifdef WITH_ZLIB
	buffer += seprintf(buffer, last, " Zlib:       %s\n", zlibVersion());
#endif

	buffer += seprintf(buffer, last, "\n");
	return buffer;
}

/**
 * Helper function for printing the gamelog.
 * @param s the string to print.
 */
/* static */ void CrashLog::GamelogFillCrashLog(const char *s)
{
	CrashLog::gamelog_buffer += seprintf(CrashLog::gamelog_buffer, CrashLog::gamelog_last, "%s\n", s);
}

/**
 * Writes the gamelog data to the buffer.
 * @param buffer The begin where to write at.
 * @param last   The last position in the buffer to write to.
 * @return the position of the \c '\0' character after the buffer.
 */
char *CrashLog::LogGamelog(char *buffer, const char *last) const
{
	CrashLog::gamelog_buffer = buffer;
	CrashLog::gamelog_last = last;
	GamelogPrint(&CrashLog::GamelogFillCrashLog);
	return CrashLog::gamelog_buffer + seprintf(CrashLog::gamelog_buffer, last, "\n");
}

/**
 * Fill the crash log buffer with all data of a crash log.
 * @param buffer The begin where to write at.
 * @param last   The last position in the buffer to write to.
 * @return the position of the \c '\0' character after the buffer.
 */
char *CrashLog::FillCrashLog(char *buffer, const char *last) const
{
	time_t cur_time = time(NULL);
	buffer += seprintf(buffer, last, "*** OpenTTD Crash Report ***\n\n");
	buffer += seprintf(buffer, last, "Crash at: %s", asctime(gmtime(&cur_time)));

	YearMonthDay ymd;
	ConvertDateToYMD(_date, &ymd);
	buffer += seprintf(buffer, last, "In game date: %i-%02i-%02i (%i)\n\n", ymd.year, ymd.month + 1, ymd.day, _date_fract);

	buffer = this->LogError(buffer, last, CrashLog::message);
	buffer = this->LogOpenTTDVersion(buffer, last);
	buffer = this->LogRegisters(buffer, last);
	buffer = this->LogStacktrace(buffer, last);
	buffer = this->LogOSVersion(buffer, last);
	buffer = this->LogCompiler(buffer, last);
	buffer = this->LogConfiguration(buffer, last);
	buffer = this->LogLibraries(buffer, last);
	buffer = this->LogModules(buffer, last);
	buffer = this->LogGamelog(buffer, last);

	buffer += seprintf(buffer, last, "*** End of OpenTTD Crash Report ***\n");
	return buffer;
}

/**
 * Write the crash log to a file.
 * @note On success the filename will be filled with the full path of the
 *       crash log file. Make sure filename is at least \c MAX_PATH big.
 * @param buffer The begin of the buffer to write to the disk.
 * @param filename      Output for the filename of the written file.
 * @param filename_last The last position in the filename buffer.
 * @return true when the crash log was successfully written.
 */
bool CrashLog::WriteCrashLog(const char *buffer, char *filename, const char *filename_last) const
{
	seprintf(filename, filename_last, "%scrash.log", _personal_dir);

	FILE *file = FioFOpenFile(filename, "w", NO_DIRECTORY);
	if (file == NULL) return false;

	size_t len = strlen(buffer);
	size_t written = fwrite(buffer, 1, len, file);

	FioFCloseFile(file);
	return len == written;
}

/* virtual */ int CrashLog::WriteCrashDump(char *filename, const char *filename_last) const
{
	/* Stub implementation; not all OSes support this. */
	return 0;
}

/**
 * Write the (crash) savegame to a file.
 * @note On success the filename will be filled with the full path of the
 *       crash save file. Make sure filename is at least \c MAX_PATH big.
 * @param filename      Output for the filename of the written file.
 * @param filename_last The last position in the filename buffer.
 * @return true when the crash save was successfully made.
 */
bool CrashLog::WriteSavegame(char *filename, const char *filename_last) const
{
	/* If the map array doesn't exist, saving will fail too. If the map got
	 * initialised, there is a big chance the rest is initialised too. */
	if (_m == NULL) return false;

	try {
		GamelogEmergency();

		seprintf(filename, filename_last, "%scrash.sav", _personal_dir);

		/* Don't do a threaded saveload. */
		return SaveOrLoad(filename, SLO_SAVE, DFT_GAME_FILE, NO_DIRECTORY, false) == SL_OK;
	} catch (...) {
		return false;
	}
}

/**
 * Write the (crash) screenshot to a file.
 * @note On success the filename will be filled with the full path of the
 *       screenshot. Make sure filename is at least \c MAX_PATH big.
 * @param filename      Output for the filename of the written file.
 * @param filename_last The last position in the filename buffer.
 * @return true when the crash screenshot was successfully made.
 */
bool CrashLog::WriteScreenshot(char *filename, const char *filename_last) const
{
	/* Don't draw when we have invalid screen size */
	if (_screen.width < 1 || _screen.height < 1 || _screen.dst_ptr == NULL) return false;

	bool res = MakeScreenshot(SC_CRASHLOG, "crash");
	if (res) strecpy(filename, _full_screenshot_name, filename_last);
	return res;
}

/**
 * Makes the crash log, writes it to a file and then subsequently tries
 * to make a crash dump and crash savegame. It uses DEBUG to write
 * information like paths to the console.
 * @return true when everything is made successfully.
 */
bool CrashLog::MakeCrashLog() const
{
	/* Don't keep looping logging crashes. */
	static bool crashlogged = false;
	if (crashlogged) return false;
	crashlogged = true;

	char filename[MAX_PATH];
	char buffer[65536];
	bool ret = true;

	printf("Crash encountered, generating crash log...\n");
	this->FillCrashLog(buffer, lastof(buffer));
	printf("%s\n", buffer);
	printf("Crash log generated.\n\n");

	printf("Writing crash log to disk...\n");
	bool bret = this->WriteCrashLog(buffer, filename, lastof(filename));
	if (bret) {
		printf("Crash log written to %s. Please add this file to any bug reports.\n\n", filename);
	} else {
		printf("Writing crash log failed. Please attach the output above to any bug reports.\n\n");
		ret = false;
	}

	/* Don't mention writing crash dumps because not all platforms support it. */
	int dret = this->WriteCrashDump(filename, lastof(filename));
	if (dret < 0) {
		printf("Writing crash dump failed.\n\n");
		ret = false;
	} else if (dret > 0) {
		printf("Crash dump written to %s. Please add this file to any bug reports.\n\n", filename);
	}

	printf("Writing crash savegame...\n");
	bret = this->WriteSavegame(filename, lastof(filename));
	if (bret) {
		printf("Crash savegame written to %s. Please add this file and the last (auto)save to any bug reports.\n\n", filename);
	} else {
		ret = false;
		printf("Writing crash savegame failed. Please attach the last (auto)save to any bug reports.\n\n");
	}

	printf("Writing crash screenshot...\n");
	bret = this->WriteScreenshot(filename, lastof(filename));
	if (bret) {
		printf("Crash screenshot written to %s. Please add this file to any bug reports.\n\n", filename);
	} else {
		ret = false;
		printf("Writing crash screenshot failed.\n\n");
	}

	return ret;
}

/**
 * Sets a message for the error message handler.
 * @param message The error message of the error.
 */
/* static */ void CrashLog::SetErrorMessage(const char *message)
{
	CrashLog::message = message;
}

/**
 * Try to close the sound/video stuff so it doesn't keep lingering around
 * incorrect video states or so, e.g. keeping dpmi disabled.
 */
/* static */ void CrashLog::AfterCrashLogCleanup()
{
	if (MusicDriver::GetInstance() != NULL) MusicDriver::GetInstance()->Stop();
	if (SoundDriver::GetInstance() != NULL) SoundDriver::GetInstance()->Stop();
	if (VideoDriver::GetInstance() != NULL) VideoDriver::GetInstance()->Stop();
}
