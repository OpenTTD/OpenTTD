/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file console_internal.h Internally used functions for the console. */

#ifndef CONSOLE_INTERNAL_H
#define CONSOLE_INTERNAL_H

#include "gfx_type.h"

static const uint ICON_CMDLN_SIZE     = 1024; ///< maximum length of a typed in command
static const uint ICON_MAX_STREAMSIZE = 2048; ///< maximum length of a totally expanded command

/** Return values of console hooks (#IConsoleHook). */
enum ConsoleHookResult {
	CHR_ALLOW,    ///< Allow command execution.
	CHR_DISALLOW, ///< Disallow command execution.
	CHR_HIDE,     ///< Hide the existence of the command.
};

/**
 * --Commands--
 * Commands are commands, or functions. They get executed once and any
 * effect they produce are carried out. The arguments to the commands
 * are given to them, each input word seperated by a double-quote (") is an argument
 * If you want to handle multiple words as one, enclose them in double-quotes
 * eg. 'say "hello sexy boy"'
 */
typedef bool IConsoleCmdProc(byte argc, char *argv[]);
typedef ConsoleHookResult IConsoleHook(bool echo);
struct IConsoleCmd {
	char *name;               ///< name of command
	IConsoleCmd *next;        ///< next command in list

	IConsoleCmdProc *proc;    ///< process executed when command is typed
	IConsoleHook *hook;       ///< any special trigger action that needs executing
};

/**
 * --Aliases--
 * Aliases are like shortcuts for complex functions, variable assignments,
 * etc. You can use a simple alias to rename a longer command (eg 'set' for
 * 'setting' for example), or concatenate more commands into one
 * (eg. 'ng' for 'load %A; unpause; debug_level 5'). Aliases can parse the arguments
 * given to them in the command line.
 * - "%A - %Z" substitute arguments 1 t/m 26
 * - "%+" lists all parameters keeping them seperated
 * - "%!" also lists all parameters but presenting them to the aliased command as one argument
 * - ";" allows for combining commands (see example 'ng')
 */
struct IConsoleAlias {
	char *name;                 ///< name of the alias
	IConsoleAlias *next;        ///< next alias in list

	char *cmdline;              ///< command(s) that is/are being aliased
};

/* console parser */
extern IConsoleCmd   *_iconsole_cmds;    ///< List of registered commands.
extern IConsoleAlias *_iconsole_aliases; ///< List of registered aliases.

/* console functions */
void IConsoleClearBuffer();

/* Commands */
void IConsoleCmdRegister(const char *name, IConsoleCmdProc *proc, IConsoleHook *hook = NULL);
void IConsoleAliasRegister(const char *name, const char *cmd);
IConsoleCmd *IConsoleCmdGet(const char *name);
IConsoleAlias *IConsoleAliasGet(const char *name);

/* console std lib (register ingame commands/aliases) */
void IConsoleStdLibRegister();

/* Supporting functions */
bool GetArgumentInteger(uint32 *value, const char *arg);

void IConsoleGUIInit();
void IConsoleGUIFree();
void IConsoleGUIPrint(TextColour colour_code, char *string);
char *RemoveUnderscores(char *name);

#endif /* CONSOLE_INTERNAL_H */
