/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file sdl_v.cpp Implementation of the SDL video driver. */

#ifdef WITH_SDL

#include "../stdafx.h"
#include "../openttd.h"
#include "../error_func.h"
#include "../gfx_func.h"
#include "../blitter/factory.hpp"
#include "../thread.h"
#include "../progress.h"
#include "../core/random_func.hpp"
#include "../core/math_func.hpp"
#include "../fileio_func.h"
#include "../framerate_type.h"
#include "../window_func.h"
#include "sdl_v.h"
#include <SDL.h>

#include "../safeguards.h"

static FVideoDriver_SDL iFVideoDriver_SDL;

static SDL_Surface *_sdl_surface;
static SDL_Surface *_sdl_realscreen;
static bool _all_modes;

static Palette _local_palette;

#define MAX_DIRTY_RECTS 100
static SDL_Rect _dirty_rects[MAX_DIRTY_RECTS];
static int _num_dirty_rects;
static int _use_hwpalette;
static int _requested_hwpalette; /* Did we request a HWPALETTE for the current video mode? */

void VideoDriver_SDL::MakeDirty(int left, int top, int width, int height)
{
	if (_num_dirty_rects < MAX_DIRTY_RECTS) {
		_dirty_rects[_num_dirty_rects].x = left;
		_dirty_rects[_num_dirty_rects].y = top;
		_dirty_rects[_num_dirty_rects].w = width;
		_dirty_rects[_num_dirty_rects].h = height;
	}
	_num_dirty_rects++;
}

static void UpdatePalette(bool init = false)
{
	SDL_Color pal[256];

	for (int i = 0; i != _local_palette.count_dirty; i++) {
		pal[i].r = _local_palette.palette[_local_palette.first_dirty + i].r;
		pal[i].g = _local_palette.palette[_local_palette.first_dirty + i].g;
		pal[i].b = _local_palette.palette[_local_palette.first_dirty + i].b;
		pal[i].unused = 0;
	}

	SDL_SetColors(_sdl_surface, pal, _local_palette.first_dirty, _local_palette.count_dirty);

	if (_sdl_surface != _sdl_realscreen && init) {
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
		SDL_SetColors(_sdl_realscreen, pal, _local_palette.first_dirty, _local_palette.count_dirty);
	}

	if (_sdl_surface != _sdl_realscreen && !init) {
		/* We're not using real hardware palette, but are letting SDL
		 * approximate the palette during shadow -> screen copy. To
		 * change the palette, we need to recopy the entire screen.
		 *
		 * Note that this operation can slow down the rendering
		 * considerably, especially since changing the shadow
		 * palette will need the next blit to re-detect the
		 * best mapping of shadow palette colors to real palette
		 * colors from scratch.
		 */
		SDL_BlitSurface(_sdl_surface, nullptr, _sdl_realscreen, nullptr);
		SDL_UpdateRect(_sdl_realscreen, 0, 0, 0, 0);
	}
}

static void InitPalette()
{
	CopyPalette(_local_palette, true);
	UpdatePalette(true);
}

void VideoDriver_SDL::CheckPaletteAnim()
{
	if (!CopyPalette(_local_palette)) return;

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
}

void VideoDriver_SDL::Paint()
{
	PerformanceMeasurer framerate(PFE_VIDEO);

	int n = _num_dirty_rects;
	if (n == 0) return;

	_num_dirty_rects = 0;

	if (n > MAX_DIRTY_RECTS) {
		if (_sdl_surface != _sdl_realscreen) {
			SDL_BlitSurface(_sdl_surface, nullptr, _sdl_realscreen, nullptr);
		}

		SDL_UpdateRect(_sdl_realscreen, 0, 0, 0, 0);
	} else {
		if (_sdl_surface != _sdl_realscreen) {
			for (int i = 0; i < n; i++) {
				SDL_BlitSurface(_sdl_surface, &_dirty_rects[i], _sdl_realscreen, &_dirty_rects[i]);
			}
		}

		SDL_UpdateRects(_sdl_realscreen, n, _dirty_rects);
	}
}

