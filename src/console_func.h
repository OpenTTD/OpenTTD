/* $Id$ */

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
void IConsolePrint(ConsoleColour color_code, const char *string);
void CDECL IConsolePrintF(ConsoleColour color_code, const char *s, ...);
void IConsoleDebug(const char *dbg, const char *string);
void IConsoleWarning(const char *string);
void IConsoleError(const char *string);

/* Parser */
void IConsoleCmdExec(const char *cmdstr);

#endif /* CONSOLE_FUNC_H */
