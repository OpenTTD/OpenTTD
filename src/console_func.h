/* $Id$ */

/** @file console_func.h Console functions used outside of the console code. */

#ifndef CONSOLE_FUNC_H
#define CONSOLE_FUNC_H

#include "console_type.h"

/* console colors/modes */
extern byte _icolour_def;
extern byte _icolour_err;
extern byte _icolour_warn;
extern byte _icolour_dbg;
extern byte _icolour_cmd;
extern IConsoleModes _iconsole_mode;

/* console functions */
void IConsoleInit();
void IConsoleFree();
void IConsoleClose();

/* console output */
void IConsolePrint(uint16 color_code, const char *string);
void CDECL IConsolePrintF(uint16 color_code, const char *s, ...);
void IConsoleDebug(const char *dbg, const char *string);

/* Parser */
void IConsoleCmdExec(const char *cmdstr);

#endif /* CONSOLE_FUNC_H */
