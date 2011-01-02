/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file openttd.cpp Functions related to starting OpenTTD. */

#include "stdafx.h"

#include "blitter/factory.hpp"
#include "sound/sound_driver.hpp"
#include "music/music_driver.hpp"
#include "video/video_driver.hpp"

#include "fontcache.h"
#include "gui.h"
#include "sound_func.h"
#include "window_func.h"

#include "base_media_base.h"
#include "saveload/saveload.h"
#include "company_func.h"
#include "command_func.h"
#include "news_func.h"
#include "fios.h"
#include "aircraft.h"
#include "roadveh.h"
#include "train.h"
#include "ship.h"
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
#include "roadstop_base.h"
#include "functions.h"
#include "elrail_func.h"
#include "rev.h"
#include "highscore.h"
#include "thread/thread.h"
#include "station_base.h"
#include "crashlog.h"
#include "engine_func.h"
#include "core/random_func.hpp"
#include "rail_gui.h"
#include "core/backup_type.hpp"
#include "hotkeys.h"
#include "newgrf.h"


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
extern Company *DoStartupNewCompany(bool is_ai, CompanyID company = INVALID_COMPANY);
extern void ShowOSErrorBox(const char *buf, bool system);
extern char *_config_file;

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

	/* Set the error message for the crash log and then invoke it. */
	CrashLog::SetErrorMessage(buf);
	abort();
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
		"  -r res              = Set resolution (for instance 800x600)\n"
		"  -h                  = Display this help text\n"
		"  -t year             = Set starting year\n"
		"  -d [[fac=]lvl[,...]]= Debug mode\n"
		"  -e                  = Start Editor\n"
		"  -g [savegame]       = Start new/save game immediately\n"
		"  -G seed             = Set random seed\n"
#if defined(ENABLE_NETWORK)
		"  -n [ip:port#company]= Start networkgame\n"
		"  -p password         = Password to join server\n"
		"  -P password         = Password to join company\n"
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
		"  -S sounds_set       = Force the sounds set (see below)\n"
		"  -M music_set        = Force the music set (see below)\n"
		"  -c config_file      = Use 'config_file' instead of 'openttd.cfg'\n"
		"  -x                  = Do not automatically save to config file on exit\n"
		"\n",
		lastof(buf)
	);

	/* List the graphics packs */
	p = BaseGraphics::GetSetsList(p, lastof(buf));

	/* List the sounds packs */
	p = BaseSounds::GetSetsList(p, lastof(buf));

	/* List the music packs */
	p = BaseMusic::GetSetsList(p, lastof(buf));

	/* List the drivers */
	p = VideoDriverFactoryBase::GetDriversInfo(p, lastof(buf));

	/* List the blitters */
	p = BlitterFactoryBase::GetBlittersInfo(p, lastof(buf));

	/* We need to initialize the AI, so it finds the AIs */
	TarScanner::DoScan();
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
	char *cont;

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
	char *s = md->cont;
	if (s != NULL) {
		goto md_continue_here;
	}

	for (;;) {
		if (--md->numleft < 0) return -1;

		s = *md->argv++;
		if (*s == '-') {
md_continue_here:;
			s++;
			if (*s != 0) {
				const char *r;
				/* Found argument, try to locate it in options. */
				if (*s == ':' || (r = strchr(md->options, *s)) == NULL) {
					/* ERROR! */
					return -2;
				}
				if (r[1] == ':') {
					char *t;
					/* Item wants an argument. Check if the argument follows, or if it comes as a separate arg. */
					if (!*(t = s + 1)) {
						/* It comes as a separate arg. Check if out of args? */
						if (--md->numleft < 0 || *(t = *md->argv) == '-') {
							/* Check if item is optional? */
							if (r[2] != ':') return -2;
							md->numleft++;
							t = NULL;
						} else {
							md->argv++;
						}
					}
					md->opt = t;
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
	_engine_mngr.ResetToDefaultMapping();
	_house_mngr.ResetMapping();
	_industry_mngr.ResetMapping();
	_industile_mngr.ResetMapping();
	_airport_mngr.ResetMapping();
	_airporttile_mngr.ResetMapping();
}


/**
 * Unitializes drivers, frees allocated memory, cleans pools, ...
 * Generally, prepares the game for shutting down
 */
static void ShutdownGame()
{
	IConsoleFree();

	if (_network_available) NetworkShutDown(); // Shut down the network and close any open connections

	DriverFactoryBase::ShutdownDrivers();

	UnInitWindowSystem();

	/* stop the AI */
	AI::Uninitialize(false);

	/* Uninitialize variables that are allocated dynamically */
	GamelogReset();
	_town_pool.CleanPool();
	_industry_pool.CleanPool();
	_station_pool.CleanPool();
	_roadstop_pool.CleanPool();
	_vehicle_pool.CleanPool();
	_sign_pool.CleanPool();
	_order_pool.CleanPool();
	_group_pool.CleanPool();
	_cargopacket_pool.CleanPool();
	_engine_pool.CleanPool();
	_company_pool.CleanPool();

#ifdef ENABLE_NETWORK
	free(_config_file);
#endif

	ResetNewGRFData();

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
		GenerateWorld(GWM_EMPTY, 64, 64); // if failed loading, make empty world.
		WaitTillGeneratedWorld();
		SetLocalCompany(COMPANY_SPECTATOR);
	} else {
		SetLocalCompany(COMPANY_FIRST);
	}

	_pause_mode = PM_UNPAUSED;
	_cursor.fix_at = false;

	CheckForMissingSprites();
	CheckForMissingGlyphsInLoadedLanguagePack();

	/* Play main theme */
	if (_music_driver->IsSongPlaying()) ResetMusic();
}

void MakeNewgameSettingsLive()
{
#ifdef ENABLE_AI
	for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
		if (_settings_game.ai_config[c] != NULL) {
			delete _settings_game.ai_config[c];
		}
	}
#endif /* ENABLE_AI */

	_settings_game = _settings_newgame;

#ifdef ENABLE_AI
	for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
		_settings_game.ai_config[c] = NULL;
		if (_settings_newgame.ai_config[c] != NULL) {
			_settings_game.ai_config[c] = new AIConfig(_settings_newgame.ai_config[c]);
		}
	}
#endif /* ENABLE_AI */
}

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
	char *sounds_set = NULL;
	char *music_set = NULL;
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
	char *join_server_password = NULL;
	char *join_company_password = NULL;

	extern bool _dedicated_forks;
	_dedicated_forks = false;
