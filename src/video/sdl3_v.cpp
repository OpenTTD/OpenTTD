/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file sdl3_v.cpp Implementation of the SDL3 video driver. */

#include "../stdafx.h"
#include "../openttd.h"
#include "../gfx_func.h"
#include "../blitter/factory.hpp"
#include "../thread.h"
#include "../progress.h"
#include "../core/math_func.hpp"
#include "../core/geometry_func.hpp"
#include "../core/utf8.hpp"
#include "../fileio_func.h"
#include "../framerate_type.h"
#include "../window_func.h"
#include "sdl3_v.h"
#include <SDL3/SDL.h>
#ifdef __EMSCRIPTEN__
#	include <emscripten.h>
#	include <emscripten/html5.h>
#endif

#include "../safeguards.h"

void VideoDriver_SDL_Base::MakeDirty(int left, int top, int width, int height)
{
	Rect r = {left, top, left + width, top + height};
	this->dirty_rect = BoundingRect(this->dirty_rect, r);
}

void VideoDriver_SDL_Base::CheckPaletteAnim()
{
	if (!CopyPalette(this->local_palette)) return;
	this->MakeDirty(0, 0, _screen.width, _screen.height);
}

/** Default windowed resolutions. */
static const Dimension default_resolutions[] = {
	{  640,  480 },
	{  800,  600 },
	{ 1024,  768 },
	{ 1152,  864 },
	{ 1280,  800 },
	{ 1280,  960 },
	{ 1280, 1024 },
	{ 1400, 1050 },
	{ 1600, 1200 },
	{ 1680, 1050 },
	{ 1920, 1200 }
};

/** Detect available screen resolutions. */
static void FindResolutions()
{
	_resolutions.clear();

	int num_displays;
	SDL_DisplayID *displays = SDL_GetDisplays(&num_displays);

	for (int display_idx = 0; display_idx < num_displays; display_idx++) {
		SDL_DisplayID display_id = displays[display_idx];
		int modes_count = 0;
		SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes(
			display_id, &modes_count);

		for (int i = 0; i < modes_count; i++) {
			const SDL_DisplayMode *mode = modes[i];

			if (mode->w < 640 || mode->h < 480) continue;
			if (std::ranges::find(_resolutions, Dimension(mode->w, mode->h)) != _resolutions.end()) continue;
			_resolutions.emplace_back(mode->w, mode->h);
		}

		SDL_free(modes);
	}

	SDL_free(displays);

	/* We have found no resolutions, show the default list */
	if (_resolutions.empty()) {
		_resolutions.assign(std::begin(default_resolutions), std::end(default_resolutions));
	}

	SortResolutions();
}

/**
 * Get an available video mode.
 * @param w Width (in/out).
 * @param h Height (in/out).
 */
static void GetAvailableVideoMode(uint *w, uint *h)
{
	/* All modes available? */
	if (!_fullscreen || _resolutions.empty()) return;

	/* Is the wanted mode among the available modes? */
	if (std::ranges::find(_resolutions, Dimension(*w, *h)) != _resolutions.end()) return;

	/* Use the closest possible resolution */
	uint best = 0;
	uint delta = Delta(_resolutions[0].width, *w) * Delta(_resolutions[0].height, *h);
	for (uint i = 1; i != _resolutions.size(); ++i) {
		uint newdelta = Delta(_resolutions[i].width, *w) * Delta(_resolutions[i].height, *h);
		if (newdelta < delta) {
			best = i;
			delta = newdelta;
		}
	}
	*w = _resolutions[best].width;
	*h = _resolutions[best].height;
}

/**
 * Select startup display.
 * @param startup_display Preferred display index.
 * @return Selected display index.
 */
static uint FindStartupDisplay(uint startup_display)
{
	int num_displays;
	SDL_DisplayID *displays = SDL_GetDisplays(&num_displays);

	/* If the user indicated a valid monitor, use that. */
	if (IsInsideBS(startup_display, 0, num_displays)) return startup_display;

	/* Mouse position decides which display to use. */
	float mx, my;
	SDL_GetGlobalMouseState(&mx, &my);
	for (int display = 0; display < num_displays; ++display) {
		SDL_Rect r;
		if (SDL_GetDisplayBounds(display, &r) == 0 && IsInsideBS(mx, r.x, r.w) && IsInsideBS(my, r.y, r.h)) {
			Debug(driver, 1, "SDL3: Mouse is at ({}, {}), use display {} ({}, {}, {}, {})", mx, my, display, r.x, r.y, r.w, r.h);
			return display;
		}
	}

	SDL_free(displays);

	return 0;
}

