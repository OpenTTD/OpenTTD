/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file debug.cpp Handling of printing debug messages. */

#include "stdafx.h"
#include <stdarg.h>
#include "console_func.h"
#include "debug.h"
#include "string_func.h"
#include "fileio_func.h"
#include "settings_type.h"

#if defined(WIN32) || defined(WIN64)
#include "os/windows/win32.h"
#endif

#include <time.h>

#if defined(ENABLE_NETWORK)
#include "network/network_admin.h"
SOCKET _debug_socket = INVALID_SOCKET;
#endif /* ENABLE_NETWORK */

#include "safeguards.h"

int _debug_driver_level;
int _debug_grf_level;
int _debug_map_level;
int _debug_misc_level;
int _debug_net_level;
int _debug_sprite_level;
int _debug_oldloader_level;
int _debug_npf_level;
int _debug_yapf_level;
int _debug_freetype_level;
int _debug_script_level;
int _debug_sl_level;
int _debug_gamelog_level;
int _debug_desync_level;
int _debug_console_level;
#ifdef RANDOM_DEBUG
int _debug_random_level;
#endif

uint32 _realtime_tick = 0;

struct DebugLevel {
	const char *name;
	int *level;
};

#define DEBUG_LEVEL(x) { #x, &_debug_##x##_level }
	static const DebugLevel debug_level[] = {
	DEBUG_LEVEL(driver),
	DEBUG_LEVEL(grf),
	DEBUG_LEVEL(map),
	DEBUG_LEVEL(misc),
	DEBUG_LEVEL(net),
	DEBUG_LEVEL(sprite),
	DEBUG_LEVEL(oldloader),
	DEBUG_LEVEL(npf),
	DEBUG_LEVEL(yapf),
	DEBUG_LEVEL(freetype),
	DEBUG_LEVEL(script),
	DEBUG_LEVEL(sl),
	DEBUG_LEVEL(gamelog),
	DEBUG_LEVEL(desync),
	DEBUG_LEVEL(console),
#ifdef RANDOM_DEBUG
	DEBUG_LEVEL(random),
#endif
	};
#undef DEBUG_LEVEL

/**
 * Dump the available debug facility names in the help text.
 * @param buf Start address for storing the output.
 * @param last Last valid address for storing the output.
 * @return Next free position in the output.
 */
char *DumpDebugFacilityNames(char *buf, char *last)
{
	size_t length = 0;
	for (const DebugLevel *i = debug_level; i != endof(debug_level); ++i) {
		if (length == 0) {
			buf = strecpy(buf, "List of debug facility names:\n", last);
		} else {
			buf = strecpy(buf, ", ", last);
			length += 2;
		}
		buf = strecpy(buf, i->name, last);
		length += strlen(i->name);
	}
	if (length > 0) {
		buf = strecpy(buf, "\n\n", last);
	}
	return buf;
}

/**
 * Internal function for outputting the debug line.
 * @param dbg Debug category.
 * @param buf Text line to output.
 */
static void debug_print(const char *dbg, const char *buf)
{
#if defined(ENABLE_NETWORK)
	if (_debug_socket != INVALID_SOCKET) {
		char buf2[1024 + 32];

		seprintf(buf2, lastof(buf2), "%sdbg: [%s] %s\n", GetLogPrefix(), dbg, buf);
		/* Sending out an error when this fails would be nice, however... the error
		 * would have to be send over this failing socket which won't work. */
		send(_debug_socket, buf2, (int)strlen(buf2), 0);
		return;
	}
#endif /* ENABLE_NETWORK */
	if (strcmp(dbg, "desync") == 0) {
		static FILE *f = FioFOpenFile("commands-out.log", "wb", AUTOSAVE_DIR);
		if (f == NULL) return;

		fprintf(f, "%s%s\n", GetLogPrefix(), buf);
		fflush(f);
#ifdef RANDOM_DEBUG
	} else if (strcmp(dbg, "random") == 0) {
		static FILE *f = FioFOpenFile("random-out.log", "wb", AUTOSAVE_DIR);
		if (f == NULL) return;

		fprintf(f, "%s\n", buf);
		fflush(f);
#endif
	} else {
		char buffer[512];
		seprintf(buffer, lastof(buffer), "%sdbg: [%s] %s\n", GetLogPrefix(), dbg, buf);
#if defined(WIN32) || defined(WIN64)
		TCHAR system_buf[512];
		convert_to_fs(buffer, system_buf, lengthof(system_buf), true);
		_fputts(system_buf, stderr);
#else
		fputs(buffer, stderr);
#endif
#ifdef ENABLE_NETWORK
		NetworkAdminConsole(dbg, buf);
#endif /* ENABLE_NETWORK */
		IConsoleDebug(dbg, buf);
	}
}

