/* $Id$ */

#include "stdafx.h"
#define VARDEF
#include "string.h"
#include "table/strings.h"
#include "debug.h"
#include "driver.h"
#include "saveload.h"
#include "strings.h"
#include "map.h"
#include "tile.h"
#include "void_map.h"
#include "helpers.hpp"

#include "openttd.h"
#include "bridge_map.h"
#include "functions.h"
#include "mixer.h"
#include "spritecache.h"
#include "strings.h"
#include "gfx.h"
#include "gfxinit.h"
#include "gui.h"
#include "station.h"
#include "station_map.h"
#include "town_map.h"
#include "tunnel_map.h"
#include "vehicle.h"
#include "viewport.h"
#include "window.h"
#include "player.h"
#include "command.h"
#include "town.h"
#include "industry.h"
#include "news.h"
#include "engine.h"
#include "sound.h"
#include "economy.h"
#include "fileio.h"
#include "hal.h"
#include "airport.h"
#include "aircraft.h"
#include "console.h"
#include "screenshot.h"
#include "network/network.h"
#include "signs.h"
#include "depot.h"
#include "waypoint.h"
#include "ai/ai.h"
#include "train.h"
#include "yapf/yapf.h"
#include "settings.h"
#include "genworld.h"
#include "date.h"
#include "clear_map.h"
#include "fontcache.h"
#include "newgrf_config.h"
#include "player_face.h"

#include "bridge_map.h"
#include "clear_map.h"
#include "rail_map.h"
#include "road_map.h"
#include "water_map.h"
#include "industry_map.h"
#include "unmovable_map.h"

#include <stdarg.h>

void CallLandscapeTick();
void IncreaseDate();
void DoPaletteAnimations();
void MusicLoop();
void ResetMusic();

extern void SetDifficultyLevel(int mode, GameOptions *gm_opt);
extern Player* DoStartupNewPlayer(bool is_ai);
extern void ShowOSErrorBox(const char *buf);

/* TODO: usrerror() for errors which are not of an internal nature but
 * caused by the user, i.e. missing files or fatal configuration errors.
 * Post-0.4.0 since Celestar doesn't want this in SVN before. --pasky */

void CDECL error(const char *s, ...)
{
	va_list va;
	char buf[512];

	va_start(va, s);
	vsnprintf(buf, lengthof(buf), s, va);
	va_end(va);

	ShowOSErrorBox(buf);
	if (_video_driver != NULL) _video_driver->stop();

	assert(0);
	exit(1);
}

void CDECL ShowInfoF(const char *str, ...)
{
	va_list va;
	char buf[1024];
	va_start(va, str);
	vsnprintf(buf, lengthof(buf), str, va);
	va_end(va);
	ShowInfo(buf);
}


void *ReadFileToMem(const char *filename, size_t *lenp, size_t maxsize)
{
	FILE *in;
	byte *mem;
	size_t len;

	in = fopen(filename, "rb");
	if (in == NULL) return NULL;

	fseek(in, 0, SEEK_END);
	len = ftell(in);
	fseek(in, 0, SEEK_SET);
	if (len > maxsize || (mem = MallocT<byte>(len + 1)) == NULL) {
		fclose(in);
		return NULL;
	}
	mem[len] = 0;
	if (fread(mem, len, 1, in) != 1) {
		fclose(in);
		free(mem);
		return NULL;
	}
	fclose(in);

	*lenp = len;
	return mem;
}

extern const char _openttd_revision[];
static void showhelp()
{
	char buf[4096], *p;

	p = buf;

	p += snprintf(p, lengthof(buf), "OpenTTD %s\n", _openttd_revision);
	p = strecpy(p,
		"\n"
		"\n"
		"Command line options:\n"
		"  -v drv              = Set video driver (see below)\n"
		"  -s drv              = Set sound driver (see below) (param bufsize,hz)\n"
		"  -m drv              = Set music driver (see below)\n"
		"  -r res              = Set resolution (for instance 800x600)\n"
		"  -h                  = Display this help text\n"
		"  -t year             = Set starting year\n"
		"  -d [[fac=]lvl[,...]]= Debug mode\n"
		"  -e                  = Start Editor\n"
		"  -g [savegame]       = Start new/save game immediately\n"
		"  -G seed             = Set random seed\n"
#if defined(ENABLE_NETWORK)
		"  -n [ip:port#player] = Start networkgame\n"
		"  -D [ip][:port]      = Start dedicated server\n"
		"  -l ip[:port]        = Redirect DEBUG()\n"
#if !defined(__MORPHOS__) && !defined(__AMIGA__) && !defined(WIN32)
		"  -f                  = Fork into the background (dedicated only)\n"
#endif
#endif /* ENABLE_NETWORK */
		"  -i                  = Force to use the DOS palette\n"
		"                          (use this if you see a lot of pink)\n"
		"  -c config_file      = Use 'config_file' instead of 'openttd.cfg'\n"
		"  -x                  = Do not automatically save to config file on exit\n",
		lastof(buf)
	);

	p = GetDriverList(p, lastof(buf));

	/* ShowInfo put output to stderr, but version information should go
	 * to stdout; this is the only exception */
#if !defined(WIN32) && !defined(WIN64)
	printf("%s\n", buf);
#else
	ShowInfo(buf);
#endif
}


struct MyGetOptData {
	char *opt;
	int numleft;
	char **argv;
	const char *options;
	const char *cont;

	MyGetOptData(int argc, char **argv, const char *options)
	{
		opt = NULL;
		numleft = argc;
		this->argv = argv;
		this->options = options;
		cont = NULL;
	}
};

static int MyGetOpt(MyGetOptData *md)
{
	const char *s,*r,*t;

	s = md->cont;
	if (s != NULL)
		goto md_continue_here;

	for (;;) {
		if (--md->numleft < 0) return -1;

		s = *md->argv++;
		if (*s == '-') {
md_continue_here:;
			s++;
			if (*s != 0) {
				// Found argument, try to locate it in options.
				if (*s == ':' || (r = strchr(md->options, *s)) == NULL) {
					// ERROR!
					return -2;
				}
				if (r[1] == ':') {
					// Item wants an argument. Check if the argument follows, or if it comes as a separate arg.
					if (!*(t = s + 1)) {
						// It comes as a separate arg. Check if out of args?
						if (--md->numleft < 0 || *(t = *md->argv) == '-') {
							// Check if item is optional?
							if (r[2] != ':')
								return -2;
							md->numleft++;
							t = NULL;
						} else {
							md->argv++;
						}
					}
					md->opt = (char*)t;
					md->cont = NULL;
					return *s;
				}
				md->opt = NULL;
				md->cont = s;
				return *s;
			}
		} else {
			// This is currently not supported.
			return -2;
		}
	}
}


static void ParseResolution(int res[2], const char *s)
{
	const char *t = strchr(s, 'x');
	if (t == NULL) {
		ShowInfoF("Invalid resolution '%s'", s);
		return;
	}

	res[0] = clamp(strtoul(s, NULL, 0), 64, MAX_SCREEN_WIDTH);
	res[1] = clamp(strtoul(t + 1, NULL, 0), 64, MAX_SCREEN_HEIGHT);
}

static void InitializeDynamicVariables()
{
	/* Dynamic stuff needs to be initialized somewhere... */
	_town_sort     = NULL;
	_industry_sort = NULL;
}


static void UnInitializeGame()
{
	UnInitWindowSystem();

	/* Uninitialize airport state machines */
	UnInitializeAirports();

	/* Uninitialize variables that are allocated dynamically */
	CleanPool(&_Town_pool);
	CleanPool(&_Industry_pool);
	CleanPool(&_Station_pool);
	CleanPool(&_Vehicle_pool);
	CleanPool(&_Sign_pool);
	CleanPool(&_Order_pool);

	free((void*)_town_sort);
	free((void*)_industry_sort);

	free(_config_file);
}

