/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file sdl2_v.cpp Implementation of the SDL2 video driver. */

#include "../stdafx.h"
#include "../openttd.h"
#include "../gfx_func.h"
#include "../rev.h"
#include "../blitter/factory.hpp"
#include "../network/network.h"
#include "../thread.h"
#include "../progress.h"
#include "../core/random_func.hpp"
#include "../core/math_func.hpp"
#include "../core/mem_func.hpp"
#include "../core/geometry_func.hpp"
#include "../fileio_func.h"
#include "../framerate_type.h"
#include "../window_func.h"
#include "sdl2_v.h"
#include <SDL.h>
#include <mutex>
#include <condition_variable>
#ifdef __EMSCRIPTEN__
#	include <emscripten.h>
#	include <emscripten/html5.h>
#endif

#include "../safeguards.h"

static FVideoDriver_SDL iFVideoDriver_SDL;

static SDL_Window *_sdl_window;
static SDL_Surface *_sdl_surface;
static SDL_Surface *_sdl_rgb_surface;
static SDL_Surface *_sdl_real_surface;

/** Whether the drawing is/may be done in a separate thread. */
static bool _draw_threaded;
/** Mutex to keep the access to the shared memory controlled. */
static std::recursive_mutex *_draw_mutex = nullptr;
/** Signal to draw the next frame. */
static std::condition_variable_any *_draw_signal = nullptr;
/** Should we keep continue drawing? */
static volatile bool _draw_continue;
static Palette _local_palette;
static SDL_Palette *_sdl_palette;

#ifdef __EMSCRIPTEN__
/** Whether we just had a window-enter event. */
static bool _cursor_new_in_window = false;
#endif

static Rect _dirty_rect;

void VideoDriver_SDL::MakeDirty(int left, int top, int width, int height)
{
	Rect r = {left, top, left + width, top + height};
	_dirty_rect = BoundingRect(_dirty_rect, r);
}

static void UpdatePalette()
{
	SDL_Color pal[256];

	for (int i = 0; i != _local_palette.count_dirty; i++) {
		pal[i].r = _local_palette.palette[_local_palette.first_dirty + i].r;
		pal[i].g = _local_palette.palette[_local_palette.first_dirty + i].g;
		pal[i].b = _local_palette.palette[_local_palette.first_dirty + i].b;
		pal[i].a = 0;
	}

	SDL_SetPaletteColors(_sdl_palette, pal, _local_palette.first_dirty, _local_palette.count_dirty);
	SDL_SetSurfacePalette(_sdl_surface, _sdl_palette);
}

static void MakePalette()
{
	if (_sdl_palette == nullptr) {
		_sdl_palette = SDL_AllocPalette(256);
		if (_sdl_palette == nullptr) usererror("SDL2: Couldn't allocate palette: %s", SDL_GetError());
	}

	_cur_palette.first_dirty = 0;
	_cur_palette.count_dirty = 256;
	_local_palette = _cur_palette;
	UpdatePalette();

	if (_sdl_surface != _sdl_real_surface) {
		/* When using a shadow surface, also set our palette on the real screen. This lets SDL
		 * allocate as many colors (or approximations) as
		 * possible, instead of using only the default SDL
		 * palette. This allows us to get more colors exactly
		 * right and might allow using better approximations for
		 * other colors.
		 *
		 * Note that colors allocations are tried in-order, so
		 * this favors colors further up into the palette. Also
		 * note that if two colors from the same animation
		 * sequence are approximated using the same color, that
		 * animation will stop working.
		 *
		 * Since changing the system palette causes the colours
		 * to change right away, and allocations might
		 * drastically change, we can't use this for animation,
		 * since that could cause weird coloring between the
		 * palette change and the blitting below, so we only set
		 * the real palette during initialisation.
		 */
		SDL_SetSurfacePalette(_sdl_real_surface, _sdl_palette);
	}
}

void VideoDriver_SDL::CheckPaletteAnim()
{
	if (_cur_palette.count_dirty == 0) return;

	_local_palette = _cur_palette;
	this->MakeDirty(0, 0, _screen.width, _screen.height);
}

