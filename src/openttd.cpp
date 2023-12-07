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
#include "mixer.h"

#include "fontcache.h"
#include "error.h"
#include "error_func.h"
#include "gui.h"

#include "base_media_base.h"
#include "saveload/saveload.h"
#include "company_cmd.h"
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
#include "ai/ai.hpp"
#include "ai/ai_config.hpp"
#include "settings_func.h"
#include "genworld.h"
#include "progress.h"
#include "strings_func.h"
#include "vehicle_func.h"
#include "gamelog.h"
#include "animated_tile_func.h"
#include "roadstop_base.h"
#include "elrail_func.h"
#include "rev.h"
#include "highscore.h"
#include "station_base.h"
#include "crashlog.h"
#include "engine_func.h"
#include "core/random_func.hpp"
#include "rail_gui.h"
#include "road_gui.h"
#include "core/backup_type.hpp"
#include "hotkeys.h"
#include "newgrf.h"
#include "misc/getoptdata.h"
#include "game/game.hpp"
#include "game/game_config.hpp"
#include "town.h"
#include "subsidy_func.h"
#include "gfx_layout.h"
#include "viewport_func.h"
#include "viewport_sprite_sorter.h"
#include "framerate_type.h"
#include "industry.h"
#include "network/network_gui.h"
#include "network/network_survey.h"
#include "misc_cmd.h"
#include "timer/timer.h"
#include "timer/timer_game_calendar.h"
#include "timer/timer_game_realtime.h"
#include "timer/timer_game_tick.h"

#include "linkgraph/linkgraphschedule.h"

#include <system_error>

#include "safeguards.h"

#ifdef __EMSCRIPTEN__
#	include <emscripten.h>
#	include <emscripten/html5.h>
#endif

void CallLandscapeTick();
void DoPaletteAnimations();
void MusicLoop();
void CallWindowGameTickEvent();
bool HandleBootstrap();

extern void AfterLoadCompanyStats();
extern Company *DoStartupNewCompany(bool is_ai, CompanyID company = INVALID_COMPANY);
extern void OSOpenBrowser(const std::string &url);
extern void RebuildTownCaches();
extern void ShowOSErrorBox(const char *buf, bool system);
extern std::string _config_file;

bool _save_config = false;
bool _request_newgrf_scan = false;
NewGRFScanCallback *_request_newgrf_scan_callback = nullptr;

/**
 * Error handling for fatal user errors.
 * @param str the string to print.
 * @note Does NEVER return.
 */
void UserErrorI(const std::string &str)
{
	ShowOSErrorBox(str.c_str(), false);
	if (VideoDriver::GetInstance() != nullptr) VideoDriver::GetInstance()->Stop();

#ifdef __EMSCRIPTEN__
	emscripten_exit_pointerlock();
	/* In effect, the game ends here. As emscripten_set_main_loop() caused
	 * the stack to be unwound, the code after MainLoop() in
	 * openttd_main() is never executed. */
	EM_ASM(if (window["openttd_abort"]) openttd_abort());
#endif

	_exit(1);
}

/**
 * Error handling for fatal non-user errors.
 * @param str the string to print.
 * @note Does NEVER return.
 */
void FatalErrorI(const std::string &str)
{
	if (VideoDriver::GetInstance() == nullptr || VideoDriver::GetInstance()->HasGUI()) {
		ShowOSErrorBox(str.c_str(), true);
	}

	/* Set the error message for the crash log and then invoke it. */
	CrashLog::SetErrorMessage(str);
	abort();
}

/**
 * Show the help message when someone passed a wrong parameter.
 */
static void ShowHelp()
{
	std::string str;
	str.reserve(8192);

	std::back_insert_iterator<std::string> output_iterator = std::back_inserter(str);
	fmt::format_to(output_iterator, "OpenTTD {}\n", _openttd_revision);
	str +=
		"\n"
		"\n"
		"Command line options:\n"
		"  -v drv              = Set video driver (see below)\n"
		"  -s drv              = Set sound driver (see below)\n"
		"  -m drv              = Set music driver (see below)\n"
		"  -b drv              = Set the blitter to use (see below)\n"
		"  -r res              = Set resolution (for instance 800x600)\n"
		"  -h                  = Display this help text\n"
		"  -t year             = Set starting year\n"
		"  -d [[fac=]lvl[,...]]= Debug mode\n"
		"  -e                  = Start Editor\n"
		"  -g [savegame]       = Start new/save game immediately\n"
		"  -G seed             = Set random seed\n"
		"  -n host[:port][#company]= Join network game\n"
		"  -p password         = Password to join server\n"
		"  -P password         = Password to join company\n"
		"  -D [host][:port]    = Start dedicated server\n"
		"  -l host[:port]      = Redirect Debug()\n"
#if !defined(_WIN32)
		"  -f                  = Fork into the background (dedicated only)\n"
#endif
		"  -I graphics_set     = Force the graphics set (see below)\n"
		"  -S sounds_set       = Force the sounds set (see below)\n"
		"  -M music_set        = Force the music set (see below)\n"
		"  -c config_file      = Use 'config_file' instead of 'openttd.cfg'\n"
		"  -x                  = Never save configuration changes to disk\n"
		"  -X                  = Don't use global folders to search for files\n"
		"  -q savegame         = Write some information about the savegame and exit\n"
		"  -Q                  = Don't scan for/load NewGRF files on startup\n"
		"  -QQ                 = Disable NewGRF scanning/loading entirely\n"
		"\n";

	/* List the graphics packs */
	BaseGraphics::GetSetsList(output_iterator);

	/* List the sounds packs */
	BaseSounds::GetSetsList(output_iterator);

	/* List the music packs */
	BaseMusic::GetSetsList(output_iterator);

	/* List the drivers */
	DriverFactoryBase::GetDriversInfo(output_iterator);

	/* List the blitters */
	BlitterFactory::GetBlittersInfo(output_iterator);

	/* List the debug facilities. */
	DumpDebugFacilityNames(output_iterator);

	/* We need to initialize the AI, so it finds the AIs */
	AI::Initialize();
	AI::GetConsoleList(output_iterator, true);
	AI::Uninitialize(true);

	/* We need to initialize the GameScript, so it finds the GSs */
	Game::Initialize();
	Game::GetConsoleList(output_iterator, true);
	Game::Uninitialize(true);

	/* ShowInfo put output to stderr, but version information should go
	 * to stdout; this is the only exception */
#if !defined(_WIN32)
	fmt::print("{}\n", str);
#else
	ShowInfoI(str);
#endif
}