static void LoadIntroGame()
{
	char filename[256];

	_game_mode = GM_MENU;
	CLRBITS(_display_opt, DO_TRANS_BUILDINGS); // don't make buildings transparent in intro
	_opt_ptr = &_opt_newgame;
	ResetGRFConfig(false);

	// Setup main window
	ResetWindowSystem();
	SetupColorsAndInitialWindow();

	// Generate a world.
	snprintf(filename, lengthof(filename), "%sopntitle.dat",  _paths.data_dir);
#if defined SECOND_DATA_DIR
	if (SaveOrLoad(filename, SL_LOAD) != SL_OK) {
		snprintf(filename, lengthof(filename), "%sopntitle.dat",  _paths.second_data_dir);
	}
#endif
	if (SaveOrLoad(filename, SL_LOAD) != SL_OK) {
		GenerateWorld(GW_EMPTY, 64, 64); // if failed loading, make empty world.
		WaitTillGeneratedWorld();
	}

	_pause_game = 0;
	SetLocalPlayer(PLAYER_FIRST);
	/* Make sure you can't scroll in the menu */
	_scrolling_viewport = 0;
	_cursor.fix_at = false;
	MarkWholeScreenDirty();

	// Play main theme
	if (_music_driver->is_song_playing()) ResetMusic();
}

#if defined(UNIX) && !defined(__MORPHOS__)
extern void DedicatedFork();
#endif

int ttd_main(int argc, char *argv[])
{
	int i;
	const char *optformat;
	char musicdriver[32], sounddriver[32], videodriver[32];
	int resolution[2] = {0,0};
	Year startyear = INVALID_YEAR;
	uint generation_seed = GENERATE_NEW_SEED;
	bool save_config = true;
#if defined(ENABLE_NETWORK)
	bool dedicated = false;
	bool network   = false;
	char *network_conn = NULL;
	char *debuglog_conn = NULL;
	char *dedicated_host = NULL;
	uint16 dedicated_port = 0;
#endif /* ENABLE_NETWORK */

	musicdriver[0] = sounddriver[0] = videodriver[0] = '\0';

	_game_mode = GM_MENU;
	_switch_mode = SM_MENU;
	_switch_mode_errorstr = INVALID_STRING_ID;
	_dedicated_forks = false;
	_config_file = NULL;

	// The last param of the following function means this:
	//   a letter means: it accepts that param (e.g.: -h)
	//   a ':' behind it means: it need a param (e.g.: -m<driver>)
	//   a '::' behind it means: it can optional have a param (e.g.: -d<debug>)
	optformat = "m:s:v:hD::n::eit:d::r:g::G:c:xl:"
#if !defined(__MORPHOS__) && !defined(__AMIGA__) && !defined(WIN32)
		"f"
#endif
	;

	MyGetOptData mgo(argc-1, argv+1, optformat);

	while ((i = MyGetOpt(&mgo)) != -1) {
		switch (i) {
		case 'm': ttd_strlcpy(musicdriver, mgo.opt, sizeof(musicdriver)); break;
		case 's': ttd_strlcpy(sounddriver, mgo.opt, sizeof(sounddriver)); break;
		case 'v': ttd_strlcpy(videodriver, mgo.opt, sizeof(videodriver)); break;
#if defined(ENABLE_NETWORK)
		case 'D':
			strcpy(musicdriver, "null");
			strcpy(sounddriver, "null");
			strcpy(videodriver, "dedicated");
			dedicated = true;
			if (mgo.opt != NULL) {
				/* Use the existing method for parsing (openttd -n).
				 * However, we do ignore the #player part. */
				const char *temp = NULL;
				const char *port = NULL;
				ParseConnectionString(&temp, &port, mgo.opt);
				if (!StrEmpty(mgo.opt)) dedicated_host = mgo.opt;
				if (port != NULL) dedicated_port = atoi(port);
			}
			break;
		case 'f': _dedicated_forks = true; break;
		case 'n':
			network = true;
			network_conn = mgo.opt; // optional IP parameter, NULL if unset
			break;
		case 'l':
			debuglog_conn = mgo.opt;
			break;
#endif /* ENABLE_NETWORK */
		case 'r': ParseResolution(resolution, mgo.opt); break;
		case 't': startyear = atoi(mgo.opt); break;
		case 'd': {
#if defined(WIN32)
				CreateConsole();
#endif
				if (mgo.opt != NULL) SetDebugString(mgo.opt);
			} break;
		case 'e': _switch_mode = SM_EDITOR; break;
		case 'i': _use_dos_palette = true; break;
		case 'g':
			if (mgo.opt != NULL) {
				strcpy(_file_to_saveload.name, mgo.opt);
				_switch_mode = SM_LOAD;
			} else {
				_switch_mode = SM_NEWGAME;
			}
			break;
		case 'G': generation_seed = atoi(mgo.opt); break;
		case 'c': _config_file = strdup(mgo.opt); break;
		case 'x': save_config = false; break;
		case -2:
		case 'h':
			showhelp();
			return 0;
		}
	}

	DeterminePaths();
	CheckExternalFiles();

#if defined(UNIX) && !defined(__MORPHOS__)
	// We must fork here, or we'll end up without some resources we need (like sockets)
	if (_dedicated_forks)
		DedicatedFork();
#endif

	LoadFromConfig();
	CheckConfig();
	LoadFromHighScore();

	// override config?
	if (!StrEmpty(musicdriver)) ttd_strlcpy(_ini_musicdriver, musicdriver, sizeof(_ini_musicdriver));
	if (!StrEmpty(sounddriver)) ttd_strlcpy(_ini_sounddriver, sounddriver, sizeof(_ini_sounddriver));
	if (!StrEmpty(videodriver)) ttd_strlcpy(_ini_videodriver, videodriver, sizeof(_ini_videodriver));
	if (resolution[0] != 0) { _cur_resolution[0] = resolution[0]; _cur_resolution[1] = resolution[1]; }
	if (startyear != INVALID_YEAR) _patches_newgame.starting_year = startyear;
	if (generation_seed != GENERATE_NEW_SEED) _patches_newgame.generation_seed = generation_seed;

#if defined(ENABLE_NETWORK)
	if (dedicated_host) snprintf(_network_server_bind_ip_host, NETWORK_HOSTNAME_LENGTH, "%s", dedicated_host);
	if (dedicated_port) _network_server_port = dedicated_port;
	if (_dedicated_forks && !dedicated) _dedicated_forks = false;
#endif /* ENABLE_NETWORK */

	// enumerate language files
	InitializeLanguagePacks();

	// initialize screenshot formats
	InitializeScreenshotFormats();

	// initialize airport state machines
	InitializeAirports();

	/* initialize all variables that are allocated dynamically */
	InitializeDynamicVariables();

	/* start the AI */
	AI_Initialize();

	// Sample catalogue
	DEBUG(misc, 1, "Loading sound effects...");
	MxInitialize(11025);
	SoundInitialize("sample.cat");

	/* Initialize FreeType */
	InitFreeType();

	// This must be done early, since functions use the InvalidateWindow* calls
	InitWindowSystem();

	/* Initialize game palette */
	GfxInitPalettes();

	DEBUG(driver, 1, "Loading drivers...");
	LoadDriver(SOUND_DRIVER, _ini_sounddriver);
	LoadDriver(MUSIC_DRIVER, _ini_musicdriver);
	LoadDriver(VIDEO_DRIVER, _ini_videodriver); // load video last, to prevent an empty window while sound and music loads
	_savegame_sort_order = SORT_BY_DATE | SORT_DESCENDING;

	// restore saved music volume
	_music_driver->set_volume(msf.music_vol);

	NetworkStartUp(); // initialize network-core

#if defined(ENABLE_NETWORK)
	if (debuglog_conn != NULL && _network_available) {
		const char *not_used = NULL;
		const char *port = NULL;
		uint16 rport;

		rport = NETWORK_DEFAULT_DEBUGLOG_PORT;

		ParseConnectionString(&not_used, &port, debuglog_conn);
		if (port != NULL) rport = atoi(port);

		NetworkStartDebugLog(debuglog_conn, rport);
	}
#endif /* ENABLE_NETWORK */

	ScanNewGRFFiles();

	_opt_ptr = &_opt_newgame;
	ResetGRFConfig(false);

	/* XXX - ugly hack, if diff_level is 9, it means we got no setting from the config file */
	if (_opt_newgame.diff_level == 9) SetDifficultyLevel(0, &_opt_newgame);

	/* Make sure _patches is filled with _patches_newgame if we switch to a game directly */
	if (_switch_mode != SM_NONE) {
		_opt = _opt_newgame;
		UpdatePatches();
	}

	// initialize the ingame console
	IConsoleInit();
	_cursor.in_window = true;
	InitializeGUI();
	IConsoleCmdExec("exec scripts/autoexec.scr 0");

	GenerateWorld(GW_EMPTY, 64, 64); // Make the viewport initialization happy
	WaitTillGeneratedWorld();

#ifdef ENABLE_NETWORK
	if (network && _network_available) {
		if (network_conn != NULL) {
			const char *port = NULL;
			const char *player = NULL;
			uint16 rport;

			rport = NETWORK_DEFAULT_PORT;
			_network_playas = PLAYER_NEW_COMPANY;

			ParseConnectionString(&player, &port, network_conn);

			if (player != NULL) {
				_network_playas = (PlayerID)atoi(player);

				if (_network_playas != PLAYER_SPECTATOR) {
					_network_playas--;
					if (!IsValidPlayer(_network_playas)) return false;
				}
			}
			if (port != NULL) rport = atoi(port);

			LoadIntroGame();
			_switch_mode = SM_NONE;
			NetworkClientConnectGame(network_conn, rport);
		}
	}
#endif /* ENABLE_NETWORK */

	_video_driver->main_loop();

	WaitTillSaved();
	IConsoleFree();

	if (_network_available) NetworkShutDown(); // Shut down the network and close any open connections

	_video_driver->stop();
	_music_driver->stop();
	_sound_driver->stop();

	/* only save config if we have to */
	if (save_config) {
		SaveToConfig();
		SaveToHighScore();
	}

	/* Reset windowing system and free config file */
	UnInitializeGame();

	/* stop the AI */
	AI_Uninitialize();

	/* Close all and any open filehandles */
	FioCloseAll();

	return 0;
}