void VideoDriver_SDL::Paint()
{
	PerformanceMeasurer framerate(PFE_VIDEO);

	if (IsEmptyRect(_dirty_rect) && _cur_palette.count_dirty == 0) return;

	if (_cur_palette.count_dirty != 0) {
		Blitter *blitter = BlitterFactory::GetCurrentBlitter();

		switch (blitter->UsePaletteAnimation()) {
			case Blitter::PALETTE_ANIMATION_VIDEO_BACKEND:
				UpdatePalette();
				break;

			case Blitter::PALETTE_ANIMATION_BLITTER:
				blitter->PaletteAnimate(_local_palette);
				break;

			case Blitter::PALETTE_ANIMATION_NONE:
				break;

			default:
				NOT_REACHED();
		}
		_cur_palette.count_dirty = 0;
	}

	SDL_Rect r = { _dirty_rect.left, _dirty_rect.top, _dirty_rect.right - _dirty_rect.left, _dirty_rect.bottom - _dirty_rect.top };

	if (_sdl_surface != _sdl_real_surface) {
		SDL_BlitSurface(_sdl_surface, &r, _sdl_real_surface, &r);
	}
	SDL_UpdateWindowSurfaceRects(_sdl_window, &r, 1);

	_dirty_rect = {};
}

void VideoDriver_SDL::PaintThread()
{
	/* First tell the main thread we're started */
	std::unique_lock<std::recursive_mutex> lock(*_draw_mutex);
	_draw_signal->notify_one();

	/* Now wait for the first thing to draw! */
	_draw_signal->wait(*_draw_mutex);

	while (_draw_continue) {
		/* Then just draw and wait till we stop */
		this->Paint();
		_draw_signal->wait(lock);
	}
}

/* static */ void VideoDriver_SDL::PaintThreadThunk(VideoDriver_SDL *drv)
{
	drv->PaintThread();
}

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

static void FindResolutions()
{
	_resolutions.clear();

	for (int i = 0; i < SDL_GetNumDisplayModes(0); i++) {
		SDL_DisplayMode mode;
		SDL_GetDisplayMode(0, i, &mode);

		if (mode.w < 640 || mode.h < 480) continue;
		if (std::find(_resolutions.begin(), _resolutions.end(), Dimension(mode.w, mode.h)) != _resolutions.end()) continue;
		_resolutions.emplace_back(mode.w, mode.h);
	}

	/* We have found no resolutions, show the default list */
	if (_resolutions.empty()) {
		_resolutions.assign(std::begin(default_resolutions), std::end(default_resolutions));
	}

	SortResolutions();
}