static void WriteSavegameInfo(const std::string &name)
{
	extern SaveLoadVersion _sl_version;
	uint32_t last_ottd_rev = 0;
	byte ever_modified = 0;
	bool removed_newgrfs = false;

	_gamelog.Info(&last_ottd_rev, &ever_modified, &removed_newgrfs);

	std::string message;
	message.reserve(1024);
	fmt::format_to(std::back_inserter(message), "Name:         {}\n", name);
	fmt::format_to(std::back_inserter(message), "Savegame ver: {}\n", _sl_version);
	fmt::format_to(std::back_inserter(message), "NewGRF ver:   0x{:08X}\n", last_ottd_rev);
	fmt::format_to(std::back_inserter(message), "Modified:     {}\n", ever_modified);

	if (removed_newgrfs) {
		fmt::format_to(std::back_inserter(message), "NewGRFs have been removed\n");
	}

	message += "NewGRFs:\n";
	if (_load_check_data.HasNewGrfs()) {
		for (GRFConfig *c = _load_check_data.grfconfig; c != nullptr; c = c->next) {
			fmt::format_to(std::back_inserter(message), "{:08X} {} {}\n", c->ident.grfid,
				FormatArrayAsHex(HasBit(c->flags, GCF_COMPATIBLE) ? c->original_md5sum : c->ident.md5sum), c->filename);
		}
	}

	/* ShowInfo put output to stderr, but version information should go
	 * to stdout; this is the only exception */
#if !defined(_WIN32)
	fmt::print("{}\n", message);
#else
	ShowInfoI(message);
#endif
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
	if (t == nullptr) {
		ShowInfo("Invalid resolution '{}'", s);
		return;
	}

	res->width  = std::max(std::strtoul(s, nullptr, 0), 64UL);
	res->height = std::max(std::strtoul(t + 1, nullptr, 0), 64UL);
}


/**
 * Uninitializes drivers, frees allocated memory, cleans pools, ...
 * Generally, prepares the game for shutting down
 */
static void ShutdownGame()
{
	IConsoleFree();

	if (_network_available) NetworkShutDown(); // Shut down the network and close any open connections

	DriverFactoryBase::ShutdownDrivers();

	UnInitWindowSystem();

	/* stop the scripts */
	AI::Uninitialize(false);
	Game::Uninitialize(false);

	/* Uninitialize variables that are allocated dynamically */
	_gamelog.Reset();

	LinkGraphSchedule::Clear();
	PoolBase::Clean(PT_ALL);

	/* No NewGRFs were loaded when it was still bootstrapping. */
	if (_game_mode != GM_BOOTSTRAP) ResetNewGRFData();

	UninitFontCache();
}

/**
 * Load the introduction game.
 * @param load_newgrfs Whether to load the NewGRFs or not.
 */
static void LoadIntroGame(bool load_newgrfs = true)
{
	_game_mode = GM_MENU;

	if (load_newgrfs) ResetGRFConfig(false);

	/* Setup main window */
	ResetWindowSystem();
	SetupColoursAndInitialWindow();

	/* Load the default opening screen savegame */
	if (SaveOrLoad("opntitle.dat", SLO_LOAD, DFT_GAME_FILE, BASESET_DIR) != SL_OK) {
		GenerateWorld(GWM_EMPTY, 64, 64); // if failed loading, make empty world.
		SetLocalCompany(COMPANY_SPECTATOR);
	} else {
		SetLocalCompany(COMPANY_FIRST);
	}

	FixTitleGameZoom();
	_pause_mode = PM_UNPAUSED;
	_cursor.fix_at = false;

	CheckForMissingGlyphs();

	MusicLoop(); // ensure music is correct
}

void MakeNewgameSettingsLive()
{
	for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
		if (_settings_game.ai_config[c] != nullptr) {
			delete _settings_game.ai_config[c];
		}
	}
	if (_settings_game.game_config != nullptr) {
		delete _settings_game.game_config;
	}

	/* Copy newgame settings to active settings.
	 * Also initialise old settings needed for savegame conversion. */
	_settings_game = _settings_newgame;
	_old_vds = _settings_client.company.vehicle;

	for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
		_settings_game.ai_config[c] = nullptr;
		if (_settings_newgame.ai_config[c] != nullptr) {
			_settings_game.ai_config[c] = new AIConfig(_settings_newgame.ai_config[c]);
			if (!AIConfig::GetConfig(c, AIConfig::SSS_FORCE_GAME)->HasScript()) {
				AIConfig::GetConfig(c, AIConfig::SSS_FORCE_GAME)->Change(std::nullopt);
			}
		}
	}
	_settings_game.game_config = nullptr;
	if (_settings_newgame.game_config != nullptr) {
		_settings_game.game_config = new GameConfig(_settings_newgame.game_config);
	}
}

void OpenBrowser(const std::string &url)
{
	/* Make sure we only accept urls that are sure to open a browser. */
	if (StrStartsWith(url, "http://") || StrStartsWith(url, "https://")) {
		OSOpenBrowser(url);
	}
}

/** Callback structure of statements to be executed after the NewGRF scan. */
struct AfterNewGRFScan : NewGRFScanCallback {
	TimerGameCalendar::Year startyear = CalendarTime::INVALID_YEAR; ///< The start year.
	uint32_t generation_seed = GENERATE_NEW_SEED; ///< Seed for the new game.
	std::string dedicated_host;                 ///< Hostname for the dedicated server.
	uint16_t dedicated_port = 0;                  ///< Port for the dedicated server.
	std::string connection_string;              ///< Information about the server to connect to
	std::string join_server_password;           ///< The password to join the server with.
	std::string join_company_password;          ///< The password to join the company with.
	bool save_config = true;                    ///< The save config setting.

	/**
	 * Create a new callback.
	 */
	AfterNewGRFScan()
	{
		/* Visual C++ 2015 fails compiling this line (AfterNewGRFScan::generation_seed undefined symbol)
		 * if it's placed outside a member function, directly in the struct body. */
		static_assert(sizeof(generation_seed) == sizeof(_settings_game.game_creation.generation_seed));
	}

	void OnNewGRFsScanned() override
	{
		ResetGRFConfig(false);

		TarScanner::DoScan(TarScanner::SCENARIO);

		AI::Initialize();
		Game::Initialize();

		/* We want the new (correct) NewGRF count to survive the loading. */
		uint last_newgrf_count = _settings_client.gui.last_newgrf_count;
		LoadFromConfig();
		_settings_client.gui.last_newgrf_count = last_newgrf_count;
		/* Since the default for the palette might have changed due to
		 * reading the configuration file, recalculate that now. */
		UpdateNewGRFConfigPalette();

		Game::Uninitialize(true);
		AI::Uninitialize(true);
		LoadFromHighScore();
		LoadHotkeysFromConfig();
		WindowDesc::LoadFromConfig();

		/* We have loaded the config, so we may possibly save it. */
		_save_config = save_config;

		/* restore saved music and effects volumes */
		MusicDriver::GetInstance()->SetVolume(_settings_client.music.music_vol);
		SetEffectVolume(_settings_client.music.effect_vol);

		if (startyear != CalendarTime::INVALID_YEAR) IConsoleSetSetting("game_creation.starting_year", startyear.base());
		if (generation_seed != GENERATE_NEW_SEED) _settings_newgame.game_creation.generation_seed = generation_seed;

		if (!dedicated_host.empty()) {
			_network_bind_list.clear();
			_network_bind_list.emplace_back(dedicated_host);
		}
		if (dedicated_port != 0) _settings_client.network.server_port = dedicated_port;

		/* initialize the ingame console */
		IConsoleInit();
		InitializeGUI();
		IConsoleCmdExec("exec scripts/autoexec.scr 0");

		/* Make sure _settings is filled with _settings_newgame if we switch to a game directly */
		if (_switch_mode != SM_NONE) MakeNewgameSettingsLive();

		if (_network_available && !connection_string.empty()) {
			LoadIntroGame();
			_switch_mode = SM_NONE;

			NetworkClientConnectGame(connection_string, COMPANY_NEW_COMPANY, join_server_password, join_company_password);
		}

		/* After the scan we're not used anymore. */
		delete this;
	}
};

