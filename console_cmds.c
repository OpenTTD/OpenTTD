/* -------------------- dont cross this line --------------------- */
#include "stdafx.h"
#include "ttd.h"
#include "console.h"
#include "engine.h"
#include "functions.h"
#include "variables.h"
#include "network_data.h"
#include "network_client.h"
#include "network_server.h"
#include "network_udp.h"
#include "command.h"
#include "settings.h"
#include "hal.h" /* for file list */


// ** scriptfile handling ** //
static FILE * _script_file;
static bool _script_running;

// ** console command / variable defines ** //

#define DEF_CONSOLE_CMD(yyyy) static _iconsole_var * yyyy(byte argc, char* argv[], byte argt[])
#define DEF_CONSOLE_CMD_HOOK(yyyy) static bool yyyy(_iconsole_cmd * hookcmd)
#define DEF_CONSOLE_VAR_HOOK(yyyy) static bool yyyy(_iconsole_var * hookvar)


// ** supporting functions ** //

static uint32 GetArgumentInteger(const char* arg)
{
	uint32 result;
	sscanf(arg, "%u", &result);

	if (result == 0 && arg[0] == '0' && arg[1] == 'x')
		sscanf(arg, "%x", &result);

	return result;
}

/* **************************** */
/* variable and command hooks   */
/* **************************** */

#ifdef ENABLE_NETWORK

DEF_CONSOLE_CMD_HOOK(ConCmdHookNoNetwork)
{
	if (_networking) {
		IConsoleError("This command is forbidden in multiplayer.");
		return false;
	}
	return true;
}

DEF_CONSOLE_VAR_HOOK(ConVarHookNoNetClient)
{
	if (!_network_available) {
		IConsoleError("You can not use this command because there is no network available.");
		return false;
	}
	if (!_network_server) {
		IConsoleError("This variable only makes sense for a network server.");
		return false;
	}
	return true;
}

DEF_CONSOLE_CMD_HOOK(ConCmdHookNoNetClient)
{
	if (!_network_available) {
		IConsoleError("You can not use this command because there is no network available.");
		return false;
	}
	if (!_network_server) {
		IConsoleError("This command is only available for a network server.");
		return false;
	}
	return true;
}

DEF_CONSOLE_CMD_HOOK(ConCmdHookNoNetServer)
{
	if (!_network_available) {
		IConsoleError("You can not use this command because there is no network available.");
		return false;
	}
	if (_network_server) {
		IConsoleError("You can not use this command because you are a network-server.");
		return false;
	}
	return true;
}

DEF_CONSOLE_CMD_HOOK(ConCmdHookNeedNetwork)
{
	if (!_network_available) {
		IConsoleError("You can not use this command because there is no network available.");
		return false;
	}
	if (!_networking) {
		IConsoleError("Not connected. Multiplayer only command.");
		return false;
	}
	return true;
}

#endif /* ENABLE_NETWORK */

/* **************************** */
/* reset commands               */
/* **************************** */

DEF_CONSOLE_CMD(ConResetEngines)
{
	StartupEngines();
	return 0;
}

#ifdef _DEBUG
DEF_CONSOLE_CMD(ConResetTile)
{
	if (argc == 2) {
		TileIndex tile = (TileIndex)GetArgumentInteger(argv[1]);
		DoClearSquare(tile);
	}

	return 0;
}
#endif

DEF_CONSOLE_CMD(ConScrollToTile)
{
	if (argc == 2) {
		TileIndex tile = (TileIndex)GetArgumentInteger(argv[1]);
		ScrollMainWindowToTile(tile);
	}

	return 0;
}

extern bool SafeSaveOrLoad(const char *filename, int mode, int newgm);
extern void BuildFileList(void);
extern void SetFiosType(const byte fiostype);

/* Load a file-number from current dir */
static void LoadMap(uint no)
{
	/* Build file list */
	BuildFileList();

	/* Check if in range */
	if (no != 0 && no <= (uint)_fios_num) {
		const FiosItem *item = &_fios_list[no - 1];

		if (item->type == FIOS_TYPE_FILE) {
			/* Load the file */
			_switch_mode = SM_LOAD;
			SetFiosType(item->type);
			strcpy(_file_to_saveload.name, FiosBrowseTo(item));

			IConsolePrint(_iconsole_color_default, "Loading map...");
		} else
			IConsolePrint(_iconsole_color_error, "That is not a map.");

	} else /* Show usages */
		IConsolePrint(_iconsole_color_error, "Unknown map. Use 'list_files' and 'goto_dir' to find the numbers of the savegame.");

	/* Free the file-list */
	FiosFreeSavegameList();
}

/* Load a file from a map */
DEF_CONSOLE_CMD(ConLoad)
{
	/* We need 1 argument */
	if (argc == 2) {
		/* Load the map */
		LoadMap(atoi(argv[1]));
		return NULL;
	}

	/* Give usage */
	IConsolePrint(_iconsole_color_default, "Unknown usage. Usage: load <file-no>");
	return NULL;
}

/* List all the files in the current dir via console */
DEF_CONSOLE_CMD(ConListFiles)
{
	const FiosItem *item;
	int pos = 0;

	/* Build the file-list */
	BuildFileList();

	/* As long as we have files */
	while (pos < _fios_num) {
		item = _fios_list + pos;
		pos++;
		/* Show them */
		IConsolePrintF(_iconsole_color_default, "%d) %s", pos, item->title[0] ? item->title : item->name);
	}

	/* Destroy the file list */
	FiosFreeSavegameList();

	return NULL;
}

