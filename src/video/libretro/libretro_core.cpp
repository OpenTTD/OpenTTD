/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file libretro_core.cpp Full libretro core implementation for OpenTTD. */

#include "libretro.h"
#include "libretro_core.h"

#ifdef WITH_LIBRETRO
#include "libretro_v.h"
#include "../../stdafx.h"
#include "../../openttd.h"
#include "../../driver.h"
#include "../../gfx_func.h"
#include "../../window_func.h"
#include "../../fileio_func.h"
#include "../../fios.h"
#include "../../blitter/factory.hpp"
#include "../../fontcache.h"
#include "../../base_media_base.h"
#include "../../base_media_graphics.h"
#include "../../base_media_sounds.h"
#include "../../base_media_music.h"
#include "../../settings_func.h"
#include "../../network/network.h"
#include "../../ai/ai.hpp"
#include "../../game/game.hpp"
#include "../../social_integration.h"
#include "../../genworld.h"
#include "../../fileio_type.h"
#include "../../fileio_func.h"
#include "../../progress.h"
#include "../../sound/sound_driver.hpp"
#include "../../music/music_driver.hpp"
#include "../../strings_func.h"
#include "../../language.h"
#include "../../gfxinit.h"
#include "../../palette_func.h"
#include "../../mixer.h"
#include "../../console_func.h"
#include "../../hotkeys.h"
#include "../../company_func.h"
#include "../../rev.h"
#include "../../tar_type.h"
#include "../../debug.h"
#include "../../window_gui.h"
#include "../../viewport_func.h"
#include "../../sprite.h"
#include "../../table/sprites.h"
#include "../../table/palettes.h"
#include "../../saveload/saveload.h"
#include "../../newgrf_config.h"
#include "../../sound_func.h"
#endif

#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <mutex>
#include <atomic>

#if defined(_WIN32) && defined(_MSC_VER)
#include <windows.h>
#include <dbghelp.h>
#include "../../library_loader.h"
#endif

#if defined(_MSC_VER)
#include <crtdbg.h>
#endif

#if defined(_MSC_VER)
static void ConfigureCrtReportToStderr()
{
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
	_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
	_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
}
#endif

/* Libretro callbacks */
static retro_log_printf_t log_cb = nullptr;
static retro_video_refresh_t video_cb = nullptr;
static retro_audio_sample_t audio_cb = nullptr;
static retro_audio_sample_batch_t audio_batch_cb = nullptr;
static retro_input_poll_t input_poll_cb = nullptr;
static retro_input_state_t input_state_cb = nullptr;
static retro_environment_t environ_cb = nullptr;

/* State */
static std::string system_directory;
static std::string save_directory;
static unsigned video_width = 1280;
static unsigned video_height = 720;
static std::atomic<bool> core_initialized{false};
static std::atomic<bool> game_loaded{false};
static std::atomic<bool> openttd_initialized{false};

extern std::string _config_file;
extern std::string _private_file;
extern std::string _secrets_file;
extern std::string _favs_file;

/* Input state */
static int mouse_x = 640;
static int mouse_y = 360;
static bool mouse_left = false;
static bool mouse_right = false;
static bool mouse_middle = false;
static int mouse_wheel = 0;
static std::mutex input_mutex;

static bool pointer_tracking = false;
static int16_t last_pointer_x = 0;
static int16_t last_pointer_y = 0;

/* Keyboard state */
static uint16_t keyboard_modifiers = 0;
static bool keyboard_keys[RETROK_LAST] = {false};

/* Keyboard event queue for passing to OpenTTD */
struct KeyEvent {
	bool down;
	unsigned keycode;
	uint32_t character;
	uint16_t modifiers;
};
static std::vector<KeyEvent> pending_key_events;
static std::mutex key_event_mutex;

/* Gamepad cursor speed */
static const int GAMEPAD_CURSOR_SPEED = 8;
static const int GAMEPAD_CURSOR_SPEED_FAST = 16;

/* Audio */
static const unsigned AUDIO_SAMPLE_RATE = 44100;
static int16_t audio_buffer[4096];

#ifdef WITH_LIBRETRO
/* Forward declarations for OpenTTD functions */
extern void DeterminePaths(std::string_view exe, bool only_local_path);
extern void LoadFromConfig(bool startup);
extern void InitializeLanguagePacks();
extern bool HandleBootstrap();
extern void MakeNewgameSettingsLive();
extern void InitializeSound();
extern void InitializeMusic();
extern void GameLoop();
extern void InputLoop();
extern void UpdateWindows();
extern void IConsoleFree();

/* Register the libretro video driver factory */
static FVideoDriver_Libretro _libretro_video_factory;
#endif