void PostMainLoop()
{
	WaitTillSaved();

	/* only save config if we have to */
	if (_save_config) {
		SaveToConfig();
		SaveHotkeysToConfig();
		WindowDesc::SaveToConfig();
		SaveToHighScore();
	}

	/* Reset windowing system, stop drivers, free used memory, ... */
	ShutdownGame();
}

#if defined(UNIX)
extern void DedicatedFork();
#endif

/** Options of OpenTTD. */
static const OptionData _options[] = {
	 GETOPT_SHORT_VALUE('I'),
	 GETOPT_SHORT_VALUE('S'),
	 GETOPT_SHORT_VALUE('M'),
	 GETOPT_SHORT_VALUE('m'),
	 GETOPT_SHORT_VALUE('s'),
	 GETOPT_SHORT_VALUE('v'),
	 GETOPT_SHORT_VALUE('b'),
	GETOPT_SHORT_OPTVAL('D'),
	 GETOPT_SHORT_VALUE('n'),
	 GETOPT_SHORT_VALUE('l'),
	 GETOPT_SHORT_VALUE('p'),
	 GETOPT_SHORT_VALUE('P'),
#if !defined(_WIN32)
	 GETOPT_SHORT_NOVAL('f'),
#endif
	 GETOPT_SHORT_VALUE('r'),
	 GETOPT_SHORT_VALUE('t'),
	GETOPT_SHORT_OPTVAL('d'),
	 GETOPT_SHORT_NOVAL('e'),
	GETOPT_SHORT_OPTVAL('g'),
	 GETOPT_SHORT_VALUE('G'),
	 GETOPT_SHORT_VALUE('c'),
	 GETOPT_SHORT_NOVAL('x'),
	 GETOPT_SHORT_NOVAL('X'),
	 GETOPT_SHORT_VALUE('q'),
	 GETOPT_SHORT_NOVAL('h'),
	 GETOPT_SHORT_NOVAL('Q'),
	GETOPT_END()
};

/**
 * Main entry point for this lovely game.
 * @param argc The number of arguments passed to this game.
 * @param argv The values of the arguments.
 * @return 0 when there is no error.
 */