static void GetAvailableVideoMode(uint *w, uint *h)
{
	/* All modes available? */
	if (!_fullscreen || _resolutions.empty()) return;

	/* Is the wanted mode among the available modes? */
	if (std::find(_resolutions.begin(), _resolutions.end(), Dimension(*w, *h)) != _resolutions.end()) return;

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

static uint FindStartupDisplay(uint startup_display)
{
	int num_displays = SDL_GetNumVideoDisplays();

	/* If the user indicated a valid monitor, use that. */
	if (IsInsideBS(startup_display, 0, num_displays)) return startup_display;

	/* Mouse position decides which display to use. */
	int mx, my;
	SDL_GetGlobalMouseState(&mx, &my);
	for (int display = 0; display < num_displays; ++display) {
		SDL_Rect r;
		if (SDL_GetDisplayBounds(display, &r) == 0 && IsInsideBS(mx, r.x, r.w) && IsInsideBS(my, r.y, r.h)) {
			DEBUG(driver, 1, "SDL2: Mouse is at (%d, %d), use display %d (%d, %d, %d, %d)", mx, my, display, r.x, r.y, r.w, r.h);
			return display;
		}
	}

	return 0;
}

bool VideoDriver_SDL::CreateMainWindow(uint w, uint h)
{
	if (_sdl_window != nullptr) return true;

	Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;

	if (_fullscreen) {
		flags |= SDL_WINDOW_FULLSCREEN;
	}

	int x = SDL_WINDOWPOS_UNDEFINED, y = SDL_WINDOWPOS_UNDEFINED;
	SDL_Rect r;
	if (SDL_GetDisplayBounds(this->startup_display, &r) == 0) {
		x = r.x + std::max(0, r.w - static_cast<int>(w)) / 2;
		y = r.y + std::max(0, r.h - static_cast<int>(h)) / 4; // decent desktops have taskbars at the bottom
	}

	char caption[50];
	seprintf(caption, lastof(caption), "OpenTTD %s", _openttd_revision);
	_sdl_window = SDL_CreateWindow(
		caption,
		x, y,
		w, h,
		flags);

	if (_sdl_window == nullptr) {
		DEBUG(driver, 0, "SDL2: Couldn't allocate a window to draw on: %s", SDL_GetError());
		return false;
	}

	std::string icon_path = FioFindFullPath(BASESET_DIR, "openttd.32.bmp");
	if (!icon_path.empty()) {
		/* Give the application an icon */
		SDL_Surface *icon = SDL_LoadBMP(icon_path.c_str());
		if (icon != nullptr) {
			/* Get the colourkey, which will be magenta */
			uint32 rgbmap = SDL_MapRGB(icon->format, 255, 0, 255);

			SDL_SetColorKey(icon, SDL_TRUE, rgbmap);
			SDL_SetWindowIcon(_sdl_window, icon);
			SDL_FreeSurface(icon);
		}
	}

	return true;
}

bool VideoDriver_SDL::CreateMainSurface(uint w, uint h, bool resize)
{
	int bpp = BlitterFactory::GetCurrentBlitter()->GetScreenDepth();

	GetAvailableVideoMode(&w, &h);
	DEBUG(driver, 1, "SDL2: using mode %ux%ux%d", w, h, bpp);

	if (!this->CreateMainWindow(w, h)) return false;
	if (resize) SDL_SetWindowSize(_sdl_window, w, h);

	_sdl_real_surface = SDL_GetWindowSurface(_sdl_window);
	if (_sdl_real_surface == nullptr) {
		DEBUG(driver, 0, "SDL2: Couldn't get window surface: %s", SDL_GetError());
		return false;
	}

	/* Free any previously allocated rgb surface. */
	if (_sdl_rgb_surface != nullptr) {
		SDL_FreeSurface(_sdl_rgb_surface);
		_sdl_rgb_surface = nullptr;
	}

	if (bpp == 8) {
		_sdl_rgb_surface = SDL_CreateRGBSurface(0, w, h, 8, 0, 0, 0, 0);

		if (_sdl_rgb_surface == nullptr) {
			DEBUG(driver, 0, "SDL2: Couldn't allocate shadow surface: %s", SDL_GetError());
			return false;
		}

		_sdl_surface = _sdl_rgb_surface;
	} else {
		_sdl_surface = _sdl_real_surface;
	}

	/* X11 doesn't appreciate it if we invalidate areas outside the window
	 * if shared memory is enabled (read: it crashes). So, as we might have
	 * gotten smaller, reset our dirty rects. GameSizeChanged() a bit lower
	 * will mark the whole screen dirty again anyway, but this time with the
	 * new dimensions. */
	_dirty_rect = {};

	_screen.width = _sdl_surface->w;
	_screen.height = _sdl_surface->h;
	_screen.pitch = _sdl_surface->pitch / (bpp / 8);
	_screen.dst_ptr = _sdl_surface->pixels;

	MakePalette();

	/* When in full screen, we will always have the mouse cursor
	 * within the window, even though SDL does not give us the
	 * appropriate event to know this. */
	if (_fullscreen) _cursor.in_window = true;

	BlitterFactory::GetCurrentBlitter()->PostResize();

	GameSizeChanged();
	return true;
}

bool VideoDriver_SDL::ClaimMousePointer()
{
	SDL_ShowCursor(0);
#ifdef __EMSCRIPTEN__
	SDL_SetRelativeMouseMode(SDL_TRUE);
#endif
	return true;
}

/**
 * This is called to indicate that an edit box has gained focus, text input mode should be enabled.
 */
void VideoDriver_SDL::EditBoxGainedFocus()
{
	if (!this->edit_box_focused) {
		SDL_StartTextInput();
		this->edit_box_focused = true;
	}
}

/**
 * This is called to indicate that an edit box has lost focus, text input mode should be disabled.
 */
void VideoDriver_SDL::EditBoxLostFocus()
{
	if (this->edit_box_focused) {
		SDL_StopTextInput();
		this->edit_box_focused = false;
	}
}


struct SDLVkMapping {
	SDL_Keycode vk_from;
	byte vk_count;
	byte map_to;
	bool unprintable;
};

#define AS(x, z) {x, 0, z, false}
#define AM(x, y, z, w) {x, (byte)(y - x), z, false}
#define AS_UP(x, z) {x, 0, z, true}
#define AM_UP(x, y, z, w) {x, (byte)(y - x), z, true}

static const SDLVkMapping _vk_mapping[] = {
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
	AM(SDLK_a, SDLK_z, 'A', 'Z'),
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
	AM(SDLK_KP_0, SDLK_KP_9, '0', '9'),
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

	AS(SDLK_QUOTE,   WKC_SINGLEQUOTE),
	AS(SDLK_COMMA,   WKC_COMMA),
	AS(SDLK_MINUS,   WKC_MINUS),
	AS(SDLK_PERIOD,  WKC_PERIOD)
};

static uint ConvertSdlKeyIntoMy(SDL_Keysym *sym, WChar *character)
{
	const SDLVkMapping *map;
	uint key = 0;
	bool unprintable = false;

	for (map = _vk_mapping; map != endof(_vk_mapping); ++map) {
		if ((uint)(sym->sym - map->vk_from) <= map->vk_count) {
			key = sym->sym - map->vk_from + map->map_to;
			unprintable = map->unprintable;
			break;
		}
	}

	/* check scancode for BACKQUOTE key, because we want the key left of "1", not anything else (on non-US keyboards) */
	if (sym->scancode == SDL_SCANCODE_GRAVE) key = WKC_BACKQUOTE;

	/* META are the command keys on mac */
	if (sym->mod & KMOD_GUI)   key |= WKC_META;
	if (sym->mod & KMOD_SHIFT) key |= WKC_SHIFT;
	if (sym->mod & KMOD_CTRL)  key |= WKC_CTRL;
	if (sym->mod & KMOD_ALT)   key |= WKC_ALT;

	/* The mod keys have no character. Prevent '?' */
	if (sym->mod & KMOD_GUI ||
		sym->mod & KMOD_CTRL ||
		sym->mod & KMOD_ALT ||
		unprintable) {
		*character = WKC_NONE;
	} else {
		*character = sym->sym;
	}

	return key;
}

/**
 * Like ConvertSdlKeyIntoMy(), but takes an SDL_Keycode as input
 * instead of an SDL_Keysym.
 */
static uint ConvertSdlKeycodeIntoMy(SDL_Keycode kc)
{
	const SDLVkMapping *map;
	uint key = 0;

	for (map = _vk_mapping; map != endof(_vk_mapping); ++map) {
		if ((uint)(kc - map->vk_from) <= map->vk_count) {
			key = kc - map->vk_from + map->map_to;
			break;
		}
	}

	/* check scancode for BACKQUOTE key, because we want the key left
	 * of "1", not anything else (on non-US keyboards) */
	SDL_Scancode sc = SDL_GetScancodeFromKey(kc);
	if (sc == SDL_SCANCODE_GRAVE) key = WKC_BACKQUOTE;

	return key;
}

int VideoDriver_SDL::PollEvent()
{
	SDL_Event ev;

	if (!SDL_PollEvent(&ev)) return -2;

	switch (ev.type) {
		case SDL_MOUSEMOTION:
#ifdef __EMSCRIPTEN__
			if (_cursor_new_in_window) {
				/* The cursor just moved into the window; this means we don't
				 * know the absolutely position yet to move relative from.
				 * Before this time, SDL didn't know it either, and this is
				 * why we postpone it till now. Update the absolute position
				 * for this once, and work relative after. */
				_cursor.pos.x = ev.motion.x;
				_cursor.pos.y = ev.motion.y;
				_cursor.dirty = true;

				_cursor_new_in_window = false;
				SDL_SetRelativeMouseMode(SDL_TRUE);
			} else {
				_cursor.UpdateCursorPositionRelative(ev.motion.xrel, ev.motion.yrel);
			}
#else
			if (_cursor.UpdateCursorPosition(ev.motion.x, ev.motion.y, true)) {
				SDL_WarpMouseInWindow(_sdl_window, _cursor.pos.x, _cursor.pos.y);
			}
#endif
			HandleMouseEvents();
			break;

		case SDL_MOUSEWHEEL:
			if (ev.wheel.y > 0) {
				_cursor.wheel--;
			} else if (ev.wheel.y < 0) {
				_cursor.wheel++;
			}
			break;

		case SDL_MOUSEBUTTONDOWN:
			if (_rightclick_emulate && SDL_GetModState() & KMOD_CTRL) {
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

		case SDL_MOUSEBUTTONUP:
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

		case SDL_QUIT:
			HandleExitGameRequest();
			break;

		case SDL_KEYDOWN: // Toggle full-screen on ALT + ENTER/F
			if ((ev.key.keysym.mod & (KMOD_ALT | KMOD_GUI)) &&
					(ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_f)) {
				if (ev.key.repeat == 0) ToggleFullScreen(!_fullscreen);
			} else {
				WChar character;

				uint keycode = ConvertSdlKeyIntoMy(&ev.key.keysym, &character);
				// Only handle non-text keys here. Text is handled in
				// SDL_TEXTINPUT below.
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

		case SDL_TEXTINPUT: {
			if (!this->edit_box_focused) break;
			SDL_Keycode kc = SDL_GetKeyFromName(ev.text.text);
			uint keycode = ConvertSdlKeycodeIntoMy(kc);

			if (keycode == WKC_BACKQUOTE && FocusedWindowIsConsole()) {
				WChar character;
				Utf8Decode(&character, ev.text.text);
				HandleKeypress(keycode, character);
			} else {
				HandleTextInput(ev.text.text);
			}
			break;
		}
		case SDL_WINDOWEVENT: {
			if (ev.window.event == SDL_WINDOWEVENT_EXPOSED) {
				// Force a redraw of the entire screen.
				this->MakeDirty(0, 0, _screen.width, _screen.height);
			} else if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
				int w = std::max(ev.window.data1, 64);
				int h = std::max(ev.window.data2, 64);
				CreateMainSurface(w, h, w != ev.window.data1 || h != ev.window.data2);
			} else if (ev.window.event == SDL_WINDOWEVENT_ENTER) {
				// mouse entered the window, enable cursor
				_cursor.in_window = true;
#ifdef __EMSCRIPTEN__
				/* Disable relative mouse mode for the first mouse motion,
				 * so we can pick up the absolutely position again. */
				_cursor_new_in_window = true;
				SDL_SetRelativeMouseMode(SDL_FALSE);
#endif
			} else if (ev.window.event == SDL_WINDOWEVENT_LEAVE) {
				// mouse left the window, undraw cursor
				UndrawMouseCursor();
				_cursor.in_window = false;
			}
			break;
		}
	}
	return -1;
}

static const char *InitializeSDL()
{
	/* Explicitly disable hardware acceleration. Enabling this causes
	 * UpdateWindowSurface() to update the window's texture instead of
	 * its surface. */
	SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "0");
	SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1");

	/* Check if the video-driver is already initialized. */
	if (SDL_WasInit(SDL_INIT_VIDEO) != 0) return nullptr;

	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) return SDL_GetError();
	return nullptr;
}

