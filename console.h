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
	ICONSOLE_VAR_UNKNOWN
} _iconsole_var_types;

typedef struct {
	// -------------- //
	void * addr;
	byte * name;
	// -------------- //
	void * _next;
	} _iconsole_cmd;

typedef struct {
	// --------------- //
	void * addr;
	byte * name;
	byte type;
	// -------------- //
	void * _next;
	bool _malloc;
	} _iconsole_var;

// ** console colors ** //
VARDEF byte _iconsole_color_default;
VARDEF byte _iconsole_color_error;
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
void IConsoleCmdBufferAdd(byte * cmd);
void IConsoleCmdBufferNavigate(signed char direction);

// ** console output ** //
void IConsolePrint(byte color_code, byte* string);
void IConsolePrintF(byte color_code, const char *s, ...);
void IConsoleDebug(byte* string);
void IConsoleError(byte* string);

// *** Commands *** //

void IConsoleCmdRegister(byte * name, void * addr);
void* IConsoleCmdGetAddr(byte * name);

// *** Variables *** //

void IConsoleVarRegister(byte * name, void * addr, byte type);
void IConsoleVarInsert(_iconsole_var * var, byte * name);
_iconsole_var * IConsoleVarGet(byte * name);
_iconsole_var * IConsoleVarAlloc(byte type);
void IConsoleVarFree(_iconsole_var * var);
void IConsoleVarSetString(_iconsole_var * var, byte * string);
void IConsoleVarSetValue(_iconsole_var * var, int value);
void IConsoleVarDump(_iconsole_var * var, byte * dump_desc);

// *** Parser *** //

void IConsoleCmdExec(byte * cmdstr);
