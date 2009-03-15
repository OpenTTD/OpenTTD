/* $Id$ */

/** @file console_cmds.cpp Implementation of the console hooks. */

#include "stdafx.h"
#include "openttd.h"
#include "console_internal.h"
#include "debug.h"
#include "engine_func.h"
#include "landscape.h"
#include "saveload/saveload.h"
#include "variables.h"
#include "network/network.h"
#include "network/network_func.h"
#include "network/network_base.h"
#include "command_func.h"
#include "settings_func.h"
#include "fios.h"
#include "fileio_func.h"
#include "screenshot.h"
#include "genworld.h"
#include "strings_func.h"
#include "viewport_func.h"
#include "window_func.h"
#include "map_func.h"
#include "date_func.h"
#include "vehicle_func.h"
#include "string_func.h"
#include "company_func.h"
#include "company_base.h"
#include "settings_type.h"
#include "gamelog.h"
#include "ai/ai.hpp"
#include "ai/ai_config.hpp"

#ifdef ENABLE_NETWORK
	#include "table/strings.h"
#endif /* ENABLE_NETWORK */

/* scriptfile handling */
static FILE *_script_file;
static bool _script_running;

/* console command / variable defines */
#define DEF_CONSOLE_CMD(function) static bool function(byte argc, char *argv[])
#define DEF_CONSOLE_HOOK(function) static bool function()


/*****************************
 * variable and command hooks
 *****************************/

#ifdef ENABLE_NETWORK

static inline bool NetworkAvailable()
{
	if (!_network_available) {
		IConsoleError("You cannot use this command because there is no network available.");
		return false;
	}
	return true;
}

DEF_CONSOLE_HOOK(ConHookServerOnly)
{
	if (!NetworkAvailable()) return false;

	if (!_network_server) {
		IConsoleError("This command/variable is only available to a network server.");
		return false;
	}
	return true;
}

DEF_CONSOLE_HOOK(ConHookClientOnly)
{
	if (!NetworkAvailable()) return false;

	if (_network_server) {
		IConsoleError("This command/variable is not available to a network server.");
		return false;
	}
	return true;
}

DEF_CONSOLE_HOOK(ConHookNeedNetwork)
{
	if (!NetworkAvailable()) return false;

	if (!_networking) {
		IConsoleError("Not connected. This command/variable is only available in multiplayer.");
		return false;
	}
	return true;
}

DEF_CONSOLE_HOOK(ConHookNoNetwork)
{
	if (_networking) {
		IConsoleError("This command/variable is forbidden in multiplayer.");
		return false;
	}
	return true;
}

#endif /* ENABLE_NETWORK */

static void IConsoleHelp(const char *str)
{
	IConsolePrintF(CC_WARNING, "- %s", str);
}

DEF_CONSOLE_CMD(ConResetEngines)
{
	if (argc == 0) {
		IConsoleHelp("Reset status data of all engines. This might solve some issues with 'lost' engines. Usage: 'resetengines'");
		return true;
	}

	StartupEngines();
	return true;
}

#ifdef _DEBUG
DEF_CONSOLE_CMD(ConResetTile)
{
	if (argc == 0) {
		IConsoleHelp("Reset a tile to bare land. Usage: 'resettile <tile>'");
		IConsoleHelp("Tile can be either decimal (34161) or hexadecimal (0x4a5B)");
		return true;
	}

	if (argc == 2) {
		uint32 result;
		if (GetArgumentInteger(&result, argv[1])) {
			DoClearSquare((TileIndex)result);
			return true;
		}
	}

	return false;
}

DEF_CONSOLE_CMD(ConStopAllVehicles)
{
	if (argc == 0) {
		IConsoleHelp("Stops all vehicles in the game. For debugging only! Use at your own risk... Usage: 'stopall'");
		return true;
	}

	StopAllVehicles();
	return true;
}
#endif /* _DEBUG */

DEF_CONSOLE_CMD(ConScrollToTile)
{
	if (argc == 0) {
		IConsoleHelp("Center the screen on a given tile. Usage: 'scrollto <tile>'");
		IConsoleHelp("Tile can be either decimal (34161) or hexadecimal (0x4a5B)");
		return true;
	}

	if (argc == 2) {
		uint32 result;
		if (GetArgumentInteger(&result, argv[1])) {
			if (result >= MapSize()) {
				IConsolePrint(CC_ERROR, "Tile does not exist");
				return true;
			}
			ScrollMainWindowToTile((TileIndex)result);
			return true;
		}
	}

	return false;
}

extern void BuildFileList();
extern void SetFiosType(const byte fiostype);

/* Save the map to a file */
DEF_CONSOLE_CMD(ConSave)
{
	if (argc == 0) {
		IConsoleHelp("Save the current game. Usage: 'save <filename>'");
		return true;
	}

	if (argc == 2) {
		char *filename = str_fmt("%s.sav", argv[1]);
		IConsolePrint(CC_DEFAULT, "Saving map...");

		if (SaveOrLoad(filename, SL_SAVE, SAVE_DIR) != SL_OK) {
			IConsolePrint(CC_ERROR, "Saving map failed");
		} else {
			IConsolePrintF(CC_DEFAULT, "Map sucessfully saved to %s", filename);
		}
		free(filename);
		return true;
	}

	return false;
}

/* Explicitly save the configuration */
DEF_CONSOLE_CMD(ConSaveConfig)
{
	if (argc == 0) {
		IConsoleHelp("Saves the current config, typically to 'openttd.cfg'.");
		return true;
	}

	SaveToConfig();
	IConsolePrint(CC_DEFAULT, "Saved config.");
	return true;
}

static const FiosItem *GetFiosItem(const char *file)
{
	_saveload_mode = SLD_LOAD_GAME;
	BuildFileList();

	for (const FiosItem *item = _fios_items.Begin(); item != _fios_items.End(); item++) {
		if (strcmp(file, item->name) == 0) return item;
		if (strcmp(file, item->title) == 0) return item;
	}

	/* If no name matches, try to parse it as number */
	char *endptr;
	int i = strtol(file, &endptr, 10);
	if (file == endptr || *endptr != '\0') i = -1;

	return IsInsideMM(i, 0, _fios_items.Length()) ? _fios_items.Get(i) : NULL;
}


DEF_CONSOLE_CMD(ConLoad)
{
	if (argc == 0) {
		IConsoleHelp("Load a game by name or index. Usage: 'load <file | number>'");
		return true;
	}

	if (argc != 2) return false;

	const char *file = argv[1];
	const FiosItem *item = GetFiosItem(file);
	if (item != NULL) {
		switch (item->type) {
			case FIOS_TYPE_FILE: case FIOS_TYPE_OLDFILE: {
				_switch_mode = SM_LOAD;
				SetFiosType(item->type);

				strecpy(_file_to_saveload.name, FiosBrowseTo(item), lastof(_file_to_saveload.name));
				strecpy(_file_to_saveload.title, item->title, lastof(_file_to_saveload.title));
			} break;
			default: IConsolePrintF(CC_ERROR, "%s: Not a savegame.", file);
		}
	} else {
		IConsolePrintF(CC_ERROR, "%s: No such file or directory.", file);
	}

	FiosFreeSavegameList();
	return true;
}


DEF_CONSOLE_CMD(ConRemove)
{
	if (argc == 0) {
		IConsoleHelp("Remove a savegame by name or index. Usage: 'rm <file | number>'");
		return true;
	}

	if (argc != 2) return false;

	const char *file = argv[1];
	const FiosItem *item = GetFiosItem(file);
	if (item != NULL) {
		if (!FiosDelete(item->name))
			IConsolePrintF(CC_ERROR, "%s: Failed to delete file", file);
	} else {
		IConsolePrintF(CC_ERROR, "%s: No such file or directory.", file);
	}

	FiosFreeSavegameList();
	return true;
}


