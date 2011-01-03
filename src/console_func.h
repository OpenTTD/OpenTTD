/* $Id$ */

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
void IConsoleDebug(const char *dbg, const char *string);
void IConsoleWarning(const char *string);
void IConsoleError(const char *string);

/* Parser */
void IConsoleCmdExec(const char *cmdstr);

bool IsValidConsoleColour(TextColour c);

#endif /* CONSOLE_FUNC_H */
