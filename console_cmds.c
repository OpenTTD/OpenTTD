/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "console.h"
#include "debug.h"
#include "engine.h"
#include "functions.h"
#include "saveload.h"
#include "string.h"
#include "variables.h"
#include "network_data.h"
#include "network_client.h"
#include "network_server.h"
#include "network_udp.h"
#include "command.h"
#include "settings.h"
#include "fios.h"
#include "vehicle.h"
#include "station.h"
#include "strings.h"
#include "screenshot.h"
#include "genworld.h"
#include "date.h"
#include "network.h"

// ** scriptfile handling ** //
static FILE *_script_file;
static bool _script_running;

// ** console command / variable defines ** //
#define DEF_CONSOLE_CMD(function) static bool function(byte argc, char *argv[])
#define DEF_CONSOLE_HOOK(function) static bool function(void)


/* **************************** */
/* variable and command hooks   */
/* **************************** */

#ifdef ENABLE_NETWORK

static inline bool NetworkAvailable(void)
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
	IConsolePrintF(_icolour_warn, "- %s", str);
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
	Vehicle* v;
	if (argc == 0) {
		IConsoleHelp("Stops all vehicles in the game. For debugging only! Use at your own risk... Usage: 'stopall'");
		return true;
	}

	FOR_ALL_VEHICLES(v) {
		/* Code ripped from CmdStartStopTrain. Can't call it, because of
		 * ownership problems, so we'll duplicate some code, for now */
		if (v->type == VEH_Train)
			v->u.rail.days_since_order_progr = 0;
		v->vehstatus |= VS_STOPPED;
		InvalidateWindowWidget(WC_VEHICLE_VIEW, v->index, STATUS_BAR);
		InvalidateWindow(WC_VEHICLE_DEPOT, v->tile);
	}
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
				IConsolePrint(_icolour_err, "Tile does not exist");
				return true;
			}
			ScrollMainWindowToTile((TileIndex)result);
			return true;
		}
	}

	return false;
}

extern bool SafeSaveOrLoad(const char *filename, int mode, int newgm);
extern void BuildFileList(void);
extern void SetFiosType(const byte fiostype);

/* Save the map to a file */
DEF_CONSOLE_CMD(ConSave)
{
	if (argc == 0) {
		IConsoleHelp("Save the current game. Usage: 'save <filename>'");
		return true;
	}

	if (argc == 2) {
		char buf[200];

		snprintf(buf, lengthof(buf), "%s%s%s.sav", _path.save_dir, PATHSEP, argv[1]);
		IConsolePrint(_icolour_def, "Saving map...");

		if (SaveOrLoad(buf, SL_SAVE) != SL_OK) {
			IConsolePrint(_icolour_err, "SaveMap failed");
		} else {
			IConsolePrintF(_icolour_def, "Map sucessfully saved to %s", buf);
		}
		return true;
	}

	return false;
}

static const FiosItem* GetFiosItem(const char* file)
{
	int i;

	_saveload_mode = SLD_LOAD_GAME;
	BuildFileList();

	for (i = 0; i < _fios_num; i++) {
		if (strcmp(file, _fios_list[i].name) == 0) break;
		if (strcmp(file, _fios_list[i].title) == 0) break;
	}

	if (i == _fios_num) { /* If no name matches, try to parse it as number */
		char* endptr;

		i = strtol(file, &endptr, 10);
		if (file == endptr || *endptr != '\0') i = -1;
	}

	return IS_INT_INSIDE(i, 0, _fios_num) ? &_fios_list[i] : NULL;
}


DEF_CONSOLE_CMD(ConLoad)
{
	const FiosItem *item;
	const char *file;

	if (argc == 0) {
		IConsoleHelp("Load a game by name or index. Usage: 'load <file | number>'");
		return true;
	}

	if (argc != 2) return false;

	file = argv[1];
	item = GetFiosItem(file);
	if (item != NULL) {
		switch (item->type) {
			case FIOS_TYPE_FILE: case FIOS_TYPE_OLDFILE: {
				_switch_mode = SM_LOAD;
				SetFiosType(item->type);

				ttd_strlcpy(_file_to_saveload.name, FiosBrowseTo(item), sizeof(_file_to_saveload.name));
				ttd_strlcpy(_file_to_saveload.title, item->title, sizeof(_file_to_saveload.title));
			} break;
			default: IConsolePrintF(_icolour_err, "%s: Not a savegame.", file);
		}
	} else {
		IConsolePrintF(_icolour_err, "%s: No such file or directory.", file);
	}

	FiosFreeSavegameList();
	return true;
}


