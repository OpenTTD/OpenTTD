/* $Id$ */

/** @file openttd.cpp Functions related to starting OpenTTD. */

#include "stdafx.h"

#define VARDEF
#include "variables.h"
#undef VARDEF

#include "openttd.h"

#include "blitter/factory.hpp"
#include "sound/sound_driver.hpp"
#include "music/music_driver.hpp"
#include "video/video_driver.hpp"

#include "fontcache.h"
#include "gfxinit.h"
#include "gui.h"
#include "mixer.h"
#include "sound_func.h"
#include "window_func.h"

#include "saveload/saveload.h"
#include "landscape.h"
#include "company_func.h"
#include "command_func.h"
#include "news_func.h"
#include "fileio_func.h"
#include "fios.h"
#include "aircraft.h"
#include "console_func.h"
#include "screenshot.h"
#include "network/network.h"
#include "network/network_func.h"
#include "signs_base.h"
#include "ai/ai.hpp"
#include "ai/ai_config.hpp"
#include "settings_func.h"
#include "genworld.h"
#include "group.h"
#include "strings_func.h"
#include "date_func.h"
#include "vehicle_func.h"
#include "gamelog.h"
#include "cheat_type.h"
#include "animated_tile_func.h"
#include "functions.h"
#include "elrail_func.h"
#include "rev.h"
#include "highscore.h"

#include "newgrf_commons.h"

#include "town.h"
#include "industry.h"

#include <stdarg.h>

#include "table/strings.h"

StringID _switch_mode_errorstr;

void CallLandscapeTick();
void IncreaseDate();
void DoPaletteAnimations();
void MusicLoop();
void ResetMusic();
void ProcessAsyncSaveFinish();
void CallWindowTickEvent();

extern void SetDifficultyLevel(int mode, DifficultySettings *gm_opt);
extern Company *DoStartupNewCompany(bool is_ai);
extern void ShowOSErrorBox(const char *buf, bool system);
extern void InitializeRailGUI();

/**
 * Error handling for fatal user errors.
 * @param s the string to print.
 * @note Does NEVER return.
 */
void CDECL usererror(const char *s, ...)
{
	va_list va;
	char buf[512];

	va_start(va, s);
	vsnprintf(buf, lengthof(buf), s, va);
	va_end(va);

	ShowOSErrorBox(buf, false);
	if (_video_driver != NULL) _video_driver->Stop();

	exit(1);
}

/**
 * Error handling for fatal non-user errors.
 * @param s the string to print.
 * @note Does NEVER return.
 */
void CDECL error(const char *s, ...)
{
	va_list va;
	char buf[512];

	va_start(va, s);
	vsnprintf(buf, lengthof(buf), s, va);
	va_end(va);

	ShowOSErrorBox(buf, true);
	if (_video_driver != NULL) _video_driver->Stop();

	assert(0);
	exit(1);
}

/**
 * Shows some information on the console/a popup box depending on the OS.
 * @param str the text to show.
 */
void CDECL ShowInfoF(const char *str, ...)
{
	va_list va;
	char buf[1024];
	va_start(va, str);
	vsnprintf(buf, lengthof(buf), str, va);
	va_end(va);
	ShowInfo(buf);
}

/**
 * Show the help message when someone passed a wrong parameter.
 */
