/* $Id$ */

/** @file console_internal.h Internally used functions for the console. */

#ifndef CONSOLE_INTERNAL_H
#define CONSOLE_INTERNAL_H

#include "console_func.h"

/* maximum length of a typed in command */
#define ICON_CMDLN_SIZE 255
/* maximum length of a totally expanded command */
#define ICON_MAX_STREAMSIZE 1024

enum IConsoleVarTypes {
	ICONSOLE_VAR_BOOLEAN,
	ICONSOLE_VAR_BYTE,
	ICONSOLE_VAR_UINT16,
	ICONSOLE_VAR_UINT32,
	ICONSOLE_VAR_INT16,
	ICONSOLE_VAR_INT32,
	ICONSOLE_VAR_STRING
};

enum IConsoleHookTypes {
	ICONSOLE_HOOK_ACCESS,
	ICONSOLE_HOOK_PRE_ACTION,
	ICONSOLE_HOOK_POST_ACTION
};

/** --Hooks--
 * Hooks are certain triggers get get accessed/executed on either
 * access, before execution/change or after execution/change. This allows
 * for general flow of permissions or special action needed in some cases
 */
typedef bool IConsoleHook();
struct IConsoleHooks{
	IConsoleHook *access; ///< trigger when accessing the variable/command
	IConsoleHook *pre;    ///< trigger before the variable/command is changed/executed
	IConsoleHook *post;   ///< trigger after the variable/command is changed/executed
};

/** --Commands--
 * Commands are commands, or functions. They get executed once and any
 * effect they produce are carried out. The arguments to the commands
 * are given to them, each input word seperated by a double-quote (") is an argument
 * If you want to handle multiple words as one, enclose them in double-quotes
 * eg. 'say "hello sexy boy"'
 */
typedef bool (IConsoleCmdProc)(byte argc, char *argv[]);

struct IConsoleCmd {
	char *name;               ///< name of command
	IConsoleCmd *next;        ///< next command in list

	IConsoleCmdProc *proc;    ///< process executed when command is typed
	IConsoleHooks hook;       ///< any special trigger action that needs executing
};

/** --Variables--
 * Variables are pointers to real ingame variables which allow for
 * changing while ingame. After changing they keep their new value
 * and can be used for debugging, gameplay, etc. It accepts:
 * - no arguments; just print out current value
 * - '= <new value>' to assign a new value to the variable
 * - '++' to increase value by one
 * - '--' to decrease value by one
 */
struct IConsoleVar {
	char *name;               ///< name of the variable
	IConsoleVar *next;        ///< next variable in list

	void *addr;               ///< the address where the variable is pointing at
	uint32 size;              ///< size of the variable, used for strings
	char *help;               ///< the optional help string shown when requesting information
	IConsoleVarTypes type;    ///< type of variable (for correct assignment/output)
	IConsoleCmdProc *proc;    ///< some variables need really special handling, use a callback function for that
	IConsoleHooks hook;       ///< any special trigger action that needs executing
};

/** --Aliases--
 * Aliases are like shortcuts for complex functions, variable assignments,
 * etc. You can use a simple alias to rename a longer command (eg 'lv' for
 * 'list_vars' for example), or concatenate more commands into one
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
extern IConsoleCmd   *_iconsole_cmds;    ///< list of registred commands
extern IConsoleVar   *_iconsole_vars;    ///< list of registred vars
extern IConsoleAlias *_iconsole_aliases; ///< list of registred aliases

/* console functions */
void IConsoleClearBuffer();
void IConsoleOpen();

/* Commands */
void IConsoleCmdRegister(const char *name, IConsoleCmdProc *proc);
void IConsoleAliasRegister(const char *name, const char *cmd);
IConsoleCmd *IConsoleCmdGet(const char *name);
IConsoleAlias *IConsoleAliasGet(const char *name);

/* Variables */
void IConsoleVarRegister(const char *name, void *addr, IConsoleVarTypes type, const char *help);
void IConsoleVarStringRegister(const char *name, void *addr, uint32 size, const char *help);
IConsoleVar* IConsoleVarGet(const char *name);
void IConsoleVarPrintGetValue(const IConsoleVar *var);
void IConsoleVarPrintSetValue(const IConsoleVar *var);

/* Parser */
void IConsoleVarExec(const IConsoleVar *var, byte tokencount, char *token[]);

/* console std lib (register ingame commands/aliases/variables) */
void IConsoleStdLibRegister();

/* Hooking code */
void IConsoleCmdHookAdd(const char *name, IConsoleHookTypes type, IConsoleHook *proc);
void IConsoleVarHookAdd(const char *name, IConsoleHookTypes type, IConsoleHook *proc);
void IConsoleVarProcAdd(const char *name, IConsoleCmdProc *proc);

/* Supporting functions */
bool GetArgumentInteger(uint32 *value, const char *arg);

void IConsoleGUIInit();
void IConsoleGUIFree();
void IConsoleGUIPrint(ConsoleColour color_code, char *string);

#endif /* CONSOLE_INTERNAL_H */