int openttd_main(int argc, char *argv[])
{
	std::string musicdriver;
	std::string sounddriver;
	std::string videodriver;
	std::string blitter;
	std::string graphics_set;
	std::string sounds_set;
	std::string music_set;
	Dimension resolution = {0, 0};
	std::unique_ptr<AfterNewGRFScan> scanner(new AfterNewGRFScan());
	bool dedicated = false;
	char *debuglog_conn = nullptr;
	bool only_local_path = false;

	extern bool _dedicated_forks;
	_dedicated_forks = false;

	_game_mode = GM_MENU;
	_switch_mode = SM_MENU;

	GetOptData mgo(argc - 1, argv + 1, _options);
	int ret = 0;

	int i;
	while ((i = mgo.GetOpt()) != -1) {
		switch (i) {
		case 'I': graphics_set = mgo.opt; break;
		case 'S': sounds_set = mgo.opt; break;
		case 'M': music_set = mgo.opt; break;
		case 'm': musicdriver = mgo.opt; break;
		case 's': sounddriver = mgo.opt; break;
		case 'v': videodriver = mgo.opt; break;
		case 'b': blitter = mgo.opt; break;
		case 'D':
			musicdriver = "null";
			sounddriver = "null";
			videodriver = "dedicated";
			blitter = "null";
			dedicated = true;
			SetDebugString("net=4", ShowInfoI);
			if (mgo.opt != nullptr) {
				scanner->dedicated_host = ParseFullConnectionString(mgo.opt, scanner->dedicated_port);
			}
			break;
		case 'f': _dedicated_forks = true; break;
		case 'n':
			scanner->connection_string = mgo.opt; // host:port#company parameter
			break;
		case 'l':
			debuglog_conn = mgo.opt;
			break;
		case 'p':
			scanner->join_server_password = mgo.opt;
			break;
		case 'P':
			scanner->join_company_password = mgo.opt;
			break;
		case 'r': ParseResolution(&resolution, mgo.opt); break;
		case 't': scanner->startyear = atoi(mgo.opt); break;
		case 'd': {
#if defined(_WIN32)
				CreateConsole();
#endif
				if (mgo.opt != nullptr) SetDebugString(mgo.opt, ShowInfoI);
				break;
			}
		case 'e': _switch_mode = (_switch_mode == SM_LOAD_GAME || _switch_mode == SM_LOAD_SCENARIO ? SM_LOAD_SCENARIO : SM_EDITOR); break;
		case 'g':
			if (mgo.opt != nullptr) {
				_file_to_saveload.name = mgo.opt;
				bool is_scenario = _switch_mode == SM_EDITOR || _switch_mode == SM_LOAD_SCENARIO;
				_switch_mode = is_scenario ? SM_LOAD_SCENARIO : SM_LOAD_GAME;
				_file_to_saveload.SetMode(SLO_LOAD, is_scenario ? FT_SCENARIO : FT_SAVEGAME, DFT_GAME_FILE);

				/* if the file doesn't exist or it is not a valid savegame, let the saveload code show an error */
				auto t = _file_to_saveload.name.find_last_of('.');
				if (t != std::string::npos) {
					auto [ft, _] = FiosGetSavegameListCallback(SLO_LOAD, _file_to_saveload.name, _file_to_saveload.name.substr(t));
					if (ft != FIOS_TYPE_INVALID) _file_to_saveload.SetMode(ft);
				}

				break;
			}

			_switch_mode = SM_NEWGAME;
			/* Give a random map if no seed has been given */
			if (scanner->generation_seed == GENERATE_NEW_SEED) {
				scanner->generation_seed = InteractiveRandom();
			}
			break;
		case 'q': {
			DeterminePaths(argv[0], only_local_path);
			if (StrEmpty(mgo.opt)) {
				ret = 1;
				return ret;
			}

			auto [_, title] = FiosGetSavegameListCallback(SLO_LOAD, mgo.opt, strrchr(mgo.opt, '.'));

			_load_check_data.Clear();
			SaveOrLoadResult res = SaveOrLoad(mgo.opt, SLO_CHECK, DFT_GAME_FILE, SAVE_DIR, false);
			if (res != SL_OK || _load_check_data.HasErrors()) {
				fmt::print(stderr, "Failed to open savegame\n");
				if (_load_check_data.HasErrors()) {
					InitializeLanguagePacks(); // A language pack is needed for GetString()
					SetDParamStr(0, _load_check_data.error_msg);
					fmt::print(stderr, "{}\n", GetString(_load_check_data.error));
				}
				return ret;
			}

			WriteSavegameInfo(title);
			return ret;
		}
		case 'Q': {
			extern int _skip_all_newgrf_scanning;
			_skip_all_newgrf_scanning += 1;
			break;
		}
		case 'G': scanner->generation_seed = std::strtoul(mgo.opt, nullptr, 10); break;
		case 'c': _config_file = mgo.opt; break;
		case 'x': scanner->save_config = false; break;
		case 'X': only_local_path = true; break;
		case 'h':
			i = -2; // Force printing of help.
			break;
		}
		if (i == -2) break;
	}

	if (i == -2 || mgo.numleft > 0) {
		/* Either the user typed '-h', they made an error, or they added unrecognized command line arguments.
		 * In all cases, print the help, and exit.
		 *
		 * The next two functions are needed to list the graphics sets. We can't do them earlier
		 * because then we cannot show it on the debug console as that hasn't been configured yet. */
		DeterminePaths(argv[0], only_local_path);
		TarScanner::DoScan(TarScanner::BASESET);
		BaseGraphics::FindSets();
		BaseSounds::FindSets();
		BaseMusic::FindSets();
		ShowHelp();
		return ret;
	}

	DeterminePaths(argv[0], only_local_path);
	TarScanner::DoScan(TarScanner::BASESET);

	if (dedicated) Debug(net, 3, "Starting dedicated server, version {}", _openttd_revision);
	if (_dedicated_forks && !dedicated) _dedicated_forks = false;

#if defined(UNIX)
	/* We must fork here, or we'll end up without some resources we need (like sockets) */
	if (_dedicated_forks) DedicatedFork();
#endif

	LoadFromConfig(true);

	if (resolution.width != 0) _cur_resolution = resolution;

	/* Limit width times height times bytes per pixel to fit a 32 bit
	 * integer, This way all internal drawing routines work correctly.
	 * A resolution that has one component as 0 is treated as a marker to
	 * auto-detect a good window size. */
	_cur_resolution.width  = std::min(_cur_resolution.width, UINT16_MAX / 2u);
	_cur_resolution.height = std::min(_cur_resolution.height, UINT16_MAX / 2u);

	/* Assume the cursor starts within the game as not all video drivers
	 * get an event that the cursor is within the window when it is opened.
	 * Saying the cursor is there makes no visible difference as it would
	 * just be out of the bounds of the window. */
	_cursor.in_window = true;

	/* enumerate language files */
	InitializeLanguagePacks();

	/* Initialize the font cache */
	InitFontCache(false);

	/* This must be done early, since functions use the SetWindowDirty* calls */
	InitWindowSystem();

	BaseGraphics::FindSets();
	bool valid_graphics_set;
	if (!graphics_set.empty()) {
		valid_graphics_set = BaseGraphics::SetSetByName(graphics_set);
	} else if (BaseGraphics::ini_data.shortname != 0) {
		graphics_set = BaseGraphics::ini_data.name;
		valid_graphics_set = BaseGraphics::SetSetByShortname(BaseGraphics::ini_data.shortname);
		if (valid_graphics_set && !BaseGraphics::ini_data.extra_params.empty()) {
			GRFConfig &extra_cfg = BaseGraphics::GetUsedSet()->GetOrCreateExtraConfig();
			if (extra_cfg.IsCompatible(BaseGraphics::ini_data.extra_version)) {
				extra_cfg.SetParams(BaseGraphics::ini_data.extra_params);
			}
		}
	} else if (!BaseGraphics::ini_data.name.empty()) {
		graphics_set = BaseGraphics::ini_data.name;
		valid_graphics_set = BaseGraphics::SetSetByName(BaseGraphics::ini_data.name);
	} else {
		valid_graphics_set = true;
		BaseGraphics::SetSet(nullptr); // ignore error, continue to bootstrap GUI
	}
	if (!valid_graphics_set) {
		BaseGraphics::SetSet(nullptr);

		ErrorMessageData msg(STR_CONFIG_ERROR, STR_CONFIG_ERROR_INVALID_BASE_GRAPHICS_NOT_FOUND);
		msg.SetDParamStr(0, graphics_set);
		ScheduleErrorMessage(msg);
	}

	/* Initialize game palette */
	GfxInitPalettes();

	Debug(misc, 1, "Loading blitter...");
	if (blitter.empty() && !_ini_blitter.empty()) blitter = _ini_blitter;
	_blitter_autodetected = blitter.empty();
	/* Activate the initial blitter.
	 * This is only some initial guess, after NewGRFs have been loaded SwitchNewGRFBlitter may switch to a different one.
	 *  - Never guess anything, if the user specified a blitter. (_blitter_autodetected)
	 *  - Use 32bpp blitter if baseset or 8bpp-support settings says so.
	 *  - Use 8bpp blitter otherwise.
	 */
	if (!_blitter_autodetected ||
			(_support8bpp != S8BPP_NONE && (BaseGraphics::GetUsedSet() == nullptr || BaseGraphics::GetUsedSet()->blitter == BLT_8BPP)) ||
			BlitterFactory::SelectBlitter("32bpp-anim") == nullptr) {
		if (BlitterFactory::SelectBlitter(blitter) == nullptr) {
			blitter.empty() ?
				UserError("Failed to autoprobe blitter") :
				UserError("Failed to select requested blitter '{}'; does it exist?", blitter);
		}
	}

	if (videodriver.empty() && !_ini_videodriver.empty()) videodriver = _ini_videodriver;
	DriverFactoryBase::SelectDriver(videodriver, Driver::DT_VIDEO);

	InitializeSpriteSorter();

	/* Initialize the zoom level of the screen to normal */
	_screen.zoom = ZOOM_LVL_NORMAL;

	/* The video driver is now selected, now initialise GUI zoom */
	AdjustGUIZoom(false);

	NetworkStartUp(); // initialize network-core

	if (debuglog_conn != nullptr && _network_available) {
		NetworkStartDebugLog(debuglog_conn);
	}

	if (!HandleBootstrap()) {
		ShutdownGame();
		return ret;
	}

	VideoDriver::GetInstance()->ClaimMousePointer();

	/* initialize screenshot formats */
	InitializeScreenshotFormats();

	BaseSounds::FindSets();
	if (sounds_set.empty() && !BaseSounds::ini_set.empty()) sounds_set = BaseSounds::ini_set;
	if (!BaseSounds::SetSetByName(sounds_set)) {
		if (sounds_set.empty() || !BaseSounds::SetSet({})) {
			UserError("Failed to find a sounds set. Please acquire a sounds set for OpenTTD. See section 1.4 of README.md.");
		} else {
			ErrorMessageData msg(STR_CONFIG_ERROR, STR_CONFIG_ERROR_INVALID_BASE_SOUNDS_NOT_FOUND);
			msg.SetDParamStr(0, sounds_set);
			ScheduleErrorMessage(msg);
		}
	}

	BaseMusic::FindSets();
	if (music_set.empty() && !BaseMusic::ini_set.empty()) music_set = BaseMusic::ini_set;
	if (!BaseMusic::SetSetByName(music_set)) {
		if (music_set.empty() || !BaseMusic::SetSet({})) {
			UserError("Failed to find a music set. Please acquire a music set for OpenTTD. See section 1.4 of README.md.");
		} else {
			ErrorMessageData msg(STR_CONFIG_ERROR, STR_CONFIG_ERROR_INVALID_BASE_MUSIC_NOT_FOUND);
			msg.SetDParamStr(0, music_set);
			ScheduleErrorMessage(msg);
		}
	}

	if (sounddriver.empty() && !_ini_sounddriver.empty()) sounddriver = _ini_sounddriver;
	DriverFactoryBase::SelectDriver(sounddriver, Driver::DT_SOUND);

	if (musicdriver.empty() && !_ini_musicdriver.empty()) musicdriver = _ini_musicdriver;
	DriverFactoryBase::SelectDriver(musicdriver, Driver::DT_MUSIC);

	GenerateWorld(GWM_EMPTY, 64, 64); // Make the viewport initialization happy
	LoadIntroGame(false);

	CheckForMissingGlyphs();

	/* ScanNewGRFFiles now has control over the scanner. */
	RequestNewGRFScan(scanner.release());

	VideoDriver::GetInstance()->MainLoop();

	PostMainLoop();
	return ret;
}