DEF_CONSOLE_CMD(ConRemove)
{
	const FiosItem* item;
	const char* file;

	if (argc == 0) {
		IConsoleHelp("Remove a savegame by name or index. Usage: 'rm <file | number>'");
		return true;
	}

	if (argc != 2) return false;

	file = argv[1];
	item = GetFiosItem(file);
	if (item != NULL) {
		if (!FiosDelete(item->name))
			IConsolePrintF(_icolour_err, "%s: Failed to delete file", file);
	} else {
		IConsolePrintF(_icolour_err, "%s: No such file or directory.", file);
	}

	FiosFreeSavegameList();
	return true;
}


/* List all the files in the current dir via console */
DEF_CONSOLE_CMD(ConListFiles)
{
	int i;

	if (argc == 0) {
		IConsoleHelp("List all loadable savegames and directories in the current dir via console. Usage: 'ls | dir'");
		return true;
	}

	BuildFileList();

	for (i = 0; i < _fios_num; i++) {
		const FiosItem *item = &_fios_list[i];
		IConsolePrintF(_icolour_def, "%d) %s", i, item->title);
	}

	FiosFreeSavegameList();
	return true;
}

/* Change the dir via console */
DEF_CONSOLE_CMD(ConChangeDirectory)
{
	const FiosItem *item;
	const char *file;

	if (argc == 0) {
		IConsoleHelp("Change the dir via console. Usage: 'cd <directory | number>'");
		return true;
	}

	if (argc != 2) return false;

	file = argv[1];
	item = GetFiosItem(file);
	if (item != NULL) {
		switch (item->type) {
			case FIOS_TYPE_DIR: case FIOS_TYPE_DRIVE: case FIOS_TYPE_PARENT:
				FiosBrowseTo(item);
				break;
			default: IConsolePrintF(_icolour_err, "%s: Not a directory.", file);
		}
	} else {
		IConsolePrintF(_icolour_err, "%s: No such file or directory.", file);
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
	IConsolePrint(_icolour_def, path);
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
		banip = inet_ntoa(*(struct in_addr *)&ci->client_ip);
		SEND_COMMAND(PACKET_SERVER_ERROR)(NetworkFindClientStateFromIndex(index), NETWORK_ERROR_KICKED);
		IConsolePrint(_icolour_def, "Client banned");
	} else {
		IConsolePrint(_icolour_def, "Client not online, banned IP");
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
			IConsolePrint(_icolour_def, "IP unbanned.");
			return true;
		}
	}

	IConsolePrint(_icolour_def, "IP not in ban-list.");
	return true;
}

DEF_CONSOLE_CMD(ConBanList)
{
	uint i;

	if (argc == 0) {
		IConsoleHelp("List the IP's of banned clients: Usage 'banlist'");
		return true;
	}

	IConsolePrint(_icolour_def, "Banlist: ");

	for (i = 0; i < lengthof(_network_ban_list); i++) {
		if (_network_ban_list[i] != NULL)
			IConsolePrintF(_icolour_def, "  %d) %s", i + 1, _network_ban_list[i]);
	}

	return true;
}

DEF_CONSOLE_CMD(ConPauseGame)
{
	if (argc == 0) {
		IConsoleHelp("Pause a network game. Usage: 'pause'");
		return true;
	}

	if (_pause == 0) {
		DoCommandP(0, 1, 0, NULL, CMD_PAUSE);
		IConsolePrint(_icolour_def, "Game paused.");
	} else {
		IConsolePrint(_icolour_def, "Game is already paused.");
	}

	return true;
}