#endif /* ENABLE_NETWORK */

	_game_mode = GM_MENU;
	_switch_mode = SM_MENU;
	_switch_mode_errorstr = INVALID_STRING_ID;
	_config_file = NULL;

	/* The last param of the following function means this:
	 *   a letter means: it accepts that param (e.g.: -h)
	 *   a ':' behind it means: it need a param (e.g.: -m<driver>)
	 *   a '::' behind it means: it can optional have a param (e.g.: -d<debug>) */
	optformat = "m:s:v:b:hD::n::ei::I:S:M:t:d::r:g::G:c:xl:p:P:"
#if !defined(__MORPHOS__) && !defined(__AMIGA__) && !defined(WIN32)
		"f"
#endif
	;

	MyGetOptData mgo(argc - 1, argv + 1, optformat);

	while ((i = MyGetOpt(&mgo)) != -1) {
		switch (i) {
		case 'I': free(graphics_set); graphics_set = strdup(mgo.opt); break;
		case 'S': free(sounds_set); sounds_set = strdup(mgo.opt); break;
		case 'M': free(music_set); music_set = strdup(mgo.opt); break;
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
			SetDebugString("net=6");
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
		case 'p':
			join_server_password = mgo.opt;
			break;
		case 'P':
			join_company_password = mgo.opt;
			break;
#endif /* ENABLE_NETWORK */
		case 'r': ParseResolution(&resolution, mgo.opt); break;
		case 't': startyear = atoi(mgo.opt); break;
		case 'd': {
#if defined(WIN32)
				CreateConsole();
#endif
				if (mgo.opt != NULL) SetDebugString(mgo.opt);
				break;
			}
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
			BaseGraphics::FindSets();
			BaseSounds::FindSets();
			BaseMusic::FindSets();
			ShowHelp();
			return 0;
		}
	}

#if defined(WINCE) && defined(_DEBUG)
	/* Switch on debug lvl 4 for WinCE if Debug release, as you can't give params, and you most likely do want this information */
	SetDebugString("4");
#endif

	DeterminePaths(argv[0]);
	BaseGraphics::FindSets();
	BaseSounds::FindSets();
	BaseMusic::FindSets();