void HandleExitGameRequest()
{
	if (_game_mode == GM_MENU || _game_mode == GM_BOOTSTRAP) { // do not ask to quit on the main screen
		_exit_game = true;
	} else if (_settings_client.gui.autosave_on_exit) {
		DoExitSave();
		_survey.Transmit(NetworkSurveyHandler::Reason::EXIT, true);
		_exit_game = true;
	} else {
		AskExitGame();
	}
}

/**
 * Triggers everything required to set up a saved scenario for a new game.
 */
static void OnStartScenario()
{
	/* Reset engine pool to simplify changing engine NewGRFs in scenario editor. */
	EngineOverrideManager::ResetToCurrentNewGRFConfig();

	/* Make sure all industries were built "this year", to avoid too early closures. (#9918) */
	for (Industry *i : Industry::Iterate()) {
		i->last_prod_year = TimerGameCalendar::year;
	}
}

/**
 * Triggers everything that should be triggered when starting a game.
 * @param dedicated_server Whether this is a dedicated server or not.
 */
static void OnStartGame(bool dedicated_server)
{
	/* Update the local company for a loaded game. It is either the first available company
	 * or in the case of a dedicated server, a spectator */
	SetLocalCompany(dedicated_server ? COMPANY_SPECTATOR : GetFirstPlayableCompanyID());

	/* Update the static game info to set the values from the new game. */
	NetworkServerUpdateGameInfo();
	/* Execute the game-start script */
	IConsoleCmdExec("exec scripts/game_start.scr 0");
}

static void MakeNewGameDone()
{
	SettingsDisableElrail(_settings_game.vehicle.disable_elrails);

	/* In a dedicated server, the server does not play */
	if (!VideoDriver::GetInstance()->HasGUI()) {
		OnStartGame(true);
		if (_settings_client.gui.pause_on_newgame) Command<CMD_PAUSE>::Post(PM_PAUSED_NORMAL, true);
		return;
	}

	/* Create a single company */
	DoStartupNewCompany(false);

	Company *c = Company::Get(COMPANY_FIRST);
	c->settings = _settings_client.company;

	/* Overwrite color from settings if needed
	 * COLOUR_END corresponds to Random colour */

	if (_settings_client.gui.starting_colour != COLOUR_END) {
		c->colour = _settings_client.gui.starting_colour;
		ResetCompanyLivery(c);
		_company_colours[c->index] = (Colours)c->colour;
	}

	if (_settings_client.gui.starting_colour_secondary != COLOUR_END && HasBit(_loaded_newgrf_features.used_liveries, LS_DEFAULT)) {
		Command<CMD_SET_COMPANY_COLOUR>::Post(LS_DEFAULT, false, (Colours)_settings_client.gui.starting_colour_secondary);
	}

	OnStartGame(false);

	InitializeRailGUI();
	InitializeRoadGUI();

	/* We are the server, we start a new company (not dedicated),
	 * so set the default password *if* needed. */
	if (_network_server && !_settings_client.network.default_company_pass.empty()) {
		NetworkChangeCompanyPassword(_local_company, _settings_client.network.default_company_pass);
	}

	if (_settings_client.gui.pause_on_newgame) Command<CMD_PAUSE>::Post(PM_PAUSED_NORMAL, true);

	CheckEngines();
	CheckIndustries();
	MarkWholeScreenDirty();

	if (_network_server && !_network_dedicated) ShowClientList();
}

static void MakeNewGame(bool from_heightmap, bool reset_settings)
{
	_game_mode = GM_NORMAL;
	if (!from_heightmap) {
		/* "reload" command needs to know what mode we were in. */
		_file_to_saveload.SetMode(SLO_INVALID, FT_INVALID, DFT_INVALID);
	}

	ResetGRFConfig(true);

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
	/* "reload" command needs to know what mode we were in. */
	_file_to_saveload.SetMode(SLO_INVALID, FT_INVALID, DFT_INVALID);

	ResetGRFConfig(true);

	GenerateWorldSetCallback(&MakeNewEditorWorldDone);
	GenerateWorld(GWM_EMPTY, 1 << _settings_game.game_creation.map_x, 1 << _settings_game.game_creation.map_y);
}

/**
 * Load the specified savegame but on error do different things.
 * If loading fails due to corrupt savegame, bad version, etc. go back to
 * a previous correct state. In the menu for example load the intro game again.
 * @param filename file to be loaded
 * @param fop mode of loading, always SLO_LOAD
 * @param newgm switch to this mode of loading fails due to some unknown error
 * @param subdir default directory to look for filename, set to 0 if not needed
 * @param lf Load filter to use, if nullptr: use filename + subdir.
 */
bool SafeLoad(const std::string &filename, SaveLoadOperation fop, DetailedFileType dft, GameMode newgm, Subdirectory subdir, struct LoadFilter *lf = nullptr)
{
	assert(fop == SLO_LOAD);
	assert(dft == DFT_GAME_FILE || (lf == nullptr && dft == DFT_OLD_GAME_FILE));
	GameMode ogm = _game_mode;

	_game_mode = newgm;

	SaveOrLoadResult result = (lf == nullptr) ? SaveOrLoad(filename, fop, dft, subdir) : LoadWithFilter(lf);
	if (result == SL_OK) return true;

	if (_network_dedicated && ogm == GM_MENU) {
		/*
		 * If we are a dedicated server *and* we just were in the menu, then we
		 * are loading the first savegame. If that fails, not starting the
		 * server is a better reaction than starting the server with a newly
		 * generated map as it is quite likely to be started from a script.
		 */
		Debug(net, 0, "Loading requested map failed; closing server.");
		_exit_game = true;
		return false;
	}

	if (result != SL_REINIT) {
		_game_mode = ogm;
		return false;
	}

	if (_network_dedicated) {
		/*
		 * If we are a dedicated server, have already loaded/started a game,
		 * and then loading the savegame fails in a manner that we need to
		 * reinitialize everything. We must not fall back into the menu mode
		 * with the intro game, as that is unjoinable by clients. So there is
		 * nothing else to do than start a new game, as it might have failed
		 * trying to reload the originally loaded savegame/scenario.
		 */
		Debug(net, 0, "Loading game failed, so a new (random) game will be started");
		MakeNewGame(false, true);
		return false;
	}

	if (_network_server) {
		/* We can't load the intro game as server, so disconnect first. */
		NetworkDisconnect();
	}

	switch (ogm) {
		default:
		case GM_MENU:   LoadIntroGame();      break;
		case GM_EDITOR: MakeNewEditorWorld(); break;
	}
	return false;
}