/**
 * Indicate to the driver the client-size might have changed.
 * @param w The new width of the window.
 * @param h The new height of the window.
 * @param force Whether to force full reallocation, instead of not reallocating when size did not change.
 */
void VideoDriver_SDL_Base::ClientSizeChanged(int w, int h, bool force)
{
	/* Allocate backing store of the new size. */
	if (this->AllocateBackingStore(w, h, force)) {
		CopyPalette(this->local_palette, true);

		BlitterFactory::GetCurrentBlitter()->PostResize();

		GameSizeChanged();
	}
}

bool VideoDriver_SDL_Base::CreateMainWindow(uint w, uint h, uint flags)
{
	if (this->sdl_window != nullptr) return true;

	flags |= SDL_WINDOW_RESIZABLE;

	if (_fullscreen) {
		flags |= SDL_WINDOW_FULLSCREEN;
	}

	std::string caption = VideoDriver::GetCaption();
	this->sdl_window = SDL_CreateWindow(
		caption.c_str(),
		w, h,
		flags);

	if (this->sdl_window == nullptr) {
		Debug(driver, 0, "SDL3: Couldn't allocate a window to draw on: {}", SDL_GetError());
		return false;
	}

	std::string icon_path = FioFindFullPath(Subdirectory::Baseset, "openttd.32.bmp");
	if (!icon_path.empty()) {
		/* Give the application an icon */
		SDL_Surface *icon = SDL_LoadBMP(icon_path.c_str());
		if (icon != nullptr) {
			/* Get the colourkey, which will be magenta */
			uint32_t rgbmap = SDL_MapSurfaceRGB(icon, 255, 0, 255);

			if (!SDL_SetSurfaceColorKey(icon, true, rgbmap)) {
				Debug(driver, 1, "SDL3: SDL_SetSurfaceColorKey failed: {}",
					  SDL_GetError());
			}
			SDL_SetWindowIcon(this->sdl_window, icon);
			SDL_DestroySurface(icon);
		}
	}

	return true;
}

/**
 * Create or resize the main SDL drawing surface.
 * @param w Requested width.
 * @param h Requested height.
 * @param resize Whether this is a resize of an existing surface.
 * @return True if the surface was created successfully.
 */
bool VideoDriver_SDL_Base::CreateMainSurface(uint w, uint h, bool resize)
{
	GetAvailableVideoMode(&w, &h);
	Debug(driver, 1, "SDL3: using mode {}x{}", w, h);

	if (!this->CreateMainWindow(w, h)) return false;
	if (resize) SDL_SetWindowSize(this->sdl_window, w, h);
	this->ClientSizeChanged(w, h, true);

	/* When in full screen, we will always have the mouse cursor
	 * within the window, even though SDL does not give us the
	 * appropriate event to know this. */
	if (_fullscreen) _cursor.in_window = true;

	return true;
}

void VideoDriver_SDL_Base::ClaimMousePointer()
{
	/* Emscripten never claims the pointer, so we do not need to change the cursor visibility. */
#ifndef __EMSCRIPTEN__
	SDL_HideCursor();
#endif
}

/**
 * This is called to indicate that an edit box has gained focus, text input mode should be enabled.
 */
void VideoDriver_SDL_Base::EditBoxGainedFocus()
{
	if (!this->edit_box_focused) {
		SDL_StartTextInput(this->sdl_window);
		this->edit_box_focused = true;
	}
}

/**
 * This is called to indicate that an edit box has lost focus, text input mode should be disabled.
 */
void VideoDriver_SDL_Base::EditBoxLostFocus()
{
	if (this->edit_box_focused) {
		SDL_StopTextInput(this->sdl_window);
		this->edit_box_focused = false;
	}
}