/* Keyboard callback - called by RetroArch when keys are pressed */
static void RETRO_CALLCONV keyboard_callback(bool down, unsigned keycode, uint32_t character, uint16_t modifiers)
{
	std::lock_guard<std::mutex> lock(key_event_mutex);

	/* Update modifier state */
	keyboard_modifiers = modifiers;

	/* Track key state */
	if (keycode < RETROK_LAST) {
		keyboard_keys[keycode] = down;
	}

	/* Queue key event for processing in game thread */
	KeyEvent evt;
	evt.down = down;
	evt.keycode = keycode;
	evt.character = character;
	evt.modifiers = modifiers;
	pending_key_events.push_back(evt);
}

static void poll_input()
{
	if (!input_poll_cb || !input_state_cb) return;

	input_poll_cb();

	std::lock_guard<std::mutex> lock(input_mutex);

	/* Get relative mouse movement */
	int16_t mx = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
	int16_t my = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);

	mouse_x += mx;
	mouse_y += my;

	/* Get mouse buttons */
	mouse_left = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT) != 0;
	mouse_right = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT) != 0;
	mouse_middle = input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_MIDDLE) != 0;

	/* Mouse wheel */
	if (input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELUP)) {
		mouse_wheel--;
	}
	if (input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELDOWN)) {
		mouse_wheel++;
	}

	/* Gamepad input - map to cursor movement and clicks */
	{
		/* Left analog stick for cursor movement */
		int16_t stick_x = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
		int16_t stick_y = input_state_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);

		/* Deadzone */
		const int16_t DEADZONE = 8000;
		if (abs(stick_x) > DEADZONE || abs(stick_y) > DEADZONE) {
			/* Check if L2 is held for fast movement */
			bool fast = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2) != 0;
			int speed = fast ? GAMEPAD_CURSOR_SPEED_FAST : GAMEPAD_CURSOR_SPEED;

			if (abs(stick_x) > DEADZONE) {
				mouse_x += (stick_x * speed) / 32768;
			}
			if (abs(stick_y) > DEADZONE) {
				mouse_y += (stick_y * speed) / 32768;
			}
		}

		/* D-pad for cursor movement */
		if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP)) mouse_y -= GAMEPAD_CURSOR_SPEED;
		if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN)) mouse_y += GAMEPAD_CURSOR_SPEED;
		if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT)) mouse_x -= GAMEPAD_CURSOR_SPEED;
		if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT)) mouse_x += GAMEPAD_CURSOR_SPEED;

		/* A = Left click, B = Right click */
		if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A)) mouse_left = true;
		if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B)) mouse_right = true;

		/* Shoulder buttons for zoom (mouse wheel) */
		static bool prev_l = false, prev_r = false;
		bool cur_l = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L) != 0;
		bool cur_r = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R) != 0;
		if (cur_l && !prev_l) mouse_wheel--; /* Zoom in */
		if (cur_r && !prev_r) mouse_wheel++; /* Zoom out */
		prev_l = cur_l;
		prev_r = cur_r;
	}

	/* Touch/pointer input for absolute positioning */
	int16_t px = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X);
	int16_t py = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y);
	bool pressed = input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED) != 0;

	/* Detect whether pointer input is meaningful.
	 * This avoids cursor jumps on click by not mixing relative mouse movement with
	 * a one-off absolute pointer update when a click happens.
	 */
	if (!pointer_tracking) {
		if (pressed || px != 0 || py != 0 || px != last_pointer_x || py != last_pointer_y) {
			pointer_tracking = pressed || px != 0 || py != 0;
			last_pointer_x = px;
			last_pointer_y = py;
		}
	}

	if (pointer_tracking) {
		/* Convert from -32768..32767 to screen coordinates.
		 * Use 0x8000/0x10000 mapping to avoid off-by-one drift.
		 */
		mouse_x = (int)(((int32_t)px + 0x8000) * (int32_t)video_width >> 16);
		mouse_y = (int)(((int32_t)py + 0x8000) * (int32_t)video_height >> 16);
		if (pressed) mouse_left = true;
	}

	/* Clamp mouse position */
	if (mouse_x < 0) mouse_x = 0;
	if (mouse_y < 0) mouse_y = 0;
	if (mouse_x >= (int)video_width) mouse_x = video_width - 1;
	if (mouse_y >= (int)video_height) mouse_y = video_height - 1;
}

static void render_audio()
{
	if (!audio_batch_cb) return;

#ifdef WITH_LIBRETRO
	/* Mix OpenTTD audio into our buffer */
	size_t samples = 735; /* ~44100/60 samples per frame */
	MxMixSamples(audio_buffer, samples);
	audio_batch_cb(audio_buffer, samples);
#else
	/* Generate silence */
	static int16_t silence[735 * 2] = {0};
	audio_batch_cb(silence, 735);
#endif
}