DEF_CONSOLE_CMD(ConUnPauseGame)
{
	if (argc == 0) {
		IConsoleHelp("Unpause a network game. Usage: 'unpause'");
		return true;
	}

	if (_pause != 0) {
		DoCommandP(0, 0, 0, NULL, CMD_PAUSE);
		IConsolePrint(_icolour_def, "Game unpaused.");
	} else {
		IConsolePrint(_icolour_def, "Game is already unpaused.");
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

	SEND_COMMAND(PACKET_CLIENT_RCON)(argv[1], argv[2]);
	return true;
}

DEF_CONSOLE_CMD(ConStatus)
{
	static const char* const stat_str[] = {
		"inactive",
		"authorized",
		"waiting",
		"loading map",
		"map done",
		"ready",
		"active"
	};

	const NetworkClientState *cs;

	if (argc == 0) {
		IConsoleHelp("List the status of all clients connected to the server. Usage 'status'");
		return true;
	}

	FOR_ALL_CLIENTS(cs) {
		int lag = NetworkCalculateLag(cs);
		const NetworkClientInfo *ci = DEREF_CLIENT_INFO(cs);
		const char* status;

		status = (cs->status < lengthof(stat_str) ? stat_str[cs->status] : "unknown");
		IConsolePrintF(8, "Client #%1d  name: '%s'  status: '%s'  frame-lag: %3d  company: %1d  IP: %s  unique-id: '%s'",
			cs->index, ci->client_name, status, lag, ci->client_playas, GetPlayerIP(ci), ci->unique_id);
	}

	return true;
}

DEF_CONSOLE_CMD(ConServerInfo)
{
	const NetworkGameInfo *gi;

	if (argc == 0) {
		IConsoleHelp("List current and maximum client/player limits. Usage 'server_info'");
		IConsoleHelp("You can change these values by setting the variables 'max_clients', 'max_companies' and 'max_spectators'");
		return true;
	}

	gi = &_network_game_info;
	IConsolePrintF(_icolour_def, "Current/maximum clients:    %2d/%2d", gi->clients_on, gi->clients_max);
	IConsolePrintF(_icolour_def, "Current/maximum companies:  %2d/%2d", ActivePlayerCount(), gi->companies_max);
	IConsolePrintF(_icolour_def, "Current/maximum spectators: %2d/%2d", NetworkSpectatorCount(), gi->spectators_max);

	return true;
}

DEF_CONSOLE_HOOK(ConHookValidateMaxClientsCount)
{
	/* XXX - hardcoded, string limiation -- TrueLight
	 * XXX - also see network.c:NetworkStartup ~1356 */
	if (_network_game_info.clients_max > 10) {
		_network_game_info.clients_max = 10;
		IConsoleError("Maximum clients out of bounds, truncating to limit.");
	}

	return true;
}

DEF_CONSOLE_HOOK(ConHookValidateMaxCompaniesCount)
{
	if (_network_game_info.companies_max > MAX_PLAYERS) {
		_network_game_info.companies_max = MAX_PLAYERS;
		IConsoleError("Maximum companies out of bounds, truncating to limit.");
	}

	return true;
}

DEF_CONSOLE_HOOK(ConHookValidateMaxSpectatorsCount)
{
	/* XXX @see ConHookValidateMaxClientsCount */
	if (_network_game_info.spectators_max > 10) {
		_network_game_info.spectators_max = 10;
		IConsoleError("Maximum spectators out of bounds, truncating to limit.");
	}

	return true;
}

DEF_CONSOLE_HOOK(ConHookCheckMinPlayers)
{
	CheckMinPlayers();
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
		SEND_COMMAND(PACKET_SERVER_ERROR)(NetworkFindClientStateFromIndex(index), NETWORK_ERROR_KICKED);
	} else {
		IConsoleError("Client not found");
	}

	return true;
}

