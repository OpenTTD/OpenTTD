#include "stdafx.h"
#include "string.h"
#include "table/strings.h"
#include "debug.h"
#include "strings.h"
#include "map.h"
#include "tile.h"

#define VARDEF
#include "openttd.h"
#include "mixer.h"
#include "spritecache.h"
#include "gfx.h"
#include "gui.h"
#include "station.h"
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
#include "ai.h"
#include "console.h"
#include "screenshot.h"
#include "network.h"
#include "signs.h"
#include "depot.h"
#include "waypoint.h"

#include <stdarg.h>

void GenerateWorld(int mode, uint log_x, uint log_y);
void CallLandscapeTick(void);
void IncreaseDate(void);
void RunOtherPlayersLoop(void);
void DoPaletteAnimations(void);
void MusicLoop(void);
void ResetMusic(void);
void InitializeStations(void);
void DeleteAllPlayerStations(void);

extern void SetDifficultyLevel(int mode, GameOptions *gm_opt);
extern void DoStartupNewPlayer(bool is_ai);
extern void ShowOSErrorBox(const char *buf);

bool LoadSavegame(const char *filename);

extern void HalGameLoop(void);

uint32 _pixels_redrawn;
bool _dbg_screen_rect;
bool disable_computer; // We should get ride of this thing.. is only used for a debug-cheat
static byte _os_version = 0;

/* TODO: usrerror() for errors which are not of an internal nature but
 * caused by the user, i.e. missing files or fatal configuration errors.
 * Post-0.4.0 since Celestar doesn't want this in SVN before. --pasky */

void CDECL error(const char *s, ...) {
	va_list va;
	char buf[512];
	va_start(va, s);
	vsprintf(buf, s, va);
	va_end(va);

	ShowOSErrorBox(buf);
	if (_video_driver)
		_video_driver->stop();

	assert(0);
	exit(1);
}

void CDECL ShowInfoF(const char *str, ...)
{
	va_list va;
	char buf[1024];
	va_start(va, str);
	vsprintf(buf, str, va);
	va_end(va);
	ShowInfo(buf);
}

char * CDECL str_fmt(const char *str, ...)
{
	char buf[4096];
	va_list va;
	int len;
	char *p;

	va_start(va, str);
	len = vsprintf(buf, str, va);
	va_end(va);
	p = malloc(len + 1);
	if (p)
		memcpy(p, buf, len + 1);
	return p;
}


// NULL midi driver
static const char *NullMidiStart(const char * const *parm) { return NULL; }
static void NullMidiStop(void) {}
static void NullMidiPlaySong(const char *filename) {}
static void NullMidiStopSong(void) {}
static bool NullMidiIsSongPlaying(void) { return true; }
static void NullMidiSetVolume(byte vol) {}

const HalMusicDriver _null_music_driver = {
	NullMidiStart,
	NullMidiStop,
	NullMidiPlaySong,
	NullMidiStopSong,
	NullMidiIsSongPlaying,
	NullMidiSetVolume,
};

// NULL video driver
static void *_null_video_mem;
static const char *NullVideoStart(const char * const *parm)
{
	_screen.width = _screen.pitch = _cur_resolution[0];
	_screen.height = _cur_resolution[1];
	_null_video_mem = malloc(_cur_resolution[0]*_cur_resolution[1]);
	return NULL;
}
static void NullVideoStop(void) { free(_null_video_mem); }
static void NullVideoMakeDirty(int left, int top, int width, int height) {}
static int NullVideoMainLoop(void)
{
	int i = 1000;
	do {
		GameLoop();
		_screen.dst_ptr = _null_video_mem;
		UpdateWindows();
	}	while (--i);
	return ML_QUIT;
}

static bool NullVideoChangeRes(int w, int h) { return false; }
static void NullVideoFullScreen(bool fs) {}

const HalVideoDriver _null_video_driver = {
	NullVideoStart,
	NullVideoStop,
	NullVideoMakeDirty,
	NullVideoMainLoop,
	NullVideoChangeRes,
	NullVideoFullScreen,
};

// NULL sound driver
static const char *NullSoundStart(const char * const *parm) { return NULL; }
static void NullSoundStop(void) {}
const HalSoundDriver _null_sound_driver = {
	NullSoundStart,
	NullSoundStop,
};

enum {
	DF_PRIORITY_MASK = 0xf,
};

typedef struct {
	const DriverDesc *descs;
	const char *name;
	void *var;
} DriverClass;

static DriverClass _driver_classes[] = {
	{_video_driver_descs, "video", &_video_driver},
	{_sound_driver_descs, "sound", &_sound_driver},
	{_music_driver_descs, "music", &_music_driver},
};

static const DriverDesc *GetDriverByName(const DriverDesc *dd, const char *name)
{
	do {
		if (!strcmp(dd->name, name))
			return dd;
	} while ((++dd)->name);
	return NULL;
}