void HandleExitGameRequest()
{
	if (_game_mode == GM_MENU) { // do not ask to quit on the main screen
		_exit_game = true;
	} else if (_patches.autosave_on_exit) {
		DoExitSave();
		_exit_game = true;
	} else {
		AskExitGame();
	}
}


/** Mutex so that only one thread can communicate with the main program
 * at any given time */
static ThreadMsg _message = MSG_OTTD_NO_MESSAGE;

static inline void OTTD_ReleaseMutex() {_message = MSG_OTTD_NO_MESSAGE;}
static inline ThreadMsg OTTD_PollThreadEvent() {return _message;}

/** Called by running thread to execute some action in the main game.
 * It will stall as long as the mutex is not freed (handled) by the game */
void OTTD_SendThreadMessage(ThreadMsg msg)
{
	if (_exit_game) return;
	while (_message != MSG_OTTD_NO_MESSAGE) CSleep(10);

	_message = msg;
}


/** Handle the user-messages sent to us
 * @param message message sent
 */
static void ProcessSentMessage(ThreadMsg message)
{
	switch (message) {
		case MSG_OTTD_SAVETHREAD_DONE:  SaveFileDone(); break;
		case MSG_OTTD_SAVETHREAD_ERROR: SaveFileError(); break;
		default: NOT_REACHED();
	}

	OTTD_ReleaseMutex(); // release mutex so that other threads, messages can be handled
}

static void ShowScreenshotResult(bool b)
{
	if (b) {
		SetDParamStr(0, _screenshot_name);
		ShowErrorMessage(INVALID_STRING_ID, STR_031B_SCREENSHOT_SUCCESSFULLY, 0, 0);
	} else {
		ShowErrorMessage(INVALID_STRING_ID, STR_031C_SCREENSHOT_FAILED, 0, 0);
	}

}

static void MakeNewGameDone()
{
	/* In a dedicated server, the server does not play */
	if (_network_dedicated) {
		SetLocalPlayer(PLAYER_SPECTATOR);
		return;
	}

	/* Create a single player */
	DoStartupNewPlayer(false);

	SetLocalPlayer(PLAYER_FIRST);
	_current_player = _local_player;
	DoCommandP(0, (_patches.autorenew << 15 ) | (_patches.autorenew_months << 16) | 4, _patches.autorenew_money, NULL, CMD_SET_AUTOREPLACE);

	SettingsDisableElrail(_patches.disable_elrails);

	MarkWholeScreenDirty();
}

static void MakeNewGame(bool from_heightmap)
{
	_game_mode = GM_NORMAL;

	ResetGRFConfig(true);

	GenerateWorldSetCallback(&MakeNewGameDone);
	GenerateWorld(from_heightmap ? GW_HEIGHTMAP : GW_NEWGAME, 1 << _patches.map_x, 1 << _patches.map_y);
}

static void MakeNewEditorWorldDone()
{
	SetLocalPlayer(OWNER_NONE);

	MarkWholeScreenDirty();
}

static void MakeNewEditorWorld()
{
	_game_mode = GM_EDITOR;

	ResetGRFConfig(true);

	GenerateWorldSetCallback(&MakeNewEditorWorldDone);
	GenerateWorld(GW_EMPTY, 1 << _patches.map_x, 1 << _patches.map_y);
}

void StartupPlayers();
void StartupDisasters();
extern void StartupEconomy();

/**
 * Start Scenario starts a new game based on a scenario.
 * Eg 'New Game' --> select a preset scenario
 * This starts a scenario based on your current difficulty settings
 */
static void StartScenario()
{
	_game_mode = GM_NORMAL;

	// invalid type
	if (_file_to_saveload.mode == SL_INVALID) {
		DEBUG(sl, 0, "Savegame is obsolete or invalid format: '%s'", _file_to_saveload.name);
		ShowErrorMessage(INVALID_STRING_ID, STR_4009_GAME_LOAD_FAILED, 0, 0);
		_game_mode = GM_MENU;
		return;
	}

	// Reinitialize windows
	ResetWindowSystem();

	SetupColorsAndInitialWindow();

	ResetGRFConfig(true);

	// Load game
	if (SaveOrLoad(_file_to_saveload.name, _file_to_saveload.mode) != SL_OK) {
		LoadIntroGame();
		ShowErrorMessage(INVALID_STRING_ID, STR_4009_GAME_LOAD_FAILED, 0, 0);
	}

	_opt_ptr = &_opt;
	_opt_ptr->diff = _opt_newgame.diff;
	_opt.diff_level = _opt_newgame.diff_level;

	// Inititalize data
	StartupEconomy();
	StartupPlayers();
	StartupEngines();
	StartupDisasters();

	SetLocalPlayer(PLAYER_FIRST);
	_current_player = _local_player;
	DoCommandP(0, (_patches.autorenew << 15 ) | (_patches.autorenew_months << 16) | 4, _patches.autorenew_money, NULL, CMD_SET_AUTOREPLACE);

	MarkWholeScreenDirty();
}

bool SafeSaveOrLoad(const char *filename, int mode, int newgm)
{
	byte ogm = _game_mode;

	_game_mode = newgm;
	switch (SaveOrLoad(filename, mode)) {
		case SL_OK: return true;

		case SL_REINIT:
			switch (ogm) {
				case GM_MENU:   LoadIntroGame();      break;
				case GM_EDITOR: MakeNewEditorWorld(); break;
				default:        MakeNewGame(false);   break;
			}
			return false;

		default:
			_game_mode = ogm;
			return false;
	}
}