#ifdef WITH_LIBRETRO
static bool initialize_openttd()
{
	if (openttd_initialized) return true;

	if (log_cb) log_cb(RETRO_LOG_INFO, "[OpenTTD] Initializing OpenTTD engine...\n");

	/* Set up paths - use system directory for game data */
	std::string exe_path = system_directory + "/OpenTTD/openttd";

	/* Determine paths */
	DeterminePaths(exe_path, false);

	/* Add RetroArch system directory to OpenTTD search paths */
	{
		extern std::array<std::string, NUM_SEARCHPATHS> _searchpaths;
		extern std::vector<Searchpath> _valid_searchpaths;

		std::string openttd_data_path = system_directory + PATHSEP "OpenTTD" PATHSEP;

		/* Set the installation dir to our RetroArch system folder */
		_searchpaths[SP_INSTALLATION_DIR] = openttd_data_path;

		/* Add to valid search paths if not already there */
		bool found = false;
		for (auto sp : _valid_searchpaths) {
			if (sp == SP_INSTALLATION_DIR) { found = true; break; }
		}
		if (!found) {
			_valid_searchpaths.insert(_valid_searchpaths.begin(), SP_INSTALLATION_DIR);
		}
	}

	/* Scan for base sets */
	TarScanner::DoScan(TarScanner::Mode::Baseset);

	/* Load config */
	LoadFromConfig(true);
	if (_settings_client.music.effect_vol == 0 && _settings_client.music.music_vol == 0) {
		_settings_client.music.effect_vol = 100;
		_settings_client.music.music_vol = 50;
	}

	/* Ensure map size settings are valid (prevent InitializeGame 1x1 crash) */
	extern GameSettings _settings_game;
	extern GameSettings _settings_newgame;
	if (_settings_game.game_creation.map_x < 6) _settings_game.game_creation.map_x = 8; // 256x256
	if (_settings_game.game_creation.map_y < 6) _settings_game.game_creation.map_y = 8; // 256x256
	if (_settings_newgame.game_creation.map_x < 6) _settings_newgame.game_creation.map_x = 8; // 256x256
	if (_settings_newgame.game_creation.map_y < 6) _settings_newgame.game_creation.map_y = 8; // 256x256

	/* Initialize language packs */
	InitializeLanguagePacks();

	/* Initialize font cache */
	FontCache::LoadFontCaches(FONTSIZES_REQUIRED);

	/* Skip InitWindowSystem here - ResetWindowSystem() will call it later */

	/* Find and set graphics */
	BaseGraphics::FindSets();

	bool valid_graphics_set = true;
	if (BaseGraphics::ini_data.shortname != 0) {
		valid_graphics_set = BaseGraphics::SetSetByShortname(BaseGraphics::ini_data.shortname);
		if (valid_graphics_set && !BaseGraphics::ini_data.extra_params.empty() && BaseGraphics::GetUsedSet() != nullptr) {
			GRFConfig &extra_cfg = BaseGraphics::GetUsedSet()->GetOrCreateExtraConfig();
			if (extra_cfg.IsCompatible(BaseGraphics::ini_data.extra_version)) {
				extra_cfg.SetParams(BaseGraphics::ini_data.extra_params);
			}
		}
	} else if (!BaseGraphics::ini_data.name.empty()) {
		valid_graphics_set = BaseGraphics::SetSetByName(BaseGraphics::ini_data.name);
	} else {
		valid_graphics_set = BaseGraphics::SetSet(nullptr);
	}

	if (!valid_graphics_set) {
		if (log_cb) log_cb(RETRO_LOG_WARN, "[OpenTTD] WARNING: Requested graphics set not found; falling back to best available\n");
		if (!BaseGraphics::SetSet(nullptr)) {
			if (log_cb) log_cb(RETRO_LOG_ERROR, "[OpenTTD] Failed to find any usable graphics set!\n");
			if (log_cb) log_cb(RETRO_LOG_ERROR, "[OpenTTD] Please place a base graphics set in: %s/OpenTTD/baseset/\n", system_directory.c_str());
			return false;
		}
	}


	/* Initialize palette */
	GfxInitPalettes();

	/* Select blitter */
	if (BlitterFactory::SelectBlitter("32bpp-sse4-anim") == nullptr) {
		if (BlitterFactory::SelectBlitter("32bpp-sse2-anim") == nullptr) {
			if (BlitterFactory::SelectBlitter("32bpp-anim") == nullptr) {
				if (BlitterFactory::SelectBlitter("32bpp-simple") == nullptr) {
					if (BlitterFactory::SelectBlitter("32bpp-optimized") == nullptr) {
						if (BlitterFactory::SelectBlitter("8bpp-optimized") == nullptr) {
							if (log_cb) log_cb(RETRO_LOG_ERROR, "[OpenTTD] Failed to select blitter!\n");
							return false;
						}
					}
				}
			}
		}
	}

	/* Select video driver - use our libretro driver */
	DriverFactoryBase::SelectDriver("libretro", Driver::DT_VIDEO);

	/* Initialize sprite sorter */
	extern void InitializeSpriteSorter();
	InitializeSpriteSorter();

	/* Set screen zoom */
	_screen.zoom = ZoomLevel::Min;

	/* Update GUI zoom */
	extern void UpdateGUIZoom();
	UpdateGUIZoom();

	/* Initialize networking */
	SocialIntegration::Initialize();
	NetworkStartUp();

	/* Handle bootstrap (download content if needed) */
	if (!HandleBootstrap()) {
		if (log_cb) log_cb(RETRO_LOG_ERROR, "[OpenTTD] Bootstrap failed!\n");
		return false;
	}

	/* Find sound sets */
	BaseSounds::FindSets();
	BaseSounds::SetSet({});

	/* Find music sets */
	BaseMusic::FindSets();
	BaseMusic::SetSet({});

	/* Use libretro sound driver so effects flow into the libretro audio callback. */
	DriverFactoryBase::SelectDriver("libretro", Driver::DT_SOUND);
	/* Autoprobe a suitable music driver. */
	DriverFactoryBase::SelectDriver("", Driver::DT_MUSIC);

	SetEffectVolume(_settings_client.music.effect_vol);
	if (MusicDriver::GetInstance() != nullptr) {
		MusicDriver::GetInstance()->SetVolume(_settings_client.music.music_vol);
	}

	VideoDriver::GetInstance()->ClaimMousePointer();

	/* Replicate LoadIntroGame logic */
	extern void ResetGRFConfig(bool defaults);
	extern void ResetWindowSystem();
	extern void SetupColoursAndInitialWindow();
	extern void GenerateWorld(GenWorldMode mode, uint size_x, uint size_y, bool reset_settings = true);
	extern void InitializeGame(uint size_x, uint size_y, bool reset_date, bool reset_settings);
	extern void SetLocalCompany(CompanyID new_company);
	extern void FixTitleGameZoom(int zoom_adjustment);
	extern void CheckForMissingGlyphs(class MissingGlyphSearcher *searcher);
	extern void MusicLoop();

	_game_mode = GM_MENU;
	ResetGRFConfig(false);
	InitializeGame(64, 64, true, true);
	SndCopyToPool();
	GfxLoadSprites();

	/* Setup main window */
	ResetWindowSystem();
	SetupColoursAndInitialWindow();

	/* Load full configuration now that the window system exists. */
	LoadFromConfig(false);
	{
		extern bool _save_config;
		_save_config = true;
	}

	/* Re-apply volumes and reinitialize sound/music after loading full config. */
	SetEffectVolume(_settings_client.music.effect_vol);
	if (MusicDriver::GetInstance() != nullptr) {
		MusicDriver::GetInstance()->SetVolume(_settings_client.music.music_vol);
	}
	InitializeSound();
	InitializeMusic();

	if (SaveOrLoad("opntitle.dat", SLO_LOAD, DFT_GAME_FILE, BASESET_DIR) != SL_OK) {
		GenerateWorld(GWM_EMPTY, 64, 64);
		SetLocalCompany(COMPANY_SPECTATOR);
	} else {
		SetLocalCompany(CompanyID::Begin());
	}

	FixTitleGameZoom(0);
	_pause_mode = {};
	_cursor.fix_at = false;
	CheckForMissingGlyphs(nullptr);
	MusicLoop();
	_pause_mode = {};
	_cursor.fix_at = false;

	openttd_initialized = true;

	if (log_cb) log_cb(RETRO_LOG_INFO, "[OpenTTD] OpenTTD engine initialized successfully!\n");

	return true;
}

