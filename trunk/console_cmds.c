/* -------------------- dont cross this line --------------------- */
#include "stdafx.h"
#include "ttd.h"
#include "console.h"
#include "engine.h"
#include "functions.h"
#include "variables.h"

#if defined(WIN32)
# define ENABLE_NETWORK
#endif


// ** scriptfile handling ** //
static FILE * _script_file;
static bool _script_running;

// ** console command / variable defines ** //

#define DEF_CONSOLE_CMD(yyyy) static _iconsole_var * yyyy(byte argc, byte* argv[], byte argt[])
#define DEF_CONSOLE_CMD_HOOK(yyyy) static bool yyyy(_iconsole_cmd * hookcmd)
#define DEF_CONSOLE_VAR_HOOK(yyyy) static bool yyyy(_iconsole_var * hookvar)


// ** supporting functions ** //

static int32 GetArgumentInteger(byte *arg)
{
	int32 result;
	sscanf((char *)arg, "%u", &result);

	if (result == 0 && arg[0] == '0' && arg[1] == 'x')
		sscanf((char *)arg, "%x", &result);

	return result;
}

/* **************************** */
/* variable and command hooks   */
/* **************************** */

DEF_CONSOLE_CMD_HOOK(ConCmdHookNoNetwork)
{
	if (_networking) {
		IConsoleError("this command is forbidden in multiplayer");
		return false;
		}
	return true;
}

#if 0 /* Not used atm */
DEF_CONSOLE_VAR_HOOK(ConVarHookNoNetwork)
{
	if (_networking) {
		IConsoleError("this variable is forbidden in multiplayer");
		return false;
		}
	return true;
}
#endif

DEF_CONSOLE_VAR_HOOK(ConVarHookNoNetClient)
{
	if (!_networking_server) {
		IConsoleError("this variable only makes sense for a network server");
		return false;
		}
	return true;
}

/* **************************** */
/* reset commands               */
/* **************************** */

DEF_CONSOLE_CMD(ConResetEngines)
{
	StartupEngines();
	return 0;
}

DEF_CONSOLE_CMD(ConResetTile)
{
	if (argc == 2) {
		TileIndex tile = (TileIndex)GetArgumentInteger(argv[1]);
		DoClearSquare(tile);
	}

	return 0;
}

DEF_CONSOLE_CMD(ConScrollToTile)
{
	if (argc == 2) {
		TileIndex tile = (TileIndex)GetArgumentInteger(argv[1]);
		ScrollMainWindowToTile(tile);
	}

	return 0;
}

// ********************************* //
// * Network Core Console Commands * //
// ********************************* //
#ifdef ENABLE_NETWORK

DEF_CONSOLE_CMD(ConNetworkConnect)
{
	byte * ip;
	const byte *port = NULL;
	const byte *player = NULL;
	uint16 rport;

	if (argc<2) return NULL;

	ip = argv[1];
	rport = _network_server_port;

	ParseConnectionString(&player, &port, ip);

	IConsolePrintF(_iconsole_color_default,"Connecting to %s...", ip);
	if (player!=NULL) {
		_network_playas = atoi(player);
		IConsolePrintF(_iconsole_color_default,"    player-no: %s", player);
	}
	if (port!=NULL) {
		rport = atoi(port);
		IConsolePrintF(_iconsole_color_default,"    port: %s", port);
	}

	NetworkCoreConnectGame(ip, rport);

	return NULL;
}

#endif

/* ******************************** */
/*   script file console commands   */
/* ******************************** */

DEF_CONSOLE_CMD(ConExec)
{
	char cmd[1024];
	bool doerror;

	if (argc<2) return NULL;

	doerror = true;
	_script_file = fopen(argv[1],"rb");

	if (_script_file == NULL) {
		if (argc>2) if (atoi(argv[2])==0) doerror=false;
		if (doerror) IConsoleError("script file not found");
		return NULL;
		}

	_script_running = true;

	while (!feof(_script_file) && _script_running) {

		fgets((char *)&cmd, 1024, _script_file);

		IConsoleCmdExec((byte *) &cmd);

	}

	_script_running = false;
	fclose(_script_file);
	return NULL;
}