static void ShowHelp()
{
	char buf[8192];
	char *p = buf;

	p += seprintf(p, lastof(buf), "OpenTTD %s\n", _openttd_revision);
	p = strecpy(p,
		"\n"
		"\n"
		"Command line options:\n"
		"  -v drv              = Set video driver (see below)\n"
		"  -s drv              = Set sound driver (see below) (param bufsize,hz)\n"
		"  -m drv              = Set music driver (see below)\n"
		"  -b drv              = Set the blitter to use (see below)\n"
		"  -a ai               = Force use of specific AI (see below)\n"
		"  -r res              = Set resolution (for instance 800x600)\n"
		"  -h                  = Display this help text\n"
		"  -t year             = Set starting year\n"
		"  -d [[fac=]lvl[,...]]= Debug mode\n"
		"  -e                  = Start Editor\n"
		"  -g [savegame]       = Start new/save game immediately\n"
		"  -G seed             = Set random seed\n"
#if defined(ENABLE_NETWORK)
		"  -n [ip:port#company]= Start networkgame\n"
		"  -D [ip][:port]      = Start dedicated server\n"
		"  -l ip[:port]        = Redirect DEBUG()\n"
#if !defined(__MORPHOS__) && !defined(__AMIGA__) && !defined(WIN32)
		"  -f                  = Fork into the background (dedicated only)\n"
#endif
#endif /* ENABLE_NETWORK */
		"  -i palette          = Force to use the DOS (0) or Windows (1) palette\n"
		"                          (defines default setting when adding newgrfs)\n"
		"                        Default value (2) lets OpenTTD use the palette\n"
		"                          specified in graphics set file (see below)\n"
		"  -I graphics_set     = Force the graphics set (see below)\n"
		"  -c config_file      = Use 'config_file' instead of 'openttd.cfg'\n"
		"  -x                  = Do not automatically save to config file on exit\n"
		"\n",
		lastof(buf)
	);

	/* List the graphics packs */
	p = GetGraphicsSetsList(p, lastof(buf));

	/* List the drivers */
	p = VideoDriverFactoryBase::GetDriversInfo(p, lastof(buf));

	/* List the blitters */
	p = BlitterFactoryBase::GetBlittersInfo(p, lastof(buf));

	/* We need to initialize the AI, so it finds the AIs */
	AI::Initialize();
	p = AI::GetConsoleList(p, lastof(buf));
	AI::Uninitialize(true);

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
				/* Found argument, try to locate it in options. */
				if (*s == ':' || (r = strchr(md->options, *s)) == NULL) {
					/* ERROR! */
					return -2;
				}
				if (r[1] == ':') {
					/* Item wants an argument. Check if the argument follows, or if it comes as a separate arg. */
					if (!*(t = s + 1)) {
						/* It comes as a separate arg. Check if out of args? */
						if (--md->numleft < 0 || *(t = *md->argv) == '-') {
							/* Check if item is optional? */
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
			/* This is currently not supported. */
			return -2;
		}
	}
}

/**
 * Extract the resolution from the given string and store
 * it in the 'res' parameter.
 * @param res variable to store the resolution in.
 * @param s   the string to decompose.
 */
static void ParseResolution(Dimension *res, const char *s)
{
	const char *t = strchr(s, 'x');
	if (t == NULL) {
		ShowInfoF("Invalid resolution '%s'", s);
		return;
	}

	res->width  = max(strtoul(s, NULL, 0), 64UL);
	res->height = max(strtoul(t + 1, NULL, 0), 64UL);
}

static void InitializeDynamicVariables()
{
	/* Dynamic stuff needs to be initialized somewhere... */
	_industry_mngr.ResetMapping();
	_industile_mngr.ResetMapping();
	_Company_pool.AddBlockToPool();
}


/** Unitializes drivers, frees allocated memory, cleans pools, ...
 * Generally, prepares the game for shutting down
 */
static void ShutdownGame()
{
	/* stop the AI */
	AI::Uninitialize(false);

	IConsoleFree();

	if (_network_available) NetworkShutDown(); // Shut down the network and close any open connections

	DriverFactoryBase::ShutdownDrivers();

	UnInitWindowSystem();

	/* Uninitialize airport state machines */
	UnInitializeAirports();

	/* Uninitialize variables that are allocated dynamically */
	GamelogReset();
	_Town_pool.CleanPool();
	_Industry_pool.CleanPool();
	_Station_pool.CleanPool();
	_Vehicle_pool.CleanPool();
	_Sign_pool.CleanPool();
	_Order_pool.CleanPool();
	_Group_pool.CleanPool();
	_CargoPacket_pool.CleanPool();
	_Engine_pool.CleanPool();
	_Company_pool.CleanPool();

	free(_config_file);

	/* Close all and any open filehandles */
	FioCloseAll();
}

static void LoadIntroGame()
{
	_game_mode = GM_MENU;

	ResetGRFConfig(false);

	/* Setup main window */
	ResetWindowSystem();
	SetupColoursAndInitialWindow();

	/* Load the default opening screen savegame */
	if (SaveOrLoad("opntitle.dat", SL_LOAD, DATA_DIR) != SL_OK) {
		GenerateWorld(GW_EMPTY, 64, 64); // if failed loading, make empty world.
		WaitTillGeneratedWorld();
		SetLocalCompany(COMPANY_SPECTATOR);
	} else {
		SetLocalCompany(COMPANY_FIRST);
	}

	_pause_game = 0;
	_cursor.fix_at = false;

	CheckForMissingGlyphsInLoadedLanguagePack();

	/* Play main theme */
	if (_music_driver->IsSongPlaying()) ResetMusic();
}

void MakeNewgameSettingsLive()
{
	for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
		if (_settings_game.ai_config[c] != NULL) {
			delete _settings_game.ai_config[c];
		}
	}

	_settings_game = _settings_newgame;

	for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
		_settings_game.ai_config[c] = NULL;
		if (_settings_newgame.ai_config[c] != NULL) {
			_settings_game.ai_config[c] = new AIConfig(_settings_newgame.ai_config[c]);
		}
	}
}