std::vector<int> VideoDriver_SDL_Base::GetListOfMonitorRefreshRates()
{
	std::vector<int> rates = {};
	int num_displays;
	SDL_DisplayID *displays = SDL_GetDisplays(&num_displays);
	if (displays == nullptr) return rates;

	for (int i = 0; i < num_displays; i++) {
		SDL_DisplayID display_id = displays[i];

		int modes_count = 0;
		SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes(
			display_id, &modes_count);
		if (modes == nullptr) continue;

		for (int mode_idx = 0; mode_idx < modes_count; mode_idx++) {
			const SDL_DisplayMode *mode = modes[mode_idx];
			if (mode != nullptr && mode->refresh_rate) {
				rates.push_back(
					static_cast<int>(std::round(mode->refresh_rate)));
			}
		}

		SDL_free(modes);
	}

	SDL_free(displays);
	return rates;
}


/** Mapping from a contiguous SDL keycode range to internal keycodes. */
struct SDLVkMapping {
	const SDL_Keycode vk_from;
	const uint8_t vk_count;
	const uint8_t map_to;
	const bool unprintable;

	/**
	 * Create a keycode range mapping.
	 * @param vk_first First SDL keycode.
	 * @param vk_last Last SDL keycode.
	 * @param map_first First internal keycode.
	 * @param map_last Last internal keycode.
	 * @param unprintable Whether the mapped keys are unprintable.
	 */
	constexpr SDLVkMapping(
		SDL_Keycode vk_first, SDL_Keycode vk_last,
		uint8_t map_first, uint8_t map_last,
		bool unprintable)
		: vk_from(vk_first), vk_count(vk_last - vk_first + 1),
		  map_to(map_first), unprintable(unprintable)
		{
			const auto vk_range = vk_last - vk_first;
			const auto map_range = static_cast<SDL_Keycode>(map_last) -
				static_cast<SDL_Keycode>(map_first);

			assert(vk_range == map_range);
		}
};

/** Add a single printable key mapping. */
#define AS(x, z) {x, x, z, z, false}
/** Add a contiguous printable key mapping range. */
#define AM(x, y, z, w) {x, y, z, w, false}
/** Add a single unprintable key mapping. */
#define AS_UP(x, z) {x, x, z, z, true}
/** Add a contiguous unprintable key mapping range. */
#define AM_UP(x, y, z, w) {x, y, z, w, true}

/** SDL to internal key mapping table. */
static constexpr SDLVkMapping _vk_mapping[] = {
	/* Pageup stuff + up/down */
	AS_UP(SDLK_PAGEUP,   WKC_PAGEUP),
	AS_UP(SDLK_PAGEDOWN, WKC_PAGEDOWN),
	AS_UP(SDLK_UP,     WKC_UP),
	AS_UP(SDLK_DOWN,   WKC_DOWN),
	AS_UP(SDLK_LEFT,   WKC_LEFT),
	AS_UP(SDLK_RIGHT,  WKC_RIGHT),

	AS_UP(SDLK_HOME,   WKC_HOME),
	AS_UP(SDLK_END,    WKC_END),

	AS_UP(SDLK_INSERT, WKC_INSERT),
	AS_UP(SDLK_DELETE, WKC_DELETE),

	/* Map letters & digits */
	AM(SDLK_A, SDLK_Z, 'A', 'Z'),
	AM(SDLK_0, SDLK_9, '0', '9'),

	AS_UP(SDLK_ESCAPE,    WKC_ESC),
	AS_UP(SDLK_PAUSE,     WKC_PAUSE),
	AS_UP(SDLK_BACKSPACE, WKC_BACKSPACE),

	AS(SDLK_SPACE,     WKC_SPACE),
	AS(SDLK_RETURN,    WKC_RETURN),
	AS(SDLK_TAB,       WKC_TAB),

	/* Function keys */
	AM_UP(SDLK_F1, SDLK_F12, WKC_F1, WKC_F12),

	/* Numeric part. */
	AS(SDLK_KP_1,        '1'),
	AS(SDLK_KP_2,        '2'),
	AS(SDLK_KP_3,        '3'),
	AS(SDLK_KP_4,        '4'),
	AS(SDLK_KP_5,        '5'),
	AS(SDLK_KP_6,        '6'),
	AS(SDLK_KP_7,        '7'),
	AS(SDLK_KP_8,        '8'),
	AS(SDLK_KP_9,        '9'),
	AS(SDLK_KP_0,        '0'),
	AS(SDLK_KP_DIVIDE,   WKC_NUM_DIV),
	AS(SDLK_KP_MULTIPLY, WKC_NUM_MUL),
	AS(SDLK_KP_MINUS,    WKC_NUM_MINUS),
	AS(SDLK_KP_PLUS,     WKC_NUM_PLUS),
	AS(SDLK_KP_ENTER,    WKC_NUM_ENTER),
	AS(SDLK_KP_PERIOD,   WKC_NUM_DECIMAL),

	/* Other non-letter keys */
	AS(SDLK_SLASH,        WKC_SLASH),
	AS(SDLK_SEMICOLON,    WKC_SEMICOLON),
	AS(SDLK_EQUALS,       WKC_EQUALS),
	AS(SDLK_LEFTBRACKET,  WKC_L_BRACKET),
	AS(SDLK_BACKSLASH,    WKC_BACKSLASH),
	AS(SDLK_RIGHTBRACKET, WKC_R_BRACKET),

	AS(SDLK_APOSTROPHE,   WKC_SINGLEQUOTE),
	AS(SDLK_COMMA,   WKC_COMMA),
	AS(SDLK_MINUS,   WKC_MINUS),
	AS(SDLK_PERIOD,  WKC_PERIOD)
};