/* List all the files in the current dir via console */
DEF_CONSOLE_CMD(ConListFiles)
{
	if (argc == 0) {
		IConsoleHelp("List all loadable savegames and directories in the current dir via console. Usage: 'ls | dir'");
		return true;
	}

	BuildFileList();

	for (uint i = 0; i < _fios_items.Length(); i++) {
		IConsolePrintF(CC_DEFAULT, "%d) %s", i, _fios_items[i].title);
	}

	FiosFreeSavegameList();
	return true;
}

/* Change the dir via console */
DEF_CONSOLE_CMD(ConChangeDirectory)
{
	if (argc == 0) {
		IConsoleHelp("Change the dir via console. Usage: 'cd <directory | number>'");
		return true;
	}

	if (argc != 2) return false;

	const char *file = argv[1];
	const FiosItem *item = GetFiosItem(file);
	if (item != NULL) {
		switch (item->type) {
			case FIOS_TYPE_DIR: case FIOS_TYPE_DRIVE: case FIOS_TYPE_PARENT:
				FiosBrowseTo(item);
				break;
			default: IConsolePrintF(CC_ERROR, "%s: Not a directory.", file);
		}
	} else {
		IConsolePrintF(CC_ERROR, "%s: No such file or directory.", file);
	}

	FiosFreeSavegameList();
	return true;
}

DEF_CONSOLE_CMD(ConPrintWorkingDirectory)
{
	const char *path;

	if (argc == 0) {
		IConsoleHelp("Print out the current working directory. Usage: 'pwd'");
		return true;
	}

	/* XXX - Workaround for broken file handling */
	FiosGetSavegameList(SLD_LOAD_GAME);
	FiosFreeSavegameList();

	FiosGetDescText(&path, NULL);
	IConsolePrint(CC_DEFAULT, path);
	return true;
}

DEF_CONSOLE_CMD(ConClearBuffer)
{
	if (argc == 0) {
		IConsoleHelp("Clear the console buffer. Usage: 'clear'");
		return true;
	}

	IConsoleClearBuffer();
	InvalidateWindow(WC_CONSOLE, 0);
	return true;
}


/**********************************
 * Network Core Console Commands
 **********************************/
#ifdef ENABLE_NETWORK

DEF_CONSOLE_CMD(ConBan)
{
	NetworkClientInfo *ci;
	const char *banip = NULL;
	ClientID client_id;

	if (argc == 0) {
		IConsoleHelp("Ban a client from a network game. Usage: 'ban <ip | client-id>'");
		IConsoleHelp("For client-id's, see the command 'clients'");
		IConsoleHelp("If the client is no longer online, you can still ban his/her IP");
		return true;
	}

	if (argc != 2) return false;

	if (strchr(argv[1], '.') == NULL) { // banning with ID
		client_id = (ClientID)atoi(argv[1]);
		ci = NetworkFindClientInfoFromClientID(client_id);
	} else { // banning IP
		ci = NetworkFindClientInfoFromIP(argv[1]);
		if (ci == NULL) {
			banip = argv[1];
			client_id = (ClientID)-1;
		} else {
			client_id = ci->client_id;
		}
	}

	if (client_id == CLIENT_ID_SERVER) {
		IConsoleError("Silly boy, you can not ban yourself!");
		return true;
	}

	if (client_id == INVALID_CLIENT_ID || (ci == NULL && client_id != (ClientID)-1)) {
		IConsoleError("Invalid client");
		return true;
	}

	if (ci != NULL) {
		IConsolePrint(CC_DEFAULT, "Client banned");
		banip = GetClientIP(ci);
	} else {
		IConsolePrint(CC_DEFAULT, "Client not online, banned IP");
	}

	NetworkServerBanIP(banip);

	return true;
}

DEF_CONSOLE_CMD(ConUnBan)
{
	uint i, index;

	if (argc == 0) {
		IConsoleHelp("Unban a client from a network game. Usage: 'unban <ip | client-id>'");
		IConsoleHelp("For a list of banned IP's, see the command 'banlist'");
		return true;
	}

	if (argc != 2) return false;

	index = (strchr(argv[1], '.') == NULL) ? atoi(argv[1]) : 0;
	index--;

	for (i = 0; i < lengthof(_network_ban_list); i++) {
		if (_network_ban_list[i] == NULL) continue;

		if (strcmp(_network_ban_list[i], argv[1]) == 0 || index == i) {
			free(_network_ban_list[i]);
			_network_ban_list[i] = NULL;
			IConsolePrint(CC_DEFAULT, "IP unbanned.");
			return true;
		}
	}

	IConsolePrint(CC_DEFAULT, "IP not in ban-list.");
	return true;
}

DEF_CONSOLE_CMD(ConBanList)
{
	uint i;

	if (argc == 0) {
		IConsoleHelp("List the IP's of banned clients: Usage 'banlist'");
		return true;
	}

	IConsolePrint(CC_DEFAULT, "Banlist: ");

	for (i = 0; i < lengthof(_network_ban_list); i++) {
		if (_network_ban_list[i] != NULL)
			IConsolePrintF(CC_DEFAULT, "  %d) %s", i + 1, _network_ban_list[i]);
	}

	return true;
}

DEF_CONSOLE_CMD(ConPauseGame)
{
	if (argc == 0) {
		IConsoleHelp("Pause a network game. Usage: 'pause'");
		return true;
	}

	if (_pause_game == 0) {
		DoCommandP(0, 1, 0, CMD_PAUSE);
		IConsolePrint(CC_DEFAULT, "Game paused.");
	} else {
		IConsolePrint(CC_DEFAULT, "Game is already paused.");
	}

	return true;
}

DEF_CONSOLE_CMD(ConUnPauseGame)
{
	if (argc == 0) {
		IConsoleHelp("Unpause a network game. Usage: 'unpause'");
		return true;
	}

	if (_pause_game != 0) {
		DoCommandP(0, 0, 0, CMD_PAUSE);
		IConsolePrint(CC_DEFAULT, "Game unpaused.");
	} else {
		IConsolePrint(CC_DEFAULT, "Game is already unpaused.");
	}

	return true;
}

DEF_CONSOLE_CMD(ConRcon)
{
	if (argc == 0) {
		IConsoleHelp("Remote control the server from another client. Usage: 'rcon <password> <command>'");
		IConsoleHelp("Remember to enclose the command in quotes, otherwise only the first parameter is sent");
		return true;
	}

	if (argc < 3) return false;

	if (_network_server) {
		IConsoleCmdExec(argv[2]);
	} else {
		NetworkClientSendRcon(argv[1], argv[2]);
	}
	return true;
}

DEF_CONSOLE_CMD(ConStatus)
{
	if (argc == 0) {
		IConsoleHelp("List the status of all clients connected to the server. Usage 'status'");
		return true;
	}

	NetworkServerShowStatusToConsole();
	return true;
}

DEF_CONSOLE_CMD(ConServerInfo)
{
	if (argc == 0) {
		IConsoleHelp("List current and maximum client/company limits. Usage 'server_info'");
		IConsoleHelp("You can change these values by setting the variables 'max_clients', 'max_companies' and 'max_spectators'");
		return true;
	}

	IConsolePrintF(CC_DEFAULT, "Current/maximum clients:    %2d/%2d", _network_game_info.clients_on, _settings_client.network.max_clients);
	IConsolePrintF(CC_DEFAULT, "Current/maximum companies:  %2d/%2d", ActiveCompanyCount(), _settings_client.network.max_companies);
	IConsolePrintF(CC_DEFAULT, "Current/maximum spectators: %2d/%2d", NetworkSpectatorCount(), _settings_client.network.max_spectators);

	return true;
}

DEF_CONSOLE_CMD(ConClientNickChange)
{
	if (argc != 3) {
		IConsoleHelp("Change the nickname of a connected client. Usage: 'client_name <client-id> <new-name>'");
		IConsoleHelp("For client-id's, see the command 'clients'");
		return true;
	}

	ClientID client_id = (ClientID)atoi(argv[1]);

	if (client_id == CLIENT_ID_SERVER) {
		IConsoleError("Please use the command 'name' to change your own name!");
		return true;
	}

	if (NetworkFindClientInfoFromClientID(client_id) == NULL) {
		IConsoleError("Invalid client");
		return true;
	}

	if (!NetworkServerChangeClientName(client_id, argv[2])) {
		IConsoleError("Cannot give a client a duplicate name");
	}

	return true;
}