static void shutdown_openttd()
{
	if (!openttd_initialized) return;

	if (log_cb) log_cb(RETRO_LOG_INFO, "[OpenTTD] Shutting down OpenTTD engine...\n");

	/* Clean up - use available public functions */
	IConsoleFree();
	NetworkShutDown();
	SocialIntegration::Shutdown();

	openttd_initialized = false;

	if (log_cb) log_cb(RETRO_LOG_INFO, "[OpenTTD] OpenTTD engine shut down\n");
}
#endif

/* Libretro API implementation */

RETRO_API void retro_set_environment(retro_environment_t cb)
{
	environ_cb = cb;

	/* Get log callback */
	struct retro_log_callback logging;
	memset(&logging, 0, sizeof(logging));
	cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging);
	if (logging.log) {
		log_cb = logging.log;
	}

	/* Set support for no game (contentless) */
	bool no_content = true;
	cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);

	/* Set core options */
	static const struct retro_variable vars[] = {
		{ "openttd_resolution", "Resolution; 1280x720|1920x1080|1024x768|800x600|640x480" },
		{ NULL, NULL }
	};
	cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)vars);

	/* Register keyboard callback */
	static struct retro_keyboard_callback kb_callback;
	kb_callback.callback = keyboard_callback;
	cb(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &kb_callback);

	/* Set input descriptors */
	static const struct retro_input_descriptor input_desc[] = {
		/* Mouse */
		{ 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X, "Mouse X" },
		{ 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y, "Mouse Y" },
		{ 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT, "Left Click" },
		{ 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT, "Right Click" },
		{ 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_MIDDLE, "Middle Click" },
		{ 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELUP, "Zoom In" },
		{ 0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_WHEELDOWN, "Zoom Out" },
		/* Gamepad */
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "Cursor Up" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "Cursor Down" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "Cursor Left" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Cursor Right" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "Left Click / Select" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "Right Click / Cancel" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "Toggle Build Menu" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Toggle Pause" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "Zoom In" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "Zoom Out" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "Fast Cursor" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Open Menu" },
		{ 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Toggle Fullscreen GUI" },
		/* Analog */
		{ 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X, "Cursor X" },
		{ 0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y, "Cursor Y" },
		/* Touch/Pointer */
		{ 0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X, "Touch X" },
		{ 0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y, "Touch Y" },
		{ 0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED, "Touch" },
		{ 0, 0, 0, 0, NULL }
	};
	cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, (void*)input_desc);

	if (log_cb) {
		log_cb(RETRO_LOG_INFO, "[OpenTTD] Environment set up with keyboard and input descriptors\n");
	}
}