#if defined(ENABLE_NETWORK) && defined(UNIX) && !defined(__MORPHOS__)
	/* We must fork here, or we'll end up without some resources we need (like sockets) */
	if (_dedicated_forks) DedicatedFork();
#endif

	TarScanner::DoScan();
	AI::Initialize();
	LoadFromConfig();
	AI::Uninitialize(true);
	CheckConfig();
	LoadFromHighScore();
	LoadHotkeysFromConfig();

	if (resolution.width != 0) { _cur_resolution = resolution; }
	if (startyear != INVALID_YEAR) _settings_newgame.game_creation.starting_year = startyear;
	if (generation_seed != GENERATE_NEW_SEED) _settings_newgame.game_creation.generation_seed = generation_seed;

	/*
	 * The width and height must be at least 1 pixel and width times
	 * height must still fit within a 32 bits integer, this way all
	 * internal drawing routines work correctly.
	 */
	_cur_resolution.width  = ClampU(_cur_resolution.width,  1, UINT16_MAX);
	_cur_resolution.height = ClampU(_cur_resolution.height, 1, UINT16_MAX);

#if defined(ENABLE_NETWORK)
	if (dedicated) DEBUG(net, 0, "Starting dedicated version %s", _openttd_revision);
	if (dedicated_host) {
		_network_bind_list.Clear();
		*_network_bind_list.Append() = strdup(dedicated_host);
	}
	if (dedicated_port) _settings_client.network.server_port = dedicated_port;
	if (_dedicated_forks && !dedicated) _dedicated_forks = false;
#endif /* ENABLE_NETWORK */

	/* enumerate language files */
	InitializeLanguagePacks();

	/* initialize screenshot formats */
	InitializeScreenshotFormats();

	/* initialize all variables that are allocated dynamically */
	InitializeDynamicVariables();

	/* Initialize FreeType */
	InitFreeType();

	/* This must be done early, since functions use the SetWindowDirty* calls */
	InitWindowSystem();

	/* Look for the sounds before the graphics. Otherwise none would be set and
	 * the first initialisation of the video happens on the wrong data. Now it
	 * can do the first initialisation right. */
	if (sounds_set == NULL && BaseSounds::ini_set != NULL) sounds_set = strdup(BaseSounds::ini_set);
	if (!BaseSounds::SetSet(sounds_set)) {
		StrEmpty(sounds_set) ?
			usererror("Failed to find a sounds set. Please acquire a sounds set for OpenTTD. See section 4.1 of readme.txt.") :
			usererror("Failed to select requested sounds set '%s'", sounds_set);
	}
	free(sounds_set);

	if (graphics_set == NULL && BaseGraphics::ini_set != NULL) graphics_set = strdup(BaseGraphics::ini_set);
	if (!BaseGraphics::SetSet(graphics_set)) {
		StrEmpty(graphics_set) ?
			usererror("Failed to find a graphics set. Please acquire a graphics set for OpenTTD. See section 4.1 of readme.txt.") :
			usererror("Failed to select requested graphics set '%s'", graphics_set);
	}
	free(graphics_set);

	if (music_set == NULL && BaseMusic::ini_set != NULL) music_set = strdup(BaseMusic::ini_set);
	if (!BaseMusic::SetSet(music_set)) {
		StrEmpty(music_set) ?
			usererror("Failed to find a music set. Please acquire a music set for OpenTTD. See section 4.1 of readme.txt.") :
			usererror("Failed to select requested music set '%s'", music_set);
	}
	free(music_set);

	/* Initialize game palette */
	GfxInitPalettes();

	DEBUG(misc, 1, "Loading blitter...");
	if (blitter == NULL && _ini_blitter != NULL) blitter = strdup(_ini_blitter);
	if (BlitterFactoryBase::SelectBlitter(blitter) == NULL) {
		StrEmpty(blitter) ?
			usererror("Failed to autoprobe blitter") :
			usererror("Failed to select requested blitter '%s'; does it exist?", blitter);
	}
	free(blitter);

	DEBUG(driver, 1, "Loading drivers...");

	if (sounddriver == NULL && _ini_sounddriver != NULL) sounddriver = strdup(_ini_sounddriver);
	_sound_driver = (SoundDriver*)SoundDriverFactoryBase::SelectDriver(sounddriver, Driver::DT_SOUND);
	if (_sound_driver == NULL) {
		StrEmpty(sounddriver) ?
			usererror("Failed to autoprobe sound driver") :
			usererror("Failed to select requested sound driver '%s'", sounddriver);
	}
	free(sounddriver);

	if (videodriver == NULL && _ini_videodriver != NULL) videodriver = strdup(_ini_videodriver);
	_video_driver = (VideoDriver*)VideoDriverFactoryBase::SelectDriver(videodriver, Driver::DT_VIDEO);
	if (_video_driver == NULL) {
		StrEmpty(videodriver) ?
			usererror("Failed to autoprobe video driver") :
			usererror("Failed to select requested video driver '%s'", videodriver);
	}
	free(videodriver);

	if (musicdriver == NULL && _ini_musicdriver != NULL) musicdriver = strdup(_ini_musicdriver);
	_music_driver = (MusicDriver*)MusicDriverFactoryBase::SelectDriver(musicdriver, Driver::DT_MUSIC);
	if (_music_driver == NULL) {
		StrEmpty(musicdriver) ?
			usererror("Failed to autoprobe music driver") :
			usererror("Failed to select requested music driver '%s'", musicdriver);
	}
	free(musicdriver);

	/* Initialize the zoom level of the screen to normal */
	_screen.zoom = ZOOM_LVL_NORMAL;

	/* restore saved music volume */
	_music_driver->SetVolume(_msf.music_vol);

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

	/* Take our initial lock on whatever we might want to do! */
	_genworld_paint_mutex->BeginCritical();
	_genworld_mapgen_mutex->BeginCritical();

	GenerateWorld(GWM_EMPTY, 64, 64); // Make the viewport initialization happy
	WaitTillGeneratedWorld();

	CheckForMissingGlyphsInLoadedLanguagePack();

