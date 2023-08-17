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
 * are given to them, each input word separated by a double-quote (") is an argument
 * If you want to handle multiple words as one, enclose them in double-quotes
 * eg. 'say "hello everybody"'
 */
typedef bool IConsoleCmdProc(byte argc, char *argv[]);
typedef ConsoleHookResult IConsoleHook(bool echo);
struct IConsoleCmd {
	IConsoleCmd(const std::string &name, IConsoleCmdProc *proc, IConsoleHook *hook) : name(name), proc(proc), hook(hook) {}

	std::string name;         ///< name of command
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
 * - "%+" lists all parameters keeping them separated
 * - "%!" also lists all parameters but presenting them to the aliased command as one argument
 * - ";" allows for combining commands (see example 'ng')
 */
struct IConsoleAlias {
	IConsoleAlias(const std::string &name, const std::string &cmdline) : name(name), cmdline(cmdline) {}

	std::string name;           ///< name of the alias
	std::string cmdline;        ///< command(s) that is/are being aliased
};

struct IConsole
{
	typedef std::map<std::string, IConsoleCmd> CommandList;
	typedef std::map<std::string, IConsoleAlias> AliasList;

	/* console parser */
	static CommandList &Commands();
	static AliasList &Aliases();

	/* Commands */
	static void CmdRegister(const std::string &name, IConsoleCmdProc *proc, IConsoleHook *hook = nullptr);
	static IConsoleCmd *CmdGet(const std::string &name);
	static void AliasRegister(const std::string &name, const std::string &cmd);
	static IConsoleAlias *AliasGet(const std::string &name);
};

/* console functions */
void IConsoleClearBuffer();

/* console std lib (register ingame commands/aliases) */
void IConsoleStdLibRegister();

/* Supporting functions */
bool GetArgumentInteger(uint32_t *value, const char *arg);

void IConsoleGUIInit();
void IConsoleGUIFree();
void IConsoleGUIPrint(TextColour colour_code, const std::string &string);

#endif /* CONSOLE_INTERNAL_H */