/**
 * Convert an SDL keyboard event to internal key representation.
 * @param key_event SDL keyboard event.
 * @param character Output character (if any).
 * @return Internal key code.
 */
static uint ConvertSdlKeyIntoMy(SDL_KeyboardEvent *key_event, char32_t *character)
{
	uint key = 0;
	bool unprintable = false;

	SDL_Keycode sym = key_event->key;
	SDL_Scancode scancode = key_event->scancode;
	SDL_Keymod mod = key_event->mod;

	for (const auto &map : _vk_mapping) {
		if (IsInsideBS(sym, map.vk_from, map.vk_count)) {
			key = sym - map.vk_from + map.map_to;
			unprintable = map.unprintable;
			break;
		}
	}

	/* check scancode for BACKQUOTE key, because we want the key left of "1", not anything else (on non-US keyboards) */
	if (scancode == SDL_SCANCODE_GRAVE) key = WKC_BACKQUOTE;

	/* META are the command keys on mac */
	if (mod & SDL_KMOD_GUI)   key |= WKC_META;
	if (mod & SDL_KMOD_SHIFT) key |= WKC_SHIFT;
	if (mod & SDL_KMOD_CTRL)  key |= WKC_CTRL;
	if (mod & SDL_KMOD_ALT)   key |= WKC_ALT;

	/* The mod keys have no character. Prevent '?' */
	if (mod & SDL_KMOD_GUI ||
		mod & SDL_KMOD_CTRL ||
		mod & SDL_KMOD_ALT ||
		unprintable) {
		*character = WKC_NONE;
	} else {
		*character = sym;
	}

	return key;
}

/**
 * Convert a SDL_Keycode into our own key codes.
 * @param kc The SDL key code.
 * @return OpenTTD's internal key code.
 */
static uint ConvertSdlKeycodeIntoMy(SDL_Keycode kc)
{
	uint key = 0;

	for (const auto &map : _vk_mapping) {
		if (IsInsideBS(kc, map.vk_from, map.vk_count)) {
			key = kc - map.vk_from + map.map_to;
			break;
		}
	}

	/* check scancode for BACKQUOTE key, because we want the key left
	 * of "1", not anything else (on non-US keyboards) */
	SDL_Scancode sc = SDL_GetScancodeFromKey(kc, nullptr);
	if (sc == SDL_SCANCODE_GRAVE) key = WKC_BACKQUOTE;

	return key;
}

