// ** console ** //

enum {
	ICONSOLE_OPENED=0,
	ICONSOLE_CLOSED,
	ICONSOLE_OPENING,
	ICONSOLE_CLOSING,
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
	ICONSOLE_VAR_VARPTR,
	ICONSOLE_VAR_POINTER,
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
	} _iconsole_var;

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
void IConsolePrint(byte color_code, byte* string);
void IConsolePrintF(byte color_code, const char *s, ...);
void IConsoleDebug(byte* string);
void IConsoleError(byte* string);
void IConsoleCmdRegister(byte * name, void * addr);
void IConsoleVarRegister(byte * name, void * addr, byte type);
void IConsoleCmdExec(byte * cmdstr);