void SwitchMode(int new_mode)
{
#ifdef ENABLE_NETWORK
	// If we are saving something, the network stays in his current state
	if (new_mode != SM_SAVE) {
		// If the network is active, make it not-active
		if (_networking) {
			if (_network_server && (new_mode == SM_LOAD || new_mode == SM_NEWGAME)) {
				NetworkReboot();
				NetworkUDPCloseAll();
			} else {
				NetworkDisconnect();
				NetworkUDPCloseAll();
			}
		}

		// If we are a server, we restart the server
		if (_is_network_server) {
			// But not if we are going to the menu
			if (new_mode != SM_MENU) {
				NetworkServerStart();
			} else {
				// This client no longer wants to be a network-server
				_is_network_server = false;
			}
		}
	}
#endif /* ENABLE_NETWORK */

	switch (new_mode) {
	case SM_EDITOR: /* Switch to scenario editor */
		MakeNewEditorWorld();
		break;

	case SM_NEWGAME: /* New Game --> 'Random game' */
#ifdef ENABLE_NETWORK
		if (_network_server) {
			snprintf(_network_game_info.map_name, lengthof(_network_game_info.map_name), "Random Map");
		}
#endif /* ENABLE_NETWORK */
		MakeNewGame(false);
		break;

	case SM_START_SCENARIO: /* New Game --> Choose one of the preset scenarios */
#ifdef ENABLE_NETWORK
		if (_network_server) {
			snprintf(_network_game_info.map_name, lengthof(_network_game_info.map_name), "%s (Loaded scenario)", _file_to_saveload.title);
		}
#endif /* ENABLE_NETWORK */
		StartScenario();
		break;

	case SM_LOAD: { /* Load game, Play Scenario */
		_opt_ptr = &_opt;
		ResetGRFConfig(true);

		if (!SafeSaveOrLoad(_file_to_saveload.name, _file_to_saveload.mode, GM_NORMAL)) {
			LoadIntroGame();
			ShowErrorMessage(INVALID_STRING_ID, STR_4009_GAME_LOAD_FAILED, 0, 0);
		} else {
			/* Update the local player for a loaded game. It is either always
			 * player #1 (eg 0) or in the case of a dedicated server a spectator */
			SetLocalPlayer(_network_dedicated ? PLAYER_SPECTATOR : PLAYER_FIRST);
			DoCommandP(0, 0, 0, NULL, CMD_PAUSE); // decrease pause counter (was increased from opening load dialog)
#ifdef ENABLE_NETWORK
			if (_network_server) {
				snprintf(_network_game_info.map_name, lengthof(_network_game_info.map_name), "%s (Loaded game)", _file_to_saveload.title);
			}
#endif /* ENABLE_NETWORK */
		}
		break;
	}

	case SM_START_HEIGHTMAP: /* Load a heightmap and start a new game from it */
#ifdef ENABLE_NETWORK
		if (_network_server) {
			snprintf(_network_game_info.map_name, lengthof(_network_game_info.map_name), "%s (Heightmap)", _file_to_saveload.title);
		}
#endif /* ENABLE_NETWORK */
		MakeNewGame(true);
		break;

	case SM_LOAD_HEIGHTMAP: /* Load heightmap from scenario editor */
		SetLocalPlayer(OWNER_NONE);

		GenerateWorld(GW_HEIGHTMAP, 1 << _patches.map_x, 1 << _patches.map_y);
		MarkWholeScreenDirty();
		break;

	case SM_LOAD_SCENARIO: { /* Load scenario from scenario editor */
		if (SafeSaveOrLoad(_file_to_saveload.name, _file_to_saveload.mode, GM_EDITOR)) {
			_opt_ptr = &_opt;

			SetLocalPlayer(OWNER_NONE);
			_patches_newgame.starting_year = _cur_year;
		} else {
			ShowErrorMessage(INVALID_STRING_ID, STR_4009_GAME_LOAD_FAILED, 0, 0);
		}
		break;
	}

	case SM_MENU: /* Switch to game intro menu */
		LoadIntroGame();
		break;

	case SM_SAVE: /* Save game */
		if (SaveOrLoad(_file_to_saveload.name, SL_SAVE) != SL_OK) {
			ShowErrorMessage(INVALID_STRING_ID, STR_4007_GAME_SAVE_FAILED, 0, 0);
		} else {
			DeleteWindowById(WC_SAVELOAD, 0);
		}
		break;

	case SM_GENRANDLAND: /* Generate random land within scenario editor */
		SetLocalPlayer(OWNER_NONE);
		GenerateWorld(GW_RANDOM, 1 << _patches.map_x, 1 << _patches.map_y);
		// XXX: set date
		MarkWholeScreenDirty();
		break;
	}

	if (_switch_mode_errorstr != INVALID_STRING_ID) {
		ShowErrorMessage(INVALID_STRING_ID, _switch_mode_errorstr, 0, 0);
	}
}


// State controlling game loop.
// The state must not be changed from anywhere
// but here.
// That check is enforced in DoCommand.
void StateGameLoop()
{
	// dont execute the state loop during pause
	if (_pause_game) return;
	if (IsGeneratingWorld()) return;

	if (_game_mode == GM_EDITOR) {
		RunTileLoop();
		CallVehicleTicks();
		CallLandscapeTick();
		CallWindowTickEvent();
		NewsLoop();
	} else {
		// All these actions has to be done from OWNER_NONE
		//  for multiplayer compatibility
		PlayerID p = _current_player;
		_current_player = OWNER_NONE;

		AnimateAnimatedTiles();
		IncreaseDate();
		RunTileLoop();
		CallVehicleTicks();
		CallLandscapeTick();

		AI_RunGameLoop();

		CallWindowTickEvent();
		NewsLoop();
		_current_player = p;
	}
}

static void DoAutosave()
{
	char buf[200];

#if defined(PSP)
	/* Autosaving in networking is too time expensive for the PSP */
	if (_networking)
		return;
#endif /* PSP */

	if (_patches.keep_all_autosave && _local_player != PLAYER_SPECTATOR) {
		const Player *p = GetPlayer(_local_player);
		char* s = buf;

		s += snprintf(buf, lengthof(buf), "%s%s", _paths.autosave_dir, PATHSEP);

		SetDParam(0, p->name_1);
		SetDParam(1, p->name_2);
		SetDParam(2, _date);
		s = GetString(s, STR_4004, lastof(buf));
		strecpy(s, ".sav", lastof(buf));
	} else { /* generate a savegame name and number according to _patches.max_num_autosaves */
		snprintf(buf, lengthof(buf), "%s%sautosave%d.sav", _paths.autosave_dir, PATHSEP, _autosave_ctr);

		_autosave_ctr++;
		if (_autosave_ctr >= _patches.max_num_autosaves) {
			// we reached the limit for numbers of autosaves. We will start over
			_autosave_ctr = 0;
		}
	}

	DEBUG(sl, 2, "Autosaving to '%s'", buf);
	if (SaveOrLoad(buf, SL_SAVE) != SL_OK)
		ShowErrorMessage(INVALID_STRING_ID, STR_AUTOSAVE_FAILED, 0, 0);
}

static void ScrollMainViewport(int x, int y)
{
	if (_game_mode != GM_MENU) {
		Window *w = FindWindowById(WC_MAIN_WINDOW, 0);
		assert(w);

		WP(w,vp_d).scrollpos_x += x << w->viewport->zoom;
		WP(w,vp_d).scrollpos_y += y << w->viewport->zoom;
	}
}