bool VideoDriver_SDL_Base::PollEvent()
{
	SDL_Event ev;

	if (!SDL_PollEvent(&ev)) return false;

	switch (ev.type) {
		case SDL_EVENT_MOUSE_MOTION: {
			int32_t x = ev.motion.x;
			int32_t y = ev.motion.y;

			if (_cursor.fix_at) {
				/* Get all queued mouse events now in case we have to warp the cursor. In the
				 * end, we only care about the current mouse position and not bygone events. */
				while (SDL_PeepEvents(&ev, 1, SDL_GETEVENT, SDL_EVENT_MOUSE_MOTION, SDL_EVENT_MOUSE_MOTION)) {
					x = ev.motion.x;
					y = ev.motion.y;
				}
			}

			if (_cursor.UpdateCursorPosition(x, y)) {
				SDL_WarpMouseInWindow(this->sdl_window, _cursor.pos.x, _cursor.pos.y);
			}
			HandleMouseEvents();
			break;
		}

		case SDL_EVENT_MOUSE_WHEEL: {
			if (ev.wheel.y > 0) {
				_cursor.wheel--;
			} else if (ev.wheel.y < 0) {
				_cursor.wheel++;
			}

			/* Handle 2D scrolling. */
			const float SCROLL_BUILTIN_MULTIPLIER = 14.0f;
			_cursor.v_wheel -= ev.wheel.mouse_x * SCROLL_BUILTIN_MULTIPLIER * _settings_client.gui.scrollwheel_multiplier;
			_cursor.h_wheel += ev.wheel.mouse_y * SCROLL_BUILTIN_MULTIPLIER * _settings_client.gui.scrollwheel_multiplier;
			_cursor.wheel_moved = true;
			break;
		}

		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			if (_rightclick_emulate && SDL_GetModState() & SDL_KMOD_CTRL) {
				ev.button.button = SDL_BUTTON_RIGHT;
			}

			switch (ev.button.button) {
				case SDL_BUTTON_LEFT:
					_left_button_down = true;
					break;

				case SDL_BUTTON_RIGHT:
					_right_button_down = true;
					_right_button_clicked = true;
					break;

				default: break;
			}
			HandleMouseEvents();
			break;

		case SDL_EVENT_MOUSE_BUTTON_UP:
			if (_rightclick_emulate) {
				_right_button_down = false;
				_left_button_down = false;
				_left_button_clicked = false;
			} else if (ev.button.button == SDL_BUTTON_LEFT) {
				_left_button_down = false;
				_left_button_clicked = false;
			} else if (ev.button.button == SDL_BUTTON_RIGHT) {
				_right_button_down = false;
			}
			HandleMouseEvents();
			break;

		case SDL_EVENT_QUIT:
			HandleExitGameRequest();
			break;

		case SDL_EVENT_KEY_DOWN: // Toggle full-screen on ALT + ENTER/F
			if ((ev.key.mod & (SDL_KMOD_ALT | SDL_KMOD_GUI)) &&
				(ev.key.key == SDLK_RETURN || ev.key.key == SDLK_F)) {
				if (ev.key.repeat == 0) ToggleFullScreen(!_fullscreen);
			} else {
				char32_t character;

				uint keycode = ConvertSdlKeyIntoMy(&ev.key, &character);
				/* Only handle non-text keys here. Text is handled in
				 * SDL_TEXTINPUT below. */
				if (!this->edit_box_focused ||
					keycode == WKC_DELETE ||
					keycode == WKC_NUM_ENTER ||
					keycode == WKC_LEFT ||
					keycode == WKC_RIGHT ||
					keycode == WKC_UP ||
					keycode == WKC_DOWN ||
					keycode == WKC_HOME ||
					keycode == WKC_END ||
					keycode & WKC_META ||
					keycode & WKC_CTRL ||
					keycode & WKC_ALT ||
					(keycode >= WKC_F1 && keycode <= WKC_F12) ||
					!IsValidChar(character, CS_ALPHANUMERAL)) {
					HandleKeypress(keycode, character);
				}
			}
			break;

		case SDL_EVENT_TEXT_INPUT: {
			if (!this->edit_box_focused) break;
			SDL_Keycode kc = SDL_GetKeyFromName(ev.text.text);
			uint keycode = ConvertSdlKeycodeIntoMy(kc);

			if (keycode == WKC_BACKQUOTE && FocusedWindowIsConsole()) {
				auto [len, c] = DecodeUtf8(ev.text.text);
				if (len > 0) HandleKeypress(keycode, c);
			} else {
				HandleTextInput(ev.text.text);
			}
			break;
		}
		case SDL_EVENT_WINDOW_EXPOSED: {
			/* Force a redraw of the entire screen. */
			this->MakeDirty(0, 0, _screen.width, _screen.height);
			break;
		}
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
			int w = std::max(ev.window.data1, 64);
			int h = std::max(ev.window.data2, 64);
			CreateMainSurface(w, h, w != ev.window.data1 || h != ev.window.data2);
			break;
		}
		case SDL_EVENT_WINDOW_MOUSE_ENTER: {
			/* mouse entered the window, enable cursor */
			_cursor.in_window = true;
			/* Ensure pointer lock will not occur. */
			SDL_SetWindowRelativeMouseMode(this->sdl_window, false);
			break;
		}
		case SDL_EVENT_WINDOW_MOUSE_LEAVE: {
			/* mouse left the window, undraw cursor */
			UndrawMouseCursor();
			_cursor.in_window = false;
			break;
		}
	}

	return true;
}