/* Get an Specific file */
DEF_CONSOLE_CMD(ConScanFiles)
{
	const FiosItem *item;
	int pos = 0;
	_iconsole_var* result;


	result = IConsoleVarAlloc(ICONSOLE_VAR_STRING);

	if (argc <= 1) {
		IConsoleVarSetString(result, "0");
		return result; // return an zero
	}

	/* Build the file-list */
	BuildFileList();

	/* As long as we have files */
	while (pos < _fios_num) {
		item = _fios_list + pos;
		pos++;
		if (strcmp(argv[1], "..") == 0) {
			if (item->type == FIOS_TYPE_PARENT) {
				// huh we are searching for the parent directory
				char buffer[10];
				sprintf(buffer, "%d", pos);
				IConsoleVarSetString(result, buffer);
				return result;
			}
		} else
			// file records ?
			if (item->type == FIOS_TYPE_FILE) {
				if (strcmp(argv[1], item->name) == 0) {
					char buffer[10];
					sprintf(buffer, "%d", pos);
					IConsoleVarSetString(result, buffer);
					return result;
				}
			}
	}

	/* Destroy the file list */
	FiosFreeSavegameList();

	return NULL;
}

/* Change the dir via console */
DEF_CONSOLE_CMD(ConGotoDir)
{
	char *file;
	int no;

	/* We need 1 argument */
	if (argc != 2) {
		IConsolePrint(_iconsole_color_default, "Unknown usage. Usage: goto_dir <dir-no>");
		return NULL;
	}

	no = atoi(argv[1]);

	/* Make the file list */
	BuildFileList();

	/* Check if we are in range */
	if (no != 0 && no <= _fios_num) {
		const FiosItem *item = &_fios_list[no - 1];
		/* Only DIR and PARENT we do allow here */
		if (item->type == FIOS_TYPE_DIR || item->type == FIOS_TYPE_PARENT) {
			/* Goto the map */
			file = FiosBrowseTo(item);
			FiosFreeSavegameList();
			return NULL;
		}
	}

	/* Report error */
	IConsolePrint(_iconsole_color_default, "That number is no directory.");
	FiosFreeSavegameList();

	return NULL;
}


// ********************************* //
// * Network Core Console Commands * //
// ********************************* //
#ifdef ENABLE_NETWORK

DEF_CONSOLE_CMD(ConBan)
{
	NetworkClientInfo *ci;

	if (argc == 2) {
		uint32 index = atoi(argv[1]);
		if (index == NETWORK_SERVER_INDEX) {
			IConsolePrint(_iconsole_color_default, "Silly boy, you can not ban yourself!");
			return NULL;
		}
		if (index == 0) {
			IConsoleError("Invalid Client-ID");
			return NULL;
		}

		ci = NetworkFindClientInfoFromIndex(index);

		if (ci != NULL) {
			uint i;
			/* Add user to ban-list */
			for (i = 0; i < lengthof(_network_ban_list); i++) {
				if (_network_ban_list[i] == NULL || _network_ban_list[i][0] == '\0') {
					_network_ban_list[i] = strdup(inet_ntoa(*(struct in_addr *)&ci->client_ip));
					break;
				}
			}

			SEND_COMMAND(PACKET_SERVER_ERROR)(NetworkFindClientStateFromIndex(index), NETWORK_ERROR_KICKED);
			return NULL;
		} else {
			IConsoleError("Client-ID not found");
			return NULL;
		}
	}

	IConsolePrint(_iconsole_color_default, "Unknown usage. Usage: ban <client-id>. For client-ids, see 'clients'.");

	return NULL;
}

DEF_CONSOLE_CMD(ConUnBan)
{
	if (argc == 2) {
		uint i;
		for (i = 0; i < lengthof(_network_ban_list); i++) {
			if (_network_ban_list[i] == NULL || _network_ban_list[i][0] == '\0')
				continue;

			if (strncmp(_network_ban_list[i], argv[1], strlen(_network_ban_list[i])) == 0) {
				_network_ban_list[i][0] = '\0';
				IConsolePrint(_iconsole_color_default, "IP unbanned.");
				return NULL;
			}
		}

		IConsolePrint(_iconsole_color_default, "IP not in ban-list.");

		return NULL;
	}

	IConsolePrint(_iconsole_color_default, "Unknown usage. Usage: unban <ip>.");

	return NULL;
}

DEF_CONSOLE_CMD(ConBanList)
{
	uint i;

	IConsolePrint(_iconsole_color_default, "Banlist: ");

	for (i = 0; i < lengthof(_network_ban_list); i++) {
		if (_network_ban_list[i] == NULL || _network_ban_list[i][0] == '\0')
			continue;

		IConsolePrintF(_iconsole_color_default, "  %d) %s", i + 1, _network_ban_list[i]);
	}

	return NULL;
}

DEF_CONSOLE_CMD(ConRcon)
{
	if (argc < 3) {
		IConsolePrint(_iconsole_color_default, "Usage: rcon <password> <command>");
		return NULL;
	}

	SEND_COMMAND(PACKET_CLIENT_RCON)(argv[1], argv[2]);

	return NULL;
}

DEF_CONSOLE_CMD(ConStatus)
{
	const char *status;
	int lag;
	const NetworkClientState *cs;
	const NetworkClientInfo *ci;
	FOR_ALL_CLIENTS(cs) {
		lag = NetworkCalculateLag(cs);
		ci = DEREF_CLIENT_INFO(cs);

		switch (cs->status) {
			case STATUS_INACTIVE:
				status = "inactive";
				break;
			case STATUS_AUTH:
				status = "authorized";
				break;
			case STATUS_MAP:
				status = "loading map";
				break;
			case STATUS_DONE_MAP:
				status = "done map";
				break;
			case STATUS_PRE_ACTIVE:
				status = "ready";
				break;
			case STATUS_ACTIVE:
				status = "active";
				break;
			default:
				status = "unknown";
				break;
		}
		IConsolePrintF(8, "Client #%d/%s  status: %s  frame-lag: %d  play-as: %d  unique-id: %s",
			cs->index, ci->client_name, status, lag, ci->client_playas, ci->unique_id);
	}

	return NULL;
}