DEF_CONSOLE_CMD(ConKick)
{
	NetworkClientInfo *ci;
	ClientID client_id;

	if (argc == 0) {
		IConsoleHelp("Kick a client from a network game. Usage: 'kick <ip | client-id>'");
		IConsoleHelp("For client-id's, see the command 'clients'");
		return true;
	}

	if (argc != 2) return false;

	if (strchr(argv[1], '.') == NULL) {
		client_id = (ClientID)atoi(argv[1]);
		ci = NetworkFindClientInfoFromClientID(client_id);
	} else {
		ci = NetworkFindClientInfoFromIP(argv[1]);
		client_id = (ci == NULL) ? INVALID_CLIENT_ID : ci->client_id;
	}

	if (client_id == CLIENT_ID_SERVER) {
		IConsoleError("Silly boy, you can not kick yourself!");
		return true;
	}

	if (client_id == INVALID_CLIENT_ID) {
		IConsoleError("Invalid client");
		return true;
	}

	if (ci != NULL) {
		NetworkServerKickClient(client_id);
	} else {
		IConsoleError("Client not found");
	}

	return true;
}

DEF_CONSOLE_CMD(ConJoinCompany)
{
	if (argc < 2) {
		IConsoleHelp("Request joining another company. Usage: join <company-id> [<password>]");
		IConsoleHelp("For valid company-id see company list, use 255 for spectator");
		return true;
	}

	CompanyID company_id = (CompanyID)(atoi(argv[1]) <= MAX_COMPANIES ? atoi(argv[1]) - 1 : atoi(argv[1]));

	/* Check we have a valid company id! */
	if (!IsValidCompanyID(company_id) && company_id != COMPANY_SPECTATOR) {
		IConsolePrintF(CC_ERROR, "Company does not exist. Company-id must be between 1 and %d.", MAX_COMPANIES);
		return true;
	}

	if (NetworkFindClientInfoFromClientID(_network_own_client_id)->client_playas == company_id) {
		IConsoleError("You are already there!");
		return true;
	}

	if (company_id == COMPANY_SPECTATOR && NetworkMaxSpectatorsReached()) {
		IConsoleError("Cannot join spectators, maximum number of spectators reached.");
		return true;
	}

	/* Check if the company requires a password */
	if (NetworkCompanyIsPassworded(company_id) && argc < 3) {
		IConsolePrintF(CC_ERROR, "Company %d requires a password to join.", company_id + 1);
		return true;
	}

	/* non-dedicated server may just do the move! */
	if (_network_server) {
		NetworkServerDoMove(CLIENT_ID_SERVER, company_id);
	} else {
		NetworkClientRequestMove(company_id, NetworkCompanyIsPassworded(company_id) ? argv[2] : "");
	}

	return true;
}

DEF_CONSOLE_CMD(ConMoveClient)
{
	if (argc < 3) {
		IConsoleHelp("Move a client to another company. Usage: move <client-id> <company-id>");
		IConsoleHelp("For valid client-id see 'clients', for valid company-id see 'companies', use 255 for moving to spectators");
		return true;
	}

	const NetworkClientInfo *ci = NetworkFindClientInfoFromClientID((ClientID)atoi(argv[1]));
	CompanyID company_id = (CompanyID)(atoi(argv[2]) <= MAX_COMPANIES ? atoi(argv[2]) - 1 : atoi(argv[2]));

	/* check the client exists */
	if (ci == NULL) {
		IConsoleError("Invalid client-id, check the command 'clients' for valid client-id's.");
		return true;
	}

	if (!IsValidCompanyID(company_id) && company_id != COMPANY_SPECTATOR) {
		IConsolePrintF(CC_ERROR, "Company does not exist. Company-id must be between 1 and %d.", MAX_COMPANIES);
		return true;
	}

	if (ci->client_id == CLIENT_ID_SERVER && _network_dedicated) {
		IConsoleError("Silly boy, you cannot move the server!");
		return true;
	}

	if (ci->client_playas == company_id) {
		IConsoleError("You cannot move someone to where he/she already is!");
		return true;
	}

	/* we are the server, so force the update */
	NetworkServerDoMove(ci->client_id, company_id);

	return true;
}

DEF_CONSOLE_CMD(ConResetCompany)
{
	CompanyID index;

	if (argc == 0) {
		IConsoleHelp("Remove an idle company from the game. Usage: 'reset_company <company-id>'");
		IConsoleHelp("For company-id's, see the list of companies from the dropdown menu. Company 1 is 1, etc.");
		return true;
	}

	if (argc != 2) return false;

	index = (CompanyID)(atoi(argv[1]) - 1);

	/* Check valid range */
	if (!IsValidCompanyID(index)) {
		IConsolePrintF(CC_ERROR, "Company does not exist. Company-id must be between 1 and %d.", MAX_COMPANIES);
		return true;
	}

	const Company *c = GetCompany(index);

	if (c->is_ai) {
		IConsoleError("Company is owned by an AI.");
		return true;
	}

	if (NetworkCompanyHasClients(index)) {
		IConsoleError("Cannot remove company: a client is connected to that company.");
		return false;
	}
	const NetworkClientInfo *ci = NetworkFindClientInfoFromClientID(CLIENT_ID_SERVER);
	if (ci->client_playas == index) {
		IConsoleError("Cannot remove company: the server is connected to that company.");
		return true;
	}

	/* It is safe to remove this company */
	DoCommandP(0, 2, index, CMD_COMPANY_CTRL);
	IConsolePrint(CC_DEFAULT, "Company deleted.");

	return true;
}

DEF_CONSOLE_CMD(ConNetworkClients)
{
	if (argc == 0) {
		IConsoleHelp("Get a list of connected clients including their ID, name, company-id, and IP. Usage: 'clients'");
		return true;
	}

	NetworkPrintClients();

	return true;
}

DEF_CONSOLE_CMD(ConNetworkConnect)
{
	char *ip;
	const char *port = NULL;
	const char *company = NULL;
	uint16 rport;

	if (argc == 0) {
		IConsoleHelp("Connect to a remote OTTD server and join the game. Usage: 'connect <ip>'");
		IConsoleHelp("IP can contain port and company: 'IP[[#Company]:Port]', eg: 'server.ottd.org#2:443'");
		IConsoleHelp("Company #255 is spectator all others are a certain company with Company 1 being #1");
		return true;
	}

	if (argc < 2) return false;
	if (_networking) NetworkDisconnect(); // we are in network-mode, first close it!

	ip = argv[1];
	/* Default settings: default port and new company */
	rport = NETWORK_DEFAULT_PORT;
	_network_playas = COMPANY_NEW_COMPANY;

	ParseConnectionString(&company, &port, ip);

	IConsolePrintF(CC_DEFAULT, "Connecting to %s...", ip);
	if (company != NULL) {
		_network_playas = (CompanyID)atoi(company);
		IConsolePrintF(CC_DEFAULT, "    company-no: %d", _network_playas);

		/* From a user pov 0 is a new company, internally it's different and all
		 * companies are offset by one to ease up on users (eg companies 1-8 not 0-7) */
		if (_network_playas != COMPANY_SPECTATOR) {
			_network_playas--;
			if (!IsValidCompanyID(_network_playas)) return false;
		}
	}
	if (port != NULL) {
		rport = atoi(port);
		IConsolePrintF(CC_DEFAULT, "    port: %s", port);
	}

	NetworkClientConnectGame(NetworkAddress(ip, rport));

	return true;
}

#endif /* ENABLE_NETWORK */

/*********************************
 *  script file console commands
 *********************************/