#ifdef ENABLE_NETWORK
	if (network && _network_available) {
		if (network_conn != NULL) {
			const char *port = NULL;
			const char *company = NULL;
			uint16 rport = NETWORK_DEFAULT_PORT;
			CompanyID join_as = COMPANY_NEW_COMPANY;

			ParseConnectionString(&company, &port, network_conn);

			if (company != NULL) {
				join_as = (CompanyID)atoi(company);

				if (join_as != COMPANY_SPECTATOR) {
					join_as--;
					if (join_as >= MAX_COMPANIES) return false;
				}
			}
			if (port != NULL) rport = atoi(port);

			LoadIntroGame();
			_switch_mode = SM_NONE;
			NetworkClientConnectGame(NetworkAddress(network_conn, rport), join_as, join_server_password, join_company_password);
		}
	}
#endif /* ENABLE_NETWORK */

	_video_driver->MainLoop();

	WaitTillSaved();

	/* only save config if we have to */
	if (save_config) {
		SaveToConfig();
		SaveHotkeysToConfig();
		SaveToHighScore();
	}

	/* Reset windowing system, stop drivers, free used memory, ... */
	ShutdownGame();

	free(const_cast<char *>(BaseGraphics::ini_set));
	free(const_cast<char *>(BaseSounds::ini_set));
	free(const_cast<char *>(BaseMusic::ini_set));
	free(_ini_musicdriver);
	free(_ini_sounddriver);
	free(_ini_videodriver);
	free(_ini_blitter);

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

static void MakeNewGameDone()
{
	SettingsDisableElrail(_settings_game.vehicle.disable_elrails);

	/* In a dedicated server, the server does not play */
	if (_network_dedicated || BlitterFactoryBase::GetCurrentBlitter()->GetScreenDepth() == 0) {
		SetLocalCompany(COMPANY_SPECTATOR);
		IConsoleCmdExec("exec scripts/game_start.scr 0");
		return;
	}

	/* Create a single company */
	DoStartupNewCompany(false);

	Company *c = Company::Get(COMPANY_FIRST);
	c->settings = _settings_client.company;

	IConsoleCmdExec("exec scripts/game_start.scr 0");

	SetLocalCompany(COMPANY_FIRST);

	InitializeRailGUI();

#ifdef ENABLE_NETWORK
	/* We are the server, we start a new company (not dedicated),
	 * so set the default password *if* needed. */
	if (_network_server && !StrEmpty(_settings_client.network.default_company_pass)) {
		NetworkChangeCompanyPassword(_settings_client.network.default_company_pass);
	}
#endif /* ENABLE_NETWORK */

	if (_settings_client.gui.pause_on_newgame) DoCommandP(0, PM_PAUSED_NORMAL, 1, CMD_PAUSE);

	MarkWholeScreenDirty();
}