DEF_CONSOLE_CMD(ConReturn)
{
	_script_running = false;
	return NULL;
}

/* **************************** */
/*   default console commands   */
/* **************************** */

DEF_CONSOLE_CMD(ConScript)
{
	extern FILE* _iconsole_output_file;

	if (_iconsole_output_file!=NULL) {
		if (argc<2) return NULL;
		IConsolePrintF(_iconsole_color_default,"file output complete");
		fclose(_iconsole_output_file);
	} else {
		IConsolePrintF(_iconsole_color_default,"file output started to: %s",argv[1]);
		_iconsole_output_file = fopen(argv[1],"ab");
		if (_iconsole_output_file == NULL) IConsoleError("could not open file");
	}
	return NULL;
}


DEF_CONSOLE_CMD(ConEcho)
{
	if (argc<2) return NULL;
	IConsolePrint(_iconsole_color_default, argv[1]);
	return NULL;
}

DEF_CONSOLE_CMD(ConEchoC)
{
	if (argc<3) return NULL;
	IConsolePrint(atoi(argv[1]), argv[2]);
	return NULL;
}

DEF_CONSOLE_CMD(ConPrintF)
{
	if (argc<3) return NULL;
	IConsolePrintF(_iconsole_color_default, argv[1] ,argv[2],argv[3],argv[4],argv[5],argv[6],argv[7],argv[8],argv[9],argv[10],argv[11],argv[12],argv[13],argv[14],argv[15],argv[16],argv[17],argv[18],argv[19]);
	return NULL;
}

DEF_CONSOLE_CMD(ConPrintFC)
{
	if (argc<3) return NULL;
	IConsolePrintF(atoi(argv[1]), argv[2] ,argv[3],argv[4],argv[5],argv[6],argv[7],argv[8],argv[9],argv[10],argv[11],argv[12],argv[13],argv[14],argv[15],argv[16],argv[17],argv[18],argv[19]);
	return NULL;
}

DEF_CONSOLE_CMD(ConScreenShot)
{
	if (argc<2) {
		_make_screenshot=1;
	} else {
		if (strcmp(argv[1],"big")==0) {
			_make_screenshot=2;
			}
		if (strcmp(argv[1],"no_con")==0) {
			IConsoleClose();
			_make_screenshot=1;
			}
		}
	return NULL;
}

DEF_CONSOLE_CMD(ConInfoVar)
{
	if (argc<2) return NULL;
	if (argt[1]!=ICONSOLE_VAR_REFERENCE) {
		IConsoleError("first argument has to be a variable reference");
		} else {
		_iconsole_var * item;
		item = (_iconsole_var *) argv[1];
		IConsolePrintF(_iconsole_color_default,"var_name: %s",item->name);
		IConsolePrintF(_iconsole_color_default,"var_type: %i",item->type);
		IConsolePrintF(_iconsole_color_default,"var_addr: %i",item->addr);
		if (item->_malloc) IConsolePrintF(_iconsole_color_default,"var_malloc: internal");
			else IConsolePrintF(_iconsole_color_default, "var_malloc: external");
		if (item->hook_access) IConsoleWarning("var_access hooked");
		if (item->hook_before_change) IConsoleWarning("var_before_change hooked");
		if (item->hook_after_change) IConsoleWarning("var_after_change hooked");
		}
	return NULL;
}