DEF_CONSOLE_CMD(ConExec)
{
	char cmdline[ICON_CMDLN_SIZE];
	char *cmdptr;

	if (argc == 0) {
		IConsoleHelp("Execute a local script file. Usage: 'exec <script> <?>'");
		return true;
	}

	if (argc < 2) return false;

	_script_file = FioFOpenFile(argv[1], "r", BASE_DIR);

	if (_script_file == NULL) {
		if (argc == 2 || atoi(argv[2]) != 0) IConsoleError("script file not found");
		return true;
	}

	_script_running = true;

	while (_script_running && fgets(cmdline, sizeof(cmdline), _script_file) != NULL) {
		/* Remove newline characters from the executing script */
		for (cmdptr = cmdline; *cmdptr != '\0'; cmdptr++) {
			if (*cmdptr == '\n' || *cmdptr == '\r') {
				*cmdptr = '\0';
				break;
			}
		}
		IConsoleCmdExec(cmdline);
	}

	if (ferror(_script_file))
		IConsoleError("Encountered errror while trying to read from script file");

	_script_running = false;
	FioFCloseFile(_script_file);
	return true;
}

DEF_CONSOLE_CMD(ConReturn)
{
	if (argc == 0) {
		IConsoleHelp("Stop executing a running script. Usage: 'return'");
		return true;
	}

	_script_running = false;
	return true;
}

/*****************************
 *  default console commands
 ******************************/
extern bool CloseConsoleLogIfActive();

DEF_CONSOLE_CMD(ConScript)
{
	extern FILE *_iconsole_output_file;

	if (argc == 0) {
		IConsoleHelp("Start or stop logging console output to a file. Usage: 'script <filename>'");
		IConsoleHelp("If filename is omitted, a running log is stopped if it is active");
		return true;
	}

	if (!CloseConsoleLogIfActive()) {
		if (argc < 2) return false;

		IConsolePrintF(CC_DEFAULT, "file output started to: %s", argv[1]);
		_iconsole_output_file = fopen(argv[1], "ab");
		if (_iconsole_output_file == NULL) IConsoleError("could not open file");
	}

	return true;
}


DEF_CONSOLE_CMD(ConEcho)
{
	if (argc == 0) {
		IConsoleHelp("Print back the first argument to the console. Usage: 'echo <arg>'");
		return true;
	}

	if (argc < 2) return false;
	IConsolePrint(CC_DEFAULT, argv[1]);
	return true;
}

DEF_CONSOLE_CMD(ConEchoC)
{
	if (argc == 0) {
		IConsoleHelp("Print back the first argument to the console in a given colour. Usage: 'echoc <colour> <arg2>'");
		return true;
	}

	if (argc < 3) return false;
	IConsolePrint((ConsoleColour)atoi(argv[1]), argv[2]);
	return true;
}

DEF_CONSOLE_CMD(ConNewGame)
{
	if (argc == 0) {
		IConsoleHelp("Start a new game. Usage: 'newgame [seed]'");
		IConsoleHelp("The server can force a new game using 'newgame'; any client joined will rejoin after the server is done generating the new game.");
		return true;
	}

	StartNewGameWithoutGUI((argc == 2) ? (uint)atoi(argv[1]) : GENERATE_NEW_SEED);
	return true;
}

extern void SwitchToMode(SwitchMode new_mode);

DEF_CONSOLE_CMD(ConRestart)
{
	if (argc == 0) {
		IConsoleHelp("Restart game. Usage: 'restart'");
		IConsoleHelp("Restarts a game. It tries to reproduce the exact same map as the game started with.");
		return true;
	}

	/* Don't copy the _newgame pointers to the real pointers, so call SwitchToMode directly */
	_settings_game.game_creation.map_x = MapLogX();
	_settings_game.game_creation.map_y = FindFirstBit(MapSizeY());
	SwitchToMode(SM_NEWGAME);
	return true;
}

DEF_CONSOLE_CMD(ConListAI)
{
	char buf[4096];
	char *p = &buf[0];
	p = AI::GetConsoleList(p, lastof(buf));

	p = &buf[0];
	/* Print output line by line */
	for (char *p2 = &buf[0]; *p2 != '\0'; p2++) {
		if (*p2 == '\n') {
			*p2 = '\0';
			IConsolePrintF(CC_DEFAULT, "%s", p);
			p = p2 + 1;
		}
	}

	return true;
}

DEF_CONSOLE_CMD(ConStartAI)
{
	if (argc == 0 || argc > 3) {
		IConsoleHelp("Start a new AI. Usage: 'start_ai [<AI>] [<settings>]'");
		IConsoleHelp("Start a new AI. If <AI> is given, it starts that specific AI (if found).");
		IConsoleHelp("If <settings> is given, it is parsed and the AI settings are set to that.");
		return true;
	}

	if (_game_mode != GM_NORMAL) {
		IConsoleWarning("AIs can only be managed in a game.");
		return true;
	}

	if (ActiveCompanyCount() == MAX_COMPANIES) {
		IConsoleWarning("Can't start a new AI (no more free slots).");
		return true;
	}
	if (_networking && !_network_server) {
		IConsoleWarning("Only the server can start a new AI.");
		return true;
	}
	if (_networking && !_settings_game.ai.ai_in_multiplayer) {
		IConsoleWarning("AIs are not allowed in multiplayer by configuration.");
		IConsoleWarning("Switch AI -> AI in multiplayer to True.");
		return true;
	}
	if (!AI::CanStartNew()) {
		IConsoleWarning("Can't start a new AI.");
		return true;
	}

	int n = 0;
	Company *c;
	/* Find the next free slot */
	FOR_ALL_COMPANIES(c) {
		if (c->index != n) break;
		n++;
	}

	AIConfig *config = AIConfig::GetConfig((CompanyID)n);
	if (argc >= 2) {
		config->ChangeAI(argv[1]);
		if (!config->HasAI()) {
			IConsoleWarning("Failed to load the specified AI");
			return true;
		}
		if (argc == 3) {
			config->StringToSettings(argv[2]);
		}
	}

	/* Start a new AI company */
	DoCommandP(0, 1, 0, CMD_COMPANY_CTRL);

	return true;
}

DEF_CONSOLE_CMD(ConReloadAI)
{
	if (argc != 2) {
		IConsoleHelp("Reload an AI. Usage: 'reload_ai <company-id>'");
		IConsoleHelp("Reload the AI with the given company id. For company-id's, see the list of companies from the dropdown menu. Company 1 is 1, etc.");
		return true;
	}

	if (_game_mode != GM_NORMAL) {
		IConsoleWarning("AIs can only be managed in a game.");
		return true;
	}

	if (_networking && !_network_server) {
		IConsoleWarning("Only the server can reload an AI.");
		return true;
	}

	CompanyID company_id = (CompanyID)(atoi(argv[1]) - 1);
	if (!IsValidCompanyID(company_id)) {
		IConsolePrintF(CC_DEFAULT, "Unknown company. Company range is between 1 and %d.", MAX_COMPANIES);
		return true;
	}

	if (IsHumanCompany(company_id)) {
		IConsoleWarning("Company is not controlled by an AI.");
		return true;
	}

	/* First kill the company of the AI, then start a new one. This should start the current AI again */
	DoCommandP(0, 2, company_id, CMD_COMPANY_CTRL);
	DoCommandP(0, 1, 0, CMD_COMPANY_CTRL);
	IConsolePrint(CC_DEFAULT, "AI reloaded.");

	return true;
}