static const Dimension _default_resolutions[] = {
	{ 640,  480},
	{ 800,  600},
	{1024,  768},
	{1152,  864},
	{1280,  800},
	{1280,  960},
	{1280, 1024},
	{1400, 1050},
	{1600, 1200},
	{1680, 1050},
	{1920, 1200}
};

static void GetVideoModes()
{
	SDL_Rect **modes = SDL_ListModes(nullptr, SDL_SWSURFACE | SDL_FULLSCREEN);
	if (modes == nullptr) UserError("sdl: no modes available");

	_resolutions.clear();

	_all_modes = (SDL_ListModes(nullptr, SDL_SWSURFACE | (_fullscreen ? SDL_FULLSCREEN : 0)) == (void*)-1);
	if (modes == (void*)-1) {
		for (uint i = 0; i < lengthof(_default_resolutions); i++) {
			if (SDL_VideoModeOK(_default_resolutions[i].width, _default_resolutions[i].height, 8, SDL_FULLSCREEN) != 0) {
				_resolutions.push_back(_default_resolutions[i]);
			}
		}
	} else {
		for (int i = 0; modes[i]; i++) {
			uint w = modes[i]->w;
			uint h = modes[i]->h;
			if (w < 640 || h < 480) continue; // reject too small resolutions
			if (std::find(_resolutions.begin(), _resolutions.end(), Dimension(w, h)) != _resolutions.end()) continue;
			_resolutions.emplace_back(w, h);
		}
		if (_resolutions.empty()) UserError("No usable screen resolutions found!\n");
		SortResolutions();
	}
}