DEF_CONSOLE_CMD(ConResetCompany)
{
	Player *p;
	NetworkClientState *cs;
	NetworkClientInfo *ci;
	byte index;

	if (argc == 0) {
		IConsoleHelp("Remove an idle company from the game. Usage: 'reset_company <company-id>'");
		IConsoleHelp("For company-id's, see the list of companies from the dropdown menu. Player 1 is 1, etc.");
		return true;
	}

	if (argc != 2) return false;

	index = atoi(argv[1]);

	/* Check valid range */
	if (index < 1 || index > MAX_PLAYERS) {
		IConsolePrintF(_icolour_err, "Company does not exist. Company-id must be between 1 and %d.", MAX_PLAYERS);
		return true;
	}

	/* Check if company does exist */
	index--;
	p = GetPlayer(index);
	if (!p->is_active) {
		IConsoleError("Company does not exist.");
		return true;
	}

	if (p->is_ai) {
		IConsoleError("Company is owned by an AI.");
		return true;
	}

	/* Check if the company has active players */
	FOR_ALL_CLIENTS(cs) {
		ci = DEREF_CLIENT_INFO(cs);
		if (ci->client_playas - 1 == index) {
			IConsoleError("Cannot remove company: a client is connected to that company.");
			return true;
		}
	}
	ci = NetworkFindClientInfoFromIndex(NETWORK_SERVER_INDEX);
	if (ci->client_playas - 1 == index) {
		IConsoleError("Cannot remove company: the server is connected to that company.");
		return true;
	}

	/* It is safe to remove this company */
	DoCommandP(0, 2, index, NULL, CMD_PLAYER_CTRL);
	IConsolePrint(_icolour_def, "Company deleted.");

	return true;
}

DEF_CONSOLE_CMD(ConNetworkClients)
{
	NetworkClientInfo *ci;

	if (argc == 0) {
		IConsoleHelp("Get a list of connected clients including their ID, name, company-id, and IP. Usage: 'clients'");
		return true;
	}

	for (ci = _network_client_info; ci != &_network_client_info[MAX_CLIENT_INFO]; ci++) {
		if (ci->client_index != NETWORK_EMPTY_INDEX) {
			IConsolePrintF(8, "Client #%1d  name: '%s'  company: %1d  IP: %s",
				ci->client_index, ci->client_name, ci->client_playas, GetPlayerIP(ci));
		}
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
		IConsoleHelp("IP can contain port and player: 'IP#Player:Port', eg: 'server.ottd.org#2:443'");
		return true;
	}

	if (argc < 2) return false;

	if (_networking) // We are in network-mode, first close it!
		NetworkDisconnect();

	ip = argv[1];
	rport = NETWORK_DEFAULT_PORT;

	ParseConnectionString(&player, &port, ip);

	IConsolePrintF(_icolour_def, "Connecting to %s...", ip);
	if (player != NULL) {
		_network_playas = atoi(player);
		IConsolePrintF(_icolour_def, "    player-no: %s", player);
	}
	if (port != NULL) {
		rport = atoi(port);
		IConsolePrintF(_icolour_def, "    port: %s", port);
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

	_script_file = fopen(argv[1], "r");

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
	fclose(_script_file);
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
extern bool CloseConsoleLogIfActive(void);

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

		IConsolePrintF(_icolour_def, "file output started to: %s", argv[1]);
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
	IConsolePrint(_icolour_def, argv[1]);
	return true;
}

DEF_CONSOLE_CMD(ConEchoC)
{
	if (argc == 0) {
		IConsoleHelp("Print back the first argument to the console in a given colour. Usage: 'echoc <colour> <arg2>'");
		return true;
	}

	if (argc < 3) return false;
	IConsolePrint(atoi(argv[1]), argv[2]);
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
	_patches.map_x = MapLogX();
	_patches.map_y = FindFirstBit(MapSizeY());
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

	IConsolePrintF(_icolour_def, "Generation Seed: %u", _patches.generation_seed);
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

	IConsolePrintF(_icolour_def, "variable name: %s", var->name);
	IConsolePrintF(_icolour_def, "variable type: %s", _icon_vartypes[var->type]);
	IConsolePrintF(_icolour_def, "variable addr: 0x%X", var->addr);

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

	IConsolePrintF(_icolour_def, "command name: %s", cmd->name);
	IConsolePrintF(_icolour_def, "command proc: 0x%X", cmd->proc);

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
		IConsolePrintF(_icolour_def, "Current debug-level: '%s'", GetDebugString());
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
			IConsolePrintF(_icolour_err, "ERROR: alias is of special type, please see its execution-line: '%s'", alias->cmdline);
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

	IConsolePrint(13, " ---- OpenTTD Console Help ---- ");
	IConsolePrint( 1, " - variables: [command to list all variables: list_vars]");
	IConsolePrint( 1, " set value with '<var> = <value>', use '++/--' to in-or decrement");
	IConsolePrint( 1, " or omit '=' and just '<var> <value>'. get value with typing '<var>'");
	IConsolePrint( 1, " - commands: [command to list all commands: list_cmds]");
	IConsolePrint( 1, " call commands with '<command> <arg2> <arg3>...'");
	IConsolePrint( 1, " - to assign strings, or use them as arguments, enclose it within quotes");
	IConsolePrint( 1, " like this: '<command> \"string argument with spaces\"'");
	IConsolePrint( 1, " - use 'help <command> | <variable>' to get specific information");
	IConsolePrint( 1, " - scroll console output with shift + (up | down) | (pageup | pagedown))");
	IConsolePrint( 1, " - scroll console input history with the up | down arrows");
	IConsolePrint( 1, "");
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
				IConsolePrintF(_icolour_def, "%s", cmd->name);
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
			IConsolePrintF(_icolour_def, "%s", var->name);
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
			IConsolePrintF(_icolour_def, "%s => %s", alias->name, alias->cmdline);
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
		SEND_COMMAND(PACKET_CLIENT_CHAT)(NETWORK_ACTION_CHAT, DESTTYPE_BROADCAST, 0 /* param does not matter */, argv[1]);
	} else {
		NetworkServer_HandleChat(NETWORK_ACTION_CHAT, DESTTYPE_BROADCAST, 0, argv[1], NETWORK_SERVER_INDEX);
	}

	return true;
}