const char *VideoDriver_SDL::Initialize()
{
	this->UpdateAutoResolution();

	const char *error = InitializeSDL();
	if (error != nullptr) return error;

	FindResolutions();
	DEBUG(driver, 2, "Resolution for display: %ux%u", _cur_resolution.width, _cur_resolution.height);

	return nullptr;
}

const char *VideoDriver_SDL::Start(const StringList &parm)
{
	if (BlitterFactory::GetCurrentBlitter()->GetScreenDepth() == 0) return "Only real blitters supported";

	const char *error = this->Initialize();
	if (error != nullptr) return error;

	this->startup_display = FindStartupDisplay(GetDriverParamInt(parm, "display", -1));

	if (!CreateMainSurface(_cur_resolution.width, _cur_resolution.height, false)) {
		return SDL_GetError();
	}

	const char *dname = SDL_GetCurrentVideoDriver();
	DEBUG(driver, 1, "SDL2: using driver '%s'", dname);

	MarkWholeScreenDirty();

	_draw_threaded = !GetDriverParamBool(parm, "no_threads") && !GetDriverParamBool(parm, "no_thread");
	/* Wayland SDL video driver uses EGL to render the game. SDL created the
	 * EGL context from the main-thread, and with EGL you are not allowed to
	 * draw in another thread than the context was created. The function of
	 * _draw_threaded is to do exactly this: draw in another thread than the
	 * window was created, and as such, this fails on Wayland SDL video
	 * driver. So, we disable threading by default if Wayland SDL video
	 * driver is detected.
	 */
	if (strcmp(dname, "wayland") == 0) {
		_draw_threaded = false;
	}

	SDL_StopTextInput();
	this->edit_box_focused = false;

	return nullptr;
}