static void GetAvailableVideoMode(uint *w, uint *h)
{
	/* All modes available? */
	if (_all_modes || _resolutions.empty()) return;

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

bool VideoDriver_SDL::CreateMainSurface(uint w, uint h)
{
	SDL_Surface *newscreen, *icon;
	int bpp = BlitterFactory::GetCurrentBlitter()->GetScreenDepth();
	bool want_hwpalette;

	GetAvailableVideoMode(&w, &h);

	Debug(driver, 1, "SDL: using mode {}x{}x{}", w, h, bpp);

	if (bpp == 0) UserError("Can't use a blitter that blits 0 bpp for normal visuals");

	std::string icon_path = FioFindFullPath(BASESET_DIR, "openttd.32.bmp");
	if (!icon_path.empty()) {
		/* Give the application an icon */
		icon = SDL_LoadBMP(icon_path.c_str());
		if (icon != nullptr) {
			/* Get the colourkey, which will be magenta */
			uint32_t rgbmap = SDL_MapRGB(icon->format, 255, 0, 255);

			SDL_SetColorKey(icon, SDL_SRCCOLORKEY, rgbmap);
			SDL_WM_SetIcon(icon, nullptr);
			SDL_FreeSurface(icon);
		}
	}

	if (_use_hwpalette == 2) {
		/* Default is to autodetect when to use SDL_HWPALETTE.
		 * In this case, SDL_HWPALETTE is only used for 8bpp
		 * blitters in fullscreen.
		 *
		 * When using an 8bpp blitter on a 8bpp system in
		 * windowed mode with SDL_HWPALETTE, OpenTTD will claim
		 * the system palette, making all other applications
		 * get the wrong colours. In this case, we're better of
		 * trying to approximate the colors we need using system
		 * colors, using a shadow surface (see below).
		 *
		 * On a 32bpp system, SDL_HWPALETTE is ignored, so it
		 * doesn't matter what we do.
		 *
		 * When using a 32bpp blitter on a 8bpp system, setting
		 * SDL_HWPALETTE messes up rendering (at least on X11),
		 * so we don't do that. In this case, SDL takes care of
		 * color approximation using its own shadow surface
		 * (which we can't force in 8bpp on 8bpp mode,
		 * unfortunately).
		 */
		want_hwpalette = bpp == 8 && _fullscreen && _support8bpp == S8BPP_HARDWARE;
	} else {
		/* User specified a value manually */
		want_hwpalette = _use_hwpalette;
	}

	if (want_hwpalette) Debug(driver, 1, "SDL: requesting hardware palette");

	/* Free any previously allocated shadow surface */
	if (_sdl_surface != nullptr && _sdl_surface != _sdl_realscreen) SDL_FreeSurface(_sdl_surface);

	if (_sdl_realscreen != nullptr) {
		if (_requested_hwpalette != want_hwpalette) {
			/* SDL (at least the X11 driver), reuses the
			 * same window and palette settings when the bpp
			 * (and a few flags) are the same. Since we need
			 * to hwpalette value to change (in particular
			 * when switching between fullscreen and
			 * windowed), we restart the entire video
			 * subsystem to force creating a new window.
			 */
			Debug(driver, 0, "SDL: Restarting SDL video subsystem, to force hwpalette change");
			SDL_QuitSubSystem(SDL_INIT_VIDEO);
			SDL_InitSubSystem(SDL_INIT_VIDEO);
			ClaimMousePointer();
			SetupKeyboard();
		}
	}
	/* Remember if we wanted a hwpalette. We can't reliably query
	 * SDL for the SDL_HWPALETTE flag, since it might get set even
	 * though we didn't ask for it (when SDL creates a shadow
	 * surface, for example). */
	_requested_hwpalette = want_hwpalette;

	/* DO NOT CHANGE TO HWSURFACE, IT DOES NOT WORK */
	newscreen = SDL_SetVideoMode(w, h, bpp, SDL_SWSURFACE | (want_hwpalette ? SDL_HWPALETTE : 0) | (_fullscreen ? SDL_FULLSCREEN : SDL_RESIZABLE));
	if (newscreen == nullptr) {
		Debug(driver, 0, "SDL: Couldn't allocate a window to draw on");
		return false;
	}
	_sdl_realscreen = newscreen;

	if (bpp == 8 && (_sdl_realscreen->flags & SDL_HWPALETTE) != SDL_HWPALETTE) {
		/* Using an 8bpp blitter, if we didn't get a hardware
		 * palette (most likely because we didn't request one,
		 * see above), we'll have to set up a shadow surface to
		 * render on.
		 *
		 * Our palette will be applied to this shadow surface,
		 * while the real screen surface will use the shared
		 * system palette (which will partly contain our colors,
		 * but most likely will not have enough free color cells
		 * for all of our colors). SDL can use these two
		 * palettes at blit time to approximate colors used in
		 * the shadow surface using system colors automatically.
		 *
		 * Note that when using an 8bpp blitter on a 32bpp
		 * system, SDL will create an internal shadow surface.
		 * This shadow surface will have SDL_HWPALLETE set, so
		 * we won't create a second shadow surface in this case.
		 */
		Debug(driver, 1, "SDL: using shadow surface");
		newscreen = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, bpp, 0, 0, 0, 0);
		if (newscreen == nullptr) {
			Debug(driver, 0, "SDL: Couldn't allocate a shadow surface to draw on");
			return false;
		}
	}

	/* Delay drawing for this cycle; the next cycle will redraw the whole screen */
	_num_dirty_rects = 0;

	_screen.width = newscreen->w;
	_screen.height = newscreen->h;
	_screen.pitch = newscreen->pitch / (bpp / 8);
	_screen.dst_ptr = newscreen->pixels;
	_sdl_surface = newscreen;

	/* When in full screen, we will always have the mouse cursor
	 * within the window, even though SDL does not give us the
	 * appropriate event to know this. */
	if (_fullscreen) _cursor.in_window = true;

	Blitter *blitter = BlitterFactory::GetCurrentBlitter();
	blitter->PostResize();

	InitPalette();

	std::string caption = VideoDriver::GetCaption();
	SDL_WM_SetCaption(caption.c_str(), caption.c_str());

	GameSizeChanged();

	return true;
}

bool VideoDriver_SDL::ClaimMousePointer()
{
	SDL_ShowCursor(0);
	return true;
}

struct SDLVkMapping {
	uint16_t vk_from;
	byte vk_count;
	byte map_to;
};

#define AS(x, z) {x, 0, z}
#define AM(x, y, z, w) {x, (byte)(y - x), z}