#ifdef ENABLE_NETWORK
	#include "table/strings.h"
#endif /* ENABLE_NETWORK */

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

		GetString(buffer, STR_00D1_DARK_BLUE + _player_colors[p->index]);
		IConsolePrintF(8, "#:%d(%s) Company Name: '%s'  Year Founded: %d  Money: %d  Loan: %d  Value: %" OTTD_PRINTF64 "d  (T:%d, R:%d, P:%d, S:%d)",
			p->index + 1, buffer, _network_player_info[p->index].company_name, p->inaugurated_year, p->player_money, p->current_loan, CalculateCompanyValue(p),
			/* trains      */ _network_player_info[p->index].num_vehicle[0],
			/* lorry + bus */ _network_player_info[p->index].num_vehicle[1] + _network_player_info[p->index].num_vehicle[2],
			/* planes      */ _network_player_info[p->index].num_vehicle[3],
			/* ships       */ _network_player_info[p->index].num_vehicle[4]);
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
		IConsolePrintF(_icolour_def, "Unknown player. Player range is between 1 and %d.", MAX_PLAYERS);
		return true;
	}

	if (!_network_server) {
		SEND_COMMAND(PACKET_CLIENT_CHAT)(NETWORK_ACTION_CHAT_PLAYER, DESTTYPE_PLAYER, atoi(argv[1]), argv[2]);
	} else {
		NetworkServer_HandleChat(NETWORK_ACTION_CHAT_PLAYER, DESTTYPE_PLAYER, atoi(argv[1]), argv[2], NETWORK_SERVER_INDEX);
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
		SEND_COMMAND(PACKET_CLIENT_CHAT)(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, atoi(argv[1]), argv[2]);
	} else {
		NetworkServer_HandleChat(NETWORK_ACTION_CHAT_CLIENT, DESTTYPE_CLIENT, atoi(argv[1]), argv[2], NETWORK_SERVER_INDEX);
	}

	return true;
}

