/* $Id$ */

/** @file console_cmds.cpp Implementation of the console hooks. */

#include "stdafx.h"
#include "openttd.h"
#include "console_internal.h"
#include "debug.h"
#include "engine_func.h"
#include "landscape.h"
#include "saveload.h"
#include "variables.h"
#include "network/network.h"
#include "network/network_func.h"
#include "command_func.h"
#include "settings_func.h"
#include "fios.h"
#include "fileio.h"
#include "screenshot.h"
#include "genworld.h"
#include "strings_func.h"
#include "viewport_func.h"
#include "window_func.h"
#include "functions.h"
#include "map_func.h"
#include "date_func.h"
#include "vehicle_func.h"
#include "string_func.h"
#include "player_func.h"
#include "player_base.h"
#include "settings_type.h"

#ifdef ENABLE_NETWORK
	#include "table/strings.h"
#endif /* ENABLE_NETWORK */

// ** scriptfile handling ** //
static FILE *_script_file;
static bool _script_running;

// ** console command / variable defines ** //
#define DEF_CONSOLE_CMD(function) static bool function(byte argc, char *argv[])
#define DEF_CONSOLE_HOOK(function) static bool function()


/* **************************** */
/* variable and command hooks   */
/* **************************** */

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

static const FiosItem* GetFiosItem(const char* file)
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

				ttd_strlcpy(_file_to_saveload.name, FiosBrowseTo(item), sizeof(_file_to_saveload.name));
				ttd_strlcpy(_file_to_saveload.title, item->title, sizeof(_file_to_saveload.title));
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

	// XXX - Workaround for broken file handling
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


// ********************************* //
// * Network Core Console Commands * //
// ********************************* //
#ifdef ENABLE_NETWORK

DEF_CONSOLE_CMD(ConBan)
{
	NetworkClientInfo *ci;
	const char *banip = NULL;
	uint32 index;

	if (argc == 0) {
		IConsoleHelp("Ban a player from a network game. Usage: 'ban <ip | client-id>'");
		IConsoleHelp("For client-id's, see the command 'clients'");
		IConsoleHelp("If the client is no longer online, you can still ban his/her IP");
		return true;
	}

	if (argc != 2) return false;

	if (strchr(argv[1], '.') == NULL) { // banning with ID
		index = atoi(argv[1]);
		ci = NetworkFindClientInfoFromIndex(index);
	} else { // banning IP
		ci = NetworkFindClientInfoFromIP(argv[1]);
		if (ci == NULL) {
			banip = argv[1];
			index = (uint32)-1;
		} else {
			index = ci->client_index;
		}
	}

	if (index == NETWORK_SERVER_INDEX) {
		IConsoleError("Silly boy, you can not ban yourself!");
		return true;
	}

	if (index == 0 || (ci == NULL && index != (uint32)-1)) {
		IConsoleError("Invalid client");
		return true;
	}

	if (ci != NULL) {
		banip = GetPlayerIP(ci);
		NetworkServerSendError(index, NETWORK_ERROR_KICKED);
		IConsolePrint(CC_DEFAULT, "Client banned");
	} else {
		IConsolePrint(CC_DEFAULT, "Client not online, banned IP");
	}

	/* Add user to ban-list */
	for (index = 0; index < lengthof(_network_ban_list); index++) {
		if (_network_ban_list[index] == NULL) {
			_network_ban_list[index] = strdup(banip);
			break;
		}
	}

	return true;
}

DEF_CONSOLE_CMD(ConUnBan)
{
	uint i, index;

	if (argc == 0) {
		IConsoleHelp("Unban a player from a network game. Usage: 'unban <ip | client-id>'");
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
		DoCommandP(0, 1, 0, NULL, CMD_PAUSE);
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
		DoCommandP(0, 0, 0, NULL, CMD_PAUSE);
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
		IConsoleHelp("List current and maximum client/player limits. Usage 'server_info'");
		IConsoleHelp("You can change these values by setting the variables 'max_clients', 'max_companies' and 'max_spectators'");
		return true;
	}

	IConsolePrintF(CC_DEFAULT, "Current/maximum clients:    %2d/%2d", _network_game_info.clients_on, _settings_client.network.max_clients);
	IConsolePrintF(CC_DEFAULT, "Current/maximum companies:  %2d/%2d", ActivePlayerCount(), _settings_client.network.max_companies);
	IConsolePrintF(CC_DEFAULT, "Current/maximum spectators: %2d/%2d", NetworkSpectatorCount(), _settings_client.network.max_spectators);

	return true;
}