DEF_CONSOLE_CMD(ConInfoCmd)
{
	if (argc<2) return NULL;
	if (argt[1]!=ICONSOLE_VAR_UNKNOWN) {
		IConsoleError("first argument has to be a command name");
		} else {
		_iconsole_cmd * item;
		item = IConsoleCmdGet(argv[1]);
		if (item==NULL) {
			IConsoleError("the given command was not found");
			return NULL;
		}
		IConsolePrintF(_iconsole_color_default,"cmd_name: %s",item->name);
		IConsolePrintF(_iconsole_color_default,"cmd_addr: %i",item->addr);
		if (item->hook_access) IConsoleWarning("cmd_access hooked");
		if (item->hook_before_exec) IConsoleWarning("cmd_before_exec hooked");
		if (item->hook_after_exec) IConsoleWarning("cmd_after_exec hooked");
		}
	return NULL;
}

DEF_CONSOLE_CMD(ConDebugLevel)
{
	if (argc<2) return NULL;
	SetDebugString(argv[1]);
	return NULL;
}

DEF_CONSOLE_CMD(ConExit)
{
	_exit_game = true;
	return NULL;
}

DEF_CONSOLE_CMD(ConHelp)
{
	IConsolePrint(13	," -- console help -- ");
	IConsolePrint(1		," variables: [command to list them: list_vars]");
	IConsolePrint(1		," *temp_string = \"my little \"");
	IConsolePrint(1		,"");
	IConsolePrint(1		," commands: [command to list them: list_cmds]");
	IConsolePrint(1		," [command] [\"string argument with spaces\"] [argument 2] ...");
	IConsolePrint(1		," printf \"%s world\" *temp_string");
	IConsolePrint(1		,"");
	IConsolePrint(1		," command/variable returning a value into an variable:");
	IConsolePrint(1		," *temp_uint16 << random");
	IConsolePrint(1		," *temp_uint16 << *temp_uint16_2");
	IConsolePrint(1		,"");
	return NULL;
}

DEF_CONSOLE_CMD(ConRandom)
{
	_iconsole_var * result;
	result = IConsoleVarAlloc(ICONSOLE_VAR_UINT16);
	IConsoleVarSetValue(result,rand());
	return result;
}

DEF_CONSOLE_CMD(ConListCommands)
{
	_iconsole_cmd * item;
	int l = 0;

	if (argv[1]!=NULL) l = strlen((char *) argv[1]);

	item = _iconsole_cmds;
	while (item != NULL) {
		if (argv[1]!=NULL) {

			if (memcmp((void *) item->name, (void *) argv[1],l)==0)
					IConsolePrintF(_iconsole_color_default,"%s",item->name);

			} else {

			IConsolePrintF(_iconsole_color_default,"%s",item->name);

			}
		item = item->_next;
		}

	return NULL;
}

DEF_CONSOLE_CMD(ConListVariables)
{
	_iconsole_var * item;
	int l = 0;

	if (argv[1]!=NULL) l = strlen((char *) argv[1]);

	item = _iconsole_vars;
	while (item != NULL) {
		if (argv[1]!=NULL) {

			if (memcmp(item->name, argv[1],l)==0)
					IConsolePrintF(_iconsole_color_default,"%s",item->name);

			} else {

			IConsolePrintF(_iconsole_color_default,"%s",item->name);

			}
		item = item->_next;
		}

	return NULL;
}

DEF_CONSOLE_CMD(ConListDumpVariables)
{
	_iconsole_var * item;
	int l = 0;

	if (argv[1]!=NULL) l = strlen((char *) argv[1]);

	item = _iconsole_vars;
	while (item != NULL) {
		if (argv[1]!=NULL) {

			if (memcmp(item->name, argv[1],l)==0)
					IConsoleVarDump(item,NULL);

			} else {

			IConsoleVarDump(item,NULL);

			}
		item = item->_next;
		}

	return NULL;
}

#ifdef _DEBUG
/* ****************************************** */
/*  debug commands and variables */
/* ****************************************** */

