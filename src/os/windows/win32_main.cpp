/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file win32_main.cpp Implementation main for Windows. */

#include "../../stdafx.h"
#include <windows.h>
#include <mmsystem.h>
#include "../../openttd.h"
#include "../../core/random_func.hpp"
#include "../../string_func.h"
#include "../../crashlog.h"
#include "../../debug.h"

#include "../../safeguards.h"

static int ParseCommandLine(char *line, char **argv, int max_argc)
{
	int n = 0;

	do {
		/* skip whitespace */
		while (*line == ' ' || *line == '\t') line++;

		/* end? */
		if (*line == '\0') break;

		/* special handling when quoted */
		if (*line == '"') {
			argv[n++] = ++line;
			while (*line != '"') {
				if (*line == '\0') return n;
				line++;
			}
		} else {
			argv[n++] = line;
			while (*line != ' ' && *line != '\t') {
				if (*line == '\0') return n;
				line++;
			}
		}
		*line++ = '\0';
	} while (n != max_argc);

	return n;
}

void CreateConsole();

int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	/* Set system timer resolution to 1ms. */
	timeBeginPeriod(1);

	CrashLog::InitialiseCrashLog();

	/* Convert the command line to UTF-8. */
	std::string cmdline = FS2OTTD(GetCommandLine());

	/* Set the console codepage to UTF-8. */
	SetConsoleOutputCP(CP_UTF8);

#if defined(_DEBUG)
	CreateConsole();
#endif

	_set_error_mode(_OUT_TO_MSGBOX); // force assertion output to messagebox

	/* setup random seed to something quite random */
	SetRandomSeed(GetTickCount());

	char *argv[64]; // max 64 command line arguments
	int argc = ParseCommandLine(cmdline.data(), argv, lengthof(argv));

	/* Make sure our arguments contain only valid UTF-8 characters. */
	for (int i = 0; i < argc; i++) StrMakeValidInPlace(argv[i]);

	int ret = openttd_main(argc, argv);

	/* Restore system timer resolution. */
	timeEndPeriod(1);

	return ret;
}