DEF_CONSOLE_CMD(ConKick)
{
	NetworkClientInfo *ci;
	uint32 index;

	if (argc == 0) {
		IConsoleHelp("Kick a player from a network game. Usage: 'kick <ip | client-id>'");
		IConsoleHelp("For client-id's, see the command 'clients'");
		return true;
	}

	if (argc != 2) return false;

	if (strchr(argv[1], '.') == NULL) {
		index = atoi(argv[1]);
		ci = NetworkFindClientInfoFromIndex(index);
	} else {
		ci = NetworkFindClientInfoFromIP(argv[1]);
		index = (ci == NULL) ? 0 : ci->client_index;
	}

	if (index == NETWORK_SERVER_INDEX) {
		IConsoleError("Silly boy, you can not kick yourself!");
		return true;
	}

	if (index == 0) {
		IConsoleError("Invalid client");
		return true;
	}

	if (ci != NULL) {
		NetworkServerSendError(index, NETWORK_ERROR_KICKED);
	} else {
		IConsoleError("Client not found");
	}

	return true;
}

DEF_CONSOLE_CMD(ConResetCompany)
{
	const Player *p;
	PlayerID index;

	if (argc == 0) {
		IConsoleHelp("Remove an idle company from the game. Usage: 'reset_company <company-id>'");
		IConsoleHelp("For company-id's, see the list of companies from the dropdown menu. Player 1 is 1, etc.");
		return true;
	}

	if (argc != 2) return false;

	index = (PlayerID)(atoi(argv[1]) - 1);

	/* Check valid range */
	if (!IsValidPlayer(index)) {
		IConsolePrintF(CC_ERROR, "Company does not exist. Company-id must be between 1 and %d.", MAX_PLAYERS);
		return true;
	}

	/* Check if company does exist */
	p = GetPlayer(index);
	if (!p->is_active) {
		IConsoleError("Company does not exist.");
		return true;
	}

	if (p->is_ai) {
		IConsoleError("Company is owned by an AI.");
		return true;
	}

	if (NetworkCompanyHasPlayers(index)) {
		IConsoleError("Cannot remove company: a client is connected to that company.");
		return false;
	}
	const NetworkClientInfo *ci = NetworkFindClientInfoFromIndex(NETWORK_SERVER_INDEX);
	if (ci->client_playas == index) {
		IConsoleError("Cannot remove company: the server is connected to that company.");
		return true;
	}

	/* It is safe to remove this company */
	DoCommandP(0, 2, index, NULL, CMD_PLAYER_CTRL);
	IConsolePrint(CC_DEFAULT, "Company deleted.");

	return true;
}

DEF_CONSOLE_CMD(ConNetworkClients)
{
	NetworkClientInfo *ci;

	if (argc == 0) {
		IConsoleHelp("Get a list of connected clients including their ID, name, company-id, and IP. Usage: 'clients'");
		return true;
	}

	FOR_ALL_ACTIVE_CLIENT_INFOS(ci) {
		IConsolePrintF(CC_INFO, "Client #%1d  name: '%s'  company: %1d  IP: %s",
		               ci->client_index, ci->client_name,
		               ci->client_playas + (IsValidPlayer(ci->client_playas) ? 1 : 0),
		               GetPlayerIP(ci));
	}

	return true;
}

DEF_CONSOLE_CMD(ConNetworkConnect)
{
	char *ip;
	const char *port = NULL;
	const char *player = NULL;
	uint16 rport;

	if (argc == 0) {
		IConsoleHelp("Connect to a remote OTTD server and join the game. Usage: 'connect <ip>'");
		IConsoleHelp("IP can contain port and player: 'IP[[#Player]:Port]', eg: 'server.ottd.org#2:443'");
		IConsoleHelp("Player #255 is spectator all others are a certain company with Company 1 being #1");
		return true;
	}

	if (argc < 2) return false;
	if (_networking) NetworkDisconnect(); // we are in network-mode, first close it!

	ip = argv[1];
	/* Default settings: default port and new company */
	rport = NETWORK_DEFAULT_PORT;
	_network_playas = PLAYER_NEW_COMPANY;

	ParseConnectionString(&player, &port, ip);

	IConsolePrintF(CC_DEFAULT, "Connecting to %s...", ip);
	if (player != NULL) {
		_network_playas = (PlayerID)atoi(player);
		IConsolePrintF(CC_DEFAULT, "    player-no: %d", _network_playas);

		/* From a user pov 0 is a new player, internally it's different and all
		 * players are offset by one to ease up on users (eg players 1-8 not 0-7) */
		if (_network_playas != PLAYER_SPECTATOR) {
			_network_playas--;
			if (!IsValidPlayer(_network_playas)) return false;
		}
	}
	if (port != NULL) {
		rport = atoi(port);
		IConsolePrintF(CC_DEFAULT, "    port: %s", port);
	}

	NetworkClientConnectGame(ip, rport);

	return true;
}