DEF_CONSOLE_CMD(ConKick)
{
	NetworkClientInfo *ci;

	if (argc == 2) {
		uint32 index = atoi(argv[1]);
		if (index == NETWORK_SERVER_INDEX) {
			IConsolePrint(_iconsole_color_default, "Silly boy, you can not kick yourself!");
			return NULL;
		}
		if (index == 0) {
			IConsoleError("Invalid Client-ID");
			return NULL;
		}

		ci = NetworkFindClientInfoFromIndex(index);

		if (ci != NULL) {
			SEND_COMMAND(PACKET_SERVER_ERROR)(NetworkFindClientStateFromIndex(index), NETWORK_ERROR_KICKED);
			return NULL;
		} else {
			IConsoleError("Client-ID not found");
			return NULL;
		}
	}

	IConsolePrint(_iconsole_color_default, "Unknown usage. Usage: kick <client-id>. For client-ids, see 'clients'.");

	return NULL;
}

DEF_CONSOLE_CMD(ConResetCompany)
{
	Player *p;
	NetworkClientState *cs;
	NetworkClientInfo *ci;

	if (argc == 2) {
		byte index = atoi(argv[1]);

		/* Check valid range */
		if (index < 1 || index > MAX_PLAYERS) {
			IConsolePrintF(_iconsole_color_error, "Company does not exist. Company-ID must be between 1 and %d.", MAX_PLAYERS);
			return NULL;
		}

		/* Check if company does exist */
		index--;
		p = DEREF_PLAYER(index);
		if (!p->is_active) {
			IConsolePrintF(_iconsole_color_error, "Company does not exist.");
			return NULL;
		}

		if (p->is_ai) {
			IConsolePrintF(_iconsole_color_error, "Company is owned by an AI.");
			return NULL;
		}

		/* Check if the company has active players */
		FOR_ALL_CLIENTS(cs) {
			ci = DEREF_CLIENT_INFO(cs);
			if (ci->client_playas-1 == index) {
				IConsolePrintF(_iconsole_color_error, "Cannot remove company: a client is connected to that company.");
				return NULL;
			}
		}
		ci = NetworkFindClientInfoFromIndex(NETWORK_SERVER_INDEX);
		if (ci->client_playas-1 == index) {
			IConsolePrintF(_iconsole_color_error, "Cannot remove company: a client is connected to that company.");
			return NULL;
		}

		/* It is safe to remove this company */
		DoCommandP(0, 2, index, NULL, CMD_PLAYER_CTRL);
		IConsolePrint(_iconsole_color_default, "Company deleted.");
		return NULL;
	}

	IConsolePrint(_iconsole_color_default, "Unknown usage. Usage: reset_company <company-id>.");

	return NULL;
}

DEF_CONSOLE_CMD(ConNetworkClients)
{
	NetworkClientInfo *ci;
	for (ci = _network_client_info; ci != &_network_client_info[MAX_CLIENT_INFO]; ci++) {
		if (ci->client_index != NETWORK_EMPTY_INDEX) {
			IConsolePrintF(8,"Client #%d   name: %s  play-as: %d", ci->client_index, ci->client_name, ci->client_playas);
		}
	}

	return NULL;
}

DEF_CONSOLE_CMD(ConNetworkConnect)
{
	char* ip;
	const byte *port = NULL;
	const byte *player = NULL;
	uint16 rport;

	if (argc<2) return NULL;

	if (_networking) {
		// We are in network-mode, first close it!
		NetworkDisconnect();
	}

	ip = argv[1];
	rport = NETWORK_DEFAULT_PORT;

	ParseConnectionString(&player, &port, ip);

	IConsolePrintF(_iconsole_color_default, "Connecting to %s...", ip);
	if (player != NULL) {
		_network_playas = atoi(player);
		IConsolePrintF(_iconsole_color_default, "    player-no: %s", player);
	}
	if (port != NULL) {
		rport = atoi(port);
		IConsolePrintF(_iconsole_color_default, "    port: %s", port);
	}

	NetworkClientConnectGame(ip, rport);

	return NULL;
}

#endif /* ENABLE_NETWORK */

/* ******************************** */
/*   script file console commands   */
/* ******************************** */