static void MakeNewGame(bool from_heightmap, bool reset_settings)
{
	_game_mode = GM_NORMAL;

	ResetGRFConfig(true);
	InitializeDynamicVariables();

	GenerateWorldSetCallback(&MakeNewGameDone);
	GenerateWorld(from_heightmap ? GWM_HEIGHTMAP : GWM_NEWGAME, 1 << _settings_game.game_creation.map_x, 1 << _settings_game.game_creation.map_y, reset_settings);
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
	GenerateWorld(GWM_EMPTY, 1 << _settings_game.game_creation.map_x, 1 << _settings_game.game_creation.map_y);
}

void StartupCompanies();
void StartupDisasters();
extern void StartupEconomy();

/**
 * Load the specified savegame but on error do different things.
 * If loading fails due to corrupt savegame, bad version, etc. go back to
 * a previous correct state. In the menu for example load the intro game again.
 * @param mode mode of loading, either SL_LOAD or SL_OLD_LOAD
 * @param newgm switch to this mode of loading fails due to some unknown error
 * @param filename file to be loaded
 * @param subdir default directory to look for filename, set to 0 if not needed
 * @param lf Load filter to use, if NULL: use filename + subdir.
 */
bool SafeLoad(const char *filename, int mode, GameMode newgm, Subdirectory subdir, struct LoadFilter *lf = NULL)
{
	assert(mode == SL_LOAD || (lf == NULL && mode == SL_OLD_LOAD));
	GameMode ogm = _game_mode;

	_game_mode = newgm;

	switch (lf == NULL ? SaveOrLoad(filename, mode, subdir) : LoadWithFilter(lf)) {
		case SL_OK: return true;

		case SL_REINIT:
#ifdef ENABLE_NETWORK
			if (_network_dedicated) {
				/*
				 * We need to reinit a network map...
				 * We can't simply load the intro game here as that game has many
				 * special cases which make clients desync immediately. So we fall
				 * back to just generating a new game with the current settings.
				 */
				DEBUG(net, 0, "Loading game failed, so a new (random) game will be started!");
				MakeNewGame(false, true);
				return false;
			}
			if (_network_server) {
				/* We can't load the intro game as server, so disconnect first. */
				NetworkDisconnect();
			}
#endif /* ENABLE_NETWORK */

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
		ShowErrorMessage(STR_JUST_RAW_STRING, INVALID_STRING_ID, WL_ERROR);
		_game_mode = GM_MENU;
		return;
	}

	/* Reinitialize windows */
	ResetWindowSystem();

	SetupColoursAndInitialWindow();

	ResetGRFConfig(true);

	/* Load game */
	if (!SafeLoad(_file_to_saveload.name, _file_to_saveload.mode, GM_NORMAL, SCENARIO_DIR)) {
		SetDParamStr(0, GetSaveLoadErrorString());
		ShowErrorMessage(STR_JUST_RAW_STRING, INVALID_STRING_ID, WL_ERROR);
		return;
	}

	_settings_game.difficulty = _settings_newgame.difficulty;

	/* Inititalize data */
	StartupEconomy();
	StartupCompanies();
	StartupEngines();
	StartupDisasters();

	SetLocalCompany(COMPANY_FIRST);
	Company *c = Company::Get(COMPANY_FIRST);
	c->settings = _settings_client.company;

	MarkWholeScreenDirty();
}

