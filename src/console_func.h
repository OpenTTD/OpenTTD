/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file console_func.h Console functions used outside of the console code. */

#ifndef CONSOLE_FUNC_H
#define CONSOLE_FUNC_H

#include "console_type.h"

/* console modes */
extern IConsoleModes _iconsole_mode;

/* console functions */
void IConsoleInit();
void IConsoleFree();
void IConsoleClose();

/* console output */
void IConsolePrint(TextColour colour_code, const char *string);
void CDECL IConsolePrintF(TextColour colour_code, const char *format, ...) WARN_FORMAT(2, 3);
void CDECL IConsoleDebugF(const char *format, ...) WARN_FORMAT(1, 2);
void CDECL IConsoleInfoF(const char *format, ...) WARN_FORMAT(1, 2);
void CDECL IConsoleWarningF(const char *format, ...) WARN_FORMAT(1, 2);
void CDECL IConsoleErrorF(const char *format, ...) WARN_FORMAT(1, 2);

/**
 * It is possible to print debugging information to the console,
 * which is achieved by using this function. Can only be used by
 * debug() in debug.cpp. You need at least a level 2 (developer) for debugging
 * messages to show up
 * @param dbg debugging category
 * @param string debugging message
 */
static inline void IConsoleDebug(const char *dbg, const char *string)
{
	IConsoleDebugF("dbg: [%s] %s", dbg, string);
}

/**
 * It is possible to print informational messages to the console
 */
static inline void IConsoleInfo(const char *string)
{
	IConsoleInfoF("INFO: %s", string);
}

/**
 * It is possible to print warnings to the console. These are mostly
 * errors or mishaps, but non-fatal. You need at least a level 1 (developer) for
 * debugging messages to show up
 */
static inline void IConsoleWarning(const char *string)
{
	IConsoleWarningF("WARNING: %s", string);
}

/**
 * It is possible to print error information to the console. This can include
 * game errors, or errors in general you would want the user to notice
 */
static inline void IConsoleError(const char *string)
{
	IConsoleErrorF("ERROR: %s", string);
}

/* Parser */
void IConsoleCmdExec(const char *cmdstr, const uint recurse_count = 0);

bool IsValidConsoleColour(TextColour c);

#endif /* CONSOLE_FUNC_H */