#endif /* ENABLE_NETWORK */

/* ******************************** */
/*   script file console commands   */
/* ******************************** */

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

/* **************************** */
/*   default console commands   */
/* **************************** */
extern bool CloseConsoleLogIfActive();

DEF_CONSOLE_CMD(ConScript)
{
	extern FILE* _iconsole_output_file;

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

extern void SwitchMode(int new_mode);

DEF_CONSOLE_CMD(ConRestart)
{
	if (argc == 0) {
		IConsoleHelp("Restart game. Usage: 'restart'");
		IConsoleHelp("Restarts a game. It tries to reproduce the exact same map as the game started with.");
		return true;
	}

	/* Don't copy the _newgame pointers to the real pointers, so call SwitchMode directly */
	_settings_game.game_creation.map_x = MapLogX();
	_settings_game.game_creation.map_y = FindFirstBit(MapSizeY());
	SwitchMode(SM_NEWGAME);
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
		NetworkServerSendChat(NETWORK_ACTION_CHAT, DESTTYPE_BROADCAST, 0, argv[1], NETWORK_SERVER_INDEX);
	}

	return true;
}

DEF_CONSOLE_CMD(ConPlayers)
{
	Player *p;

	if (argc == 0) {
		IConsoleHelp("List the in-game details of all clients connected to the server. Usage 'players'");
		return true;
	}
	NetworkPopulateCompanyInfo();

	FOR_ALL_PLAYERS(p) {
		char buffer[512];

		if (!p->is_active) continue;

		const NetworkPlayerInfo *npi = &_network_player_info[p->index];

		GetString(buffer, STR_00D1_DARK_BLUE + _player_colors[p->index], lastof(buffer));
		IConsolePrintF(CC_INFO, "#:%d(%s) Company Name: '%s'  Year Founded: %d  Money: %" OTTD_PRINTF64 "d  Loan: %" OTTD_PRINTF64 "d  Value: %" OTTD_PRINTF64 "d  (T:%d, R:%d, P:%d, S:%d) %sprotected",
			p->index + 1, buffer, npi->company_name, p->inaugurated_year, (int64)p->player_money, (int64)p->current_loan, (int64)CalculateCompanyValue(p),
			/* trains      */ npi->num_vehicle[0],
			/* lorry + bus */ npi->num_vehicle[1] + npi->num_vehicle[2],
			/* planes      */ npi->num_vehicle[3],
			/* ships       */ npi->num_vehicle[4],
			/* protected   */ StrEmpty(npi->password) ? "un" : "");
	}

	return true;
}

DEF_CONSOLE_CMD(ConSayPlayer)
{
	if (argc == 0) {
		IConsoleHelp("Chat to a certain player in a multiplayer game. Usage: 'say_player <player-no> \"<msg>\"'");
		IConsoleHelp("PlayerNo is the player that plays as company <playerno>, 1 through max_players");
		return true;
	}

	if (argc != 3) return false;

	if (atoi(argv[1]) < 1 || atoi(argv[1]) > MAX_PLAYERS) {
		IConsolePrintF(CC_DEFAULT, "Unknown player. Player range is between 1 and %d.", MAX_PLAYERS);
		return true;
	}

	if (!_network_server) {
		NetworkClientSendChat(NETWORK_ACTION_CHAT_COMPANY, DESTTYPE_TEAM, atoi(argv[1]), argv[2]);
	} else {
		NetworkServerSendChat(NETWORK_ACTION_CHAT_COMPANY, DESTTYPE_TEAM, atoi(argv[1]), argv[2], NETWORK_SERVER_INDEX);
	}

	return true;
}