DEF_CONSOLE_HOOK(ConHookServerPW)
{
	if (strcmp(_network_server_password, "*") == 0) {
		_network_server_password[0] = '\0';
		_network_game_info.use_password = 0;
	} else {
		ttd_strlcpy(_network_game_info.server_password, _network_server_password, sizeof(_network_server_password));
		_network_game_info.use_password = 1;
	}

	return true;
}

DEF_CONSOLE_HOOK(ConHookRconPW)
{
	if (strcmp(_network_rcon_password, "*") == 0)
		_network_rcon_password[0] = '\0';

	ttd_strlcpy(_network_game_info.rcon_password, _network_rcon_password, sizeof(_network_game_info.rcon_password));

	return true;
}

/* Also use from within player_gui to change the password graphically */
bool NetworkChangeCompanyPassword(byte argc, char *argv[])
{
	if (argc == 0) {
		if (_local_player >= MAX_PLAYERS) return true; // dedicated server
		IConsolePrintF(_icolour_warn, "Current value for 'company_pw': %s", _network_player_info[_local_player].password);
		return true;
	}

	if (_local_player >= MAX_PLAYERS) {
		IConsoleError("You have to own a company to make use of this command.");
		return false;
	}

	if (argc != 1) return false;

	if (strcmp(argv[0], "*") == 0) argv[0][0] = '\0';

	ttd_strlcpy(_network_player_info[_local_player].password, argv[0], sizeof(_network_player_info[_local_player].password));

	if (!_network_server)
		SEND_COMMAND(PACKET_CLIENT_SET_PASSWORD)(_network_player_info[_local_player].password);

	IConsolePrintF(_icolour_warn, "'company_pw' changed to:  %s", _network_player_info[_local_player].password);

	return true;
}

DEF_CONSOLE_HOOK(ConProcPlayerName)
{
	NetworkClientInfo *ci = NetworkFindClientInfoFromIndex(_network_own_client_index);

	if (ci == NULL) return false;

	// Don't change the name if it is the same as the old name
	if (strcmp(ci->client_name, _network_player_name) != 0) {
		if (!_network_server) {
			SEND_COMMAND(PACKET_CLIENT_SET_NAME)(_network_player_name);
		} else {
			if (NetworkFindName(_network_player_name)) {
				NetworkTextMessage(NETWORK_ACTION_NAME_CHANGE, 1, false, ci->client_name, "%s", _network_player_name);
				ttd_strlcpy(ci->client_name, _network_player_name, sizeof(ci->client_name));
				NetworkUpdateClientInfo(NETWORK_SERVER_INDEX);
			}
		}
	}

	return true;
}

DEF_CONSOLE_HOOK(ConHookServerName)
{
	ttd_strlcpy(_network_game_info.server_name, _network_server_name, sizeof(_network_game_info.server_name));
	return true;
}

DEF_CONSOLE_HOOK(ConHookServerAdvertise)
{
	if (!_network_advertise) // remove us from advertising
		NetworkUDPRemoveAdvertise();

	return true;
}

DEF_CONSOLE_CMD(ConProcServerIP)
{
	if (argc == 0) {
		IConsolePrintF(_icolour_warn, "Current value for 'server_ip': %s", inet_ntoa(*(struct in_addr *)&_network_server_bind_ip));
		return true;
	}

	if (argc != 1) return false;

	_network_server_bind_ip = (strcmp(argv[0], "all") == 0) ? inet_addr("0.0.0.0") : inet_addr(argv[0]);
	snprintf(_network_server_bind_ip_host, sizeof(_network_server_bind_ip_host), "%s", inet_ntoa(*(struct in_addr *)&_network_server_bind_ip));
	IConsolePrintF(_icolour_warn, "'server_ip' changed to:  %s", inet_ntoa(*(struct in_addr *)&_network_server_bind_ip));
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
		uint32 val;

		if (GetArgumentInteger(&val, argv[2])) {
			if (!IConsoleSetPatchSetting(argv[1], val)) {
				IConsoleError("This command/variable is only available to a network server.");
			}
		}
	}

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