DEF_CONSOLE_CMD(ConExec)
{
	char cmd[1024];
	bool doerror;

	if (argc<2) return NULL;

	doerror = true;
	_script_file = fopen(argv[1], "r");

	if (_script_file == NULL) {
		if (argc>2) if (atoi(argv[2])==0) doerror=false;
		if (doerror) IConsoleError("script file not found");
		return NULL;
		}

	_script_running = true;

	fgets(cmd, sizeof(cmd), _script_file);
	while (!feof(_script_file) && _script_running) {
		strtok(cmd, "\r\n#");
		if (strlen(cmd) > 0 && cmd[0] != '#')
			IConsoleCmdExec(cmd);
		fgets(cmd, sizeof(cmd), _script_file);
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
extern bool CloseConsoleLogIfActive(void);

DEF_CONSOLE_CMD(ConScript)
{
	extern FILE* _iconsole_output_file;
	if (!CloseConsoleLogIfActive()) {
		if (argc < 2) return NULL;
		IConsolePrintF(_iconsole_color_default, "file output started to: %s",	argv[1]);
		_iconsole_output_file = fopen(argv[1], "ab");
		if (_iconsole_output_file == NULL) IConsoleError("could not open file");
	}

	return NULL;
}


DEF_CONSOLE_CMD(ConEcho)
{
	if (argc < 2) return NULL;
	IConsolePrint(_iconsole_color_default, argv[1]);
	return NULL;
}

DEF_CONSOLE_CMD(ConEchoC)
{
	if (argc < 3) return NULL;
	IConsolePrint(atoi(argv[1]), argv[2]);
	return NULL;
}

extern void SwitchMode(int new_mode);

DEF_CONSOLE_CMD(ConNewGame)
{
	_docommand_recursive = 0;

	_random_seeds[0][0] = Random();
	_random_seeds[0][1] = InteractiveRandom();

	SwitchMode(SM_NEWGAME);
	return NULL;
}

DEF_CONSOLE_CMD(ConPrintF)
{
	if (argc < 3) return NULL;
	IConsolePrintF(_iconsole_color_default, argv[1] , argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8], argv[9], argv[10], argv[11], argv[12], argv[13], argv[14], argv[15], argv[16], argv[17], argv[18], argv[19]); /* XXX ugh... */
	return NULL;
}

DEF_CONSOLE_CMD(ConPrintFC)
{
	if (argc < 3) return NULL;
	IConsolePrintF(atoi(argv[1]), argv[2] , argv[3], argv[4], argv[5], argv[6], argv[7], argv[8], argv[9], argv[10], argv[11], argv[12], argv[13], argv[14], argv[15], argv[16], argv[17], argv[18], argv[19]); /* XXX ugh... */
	return NULL;
}

DEF_CONSOLE_CMD(ConAlias)
{
	_iconsole_alias* alias;

	if (argc < 3) return NULL;

	alias = IConsoleAliasGet(argv[1]);
	if (alias == NULL) {
		IConsoleAliasRegister(argv[1],argv[2]);
	} else {
		free(alias->cmdline);
		alias->cmdline = strdup(argv[2]);
	}
	return NULL;
}

DEF_CONSOLE_CMD(ConScreenShot)
{
	if (argc < 2) {
		_make_screenshot = 1;
	} else {
		if (strcmp(argv[1], "big") == 0)
			_make_screenshot=2;
		if (strcmp(argv[1], "no_con") == 0) {
			IConsoleClose();
			_make_screenshot = 1;
		}
	}
	return NULL;
}

DEF_CONSOLE_CMD(ConInfoVar)
{
	if (argc < 2) return NULL;
	if (argt[1] != ICONSOLE_VAR_REFERENCE) {
		IConsoleError("first argument has to be a variable reference");
	} else {
		_iconsole_var* item;
		item = (_iconsole_var*)argv[1];
		IConsolePrintF(_iconsole_color_default, "var_name: %s", item->name);
		IConsolePrintF(_iconsole_color_default, "var_type: %i", item->type);
		IConsolePrintF(_iconsole_color_default, "var_addr: %i", item->data.addr);
		if (item->_malloc)
			IConsolePrintF(_iconsole_color_default, "var_malloc: internal");
		else
			IConsolePrintF(_iconsole_color_default, "var_malloc: external");
		if (item->hook_access) IConsoleWarning("var_access hooked");
		if (item->hook_before_change) IConsoleWarning("var_before_change hooked");
		if (item->hook_after_change) IConsoleWarning("var_after_change hooked");
	}
	return NULL;
}


DEF_CONSOLE_CMD(ConInfoCmd)
{
	if (argc < 2) return NULL;
	if (argt[1] != ICONSOLE_VAR_UNKNOWN) {
		IConsoleError("first argument has to be a command name");
	} else {
		_iconsole_cmd* item;
		item = IConsoleCmdGet(argv[1]);
		if (item == NULL) {
			IConsoleError("the given command was not found");
			return NULL;
		}
		IConsolePrintF(_iconsole_color_default, "cmd_name: %s", item->name);
		IConsolePrintF(_iconsole_color_default, "cmd_addr: %i", item->addr);
		if (item->hook_access) IConsoleWarning("cmd_access hooked");
		if (item->hook_before_exec) IConsoleWarning("cmd_before_exec hooked");
		if (item->hook_after_exec) IConsoleWarning("cmd_after_exec hooked");
	}
	return NULL;
}

DEF_CONSOLE_CMD(ConDebugLevel)
{
	if (argc < 2) return NULL;
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
	IConsolePrint(13, " -- console help -- ");
	IConsolePrint( 1, " variables: [command to list them: list_vars]");
	IConsolePrint( 1, " temp_string = \"my little \"");
	IConsolePrint( 1, "");
	IConsolePrint( 1, " commands: [command to list them: list_cmds]");
	IConsolePrint( 1, " [command] [\"string argument with spaces\"] [argument 2] ...");
	IConsolePrint( 1, " printf \"%s world\" temp_string");
	IConsolePrint( 1, "");
	IConsolePrint( 1, " command/variable returning a value into an variable:");
	IConsolePrint( 1, " temp_uint16 << random");
	IConsolePrint( 1, " temp_uint16 << temp_uint16_2");
	IConsolePrint( 1, "");
	return NULL;
}

DEF_CONSOLE_CMD(ConRandom)
{
	_iconsole_var* result;
	result = IConsoleVarAlloc(ICONSOLE_VAR_UINT16);
	IConsoleVarSetValue(result, rand());
	return result;
}

DEF_CONSOLE_CMD(ConListCommands)
{
	const _iconsole_cmd* item;
	size_t l = 0;

	if (argv[1] != NULL) l = strlen(argv[1]);

	for (item = _iconsole_cmds; item != NULL; item = item->_next)
		if (argv[1] == NULL || strncmp(item->name, argv[1], l) == 0)
			IConsolePrintF(_iconsole_color_default, "%s", item->name);

	return NULL;
}

DEF_CONSOLE_CMD(ConListVariables)
{
	const _iconsole_var* item;
	size_t l = 0;

	if (argv[1] != NULL) l = strlen(argv[1]);

	for (item = _iconsole_vars; item != NULL; item = item->_next)
		if (argv[1] == NULL || strncmp(item->name, argv[1], l) == 0)
			IConsolePrintF(_iconsole_color_default, "%s", item->name);

	return NULL;
}

DEF_CONSOLE_CMD(ConListAliases)
{
	const _iconsole_alias* item;
	size_t l = 0;

	if (argv[1] != NULL) l = strlen(argv[1]);

	for (item = _iconsole_aliases; item != NULL; item = item->_next)
		if (argv[1] == NULL || strncmp(item->name, argv[1], l) == 0)
			IConsolePrintF(_iconsole_color_default, "%s => %s", item->name, item->cmdline);

	return NULL;
}

DEF_CONSOLE_CMD(ConListDumpVariables)
{
	const _iconsole_var* item;
	size_t l = 0;

	if (argv[1] != NULL) l = strlen(argv[1]);

	for (item = _iconsole_vars; item != NULL; item = item->_next)
		if (argv[1] == NULL || strncmp(item->name, argv[1], l) == 0)
			IConsoleVarDump(item, NULL);

	return NULL;
}

#ifdef ENABLE_NETWORK

DEF_CONSOLE_CMD(ConSay)
{
	if (argc == 2) {
		if (!_network_server)
			SEND_COMMAND(PACKET_CLIENT_CHAT)(NETWORK_ACTION_CHAT, DESTTYPE_BROADCAST, 0 /* param does not matter */, argv[1]);
		else
			NetworkServer_HandleChat(NETWORK_ACTION_CHAT, DESTTYPE_BROADCAST, 0, argv[1], NETWORK_SERVER_INDEX);
	} else
		IConsolePrint(_iconsole_color_default, "Unknown usage. Usage: say \"<msg>\"");
	return NULL;
}

DEF_CONSOLE_CMD(ConSayPlayer)
{
	if (argc == 3) {
		if (atoi(argv[1]) < 1 || atoi(argv[1]) > MAX_PLAYERS) {
			IConsolePrintF(_iconsole_color_default, "Unknown player. Player range is between 1 and %d.", MAX_PLAYERS);
			return NULL;
		}

		if (!_network_server)
			SEND_COMMAND(PACKET_CLIENT_CHAT)(NETWORK_ACTION_CHAT_PLAYER, DESTTYPE_PLAYER, atoi(argv[1]), argv[2]);
		else
			NetworkServer_HandleChat(NETWORK_ACTION_CHAT_PLAYER, DESTTYPE_PLAYER, atoi(argv[1]), argv[2], NETWORK_SERVER_INDEX);
	} else
		IConsolePrint(_iconsole_color_default, "Unknown usage. Usage: say_player <playerno> \"<msg>\"");
	return NULL;
}

DEF_CONSOLE_CMD(ConSayClient)
{
	if (argc == 3) {
		if (!_network_server)
			SEND_COMMAND(PACKET_CLIENT_CHAT)(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, atoi(argv[1]), argv[2]);
		else
			NetworkServer_HandleChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, atoi(argv[1]), argv[2], NETWORK_SERVER_INDEX);
	} else
		IConsolePrint(_iconsole_color_default, "Unknown usage. Usage: say_client <clientno> \"<msg>\"");
	return NULL;
}