static const DriverDesc *ChooseDefaultDriver(const DriverDesc *dd)
{
	const DriverDesc *best = NULL;
	int best_pri = -1;
	do {
		if ((int)(dd->flags&DF_PRIORITY_MASK) > best_pri && _os_version >= (byte)dd->flags) {
			best_pri = dd->flags&DF_PRIORITY_MASK;
			best = dd;
		}
	} while ((++dd)->name);
	return best;
}


void *ReadFileToMem(const char *filename, size_t *lenp, size_t maxsize)
{
	FILE *in;
	byte *mem;
	size_t len;

	in = fopen(filename, "rb");
	if (in == NULL)
		return NULL;

	fseek(in, 0, SEEK_END);
	len = ftell(in);
	fseek(in, 0, SEEK_SET);
	if (len > maxsize || (mem = malloc(len + 1)) == NULL) {
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

void LoadDriver(int driver, const char *name)
{
	const DriverClass *dc = &_driver_classes[driver];
	const DriverDesc *dd;
	const void **var;
	const void *drv;
	const char *err;
	char *parm;
	char buffer[256];
	const char *parms[32];

	parms[0] = NULL;

	if (!*name) {
		dd = ChooseDefaultDriver(dc->descs);
	} else {
		// Extract the driver name and put parameter list in parm
		ttd_strlcpy(buffer, name, sizeof(buffer));
		parm = strchr(buffer, ':');
		if (parm) {
			uint np = 0;
			// Tokenize the parm.
			do {
				*parm++ = 0;
				if (np < lengthof(parms) - 1)
					parms[np++] = parm;
				while (*parm != 0 && *parm != ',')
					parm++;
			} while (*parm == ',');
			parms[np] = NULL;
		}
		dd = GetDriverByName(dc->descs, buffer);
		if (dd == NULL)
			error("No such %s driver: %s\n", dc->name, buffer);
	}
	var = dc->var;
	if (*var != NULL) ((const HalCommonDriver*)*var)->stop();
	*var = NULL;
	drv = dd->drv;
	if ((err=((const HalCommonDriver*)drv)->start(parms)) != NULL)
		error("Unable to load driver %s(%s). The error was: %s\n", dd->name, dd->longname, err);
	*var = drv;
}

static void showhelp(void)
{
	char buf[4096], *p;
	const DriverClass *dc = _driver_classes;
	const DriverDesc *dd;
	int i;

	p = strecpy(buf,
		"Command line options:\n"
		"  -v drv              = Set video driver (see below)\n"
		"  -s drv              = Set sound driver (see below)\n"
		"  -m drv              = Set music driver (see below)\n"
		"  -r res              = Set resolution (for instance 800x600)\n"
		"  -h                  = Display this help text\n"
		"  -t date             = Set starting date\n"
		"  -d [[fac=]lvl[,...]]= Debug mode\n"
		"  -l lng              = Select Language\n"
		"  -e                  = Start Editor\n"
		"  -g [savegame]       = Start new/save game immediately\n"
		"  -G seed             = Set random seed\n"
		"  -n [ip#player:port] = Start networkgame\n"
		"  -D                  = Start dedicated server\n"
		#if !defined(__MORPHOS__) && !defined(__AMIGA__)
		"  -f                  = Fork into the background (dedicated only)\n"
		#endif
		"  -i                  = Force to use the DOS palette (use this if you see a lot of pink)\n"
		"  -p #player          = Player as #player (deprecated) (network only)\n"
		"  -c config_file      = Use 'config_file' instead of 'openttd.cfg'\n",
		lastof(buf)
	);

	for(i=0; i!=lengthof(_driver_classes); i++,dc++) {
		p += sprintf(p, "List of %s drivers:\n", dc->name);
		dd = dc->descs;
		do {
			p += sprintf(p, "%10s: %s\n", dd->name, dd->longname);
		} while ((++dd)->name);
	}

	ShowInfo(buf);
}


const char *GetDriverParam(const char * const *parm, const char *name)
{
	const char *p;
	int len = strlen(name);
	while ((p = *parm++) != NULL) {
		if (!strncmp(p,name,len)) {
			if (p[len] == '=') return p + len + 1;
			if (p[len] == 0)   return p + len;
		}
	}
	return NULL;
}

bool GetDriverParamBool(const char * const *parm, const char *name)
{
	const char *p = GetDriverParam(parm, name);
	return p != NULL;
}

int GetDriverParamInt(const char * const *parm, const char *name, int def)
{
	const char *p = GetDriverParam(parm, name);
	return p != NULL ? atoi(p) : def;
}

typedef struct {
	char *opt;
	int numleft;
	char **argv;
	const char *options;
	char *cont;
} MyGetOptData;

static void MyGetOptInit(MyGetOptData *md, int argc, char **argv, const char *options)
{
	md->cont = NULL;
	md->numleft = argc;
	md->argv = argv;
	md->options = options;
}

static int MyGetOpt(MyGetOptData *md)
{
	char *s,*r,*t;

	if ((s=md->cont) != NULL)
		goto md_continue_here;

	while(true) {
		if (--md->numleft < 0)
			return -1;

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
					md->opt = t;
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


static void ParseResolution(int res[2], char *s)
{
	char *t = strchr(s, 'x');
	if (t == NULL) {
		ShowInfoF("Invalid resolution '%s'", s);
		return;
	}

	res[0] = clamp(strtoul(s, NULL, 0), 64, MAX_SCREEN_WIDTH);
	res[1] = clamp(strtoul(t + 1, NULL, 0), 64, MAX_SCREEN_HEIGHT);
}

static void InitializeDynamicVariables(void)
{
	/* Dynamic stuff needs to be initialized somewhere... */
	_station_sort  = NULL;
	_vehicle_sort  = NULL;
	_town_sort     = NULL;
	_industry_sort = NULL;
}

static void UnInitializeDynamicVariables(void)
{
	/* Dynamic stuff needs to be free'd somewhere... */
	CleanPool(&_town_pool);
	CleanPool(&_industry_pool);
	CleanPool(&_station_pool);
	CleanPool(&_vehicle_pool);
	CleanPool(&_sign_pool);
	CleanPool(&_order_pool);

	free(_station_sort);
	free(_vehicle_sort);
	free(_town_sort);
	free(_industry_sort);
}

static void UnInitializeGame(void)
{
	UnInitWindowSystem();
	UnInitNewgrEngines();

	free(_config_file);
}

static void LoadIntroGame(void)
{
	char filename[256];

	_game_mode = GM_MENU;
	CLRBITS(_display_opt, DO_TRANS_BUILDINGS); // don't make buildings transparent in intro
	_opt_ptr = &_opt_newgame;

	GfxLoadSprites();
	LoadStringWidthTable();

	// Setup main window
	ResetWindowSystem();
	SetupColorsAndInitialWindow();

	// Generate a world.
	sprintf(filename, "%sopntitle.dat",  _path.data_dir);
	if (SaveOrLoad(filename, SL_LOAD) != SL_OK) {
#if defined SECOND_DATA_DIR
		sprintf(filename, "%sopntitle.dat",  _path.second_data_dir);
		if (SaveOrLoad(filename, SL_LOAD) != SL_OK)
#endif
			GenerateWorld(1, 6, 6); // if failed loading, make empty world.
	}

	_pause = 0;
	_local_player = 0;
	MarkWholeScreenDirty();

	// Play main theme
	if (_music_driver->is_song_playing()) ResetMusic();
}

extern void DedicatedFork(void);
extern void CheckExternalFiles(void);

int ttd_main(int argc, char* argv[])
{
	MyGetOptData mgo;
	int i;
	bool network = false;
	char *network_conn = NULL;
	char *language = NULL;
	const char *optformat;
	char musicdriver[16], sounddriver[16], videodriver[16];
	int resolution[2] = {0,0};
	uint startdate = -1;
	bool dedicated;

	musicdriver[0] = sounddriver[0] = videodriver[0] = 0;

	_game_mode = GM_MENU;
	_switch_mode = SM_MENU;
	_switch_mode_errorstr = INVALID_STRING_ID;
	_dedicated_forks = false;
	dedicated = false;
	_config_file = NULL;

	// The last param of the following function means this:
	//   a letter means: it accepts that param (e.g.: -h)
	//   a ':' behind it means: it need a param (e.g.: -m<driver>)
	//   a '::' behind it means: it can optional have a param (e.g.: -d<debug>)
	#if !defined(__MORPHOS__) && !defined(__AMIGA__) && !defined(WIN32)
		optformat = "m:s:v:hDfn::l:eit:d::r:g::G:p:c:";
	#else
		optformat = "m:s:v:hDn::l:eit:d::r:g::G:p:c:"; // no fork option
	#endif

	MyGetOptInit(&mgo, argc-1, argv+1, optformat);
	while ((i = MyGetOpt(&mgo)) != -1) {
		switch(i) {
		case 'm': ttd_strlcpy(musicdriver, mgo.opt, sizeof(musicdriver)); break;
		case 's': ttd_strlcpy(sounddriver, mgo.opt, sizeof(sounddriver)); break;
		case 'v': ttd_strlcpy(videodriver, mgo.opt, sizeof(videodriver)); break;
		case 'D': {
				sprintf(musicdriver,"null");
				sprintf(sounddriver,"null");
				sprintf(videodriver,"dedicated");
				dedicated = true;
			} break;
		case 'f': {
				_dedicated_forks = true;
			}; break;
		case 'n': {
				network = true;
				if (mgo.opt)
					// Optional, you can give an IP
					network_conn = mgo.opt;
				else
					network_conn = NULL;
			} break;
		case 'r': ParseResolution(resolution, mgo.opt); break;
		case 'l': {
				language = mgo.opt;
			} break;
		case 't': {
				startdate = atoi(mgo.opt);
			} break;
		case 'd': {
#if defined(WIN32)
				CreateConsole();
#endif
				if (mgo.opt)
					SetDebugString(mgo.opt);
			} break;
		case 'e': _switch_mode = SM_EDITOR; break;
		case 'i': _use_dos_palette = true; break;
		case 'g':
			if (mgo.opt) {
				strcpy(_file_to_saveload.name, mgo.opt);
				_switch_mode = SM_LOAD;
			} else
				_switch_mode = SM_NEWGAME;
			break;
		case 'G':
			_random_seeds[0][0] = atoi(mgo.opt);
			break;
		case 'p': {
			int i = atoi(mgo.opt);
			// Play as an other player in network games
			if (IS_INT_INSIDE(i, 1, MAX_PLAYERS)) _network_playas = i;
			break;
		}
		case 'c':
			_config_file = strdup(mgo.opt);
			break;
		case -2:
 		case 'h':
			showhelp();
			return 0;
		}
	}

	DeterminePaths();
	CheckExternalFiles();

#ifdef UNIX
	// We must fork here, or we'll end up without some resources we need (like sockets)
	if (_dedicated_forks)
		DedicatedFork();
#endif

	LoadFromConfig();
	CheckConfig();
	LoadFromHighScore();

	// override config?
	if (musicdriver[0]) ttd_strlcpy(_ini_musicdriver, musicdriver, sizeof(_ini_musicdriver));
	if (sounddriver[0]) ttd_strlcpy(_ini_sounddriver, sounddriver, sizeof(_ini_sounddriver));
	if (videodriver[0]) ttd_strlcpy(_ini_videodriver, videodriver, sizeof(_ini_videodriver));
	if (resolution[0]) { _cur_resolution[0] = resolution[0]; _cur_resolution[1] = resolution[1]; }
	if (startdate != (uint)-1) _patches.starting_date = startdate;

	if (_dedicated_forks && !dedicated)
		_dedicated_forks = false;

	// enumerate language files
	InitializeLanguagePacks();

	// initialize screenshot formats
	InitializeScreenshotFormats();

	// initialize airport state machines
	InitializeAirports();

	/* initialize all variables that are allocated dynamically */
	InitializeDynamicVariables();

	// Sample catalogue
	DEBUG(misc, 1) ("Loading sound effects...");
	_os_version = GetOSVersion();
	MxInitialize(11025);
	SoundInitialize("sample.cat");

	// This must be done early, since functions use the InvalidateWindow* calls
	InitWindowSystem();

	GfxLoadSprites();
	LoadStringWidthTable();

	DEBUG(misc, 1) ("Loading drivers...");
	LoadDriver(SOUND_DRIVER, _ini_sounddriver);
	LoadDriver(MUSIC_DRIVER, _ini_musicdriver);
	LoadDriver(VIDEO_DRIVER, _ini_videodriver); // load video last, to prevent an empty window while sound and music loads
	_savegame_sort_order = 1; // default sorting of savegames is by date, newest first

#ifdef ENABLE_NETWORK
	// initialize network-core
	NetworkStartUp();
#endif /* ENABLE_NETWORK */

	_opt_ptr = &_opt_newgame;

	/* XXX - ugly hack, if diff_level is 9, it means we got no setting from the config file */
	if (_opt_newgame.diff_level == 9)
		SetDifficultyLevel(0, &_opt_newgame);

	// initialize the ingame console
	IConsoleInit();
	InitializeGUI();
	IConsoleCmdExec("exec scripts/autoexec.scr 0");

	InitPlayerRandoms();

	GenerateWorld(1, 6, 6); // Make the viewport initialization happy

#ifdef ENABLE_NETWORK
	if ((network) && (_network_available)) {
		if (network_conn != NULL) {
			const char *port = NULL;
			const char *player = NULL;
			uint16 rport;

			rport = NETWORK_DEFAULT_PORT;

			ParseConnectionString(&player, &port, network_conn);

			if (player != NULL) _network_playas = atoi(player);
			if (port != NULL) rport = atoi(port);

			LoadIntroGame();
			_switch_mode = SM_NONE;
			NetworkClientConnectGame(network_conn, rport);
		}
	}
#endif /* ENABLE_NETWORK */

	while (_video_driver->main_loop() == ML_SWITCHDRIVER) {}

	JoinOTTDThread();
	IConsoleFree();

#ifdef ENABLE_NETWORK
	if (_network_available) {
		// Shut down the network and close any open connections
		NetworkDisconnect();
		NetworkUDPClose();
		NetworkShutDown();
	}
#endif /* ENABLE_NETWORK */

	_video_driver->stop();
	_music_driver->stop();
	_sound_driver->stop();

	SaveToConfig();
	SaveToHighScore();

	// uninitialize airport state machines
	UnInitializeAirports();

	/* uninitialize variables that are allocated dynamic */
	UnInitializeDynamicVariables();

	/* Close all and any open filehandles */
	FioCloseAll();
	UnInitializeGame();

	return 0;
}

static void ShowScreenshotResult(bool b)
{
	if (b) {
		SetDParam(0, STR_SPEC_SCREENSHOT_NAME);
		ShowErrorMessage(INVALID_STRING_ID, STR_031B_SCREENSHOT_SUCCESSFULLY, 0, 0);
	} else {
		ShowErrorMessage(INVALID_STRING_ID, STR_031C_SCREENSHOT_FAILED, 0, 0);
	}

}

static void MakeNewGame(void)
{
	_game_mode = GM_NORMAL;

	// Copy in game options
	_opt_ptr = &_opt;
	memcpy(_opt_ptr, &_opt_newgame, sizeof(GameOptions));

	GfxLoadSprites();

	// Reinitialize windows
	ResetWindowSystem();
	LoadStringWidthTable();

	SetupColorsAndInitialWindow();

	// Randomize world
	GenerateWorld(0, _patches.map_x, _patches.map_y);

	// In a dedicated server, the server does not play
	if (_network_dedicated) {
		_local_player = OWNER_SPECTATOR;
	}	else {
		// Create a single player
		DoStartupNewPlayer(false);

		_local_player = 0;
	}

	MarkWholeScreenDirty();
}

static void MakeNewEditorWorld(void)
{
	_game_mode = GM_EDITOR;

	// Copy in game options
	_opt_ptr = &_opt;
	memcpy(_opt_ptr, &_opt_newgame, sizeof(GameOptions));

	GfxLoadSprites();

	// Re-init the windowing system
	ResetWindowSystem();

	// Create toolbars
	SetupColorsAndInitialWindow();

	// Startup the game system
	GenerateWorld(1, _patches.map_x, _patches.map_y);

	_local_player = OWNER_NONE;
	MarkWholeScreenDirty();
}

void StartupPlayers(void);
void StartupDisasters(void);

/**
 * Start Scenario starts a new game based on a scenario.
 * Eg 'New Game' --> select a preset scenario
 * This starts a scenario based on your current difficulty settings
 */
static void StartScenario(void)
{
	_game_mode = GM_NORMAL;

	// invalid type
	if (_file_to_saveload.mode == SL_INVALID) {
		printf("Savegame is obsolete or invalid format: %s\n", _file_to_saveload.name);
		ShowErrorMessage(_error_message, STR_4009_GAME_LOAD_FAILED, 0, 0);
		_game_mode = GM_MENU;
		return;
	}

	GfxLoadSprites();

	// Reinitialize windows
	ResetWindowSystem();
	LoadStringWidthTable();

	SetupColorsAndInitialWindow();

	// Load game
	if (SaveOrLoad(_file_to_saveload.name, _file_to_saveload.mode) != SL_OK) {
		LoadIntroGame();
		ShowErrorMessage(_error_message, STR_4009_GAME_LOAD_FAILED, 0, 0);
	}

	_opt_ptr = &_opt;
	memcpy(&_opt_ptr->diff, &_opt_newgame.diff, sizeof(GameDifficulty));
	_opt.diff_level = _opt_newgame.diff_level;

	// Inititalize data
	StartupPlayers();
	StartupEngines();
	StartupDisasters();

	_local_player = 0;

	MarkWholeScreenDirty();
}

bool SafeSaveOrLoad(const char *filename, int mode, int newgm)
{
	byte ogm = _game_mode;
	int r;

	_game_mode = newgm;
	r = SaveOrLoad(filename, mode);
	if (r == SL_REINIT) {
		if (ogm == GM_MENU)
			LoadIntroGame();
		else if (ogm == GM_EDITOR)
			MakeNewEditorWorld();
		else
			MakeNewGame();
		return false;
	} else if (r != SL_OK) {
		_game_mode = ogm;
		return false;
	} else
		return true;
}

void SwitchMode(int new_mode)
{
	_in_state_game_loop = true;

#ifdef ENABLE_NETWORK
	// If we are saving something, the network stays in his current state
	if (new_mode != SM_SAVE) {
		// If the network is active, make it not-active
		if (_networking) {
			if (_network_server && (new_mode == SM_LOAD || new_mode == SM_NEWGAME)) {
				NetworkReboot();
				NetworkUDPClose();
			} else {
				NetworkDisconnect();
				NetworkUDPClose();
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
		if (_network_server)
			snprintf(_network_game_info.map_name, 40, "Random");
#endif /* ENABLE_NETWORK */
		MakeNewGame();
		break;

	case SM_START_SCENARIO: /* New Game --> Choose one of the preset scenarios */
		StartScenario();
		break;

	case SM_LOAD: { /* Load game, Play Scenario */
		_opt_ptr = &_opt;

		_error_message = INVALID_STRING_ID;
		if (!SafeSaveOrLoad(_file_to_saveload.name, _file_to_saveload.mode, GM_NORMAL)) {
			LoadIntroGame();
			ShowErrorMessage(_error_message, STR_4009_GAME_LOAD_FAILED, 0, 0);
		} else {
			_local_player = 0;
			DoCommandP(0, 0, 0, NULL, CMD_PAUSE); // decrease pause counter (was increased from opening load dialog)
#ifdef ENABLE_NETWORK
			if (_network_server)
				snprintf(_network_game_info.map_name, 40, "Loaded game");
#endif /* ENABLE_NETWORK */
		}
		break;
	}

	case SM_LOAD_SCENARIO: { /* Load scenario from scenario editor */
		int i;

		if (SafeSaveOrLoad(_file_to_saveload.name, _file_to_saveload.mode, GM_EDITOR)) {
			_opt_ptr = &_opt;

			_local_player = OWNER_NONE;
			_generating_world = true;
			// delete all players.
			for (i = 0; i != MAX_PLAYERS; i++) {
				ChangeOwnershipOfPlayerItems(i, 0xff);
				_players[i].is_active = false;
			}
			_generating_world = false;
			// delete all stations owned by a player
			DeleteAllPlayerStations();

#ifdef ENABLE_NETWORK
			if (_network_server)
				snprintf(_network_game_info.map_name, 40, "Loaded scenario");
#endif /* ENABLE_NETWORK */
		} else
			ShowErrorMessage(INVALID_STRING_ID, STR_4009_GAME_LOAD_FAILED, 0, 0);

		break;
	}


	case SM_MENU: /* Switch to game intro menu */
		LoadIntroGame();
		break;

	case SM_SAVE: /* Save game */
		if (SaveOrLoad(_file_to_saveload.name, SL_SAVE) != SL_OK)
			ShowErrorMessage(INVALID_STRING_ID, STR_4007_GAME_SAVE_FAILED, 0, 0);
		else
			DeleteWindowById(WC_SAVELOAD, 0);
		break;

	case SM_GENRANDLAND: /* Generate random land within scenario editor */
		GenerateWorld(2, _patches.map_x, _patches.map_y);
		// XXX: set date
		_local_player = OWNER_NONE;
		MarkWholeScreenDirty();
		break;
	}

	if (_switch_mode_errorstr != INVALID_STRING_ID)
		ShowErrorMessage(INVALID_STRING_ID,_switch_mode_errorstr,0,0);

	_in_state_game_loop = false;
}


// State controlling game loop.
// The state must not be changed from anywhere
// but here.
// That check is enforced in DoCommand.
void StateGameLoop(void)
{
	// dont execute the state loop during pause
	if (_pause) return;

	_in_state_game_loop = true;
	// _frame_counter is increased somewhere else when in network-mode
	//  Sidenote: _frame_counter is ONLY used for _savedump in non-MP-games
	//    Should that not be deleted? If so, the next 2 lines can also be deleted
	if (!_networking)
		_frame_counter++;

	if (_savedump_path[0] && (uint)_frame_counter >= _savedump_first && (uint)(_frame_counter -_savedump_first) % _savedump_freq == 0 ) {
		char buf[100];
		sprintf(buf, "%s%.5d.sav", _savedump_path, _frame_counter);
		SaveOrLoad(buf, SL_SAVE);
		if ((uint)_frame_counter >= _savedump_last) exit(1);
	}

	if (_game_mode == GM_EDITOR) {
		RunTileLoop();
		CallVehicleTicks();
		CallLandscapeTick();
		CallWindowTickEvent();
		NewsLoop();
	} else {
		// All these actions has to be done from OWNER_NONE
		//  for multiplayer compatibility
		uint p = _current_player;
		_current_player = OWNER_NONE;

		AnimateAnimatedTiles();
		IncreaseDate();
		RunTileLoop();
		CallVehicleTicks();
		CallLandscapeTick();

		// To bad the AI does not work in multiplayer, because states are not saved
		//  perfectly
		if (!disable_computer && !_networking)
			RunOtherPlayersLoop();

		CallWindowTickEvent();
		NewsLoop();
		_current_player = p;
	}

	_in_state_game_loop = false;
}

static void DoAutosave(void)
{
	char buf[200];

	if (_patches.keep_all_autosave && _local_player != OWNER_SPECTATOR) {
		const Player *p = GetPlayer(_local_player);
		char *s;
		sprintf(buf, "%s%s", _path.autosave_dir, PATHSEP);

		SetDParam(0, p->name_1);
		SetDParam(1, p->name_2);
		SetDParam(2, _date);
		s = (char*)GetString(buf + strlen(_path.autosave_dir) + strlen(PATHSEP), STR_4004);
		strcpy(s, ".sav");
	} else { /* Save a maximum of 15 autosaves */
		int n = _autosave_ctr;
		_autosave_ctr = (_autosave_ctr + 1) & 15;
		sprintf(buf, "%s%sautosave%d.sav", _path.autosave_dir, PATHSEP, n);
	}

	DEBUG(misc, 2) ("Autosaving to %s", buf);
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
	{ 0, 0},
	{-2, 0}, // 1:left
	{ 0,-2}, // 2:up
	{-2,-1}, // 3:left + up
	{ 2, 0}, // 4:right
	{ 0, 0}, // 5:left + right
	{ 2,-1}, // 6:right + up
	{ 0,-2}, // 7:left + right + up = up
	{ 0 ,2}, // 8:down
	{-2 ,1}, // 9:down+left
	{ 0, 0}, // 10:impossible
	{-2, 0}, // 11:left + up + down = left
	{ 2, 1}, // 12:down+right
	{ 0, 2}, // 13:left + right + down = down
	{ 0,-2}, // 14:left + right + up = up
	{ 0, 0}, // 15:impossible
};

static void HandleKeyScrolling(void)
{
	if (_dirkeys && !_no_scroll) {
		int factor = _shift_pressed ? 50 : 10;
		ScrollMainViewport(scrollamt[_dirkeys][0] * factor, scrollamt[_dirkeys][1] * factor);
	}
}

void GameLoop(void)
{
	int m;

	// autosave game?
	if (_do_autosave) {
		_do_autosave = false;
		DoAutosave();
		RedrawAutosave();
	}

	// handle scrolling of the main window
	if (_dirkeys) HandleKeyScrolling();

	// make a screenshot?
	if ((m=_make_screenshot) != 0) {
		_make_screenshot = 0;
		switch(m) {
		case 1: // make small screenshot
			UndrawMouseCursor();
			ShowScreenshotResult(MakeScreenshot());
			break;
		case 2: // make large screenshot
			ShowScreenshotResult(MakeWorldScreenshot(-(int)MapMaxX() * 32, 0, MapMaxX() * 64, MapSizeY() * 32, 0));
			break;
		}
	}

	// switch game mode?
	if ((m=_switch_mode) != SM_NONE) {
		_switch_mode = SM_NONE;
		SwitchMode(m);
	}

	IncreaseSpriteLRU();
	InteractiveRandom();

	if (_scroller_click_timeout > 3)
		_scroller_click_timeout -= 3;
	else
		_scroller_click_timeout = 0;

	_caret_timer += 3;
	_timer_counter+=8;
	CursorTick();

#ifdef ENABLE_NETWORK
	// Check for UDP stuff
	NetworkUDPGameLoop();

	if (_networking) {
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

	if (!_pause && _display_opt&DO_FULL_ANIMATION)
		DoPaletteAnimations();

	if (!_pause || _cheats.build_in_pause.value)
		MoveAllTextEffects();

	InputLoop();

	MusicLoop();
}

void BeforeSaveGame(void)
{
	Window *w = FindWindowById(WC_MAIN_WINDOW, 0);

	if (w != NULL) {
		_saved_scrollpos_x = WP(w,vp_d).scrollpos_x;
		_saved_scrollpos_y = WP(w,vp_d).scrollpos_y;
		_saved_scrollpos_zoom = w->viewport->zoom;
	}
}

static void ConvertTownOwner(void)
{
	TileIndex tile;

	for (tile = 0; tile != MapSize(); tile++) {
		if (IsTileType(tile, MP_STREET)) {
			if (IsLevelCrossing(tile) && _map3_lo[tile] & 0x80)
				_map3_lo[tile] = OWNER_TOWN;

			if (_map_owner[tile] & 0x80) SetTileOwner(tile, OWNER_TOWN);
		} else if (IsTileType(tile, MP_TUNNELBRIDGE)) {
			if (_map_owner[tile] & 0x80) SetTileOwner(tile, OWNER_TOWN);
		}
	}
}

// before savegame version 4, the name of the company determined if it existed
static void CheckIsPlayerActive(void)
{
	Player *p;
	FOR_ALL_PLAYERS(p) {
		if (p->name_1 != 0) {
			p->is_active = true;
		}
	}
}

// since savegame version 4.1, exclusive transport rights are stored at towns
static void UpdateExclusiveRights(void)
{
	Town *t;
	FOR_ALL_TOWNS(t) if (t->xy != 0) {
		t->exclusivity=(byte)-1;
	}

	/* FIXME old exclusive rights status is not being imported (stored in s->blocked_months_obsolete)
			could be implemented this way:
			1.) Go through all stations
					Build an array town_blocked[ town_id ][ player_id ]
				 that stores if at least one station in that town is blocked for a player
			2.) Go through that array, if you find a town that is not blocked for
				 	one player, but for all others, then give him exclusivity.
	*/
}

const byte convert_currency[] = {
	 0,  1, 12,  8,  3,
	10, 14, 19,  4,  5,
	 9, 11, 13,  6, 17,
	16, 22, 21,  7, 15,
	18,  2, 20, };

// since savegame version 4.2 the currencies are arranged differently
static void UpdateCurrencies(void)
{
	_opt.currency = convert_currency[_opt.currency];
}

/* Up to revision 1413 the invisible tiles at the southern border have not been
 * MP_VOID, even though they should have. This is fixed by this function
 */
static void UpdateVoidTiles(void)
{
	uint i;

	for (i = 0; i < MapMaxY(); ++i)
		SetTileType(i * MapSizeX() + MapMaxX(), MP_VOID);
	for (i = 0; i < MapSizeX(); ++i)
		SetTileType(MapSizeX() * MapMaxY() + i, MP_VOID);
}

// since savegame version 6.0 each sign has an "owner", signs without owner (from old games are set to 255)
static void UpdateSignOwner(void)
{
	SignStruct *ss;
	FOR_ALL_SIGNS(ss) {
		ss->owner = OWNER_NONE; // no owner
	}
}

extern void UpdateOldAircraft( void );
extern void UpdateOilRig( void );

bool AfterLoadGame(uint version)
{
	Window *w;
	ViewPort *vp;

	// in version 2.1 of the savegame, town owner was unified.
	if (version <= 0x200) {
		ConvertTownOwner();
	}

	// from version 4.1 of the savegame, exclusive rights are stored at towns
	if (version <= 0x400) {
		UpdateExclusiveRights();
	}

	// from version 4.2 of the savegame, currencies are in a different order
	if (version <= 0x401) {
		UpdateCurrencies();
	}

	// from version 6.0 of the savegame, signs have an "owner"
	if (version <= 0x600) {
		UpdateSignOwner();
	}

	/* In old version there seems to be a problem that water is owned by
	    OWNER_NONE, not OWNER_WATER.. I can't replicate it for the current
	    (0x402) version, so I just check when versions are older, and then
	    walk through the whole map.. */
	if (version <= 0x402) {
		TileIndex tile = TILE_XY(0,0);
		uint w = MapSizeX();
		uint h = MapSizeY();

		BEGIN_TILE_LOOP(tile_cur, w, h, tile)
			if (IsTileType(tile_cur, MP_WATER) && GetTileOwner(tile_cur) >= MAX_PLAYERS)
				SetTileOwner(tile_cur, OWNER_WATER);
		END_TILE_LOOP(tile_cur, w, h, tile)
	}

	// convert road side to my format.
	if (_opt.road_side) _opt.road_side = 1;

	// Load the sprites
	GfxLoadSprites();

	// Update current year
	SetDate(_date);

	// reinit the landscape variables (landscape might have changed)
	InitializeLandscapeVariables(true);

	// Update all vehicles
	AfterLoadVehicles();

	// Update all waypoints
	if (version < 0x0C00)
		FixOldWaypoints();

	UpdateAllWaypointSigns();

	// in version 2.2 of the savegame, we have new airports
	if (version <= 0x201) {
		UpdateOldAircraft();
	}

	UpdateAllStationVirtCoord();

	// Setup town coords
	AfterLoadTown();
	UpdateAllSignVirtCoords();

	// make sure there is a town in the game
	if (_game_mode == GM_NORMAL && !ClosestTownFromTile(0, (uint)-1))
	{
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


	// in version 4.0 of the savegame, is_active was introduced to determine
	// if a player does exist, rather then checking name_1
	if (version <= 0x400) {
		CheckIsPlayerActive();
	}

	// the void tiles on the southern border used to belong to a wrong class.
	if (version <= 0x402)
		UpdateVoidTiles();

	// If Load Scenario / New (Scenario) Game is used,
	//  a player does not exist yet. So create one here.
	// 1 exeption: network-games. Those can have 0 players
	//   But this exeption is not true for network_servers!
	if (!_players[0].is_active && (!_networking || (_networking && _network_server)))
		DoStartupNewPlayer(false);

	DoZoomInOutWindow(ZOOM_NONE, w); // update button status
	MarkWholeScreenDirty();

	//In 5.1, Oilrigs have been moved (again)
	if (version <= 0x500) {
		UpdateOilRig();
	}

	if (version <= 0x600) {
		BEGIN_TILE_LOOP(tile, MapSizeX(), MapSizeY(), 0) {
			if (IsTileType(tile, MP_HOUSE)) {
				_map3_hi[tile] = _map2[tile];
				//XXX magic
				SetTileType(tile, MP_VOID);
				_map2[tile] = ClosestTownFromTile(tile,(uint)-1)->index;
				SetTileType(tile, MP_HOUSE);
			} else if (IsTileType(tile, MP_STREET)) {
				//XXX magic
				_map3_hi[tile] |= (_map2[tile] << 4);
				if (IsTileOwner(tile, OWNER_TOWN)) {
					SetTileType(tile, MP_VOID);
					_map2[tile] = ClosestTownFromTile(tile,(uint)-1)->index;
					SetTileType(tile, MP_STREET);
				} else {
					_map2[tile] = 0;
				}
			}
		} END_TILE_LOOP(tile, MapSizeX(), MapSizeY(), 0);
	}

	if (version < 0x900) {
		Town *t;
		FOR_ALL_TOWNS(t) {
			UpdateTownMaxPass(t);
		}
	}

	return true;
}