void IConsoleDebugLibRegister()
{
	// stdlib
	extern bool _stdlib_con_developer;

	IConsoleVarRegister("con_developer",(void *) &_stdlib_con_developer,ICONSOLE_VAR_BOOLEAN);
	IConsoleVarMemRegister("temp_bool",ICONSOLE_VAR_BOOLEAN);
	IConsoleVarMemRegister("temp_int16",ICONSOLE_VAR_INT16);
	IConsoleVarMemRegister("temp_int32",ICONSOLE_VAR_INT32);
	IConsoleVarMemRegister("temp_pointer",ICONSOLE_VAR_POINTER);
	IConsoleVarMemRegister("temp_uint16",ICONSOLE_VAR_UINT16);
	IConsoleVarMemRegister("temp_uint16_2",ICONSOLE_VAR_UINT16);
	IConsoleVarMemRegister("temp_uint32",ICONSOLE_VAR_UINT32);
	IConsoleVarMemRegister("temp_string",ICONSOLE_VAR_STRING);
	IConsoleVarMemRegister("temp_string2",ICONSOLE_VAR_STRING);
	IConsoleCmdRegister("resettile",ConResetTile);
}
#endif

/* ****************************************** */
/*  console command and variable registration */
/* ****************************************** */

void IConsoleStdLibRegister()
{
	// stdlib
	extern byte _stdlib_developer;

#ifdef _DEBUG
	IConsoleDebugLibRegister();
#else
	(void)ConResetTile; // Silence warning, this is only used in _DEBUG
#endif

	// functions [please add them alphabeticaly]
#ifdef ENABLE_NETWORK
	IConsoleCmdRegister("connect",ConNetworkConnect);
	IConsoleCmdHook("connect",ICONSOLE_HOOK_ACCESS,ConCmdHookNoNetwork);
#endif
	IConsoleCmdRegister("debug_level",ConDebugLevel);
	IConsoleCmdRegister("dump_vars",ConListDumpVariables);
	IConsoleCmdRegister("echo",ConEcho);
	IConsoleCmdRegister("echoc",ConEchoC);
	IConsoleCmdRegister("exec",ConExec);
	IConsoleCmdRegister("exit",ConExit);
	IConsoleCmdRegister("help",ConHelp);
	IConsoleCmdRegister("info_cmd",ConInfoCmd);
	IConsoleCmdRegister("info_var",ConInfoVar);
	IConsoleCmdRegister("list_cmds",ConListCommands);
	IConsoleCmdRegister("list_vars",ConListVariables);
	IConsoleCmdRegister("printf",ConPrintF);
	IConsoleCmdRegister("printfc",ConPrintFC);
	IConsoleCmdRegister("quit",ConExit);
	IConsoleCmdRegister("random",ConRandom);
	IConsoleCmdRegister("resetengines",ConResetEngines);
	IConsoleCmdHook("resetengines",ICONSOLE_HOOK_ACCESS,ConCmdHookNoNetwork);
	IConsoleCmdRegister("return",ConReturn);
	IConsoleCmdRegister("screenshot",ConScreenShot);
	IConsoleCmdRegister("script",ConScript);
	IConsoleCmdRegister("scrollto",ConScrollToTile);

	// variables [please add them alphabeticaly]
	IConsoleVarRegister("developer",(void *) &_stdlib_developer,ICONSOLE_VAR_BYTE);
#ifdef ENABLE_NETWORK
	IConsoleVarRegister("net_client_timeout",&_network_client_timeout,ICONSOLE_VAR_UINT16);
	IConsoleVarHook("*net_client_timeout",ICONSOLE_HOOK_ACCESS,ConVarHookNoNetClient);
	IConsoleVarRegister("net_ready_ahead",&_network_ready_ahead,ICONSOLE_VAR_UINT16);
	IConsoleVarRegister("net_sync_freq",&_network_sync_freq,ICONSOLE_VAR_UINT16);
	IConsoleVarHook("*net_sync_freq",ICONSOLE_HOOK_ACCESS,ConVarHookNoNetClient);
#endif


}
/* -------------------- dont cross this line --------------------- */