void VideoDriver_SDL::Stop()
{
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	if (SDL_WasInit(SDL_INIT_EVERYTHING) == 0) {
		SDL_Quit(); // If there's nothing left, quit SDL
	}
}

void VideoDriver_SDL::InputLoop()
{
	uint32 mod = SDL_GetModState();
	const Uint8 *keys = SDL_GetKeyboardState(NULL);

	bool old_ctrl_pressed = _ctrl_pressed;

	_ctrl_pressed  = !!(mod & KMOD_CTRL);
	_shift_pressed = !!(mod & KMOD_SHIFT);

#if defined(_DEBUG)
	if (_shift_pressed)
#else
	/* Speedup when pressing tab, except when using ALT+TAB
	 * to switch to another application. */
	if (keys[SDL_SCANCODE_TAB] && (mod & KMOD_ALT) == 0)
#endif /* defined(_DEBUG) */
	{
		if (!_networking && _game_mode != GM_MENU) _fast_forward |= 2;
	} else if (_fast_forward & 2) {
		_fast_forward = 0;
	}

	/* Determine which directional keys are down. */
	_dirkeys =
		(keys[SDL_SCANCODE_LEFT]  ? 1 : 0) |
		(keys[SDL_SCANCODE_UP]    ? 2 : 0) |
		(keys[SDL_SCANCODE_RIGHT] ? 4 : 0) |
		(keys[SDL_SCANCODE_DOWN]  ? 8 : 0);

	if (old_ctrl_pressed != _ctrl_pressed) HandleCtrlChanged();
}