void SwitchToMode(SwitchMode new_mode)
{
	/* If we are saving something, the network stays in its current state */
	if (new_mode != SM_SAVE_GAME) {
		/* If the network is active, make it not-active */
		if (_networking) {
			if (_network_server && (new_mode == SM_LOAD_GAME || new_mode == SM_NEWGAME || new_mode == SM_RESTARTGAME)) {
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

	/* Make sure all AI controllers are gone at quitting game */
	if (new_mode != SM_SAVE_GAME) AI::KillAll();

	/* When we change mode, reset the autosave. */
	if (new_mode != SM_SAVE_GAME) ChangeAutosaveFrequency(true);

	/* Transmit the survey if we were in normal-mode and not saving. It always means we leaving the current game. */
	if (_game_mode == GM_NORMAL && new_mode != SM_SAVE_GAME) _survey.Transmit(NetworkSurveyHandler::Reason::LEAVE);

	/* Keep track when we last switch mode. Used for survey, to know how long someone was in a game. */
	if (new_mode != SM_SAVE_GAME) _switch_mode_time = std::chrono::steady_clock::now();

	switch (new_mode) {
		case SM_EDITOR: // Switch to scenario editor
			MakeNewEditorWorld();
			GenerateSavegameId();
			break;

		case SM_RELOADGAME: // Reload with what-ever started the game
			if (_file_to_saveload.abstract_ftype == FT_SAVEGAME || _file_to_saveload.abstract_ftype == FT_SCENARIO) {
				/* Reload current savegame/scenario */
				_switch_mode = _game_mode == GM_EDITOR ? SM_LOAD_SCENARIO : SM_LOAD_GAME;
				SwitchToMode(_switch_mode);
				break;
			} else if (_file_to_saveload.abstract_ftype == FT_HEIGHTMAP) {
				/* Restart current heightmap */
				_switch_mode = _game_mode == GM_EDITOR ? SM_LOAD_HEIGHTMAP : SM_RESTART_HEIGHTMAP;
				SwitchToMode(_switch_mode);
				break;
			}

			MakeNewGame(false, new_mode == SM_NEWGAME);
			GenerateSavegameId();
			break;

		case SM_RESTARTGAME: // Restart --> 'Random game' with current settings
		case SM_NEWGAME: // New Game --> 'Random game'
			MakeNewGame(false, new_mode == SM_NEWGAME);
			GenerateSavegameId();
			break;

		case SM_LOAD_GAME: { // Load game, Play Scenario
			ResetGRFConfig(true);
			ResetWindowSystem();

			if (!SafeLoad(_file_to_saveload.name, _file_to_saveload.file_op, _file_to_saveload.detail_ftype, GM_NORMAL, NO_DIRECTORY)) {
				SetDParamStr(0, GetSaveLoadErrorString());
				ShowErrorMessage(STR_JUST_RAW_STRING, INVALID_STRING_ID, WL_CRITICAL);
			} else {
				if (_file_to_saveload.abstract_ftype == FT_SCENARIO) {
					OnStartScenario();
				}
				OnStartGame(_network_dedicated);
				/* Decrease pause counter (was increased from opening load dialog) */
				Command<CMD_PAUSE>::Post(PM_PAUSED_SAVELOAD, false);
			}
			break;
		}

		case SM_RESTART_HEIGHTMAP: // Load a heightmap and start a new game from it with current settings
		case SM_START_HEIGHTMAP: // Load a heightmap and start a new game from it
			MakeNewGame(true, new_mode == SM_START_HEIGHTMAP);
			GenerateSavegameId();
			break;

		case SM_LOAD_HEIGHTMAP: // Load heightmap from scenario editor
			SetLocalCompany(OWNER_NONE);

			GenerateWorld(GWM_HEIGHTMAP, 1 << _settings_game.game_creation.map_x, 1 << _settings_game.game_creation.map_y);
			GenerateSavegameId();
			MarkWholeScreenDirty();
			break;

		case SM_LOAD_SCENARIO: { // Load scenario from scenario editor
			if (SafeLoad(_file_to_saveload.name, _file_to_saveload.file_op, _file_to_saveload.detail_ftype, GM_EDITOR, NO_DIRECTORY)) {
				SetLocalCompany(OWNER_NONE);
				GenerateSavegameId();
				_settings_newgame.game_creation.starting_year = TimerGameCalendar::year;
				/* Cancel the saveload pausing */
				Command<CMD_PAUSE>::Post(PM_PAUSED_SAVELOAD, false);
			} else {
				SetDParamStr(0, GetSaveLoadErrorString());
				ShowErrorMessage(STR_JUST_RAW_STRING, INVALID_STRING_ID, WL_CRITICAL);
			}
			break;
		}

		case SM_JOIN_GAME: // Join a multiplayer game
			LoadIntroGame();
			NetworkClientJoinGame();
			break;

		case SM_MENU: // Switch to game intro menu
			LoadIntroGame();
			if (BaseSounds::ini_set.empty() && BaseSounds::GetUsedSet()->fallback && SoundDriver::GetInstance()->HasOutput()) {
				ShowErrorMessage(STR_WARNING_FALLBACK_SOUNDSET, INVALID_STRING_ID, WL_CRITICAL);
				BaseSounds::ini_set = BaseSounds::GetUsedSet()->name;
			}
			if (_settings_client.network.participate_survey == PS_ASK) {
				/* No matter how often you go back to the main menu, only ask the first time. */
				static bool asked_once = false;
				if (!asked_once) {
					asked_once = true;
					ShowNetworkAskSurvey();
				}
			}
			break;

		case SM_SAVE_GAME: // Save game.
			/* Make network saved games on pause compatible to singleplayer mode */
			if (SaveOrLoad(_file_to_saveload.name, SLO_SAVE, DFT_GAME_FILE, NO_DIRECTORY) != SL_OK) {
				SetDParamStr(0, GetSaveLoadErrorString());
				ShowErrorMessage(STR_JUST_RAW_STRING, INVALID_STRING_ID, WL_ERROR);
			} else {
				CloseWindowById(WC_SAVELOAD, 0);
			}
			break;

		case SM_SAVE_HEIGHTMAP: // Save heightmap.
			MakeHeightmapScreenshot(_file_to_saveload.name.c_str());
			CloseWindowById(WC_SAVELOAD, 0);
			break;

		case SM_GENRANDLAND: // Generate random land within scenario editor
			SetLocalCompany(OWNER_NONE);
			GenerateWorld(GWM_RANDOM, 1 << _settings_game.game_creation.map_x, 1 << _settings_game.game_creation.map_y);
			/* XXX: set date */
			MarkWholeScreenDirty();
			break;

		default: NOT_REACHED();
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

	/* Check the town caches. */
	std::vector<TownCache> old_town_caches;
	for (const Town *t : Town::Iterate()) {
		old_town_caches.push_back(t->cache);
	}

	RebuildTownCaches();
	RebuildSubsidisedSourceAndDestinationCache();

	uint i = 0;
	for (Town *t : Town::Iterate()) {
		if (MemCmpT(old_town_caches.data() + i, &t->cache) != 0) {
			Debug(desync, 2, "town cache mismatch: town {}", t->index);
		}
		i++;
	}

	/* Check company infrastructure cache. */
	std::vector<CompanyInfrastructure> old_infrastructure;
	for (const Company *c : Company::Iterate()) old_infrastructure.push_back(c->infrastructure);

	AfterLoadCompanyStats();

	i = 0;
	for (const Company *c : Company::Iterate()) {
		if (MemCmpT(old_infrastructure.data() + i, &c->infrastructure) != 0) {
			Debug(desync, 2, "infrastructure cache mismatch: company {}", c->index);
		}
		i++;
	}

	/* Strict checking of the road stop cache entries */
	for (const RoadStop *rs : RoadStop::Iterate()) {
		if (IsBayRoadStopTile(rs->xy)) continue;

		assert(rs->GetEntry(DIAGDIR_NE) != rs->GetEntry(DIAGDIR_NW));
		rs->GetEntry(DIAGDIR_NE)->CheckIntegrity(rs);
		rs->GetEntry(DIAGDIR_NW)->CheckIntegrity(rs);
	}

	for (Vehicle *v : Vehicle::Iterate()) {
		if (v != v->First() || v->vehstatus & VS_CRASHED || !v->IsPrimaryVehicle()) continue;

		uint length = 0;
		for (const Vehicle *u = v; u != nullptr; u = u->Next()) length++;

		NewGRFCache        *grf_cache = CallocT<NewGRFCache>(length);
		VehicleCache       *veh_cache = CallocT<VehicleCache>(length);
		GroundVehicleCache *gro_cache = CallocT<GroundVehicleCache>(length);
		TrainCache         *tra_cache = CallocT<TrainCache>(length);

		length = 0;
		for (const Vehicle *u = v; u != nullptr; u = u->Next()) {
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
			case VEH_TRAIN:    Train::From(v)->ConsistChanged(CCF_TRACK); break;
			case VEH_ROAD:     RoadVehUpdateCache(RoadVehicle::From(v)); break;
			case VEH_AIRCRAFT: UpdateAircraftCache(Aircraft::From(v));   break;
			case VEH_SHIP:     Ship::From(v)->UpdateCache();             break;
			default: break;
		}

		length = 0;
		for (const Vehicle *u = v; u != nullptr; u = u->Next()) {
			FillNewGRFVehicleCache(u);
			if (memcmp(&grf_cache[length], &u->grf_cache, sizeof(NewGRFCache)) != 0) {
				Debug(desync, 2, "newgrf cache mismatch: type {}, vehicle {}, company {}, unit number {}, wagon {}", v->type, v->index, v->owner, v->unitnumber, length);
			}
			if (memcmp(&veh_cache[length], &u->vcache, sizeof(VehicleCache)) != 0) {
				Debug(desync, 2, "vehicle cache mismatch: type {}, vehicle {}, company {}, unit number {}, wagon {}", v->type, v->index, v->owner, v->unitnumber, length);
			}
			switch (u->type) {
				case VEH_TRAIN:
					if (memcmp(&gro_cache[length], &Train::From(u)->gcache, sizeof(GroundVehicleCache)) != 0) {
						Debug(desync, 2, "train ground vehicle cache mismatch: vehicle {}, company {}, unit number {}, wagon {}", v->index, v->owner, v->unitnumber, length);
					}
					if (memcmp(&tra_cache[length], &Train::From(u)->tcache, sizeof(TrainCache)) != 0) {
						Debug(desync, 2, "train cache mismatch: vehicle {}, company {}, unit number {}, wagon {}", v->index, v->owner, v->unitnumber, length);
					}
					break;
				case VEH_ROAD:
					if (memcmp(&gro_cache[length], &RoadVehicle::From(u)->gcache, sizeof(GroundVehicleCache)) != 0) {
						Debug(desync, 2, "road vehicle ground vehicle cache mismatch: vehicle {}, company {}, unit number {}, wagon {}", v->index, v->owner, v->unitnumber, length);
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
	for (Vehicle *v : Vehicle::Iterate()) {
		byte buff[sizeof(VehicleCargoList)];
		memcpy(buff, &v->cargo, sizeof(VehicleCargoList));
		v->cargo.InvalidateCache();
		assert(memcmp(&v->cargo, buff, sizeof(VehicleCargoList)) == 0);
	}

	/* Backup stations_near */
	std::vector<StationList> old_town_stations_near;
	for (Town *t : Town::Iterate()) old_town_stations_near.push_back(t->stations_near);

	std::vector<StationList> old_industry_stations_near;
	for (Industry *ind : Industry::Iterate())  old_industry_stations_near.push_back(ind->stations_near);

	for (Station *st : Station::Iterate()) {
		for (GoodsEntry &ge : st->goods) {
			byte buff[sizeof(StationCargoList)];
			memcpy(buff, &ge.cargo, sizeof(StationCargoList));
			ge.cargo.InvalidateCache();
			assert(memcmp(&ge.cargo, buff, sizeof(StationCargoList)) == 0);
		}

		/* Check docking tiles */
		TileArea ta;
		std::map<TileIndex, bool> docking_tiles;
		for (TileIndex tile : st->docking_station) {
			ta.Add(tile);
			docking_tiles[tile] = IsDockingTile(tile);
		}
		UpdateStationDockingTiles(st);
		if (ta.tile != st->docking_station.tile || ta.w != st->docking_station.w || ta.h != st->docking_station.h) {
			Debug(desync, 2, "station docking mismatch: station {}, company {}", st->index, st->owner);
		}
		for (TileIndex tile : ta) {
			if (docking_tiles[tile] != IsDockingTile(tile)) {
				Debug(desync, 2, "docking tile mismatch: tile {}", tile);
			}
		}

		/* Check industries_near */
		IndustryList industries_near = st->industries_near;
		st->RecomputeCatchment();
		if (st->industries_near != industries_near) {
			Debug(desync, 2, "station industries near mismatch: station {}", st->index);
		}
	}

	/* Check stations_near */
	i = 0;
	for (Town *t : Town::Iterate()) {
		if (t->stations_near != old_town_stations_near[i]) {
			Debug(desync, 2, "town stations near mismatch: town {}", t->index);
		}
		i++;
	}
	i = 0;
	for (Industry *ind : Industry::Iterate()) {
		if (ind->stations_near != old_industry_stations_near[i]) {
			Debug(desync, 2, "industry stations near mismatch: industry {}", ind->index);
		}
		i++;
	}
}

/**
 * State controlling game loop.
 * The state must not be changed from anywhere but here.
 * That check is enforced in DoCommand.
 */
void StateGameLoop()
{
	if (!_networking || _network_server) {
		StateGameLoop_LinkGraphPauseControl();
	}

	/* Don't execute the state loop during pause or when modal windows are open. */
	if (_pause_mode != PM_UNPAUSED || HasModalProgress()) {
		PerformanceMeasurer::Paused(PFE_GAMELOOP);
		PerformanceMeasurer::Paused(PFE_GL_ECONOMY);
		PerformanceMeasurer::Paused(PFE_GL_TRAINS);
		PerformanceMeasurer::Paused(PFE_GL_ROADVEHS);
		PerformanceMeasurer::Paused(PFE_GL_SHIPS);
		PerformanceMeasurer::Paused(PFE_GL_AIRCRAFT);
		PerformanceMeasurer::Paused(PFE_GL_LANDSCAPE);

		if (!HasModalProgress()) UpdateLandscapingLimits();
#ifndef DEBUG_DUMP_COMMANDS
		Game::GameLoop();
#endif
		return;
	}

	PerformanceMeasurer framerate(PFE_GAMELOOP);
	PerformanceAccumulator::Reset(PFE_GL_LANDSCAPE);

	Layouter::ReduceLineCache();

	if (_game_mode == GM_EDITOR) {
		BasePersistentStorageArray::SwitchMode(PSM_ENTER_GAMELOOP);
		RunTileLoop();
		CallVehicleTicks();
		CallLandscapeTick();
		BasePersistentStorageArray::SwitchMode(PSM_LEAVE_GAMELOOP);
		UpdateLandscapingLimits();

		CallWindowGameTickEvent();
		NewsLoop();
	} else {
		if (_debug_desync_level > 2 && TimerGameCalendar::date_fract == 0 && (TimerGameCalendar::date.base() & 0x1F) == 0) {
			/* Save the desync savegame if needed. */
			std::string name = fmt::format("dmp_cmds_{:08x}_{:08x}.sav", _settings_game.game_creation.generation_seed, TimerGameCalendar::date);
			SaveOrLoad(name, SLO_SAVE, DFT_GAME_FILE, AUTOSAVE_DIR, false);
		}

		CheckCaches();

		/* All these actions has to be done from OWNER_NONE
		 *  for multiplayer compatibility */
		Backup<CompanyID> cur_company(_current_company, OWNER_NONE, FILE_LINE);

		BasePersistentStorageArray::SwitchMode(PSM_ENTER_GAMELOOP);
		AnimateAnimatedTiles();
		TimerManager<TimerGameCalendar>::Elapsed(1);
		TimerManager<TimerGameTick>::Elapsed(1);
		RunTileLoop();
		CallVehicleTicks();
		CallLandscapeTick();
		BasePersistentStorageArray::SwitchMode(PSM_LEAVE_GAMELOOP);

#ifndef DEBUG_DUMP_COMMANDS
		{
			PerformanceMeasurer script_framerate(PFE_ALLSCRIPTS);
			AI::GameLoop();
			Game::GameLoop();
		}
#endif
		UpdateLandscapingLimits();

		CallWindowGameTickEvent();
		NewsLoop();
		cur_company.Restore();
	}

	assert(IsLocalCompany());
}

/** Interval for regular autosaves. Initialized at zero to disable till settings are loaded. */
static IntervalTimer<TimerGameRealtime> _autosave_interval({std::chrono::milliseconds::zero(), TimerGameRealtime::AUTOSAVE}, [](auto)
{
	/* We reset the command-during-pause mode here, so we don't continue
	 * to make auto-saves when nothing more is changing. */
	_pause_mode &= ~PM_COMMAND_DURING_PAUSE;

	_do_autosave = true;
	SetWindowDirty(WC_STATUS_BAR, 0);

	static FiosNumberedSaveName _autosave_ctr("autosave");
	DoAutoOrNetsave(_autosave_ctr);

	_do_autosave = false;
	SetWindowDirty(WC_STATUS_BAR, 0);
});

/**
 * Reset the interval of the autosave.
 *
 * If reset is not set, this does not set the elapsed time on the timer,
 * so if the interval is smaller, it might result in an autosave being done
 * immediately.
 *
 * @param reset Whether to reset the timer back to zero, or to continue.
 */
void ChangeAutosaveFrequency(bool reset)
{
	_autosave_interval.SetInterval({std::chrono::minutes(_settings_client.gui.autosave_interval), TimerGameRealtime::AUTOSAVE}, reset);
}

/**
 * Request a new NewGRF scan. This will be executed on the next game-tick.
 * This is mostly needed to ensure NewGRF scans (which are blocking) are
 * done in the game-thread, and not in the draw-thread (which most often
 * triggers this request).
 * @param callback Optional callback to call when NewGRF scan is completed.
 * @return True when the NewGRF scan was actually requested, false when the scan was already running.
 */
bool RequestNewGRFScan(NewGRFScanCallback *callback)
{
	if (_request_newgrf_scan) return false;

	_request_newgrf_scan = true;
	_request_newgrf_scan_callback = callback;
	return true;
}

void GameLoop()
{
	if (_game_mode == GM_BOOTSTRAP) {
		/* Check for UDP stuff */
		if (_network_available) NetworkBackgroundLoop();
		return;
	}

	if (_request_newgrf_scan) {
		ScanNewGRFFiles(_request_newgrf_scan_callback);
		_request_newgrf_scan = false;
		_request_newgrf_scan_callback = nullptr;
		/* In case someone closed the game during our scan, don't do anything else. */
		if (_exit_game) return;
	}

	ProcessAsyncSaveFinish();

	if (_game_mode == GM_NORMAL) {
		static auto last_time = std::chrono::steady_clock::now();
		auto now = std::chrono::steady_clock::now();
		auto delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time);
		if (delta_ms.count() != 0) {
			TimerManager<TimerGameRealtime>::Elapsed(delta_ms);
			last_time = now;
		}
	}

	/* switch game mode? */
	if (_switch_mode != SM_NONE && !HasModalProgress()) {
		SwitchToMode(_switch_mode);
		_switch_mode = SM_NONE;
		if (_exit_game) return;
	}

	IncreaseSpriteLRU();

	/* Check for UDP stuff */
	if (_network_available) NetworkBackgroundLoop();

	DebugSendRemoteMessages();

	if (_networking && !HasModalProgress()) {
		/* Multiplayer */
		NetworkGameLoop();
	} else {
		if (_network_reconnect > 0 && --_network_reconnect == 0) {
			/* This means that we want to reconnect to the last host
			 * We do this here, because it means that the network is really closed */
			NetworkClientConnectGame(_settings_client.network.last_joined, COMPANY_SPECTATOR);
		}
		/* Singleplayer */
		StateGameLoop();
	}

	if (!_pause_mode && HasBit(_display_opt, DO_FULL_ANIMATION)) DoPaletteAnimations();

	SoundDriver::GetInstance()->MainLoop();
	MusicLoop();
}