void SwitchToMode(SwitchMode new_mode)
{
#ifdef ENABLE_NETWORK
	/* If we are saving something, the network stays in his current state */
	if (new_mode != SM_SAVE) {
		/* If the network is active, make it not-active */
		if (_networking) {
			if (_network_server && (new_mode == SM_LOAD || new_mode == SM_NEWGAME || new_mode == SM_RESTARTGAME)) {
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
		case SM_EDITOR: // Switch to scenario editor
			MakeNewEditorWorld();
			break;

		case SM_RESTARTGAME: // Restart --> 'Random game' with current settings
		case SM_NEWGAME: // New Game --> 'Random game'
#ifdef ENABLE_NETWORK
			if (_network_server) {
				snprintf(_network_game_info.map_name, lengthof(_network_game_info.map_name), "Random Map");
			}
#endif /* ENABLE_NETWORK */
			MakeNewGame(false, new_mode == SM_NEWGAME);
			break;

		case SM_START_SCENARIO: // New Game --> Choose one of the preset scenarios
#ifdef ENABLE_NETWORK
			if (_network_server) {
				snprintf(_network_game_info.map_name, lengthof(_network_game_info.map_name), "%s (Loaded scenario)", _file_to_saveload.title);
			}
#endif /* ENABLE_NETWORK */
			StartScenario();
			break;

		case SM_LOAD: { // Load game, Play Scenario
			ResetGRFConfig(true);
			ResetWindowSystem();

			if (!SafeLoad(_file_to_saveload.name, _file_to_saveload.mode, GM_NORMAL, NO_DIRECTORY)) {
				SetDParamStr(0, GetSaveLoadErrorString());
				ShowErrorMessage(STR_JUST_RAW_STRING, INVALID_STRING_ID, WL_ERROR);
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
				DoCommandP(0, PM_PAUSED_SAVELOAD, 0, CMD_PAUSE);
#ifdef ENABLE_NETWORK
				if (_network_server) {
					snprintf(_network_game_info.map_name, lengthof(_network_game_info.map_name), "%s (Loaded game)", _file_to_saveload.title);
				}
#endif /* ENABLE_NETWORK */
			}
			break;
		}

		case SM_START_HEIGHTMAP: // Load a heightmap and start a new game from it
#ifdef ENABLE_NETWORK
			if (_network_server) {
				snprintf(_network_game_info.map_name, lengthof(_network_game_info.map_name), "%s (Heightmap)", _file_to_saveload.title);
			}
#endif /* ENABLE_NETWORK */
			MakeNewGame(true, true);
			break;

		case SM_LOAD_HEIGHTMAP: // Load heightmap from scenario editor
			SetLocalCompany(OWNER_NONE);

			GenerateWorld(GWM_HEIGHTMAP, 1 << _settings_game.game_creation.map_x, 1 << _settings_game.game_creation.map_y);
			MarkWholeScreenDirty();
			break;

		case SM_LOAD_SCENARIO: { // Load scenario from scenario editor
			if (SafeLoad(_file_to_saveload.name, _file_to_saveload.mode, GM_EDITOR, NO_DIRECTORY)) {
				SetLocalCompany(OWNER_NONE);
				_settings_newgame.game_creation.starting_year = _cur_year;
				/* Cancel the saveload pausing */
				DoCommandP(0, PM_PAUSED_SAVELOAD, 0, CMD_PAUSE);
			} else {
				SetDParamStr(0, GetSaveLoadErrorString());
				ShowErrorMessage(STR_JUST_RAW_STRING, INVALID_STRING_ID, WL_ERROR);
			}
			break;
		}

		case SM_MENU: // Switch to game intro menu
			LoadIntroGame();
			if (BaseSounds::ini_set == NULL && BaseSounds::GetUsedSet()->fallback) {
				ShowErrorMessage(STR_WARNING_FALLBACK_SOUNDSET, INVALID_STRING_ID, WL_CRITICAL);
				BaseSounds::ini_set = strdup(BaseSounds::GetUsedSet()->name);
			}
			break;

		case SM_SAVE: // Save game
			/* Make network saved games on pause compatible to singleplayer */
			if (SaveOrLoad(_file_to_saveload.name, SL_SAVE, NO_DIRECTORY) != SL_OK) {
				SetDParamStr(0, GetSaveLoadErrorString());
				ShowErrorMessage(STR_JUST_RAW_STRING, INVALID_STRING_ID, WL_ERROR);
			} else {
				DeleteWindowById(WC_SAVELOAD, 0);
			}
			break;

		case SM_GENRANDLAND: // Generate random land within scenario editor
			SetLocalCompany(OWNER_NONE);
			GenerateWorld(GWM_RANDOM, 1 << _settings_game.game_creation.map_x, 1 << _settings_game.game_creation.map_y);
			/* XXX: set date */
			MarkWholeScreenDirty();
			break;

		default: NOT_REACHED();
	}

	if (_switch_mode_errorstr != INVALID_STRING_ID) {
		ShowErrorMessage(_switch_mode_errorstr, INVALID_STRING_ID, WL_CRITICAL);
		_switch_mode_errorstr = INVALID_STRING_ID;
	}
}


/**
 * Check the validity of some of the caches.
 * Especially in the sense of desyncs between
 * the cached value and what the value would
 * be when calculated from the 'base' data.
 */
static void CheckCaches()
{
	/* Return here so it is easy to add checks that are run
	 * always to aid testing of caches. */
	if (_debug_desync_level <= 1) return;

	/* Strict checking of the road stop cache entries */
	const RoadStop *rs;
	FOR_ALL_ROADSTOPS(rs) {
		if (IsStandardRoadStopTile(rs->xy)) continue;

		assert(rs->GetEntry(DIAGDIR_NE) != rs->GetEntry(DIAGDIR_NW));
		rs->GetEntry(DIAGDIR_NE)->CheckIntegrity(rs);
		rs->GetEntry(DIAGDIR_NW)->CheckIntegrity(rs);
	}

	Vehicle *v;
	FOR_ALL_VEHICLES(v) {
		extern void FillNewGRFVehicleCache(const Vehicle *v);
		if (v != v->First() || v->vehstatus & VS_CRASHED || !v->IsPrimaryVehicle()) continue;

		uint length = 0;
		for (const Vehicle *u = v; u != NULL; u = u->Next()) length++;

		NewGRFCache        *grf_cache = CallocT<NewGRFCache>(length);
		VehicleCache       *veh_cache = CallocT<VehicleCache>(length);
		GroundVehicleCache *gro_cache = CallocT<GroundVehicleCache>(length);
		TrainCache         *tra_cache = CallocT<TrainCache>(length);

		length = 0;
		for (const Vehicle *u = v; u != NULL; u = u->Next()) {
			FillNewGRFVehicleCache(u);
			grf_cache[length] = u->grf_cache;
			veh_cache[length] = u->vcache;
			switch (u->type) {
				case VEH_TRAIN:
					gro_cache[length] = Train::From(u)->gcache;
					tra_cache[length] = Train::From(u)->tcache;
					break;
				case VEH_ROAD:
					gro_cache[length] = RoadVehicle::From(u)->gcache;
					break;
				default:
					break;
			}
			length++;
		}

		switch (v->type) {
			case VEH_TRAIN:    Train::From(v)->ConsistChanged(true);     break;
			case VEH_ROAD:     RoadVehUpdateCache(RoadVehicle::From(v)); break;
			case VEH_AIRCRAFT: UpdateAircraftCache(Aircraft::From(v));   break;
			case VEH_SHIP:     Ship::From(v)->UpdateCache();             break;
			default: break;
		}

		length = 0;
		for (const Vehicle *u = v; u != NULL; u = u->Next()) {
			FillNewGRFVehicleCache(u);
			if (memcmp(&grf_cache[length], &u->grf_cache, sizeof(NewGRFCache)) != 0) {
				DEBUG(desync, 2, "newgrf cache mismatch: type %i, vehicle %i, company %i, unit number %i, wagon %i", (int)v->type, v->index, (int)v->owner, v->unitnumber, length);
			}
			if (memcmp(&veh_cache[length], &u->vcache, sizeof(VehicleCache)) != 0) {
				DEBUG(desync, 2, "vehicle cache mismatch: type %i, vehicle %i, company %i, unit number %i, wagon %i", (int)v->type, v->index, (int)v->owner, v->unitnumber, length);
			}
			switch (u->type) {
				case VEH_TRAIN:
					if (memcmp(&gro_cache[length], &Train::From(u)->gcache, sizeof(GroundVehicleCache)) != 0) {
						DEBUG(desync, 2, "train ground vehicle cache mismatch: vehicle %i, company %i, unit number %i, wagon %i", v->index, (int)v->owner, v->unitnumber, length);
					}
					if (memcmp(&tra_cache[length], &Train::From(u)->tcache, sizeof(TrainCache)) != 0) {
						DEBUG(desync, 2, "train cache mismatch: vehicle %i, company %i, unit number %i, wagon %i", v->index, (int)v->owner, v->unitnumber, length);
					}
					break;
				case VEH_ROAD:
					if (memcmp(&gro_cache[length], &RoadVehicle::From(u)->gcache, sizeof(GroundVehicleCache)) != 0) {
						DEBUG(desync, 2, "road vehicle ground vehicle cache mismatch: vehicle %i, company %i, unit number %i, wagon %i", v->index, (int)v->owner, v->unitnumber, length);
					}
					break;
				default:
					break;
			}
			length++;
		}

		free(grf_cache);
		free(veh_cache);
		free(gro_cache);
		free(tra_cache);
	}

	/* Check whether the caches are still valid */
	FOR_ALL_VEHICLES(v) {
		byte buff[sizeof(VehicleCargoList)];
		memcpy(buff, &v->cargo, sizeof(VehicleCargoList));
		v->cargo.InvalidateCache();
		assert(memcmp(&v->cargo, buff, sizeof(VehicleCargoList)) == 0);
	}

	Station *st;
	FOR_ALL_STATIONS(st) {
		for (CargoID c = 0; c < NUM_CARGO; c++) {
			byte buff[sizeof(StationCargoList)];
			memcpy(buff, &st->goods[c].cargo, sizeof(StationCargoList));
			st->goods[c].cargo.InvalidateCache();
			assert(memcmp(&st->goods[c].cargo, buff, sizeof(StationCargoList)) == 0);
		}
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
	if (_pause_mode != PM_UNPAUSED) {
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
		if (_debug_desync_level > 2 && _date_fract == 0 && (_date & 0x1F) == 0) {
			/* Save the desync savegame if needed. */
			char name[MAX_PATH];
			snprintf(name, lengthof(name), "dmp_cmds_%08x_%08x.sav", _settings_game.game_creation.generation_seed, _date);
			SaveOrLoad(name, SL_SAVE, AUTOSAVE_DIR);
		}

		CheckCaches();

		/* All these actions has to be done from OWNER_NONE
		 *  for multiplayer compatibility */
		Backup<CompanyByte> cur_company(_current_company, OWNER_NONE, FILE_LINE);

		AnimateAnimatedTiles();
		IncreaseDate();
		RunTileLoop();
		CallVehicleTicks();
		CallLandscapeTick();
		ClearStorageChanges(true);

		AI::GameLoop();

		CallWindowTickEvent();
		NewsLoop();
		cur_company.Restore();
	}

	assert(IsLocalCompany());
}

/**
 * Create an autosave. The default name is "autosave#.sav". However with
 * the setting 'keep_all_autosave' the name defaults to company-name + date
 */
static void DoAutosave()
{
	char buf[MAX_PATH];

#if defined(PSP)
	/* Autosaving in networking is too time expensive for the PSP */
	if (_networking) return;
#endif /* PSP */

	if (_settings_client.gui.keep_all_autosave) {
		GenerateDefaultSaveName(buf, lastof(buf));
		strecat(buf, ".sav", lastof(buf));
	} else {
		static int _autosave_ctr = 0;

		/* generate a savegame name and number according to _settings_client.gui.max_num_autosaves */
		snprintf(buf, sizeof(buf), "autosave%d.sav", _autosave_ctr);

		if (++_autosave_ctr >= _settings_client.gui.max_num_autosaves) _autosave_ctr = 0;
	}

	DEBUG(sl, 2, "Autosaving to '%s'", buf);
	if (SaveOrLoad(buf, SL_SAVE, AUTOSAVE_DIR) != SL_OK) {
		ShowErrorMessage(STR_ERROR_AUTOSAVE_FAILED, INVALID_STRING_ID, WL_ERROR);
	}
}

void GameLoop()
{
	ProcessAsyncSaveFinish();

	/* autosave game? */
	if (_do_autosave) {
		_do_autosave = false;
		DoAutosave();
		SetWindowDirty(WC_STATUS_BAR, 0);
	}

	/* switch game mode? */
	if (_switch_mode != SM_NONE) {
		SwitchToMode(_switch_mode);
		_switch_mode = SM_NONE;
	}

	IncreaseSpriteLRU();
	InteractiveRandom();

	extern int _caret_timer;
	_caret_timer += 3;
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
			NetworkClientConnectGame(NetworkAddress(_settings_client.network.last_host, _settings_client.network.last_port), COMPANY_SPECTATOR);
		}
		/* Singleplayer */
		StateGameLoop();
	}

	/* Check chat messages roughly once a second. */
	static uint check_message = 0;
	if (++check_message > 1000 / MILLISECONDS_PER_TICK) {
		check_message = 0;
		NetworkChatMessageLoop();
	}
#else
	StateGameLoop();
#endif /* ENABLE_NETWORK */

	if (!_pause_mode && HasBit(_display_opt, DO_FULL_ANIMATION)) DoPaletteAnimations();

	if (!_pause_mode || _game_mode == GM_EDITOR || _settings_game.construction.command_pause_level > CMDPL_NO_CONSTRUCTION) MoveAllTextEffects();

	InputLoop();

	_sound_driver->MainLoop();
	MusicLoop();
}