RETRO_API void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
RETRO_API void retro_set_audio_sample(retro_audio_sample_t cb) { audio_cb = cb; }
RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
RETRO_API void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
RETRO_API void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

RETRO_API void retro_init(void)
{
	if (log_cb) log_cb(RETRO_LOG_INFO, "[OpenTTD] retro_init\n");
	setvbuf(stderr, nullptr, _IONBF, 0);
	setvbuf(stdout, nullptr, _IONBF, 0);

#if defined(_MSC_VER)
	ConfigureCrtReportToStderr();
#endif

	/* Get directories */
	const char *dir = nullptr;
	environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir);
	if (dir) {
		system_directory = dir;
	}
	dir = nullptr;
	environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &dir);
	if (dir) {
		save_directory = dir;
	}

	/* Set pixel format to XRGB8888 */
	enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
	environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);

	/* Check core options for resolution */
	struct retro_variable var = { "openttd_resolution", NULL };
	environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var);
	if (var.value) {
		if (strcmp(var.value, "1920x1080") == 0) { video_width = 1920; video_height = 1080; }
		else if (strcmp(var.value, "1024x768") == 0) { video_width = 1024; video_height = 768; }
		else if (strcmp(var.value, "800x600") == 0) { video_width = 800; video_height = 600; }
		else if (strcmp(var.value, "640x480") == 0) { video_width = 640; video_height = 480; }
		else { video_width = 1280; video_height = 720; }
	}

	/* Initialize mouse position to center */
	mouse_x = video_width / 2;
	mouse_y = video_height / 2;

	core_initialized = true;

	if (log_cb) log_cb(RETRO_LOG_INFO, "[OpenTTD] Core initialized (%ux%u)\n", video_width, video_height);
}

RETRO_API void retro_deinit(void)
{
	if (log_cb) log_cb(RETRO_LOG_INFO, "[OpenTTD] retro_deinit\n");

#ifdef WITH_LIBRETRO
	shutdown_openttd();
#endif

	core_initialized = false;
	game_loaded = false;
}

RETRO_API unsigned retro_api_version(void) { return RETRO_API_VERSION; }

RETRO_API void retro_get_system_info(struct retro_system_info *info)
{
	memset(info, 0, sizeof(*info));
	info->library_name = "OpenTTD";
	info->library_version = "14.0";
	info->valid_extensions = NULL; /* Contentless */
	info->need_fullpath = false;
	info->block_extract = false;
}

RETRO_API void retro_get_system_av_info(struct retro_system_av_info *info)
{
	memset(info, 0, sizeof(*info));
	info->geometry.base_width = video_width;
	info->geometry.base_height = video_height;
	info->geometry.max_width = 1920;
	info->geometry.max_height = 1080;
	info->geometry.aspect_ratio = (float)video_width / (float)video_height;
	info->timing.fps = 60.0;
	info->timing.sample_rate = AUDIO_SAMPLE_RATE;
}

RETRO_API void retro_set_controller_port_device(unsigned port, unsigned device)
{
	(void)port; (void)device;
}

RETRO_API void retro_reset(void)
{
	if (log_cb) log_cb(RETRO_LOG_INFO, "[OpenTTD] retro_reset\n");
	/* Could restart the game here */
}