DEF_CONSOLE_CMD(ConStopAI)
{
	if (argc != 2) {
		IConsoleHelp("Stop an AI. Usage: 'stop_ai <company-id>'");
		IConsoleHelp("Stop the AI with the given company id. For company-id's, see the list of companies from the dropdown menu. Company 1 is 1, etc.");
		return true;
	}

	if (_game_mode != GM_NORMAL) {
		IConsoleWarning("AIs can only be managed in a game.");
		return true;
	}

	if (_networking && !_network_server) {
		IConsoleWarning("Only the server can stop an AI.");
		return true;
	}

	CompanyID company_id = (CompanyID)(atoi(argv[1]) - 1);
	if (!IsValidCompanyID(company_id)) {
		IConsolePrintF(CC_DEFAULT, "Unknown company. Company range is between 1 and %d.", MAX_COMPANIES);
		return true;
	}

	if (IsHumanCompany(company_id)) {
		IConsoleWarning("Company is not controlled by an AI.");
		return true;
	}

	/* Now kill the company of the AI. */
	DoCommandP(0, 2, company_id, CMD_COMPANY_CTRL);
	IConsolePrint(CC_DEFAULT, "AI stopped, company deleted.");

	return true;
}

DEF_CONSOLE_CMD(ConRescanAI)
{
	if (argc == 0) {
		IConsoleHelp("Rescan the AI dir for scripts. Usage: 'rescan_ai'");
		return true;
	}

	if (_networking && !_network_server) {
		IConsoleWarning("Only the server can rescan the AI dir for scripts.");
		return true;
	}

	AI::Rescan();

	return true;
}

DEF_CONSOLE_CMD(ConGetSeed)
{
	if (argc == 0) {
		IConsoleHelp("Returns the seed used to create this game. Usage: 'getseed'");
		IConsoleHelp("The seed can be used to reproduce the exact same map as the game started with.");
		return true;
	}

	IConsolePrintF(CC_DEFAULT, "Generation Seed: %u", _settings_game.game_creation.generation_seed);
	return true;
}

DEF_CONSOLE_CMD(ConGetDate)
{
	if (argc == 0) {
		IConsoleHelp("Returns the current date (day-month-year) of the game. Usage: 'getdate'");
		return true;
	}

	YearMonthDay ymd;
	ConvertDateToYMD(_date, &ymd);
	IConsolePrintF(CC_DEFAULT, "Date: %d-%d-%d", ymd.day, ymd.month + 1, ymd.year);
	return true;
}


DEF_CONSOLE_CMD(ConAlias)
{
	IConsoleAlias *alias;

	if (argc == 0) {
		IConsoleHelp("Add a new alias, or redefine the behaviour of an existing alias . Usage: 'alias <name> <command>'");
		return true;
	}

	if (argc < 3) return false;

	alias = IConsoleAliasGet(argv[1]);
	if (alias == NULL) {
		IConsoleAliasRegister(argv[1], argv[2]);
	} else {
		free(alias->cmdline);
		alias->cmdline = strdup(argv[2]);
	}
	return true;
}

DEF_CONSOLE_CMD(ConScreenShot)
{
	if (argc == 0) {
		IConsoleHelp("Create a screenshot of the game. Usage: 'screenshot [big | no_con]'");
		IConsoleHelp("'big' makes a screenshot of the whole map, 'no_con' hides the console to create the screenshot");
		return true;
	}

	if (argc > 3) return false;

	SetScreenshotType(SC_VIEWPORT);
	if (argc > 1) {
		if (strcmp(argv[1], "big") == 0 || (argc == 3 && strcmp(argv[2], "big") == 0))
			SetScreenshotType(SC_WORLD);

		if (strcmp(argv[1], "no_con") == 0 || (argc == 3 && strcmp(argv[2], "no_con") == 0))
			IConsoleClose();
	}

	return true;
}

DEF_CONSOLE_CMD(ConInfoVar)
{
	static const char *_icon_vartypes[] = {"boolean", "byte", "uint16", "uint32", "int16", "int32", "string"};
	const IConsoleVar *var;

	if (argc == 0) {
		IConsoleHelp("Print out debugging information about a variable. Usage: 'info_var <var>'");
		return true;
	}

	if (argc < 2) return false;

	var = IConsoleVarGet(argv[1]);
	if (var == NULL) {
		IConsoleError("the given variable was not found");
		return true;
	}

	IConsolePrintF(CC_DEFAULT, "variable name: %s", var->name);
	IConsolePrintF(CC_DEFAULT, "variable type: %s", _icon_vartypes[var->type]);
	IConsolePrintF(CC_DEFAULT, "variable addr: 0x%X", var->addr);

	if (var->hook.access) IConsoleWarning("variable is access hooked");
	if (var->hook.pre) IConsoleWarning("variable is pre hooked");
	if (var->hook.post) IConsoleWarning("variable is post hooked");
	return true;
}


DEF_CONSOLE_CMD(ConInfoCmd)
{
	const IConsoleCmd *cmd;

	if (argc == 0) {
		IConsoleHelp("Print out debugging information about a command. Usage: 'info_cmd <cmd>'");
		return true;
	}

	if (argc < 2) return false;

	cmd = IConsoleCmdGet(argv[1]);
	if (cmd == NULL) {
		IConsoleError("the given command was not found");
		return true;
	}

	IConsolePrintF(CC_DEFAULT, "command name: %s", cmd->name);
	IConsolePrintF(CC_DEFAULT, "command proc: 0x%X", cmd->proc);

	if (cmd->hook.access) IConsoleWarning("command is access hooked");
	if (cmd->hook.pre) IConsoleWarning("command is pre hooked");
	if (cmd->hook.post) IConsoleWarning("command is post hooked");

	return true;
}

DEF_CONSOLE_CMD(ConDebugLevel)
{
	if (argc == 0) {
		IConsoleHelp("Get/set the default debugging level for the game. Usage: 'debug_level [<level>]'");
		IConsoleHelp("Level can be any combination of names, levels. Eg 'net=5 ms=4'. Remember to enclose it in \"'s");
		return true;
	}

	if (argc > 2) return false;

	if (argc == 1) {
		IConsolePrintF(CC_DEFAULT, "Current debug-level: '%s'", GetDebugString());
	} else {
		SetDebugString(argv[1]);
	}

	return true;
}

DEF_CONSOLE_CMD(ConExit)
{
	if (argc == 0) {
		IConsoleHelp("Exit the game. Usage: 'exit'");
		return true;
	}

	if (_game_mode == GM_NORMAL && _settings_client.gui.autosave_on_exit) DoExitSave();

	_exit_game = true;
	return true;
}

DEF_CONSOLE_CMD(ConPart)
{
	if (argc == 0) {
		IConsoleHelp("Leave the currently joined/running game (only ingame). Usage: 'part'");
		return true;
	}

	if (_game_mode != GM_NORMAL) return false;

	_switch_mode = SM_MENU;
	return true;
}

DEF_CONSOLE_CMD(ConHelp)
{
	if (argc == 2) {
		const IConsoleCmd *cmd;
		const IConsoleVar *var;
		const IConsoleAlias *alias;

		cmd = IConsoleCmdGet(argv[1]);
		if (cmd != NULL) {
			cmd->proc(0, NULL);
			return true;
		}

		alias = IConsoleAliasGet(argv[1]);
		if (alias != NULL) {
			cmd = IConsoleCmdGet(alias->cmdline);
			if (cmd != NULL) {
				cmd->proc(0, NULL);
				return true;
			}
			IConsolePrintF(CC_ERROR, "ERROR: alias is of special type, please see its execution-line: '%s'", alias->cmdline);
			return true;
		}

		var = IConsoleVarGet(argv[1]);
		if (var != NULL && var->help != NULL) {
			IConsoleHelp(var->help);
			return true;
		}

		IConsoleError("command or variable not found");
		return true;
	}

	IConsolePrint(CC_WARNING, " ---- OpenTTD Console Help ---- ");
	IConsolePrint(CC_DEFAULT, " - variables: [command to list all variables: list_vars]");
	IConsolePrint(CC_DEFAULT, " set value with '<var> = <value>', use '++/--' to in-or decrement");
	IConsolePrint(CC_DEFAULT, " or omit '=' and just '<var> <value>'. get value with typing '<var>'");
	IConsolePrint(CC_DEFAULT, " - commands: [command to list all commands: list_cmds]");
	IConsolePrint(CC_DEFAULT, " call commands with '<command> <arg2> <arg3>...'");
	IConsolePrint(CC_DEFAULT, " - to assign strings, or use them as arguments, enclose it within quotes");
	IConsolePrint(CC_DEFAULT, " like this: '<command> \"string argument with spaces\"'");
	IConsolePrint(CC_DEFAULT, " - use 'help <command> | <variable>' to get specific information");
	IConsolePrint(CC_DEFAULT, " - scroll console output with shift + (up | down) | (pageup | pagedown))");
	IConsolePrint(CC_DEFAULT, " - scroll console input history with the up | down arrows");
	IConsolePrint(CC_DEFAULT, "");
	return true;
}