/**
 * Output a debug line.
 * @note Do not call directly, use the #DEBUG macro instead.
 * @param dbg Debug category.
 * @param format Text string a la printf, with optional arguments.
 */
void CDECL debug(const char *dbg, const char *format, ...)
{
	char buf[1024];

	va_list va;
	va_start(va, format);
	vseprintf(buf, lastof(buf), format, va);
	va_end(va);

	debug_print(dbg, buf);
}

/**
 * Set debugging levels by parsing the text in \a s.
 * For setting individual levels a string like \c "net=3,grf=6" should be used.
 * If the string starts with a number, the number is used as global debugging level.
 * @param s Text describing the wanted debugging levels.
 */
void SetDebugString(const char *s)
{
	int v;
	char *end;
	const char *t;

	/* global debugging level? */
	if (*s >= '0' && *s <= '9') {
		const DebugLevel *i;

		v = strtoul(s, &end, 0);
		s = end;

		for (i = debug_level; i != endof(debug_level); ++i) *i->level = v;
	}

	/* individual levels */
	for (;;) {
		const DebugLevel *i;
		int *p;

		/* skip delimiters */
		while (*s == ' ' || *s == ',' || *s == '\t') s++;
		if (*s == '\0') break;

		t = s;
		while (*s >= 'a' && *s <= 'z') s++;

		/* check debugging levels */
		p = NULL;
		for (i = debug_level; i != endof(debug_level); ++i) {
			if (s == t + strlen(i->name) && strncmp(t, i->name, s - t) == 0) {
				p = i->level;
				break;
			}
		}

		if (*s == '=') s++;
		v = strtoul(s, &end, 0);
		s = end;
		if (p != NULL) {
			*p = v;
		} else {
			ShowInfoF("Unknown debug level '%.*s'", (int)(s - t), t);
			return;
		}
	}
}

/**
 * Print out the current debug-level.
 * Just return a string with the values of all the debug categories.
 * @return string with debug-levels
 */
const char *GetDebugString()
{
	const DebugLevel *i;
	static char dbgstr[150];
	char dbgval[20];

	memset(dbgstr, 0, sizeof(dbgstr));
	i = debug_level;
	seprintf(dbgstr, lastof(dbgstr), "%s=%d", i->name, *i->level);

	for (i++; i != endof(debug_level); i++) {
		seprintf(dbgval, lastof(dbgval), ", %s=%d", i->name, *i->level);
		strecat(dbgstr, dbgval, lastof(dbgstr));
	}

	return dbgstr;
}

/**
 * Get the prefix for logs; if show_date_in_logs is enabled it returns
 * the date, otherwise it returns nothing.
 * @return the prefix for logs (do not free), never NULL
 */
const char *GetLogPrefix()
{
	static char _log_prefix[24];
	if (_settings_client.gui.show_date_in_logs) {
		time_t cur_time = time(NULL);
		strftime(_log_prefix, sizeof(_log_prefix), "[%Y-%m-%d %H:%M:%S] ", localtime(&cur_time));
	} else {
		*_log_prefix = '\0';
	}
	return _log_prefix;
}