#endif /* ENABLE_NETWORK */

/* **************************** */
/*   the "set" command          */
/* **************************** */

extern void ConsoleSetPatchSetting(char *name, char *value);
extern void ConsoleGetPatchSetting(char *name);

DEF_CONSOLE_CMD(ConSet) {
	if (argc < 2) {
		IConsolePrint(_iconsole_color_warning, "Unknonw usage. Usage: set [setting] [value].");
		return NULL;
	}

#ifdef ENABLE_NETWORK

	// setting the server password
	if ((strcmp(argv[1],"server_pw") == 0) || (strcmp(argv[1],"server_password") == 0)) {
		if (!_network_server) {
			IConsolePrintF(_iconsole_color_error, "You are not the server");
			return NULL;
		}
		if (argc == 3) {
			// Change server password
			if (strncmp(argv[2], "*", NETWORK_PASSWORD_LENGTH) == 0) {
				_network_server_password[0] = '\0';
				_network_game_info.use_password = 0;
			} else {
				ttd_strlcpy(_network_server_password, argv[2], sizeof(_network_server_password));
				_network_game_info.use_password = 1;
			}
			IConsolePrintF(_iconsole_color_warning, "Game-password changed to '%s'", _network_server_password);
			ttd_strlcpy(_network_game_info.server_password, _network_server_password, sizeof(_network_game_info.server_password));
		} else {
			IConsolePrintF(_iconsole_color_default, "Current game-password is set to '%s'", _network_game_info.server_password);
			IConsolePrint(_iconsole_color_warning, "Usage: set server_pw \"<password>\".   Use * as <password> to set no password.");
		}
		return NULL;
	}

	// setting the rcon password
	if ((strcmp(argv[1], "rcon_pw") == 0) || (strcmp(argv[1], "rcon_password") == 0)) {
		if (!_network_server) {
			IConsolePrintF(_iconsole_color_error, "You are not the server");
			return NULL;
		}
		if (argc == 3) {
			// Change server password
			if (strncmp(argv[2], "*", NETWORK_PASSWORD_LENGTH) == 0) {
				_network_rcon_password[0] = '\0';
			} else {
				ttd_strlcpy(_network_rcon_password, argv[2], sizeof(_network_rcon_password));
			}
			IConsolePrintF(_iconsole_color_warning, "Rcon-password changed to '%s'", _network_rcon_password);
			ttd_strlcpy(_network_game_info.rcon_password, _network_rcon_password, sizeof(_network_game_info.rcon_password));
		} else {
			IConsolePrintF(_iconsole_color_default, "Current rcon-password is set to '%s'", _network_game_info.rcon_password);
			IConsolePrint(_iconsole_color_warning, "Usage: set rcon_pw \"<password>\".   Use * as <password> to disable rcon.");
		}
		return NULL;
	}

	// setting the company password
	if ((strcmp(argv[1],"company_pw") == 0) || (strcmp(argv[1],"company_password") == 0)) {
		if (!_networking) {
			IConsolePrintF(_iconsole_color_error,"No network game running");
			return NULL;
		}
		if (_local_player >= MAX_PLAYERS) {
			IConsolePrintF(_iconsole_color_default, "You have to own a company to make use of this command.");
			return NULL;
		}
		if (argc == 3) {
			NetworkChangeCompanyPassword(argv[2]);
		} else {
			IConsolePrint(_iconsole_color_default, "'set company_pw' sets a password for your company, so no-one without the correct password can join.");
			IConsolePrint(_iconsole_color_warning, "Usage: set company_pw \"<password>\".   Use * as <password> to set no password.");
		}
		return NULL;
	}

	// setting the player name
	if (strcmp(argv[1],"name") == 0) {
		NetworkClientInfo *ci;

		if (!_networking) {
			IConsolePrintF(_iconsole_color_error,"No network game running");
			return NULL;
		}

		ci = NetworkFindClientInfoFromIndex(_network_own_client_index);

		if (argc == 3 && ci != NULL) {
			if (!_network_server)
				SEND_COMMAND(PACKET_CLIENT_SET_NAME)(argv[2]);
			else {
				if (NetworkFindName(argv[2])) {
					NetworkTextMessage(NETWORK_ACTION_NAME_CHANGE, 1, false, ci->client_name, argv[2]);
					ttd_strlcpy(ci->client_name, argv[2], sizeof(ci->client_name));
					NetworkUpdateClientInfo(NETWORK_SERVER_INDEX);
				}
			}
			/* Also keep track of the new name on the client itself */
			ttd_strlcpy(_network_player_name, argv[2], sizeof(_network_player_name));
		} else {
			IConsolePrint(_iconsole_color_default, "With 'set name' you can change your network-player name.");
			IConsolePrint(_iconsole_color_warning, "Usage: set name \"<name>\".");
		}
		return NULL;
	}

	// setting the server name
	if (strcmp(argv[1],"server_name") == 0) {
		if (!_network_server) {
			IConsolePrintF(_iconsole_color_error, "You are not the server");
			return NULL;
		}
		if (argc == 3) {
			ttd_strlcpy(_network_server_name, argv[2], sizeof(_network_server_name));
			IConsolePrintF(_iconsole_color_warning, "Server-name changed to '%s'", _network_server_name);
			ttd_strlcpy(_network_game_info.server_name, _network_server_name, sizeof(_network_game_info.server_name));
		} else {
			IConsolePrintF(_iconsole_color_default, "Current server-name is '%s'", _network_server_name);
			IConsolePrint(_iconsole_color_warning, "Usage: set server_name \"<GameName>\".");
		}
		return NULL;
	}

	// setting the server port
	if (strcmp(argv[1],"server_port") == 0) {
		if (argc == 3 && atoi(argv[2]) != 0) {
			_network_server_port = atoi(argv[2]);
			IConsolePrintF(_iconsole_color_warning, "Server-port changed to '%d'", _network_server_port);
			IConsolePrintF(_iconsole_color_warning, "Changes will take effect the next time you start a server.");
		} else {
			IConsolePrintF(_iconsole_color_default, "Current server-port is '%d'", _network_server_port);
			IConsolePrint(_iconsole_color_warning, "Usage: set server_port <port>.");
		}
		return NULL;
	}

	// setting the server ip
	if (strcmp(argv[1],"server_bind_ip") == 0 || strcmp(argv[1],"server_ip_bind") == 0 ||
			strcmp(argv[1],"server_ip") == 0 || strcmp(argv[1],"server_bind") == 0) {
		if (argc == 3) {
			_network_server_bind_ip = inet_addr(argv[2]);
			snprintf(_network_server_bind_ip_host, sizeof(_network_server_bind_ip_host), "%s", inet_ntoa(*(struct in_addr *)&_network_server_bind_ip));
			IConsolePrintF(_iconsole_color_warning, "Server-bind-ip changed to '%s'", _network_server_bind_ip_host);
			IConsolePrintF(_iconsole_color_warning, "Changes will take effect the next time you start a server.");
		} else {
			IConsolePrintF(_iconsole_color_default, "Current server-bind-ip is '%s'", _network_server_bind_ip_host);
			IConsolePrint(_iconsole_color_warning, "Usage: set server_bind_ip <ip>.");
		}
		return NULL;
	}

	// setting the server advertising on/off
	if (strcmp(argv[1],"server_advertise") == 0) {
		if (!_network_server) {
			IConsolePrintF(_iconsole_color_error, "You are not the server");
			return NULL;
		}
		if (argc == 3) {
			if (strcmp(argv[2], "on") == 0 || atoi(argv[2]) == 1)
				_network_advertise = true;
			else {
				NetworkUDPRemoveAdvertise();
				_network_advertise = false;
			}
			IConsolePrintF(_iconsole_color_warning, "Server-advertise changed to '%s'", (_network_advertise)?"on":"off");
		} else {
			IConsolePrintF(_iconsole_color_default, "Current server-advertise is '%s'", (_network_advertise)?"on":"off");
			IConsolePrint(_iconsole_color_warning, "Usage: set server_advertise on/off.");
		}
		return NULL;
	}

	// setting the server autoclean on/off
	if (strcmp(argv[1],"autoclean_companies") == 0) {
		if (!_network_server) {
			IConsolePrintF(_iconsole_color_error, "You are not the server");
			return NULL;
		}
		if (argc == 3) {
			if (strcmp(argv[2], "on") == 0 || atoi(argv[2]) == 1)
				_network_autoclean_companies = true;
			else
				_network_autoclean_companies = false;
			IConsolePrintF(_iconsole_color_warning, "Autoclean-companies changed to '%s'", (_network_autoclean_companies)?"on":"off");
		} else {
			IConsolePrintF(_iconsole_color_default, "Current autoclean-companies is '%s'", (_network_autoclean_companies)?"on":"off");
			IConsolePrint(_iconsole_color_warning, "Usage: set autoclean_companies on/off.");
		}
		return NULL;
	}

	// setting the server autoclean protected
	if (strcmp(argv[1],"autoclean_protected") == 0) {
		if (!_network_server) {
			IConsolePrintF(_iconsole_color_error, "You are not the server");
			return NULL;
		}
		if (argc == 3) {
			_network_autoclean_protected = atoi(argv[2]);
			IConsolePrintF(_iconsole_color_warning, "Autoclean-protected changed to '%d'", _network_autoclean_protected);
		} else {
			IConsolePrintF(_iconsole_color_default, "Current autoclean-protected is '%d'", _network_autoclean_protected);
			IConsolePrint(_iconsole_color_warning, "Usage: set autoclean_protected <months>.");
		}
		return NULL;
	}

	// setting the server autoclean protected
	if (strcmp(argv[1],"autoclean_unprotected") == 0) {
		if (!_network_server) {
			IConsolePrintF(_iconsole_color_error, "You are not the server");
			return NULL;
		}
		if (argc == 3) {
			_network_autoclean_unprotected = atoi(argv[2]);
			IConsolePrintF(_iconsole_color_warning, "Autoclean-unprotected changed to '%d'", _network_autoclean_unprotected);
		} else {
			IConsolePrintF(_iconsole_color_default, "Current autoclean-unprotected is '%d'", _network_autoclean_unprotected);
			IConsolePrint(_iconsole_color_warning, "Usage: set autoclean_unprotected <months>.");
		}
		return NULL;
	}

	// setting the server auto restart date
	if (strcmp(argv[1],"restart_game_date") == 0) {
		if (!_network_server) {
			IConsolePrintF(_iconsole_color_error, "You are not the server");
			return NULL;
		}
		if (argc == 3) {
			_network_restart_game_date = atoi(argv[2]);
			IConsolePrintF(_iconsole_color_warning, "Restart Game Date changed to '%d'", _network_restart_game_date);
		} else {
			IConsolePrintF(_iconsole_color_default, "Current Restart Game Date is '%d'", _network_restart_game_date);
			IConsolePrint(_iconsole_color_warning, "Usage: set restart_game_date <year>. '0' means disabled.");
			IConsolePrint(_iconsole_color_warning, " Auto-restart the server when 1 jan of this year is reached (e.g.: 2030).");
		}
		return NULL;
	}

#endif /* ENABLE_NETWORK */

	// Patch-options
	if (strcmp(argv[1],"patch") == 0) {
		if (_networking && !_network_server) {
			IConsolePrintF(_iconsole_color_error, "You are not the server");
			return NULL;
		}
		if (argc == 3)
			ConsoleGetPatchSetting(argv[2]);
		else if (argc == 4)
			ConsoleSetPatchSetting(argv[2], argv[3]);
		else
			IConsolePrint(_iconsole_color_warning, "Usage: set patch <patch_name> [<value>].");
		return NULL;
	}


	IConsolePrint(_iconsole_color_error, "Unknown setting");
	IConsolePrint(_iconsole_color_error, "Known settings are:");
#ifdef ENABLE_NETWORK
	IConsolePrint(_iconsole_color_error, " - autoclean_companies on/off");
	IConsolePrint(_iconsole_color_error, " - autoclean_protected <months>");
	IConsolePrint(_iconsole_color_error, " - autoclean_unprotected <months>");
	IConsolePrint(_iconsole_color_error, " - company_pw \"<password>\"");
	IConsolePrint(_iconsole_color_error, " - name \"<playername>\"");
	IConsolePrint(_iconsole_color_error, " - rcon_pw \"<password>\"");
	IConsolePrint(_iconsole_color_error, " - server_name \"<name>\"");
	IConsolePrint(_iconsole_color_error, " - server_advertise on/off");
	IConsolePrint(_iconsole_color_error, " - server_bind_ip <ip>");
	IConsolePrint(_iconsole_color_error, " - server_port <port>");
	IConsolePrint(_iconsole_color_error, " - server_pw \"<password>\"");
	IConsolePrint(_iconsole_color_error, " - restart_game_date \"<year>\"");
#endif /* ENABLE_NETWORK */
	IConsolePrint(_iconsole_color_error, " - patch <patch_name> [<value>]");

	return NULL;
}