RETRO_API void retro_run(void)
{
	poll_input();

#if defined(_WIN32) && defined(_MSC_VER)
	static bool in_seh_handler = false;
	static auto SehFilter = [](EXCEPTION_POINTERS *ep) -> LONG {
		if (in_seh_handler) return EXCEPTION_EXECUTE_HANDLER;
		in_seh_handler = true;

		if (log_cb != nullptr && ep != nullptr && ep->ExceptionRecord != nullptr && ep->ContextRecord != nullptr) {
			log_cb(RETRO_LOG_ERROR, "[OpenTTD][SEH] Exception code=0x%08X addr=%p\n", (unsigned)ep->ExceptionRecord->ExceptionCode, ep->ExceptionRecord->ExceptionAddress);
		}

		if (log_cb != nullptr && ep != nullptr && ep->ContextRecord != nullptr) {
			LibraryLoader dbghelp("dbghelp.dll");
			struct ProcPtrs {
				BOOL (WINAPI * pSymInitialize)(HANDLE, PCSTR, BOOL);
				BOOL (WINAPI * pSymSetOptions)(DWORD);
				BOOL (WINAPI * pSymSetSearchPath)(HANDLE, PCSTR);
				BOOL (WINAPI * pSymCleanup)(HANDLE);
				BOOL (WINAPI * pStackWalk64)(DWORD, HANDLE, HANDLE, LPSTACKFRAME64, PVOID, PREAD_PROCESS_MEMORY_ROUTINE64, PFUNCTION_TABLE_ACCESS_ROUTINE64, PGET_MODULE_BASE_ROUTINE64, PTRANSLATE_ADDRESS_ROUTINE64);
				PVOID (WINAPI * pSymFunctionTableAccess64)(HANDLE, DWORD64);
				DWORD64 (WINAPI * pSymGetModuleBase64)(HANDLE, DWORD64);
				BOOL (WINAPI * pSymGetModuleInfo64)(HANDLE, DWORD64, PIMAGEHLP_MODULE64);
				BOOL (WINAPI * pSymGetSymFromAddr64)(HANDLE, DWORD64, PDWORD64, PIMAGEHLP_SYMBOL64);
				BOOL (WINAPI * pSymGetLineFromAddr64)(HANDLE, DWORD64, PDWORD, PIMAGEHLP_LINE64);
			} proc = {
				dbghelp.GetFunction("SymInitialize"),
				dbghelp.GetFunction("SymSetOptions"),
				dbghelp.GetFunction("SymSetSearchPath"),
				dbghelp.GetFunction("SymCleanup"),
				dbghelp.GetFunction("StackWalk64"),
				dbghelp.GetFunction("SymFunctionTableAccess64"),
				dbghelp.GetFunction("SymGetModuleBase64"),
				dbghelp.GetFunction("SymGetModuleInfo64"),
				dbghelp.GetFunction("SymGetSymFromAddr64"),
				dbghelp.GetFunction("SymGetLineFromAddr64"),
			};

			if (!dbghelp.HasError() && proc.pSymInitialize != nullptr && proc.pStackWalk64 != nullptr) {
				HANDLE hCur = GetCurrentProcess();
				proc.pSymInitialize(hCur, nullptr, TRUE);
				if (proc.pSymSetSearchPath != nullptr && ep->ExceptionRecord != nullptr) {
					HMODULE hMod = nullptr;
					char mod_path[MAX_PATH] = {};
					if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)ep->ExceptionRecord->ExceptionAddress, &hMod) && hMod != nullptr) {
						GetModuleFileNameA(hMod, mod_path, (DWORD)std::size(mod_path));
					}
					std::string symbol_path;
					if (mod_path[0] != '\0') {
						symbol_path = mod_path;
						size_t last_sep = symbol_path.find_last_of("\\/");
						if (last_sep != std::string::npos) symbol_path.resize(last_sep);
					}
					if (!symbol_path.empty()) {
						proc.pSymSetSearchPath(hCur, symbol_path.c_str());
						log_cb(RETRO_LOG_ERROR, "[OpenTTD][SEH] SymSearchPath=%s\n", symbol_path.c_str());
					}
				}
				if (proc.pSymSetOptions != nullptr) proc.pSymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_UNDNAME);

				STACKFRAME64 frame{};
#ifdef _M_AMD64
				frame.AddrPC.Offset = ep->ContextRecord->Rip;
				frame.AddrFrame.Offset = ep->ContextRecord->Rbp;
				frame.AddrStack.Offset = ep->ContextRecord->Rsp;
				DWORD machine = IMAGE_FILE_MACHINE_AMD64;
#elif defined(_M_IX86)
				frame.AddrPC.Offset = ep->ContextRecord->Eip;
				frame.AddrFrame.Offset = ep->ContextRecord->Ebp;
				frame.AddrStack.Offset = ep->ContextRecord->Esp;
				DWORD machine = IMAGE_FILE_MACHINE_I386;
#elif defined(_M_ARM64)
				frame.AddrPC.Offset = ep->ContextRecord->Pc;
				frame.AddrFrame.Offset = ep->ContextRecord->Fp;
				frame.AddrStack.Offset = ep->ContextRecord->Sp;
				DWORD machine = IMAGE_FILE_MACHINE_ARM64;
#else
				DWORD machine = 0;