static const SDLVkMapping _vk_mapping[] = {
	/* Pageup stuff + up/down */
	AM(SDLK_PAGEUP, SDLK_PAGEDOWN, WKC_PAGEUP, WKC_PAGEDOWN),
	AS(SDLK_UP,     WKC_UP),
	AS(SDLK_DOWN,   WKC_DOWN),
	AS(SDLK_LEFT,   WKC_LEFT),
	AS(SDLK_RIGHT,  WKC_RIGHT),

	AS(SDLK_HOME,   WKC_HOME),
	AS(SDLK_END,    WKC_END),

	AS(SDLK_INSERT, WKC_INSERT),
	AS(SDLK_DELETE, WKC_DELETE),

	/* Map letters & digits */
	AM(SDLK_a, SDLK_z, 'A', 'Z'),
	AM(SDLK_0, SDLK_9, '0', '9'),

	AS(SDLK_ESCAPE,    WKC_ESC),
	AS(SDLK_PAUSE,     WKC_PAUSE),
	AS(SDLK_BACKSPACE, WKC_BACKSPACE),

	AS(SDLK_SPACE,     WKC_SPACE),
	AS(SDLK_RETURN,    WKC_RETURN),
	AS(SDLK_TAB,       WKC_TAB),

	/* Function keys */
	AM(SDLK_F1, SDLK_F12, WKC_F1, WKC_F12),

	/* Numeric part. */
	AM(SDLK_KP0, SDLK_KP9, '0', '9'),
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

static uint ConvertSdlKeyIntoMy(SDL_keysym *sym, char32_t *character)
{
	const SDLVkMapping *map;
	uint key = 0;

	for (map = _vk_mapping; map != endof(_vk_mapping); ++map) {
		if ((uint)(sym->sym - map->vk_from) <= map->vk_count) {
			key = sym->sym - map->vk_from + map->map_to;
			break;
		}
	}

	/* check scancode for BACKQUOTE key, because we want the key left of "1", not anything else (on non-US keyboards) */
#if defined(_WIN32)
	if (sym->scancode == 41) key = WKC_BACKQUOTE;
#elif defined(__APPLE__)
	if (sym->scancode == 10) key = WKC_BACKQUOTE;
#elif defined(__SVR4) && defined(__sun)
	if (sym->scancode == 60) key = WKC_BACKQUOTE;
	if (sym->scancode == 49) key = WKC_BACKSPACE;
#elif defined(__sgi__)
	if (sym->scancode == 22) key = WKC_BACKQUOTE;
#else
	if (sym->scancode == 49) key = WKC_BACKQUOTE;
#endif

	/* META are the command keys on mac */
	if (sym->mod & KMOD_META)  key |= WKC_META;
	if (sym->mod & KMOD_SHIFT) key |= WKC_SHIFT;
	if (sym->mod & KMOD_CTRL)  key |= WKC_CTRL;
	if (sym->mod & KMOD_ALT)   key |= WKC_ALT;

	*character = sym->unicode;
	return key;
}

bool VideoDriver_SDL::PollEvent()
{
	SDL_Event ev;

	if (!SDL_PollEvent(&ev)) return false;

	switch (ev.type) {
		case SDL_MOUSEMOTION: {
			int32_t x = ev.motion.x;
			int32_t y = ev.motion.y;

			if (_cursor.fix_at) {
				/* Get all queued mouse events now in case we have to warp the cursor. In the
				 * end, we only care about the current mouse position and not bygone events. */
				while (SDL_PeepEvents(&ev, 1, SDL_GETEVENT, SDL_MOUSEMOTION)) {
					x = ev.motion.x;
					y = ev.motion.y;
				}
			}

			if (_cursor.UpdateCursorPosition(x, y)) {
				SDL_WarpMouse(_cursor.pos.x, _cursor.pos.y);
			}
			HandleMouseEvents();
			break;
		}

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

				case SDL_BUTTON_WHEELUP:   _cursor.wheel--; break;
				case SDL_BUTTON_WHEELDOWN: _cursor.wheel++; break;

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

		case SDL_ACTIVEEVENT:
			if (!(ev.active.state & SDL_APPMOUSEFOCUS)) break;

			if (ev.active.gain) { // mouse entered the window, enable cursor
				_cursor.in_window = true;
			} else {
				UndrawMouseCursor(); // mouse left the window, undraw cursor
				_cursor.in_window = false;
			}
			break;

		case SDL_QUIT:
			HandleExitGameRequest();
			break;

		case SDL_KEYDOWN: // Toggle full-screen on ALT + ENTER/F
			if ((ev.key.keysym.mod & (KMOD_ALT | KMOD_META)) &&
					(ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_f)) {
				ToggleFullScreen(!_fullscreen);
			} else {
				char32_t character;
				uint keycode = ConvertSdlKeyIntoMy(&ev.key.keysym, &character);
				HandleKeypress(keycode, character);
			}
			break;

		case SDL_VIDEORESIZE: {
			int w = std::max(ev.resize.w, 64);
			int h = std::max(ev.resize.h, 64);
			CreateMainSurface(w, h);
			break;
		}
		case SDL_VIDEOEXPOSE: {
			/* Force a redraw of the entire screen. Note
			 * that SDL 1.2 seems to do this automatically
			 * in most cases, but 1.3 / 2.0 does not. */
		        _num_dirty_rects = MAX_DIRTY_RECTS + 1;
			break;
		}
	}

	return true;
}

const char *VideoDriver_SDL::Start(const StringList &param)
{
	char buf[30];
	_use_hwpalette = GetDriverParamInt(param, "hw_palette", 2);

	/* Just on the offchance the audio subsystem started before the video system,
	 * check whether any part of SDL has been initialised before getting here.
	 * Slightly duplicated with sound/sdl_s.cpp */
	int ret_code = 0;
	if (SDL_WasInit(SDL_INIT_EVERYTHING) == 0) {
		ret_code = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE);
	} else if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
		ret_code = SDL_InitSubSystem(SDL_INIT_VIDEO);
	}
	if (ret_code < 0) return SDL_GetError();

	this->UpdateAutoResolution();

	GetVideoModes();
	if (!CreateMainSurface(_cur_resolution.width, _cur_resolution.height)) {
		return SDL_GetError();
	}

	SDL_VideoDriverName(buf, sizeof buf);
	Debug(driver, 1, "SDL: using driver '{}'", buf);

	MarkWholeScreenDirty();
	SetupKeyboard();

	this->is_game_threaded = !GetDriverParamBool(param, "no_threads") && !GetDriverParamBool(param, "no_thread");

	return nullptr;
}