DEF_CONSOLE_CMD(ConSayClient)
{
	if (argc == 0) {
		IConsoleHelp("Chat to a certain player in a multiplayer game. Usage: 'say_client <client-no> \"<msg>\"'");
		IConsoleHelp("For client-id's, see the command 'clients'");
		return true;
	}

	if (argc != 3) return false;

	if (!_network_server) {
		NetworkClientSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, atoi(argv[1]), argv[2]);
	} else {
		NetworkServerSendChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, atoi(argv[1]), argv[2], NETWORK_SERVER_INDEX);
	}

	return true;
}

extern void HashCurrentCompanyPassword();

/* Also use from within player_gui to change the password graphically */
bool NetworkChangeCompanyPassword(byte argc, char *argv[])
{
	if (argc == 0) {
		if (!IsValidPlayer(_local_player)) return true; // dedicated server
		IConsolePrintF(CC_WARNING, "Current value for 'company_pw': %s", _network_player_info[_local_player].password);
		return true;
	}

	if (!IsValidPlayer(_local_player)) {
		IConsoleError("You have to own a company to make use of this command.");
		return false;
	}

	if (argc != 1) return false;

	if (strcmp(argv[0], "*") == 0) argv[0][0] = '\0';

	ttd_strlcpy(_network_player_info[_local_player].password, argv[0], sizeof(_network_player_info[_local_player].password));

	if (!_network_server) {
		NetworkClientSetPassword();
	} else {
		HashCurrentCompanyPassword();
	}

	IConsolePrintF(CC_WARNING, "'company_pw' changed to:  %s", _network_player_info[_local_player].password);

	return true;
}

#endif /* ENABLE_NETWORK */

DEF_CONSOLE_CMD(ConPatch)
{
	if (argc == 0) {
		IConsoleHelp("Change patch variables for all players. Usage: 'patch <name> [<value>]'");
		IConsoleHelp("Omitting <value> will print out the current value of the patch-setting.");
		return true;
	}

	if (argc == 1 || argc > 3) return false;

	if (argc == 2) {
		IConsoleGetPatchSetting(argv[1]);
	} else {
		IConsoleSetPatchSetting(argv[1], argv[2]);
	}

	return true;
}

