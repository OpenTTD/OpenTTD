#ifndef CONSOLE_H
#define CONSOLE_H

// ** console ** //

enum {
	ICONSOLE_OPENED=0,
	ICONSOLE_CLOSED,
} _iconsole_modes;

// ** console parser ** //

enum {
	ICONSOLE_VAR_NONE=0,
	ICONSOLE_VAR_BOOLEAN,
	ICONSOLE_VAR_BYTE,
	ICONSOLE_VAR_UINT16,
	ICONSOLE_VAR_UINT32,
	ICONSOLE_VAR_INT16,
	ICONSOLE_VAR_INT32,
	ICONSOLE_VAR_STRING,
	ICONSOLE_VAR_POINTER,
	ICONSOLE_VAR_REFERENCE,
	ICONSOLE_VAR_UNKNOWN,
} _iconsole_var_types;

enum {
	ICONSOLE_HOOK_ACCESS,
	ICONSOLE_HOOK_BEFORE_CHANGE,
	ICONSOLE_HOOK_BEFORE_EXEC,
	ICONSOLE_HOOK_AFTER_CHANGE,
	ICONSOLE_HOOK_AFTER_EXEC,
} _iconsole_hook_types;

typedef struct {
	// -------------- //
	void * addr;
	byte * name;
	// -------------- //
	void * hook_access;
	void * hook_before_exec;
	void * hook_after_exec;
	// -------------- //
	void * _next;
	} _iconsole_cmd;

typedef struct {
	// --------------- //
	void * addr;
	const byte * name;
	byte type;
	// -------------- //
	void * hook_access;
	void * hook_before_change;
	void * hook_after_change;
	// -------------- //
	void * _next;
	bool _malloc;
	} _iconsole_var;

// ** console parser ** //

_iconsole_cmd * _iconsole_cmds; // list of registred commands
_iconsole_var * _iconsole_vars; // list of registred vars

// ** console colors ** //
VARDEF byte _iconsole_color_default;
VARDEF byte _iconsole_color_error;
VARDEF byte _iconsole_color_warning;
VARDEF byte _iconsole_color_debug;
VARDEF byte _iconsole_color_commands;

// ** ttd.c functions ** //

void SetDebugString(const char *s);

// ** console functions ** //

void IConsoleClearCommand();
void IConsoleInit();
void IConsoleClear();
void IConsoleFree();
void IConsoleResize();
void IConsoleSwitch();
void IConsoleClose();
void IConsoleOpen();

// ** console cmd buffer ** //
void IConsoleCmdBufferAdd(const byte *cmd);
void IConsoleCmdBufferNavigate(signed char direction);

// ** console output ** //
void IConsolePrint(byte color_code, const byte* string);
void CDECL IConsolePrintF(byte color_code, const char *s, ...);
void IConsoleDebug(byte* string);
void IConsoleError(const byte* string);
void IConsoleWarning(const byte* string);

// *** Commands *** //

void IConsoleCmdRegister(const byte * name, void * addr);
_iconsole_cmd * IConsoleCmdGet(const byte * name);

// *** Variables *** //

void IConsoleVarRegister(const byte * name, void * addr, byte type);
void IConsoleVarMemRegister(const byte * name, byte type);
void IConsoleVarInsert(_iconsole_var * var, const byte * name);
_iconsole_var * IConsoleVarGet(const byte * name);
_iconsole_var * IConsoleVarAlloc(byte type);
void IConsoleVarFree(_iconsole_var * var);
void IConsoleVarSetString(_iconsole_var * var, const byte * string);
void IConsoleVarSetValue(_iconsole_var * var, int value);
void IConsoleVarDump(_iconsole_var * var, const byte * dump_desc);

// *** Parser *** //

void IConsoleCmdExec(const byte* cmdstr);

// ** console std lib ** //
void IConsoleStdLibRegister();

// ** hook code ** //
void IConsoleVarHook(const byte * name, byte type, void * proc);
void IConsoleCmdHook(const byte * name, byte type, void * proc);
bool IConsoleVarHookHandle(_iconsole_var * hook_var, byte type);
bool IConsoleCmdHookHandle(_iconsole_cmd * hook_cmd, byte type);

#endif /* CONSOLE_H */