void VideoDriver_SDL::LoopOnce()
{
	InteractiveRandom(); // randomness

	while (PollEvent() == -1) {}
	if (_exit_game) {
#ifdef __EMSCRIPTEN__
		/* Emscripten is event-driven, and as such the main loop is inside
		 * the browser. So if _exit_game goes true, the main loop ends (the
		 * cancel call), but we still have to call the cleanup that is
		 * normally done at the end of the main loop for non-Emscripten.
		 * After that, Emscripten just halts, and the HTML shows a nice
		 * "bye, see you next time" message. */
		emscripten_cancel_main_loop();
		MainLoopCleanup();
#endif
		return;
	}

	if (VideoDriver::Tick()) {
		if (_draw_mutex != nullptr && !HasModalProgress()) {
			_draw_signal->notify_one();
		} else {
			this->Paint();
		}
	}

/* Emscripten is running an event-based mainloop; there is already some
 * downtime between each iteration, so no need to sleep. */
#ifndef __EMSCRIPTEN__
	this->SleepTillNextTick();
#endif
}

void VideoDriver_SDL::MainLoop()
{
	if (_draw_threaded) {
		/* Initialise the mutex first, because that's the thing we *need*
		 * directly in the newly created thread. */
		_draw_mutex = new std::recursive_mutex();
		if (_draw_mutex == nullptr) {
			_draw_threaded = false;
		} else {
			draw_lock = std::unique_lock<std::recursive_mutex>(*_draw_mutex);
			_draw_signal = new std::condition_variable_any();
			_draw_continue = true;

			_draw_threaded = StartNewThread(&draw_thread, "ottd:draw-sdl", &VideoDriver_SDL::PaintThreadThunk, this);

			/* Free the mutex if we won't be able to use it. */
			if (!_draw_threaded) {
				draw_lock.unlock();
				draw_lock.release();
				delete _draw_mutex;
				delete _draw_signal;
				_draw_mutex = nullptr;
				_draw_signal = nullptr;
			} else {
				/* Wait till the draw mutex has started itself. */
				_draw_signal->wait(*_draw_mutex);
			}
		}
	}

	DEBUG(driver, 1, "SDL2: using %sthreads", _draw_threaded ? "" : "no ");

#ifdef __EMSCRIPTEN__
	/* Run the main loop event-driven, based on RequestAnimationFrame. */
	emscripten_set_main_loop_arg(&this->EmscriptenLoop, this, 0, 1);
#else
	while (!_exit_game) {
		LoopOnce();
	}

	MainLoopCleanup();
#endif
}