DEF_CONSOLE_CMD(ConListPatches)
{
	if (argc == 0) {
		IConsoleHelp("List patch options. Usage: 'list_patches'");
		return true;
	}

	if (argc != 1) return false;

	IConsoleListPatches();
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


#ifdef _DEBUG
/* ****************************************** */
/*  debug commands and variables */
/* ****************************************** */

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

/* ****************************************** */
/*  console command and variable registration */
/* ****************************************** */

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
	IConsoleCmdRegister("list_cmds",    ConListCommands);
	IConsoleCmdRegister("list_vars",    ConListVariables);
	IConsoleCmdRegister("list_aliases", ConListAliases);
	IConsoleCmdRegister("newgame",      ConNewGame);
	IConsoleCmdRegister("restart",      ConRestart);
	IConsoleCmdRegister("getseed",      ConGetSeed);
	IConsoleCmdRegister("getdate",      ConGetDate);
	IConsoleCmdRegister("quit",         ConExit);
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
	IConsoleCmdRegister("ls",           ConListFiles);
	IConsoleCmdRegister("cd",           ConChangeDirectory);
	IConsoleCmdRegister("pwd",          ConPrintWorkingDirectory);
	IConsoleCmdRegister("clear",        ConClearBuffer);
	IConsoleCmdRegister("patch",        ConPatch);
	IConsoleCmdRegister("list_patches", ConListPatches);

	IConsoleAliasRegister("dir",      "ls");
	IConsoleAliasRegister("del",      "rm %+");
	IConsoleAliasRegister("newmap",   "newgame");
	IConsoleAliasRegister("new_map",  "newgame");
	IConsoleAliasRegister("new_game", "newgame");


	IConsoleVarRegister("developer", &_stdlib_developer, ICONSOLE_VAR_BYTE, "Redirect debugging output from the console/command line to the ingame console (value 2). Default value: 1");

	/* networking variables and functions */
#ifdef ENABLE_NETWORK
	/* Network hooks; only active in network */
	IConsoleCmdHookAdd ("resetengines", ICONSOLE_HOOK_ACCESS, ConHookNoNetwork);

	/*** Networking commands ***/
	IConsoleCmdRegister("say",             ConSay);
	IConsoleCmdHookAdd("say",              ICONSOLE_HOOK_ACCESS, ConHookNeedNetwork);
	IConsoleCmdRegister("players",             ConPlayers);
	IConsoleCmdHookAdd("players",              ICONSOLE_HOOK_ACCESS, ConHookServerOnly);
	IConsoleCmdRegister("say_player",      ConSayPlayer);
	IConsoleCmdHookAdd("say_player",       ICONSOLE_HOOK_ACCESS, ConHookNeedNetwork);
	IConsoleCmdRegister("say_client",      ConSayClient);
	IConsoleCmdHookAdd("say_client",       ICONSOLE_HOOK_ACCESS, ConHookNeedNetwork);

	IConsoleCmdRegister("connect",         ConNetworkConnect);
	IConsoleCmdHookAdd("connect",          ICONSOLE_HOOK_ACCESS, ConHookClientOnly);
	IConsoleAliasRegister("join",          "connect %A");
	IConsoleCmdRegister("clients",         ConNetworkClients);
	IConsoleCmdHookAdd("clients",          ICONSOLE_HOOK_ACCESS, ConHookNeedNetwork);
	IConsoleCmdRegister("status",          ConStatus);
	IConsoleCmdHookAdd("status",           ICONSOLE_HOOK_ACCESS, ConHookServerOnly);
	IConsoleCmdRegister("server_info",     ConServerInfo);
	IConsoleCmdHookAdd("server_info",      ICONSOLE_HOOK_ACCESS, ConHookServerOnly);
	IConsoleAliasRegister("info",          "server_info");
	IConsoleCmdRegister("rcon",            ConRcon);
	IConsoleCmdHookAdd("rcon",             ICONSOLE_HOOK_ACCESS, ConHookNeedNetwork);

	IConsoleCmdRegister("reset_company",   ConResetCompany);
	IConsoleCmdHookAdd("reset_company",    ICONSOLE_HOOK_ACCESS, ConHookServerOnly);
	IConsoleAliasRegister("clean_company", "reset_company %A");
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

	IConsoleAliasRegister("net_frame_freq",        "patch frame_freq %+");
	IConsoleAliasRegister("net_sync_freq",         "patch sync_freq %+");
	IConsoleAliasRegister("server_pw",             "patch server_password %+");
	IConsoleAliasRegister("server_password",       "patch server_password %+");
	IConsoleAliasRegister("rcon_pw",               "patch rcon_password %+");
	IConsoleAliasRegister("rcon_password",         "patch rcon_password %+");
	IConsoleAliasRegister("name",                  "patch player_name %+");
	IConsoleAliasRegister("server_name",           "patch server_name %+");
	IConsoleAliasRegister("server_port",           "patch server_port %+");
	IConsoleAliasRegister("server_ip",             "patch server_bind_ip %+");
	IConsoleAliasRegister("server_bind_ip",        "patch server_bind_ip %+");
	IConsoleAliasRegister("server_ip_bind",        "patch server_bind_ip %+");
	IConsoleAliasRegister("server_bind",           "patch server_bind_ip %+");
	IConsoleAliasRegister("server_advertise",      "patch server_advertise %+");
	IConsoleAliasRegister("max_clients",           "patch max_clients %+");
	IConsoleAliasRegister("max_companies",         "patch max_companies %+");
	IConsoleAliasRegister("max_spectators",        "patch max_spectators %+");
	IConsoleAliasRegister("max_join_time",         "patch max_join_time %+");
	IConsoleAliasRegister("pause_on_join",         "patch pause_on_join %+");
	IConsoleAliasRegister("autoclean_companies",   "patch autoclean_companies %+");
	IConsoleAliasRegister("autoclean_protected",   "patch autoclean_protected %+");
	IConsoleAliasRegister("autoclean_unprotected", "patch autoclean_unprotected %+");
	IConsoleAliasRegister("restart_game_year",     "patch restart_game_year %+");
	IConsoleAliasRegister("min_players",           "patch min_players %+");
	IConsoleAliasRegister("reload_cfg",            "patch reload_cfg %+");
#endif /* ENABLE_NETWORK */

	// debugging stuff
#ifdef _DEBUG
	IConsoleDebugLibRegister();
#endif
}