byte _savegame_sort_order;
#if defined(UNIX) && !defined(__MORPHOS__)
extern void DedicatedFork();
#endif

int ttd_main(int argc, char *argv[])
{
	int i;
	const char *optformat;
	char *musicdriver = NULL;
	char *sounddriver = NULL;
	char *videodriver = NULL;
	char *blitter = NULL;
	char *graphics_set = NULL;
	Dimension resolution = {0, 0};
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

	_game_mode = GM_MENU;
	_switch_mode = SM_MENU;
	_switch_mode_errorstr = INVALID_STRING_ID;
	_dedicated_forks = false;
	_config_file = NULL;

	/* The last param of the following function means this:
	 *   a letter means: it accepts that param (e.g.: -h)
	 *   a ':' behind it means: it need a param (e.g.: -m<driver>)
	 *   a '::' behind it means: it can optional have a param (e.g.: -d<debug>) */
	optformat = "m:s:v:b:hD::n::ei::I:t:d::r:g::G:c:xl:"
#if !defined(__MORPHOS__) && !defined(__AMIGA__) && !defined(WIN32)
		"f"
#endif
	;

	MyGetOptData mgo(argc - 1, argv + 1, optformat);

	while ((i = MyGetOpt(&mgo)) != -1) {
		switch (i) {
		case 'I': free(graphics_set); graphics_set = strdup(mgo.opt); break;
		case 'm': free(musicdriver); musicdriver = strdup(mgo.opt); break;
		case 's': free(sounddriver); sounddriver = strdup(mgo.opt); break;
		case 'v': free(videodriver); videodriver = strdup(mgo.opt); break;
		case 'b': free(blitter); blitter = strdup(mgo.opt); break;
#if defined(ENABLE_NETWORK)
		case 'D':
			free(musicdriver);
			free(sounddriver);
			free(videodriver);
			free(blitter);
			musicdriver = strdup("null");
			sounddriver = strdup("null");
			videodriver = strdup("dedicated");
			blitter = strdup("null");
			dedicated = true;
			if (mgo.opt != NULL) {
				/* Use the existing method for parsing (openttd -n).
				 * However, we do ignore the #company part. */
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
		case 'r': ParseResolution(&resolution, mgo.opt); break;
		case 't': startyear = atoi(mgo.opt); break;
		case 'd': {
#if defined(WIN32)
				CreateConsole();
#endif
				if (mgo.opt != NULL) SetDebugString(mgo.opt);
			} break;
		case 'e': _switch_mode = SM_EDITOR; break;
		case 'i':
			/* there is an argument, it is not empty, and it is exactly 1 char long */
			if (!StrEmpty(mgo.opt) && mgo.opt[1] == '\0') {
				_use_palette = (PaletteType)(mgo.opt[0] - '0');
				if (_use_palette <= MAX_PAL) break;
			}
			usererror("Valid value for '-i' is 0, 1 or 2");
		case 'g':
			if (mgo.opt != NULL) {
				strecpy(_file_to_saveload.name, mgo.opt, lastof(_file_to_saveload.name));
				_switch_mode = SM_LOAD;
				_file_to_saveload.mode = SL_LOAD;

				/* if the file doesn't exist or it is not a valid savegame, let the saveload code show an error */
				const char *t = strrchr(_file_to_saveload.name, '.');
				if (t != NULL) {
					FiosType ft = FiosGetSavegameListCallback(SLD_LOAD_GAME, _file_to_saveload.name, t, NULL, NULL);
					if (ft != FIOS_TYPE_INVALID) SetFiosType(ft);
				}

				break;
			}

			_switch_mode = SM_NEWGAME;
			/* Give a random map if no seed has been given */
			if (generation_seed == GENERATE_NEW_SEED) {
				generation_seed = InteractiveRandom();
			}
			break;
		case 'G': generation_seed = atoi(mgo.opt); break;
		case 'c': _config_file = strdup(mgo.opt); break;
		case 'x': save_config = false; break;
		case -2:
		case 'h':
			/* The next two functions are needed to list the graphics sets.
			 * We can't do them earlier because then we can't show it on
			 * the debug console as that hasn't been configured yet. */
			DeterminePaths(argv[0]);
			FindGraphicsSets();
			ShowHelp();
			return 0;
		}
	}

#if defined(WINCE) && defined(_DEBUG)
	/* Switch on debug lvl 4 for WinCE if Debug release, as you can't give params, and you most likely do want this information */
	SetDebugString("4");
#endif

	DeterminePaths(argv[0]);
	FindGraphicsSets();

#if defined(UNIX) && !defined(__MORPHOS__)
	/* We must fork here, or we'll end up without some resources we need (like sockets) */
	if (_dedicated_forks)
		DedicatedFork();
#endif

	AI::Initialize();
	LoadFromConfig();
	AI::Uninitialize(true);
	CheckConfig();
	LoadFromHighScore();

	if (resolution.width != 0) { _cur_resolution = resolution; }
	if (startyear != INVALID_YEAR) _settings_newgame.game_creation.starting_year = startyear;
	if (generation_seed != GENERATE_NEW_SEED) _settings_newgame.game_creation.generation_seed = generation_seed;

	/* The width and height must be at least 1 pixel, this
	 * way all internal drawing routines work correctly. */
	if (_cur_resolution.width  <= 0) _cur_resolution.width  = 1;
	if (_cur_resolution.height <= 0) _cur_resolution.height = 1;

#if defined(ENABLE_NETWORK)
	if (dedicated_host) snprintf(_settings_client.network.server_bind_ip, sizeof(_settings_client.network.server_bind_ip), "%s", dedicated_host);
	if (dedicated_port) _settings_client.network.server_port = dedicated_port;
	if (_dedicated_forks && !dedicated) _dedicated_forks = false;
#endif /* ENABLE_NETWORK */

	/* enumerate language files */
	InitializeLanguagePacks();

	/* initialize screenshot formats */
	InitializeScreenshotFormats();

	/* initialize airport state machines */
	InitializeAirports();

	/* initialize all variables that are allocated dynamically */
	InitializeDynamicVariables();

	/* Sample catalogue */
	DEBUG(misc, 1, "Loading sound effects...");
	MxInitialize(11025);
	SoundInitialize("sample.cat");

	/* Initialize FreeType */
	InitFreeType();

	/* This must be done early, since functions use the InvalidateWindow* calls */
	InitWindowSystem();

	if (graphics_set == NULL) graphics_set = _ini_graphics_set;
	if (!SetGraphicsSet(graphics_set)) {
		StrEmpty(graphics_set) ?
			usererror("Failed to find a graphics set. Please acquire a graphics set for OpenTTD.") :
			usererror("Failed to select requested graphics set '%s'", graphics_set);
	}

	/* Initialize game palette */
	GfxInitPalettes();

	DEBUG(misc, 1, "Loading blitter...");
	if (blitter == NULL) blitter = _ini_blitter;
	if (BlitterFactoryBase::SelectBlitter(blitter) == NULL)
		StrEmpty(blitter) ?
			usererror("Failed to autoprobe blitter") :
			usererror("Failed to select requested blitter '%s'; does it exist?", blitter);

	DEBUG(driver, 1, "Loading drivers...");

	if (sounddriver == NULL) sounddriver = _ini_sounddriver;
	_sound_driver = (SoundDriver*)SoundDriverFactoryBase::SelectDriver(sounddriver, Driver::DT_SOUND);
	if (_sound_driver == NULL) {
		StrEmpty(sounddriver) ?
			usererror("Failed to autoprobe sound driver") :
			usererror("Failed to select requested sound driver '%s'", sounddriver);
	}

	if (musicdriver == NULL) musicdriver = _ini_musicdriver;
	_music_driver = (MusicDriver*)MusicDriverFactoryBase::SelectDriver(musicdriver, Driver::DT_MUSIC);
	if (_music_driver == NULL) {
		StrEmpty(musicdriver) ?
			usererror("Failed to autoprobe music driver") :
			usererror("Failed to select requested music driver '%s'", musicdriver);
	}

	if (videodriver == NULL) videodriver = _ini_videodriver;
	_video_driver = (VideoDriver*)VideoDriverFactoryBase::SelectDriver(videodriver, Driver::DT_VIDEO);
	if (_video_driver == NULL) {
		StrEmpty(videodriver) ?
			usererror("Failed to autoprobe video driver") :
			usererror("Failed to select requested video driver '%s'", videodriver);
	}

	_savegame_sort_order = SORT_BY_DATE | SORT_DESCENDING;
	/* Initialize the zoom level of the screen to normal */
	_screen.zoom = ZOOM_LVL_NORMAL;

	/* restore saved music volume */
	_music_driver->SetVolume(msf.music_vol);

	NetworkStartUp(); // initialize network-core

#if defined(ENABLE_NETWORK)
	if (debuglog_conn != NULL && _network_available) {
		const char *not_used = NULL;
		const char *port = NULL;
		uint16 rport;

		rport = NETWORK_DEFAULT_DEBUGLOG_PORT;

		ParseConnectionString(&not_used, &port, debuglog_conn);
		if (port != NULL) rport = atoi(port);

		NetworkStartDebugLog(NetworkAddress(debuglog_conn, rport));
	}
#endif /* ENABLE_NETWORK */

	ScanNewGRFFiles();

	ResetGRFConfig(false);

	/* Make sure _settings is filled with _settings_newgame if we switch to a game directly */
	if (_switch_mode != SM_NONE) MakeNewgameSettingsLive();

	/* initialize the ingame console */
	IConsoleInit();
	_cursor.in_window = true;
	InitializeGUI();
	IConsoleCmdExec("exec scripts/autoexec.scr 0");

	GenerateWorld(GW_EMPTY, 64, 64); // Make the viewport initialization happy
	WaitTillGeneratedWorld();

	CheckForMissingGlyphsInLoadedLanguagePack();

#ifdef ENABLE_NETWORK
	if (network && _network_available) {
		if (network_conn != NULL) {
			const char *port = NULL;
			const char *company = NULL;
			uint16 rport;

			rport = NETWORK_DEFAULT_PORT;
			_network_playas = COMPANY_NEW_COMPANY;

			ParseConnectionString(&company, &port, network_conn);

			if (company != NULL) {
				_network_playas = (CompanyID)atoi(company);

				if (_network_playas != COMPANY_SPECTATOR) {
					_network_playas--;
					if (_network_playas >= MAX_COMPANIES) return false;
				}
			}
			if (port != NULL) rport = atoi(port);

			LoadIntroGame();
			_switch_mode = SM_NONE;
			NetworkClientConnectGame(NetworkAddress(network_conn, rport));
		}
	}
#endif /* ENABLE_NETWORK */

	_video_driver->MainLoop();

	WaitTillSaved();

	/* only save config if we have to */
	if (save_config) {
		SaveToConfig();
		SaveToHighScore();
	}

	/* Reset windowing system, stop drivers, free used memory, ... */
	ShutdownGame();

	return 0;
}

void HandleExitGameRequest()
{
	if (_game_mode == GM_MENU) { // do not ask to quit on the main screen
		_exit_game = true;
	} else if (_settings_client.gui.autosave_on_exit) {
		DoExitSave();
		_exit_game = true;
	} else {
		AskExitGame();
	}
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
	SettingsDisableElrail(_settings_game.vehicle.disable_elrails);

	/* In a dedicated server, the server does not play */
	if (BlitterFactoryBase::GetCurrentBlitter()->GetScreenDepth() == 0) {
		SetLocalCompany(COMPANY_SPECTATOR);
		IConsoleCmdExec("exec scripts/game_start.scr 0");
		return;
	}

	/* Create a single company */
	DoStartupNewCompany(false);

	IConsoleCmdExec("exec scripts/game_start.scr 0");

	SetLocalCompany(COMPANY_FIRST);
	_current_company = _local_company;
	DoCommandP(0, (_settings_client.gui.autorenew << 15 ) | (_settings_client.gui.autorenew_months << 16) | 4, _settings_client.gui.autorenew_money, CMD_SET_AUTOREPLACE);

	InitializeRailGUI();

#ifdef ENABLE_NETWORK
	/* We are the server, we start a new company (not dedicated),
	 * so set the default password *if* needed. */
	if (_network_server && !StrEmpty(_settings_client.network.default_company_pass)) {
		char *password = _settings_client.network.default_company_pass;
		NetworkChangeCompanyPassword(1, &password);
	}
#endif /* ENABLE_NETWORK */

	MarkWholeScreenDirty();
}

static void MakeNewGame(bool from_heightmap)
{
	_game_mode = GM_NORMAL;

	ResetGRFConfig(true);
	_house_mngr.ResetMapping();
	_industile_mngr.ResetMapping();
	_industry_mngr.ResetMapping();

	GenerateWorldSetCallback(&MakeNewGameDone);
	GenerateWorld(from_heightmap ? GW_HEIGHTMAP : GW_NEWGAME, 1 << _settings_game.game_creation.map_x, 1 << _settings_game.game_creation.map_y);
}

static void MakeNewEditorWorldDone()
{
	SetLocalCompany(OWNER_NONE);
}

static void MakeNewEditorWorld()
{
	_game_mode = GM_EDITOR;

	ResetGRFConfig(true);

	GenerateWorldSetCallback(&MakeNewEditorWorldDone);
	GenerateWorld(GW_EMPTY, 1 << _settings_game.game_creation.map_x, 1 << _settings_game.game_creation.map_y);
}

void StartupCompanies();
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

	/* invalid type */
	if (_file_to_saveload.mode == SL_INVALID) {
		DEBUG(sl, 0, "Savegame is obsolete or invalid format: '%s'", _file_to_saveload.name);
		SetDParamStr(0, GetSaveLoadErrorString());
		ShowErrorMessage(INVALID_STRING_ID, STR_JUST_RAW_STRING, 0, 0);
		_game_mode = GM_MENU;
		return;
	}

	/* Reinitialize windows */
	ResetWindowSystem();

	SetupColoursAndInitialWindow();

	ResetGRFConfig(true);

	/* Load game */
	if (SaveOrLoad(_file_to_saveload.name, _file_to_saveload.mode, SCENARIO_DIR) != SL_OK) {
		LoadIntroGame();
		SetDParamStr(0, GetSaveLoadErrorString());
		ShowErrorMessage(INVALID_STRING_ID, STR_JUST_RAW_STRING, 0, 0);
	}

	_settings_game.difficulty = _settings_newgame.difficulty;

	/* Inititalize data */
	StartupEconomy();
	StartupCompanies();
	StartupEngines();
	StartupDisasters();

	SetLocalCompany(COMPANY_FIRST);
	_current_company = _local_company;
	DoCommandP(0, (_settings_client.gui.autorenew << 15 ) | (_settings_client.gui.autorenew_months << 16) | 4, _settings_client.gui.autorenew_money, CMD_SET_AUTOREPLACE);

	MarkWholeScreenDirty();
}