void VideoDriver_SDL::MainLoopCleanup()
{
	if (_draw_mutex != nullptr) {
		_draw_continue = false;
		/* Sending signal if there is no thread blocked
		 * is very valid and results in noop */
		_draw_signal->notify_one();
		if (draw_lock.owns_lock()) draw_lock.unlock();
		draw_lock.release();
		draw_thread.join();

		delete _draw_mutex;
		delete _draw_signal;

		_draw_mutex = nullptr;
		_draw_signal = nullptr;
	}

#ifdef __EMSCRIPTEN__
	emscripten_exit_pointerlock();
	/* In effect, the game ends here. As emscripten_set_main_loop() caused
	 * the stack to be unwound, the code after MainLoop() in
	 * openttd_main() is never executed. */
	EM_ASM(if (window["openttd_syncfs"]) openttd_syncfs());
	EM_ASM(if (window["openttd_exit"]) openttd_exit());
#endif
}

bool VideoDriver_SDL::ChangeResolution(int w, int h)
{
	std::unique_lock<std::recursive_mutex> lock;
	if (_draw_mutex != nullptr) lock = std::unique_lock<std::recursive_mutex>(*_draw_mutex);

	return CreateMainSurface(w, h, true);
}

bool VideoDriver_SDL::ToggleFullscreen(bool fullscreen)
{
	std::unique_lock<std::recursive_mutex> lock;
	if (_draw_mutex != nullptr) lock = std::unique_lock<std::recursive_mutex>(*_draw_mutex);

	int w, h;

	/* Remember current window size */
	if (fullscreen) {
		SDL_GetWindowSize(_sdl_window, &w, &h);

		/* Find fullscreen window size */
		SDL_DisplayMode dm;
		if (SDL_GetCurrentDisplayMode(0, &dm) < 0) {
			DEBUG(driver, 0, "SDL_GetCurrentDisplayMode() failed: %s", SDL_GetError());
		} else {
			SDL_SetWindowSize(_sdl_window, dm.w, dm.h);
		}
	}

	DEBUG(driver, 1, "SDL2: Setting %s", fullscreen ? "fullscreen" : "windowed");
	int ret = SDL_SetWindowFullscreen(_sdl_window, fullscreen ? SDL_WINDOW_FULLSCREEN : 0);
	if (ret == 0) {
		/* Switching resolution succeeded, set fullscreen value of window. */
		_fullscreen = fullscreen;
		if (!fullscreen) SDL_SetWindowSize(_sdl_window, w, h);
	} else {
		DEBUG(driver, 0, "SDL_SetWindowFullscreen() failed: %s", SDL_GetError());
	}

	return ret == 0;
}

bool VideoDriver_SDL::AfterBlitterChange()
{
	assert(BlitterFactory::GetCurrentBlitter()->GetScreenDepth() != 0);
	int w, h;
	SDL_GetWindowSize(_sdl_window, &w, &h);
	return CreateMainSurface(w, h, false);
}

void VideoDriver_SDL::AcquireBlitterLock()
{
	if (_draw_mutex != nullptr) _draw_mutex->lock();
}

void VideoDriver_SDL::ReleaseBlitterLock()
{
	if (_draw_mutex != nullptr) _draw_mutex->unlock();
}

Dimension VideoDriver_SDL::GetScreenSize() const
{
	SDL_DisplayMode mode;
	if (SDL_GetCurrentDisplayMode(this->startup_display, &mode) != 0) return VideoDriver::GetScreenSize();

	return { static_cast<uint>(mode.w), static_cast<uint>(mode.h) };
}

bool VideoDriver_SDL::LockVideoBuffer()
{
	if (_draw_threaded) this->draw_lock.lock();
	return true;
}

void VideoDriver_SDL::UnlockVideoBuffer()
{
	if (_draw_threaded) this->draw_lock.unlock();
}