DEF_CONSOLE_CMD(ConListCommands)
{
	const IConsoleCmd *cmd;
	size_t l = 0;

	if (argc == 0) {
		IConsoleHelp("List all registered commands. Usage: 'list_cmds [<pre-filter>]'");
		return true;
	}

	if (argv[1] != NULL) l = strlen(argv[1]);

	for (cmd = _iconsole_cmds; cmd != NULL; cmd = cmd->next) {
		if (argv[1] == NULL || strncmp(cmd->name, argv[1], l) == 0) {
				IConsolePrintF(CC_DEFAULT, "%s", cmd->name);
		}
	}

	return true;
}

DEF_CONSOLE_CMD(ConListVariables)
{
	const IConsoleVar *var;
	size_t l = 0;

	if (argc == 0) {
		IConsoleHelp("List all registered variables. Usage: 'list_vars [<pre-filter>]'");
		return true;
	}

	if (argv[1] != NULL) l = strlen(argv[1]);

	for (var = _iconsole_vars; var != NULL; var = var->next) {
		if (argv[1] == NULL || strncmp(var->name, argv[1], l) == 0)
			IConsolePrintF(CC_DEFAULT, "%s", var->name);
	}

	return true;
}

DEF_CONSOLE_CMD(ConListAliases)
{
	const IConsoleAlias *alias;
	size_t l = 0;

	if (argc == 0) {
		IConsoleHelp("List all registered aliases. Usage: 'list_aliases [<pre-filter>]'");
		return true;
	}

	if (argv[1] != NULL) l = strlen(argv[1]);

	for (alias = _iconsole_aliases; alias != NULL; alias = alias->next) {
		if (argv[1] == NULL || strncmp(alias->name, argv[1], l) == 0)
			IConsolePrintF(CC_DEFAULT, "%s => %s", alias->name, alias->cmdline);
	}

	return true;
}

#ifdef ENABLE_NETWORK

DEF_CONSOLE_CMD(ConSay)
{
	if (argc == 0) {
		IConsoleHelp("Chat to your fellow players in a multiplayer game. Usage: 'say \"<msg>\"'");
		return true;
	}

	if (argc != 2) return false;

	if (!_network_server) {
		NetworkClientSendChat(NETWORK_ACTION_CHAT, DESTTYPE_BROADCAST, 0 /* param does not matter */, argv[1]);
	} else {
		NetworkServerSendChat(NETWORK_ACTION_CHAT, DESTTYPE_BROADCAST, 0, argv[1], CLIENT_ID_SERVER);
	}

	return true;
}

DEF_CONSOLE_CMD(ConCompanies)
{
	Company *c;

	if (argc == 0) {
		IConsoleHelp("List the in-game details of all clients connected to the server. Usage 'companies'");
		return true;
	}
	NetworkCompanyStats company_stats[MAX_COMPANIES];
	NetworkPopulateCompanyStats(company_stats);

	FOR_ALL_COMPANIES(c) {
		/* Grab the company name */
		char company_name[NETWORK_COMPANY_NAME_LENGTH];
		SetDParam(0, c->index);
		GetString(company_name, STR_COMPANY_NAME, lastof(company_name));

		char buffer[512];
		const NetworkCompanyStats *stats = &company_stats[c->index];

		GetString(buffer, STR_00D1_DARK_BLUE + _company_colours[c->index], lastof(buffer));
		IConsolePrintF(CC_INFO, "#:%d(%s) Company Name: '%s'  Year Founded: %d  Money: %" OTTD_PRINTF64 "d  Loan: %" OTTD_PRINTF64 "d  Value: %" OTTD_PRINTF64 "d  (T:%d, R:%d, P:%d, S:%d) %sprotected",
			c->index + 1, buffer, company_name, c->inaugurated_year, (int64)c->money, (int64)c->current_loan, (int64)CalculateCompanyValue(c),
			/* trains      */ stats->num_vehicle[0],
			/* lorry + bus */ stats->num_vehicle[1] + stats->num_vehicle[2],
			/* planes      */ stats->num_vehicle[3],
			/* ships       */ stats->num_vehicle[4],
			/* protected   */ StrEmpty(_network_company_states[c->index].password) ? "un" : "");
	}

	return true;
}

DEF_CONSOLE_CMD(ConSayCompany)
{
	if (argc == 0) {
		IConsoleHelp("Chat to a certain company in a multiplayer game. Usage: 'say_company <company-no> \"<msg>\"'");
		IConsoleHelp("CompanyNo is the company that plays as company <companyno>, 1 through max_companies");
		return true;
	}

	if (argc != 3) return false;

	CompanyID company_id = (CompanyID)(atoi(argv[1]) - 1);
	if (!IsValidCompanyID(company_id)) {
		IConsolePrintF(CC_DEFAULT, "Unknown company. Company range is between 1 and %d.", MAX_COMPANIES);
		return true;
	}

	if (!_network_server) {
		NetworkClientSendChat(NETWORK_ACTION_CHAT_COMPANY, DESTTYPE_TEAM, company_id, argv[2]);
	} else {
		NetworkServerSendChat(NETWORK_ACTION_CHAT_COMPANY, DESTTYPE_TEAM, company_id, argv[2], CLIENT_ID_SERVER);
	}

	return true;
}

DEF_CONSOLE_CMD(ConSayClient)
{
	if (argc == 0) {
		IConsoleHelp("Chat to a certain client in a multiplayer game. Usage: 'say_client <client-no> \"<msg>\"'");
		IConsoleHelp("For client-id's, see the command 'clients'");
		return true;
	}

	if (argc != 3) return false;

	if (!_network_server) {
		NetworkClientSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, atoi(argv[1]), argv[2]);
	} else {
		NetworkServerSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, atoi(argv[1]), argv[2], CLIENT_ID_SERVER);
	}

	return true;
}

extern void HashCurrentCompanyPassword(const char *password);

/* Also use from within company_gui to change the password graphically */
bool NetworkChangeCompanyPassword(byte argc, char *argv[])
{
	if (argc == 0) {
		IConsoleHelp("Change the password of your company. Usage: 'company_pw \"<password>\"'");
		IConsoleHelp("Use \"*\" to disable the password.");
		return true;
	}

	if (!IsValidCompanyID(_local_company)) {
		IConsoleError("You have to own a company to make use of this command.");
		return false;
	}

	if (argc != 1) return false;

	if (strcmp(argv[0], "*") == 0) argv[0][0] = '\0';

	if (!_network_server) {
		NetworkClientSetPassword(argv[0]);
	} else {
		HashCurrentCompanyPassword(argv[0]);
	}

	IConsolePrintF(CC_WARNING, "'company_pw' changed to:  %s", argv[0]);

	return true;
}

#include "network/network_content.h"

/** Resolve a string to a content type. */
static ContentType StringToContentType(const char *str)
{
	static const char *inv_lookup[] = { "", "base", "newgrf", "ai", "ailib", "scenario", "heightmap" };
	for (uint i = 1 /* there is no type 0 */; i < lengthof(inv_lookup); i++) {
		if (strcasecmp(str, inv_lookup[i]) == 0) return (ContentType)i;
	}
	return CONTENT_TYPE_END;
}

/** Asynchronous callback */
struct ConsoleContentCallback : public ContentCallback {
	void OnConnect(bool success)
	{
		IConsolePrintF(CC_DEFAULT, "Content server connection %s", success ? "established" : "failed");
	}