void VideoDriver_SDL::SetupKeyboard()
{
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	SDL_EnableUNICODE(1);
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
	uint32_t mod = SDL_GetModState();
	int numkeys;
	Uint8 *keys = SDL_GetKeyState(&numkeys);

	bool old_ctrl_pressed = _ctrl_pressed;

	_ctrl_pressed  = !!(mod & KMOD_CTRL);
	_shift_pressed = !!(mod & KMOD_SHIFT);

	/* Speedup when pressing tab, except when using ALT+TAB
	 * to switch to another application. */
	this->fast_forward_key_pressed = keys[SDLK_TAB] && (mod & KMOD_ALT) == 0;

	/* Determine which directional keys are down. */
	_dirkeys =
		(keys[SDLK_LEFT]  ? 1 : 0) |
		(keys[SDLK_UP]    ? 2 : 0) |
		(keys[SDLK_RIGHT] ? 4 : 0) |
		(keys[SDLK_DOWN]  ? 8 : 0);

	if (old_ctrl_pressed != _ctrl_pressed) HandleCtrlChanged();
}

void VideoDriver_SDL::MainLoop()
{
	this->StartGameThread();

	for (;;) {
		if (_exit_game) break;

		this->Tick();
		this->SleepTillNextTick();
	}

	this->StopGameThread();
}

bool VideoDriver_SDL::ChangeResolution(int w, int h)
{
	return CreateMainSurface(w, h);
}

bool VideoDriver_SDL::ToggleFullscreen(bool fullscreen)
{
	_fullscreen = fullscreen;
	GetVideoModes(); // get the list of available video modes
	bool ret = !_resolutions.empty() && CreateMainSurface(_cur_resolution.width, _cur_resolution.height);

	if (!ret) {
		/* switching resolution failed, put back full_screen to original status */
		_fullscreen ^= true;
	}

	InvalidateWindowClassesData(WC_GAME_OPTIONS, 3);
	return ret;
}

bool VideoDriver_SDL::AfterBlitterChange()
{
	return CreateMainSurface(_screen.width, _screen.height);
}

#endif /* WITH_SDL */