#endif
				frame.AddrPC.Mode = AddrModeFlat;
				frame.AddrFrame.Mode = AddrModeFlat;
				frame.AddrStack.Mode = AddrModeFlat;

				CONTEXT ctx = *ep->ContextRecord;

				static const uint MAX_SYMBOL_LEN = 512;
				static const uint MAX_FRAMES = 48;
				std::array<char, sizeof(IMAGEHLP_SYMBOL64) + MAX_SYMBOL_LEN> sym_info_raw{};
				IMAGEHLP_SYMBOL64 *sym_info = reinterpret_cast<IMAGEHLP_SYMBOL64*>(sym_info_raw.data());
				sym_info->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
				sym_info->MaxNameLength = MAX_SYMBOL_LEN;

				log_cb(RETRO_LOG_ERROR, "[OpenTTD][SEH] Stacktrace (top %u):\n", (unsigned)MAX_FRAMES);
				for (uint num = 0; num < MAX_FRAMES; num++) {
					if (!proc.pStackWalk64(machine, hCur, GetCurrentThread(), &frame, &ctx, nullptr, proc.pSymFunctionTableAccess64, proc.pSymGetModuleBase64, nullptr)) break;
					if (frame.AddrPC.Offset == 0) break;

					std::string_view mod_name = "???";
					IMAGEHLP_MODULE64 module;
					module.SizeOfStruct = sizeof(module);
					if (proc.pSymGetModuleInfo64 != nullptr && proc.pSymGetModuleInfo64(hCur, frame.AddrPC.Offset, &module)) {
						mod_name = module.ModuleName;
					}

					DWORD64 offset = 0;
					std::string sym_name;
					if (proc.pSymGetSymFromAddr64 != nullptr && proc.pSymGetSymFromAddr64(hCur, frame.AddrPC.Offset, &offset, sym_info)) {
						sym_name = sym_info->Name;
					}

					DWORD line_offs = 0;
					IMAGEHLP_LINE64 line;
					line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
					bool have_line = (proc.pSymGetLineFromAddr64 != nullptr && proc.pSymGetLineFromAddr64(hCur, frame.AddrPC.Offset, &line_offs, &line));

					if (!sym_name.empty()) {
						if (have_line) {
							log_cb(RETRO_LOG_ERROR, "[OpenTTD][SEH]  %2u %20.*s 0x%llX %s + %llu (%s:%lu)\n", (unsigned)num, (int)mod_name.size(), mod_name.data(), (unsigned long long)frame.AddrPC.Offset, sym_name.c_str(), (unsigned long long)offset, line.FileName, (unsigned long)line.LineNumber);
						} else {
							log_cb(RETRO_LOG_ERROR, "[OpenTTD][SEH]  %2u %20.*s 0x%llX %s + %llu\n", (unsigned)num, (int)mod_name.size(), mod_name.data(), (unsigned long long)frame.AddrPC.Offset, sym_name.c_str(), (unsigned long long)offset);
						}
					} else {
						log_cb(RETRO_LOG_ERROR, "[OpenTTD][SEH]  %2u %20.*s 0x%llX\n", (unsigned)num, (int)mod_name.size(), mod_name.data(), (unsigned long long)frame.AddrPC.Offset);
					}
				}

				if (proc.pSymCleanup != nullptr) proc.pSymCleanup(hCur);
			}
		}

		return EXCEPTION_EXECUTE_HANDLER;
	};

	__try {
#endif

#ifdef WITH_LIBRETRO
	if (openttd_initialized) {
		/* Get the video driver and run a frame */
		VideoDriver_Libretro *driver = VideoDriver_Libretro::GetInstance();
		if (driver) {
			driver->RunFrame();

			/* Get the rendered frame and send to frontend */
			uint32_t *buffer = driver->GetVideoBuffer();
			unsigned w, h, pitch;
			driver->GetVideoSize(&w, &h, &pitch);

			if (buffer) {
				video_cb(buffer, w, h, pitch);
			}
		}
	} else {
		/* Show loading screen */
		static uint32_t loading_buffer[1280 * 720];
		static uint8_t frame = 0;
		frame++;

		/* Simple loading animation */
		for (unsigned y = 0; y < video_height; y++) {
			for (unsigned x = 0; x < video_width; x++) {
				uint8_t c = ((x + y + frame) % 32) < 16 ? 0x20 : 0x30;
				loading_buffer[y * video_width + x] = (c << 16) | (c << 8) | c;
			}
		}

		video_cb(loading_buffer, video_width, video_height, video_width * 4);
	}
#else
	/* Fallback test pattern */
	static uint32_t test_buffer[1280 * 720];
	static uint8_t frame = 0;
	frame++;

	for (unsigned y = 0; y < video_height; y++) {
		for (unsigned x = 0; x < video_width; x++) {
			uint8_t r = (x * 255) / video_width;
			uint8_t g = (y * 255) / video_height;
			uint8_t b = frame;
			test_buffer[y * video_width + x] = (r << 16) | (g << 8) | b;
		}
	}

	video_cb(test_buffer, video_width, video_height, video_width * 4);
#endif

	render_audio();

#if defined(_WIN32) && defined(_MSC_VER)
	} __except (SehFilter(GetExceptionInformation())) {
		/* Prevent spamming. */
		openttd_initialized = false;
		game_loaded = false;
		core_initialized = false;
		return;
	}