/**
 * Initialize SDL video subsystem.
 * @return True on success.
 */
static std::optional<std::string_view> InitializeSDL()
{
	/* Check if the video-driver is already initialized. */
	if (SDL_WasInit(SDL_INIT_VIDEO) != 0) return std::nullopt;

#ifdef SDL_HINT_APP_NAME
	SDL_SetHint(SDL_HINT_APP_NAME, "OpenTTD");
#endif

	if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) return SDL_GetError();
	return std::nullopt;
}

std::optional<std::string_view> VideoDriver_SDL_Base::Initialize()
{
	this->UpdateAutoResolution();

	auto error = InitializeSDL();
	if (error) return error;

	FindResolutions();
	Debug(driver, 2, "Resolution for display: {}x{}", _cur_resolution.width, _cur_resolution.height);

	return std::nullopt;
}

std::optional<std::string_view> VideoDriver_SDL_Base::Start(const StringList &param)
{
	if (BlitterFactory::GetCurrentBlitter()->GetScreenDepth() == 0) return "Only real blitters supported";

	auto error = this->Initialize();
	if (error) return error;

#ifdef SDL_HINT_MOUSE_AUTO_CAPTURE
	if (GetDriverParamBool(param, "no_mouse_capture")) {
		/* By default SDL captures the mouse, while a button is pressed.
		 * This is annoying during debugging, when OpenTTD is suspended while the button was pressed.
		 */
		if (!SDL_SetHint(SDL_HINT_MOUSE_AUTO_CAPTURE, "0")) return SDL_GetError();
	}
#endif

	this->startup_display = FindStartupDisplay(GetDriverParamInt(param, "display", -1));

	if (!CreateMainSurface(_cur_resolution.width, _cur_resolution.height, false)) {
		return SDL_GetError();
	}

	const char *dname = SDL_GetCurrentVideoDriver();
	Debug(driver, 1, "SDL3: using driver '{}'", dname);

	this->driver_info = this->GetName();
	this->driver_info += " (";
	this->driver_info += dname;
	this->driver_info += ")";

	MarkWholeScreenDirty();

	SDL_StopTextInput(this->sdl_window);
	this->edit_box_focused = false;

#ifdef __EMSCRIPTEN__
	this->is_game_threaded = false;
#else
	this->is_game_threaded = !GetDriverParamBool(param, "no_threads") && !GetDriverParamBool(param, "no_thread");
#endif

	return std::nullopt;
}

void VideoDriver_SDL_Base::Stop()
{
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	if (SDL_WasInit(0) == 0) {
		SDL_Quit(); // If there's nothing left, quit SDL
	}
}

void VideoDriver_SDL_Base::InputLoop()
{
	uint32_t mod = SDL_GetModState();
	const bool *key_states = SDL_GetKeyboardState(nullptr);

	bool old_ctrl_pressed = _ctrl_pressed;

	_ctrl_pressed  = !!(mod & SDL_KMOD_CTRL);
	_shift_pressed = !!(mod & SDL_KMOD_SHIFT);

	/* Speedup when pressing tab, except when using ALT+TAB
	 * to switch to another application. */
	this->fast_forward_key_pressed = key_states[SDL_SCANCODE_TAB] && (mod & SDL_KMOD_ALT) == 0;

	/* Determine which directional keys are down. */
	_dirkeys.Set(DirectionKey::Left, key_states[SDL_SCANCODE_LEFT]);
	_dirkeys.Set(DirectionKey::Up, key_states[SDL_SCANCODE_UP]);
	_dirkeys.Set(DirectionKey::Right, key_states[SDL_SCANCODE_RIGHT]);
	_dirkeys.Set(DirectionKey::Down, key_states[SDL_SCANCODE_DOWN]);

	if (old_ctrl_pressed != _ctrl_pressed) HandleCtrlChanged();
}

