#ifndef CONSOLE_H
#define CONSOLE_H

/* Pointer to console window */
VARDEF Window *_iconsole_win;

// ** console parser ** //

typedef enum _iconsole_var_types {
	ICONSOLE_VAR_NONE,
	ICONSOLE_VAR_BOOLEAN,
	ICONSOLE_VAR_BYTE,
	ICONSOLE_VAR_UINT8,
	ICONSOLE_VAR_UINT16,
	ICONSOLE_VAR_UINT32,
	ICONSOLE_VAR_INT16,
	ICONSOLE_VAR_INT32,
	ICONSOLE_VAR_STRING,
	ICONSOLE_VAR_POINTER,
	ICONSOLE_VAR_REFERENCE,
	ICONSOLE_VAR_UNKNOWN
} _iconsole_var_types;

typedef enum {
	ICONSOLE_FULL,
	ICONSOLE_OPENED,
	ICONSOLE_CLOSED
} _iconsole_modes;

typedef enum _iconsole_hook_types {
	ICONSOLE_HOOK_ACCESS,
	ICONSOLE_HOOK_BEFORE_CHANGE,
	ICONSOLE_HOOK_BEFORE_EXEC,
	ICONSOLE_HOOK_AFTER_CHANGE,
	ICONSOLE_HOOK_AFTER_EXEC
} _iconsole_hook_types;

struct _iconsole_var;
typedef bool (*iconsole_var_hook)(struct _iconsole_var* hook_var);

typedef struct _iconsole_var {
	// --------------- //
	union {
		void*   addr;
		bool*   bool_;
		byte*   byte_;
		uint16* uint16_;
		uint32* uint32_;
		int16*  int16_;
		int32*  int32_;
		char*   string_;
		struct _iconsole_var* reference_;
	} data;
	char* name;
	_iconsole_var_types type;
	// -------------- //
	iconsole_var_hook hook_access;
	iconsole_var_hook hook_before_change;
	iconsole_var_hook hook_after_change;
	// -------------- //
	struct _iconsole_var* _next;
	bool _malloc;
} _iconsole_var;

struct _iconsole_cmd;
typedef bool (*iconsole_cmd_hook)(struct _iconsole_cmd* hook_cmd);

typedef _iconsole_var* (*_iconsole_cmd_addr)(byte argc, char* argv[], byte argt[]);

typedef struct _iconsole_cmd {
	// -------------- //
	_iconsole_cmd_addr addr;
	char* name;
	// -------------- //
	iconsole_cmd_hook hook_access;
	iconsole_cmd_hook hook_before_exec;
	iconsole_cmd_hook hook_after_exec;
	// -------------- //
	void* _next;
} _iconsole_cmd;

void IConsoleAliasRegister(const char* name, const char* cmdline);

typedef struct _iconsole_alias {
	// -------------- //
	char * cmdline;
	char* name;
	void* _next;
} _iconsole_alias;

_iconsole_alias* IConsoleAliasGet(const char* name);

// ** console parser ** //

_iconsole_cmd* _iconsole_cmds; // list of registred commands
_iconsole_var* _iconsole_vars; // list of registred vars
_iconsole_alias* _iconsole_aliases; // list of registred aliases

// ** console colors ** //
VARDEF byte _iconsole_color_default;
VARDEF byte _iconsole_color_error;
VARDEF byte _iconsole_color_warning;
VARDEF byte _iconsole_color_debug;
VARDEF byte _iconsole_color_commands;
VARDEF _iconsole_modes _iconsole_mode;

// ** ttd.c functions ** //

void SetDebugString(const char* s);

// ** console functions ** //

void IConsoleInit(void);
void IConsoleClear(void);
void IConsoleFree(void);
void IConsoleResize(void);
void IConsoleSwitch(void);
void IConsoleClose(void);
void IConsoleOpen(void);

// ** console cmd buffer ** //
void IConsoleCmdBufferAdd(const char* cmd);
void IConsoleCmdBufferNavigate(signed char direction);

// ** console output ** //
void IConsolePrint(uint16 color_code, const char* string);
void CDECL IConsolePrintF(uint16 color_code, const char* s, ...);
void IConsoleDebug(const char* string);
void IConsoleError(const char* string);
void IConsoleWarning(const char* string);

// *** Commands *** //

void IConsoleCmdRegister(const char* name, _iconsole_cmd_addr addr);
_iconsole_cmd* IConsoleCmdGet(const char* name);

// *** Variables *** //

void IConsoleVarRegister(const char* name, void* addr, _iconsole_var_types type);
void IConsoleVarMemRegister(const char* name, _iconsole_var_types type);
void IConsoleVarInsert(_iconsole_var* item_new, const char* name);
_iconsole_var* IConsoleVarGet(const char* name);
_iconsole_var* IConsoleVarAlloc(_iconsole_var_types type);
void IConsoleVarFree(_iconsole_var* var);
void IConsoleVarSetString(_iconsole_var* var, const char* string);
void IConsoleVarSetValue(_iconsole_var* var, int value);
void IConsoleVarDump(const _iconsole_var* var, const char* dump_desc);

// *** Parser *** //

void IConsoleCmdExec(const char* cmdstr);

// ** console std lib ** //
void IConsoleStdLibRegister(void);

// ** hook code ** //
void IConsoleVarHook(const char* name, _iconsole_hook_types type, iconsole_var_hook proc);
void IConsoleCmdHook(const char* name, _iconsole_hook_types type, iconsole_cmd_hook proc);
bool IConsoleVarHookHandle(_iconsole_var* hook_var, _iconsole_hook_types type);
bool IConsoleCmdHookHandle(_iconsole_cmd* hook_cmd, _iconsole_hook_types type);

#endif /* CONSOLE_H */