/** Load the specified savegame but on error do different things.
 * If loading fails due to corrupt savegame, bad version, etc. go back to
 * a previous correct state. In the menu for example load the intro game again.
 * @param filename file to be loaded
 * @param mode mode of loading, either SL_LOAD or SL_OLD_LOAD
 * @param newgm switch to this mode of loading fails due to some unknown error
 * @param subdir default directory to look for filename, set to 0 if not needed
 */
bool SafeSaveOrLoad(const char *filename, int mode, int newgm, Subdirectory subdir)
{
	byte ogm = _game_mode;

	_game_mode = newgm;
	assert(mode == SL_LOAD || mode == SL_OLD_LOAD);
	switch (SaveOrLoad(filename, mode, subdir)) {
		case SL_OK: return true;

		case SL_REINIT:
			switch (ogm) {
				default:
				case GM_MENU:   LoadIntroGame();      break;
				case GM_EDITOR: MakeNewEditorWorld(); break;
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
	/* If we are saving something, the network stays in his current state */
	if (new_mode != SM_SAVE) {
		/* If the network is active, make it not-active */
		if (_networking) {
			if (_network_server && (new_mode == SM_LOAD || new_mode == SM_NEWGAME)) {
				NetworkReboot();
			} else {
				NetworkDisconnect();
			}
		}

		/* If we are a server, we restart the server */
		if (_is_network_server) {
			/* But not if we are going to the menu */
			if (new_mode != SM_MENU) {
				/* check if we should reload the config */
				if (_settings_client.network.reload_cfg) {
					LoadFromConfig();
					MakeNewgameSettingsLive();
					ResetGRFConfig(false);
				}
				NetworkServerStart();
			} else {
				/* This client no longer wants to be a network-server */
				_is_network_server = false;
			}
		}
	}
#endif /* ENABLE_NETWORK */
	/* Make sure all AI controllers are gone at quiting game */
	if (new_mode != SM_SAVE) AI::KillAll();

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
			ResetGRFConfig(true);
			ResetWindowSystem();

			if (!SafeSaveOrLoad(_file_to_saveload.name, _file_to_saveload.mode, GM_NORMAL, NO_DIRECTORY)) {
				LoadIntroGame();
				SetDParamStr(0, GetSaveLoadErrorString());
				ShowErrorMessage(INVALID_STRING_ID, STR_JUST_RAW_STRING, 0, 0);
			} else {
				if (_saveload_mode == SLD_LOAD_SCENARIO) {
					StartupEngines();
				}
				/* Update the local company for a loaded game. It is either always
				* company #1 (eg 0) or in the case of a dedicated server a spectator */
				SetLocalCompany(_network_dedicated ? COMPANY_SPECTATOR : COMPANY_FIRST);
				/* Execute the game-start script */
				IConsoleCmdExec("exec scripts/game_start.scr 0");
				/* Decrease pause counter (was increased from opening load dialog) */
				DoCommandP(0, 0, 0, CMD_PAUSE);
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
			SetLocalCompany(OWNER_NONE);

			GenerateWorld(GW_HEIGHTMAP, 1 << _settings_game.game_creation.map_x, 1 << _settings_game.game_creation.map_y);
			MarkWholeScreenDirty();
			break;

		case SM_LOAD_SCENARIO: { /* Load scenario from scenario editor */
			if (SafeSaveOrLoad(_file_to_saveload.name, _file_to_saveload.mode, GM_EDITOR, NO_DIRECTORY)) {
				SetLocalCompany(OWNER_NONE);
				_settings_newgame.game_creation.starting_year = _cur_year;
			} else {
				SetDParamStr(0, GetSaveLoadErrorString());
				ShowErrorMessage(INVALID_STRING_ID, STR_JUST_RAW_STRING, 0, 0);
			}
			break;
		}

		case SM_MENU: /* Switch to game intro menu */
			LoadIntroGame();
			break;

		case SM_SAVE: /* Save game */
			/* Make network saved games on pause compatible to singleplayer */
			if (_networking && _pause_game == 1) _pause_game = 2;
			if (SaveOrLoad(_file_to_saveload.name, SL_SAVE, NO_DIRECTORY) != SL_OK) {
				SetDParamStr(0, GetSaveLoadErrorString());
				ShowErrorMessage(INVALID_STRING_ID, STR_JUST_RAW_STRING, 0, 0);
			} else {
				DeleteWindowById(WC_SAVELOAD, 0);
			}
			if (_networking && _pause_game == 2) _pause_game = 1;
			break;

		case SM_GENRANDLAND: /* Generate random land within scenario editor */
			SetLocalCompany(OWNER_NONE);
			GenerateWorld(GW_RANDOM, 1 << _settings_game.game_creation.map_x, 1 << _settings_game.game_creation.map_y);
			/* XXX: set date */
			MarkWholeScreenDirty();
			break;
	}

	if (_switch_mode_errorstr != INVALID_STRING_ID) {
		ShowErrorMessage(INVALID_STRING_ID, _switch_mode_errorstr, 0, 0);
	}
}


/**
 * State controlling game loop.
 * The state must not be changed from anywhere but here.
 * That check is enforced in DoCommand.
 */
void StateGameLoop()
{
	/* dont execute the state loop during pause */
	if (_pause_game) {
		CallWindowTickEvent();
		return;
	}
	if (IsGeneratingWorld()) return;

	ClearStorageChanges(false);

	if (_game_mode == GM_EDITOR) {
		RunTileLoop();
		CallVehicleTicks();
		CallLandscapeTick();
		ClearStorageChanges(true);

		CallWindowTickEvent();
		NewsLoop();
	} else {
		if (_debug_desync_level > 1) {
			Vehicle *v;
			FOR_ALL_VEHICLES(v) {
				if (v != v->First()) continue;

				switch (v->type) {
					case VEH_ROAD: {
						extern byte GetRoadVehLength(const Vehicle *v);
						if (GetRoadVehLength(v) != v->u.road.cached_veh_length) {
							DEBUG(desync, 2, "cache mismatch: vehicle %i, company %i, unit number %i\n", v->index, (int)v->owner, v->unitnumber);
						}
					} break;

					case VEH_TRAIN: {
						uint length = 0;
						for (Vehicle *u = v; u != NULL; u = u->Next()) length++;

						VehicleRail *wagons = MallocT<VehicleRail>(length);
						length = 0;
						for (Vehicle *u = v; u != NULL; u = u->Next()) wagons[length++] = u->u.rail;

						TrainConsistChanged(v, true);

						length = 0;
						for (Vehicle *u = v; u != NULL; u = u->Next()) {
							if (memcmp(&wagons[length], &u->u.rail, sizeof(VehicleRail)) != 0) {
								DEBUG(desync, 2, "cache mismatch: vehicle %i, company %i, unit number %i, wagon %i\n", v->index, (int)v->owner, v->unitnumber, length);
							}
							length++;
						}

						free(wagons);
					} break;

					case VEH_AIRCRAFT: {
						uint speed = v->u.air.cached_max_speed;
						UpdateAircraftCache(v);
						if (speed != v->u.air.cached_max_speed) {
							DEBUG(desync, 2, "cache mismatch: vehicle %i, company %i, unit number %i\n", v->index, (int)v->owner, v->unitnumber);
						}
					} break;

					default:
						break;
				}
			}
		}

		/* All these actions has to be done from OWNER_NONE
		 *  for multiplayer compatibility */
		CompanyID old_company = _current_company;
		_current_company = OWNER_NONE;

		AnimateAnimatedTiles();
		IncreaseDate();
		RunTileLoop();
		CallVehicleTicks();
		CallLandscapeTick();
		ClearStorageChanges(true);

		AI::GameLoop();

		CallWindowTickEvent();
		NewsLoop();
		_current_company = old_company;
	}
}

/** Create an autosave. The default name is "autosave#.sav". However with
 * the setting 'keep_all_autosave' the name defaults to company-name + date */
static void DoAutosave()
{
	char buf[MAX_PATH];

#if defined(PSP)
	/* Autosaving in networking is too time expensive for the PSP */
	if (_networking) return;
#endif /* PSP */

	if (_settings_client.gui.keep_all_autosave && _local_company != COMPANY_SPECTATOR) {
		GenerateDefaultSaveName(buf, lastof(buf));
		strecat(buf, ".sav", lastof(buf));
	} else {
		/* generate a savegame name and number according to _settings_client.gui.max_num_autosaves */
		snprintf(buf, sizeof(buf), "autosave%d.sav", _autosave_ctr);

		if (++_autosave_ctr >= _settings_client.gui.max_num_autosaves) _autosave_ctr = 0;
	}

	DEBUG(sl, 2, "Autosaving to '%s'", buf);
	if (SaveOrLoad(buf, SL_SAVE, AUTOSAVE_DIR) != SL_OK) {
		ShowErrorMessage(INVALID_STRING_ID, STR_AUTOSAVE_FAILED, 0, 0);
	}
}

void GameLoop()
{
	ProcessAsyncSaveFinish();

	/* autosave game? */
	if (_do_autosave) {
		_do_autosave = false;
		DoAutosave();
		RedrawAutosave();
	}

	/* make a screenshot? */
	if (IsScreenshotRequested()) ShowScreenshotResult(MakeScreenshot());

	/* switch game mode? */
	if (_switch_mode != SM_NONE) {
		SwitchMode(_switch_mode);
		_switch_mode = SM_NONE;
	}

	IncreaseSpriteLRU();
	InteractiveRandom();

	_caret_timer += 3;
	_palette_animation_counter += 8;
	CursorTick();

#ifdef ENABLE_NETWORK
	/* Check for UDP stuff */
	if (_network_available) NetworkUDPGameLoop();

	if (_networking && !IsGeneratingWorld()) {
		/* Multiplayer */
		NetworkGameLoop();
	} else {
		if (_network_reconnect > 0 && --_network_reconnect == 0) {
			/* This means that we want to reconnect to the last host
			 * We do this here, because it means that the network is really closed */
			_network_playas = COMPANY_SPECTATOR;
			NetworkClientConnectGame(NetworkAddress(_settings_client.network.last_host, _settings_client.network.last_port));
		}
		/* Singleplayer */
		StateGameLoop();
	}
#else
	StateGameLoop();
#endif /* ENABLE_NETWORK */

	if (!_pause_game && HasBit(_display_opt, DO_FULL_ANIMATION)) DoPaletteAnimations();

	if (!_pause_game || _cheats.build_in_pause.value) MoveAllTextEffects();

	InputLoop();

	_sound_driver->MainLoop();
	MusicLoop();
}