	void OnDisconnect()
	{
		IConsolePrintF(CC_DEFAULT, "Content server connection closed");
	}

	void OnDownloadComplete(ContentID cid)
	{
		IConsolePrintF(CC_DEFAULT, "Completed download of %d", cid);
	}
};

DEF_CONSOLE_CMD(ConContent)
{
	static ContentCallback *cb = NULL;
	if (cb == NULL) {
		cb = new ConsoleContentCallback();
		_network_content_client.AddCallback(cb);
	}

	if (argc <= 1) {
		IConsoleHelp("Query, select and download content. Usage: 'content update|upgrade|select [all|id]|unselect [all|id]|state|download'");
		IConsoleHelp("  update: get a new list of downloadable content; must be run first");
		IConsoleHelp("  upgrade: select all items that are upgrades");
		IConsoleHelp("  select: select a specific item given by its id or 'all' to select all");
		IConsoleHelp("  unselect: unselect a specific item given by its id or 'all' to unselect all");
		IConsoleHelp("  state: show the download/select state of all downloadable content");
		IConsoleHelp("  download: download all content you've selected");
		return true;
	}

	if (strcasecmp(argv[1], "update") == 0) {
		_network_content_client.RequestContentList((argc > 2) ? StringToContentType(argv[2]) : CONTENT_TYPE_END);
		return true;
	}

	if (strcasecmp(argv[1], "upgrade") == 0) {
		_network_content_client.SelectUpgrade();
		return true;
	}

	if (strcasecmp(argv[1], "select") == 0) {
		if (argc <= 2) {
			IConsoleError("You must enter the id.");
			return false;
		}
		if (strcasecmp(argv[2], "all") == 0) {
			_network_content_client.SelectAll();
		} else {
			_network_content_client.Select((ContentID)atoi(argv[2]));
		}
		return true;
	}

	if (strcasecmp(argv[1], "unselect") == 0) {
		if (argc <= 2) {
			IConsoleError("You must enter the id.");
			return false;
		}
		if (strcasecmp(argv[2], "all") == 0) {
			_network_content_client.UnselectAll();
		} else {
			_network_content_client.Unselect((ContentID)atoi(argv[2]));
		}
		return true;
	}

	if (strcasecmp(argv[1], "state") == 0) {
		IConsolePrintF(CC_WHITE, "id, type, state, name");
		for (ConstContentIterator iter = _network_content_client.Begin(); iter != _network_content_client.End(); iter++) {
			static const char *types[] = { "Base graphics", "NewGRF", "AI", "AI library", "Scenario", "Heightmap" };
			static const char *states[] = { "Not selected", "Selected" , "Dep Selected", "Installed", "Unknown" };
			static ConsoleColour state_to_colour[] = { CC_COMMAND, CC_INFO, CC_INFO, CC_WHITE, CC_ERROR };

			const ContentInfo *ci = *iter;
			IConsolePrintF(state_to_colour[ci->state], "%d, %s, %s, %s", ci->id, types[ci->type - 1], states[ci->state], ci->name);
		}
		return true;
	}

	if (strcasecmp(argv[1], "download") == 0) {
		uint files;
		uint bytes;
		_network_content_client.DownloadSelectedContent(files, bytes);
		IConsolePrintF(CC_DEFAULT, "Downloading %d file(s) (%d bytes)", files, bytes);
		return true;
	}

	return false;
}

#endif /* ENABLE_NETWORK */

DEF_CONSOLE_CMD(ConSetting)
{
	if (argc == 0) {
		IConsoleHelp("Change setting for all clients. Usage: 'setting <name> [<value>]'");
		IConsoleHelp("Omitting <value> will print out the current value of the setting.");
		return true;
	}

	if (argc == 1 || argc > 3) return false;

	if (argc == 2) {
		IConsoleGetSetting(argv[1]);
	} else {
		IConsoleSetSetting(argv[1], argv[2]);
	}

	return true;
}

DEF_CONSOLE_CMD(ConListSettings)
{
	if (argc == 0) {
		IConsoleHelp("List settings. Usage: 'list_settings [<pre-filter>]'");
		return true;
	}

	if (argc > 2) return false;

	IConsoleListSettings((argc == 2) ? argv[1] : NULL);
	return true;
}

DEF_CONSOLE_CMD(ConListDumpVariables)
{
	const IConsoleVar *var;
	size_t l = 0;

	if (argc == 0) {
		IConsoleHelp("List all variables with their value. Usage: 'dump_vars [<pre-filter>]'");
		return true;
	}

	if (argv[1] != NULL) l = strlen(argv[1]);

	for (var = _iconsole_vars; var != NULL; var = var->next) {
		if (argv[1] == NULL || strncmp(var->name, argv[1], l) == 0)
			IConsoleVarPrintGetValue(var);
	}

	return true;
}

DEF_CONSOLE_CMD(ConGamelogPrint)
{
	GamelogPrintConsole();
	return true;
}

#ifdef _DEBUG
/*******************************************
 *  debug commands and variables
 ********************************************/

static void IConsoleDebugLibRegister()
{
	/* debugging variables and functions */
	extern bool _stdlib_con_developer; // XXX extern in .cpp

	IConsoleVarRegister("con_developer",    &_stdlib_con_developer, ICONSOLE_VAR_BOOLEAN, "Enable/disable console debugging information (internal)");
	IConsoleCmdRegister("resettile",        ConResetTile);
	IConsoleCmdRegister("stopall",          ConStopAllVehicles);
	IConsoleAliasRegister("dbg_echo",       "echo %A; echo %B");
	IConsoleAliasRegister("dbg_echo2",      "echo %!");
}
#endif

/*******************************************
 * console command and variable registration
 ********************************************/