static void IConsoleDebugLibRegister(void)
{
	// debugging variables and functions
	extern bool _stdlib_con_developer; /* XXX extern in .c */

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
	IConsoleCmdRegister("ls",           ConListFiles);
	IConsoleCmdRegister("cd",           ConChangeDirectory);
	IConsoleCmdRegister("pwd",          ConPrintWorkingDirectory);
	IConsoleCmdRegister("clear",        ConClearBuffer);
	IConsoleCmdRegister("patch",        ConPatch);

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
	IConsoleVarRegister("net_frame_freq",        &_network_frame_freq, ICONSOLE_VAR_BYTE, "The amount of frames before a command will be (visibly) executed. Default value: 1");
	IConsoleVarHookAdd("net_frame_freq",         ICONSOLE_HOOK_ACCESS, ConHookServerOnly);
	IConsoleVarRegister("net_sync_freq",         &_network_sync_freq,  ICONSOLE_VAR_UINT16, "The amount of frames to check if the game is still in sync. Default value: 100");
	IConsoleVarHookAdd("net_sync_freq",          ICONSOLE_HOOK_ACCESS, ConHookServerOnly);

	IConsoleVarStringRegister("server_pw",       &_network_server_password, sizeof(_network_server_password), "Set the server password to protect your server. Use '*' to clear the password");
	IConsoleVarHookAdd("server_pw",              ICONSOLE_HOOK_ACCESS, ConHookServerOnly);
	IConsoleVarHookAdd("server_pw",              ICONSOLE_HOOK_POST_ACTION, ConHookServerPW);
	IConsoleAliasRegister("server_password",     "server_pw %+");
	IConsoleVarStringRegister("rcon_pw",         &_network_rcon_password, sizeof(_network_rcon_password), "Set the rcon-password to change server behaviour. Use '*' to disable rcon");
	IConsoleVarHookAdd("rcon_pw",                ICONSOLE_HOOK_ACCESS, ConHookServerOnly);
	IConsoleVarHookAdd("rcon_pw",                ICONSOLE_HOOK_POST_ACTION, ConHookRconPW);
	IConsoleAliasRegister("rcon_password",       "rcon_pw %+");
	IConsoleVarStringRegister("company_pw",      NULL, 0, "Set a password for your company, so no one without the correct password can join. Use '*' to clear the password");
	IConsoleVarHookAdd("company_pw",             ICONSOLE_HOOK_ACCESS, ConHookNeedNetwork);
	IConsoleVarProcAdd("company_pw",             NetworkChangeCompanyPassword);
	IConsoleAliasRegister("company_password",    "company_pw %+");

	IConsoleVarStringRegister("name",            &_network_player_name, sizeof(_network_player_name), "Set your name for multiplayer");
	IConsoleVarHookAdd("name",                   ICONSOLE_HOOK_ACCESS, ConHookNeedNetwork);
	IConsoleVarHookAdd("name",                   ICONSOLE_HOOK_POST_ACTION, ConProcPlayerName);
	IConsoleVarStringRegister("server_name",     &_network_server_name, sizeof(_network_server_name), "Set the name of the server for multiplayer");
	IConsoleVarHookAdd("server_name",            ICONSOLE_HOOK_ACCESS, ConHookServerOnly);
	IConsoleVarHookAdd("server_name",            ICONSOLE_HOOK_POST_ACTION, ConHookServerName);

	IConsoleVarRegister("server_port",           &_network_server_port, ICONSOLE_VAR_UINT32, "Set the server port. Changes take effect the next time you start a server");
	IConsoleVarRegister("server_ip",             &_network_server_bind_ip, ICONSOLE_VAR_UINT32, "Set the IP the server binds to. Changes take effect the next time you start a server. Use 'all' to bind to any IP.");
	IConsoleVarProcAdd("server_ip",              ConProcServerIP);
	IConsoleAliasRegister("server_bind_ip",      "server_ip %+");
	IConsoleAliasRegister("server_ip_bind",      "server_ip %+");
	IConsoleAliasRegister("server_bind",         "server_ip %+");
	IConsoleVarRegister("server_advertise",      &_network_advertise, ICONSOLE_VAR_BOOLEAN, "Set if the server will advertise to the master server and show up there");
	IConsoleVarHookAdd("server_advertise",       ICONSOLE_HOOK_ACCESS, ConHookServerOnly);
	IConsoleVarHookAdd("server_advertise",       ICONSOLE_HOOK_POST_ACTION, ConHookServerAdvertise);

	IConsoleVarRegister("max_clients",           &_network_game_info.clients_max, ICONSOLE_VAR_BYTE, "Control the maximum amount of connected players during runtime. Default value: 10");
	IConsoleVarHookAdd("max_clients",            ICONSOLE_HOOK_ACCESS, ConHookServerOnly);
	IConsoleVarHookAdd("max_clients",            ICONSOLE_HOOK_POST_ACTION, ConHookValidateMaxClientsCount);
	IConsoleVarRegister("max_companies",         &_network_game_info.companies_max, ICONSOLE_VAR_BYTE, "Control the maximum amount of active companies during runtime. Default value: 8");
	IConsoleVarHookAdd("max_companies",          ICONSOLE_HOOK_ACCESS, ConHookServerOnly);
	IConsoleVarHookAdd("max_companies",          ICONSOLE_HOOK_POST_ACTION, ConHookValidateMaxCompaniesCount);
	IConsoleVarRegister("max_spectators",        &_network_game_info.spectators_max, ICONSOLE_VAR_BYTE, "Control the maximum amount of active spectators during runtime. Default value: 9");
	IConsoleVarHookAdd("max_spectators",         ICONSOLE_HOOK_ACCESS, ConHookServerOnly);
	IConsoleVarHookAdd("max_spectators",         ICONSOLE_HOOK_POST_ACTION, ConHookValidateMaxSpectatorsCount);

	IConsoleVarRegister("max_join_time",         &_network_max_join_time, ICONSOLE_VAR_UINT16, "Set the maximum amount of time (ticks) a client is allowed to join. Default value: 500");

	IConsoleVarRegister("pause_on_join",         &_network_pause_on_join, ICONSOLE_VAR_BOOLEAN, "Set if the server should pause gameplay while a client is joining. This might help slow users");
	IConsoleVarHookAdd("pause_on_join",          ICONSOLE_HOOK_ACCESS, ConHookServerOnly);

	IConsoleVarRegister("autoclean_companies",   &_network_autoclean_companies, ICONSOLE_VAR_BOOLEAN, "Automatically shut down inactive companies to free them up for other players. Customize with 'autoclean_(un)protected'");
	IConsoleVarHookAdd("autoclean_companies",    ICONSOLE_HOOK_ACCESS, ConHookServerOnly);
	IConsoleVarRegister("autoclean_protected",   &_network_autoclean_protected, ICONSOLE_VAR_BYTE, "Automatically remove the password from an inactive company after the given amount of months");
	IConsoleVarHookAdd("autoclean_protected",    ICONSOLE_HOOK_ACCESS, ConHookServerOnly);
	IConsoleVarRegister("autoclean_unprotected", &_network_autoclean_unprotected, ICONSOLE_VAR_BYTE, "Automatically shut down inactive companies after the given amount of months");
	IConsoleVarHookAdd("autoclean_unprotected",  ICONSOLE_HOOK_ACCESS, ConHookServerOnly);
	IConsoleVarRegister("restart_game_year",     &_network_restart_game_year, ICONSOLE_VAR_UINT16, "Auto-restart the server when Jan 1st of the set year is reached. Use '0' to disable this");
	IConsoleVarHookAdd("restart_game_year",      ICONSOLE_HOOK_ACCESS, ConHookServerOnly);

	IConsoleVarRegister("min_players",           &_network_min_players, ICONSOLE_VAR_BYTE, "Automatically pause the game when the number of active players passes below the given amount");
	IConsoleVarHookAdd("min_players",            ICONSOLE_HOOK_ACCESS, ConHookServerOnly);
	IConsoleVarHookAdd("min_players",            ICONSOLE_HOOK_POST_ACTION, ConHookCheckMinPlayers);

#endif /* ENABLE_NETWORK */

	// debugging stuff
#ifdef _DEBUG
	IConsoleDebugLibRegister();
#endif
}