#endif
}

RETRO_API size_t retro_serialize_size(void) { return 0; }
RETRO_API bool retro_serialize(void *data, size_t size) { (void)data; (void)size; return false; }
RETRO_API bool retro_unserialize(const void *data, size_t size) { (void)data; (void)size; return false; }
RETRO_API void retro_cheat_reset(void) {}
RETRO_API void retro_cheat_set(unsigned index, bool enabled, const char *code) { (void)index; (void)enabled; (void)code; }

RETRO_API bool retro_load_game(const struct retro_game_info *game)
{
	if (log_cb) {
		log_cb(RETRO_LOG_INFO, "[OpenTTD] retro_load_game: %s\n",
			game && game->path ? game->path : "(contentless)");
	}

#ifdef WITH_LIBRETRO
	/* Initialize OpenTTD */
	if (!initialize_openttd()) {
		if (log_cb) log_cb(RETRO_LOG_ERROR, "[OpenTTD] Failed to initialize OpenTTD!\n");
		return false;
	}
#endif

	game_loaded = true;
	return true;
}

RETRO_API bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info)
{
	(void)game_type; (void)info; (void)num_info;
	return false;
}

RETRO_API void retro_unload_game(void)
{
	if (log_cb) log_cb(RETRO_LOG_INFO, "[OpenTTD] retro_unload_game\n");
	game_loaded = false;
}

RETRO_API unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
RETRO_API void *retro_get_memory_data(unsigned id) { (void)id; return nullptr; }
RETRO_API size_t retro_get_memory_size(unsigned id) { (void)id; return 0; }

/* LibretroCore namespace implementation */
namespace LibretroCore {

bool IsRunning() { return game_loaded; }

void GetVideoInfo(unsigned *w, unsigned *h, unsigned *p)
{
	if (w) *w = video_width;
	if (h) *h = video_height;
	if (p) *p = video_width * sizeof(uint32_t);
}

bool SetVideoGeometry(unsigned w, unsigned h)
{
	if (w == 0 || h == 0) return false;
	if (w > 1920 || h > 1080) return false;

	video_width = w;
	video_height = h;

	{
		std::lock_guard<std::mutex> lock(input_mutex);
		if (mouse_x < 0) mouse_x = 0;
		if (mouse_y < 0) mouse_y = 0;
		if (mouse_x >= (int)video_width) mouse_x = video_width - 1;
		if (mouse_y >= (int)video_height) mouse_y = video_height - 1;
	}

	if (environ_cb) {
		retro_game_geometry geom{};
		geom.base_width = video_width;
		geom.base_height = video_height;
		geom.max_width = 1920;
		geom.max_height = 1080;
		geom.aspect_ratio = (float)video_width / (float)video_height;
		environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &geom);
	}

	return true;
}

void *GetVideoBuffer() { return nullptr; }

void GetMouseState(int *x, int *y, bool *left, bool *right, bool *middle, int *wheel)
{
	std::lock_guard<std::mutex> lock(input_mutex);
	if (x) *x = mouse_x;
	if (y) *y = mouse_y;
	if (left) *left = mouse_left;
	if (right) *right = mouse_right;
	if (middle) *middle = mouse_middle;
	if (wheel) {
		*wheel = mouse_wheel;
		mouse_wheel = 0; /* Clear after reading */
	}
}

uint16_t GetKeyboardModifiers()
{
	std::lock_guard<std::mutex> lock(key_event_mutex);
	return keyboard_modifiers;
}

bool GetNextKeyEvent(bool *down, unsigned *keycode, uint32_t *character, uint16_t *modifiers)
{
	std::lock_guard<std::mutex> lock(key_event_mutex);
	if (pending_key_events.empty()) return false;

	KeyEvent evt = pending_key_events.front();
	pending_key_events.erase(pending_key_events.begin());

	if (down) *down = evt.down;
	if (keycode) *keycode = evt.keycode;
	if (character) *character = evt.character;
	if (modifiers) *modifiers = evt.modifiers;
	return true;
}

const char *GetSystemDirectory() { return system_directory.c_str(); }
const char *GetSaveDirectory() { return save_directory.c_str(); }

void Log(enum retro_log_level level, const char *fmt, ...)
{
	if (!log_cb) return;

	va_list args;
	va_start(args, fmt);
	char buf[1024];
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	log_cb(level, "%s", buf);
}

void MixAudio(const int16_t *samples, size_t count)
{
	if (audio_batch_cb && samples) {
		audio_batch_cb(samples, count);
	}
}

struct retro_vfs_interface *GetVFS() { return nullptr; }
bool HasVFS() { return false; }

} /* namespace LibretroCore */