void VideoDriver_SDL_Base::LoopOnce()
{
	if (_exit_game) {
#ifdef __EMSCRIPTEN__
		/* Emscripten is event-driven, and as such the main loop is inside
		 * the browser. So if _exit_game goes true, the main loop ends (the
		 * cancel call), but we still have to call the cleanup that is
		 * normally done at the end of the main loop for non-Emscripten.
		 * After that, Emscripten just halts, and the HTML shows a nice
		 * "bye, see you next time" message. */
		extern void PostMainLoop();
		PostMainLoop();

		emscripten_cancel_main_loop();
		emscripten_exit_pointerlock();
		/* In effect, the game ends here. As emscripten_set_main_loop() caused
		 * the stack to be unwound, the code after MainLoop() in
		 * openttd_main() is never executed. */
		if (_game_mode == GM_BOOTSTRAP) {
			EM_ASM(if (window["openttd_bootstrap_reload"]) openttd_bootstrap_reload());
		} else {
			EM_ASM(if (window["openttd_exit"]) openttd_exit());
		}
#endif
		return;
	}

	this->Tick();

	/* Emscripten is running an event-based mainloop; there is already some
	 * downtime between each iteration, so no need to sleep. */
#ifndef __EMSCRIPTEN__
	this->SleepTillNextTick();
#endif
}

void VideoDriver_SDL_Base::MainLoop()
{
#ifdef __EMSCRIPTEN__
	/* Run the main loop event-driven, based on RequestAnimationFrame. */
	emscripten_set_main_loop_arg(&this->EmscriptenLoop, this, 0, 1);
#else
	this->StartGameThread();

	while (!_exit_game) {
		LoopOnce();
	}

	this->StopGameThread();
#endif
}

bool VideoDriver_SDL_Base::ChangeResolution(int w, int h)
{
	return CreateMainSurface(w, h, true);
}

bool VideoDriver_SDL_Base::ToggleFullscreen(bool fullscreen)
{
	/* Remember current window size */
	int w, h;
	SDL_GetWindowSize(this->sdl_window, &w, &h);

	if (fullscreen) {
		/* Find fullscreen window size */
		SDL_DisplayID display_id = SDL_GetDisplayForWindow(this->sdl_window);
		const SDL_DisplayMode *dm = SDL_GetCurrentDisplayMode(display_id);

		if (dm == nullptr) {
			Debug(driver, 0, "SDL_GetCurrentDisplayMode() failed: {}", SDL_GetError());
		} else {
			SDL_SetWindowSize(this->sdl_window, dm->w, dm->h);
		}
	}

	Debug(driver, 1, "SDL3: Setting {}", fullscreen ? "fullscreen" : "windowed");
	int ret = SDL_SetWindowFullscreen(this->sdl_window, fullscreen ? SDL_WINDOW_FULLSCREEN : 0);
	if (ret == 0) {
		/* Switching resolution succeeded, set fullscreen value of window. */
		_fullscreen = fullscreen;
		if (!fullscreen) SDL_SetWindowSize(this->sdl_window, w, h);
	} else {
		Debug(driver, 0, "SDL_SetWindowFullscreen() failed: {}", SDL_GetError());
	}

	InvalidateWindowClassesData(WindowClass::GameOptions, 3);
	return ret == 0;
}

bool VideoDriver_SDL_Base::AfterBlitterChange()
{
	assert(BlitterFactory::GetCurrentBlitter()->GetScreenDepth() != 0);
	int w, h;
	SDL_GetWindowSize(this->sdl_window, &w, &h);
	return CreateMainSurface(w, h, false);
}

Dimension VideoDriver_SDL_Base::GetScreenSize() const
{
	SDL_DisplayID display_id = SDL_GetDisplayForWindow(this->sdl_window);
	const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(display_id);

	if (mode == nullptr) return VideoDriver::GetScreenSize();

	return { static_cast<uint>(mode->w), static_cast<uint>(mode->h) };
}

bool VideoDriver_SDL_Base::LockVideoBuffer()
{
	if (this->buffer_locked) return false;
	this->buffer_locked = true;

	_screen.dst_ptr = this->GetVideoPointer();
	assert(_screen.dst_ptr != nullptr);

	return true;
}

void VideoDriver_SDL_Base::UnlockVideoBuffer()
{
	if (_screen.dst_ptr != nullptr) {
		/* Hand video buffer back to the drawing backend. */
		this->ReleaseVideoPointer();
		_screen.dst_ptr = nullptr;
	}

	this->buffer_locked = false;
}

void VideoDriver_SDL_Base::SetScreensaverInhibited(bool inhibited)
{
	if (inhibited) {
		SDL_DisableScreenSaver();
	} else {
		SDL_EnableScreenSaver();
	}
}