static const int8 scrollamt[16][2] = {
	{ 0,  0},
	{-2,  0}, //  1 : left
	{ 0, -2}, //  2 : up
	{-2, -1}, //  3 : left + up
	{ 2,  0}, //  4 : right
	{ 0,  0}, //  5 : left + right
	{ 2, -1}, //  6 : right + up
	{ 0, -2}, //  7 : left + right + up = up
	{ 0  ,2}, //  8 : down
	{-2  ,1}, //  9 : down+left
	{ 0,  0}, // 10 : impossible
	{-2,  0}, // 11 : left + up + down = left
	{ 2,  1}, // 12 : down+right
	{ 0,  2}, // 13 : left + right + down = down
	{ 0, -2}, // 14 : left + right + up = up
	{ 0,  0}, // 15 : impossible
};

static void HandleKeyScrolling()
{
	if (_dirkeys && !_no_scroll) {
		int factor = _shift_pressed ? 50 : 10;
		ScrollMainViewport(scrollamt[_dirkeys][0] * factor, scrollamt[_dirkeys][1] * factor);
	}
}

void GameLoop()
{
	ThreadMsg message;

	if ((message = OTTD_PollThreadEvent()) != 0) ProcessSentMessage(message);

	// autosave game?
	if (_do_autosave) {
		_do_autosave = false;
		DoAutosave();
		RedrawAutosave();
	}

	// handle scrolling of the main window
	HandleKeyScrolling();

	// make a screenshot?
	if (IsScreenshotRequested()) ShowScreenshotResult(MakeScreenshot());

	// switch game mode?
	if (_switch_mode != SM_NONE) {
		SwitchMode(_switch_mode);
		_switch_mode = SM_NONE;
	}

	IncreaseSpriteLRU();
	InteractiveRandom();

	if (_scroller_click_timeout > 3) {
		_scroller_click_timeout -= 3;
	} else {
		_scroller_click_timeout = 0;
	}

	_caret_timer += 3;
	_timer_counter += 8;
	CursorTick();

#ifdef ENABLE_NETWORK
	// Check for UDP stuff
	if (_network_available) NetworkUDPGameLoop();

	if (_networking && !IsGeneratingWorld()) {
		// Multiplayer
		NetworkGameLoop();
	} else {
		if (_network_reconnect > 0 && --_network_reconnect == 0) {
			// This means that we want to reconnect to the last host
			// We do this here, because it means that the network is really closed
			NetworkClientConnectGame(_network_last_host, _network_last_port);
		}
		// Singleplayer
		StateGameLoop();
	}
#else
	StateGameLoop();
#endif /* ENABLE_NETWORK */

	if (!_pause_game && _display_opt & DO_FULL_ANIMATION) DoPaletteAnimations();

	if (!_pause_game || _cheats.build_in_pause.value) MoveAllTextEffects();

	InputLoop();

	MusicLoop();
}

void BeforeSaveGame()
{
	const Window *w = FindWindowById(WC_MAIN_WINDOW, 0);

	if (w != NULL) {
		_saved_scrollpos_x = WP(w, const vp_d).scrollpos_x;
		_saved_scrollpos_y = WP(w, const vp_d).scrollpos_y;
		_saved_scrollpos_zoom = w->viewport->zoom;
	}
}

static void ConvertTownOwner()
{
	TileIndex tile;

	for (tile = 0; tile != MapSize(); tile++) {
		switch (GetTileType(tile)) {
			case MP_STREET:
				if (IsLevelCrossing(tile) && GetCrossingRoadOwner(tile) & 0x80) {
					SetCrossingRoadOwner(tile, OWNER_TOWN);
				}
				/* FALLTHROUGH */

			case MP_TUNNELBRIDGE:
				if (GetTileOwner(tile) & 0x80) SetTileOwner(tile, OWNER_TOWN);
				break;

			default: break;
		}
	}
}

// before savegame version 4, the name of the company determined if it existed
static void CheckIsPlayerActive()
{
	Player *p;

	FOR_ALL_PLAYERS(p) {
		if (p->name_1 != 0) p->is_active = true;
	}
}

// since savegame version 4.1, exclusive transport rights are stored at towns
static void UpdateExclusiveRights()
{
	Town *t;

	FOR_ALL_TOWNS(t) {
		t->exclusivity = INVALID_PLAYER;
	}

	/* FIXME old exclusive rights status is not being imported (stored in s->blocked_months_obsolete)
	 *   could be implemented this way:
	 * 1.) Go through all stations
	 *     Build an array town_blocked[ town_id ][ player_id ]
	 *     that stores if at least one station in that town is blocked for a player
	 * 2.) Go through that array, if you find a town that is not blocked for
	 *     one player, but for all others, then give him exclusivity.
	 */
}

static const byte convert_currency[] = {
	 0,  1, 12,  8,  3,
	10, 14, 19,  4,  5,
	 9, 11, 13,  6, 17,
	16, 22, 21,  7, 15,
	18,  2, 20, };

// since savegame version 4.2 the currencies are arranged differently
static void UpdateCurrencies()
{
	_opt.currency = convert_currency[_opt.currency];
}

/* Up to revision 1413 the invisible tiles at the southern border have not been
 * MP_VOID, even though they should have. This is fixed by this function
 */
static void UpdateVoidTiles()
{
	uint i;

	for (i = 0; i < MapMaxY(); ++i) MakeVoid(i * MapSizeX() + MapMaxX());
	for (i = 0; i < MapSizeX(); ++i) MakeVoid(MapSizeX() * MapMaxY() + i);
}

// since savegame version 6.0 each sign has an "owner", signs without owner (from old games are set to 255)
static void UpdateSignOwner()
{
	Sign *si;

	FOR_ALL_SIGNS(si) si->owner = OWNER_NONE;
}

extern void UpdateOldAircraft();


static inline RailType UpdateRailType(RailType rt, RailType min)
{
	return rt >= min ? (RailType)(rt + 1): rt;
}