#ifdef _DEBUG
/* ****************************************** */
/*  debug commands and variables */
/* ****************************************** */

static void IConsoleDebugLibRegister(void)
{
	// debugging variables and functions
	extern bool _stdlib_con_developer; /* XXX extern in .c */

	IConsoleVarRegister("con_developer", &_stdlib_con_developer, ICONSOLE_VAR_BOOLEAN);
	IConsoleVarMemRegister("temp_string2", ICONSOLE_VAR_STRING);
	IConsoleVarMemRegister("temp_uint16_2", ICONSOLE_VAR_UINT16);
	IConsoleCmdRegister("resettile", ConResetTile);
	IConsoleAliasRegister("dbg_echo","echo %A; echo %B");
	IConsoleAliasRegister("dbg_echo2","echo %+");
}
#endif

/* ****************************************** */
/*  console command and variable registration */
/* ****************************************** */

void IConsoleStdLibRegister(void)
{
	// stdlib
	extern byte _stdlib_developer; /* XXX extern in .c */

	// default variables and functions
	IConsoleCmdRegister("debug_level",  ConDebugLevel);
	IConsoleCmdRegister("dump_vars",    ConListDumpVariables);
	IConsoleCmdRegister("echo",         ConEcho);
	IConsoleCmdRegister("echoc",        ConEchoC);
	IConsoleCmdRegister("exec",         ConExec);
	IConsoleCmdRegister("exit",         ConExit);
	IConsoleCmdRegister("help",         ConHelp);
	IConsoleCmdRegister("info_cmd",     ConInfoCmd);
	IConsoleCmdRegister("info_var",     ConInfoVar);
	IConsoleCmdRegister("list_cmds",    ConListCommands);
	IConsoleCmdRegister("list_vars",    ConListVariables);
	IConsoleCmdRegister("list_aliases",    ConListAliases);
	IConsoleCmdRegister("newgame",         ConNewGame);
	IConsoleCmdRegister("printf",       ConPrintF);
	IConsoleCmdRegister("printfc",      ConPrintFC);
	IConsoleCmdRegister("quit",         ConExit);
	IConsoleCmdRegister("random",       ConRandom);
	IConsoleCmdRegister("resetengines", ConResetEngines);
	IConsoleCmdRegister("return",     ConReturn);
	IConsoleCmdRegister("screenshot", ConScreenShot);
	IConsoleCmdRegister("script",     ConScript);
	IConsoleCmdRegister("scrollto",   ConScrollToTile);
	IConsoleCmdRegister("set",			ConSet);
	IConsoleCmdRegister("alias",		ConAlias);
	IConsoleCmdRegister("load",			ConLoad);
	IConsoleCmdRegister("list_files", ConListFiles);
	IConsoleCmdRegister("scan_files", ConScanFiles);
	IConsoleCmdRegister("goto_dir", ConGotoDir);
	IConsoleAliasRegister("new_game", "newgame");
	IConsoleAliasRegister("newmap", "newgame");
	IConsoleAliasRegister("new_map", "newgame");
	IConsoleAliasRegister("load_game", "temp_string << scan_files %!;load temp_string");

	IConsoleVarRegister("developer", &_stdlib_developer, ICONSOLE_VAR_BYTE);

	// temporary data containers for alias scripting
	IConsoleVarMemRegister("temp_string", ICONSOLE_VAR_STRING);
	IConsoleVarMemRegister("temp_bool", ICONSOLE_VAR_BOOLEAN);
	IConsoleVarMemRegister("temp_int16", ICONSOLE_VAR_INT16);
	IConsoleVarMemRegister("temp_int32", ICONSOLE_VAR_INT32);
	IConsoleVarMemRegister("temp_pointer", ICONSOLE_VAR_POINTER);
	IConsoleVarMemRegister("temp_uint16", ICONSOLE_VAR_UINT16);
	IConsoleVarMemRegister("temp_uint32", ICONSOLE_VAR_UINT32);


	// networking variables and functions
#ifdef ENABLE_NETWORK
	IConsoleCmdRegister("say",        ConSay);
	IConsoleCmdHook("say", ICONSOLE_HOOK_ACCESS, ConCmdHookNeedNetwork);
	IConsoleCmdRegister("say_player", ConSayPlayer);
	IConsoleCmdHook("say_player", ICONSOLE_HOOK_ACCESS, ConCmdHookNeedNetwork);
	IConsoleCmdRegister("say_client", ConSayClient);
	IConsoleCmdHook("say_client", ICONSOLE_HOOK_ACCESS, ConCmdHookNeedNetwork);
	IConsoleCmdRegister("kick",         ConKick);
	IConsoleCmdHook("kick", ICONSOLE_HOOK_ACCESS, ConCmdHookNoNetClient);
	IConsoleCmdRegister("reset_company",         ConResetCompany);
	IConsoleCmdHook("reset_company", ICONSOLE_HOOK_ACCESS, ConCmdHookNoNetClient);
	IConsoleCmdRegister("connect", ConNetworkConnect);
	IConsoleCmdHook("connect", ICONSOLE_HOOK_ACCESS, ConCmdHookNoNetServer);
	IConsoleCmdRegister("clients", ConNetworkClients);
	IConsoleCmdRegister("status",   ConStatus);
	IConsoleCmdHook("status", ICONSOLE_HOOK_ACCESS, ConCmdHookNoNetClient);
	IConsoleCmdHook("resetengines", ICONSOLE_HOOK_ACCESS, ConCmdHookNoNetwork);

	IConsoleCmdRegister("rcon",        ConRcon);
	IConsoleCmdHook("rcon", ICONSOLE_HOOK_ACCESS, ConCmdHookNeedNetwork);

	IConsoleCmdRegister("ban",   ConBan);
	IConsoleCmdHook("ban", ICONSOLE_HOOK_ACCESS, ConCmdHookNoNetClient);
	IConsoleCmdRegister("unban",   ConUnBan);
	IConsoleCmdHook("unban", ICONSOLE_HOOK_ACCESS, ConCmdHookNoNetClient);
	IConsoleCmdRegister("banlist",   ConBanList);
	IConsoleCmdHook("banlist", ICONSOLE_HOOK_ACCESS, ConCmdHookNoNetClient);

	IConsoleAliasRegister("clean_company",		"reset_company %A");

	IConsoleVarRegister("net_frame_freq", &_network_frame_freq, ICONSOLE_VAR_UINT8);
	IConsoleVarHook("net_frame_freq", ICONSOLE_HOOK_ACCESS, ConVarHookNoNetClient);
	IConsoleVarRegister("net_sync_freq", &_network_sync_freq, ICONSOLE_VAR_UINT16);
	IConsoleVarHook("net_sync_freq", ICONSOLE_HOOK_ACCESS, ConVarHookNoNetClient);
#endif /* ENABLE_NETWORK */

	// debugging stuff
#ifdef _DEBUG
	IConsoleDebugLibRegister();
#endif

}
/* -------------------- don't cross this line --------------------- */