void IConsoleStdLibRegister()
{
	/* stdlib */
	extern byte _stdlib_developer; // XXX extern in .cpp

	/* default variables and functions */
	IConsoleCmdRegister("debug_level",  ConDebugLevel);
	IConsoleCmdRegister("dump_vars",    ConListDumpVariables);
	IConsoleCmdRegister("echo",         ConEcho);
	IConsoleCmdRegister("echoc",        ConEchoC);
	IConsoleCmdRegister("exec",         ConExec);
	IConsoleCmdRegister("exit",         ConExit);
	IConsoleCmdRegister("part",         ConPart);
	IConsoleCmdRegister("help",         ConHelp);
	IConsoleCmdRegister("info_cmd",     ConInfoCmd);
	IConsoleCmdRegister("info_var",     ConInfoVar);
	IConsoleCmdRegister("list_ai",      ConListAI);
	IConsoleCmdRegister("list_cmds",    ConListCommands);
	IConsoleCmdRegister("list_vars",    ConListVariables);
	IConsoleCmdRegister("list_aliases", ConListAliases);
	IConsoleCmdRegister("newgame",      ConNewGame);
	IConsoleCmdRegister("restart",      ConRestart);
	IConsoleCmdRegister("getseed",      ConGetSeed);
	IConsoleCmdRegister("getdate",      ConGetDate);
	IConsoleCmdRegister("quit",         ConExit);
	IConsoleCmdRegister("reload_ai",    ConReloadAI);
	IConsoleCmdRegister("rescan_ai",    ConRescanAI);
	IConsoleCmdRegister("resetengines", ConResetEngines);
	IConsoleCmdRegister("return",       ConReturn);
	IConsoleCmdRegister("screenshot",   ConScreenShot);
	IConsoleCmdRegister("script",       ConScript);
	IConsoleCmdRegister("scrollto",     ConScrollToTile);
	IConsoleCmdRegister("alias",        ConAlias);
	IConsoleCmdRegister("load",         ConLoad);
	IConsoleCmdRegister("rm",           ConRemove);
	IConsoleCmdRegister("save",         ConSave);
	IConsoleCmdRegister("saveconfig",   ConSaveConfig);
	IConsoleCmdRegister("start_ai",     ConStartAI);
	IConsoleCmdRegister("stop_ai",      ConStopAI);
	IConsoleCmdRegister("ls",           ConListFiles);
	IConsoleCmdRegister("cd",           ConChangeDirectory);
	IConsoleCmdRegister("pwd",          ConPrintWorkingDirectory);
	IConsoleCmdRegister("clear",        ConClearBuffer);
	IConsoleCmdRegister("setting",      ConSetting);
	IConsoleCmdRegister("list_settings",ConListSettings);
	IConsoleCmdRegister("gamelog",      ConGamelogPrint);

	IConsoleAliasRegister("dir",          "ls");
	IConsoleAliasRegister("del",          "rm %+");
	IConsoleAliasRegister("newmap",       "newgame");
	IConsoleAliasRegister("new_map",      "newgame");
	IConsoleAliasRegister("new_game",     "newgame");
	IConsoleAliasRegister("patch",        "setting %+");
	IConsoleAliasRegister("set",          "setting %+");
	IConsoleAliasRegister("list_patches", "list_settings %+");



	IConsoleVarRegister("developer", &_stdlib_developer, ICONSOLE_VAR_BYTE, "Redirect debugging output from the console/command line to the ingame console (value 2). Default value: 1");

	/* networking variables and functions */
#ifdef ENABLE_NETWORK
	/* Network hooks; only active in network */
	IConsoleCmdHookAdd ("resetengines", ICONSOLE_HOOK_ACCESS, ConHookNoNetwork);
	IConsoleCmdRegister("content",         ConContent);

	/*** Networking commands ***/
	IConsoleCmdRegister("say",             ConSay);
	IConsoleCmdHookAdd("say",              ICONSOLE_HOOK_ACCESS, ConHookNeedNetwork);
	IConsoleCmdRegister("companies",       ConCompanies);
	IConsoleCmdHookAdd("companies",        ICONSOLE_HOOK_ACCESS, ConHookServerOnly);
	IConsoleAliasRegister("players",       "companies");
	IConsoleCmdRegister("say_company",     ConSayCompany);
	IConsoleCmdHookAdd("say_company",      ICONSOLE_HOOK_ACCESS, ConHookNeedNetwork);
	IConsoleAliasRegister("say_player",    "say_company %+");
	IConsoleCmdRegister("say_client",      ConSayClient);
	IConsoleCmdHookAdd("say_client",       ICONSOLE_HOOK_ACCESS, ConHookNeedNetwork);

	IConsoleCmdRegister("connect",         ConNetworkConnect);
	IConsoleCmdHookAdd("connect",          ICONSOLE_HOOK_ACCESS, ConHookClientOnly);
	IConsoleCmdRegister("clients",         ConNetworkClients);
	IConsoleCmdHookAdd("clients",          ICONSOLE_HOOK_ACCESS, ConHookNeedNetwork);
	IConsoleCmdRegister("status",          ConStatus);
	IConsoleCmdHookAdd("status",           ICONSOLE_HOOK_ACCESS, ConHookServerOnly);
	IConsoleCmdRegister("server_info",     ConServerInfo);
	IConsoleCmdHookAdd("server_info",      ICONSOLE_HOOK_ACCESS, ConHookServerOnly);
	IConsoleAliasRegister("info",          "server_info");
	IConsoleCmdRegister("rcon",            ConRcon);
	IConsoleCmdHookAdd("rcon",             ICONSOLE_HOOK_ACCESS, ConHookNeedNetwork);

	IConsoleCmdRegister("join",            ConJoinCompany);
	IConsoleCmdHookAdd("join",             ICONSOLE_HOOK_ACCESS, ConHookNeedNetwork);
	IConsoleAliasRegister("spectate",      "join 255");
	IConsoleCmdRegister("move",            ConMoveClient);
	IConsoleCmdHookAdd("move",             ICONSOLE_HOOK_ACCESS, ConHookServerOnly);
	IConsoleCmdRegister("reset_company",   ConResetCompany);
	IConsoleCmdHookAdd("reset_company",    ICONSOLE_HOOK_ACCESS, ConHookServerOnly);
	IConsoleAliasRegister("clean_company", "reset_company %A");
	IConsoleCmdRegister("client_name",     ConClientNickChange);
	IConsoleCmdHookAdd("client_name",      ICONSOLE_HOOK_ACCESS, ConHookServerOnly);
	IConsoleCmdRegister("kick",            ConKick);
	IConsoleCmdHookAdd("kick",             ICONSOLE_HOOK_ACCESS, ConHookServerOnly);
	IConsoleCmdRegister("ban",             ConBan);
	IConsoleCmdHookAdd("ban",              ICONSOLE_HOOK_ACCESS, ConHookServerOnly);
	IConsoleCmdRegister("unban",           ConUnBan);
	IConsoleCmdHookAdd("unban",            ICONSOLE_HOOK_ACCESS, ConHookServerOnly);
	IConsoleCmdRegister("banlist",         ConBanList);
	IConsoleCmdHookAdd("banlist",          ICONSOLE_HOOK_ACCESS, ConHookServerOnly);

	IConsoleCmdRegister("pause",           ConPauseGame);
	IConsoleCmdHookAdd("pause",            ICONSOLE_HOOK_ACCESS, ConHookServerOnly);
	IConsoleCmdRegister("unpause",         ConUnPauseGame);
	IConsoleCmdHookAdd("unpause",          ICONSOLE_HOOK_ACCESS, ConHookServerOnly);

	/*** Networking variables ***/
	IConsoleVarStringRegister("company_pw",      NULL, 0, "Set a password for your company, so no one without the correct password can join. Use '*' to clear the password");
	IConsoleVarHookAdd("company_pw",             ICONSOLE_HOOK_ACCESS, ConHookNeedNetwork);
	IConsoleVarProcAdd("company_pw",             NetworkChangeCompanyPassword);
	IConsoleAliasRegister("company_password",    "company_pw %+");

	IConsoleAliasRegister("net_frame_freq",        "setting frame_freq %+");
	IConsoleAliasRegister("net_sync_freq",         "setting sync_freq %+");
	IConsoleAliasRegister("server_pw",             "setting server_password %+");
	IConsoleAliasRegister("server_password",       "setting server_password %+");
	IConsoleAliasRegister("rcon_pw",               "setting rcon_password %+");
	IConsoleAliasRegister("rcon_password",         "setting rcon_password %+");
	IConsoleAliasRegister("name",                  "setting client_name %+");
	IConsoleAliasRegister("server_name",           "setting server_name %+");
	IConsoleAliasRegister("server_port",           "setting server_port %+");
	IConsoleAliasRegister("server_ip",             "setting server_bind_ip %+");
	IConsoleAliasRegister("server_bind_ip",        "setting server_bind_ip %+");
	IConsoleAliasRegister("server_ip_bind",        "setting server_bind_ip %+");
	IConsoleAliasRegister("server_bind",           "setting server_bind_ip %+");
	IConsoleAliasRegister("server_advertise",      "setting server_advertise %+");
	IConsoleAliasRegister("max_clients",           "setting max_clients %+");
	IConsoleAliasRegister("max_companies",         "setting max_companies %+");
	IConsoleAliasRegister("max_spectators",        "setting max_spectators %+");
	IConsoleAliasRegister("max_join_time",         "setting max_join_time %+");
	IConsoleAliasRegister("pause_on_join",         "setting pause_on_join %+");
	IConsoleAliasRegister("autoclean_companies",   "setting autoclean_companies %+");
	IConsoleAliasRegister("autoclean_protected",   "setting autoclean_protected %+");
	IConsoleAliasRegister("autoclean_unprotected", "setting autoclean_unprotected %+");
	IConsoleAliasRegister("restart_game_year",     "setting restart_game_year %+");
	IConsoleAliasRegister("min_players",           "setting min_active_clients %+");
	IConsoleAliasRegister("reload_cfg",            "setting reload_cfg %+");
#endif /* ENABLE_NETWORK */

	/* debugging stuff */
#ifdef _DEBUG
	IConsoleDebugLibRegister();
#endif
}