bool AfterLoadGame()
{
	TileIndex map_size = MapSize();
	Window *w;
	ViewPort *vp;
	Player *p;

	// in version 2.1 of the savegame, town owner was unified.
	if (CheckSavegameVersionOldStyle(2, 1)) ConvertTownOwner();

	// from version 4.1 of the savegame, exclusive rights are stored at towns
	if (CheckSavegameVersionOldStyle(4, 1)) UpdateExclusiveRights();

	// from version 4.2 of the savegame, currencies are in a different order
	if (CheckSavegameVersionOldStyle(4, 2)) UpdateCurrencies();

	// from version 6.1 of the savegame, signs have an "owner"
	if (CheckSavegameVersionOldStyle(6, 1)) UpdateSignOwner();

	/* In old version there seems to be a problem that water is owned by
	    OWNER_NONE, not OWNER_WATER.. I can't replicate it for the current
	    (4.3) version, so I just check when versions are older, and then
	    walk through the whole map.. */
	if (CheckSavegameVersionOldStyle(4, 3)) {
		for (TileIndex t = 0; t < map_size; t++) {
			if (IsTileType(t, MP_WATER) && GetTileOwner(t) >= MAX_PLAYERS) {
				SetTileOwner(t, OWNER_WATER);
			}
		}
	}

	// convert road side to my format.
	if (_opt.road_side) _opt.road_side = 1;

	/* Check if all NewGRFs are present, we are very strict in MP mode */
	GRFListCompatibility gcf_res = IsGoodGRFConfigList();
	if (_networking && gcf_res != GLC_ALL_GOOD) return false;

	switch (gcf_res) {
		case GLC_COMPATIBLE: _switch_mode_errorstr = STR_NEWGRF_COMPATIBLE_LOAD_WARNING; break;
		case GLC_NOT_FOUND: _switch_mode_errorstr = STR_NEWGRF_DISABLED_WARNING; break;
		default: break;
	}

	/* Update current year
	 * must be done before loading sprites as some newgrfs check it */
	SetDate(_date);

	// Load the sprites
	GfxLoadSprites();
	LoadStringWidthTable();

	/* Connect front and rear engines of multiheaded trains and converts
	 * subtype to the new format */
	if (CheckSavegameVersionOldStyle(17, 1)) ConvertOldMultiheadToNew();

	/* Connect front and rear engines of multiheaded trains */
	ConnectMultiheadedTrains();

	// reinit the landscape variables (landscape might have changed)
	InitializeLandscapeVariables(true);

	// Update all vehicles
	AfterLoadVehicles();

	// Update all waypoints
	if (CheckSavegameVersion(12)) FixOldWaypoints();

	UpdateAllWaypointSigns();

	// in version 2.2 of the savegame, we have new airports
	if (CheckSavegameVersionOldStyle(2, 2)) UpdateOldAircraft();

	UpdateAllStationVirtCoord();

	// Setup town coords
	AfterLoadTown();
	UpdateAllSignVirtCoords();

	// make sure there is a town in the game
	if (_game_mode == GM_NORMAL && !ClosestTownFromTile(0, (uint)-1)) {
		_error_message = STR_NO_TOWN_IN_SCENARIO;
		return false;
	}

	// Initialize windows
	ResetWindowSystem();
	SetupColorsAndInitialWindow();

	w = FindWindowById(WC_MAIN_WINDOW, 0);

	WP(w,vp_d).scrollpos_x = _saved_scrollpos_x;
	WP(w,vp_d).scrollpos_y = _saved_scrollpos_y;

	vp = w->viewport;
	vp->zoom = _saved_scrollpos_zoom;
	vp->virtual_width = vp->width << vp->zoom;
	vp->virtual_height = vp->height << vp->zoom;

	// in version 4.1 of the savegame, is_active was introduced to determine
	// if a player does exist, rather then checking name_1
	if (CheckSavegameVersionOldStyle(4, 1)) CheckIsPlayerActive();

	// the void tiles on the southern border used to belong to a wrong class (pre 4.3).
	if (CheckSavegameVersionOldStyle(4, 3)) UpdateVoidTiles();

	// If Load Scenario / New (Scenario) Game is used,
	//  a player does not exist yet. So create one here.
	// 1 exeption: network-games. Those can have 0 players
	//   But this exeption is not true for network_servers!
	if (!_players[0].is_active && (!_networking || (_networking && _network_server)))
		DoStartupNewPlayer(false);

	DoZoomInOutWindow(ZOOM_NONE, w); // update button status
	MarkWholeScreenDirty();

	for (TileIndex t = 0; t < map_size; t++) {
		switch (GetTileType(t)) {
			case MP_STATION: {
				Station *st = GetStationByTile(t);

				st->rect.BeforeAddTile(t, StationRect::ADD_FORCE);

				switch (GetStationType(t)) {
					case STATION_TRUCK:
					case STATION_BUS:
						if (CheckSavegameVersion(6)) {
							/* From this version on there can be multiple road stops of the
							 * same type per station. Convert the existing stops to the new
							 * internal data structure. */
							RoadStop *rs = new RoadStop(t);
							if (rs == NULL) error("Too many road stops in savegame");

							RoadStop **head =
								IsTruckStop(t) ? &st->truck_stops : &st->bus_stops;
							*head = rs;
						}
						break;

					case STATION_OILRIG: {
						/* Very old savegames sometimes have phantom oil rigs, i.e.
						 * an oil rig which got shut down, but not completly removed from
						 * the map
						 */
						TileIndex t1 = TILE_ADDXY(t, 1, 0);
						if (IsTileType(t1, MP_INDUSTRY) &&
								GetIndustryGfx(t1) == GFX_OILRIG_3) {
							/* The internal encoding of oil rigs was changed twice.
							 * It was 3 (till 2.2) and later 5 (till 5.1).
							 * Setting it unconditionally does not hurt.
							 */
							GetStationByTile(t)->airport_type = AT_OILRIG;
						} else {
							DeleteOilRig(t);
						}
						break;
					}

					default: break;
				}
				break;
			}

			default: break;
		}
	}

	/* In version 6.1 we put the town index in the map-array. To do this, we need
	 *  to use m2 (16bit big), so we need to clean m2, and that is where this is
	 *  all about ;) */
	if (CheckSavegameVersionOldStyle(6, 1)) {
		for (TileIndex t = 0; t < map_size; t++) {
			switch (GetTileType(t)) {
				case MP_HOUSE:
					_m[t].m4 = _m[t].m2;
					SetTownIndex(t, CalcClosestTownFromTile(t, (uint)-1)->index);
					break;

				case MP_STREET:
					_m[t].m4 |= (_m[t].m2 << 4);
					if (IsTileOwner(t, OWNER_TOWN)) {
						SetTownIndex(t, CalcClosestTownFromTile(t, (uint)-1)->index);
					} else {
						SetTownIndex(t, 0);
					}
					break;

				default: break;
			}
		}
	}

	/* From version 9.0, we update the max passengers of a town (was sometimes negative
	 *  before that. */
	if (CheckSavegameVersion(9)) {
		Town *t;
		FOR_ALL_TOWNS(t) UpdateTownMaxPass(t);
	}

	/* From version 16.0, we included autorenew on engines, which are now saved, but
	 *  of course, we do need to initialize them for older savegames. */
	if (CheckSavegameVersion(16)) {
		FOR_ALL_PLAYERS(p) {
			p->engine_renew_list   = NULL;
			p->engine_renew        = false;
			p->engine_renew_months = -6;
			p->engine_renew_money  = 100000;
		}

		/* When loading a game, _local_player is not yet set to the correct value.
		 * However, in a dedicated server we are a spectator, so nothing needs to
		 * happen. In case we are not a dedicated server, the local player always
		 * becomes player 0, unless we are in the scenario editor where all the
		 * players are 'invalid'.
		 */
		if (!_network_dedicated && IsValidPlayer(PLAYER_FIRST)) {
			p = GetPlayer(PLAYER_FIRST);
			p->engine_renew        = _patches.autorenew;
			p->engine_renew_months = _patches.autorenew_months;
			p->engine_renew_money  = _patches.autorenew_money;
		}
	}

	if (CheckSavegameVersion(42)) {
		Vehicle* v;

		for (TileIndex t = 0; t < map_size; t++) {
			if (MayHaveBridgeAbove(t)) ClearBridgeMiddle(t);
			if (IsBridgeTile(t)) {
				if (HASBIT(_m[t].m5, 6)) { // middle part
					Axis axis = (Axis)GB(_m[t].m5, 0, 1);

					if (HASBIT(_m[t].m5, 5)) { // transport route under bridge?
						if (GB(_m[t].m5, 3, 2) == TRANSPORT_RAIL) {
							MakeRailNormal(
								t,
								GetTileOwner(t),
								axis == AXIS_X ? TRACK_BIT_Y : TRACK_BIT_X,
								GetRailType(t)
							);
						} else {
							TownID town = IsTileOwner(t, OWNER_TOWN) ? ClosestTownFromTile(t, (uint)-1)->index : 0;

							MakeRoadNormal(
								t,
								GetTileOwner(t),
								axis == AXIS_X ? ROAD_Y : ROAD_X,
								town
							);
						}
					} else {
						if (GB(_m[t].m5, 3, 2) == 0) {
							MakeClear(t, CLEAR_GRASS, 3);
						} else {
							MakeCanal(t, GetTileOwner(t));
						}
					}
					SetBridgeMiddle(t, axis);
				} else { // ramp
					Axis axis = (Axis)GB(_m[t].m5, 0, 1);
					uint north_south = GB(_m[t].m5, 5, 1);
					DiagDirection dir = ReverseDiagDir(XYNSToDiagDir(axis, north_south));
					TransportType type = (TransportType)GB(_m[t].m5, 1, 2);

					_m[t].m5 = 1 << 7 | type << 2 | dir;
				}
			}
		}

		FOR_ALL_VEHICLES(v) {
			if (v->type != VEH_Train && v->type != VEH_Road) continue;
			if (IsBridgeTile(v->tile)) {
				DiagDirection dir = GetBridgeRampDirection(v->tile);

				if (dir != DirToDiagDir(v->direction)) continue;
				switch (dir) {
					default: NOT_REACHED();
					case DIAGDIR_NE: if ((v->x_pos & 0xF) !=  0)            continue; break;
					case DIAGDIR_SE: if ((v->y_pos & 0xF) != TILE_SIZE - 1) continue; break;
					case DIAGDIR_SW: if ((v->x_pos & 0xF) != TILE_SIZE - 1) continue; break;
					case DIAGDIR_NW: if ((v->y_pos & 0xF) !=  0)            continue; break;
				}
			} else if (v->z_pos > GetSlopeZ(v->x_pos, v->y_pos)) {
				v->tile = GetNorthernBridgeEnd(v->tile);
			} else {
				continue;
			}
			if (v->type == VEH_Train) {
				v->u.rail.track = TRACK_BIT_WORMHOLE;
			} else {
				v->u.road.state = RVSB_WORMHOLE;
			}
		}
	}

	if (CheckSavegameVersion(48)) {
		for (TileIndex t = 0; t < map_size; t++) {
			switch (GetTileType(t)) {
				case MP_RAILWAY:
					if (IsPlainRailTile(t)) {
						/* Swap ground type and signal type for plain rail tiles, so the
						 * ground type uses the same bits as for depots and waypoints. */
						uint tmp = GB(_m[t].m4, 0, 4);
						SB(_m[t].m4, 0, 4, GB(_m[t].m2, 0, 4));
						SB(_m[t].m2, 0, 4, tmp);
					} else if (HASBIT(_m[t].m5, 2)) {
						/* Split waypoint and depot rail type and remove the subtype. */
						CLRBIT(_m[t].m5, 2);
						CLRBIT(_m[t].m5, 6);
					}
					break;

				case MP_STREET:
					/* Swap m3 and m4, so the track type for rail crossings is the
					 * same as for normal rail. */
					Swap(_m[t].m3, _m[t].m4);
					break;

				default: break;
			}
		}
	}

	/* Elrails got added in rev 24 */
	if (CheckSavegameVersion(24)) {
		Vehicle *v;
		RailType min_rail = RAILTYPE_ELECTRIC;

		FOR_ALL_VEHICLES(v) {
			if (v->type == VEH_Train) {
				RailType rt = RailVehInfo(v->engine_type)->railtype;

				v->u.rail.railtype = rt;
				if (rt == RAILTYPE_ELECTRIC) min_rail = RAILTYPE_RAIL;
			}
		}

		/* .. so we convert the entire map from normal to elrail (so maintain "fairness") */
		for (TileIndex t = 0; t < map_size; t++) {
			switch (GetTileType(t)) {
				case MP_RAILWAY:
					SetRailType(t, UpdateRailType(GetRailType(t), min_rail));
					break;

				case MP_STREET:
					if (IsLevelCrossing(t)) {
						SetRailType(t, UpdateRailType(GetRailType(t), min_rail));
					}
					break;

				case MP_STATION:
					if (IsRailwayStation(t)) {
						SetRailType(t, UpdateRailType(GetRailType(t), min_rail));
					}
					break;

				case MP_TUNNELBRIDGE:
					if (IsTunnel(t)) {
						if (GetTunnelTransportType(t) == TRANSPORT_RAIL) {
							SetRailType(t, UpdateRailType(GetRailType(t), min_rail));
						}
					} else {
						if (GetBridgeTransportType(t) == TRANSPORT_RAIL) {
							SetRailType(t, UpdateRailType(GetRailType(t), min_rail));
						}
					}
					break;

				default:
					break;
			}
		}

		FOR_ALL_VEHICLES(v) {
			if (v->type == VEH_Train && (IsFrontEngine(v) || IsFreeWagon(v))) TrainConsistChanged(v);
		}

	}

	/* In version 16.1 of the savegame a player can decide if trains, which get
	 * replaced, shall keep their old length. In all prior versions, just default
	 * to false */
	if (CheckSavegameVersionOldStyle(16, 1)) {
		FOR_ALL_PLAYERS(p) p->renew_keep_length = false;
	}

	/* In version 17, ground type is moved from m2 to m4 for depots and
	 * waypoints to make way for storing the index in m2. The custom graphics
	 * id which was stored in m4 is now saved as a grf/id reference in the
	 * waypoint struct. */
	if (CheckSavegameVersion(17)) {
		Waypoint *wp;

		FOR_ALL_WAYPOINTS(wp) {
			if (wp->deleted == 0) {
				const StationSpec *statspec = NULL;

				if (HASBIT(_m[wp->xy].m3, 4))
					statspec = GetCustomStationSpec(STAT_CLASS_WAYP, _m[wp->xy].m4 + 1);

				if (statspec != NULL) {
					wp->stat_id = _m[wp->xy].m4 + 1;
					wp->grfid = statspec->grfid;
					wp->localidx = statspec->localidx;
				} else {
					// No custom graphics set, so set to default.
					wp->stat_id = 0;
					wp->grfid = 0;
					wp->localidx = 0;
				}

				// Move ground type bits from m2 to m4.
				_m[wp->xy].m4 = GB(_m[wp->xy].m2, 0, 4);
				// Store waypoint index in the tile.
				_m[wp->xy].m2 = wp->index;
			}
		}
	} else {
		/* As of version 17, we recalculate the custom graphic ID of waypoints
		 * from the GRF ID / station index. */
		AfterLoadWaypoints();
	}

	/* From version 15, we moved a semaphore bit from bit 2 to bit 3 in m4, making
	 *  room for PBS. Now in version 21 move it back :P. */
	if (CheckSavegameVersion(21) && !CheckSavegameVersion(15)) {
		for (TileIndex t = 0; t < map_size; t++) {
			switch (GetTileType(t)) {
				case MP_RAILWAY:
					if (HasSignals(t)) {
						// convert PBS signals to combo-signals
						if (HASBIT(_m[t].m2, 2)) SetSignalType(t, SIGTYPE_COMBO);

						// move the signal variant back
						SetSignalVariant(t, HASBIT(_m[t].m2, 3) ? SIG_SEMAPHORE : SIG_ELECTRIC);
						CLRBIT(_m[t].m2, 3);
					}

					// Clear PBS reservation on track
					if (!IsTileDepotType(t, TRANSPORT_RAIL)) {
						SB(_m[t].m4, 4, 4, 0);
					} else {
						CLRBIT(_m[t].m3, 6);
					}
					break;

				case MP_STREET:
					// Clear PBS reservation on crossing
					if (IsLevelCrossing(t)) CLRBIT(_m[t].m5, 0);
					break;

				case MP_STATION:
					// Clear PBS reservation on station
					CLRBIT(_m[t].m3, 6);
					break;

				default: break;
			}
		}
	}

	if (CheckSavegameVersion(22))  UpdatePatches();

	if (CheckSavegameVersion(25)) {
		Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			if (v->type == VEH_Road) {
				v->vehstatus &= ~0x40;
				v->u.road.slot = NULL;
				v->u.road.slot_age = 0;
			}
		}
	} else {
		Vehicle *v;
		FOR_ALL_VEHICLES(v) {
			if (v->type == VEH_Road && v->u.road.slot != NULL) v->u.road.slot->num_vehicles++;
		}
	}

	if (CheckSavegameVersion(26)) {
		Station *st;
		FOR_ALL_STATIONS(st) {
			st->last_vehicle_type = VEH_Invalid;
		}
	}

	YapfNotifyTrackLayoutChange(INVALID_TILE, INVALID_TRACK);

	if (CheckSavegameVersion(34)) FOR_ALL_PLAYERS(p) ResetPlayerLivery(p);

	FOR_ALL_PLAYERS(p) p->avail_railtypes = GetPlayerRailtypes(p->index);

	if (!CheckSavegameVersion(27)) AfterLoadStations();

	{
		/* Set up the engine count for all players */
		Player *players[MAX_PLAYERS];
		const Vehicle *v;

		for (PlayerID i = PLAYER_FIRST; i < MAX_PLAYERS; i++) players[i] = GetPlayer(i);

		FOR_ALL_VEHICLES(v) {
			if (!IsEngineCountable(v)) continue;
			players[v->owner]->num_engines[v->engine_type]++;
		}
	}

	/* Time starts at 0 instead of 1920.
	 * Account for this in older games by adding an offset */
	if (CheckSavegameVersion(31)) {
		Station *st;
		Waypoint *wp;
		Engine *e;
		Player *player;
		Industry *i;
		Vehicle *v;

		_date += DAYS_TILL_ORIGINAL_BASE_YEAR;
		_cur_year += ORIGINAL_BASE_YEAR;

		FOR_ALL_STATIONS(st)    st->build_date += DAYS_TILL_ORIGINAL_BASE_YEAR;
		FOR_ALL_WAYPOINTS(wp)   wp->build_date += DAYS_TILL_ORIGINAL_BASE_YEAR;
		FOR_ALL_ENGINES(e)      e->intro_date  += DAYS_TILL_ORIGINAL_BASE_YEAR;
		FOR_ALL_PLAYERS(player) player->inaugurated_year += ORIGINAL_BASE_YEAR;
		FOR_ALL_INDUSTRIES(i)   i->last_prod_year        += ORIGINAL_BASE_YEAR;

		FOR_ALL_VEHICLES(v) {
			v->date_of_last_service += DAYS_TILL_ORIGINAL_BASE_YEAR;
			v->build_year += ORIGINAL_BASE_YEAR;
		}
	}

	/* From 32 on we save the industry who made the farmland.
	 *  To give this prettyness to old savegames, we remove all farmfields and
	 *  plant new ones. */
	if (CheckSavegameVersion(32)) {
		Industry *i;

		for (TileIndex t = 0; t < map_size; t++) {
			if (IsTileType(t, MP_CLEAR) && IsClearGround(t, CLEAR_FIELDS)) {
				MakeClear(t, CLEAR_GRASS, 3);
			}
		}

		FOR_ALL_INDUSTRIES(i) {
			uint j;

			if (i->type == IT_FARM || i->type == IT_FARM_2) {
				for (j = 0; j != 50; j++) PlantRandomFarmField(i);
			}
		}
	}

	/* Setting no refit flags to all orders in savegames from before refit in orders were added */
	if (CheckSavegameVersion(36)) {
		Order *order;
		Vehicle *v;

		FOR_ALL_ORDERS(order) {
			order->refit_cargo   = CT_NO_REFIT;
			order->refit_subtype = CT_NO_REFIT;
		}

		FOR_ALL_VEHICLES(v) {
			v->current_order.refit_cargo   = CT_NO_REFIT;
			v->current_order.refit_subtype = CT_NO_REFIT;
		}
	}

	if (CheckSavegameVersion(37)) {
		ConvertNameArray();
	}

	/* from version 38 we have optional elrails, since we cannot know the
	 * preference of a user, let elrails enabled; it can be disabled manually */
	if (CheckSavegameVersion(38)) _patches.disable_elrails = false;
	/* do the same as when elrails were enabled/disabled manually just now */
	SettingsDisableElrail(_patches.disable_elrails);

	if (CheckSavegameVersion(43)) {
		for (TileIndex t = 0; t < map_size; t++) {
			if (IsTileType(t, MP_INDUSTRY)) {
				switch (GetIndustryGfx(t)) {
					case GFX_POWERPLANT_SPARKS:
						SetIndustryAnimationState(t, GB(_m[t].m1, 2, 5));
						break;

					case GFX_OILWELL_ANIMATED_1:
					case GFX_OILWELL_ANIMATED_2:
					case GFX_OILWELL_ANIMATED_3:
						SetIndustryAnimationState(t, GB(_m[t].m1, 0, 2));
						break;

					case GFX_COAL_MINE_TOWER_ANIMATED:
					case GFX_COPPER_MINE_TOWER_ANIMATED:
					case GFX_GOLD_MINE_TOWER_ANIMATED:
						 SetIndustryAnimationState(t, _m[t].m1);
						 break;

					default: /* No animation states to change */
						break;
				}
			}
		}
	}

	if (CheckSavegameVersion(44)) {
		Vehicle *v;
		/* If we remove a station while cargo from it is still enroute, payment calculation will assume
		 * 0, 0 to be the origin of the cargo, resulting in very high payments usually. v->cargo_source_xy
		 * stores the coordinates, preserving them even if the station is removed. However, if a game is loaded
		 * where this situation exists, the cargo-source information is lost. in this case, we set the origin
		 * to the current tile of the vehicle to prevent excessive profits
		 */
		FOR_ALL_VEHICLES(v) {
			v->cargo_source_xy = IsValidStationID(v->cargo_source) ? GetStation(v->cargo_source)->xy : v->tile;
		}
	}

	if (CheckSavegameVersion(45)) {
		Vehicle *v;
		/* Originally just the fact that some cargo had been paid for was
		 * stored to stop people cheating and cashing in several times. This
		 * wasn't enough though as it was cleared when the vehicle started
		 * loading again, even if it didn't actually load anything, so now the
		 * amount of cargo that has been paid for is stored. */
		FOR_ALL_VEHICLES(v) {
			if (HASBIT(v->vehicle_flags, 2)) {
				v->cargo_paid_for = v->cargo_count;
				CLRBIT(v->vehicle_flags, 2);
			} else {
				v->cargo_paid_for = 0;
			}
		}
	}

	/* Buoys do now store the owner of the previous water tile, which can never
	 * be OWNER_NONE. So replace OWNER_NONE with OWNER_WATER. */
	if (CheckSavegameVersion(46)) {
		Station *st;
		FOR_ALL_STATIONS(st) {
			if (st->IsBuoy() && IsTileOwner(st->xy, OWNER_NONE)) SetTileOwner(st->xy, OWNER_WATER);
		}
	}

	if (CheckSavegameVersion(50)) {
		Vehicle *v;
		/* Aircraft units changed from 8 mph to 1 km/h */
		FOR_ALL_VEHICLES(v) {
			if (v->type == VEH_Aircraft && v->subtype <= AIR_AIRCRAFT) {
				const AircraftVehicleInfo *avi = AircraftVehInfo(v->engine_type);
				v->cur_speed *= 129;
				v->cur_speed /= 10;
				v->max_speed = avi->max_speed;
				v->acceleration = avi->acceleration;
			}
		}
	}

	if (CheckSavegameVersion(49)) FOR_ALL_PLAYERS(p) p->face = ConvertFromOldPlayerFace(p->face);

	if (CheckSavegameVersion(52)) {
		for (TileIndex t = 0; t < map_size; t++) {
			if (IsStatueTile(t)) {
				_m[t].m2 = CalcClosestTownFromTile(t, (uint)-1)->index;
			}
		}
	}

	return true;
}

/** Reload all NewGRF files during a running game. This is a cut-down
 * version of AfterLoadGame().
 * XXX - We need to reset the vehicle position hash because with a non-empty
 * hash AfterLoadVehicles() will loop infinitely. We need AfterLoadVehicles()
 * to recalculate vehicle data as some NewGRF vehicle sets could have been
 * removed or added and changed statistics */
void ReloadNewGRFData()
{
	/* reload grf data */
	GfxLoadSprites();
	LoadStringWidthTable();
	/* reload vehicles */
	ResetVehiclePosHash();
	AfterLoadVehicles();
	StartupEngines();
	/* update station and waypoint graphics */
	AfterLoadWaypoints();
	AfterLoadStations();
	/* redraw the whole screen */
	MarkWholeScreenDirty();
}

HalMusicDriver *_music_driver;
HalSoundDriver *_sound_driver;
HalVideoDriver *_video_driver;

