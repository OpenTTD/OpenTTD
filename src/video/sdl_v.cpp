/* $Id$ */

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
#include "../gfx_func.h"
#include "../sdl.h"
#include "../rev.h"
#include "../blitter/factory.hpp"
#include "../network/network.h"
#include "../thread/thread.h"
#include "../progress.h"
#include "../core/random_func.hpp"
#include "../core/math_func.hpp"
#include "../fileio_func.h"
#include "sdl_v.h"
#include <SDL.h>

#include "../safeguards.h"

static FVideoDriver_SDL iFVideoDriver_SDL;

static SDL_Surface *_sdl_screen;
static SDL_Surface *_sdl_realscreen;
static bool _all_modes;

/** Whether the drawing is/may be done in a separate thread. */
static bool _draw_threaded;
/** Thread used to 'draw' to the screen, i.e. push data to the screen. */
static ThreadObject *_draw_thread = NULL;
/** Mutex to keep the access to the shared memory controlled. */
static ThreadMutex *_draw_mutex = NULL;
/** Should we keep continue drawing? */
static volatile bool _draw_continue;
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

	SDL_CALL SDL_SetColors(_sdl_screen, pal, _local_palette.first_dirty, _local_palette.count_dirty);

	if (_sdl_screen != _sdl_realscreen && init) {
		/* When using a shadow surface, also set our palette on the real screen. This lets SDL
		 * allocate as much colors (or approximations) as
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
		SDL_CALL SDL_SetColors(_sdl_realscreen, pal, _local_palette.first_dirty, _local_palette.count_dirty);
	}

	if (_sdl_screen != _sdl_realscreen && !init) {
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
		SDL_CALL SDL_BlitSurface(_sdl_screen, NULL, _sdl_realscreen, NULL);
		SDL_CALL SDL_UpdateRect(_sdl_realscreen, 0, 0, 0, 0);
	}
}

static void InitPalette()
{
	_local_palette = _cur_palette;
	_local_palette.first_dirty = 0;
	_local_palette.count_dirty = 256;
	UpdatePalette(true);
}

static void CheckPaletteAnim()
{
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
}

static void DrawSurfaceToScreen()
{
	int n = _num_dirty_rects;
	if (n == 0) return;

	_num_dirty_rects = 0;
	if (n > MAX_DIRTY_RECTS) {
		if (_sdl_screen != _sdl_realscreen) {
			SDL_CALL SDL_BlitSurface(_sdl_screen, NULL, _sdl_realscreen, NULL);
		}
		SDL_CALL SDL_UpdateRect(_sdl_realscreen, 0, 0, 0, 0);
	} else {
		if (_sdl_screen != _sdl_realscreen) {
			for (int i = 0; i < n; i++) {
				SDL_CALL SDL_BlitSurface(_sdl_screen, &_dirty_rects[i], _sdl_realscreen, &_dirty_rects[i]);
			}
		}
		SDL_CALL SDL_UpdateRects(_sdl_realscreen, n, _dirty_rects);
	}
}

static void DrawSurfaceToScreenThread(void *)
{
	/* First tell the main thread we're started */
	_draw_mutex->BeginCritical();
	_draw_mutex->SendSignal();

	/* Now wait for the first thing to draw! */
	_draw_mutex->WaitForSignal();

	while (_draw_continue) {
		CheckPaletteAnim();
		/* Then just draw and wait till we stop */
		DrawSurfaceToScreen();
		_draw_mutex->WaitForSignal();
	}

	_draw_mutex->EndCritical();
	_draw_thread->Exit();
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
	SDL_Rect **modes = SDL_CALL SDL_ListModes(NULL, SDL_SWSURFACE | SDL_FULLSCREEN);
	if (modes == NULL) usererror("sdl: no modes available");

	_all_modes = (SDL_CALL SDL_ListModes(NULL, SDL_SWSURFACE | (_fullscreen ? SDL_FULLSCREEN : 0)) == (void*)-1);
	if (modes == (void*)-1) {
		int n = 0;
		for (uint i = 0; i < lengthof(_default_resolutions); i++) {
			if (SDL_CALL SDL_VideoModeOK(_default_resolutions[i].width, _default_resolutions[i].height, 8, SDL_FULLSCREEN) != 0) {
				_resolutions[n] = _default_resolutions[i];
				if (++n == lengthof(_resolutions)) break;
			}
		}
		_num_resolutions = n;
	} else {
		int n = 0;
		for (int i = 0; modes[i]; i++) {
			uint w = modes[i]->w;
			uint h = modes[i]->h;
			int j;
			for (j = 0; j < n; j++) {
				if (_resolutions[j].width == w && _resolutions[j].height == h) break;
			}

			if (j == n) {
				_resolutions[j].width  = w;
				_resolutions[j].height = h;
				if (++n == lengthof(_resolutions)) break;
			}
		}
		_num_resolutions = n;
		SortResolutions(_num_resolutions);
	}
}

static void GetAvailableVideoMode(uint *w, uint *h)
{
	/* All modes available? */
	if (_all_modes || _num_resolutions == 0) return;

	/* Is the wanted mode among the available modes? */
	for (int i = 0; i != _num_resolutions; i++) {
		if (*w == _resolutions[i].width && *h == _resolutions[i].height) return;
	}

	/* Use the closest possible resolution */
	int best = 0;
	uint delta = Delta(_resolutions[0].width, *w) * Delta(_resolutions[0].height, *h);
	for (int i = 1; i != _num_resolutions; ++i) {
		uint newdelta = Delta(_resolutions[i].width, *w) * Delta(_resolutions[i].height, *h);
		if (newdelta < delta) {
			best = i;
			delta = newdelta;
		}
	}
	*w = _resolutions[best].width;
	*h = _resolutions[best].height;
}

#ifdef WIN32
/* Let's redefine the LoadBMP macro with because we are dynamically
 * loading SDL and need to 'SDL_CALL' all functions */
#undef SDL_LoadBMP
#define SDL_LoadBMP(file)	SDL_LoadBMP_RW(SDL_CALL SDL_RWFromFile(file, "rb"), 1)
#endif

bool VideoDriver_SDL::CreateMainSurface(uint w, uint h)
{
	SDL_Surface *newscreen, *icon;
	char caption[50];
	int bpp = BlitterFactory::GetCurrentBlitter()->GetScreenDepth();
	bool want_hwpalette;

	GetAvailableVideoMode(&w, &h);

	DEBUG(driver, 1, "SDL: using mode %ux%ux%d", w, h, bpp);

	if (bpp == 0) usererror("Can't use a blitter that blits 0 bpp for normal visuals");

	char icon_path[MAX_PATH];
	if (FioFindFullPath(icon_path, lastof(icon_path), BASESET_DIR, "openttd.32.bmp") != NULL) {
		/* Give the application an icon */
		icon = SDL_CALL SDL_LoadBMP(icon_path);
		if (icon != NULL) {
			/* Get the colourkey, which will be magenta */
			uint32 rgbmap = SDL_CALL SDL_MapRGB(icon->format, 255, 0, 255);

			SDL_CALL SDL_SetColorKey(icon, SDL_SRCCOLORKEY, rgbmap);
			SDL_CALL SDL_WM_SetIcon(icon, NULL);
			SDL_CALL SDL_FreeSurface(icon);
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

	if (want_hwpalette) DEBUG(driver, 1, "SDL: requesting hardware palete");

	/* Free any previously allocated shadow surface */
	if (_sdl_screen != NULL && _sdl_screen != _sdl_realscreen) SDL_CALL SDL_FreeSurface(_sdl_screen);

	if (_sdl_realscreen != NULL) {
		if (_requested_hwpalette != want_hwpalette) {
			/* SDL (at least the X11 driver), reuses the
			 * same window and palette settings when the bpp
			 * (and a few flags) are the same. Since we need
			 * to hwpalette value to change (in particular
			 * when switching between fullscreen and
			 * windowed), we restart the entire video
			 * subsystem to force creating a new window.
			 */
			DEBUG(driver, 0, "SDL: Restarting SDL video subsystem, to force hwpalette change");
			SDL_CALL SDL_QuitSubSystem(SDL_INIT_VIDEO);
			SDL_CALL SDL_InitSubSystem(SDL_INIT_VIDEO);
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
	newscreen = SDL_CALL SDL_SetVideoMode(w, h, bpp, SDL_SWSURFACE | (want_hwpalette ? SDL_HWPALETTE : 0) | (_fullscreen ? SDL_FULLSCREEN : SDL_RESIZABLE));
	if (newscreen == NULL) {
		DEBUG(driver, 0, "SDL: Couldn't allocate a window to draw on");
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
		DEBUG(driver, 1, "SDL: using shadow surface");
		newscreen = SDL_CALL SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, bpp, 0, 0, 0, 0);
		if (newscreen == NULL) {
			DEBUG(driver, 0, "SDL: Couldn't allocate a shadow surface to draw on");
			return false;
		}
	}

	/* Delay drawing for this cycle; the next cycle will redraw the whole screen */
	_num_dirty_rects = 0;

	_screen.width = newscreen->w;
	_screen.height = newscreen->h;
	_screen.pitch = newscreen->pitch / (bpp / 8);
	_screen.dst_ptr = newscreen->pixels;
	_sdl_screen = newscreen;

	/* When in full screen, we will always have the mouse cursor
	 * within the window, even though SDL does not give us the
	 * appropriate event to know this. */
	if (_fullscreen) _cursor.in_window = true;

	Blitter *blitter = BlitterFactory::GetCurrentBlitter();
	blitter->PostResize();

	InitPalette();

	seprintf(caption, lastof(caption), "OpenTTD %s", _openttd_revision);
	SDL_CALL SDL_WM_SetCaption(caption, caption);

	GameSizeChanged();

	return true;
}

bool VideoDriver_SDL::ClaimMousePointer()
{
	SDL_CALL SDL_ShowCursor(0);
	return true;
}

struct VkMapping {
#if SDL_VERSION_ATLEAST(1, 3, 0)
	SDL_Keycode vk_from;
#else
	uint16 vk_from;
#endif
	byte vk_count;
	byte map_to;
};

#define AS(x, z) {x, 0, z}
#define AM(x, y, z, w) {x, (byte)(y - x), z}

static const VkMapping _vk_mapping[] = {
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

static uint ConvertSdlKeyIntoMy(SDL_keysym *sym, WChar *character)
{
	const VkMapping *map;
	uint key = 0;

	for (map = _vk_mapping; map != endof(_vk_mapping); ++map) {
		if ((uint)(sym->sym - map->vk_from) <= map->vk_count) {
			key = sym->sym - map->vk_from + map->map_to;
			break;
		}
	}

	/* check scancode for BACKQUOTE key, because we want the key left of "1", not anything else (on non-US keyboards) */
#if defined(WIN32) || defined(__OS2__)
	if (sym->scancode == 41) key = WKC_BACKQUOTE;
#elif defined(__APPLE__)
	if (sym->scancode == 10) key = WKC_BACKQUOTE;
#elif defined(__MORPHOS__)
	if (sym->scancode == 0)  key = WKC_BACKQUOTE;  // yes, that key is code '0' under MorphOS :)
#elif defined(__BEOS__)
	if (sym->scancode == 17) key = WKC_BACKQUOTE;
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

int VideoDriver_SDL::PollEvent()
{
	SDL_Event ev;

	if (!SDL_CALL SDL_PollEvent(&ev)) return -2;

	switch (ev.type) {
		case SDL_MOUSEMOTION:
			if (_cursor.UpdateCursorPosition(ev.motion.x, ev.motion.y, true)) {
				SDL_CALL SDL_WarpMouse(_cursor.pos.x, _cursor.pos.y);
			}
			HandleMouseEvents();
			break;

		case SDL_MOUSEBUTTONDOWN:
			if (_rightclick_emulate && SDL_CALL SDL_GetModState() & KMOD_CTRL) {
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
				WChar character;
				uint keycode = ConvertSdlKeyIntoMy(&ev.key.keysym, &character);
				HandleKeypress(keycode, character);
			}
			break;

		case SDL_VIDEORESIZE: {
			int w = max(ev.resize.w, 64);
			int h = max(ev.resize.h, 64);
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
	return -1;
}

const char *VideoDriver_SDL::Start(const char * const *parm)
{
	char buf[30];
	_use_hwpalette = GetDriverParamInt(parm, "hw_palette", 2);

	const char *s = SdlOpen(SDL_INIT_VIDEO);
	if (s != NULL) return s;

	GetVideoModes();
	if (!CreateMainSurface(_cur_resolution.width, _cur_resolution.height)) {
		return SDL_CALL SDL_GetError();
	}

	SDL_CALL SDL_VideoDriverName(buf, sizeof buf);
	DEBUG(driver, 1, "SDL: using driver '%s'", buf);

	MarkWholeScreenDirty();
	SetupKeyboard();

	_draw_threaded = GetDriverParam(parm, "no_threads") == NULL && GetDriverParam(parm, "no_thread") == NULL;

	return NULL;
}

void VideoDriver_SDL::SetupKeyboard()
{
	SDL_CALL SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	SDL_CALL SDL_EnableUNICODE(1);
}

void VideoDriver_SDL::Stop()
{
	SdlClose(SDL_INIT_VIDEO);
}

void VideoDriver_SDL::MainLoop()
{
	uint32 cur_ticks = SDL_CALL SDL_GetTicks();
	uint32 last_cur_ticks = cur_ticks;
	uint32 next_tick = cur_ticks + MILLISECONDS_PER_TICK;
	uint32 mod;
	int numkeys;
	Uint8 *keys;

	CheckPaletteAnim();

	if (_draw_threaded) {
		/* Initialise the mutex first, because that's the thing we *need*
		 * directly in the newly created thread. */
		_draw_mutex = ThreadMutex::New();
		if (_draw_mutex == NULL) {
			_draw_threaded = false;
		} else {
			_draw_mutex->BeginCritical();
			_draw_continue = true;

			_draw_threaded = ThreadObject::New(&DrawSurfaceToScreenThread, NULL, &_draw_thread, "ottd:draw-sdl");

			/* Free the mutex if we won't be able to use it. */
			if (!_draw_threaded) {
				_draw_mutex->EndCritical();
				delete _draw_mutex;
				_draw_mutex = NULL;
			} else {
				/* Wait till the draw mutex has started itself. */
				_draw_mutex->WaitForSignal();
			}
		}
	}

	DEBUG(driver, 1, "SDL: using %sthreads", _draw_threaded ? "" : "no ");

	for (;;) {
		uint32 prev_cur_ticks = cur_ticks; // to check for wrapping
		InteractiveRandom(); // randomness

		while (PollEvent() == -1) {}
		if (_exit_game) break;

		mod = SDL_CALL SDL_GetModState();
#if SDL_VERSION_ATLEAST(1, 3, 0)
		keys = SDL_CALL SDL_GetKeyboardState(&numkeys);
#else
		keys = SDL_CALL SDL_GetKeyState(&numkeys);
#endif
#if defined(_DEBUG)
		if (_shift_pressed)
#else
		/* Speedup when pressing tab, except when using ALT+TAB
		 * to switch to another application */
#if SDL_VERSION_ATLEAST(1, 3, 0)
		if (keys[SDL_SCANCODE_TAB] && (mod & KMOD_ALT) == 0)
#else
		if (keys[SDLK_TAB] && (mod & KMOD_ALT) == 0)
#endif /* SDL_VERSION_ATLEAST(1, 3, 0) */
#endif /* defined(_DEBUG) */
		{
			if (!_networking && _game_mode != GM_MENU) _fast_forward |= 2;
		} else if (_fast_forward & 2) {
			_fast_forward = 0;
		}

		cur_ticks = SDL_CALL SDL_GetTicks();
		if (cur_ticks >= next_tick || (_fast_forward && !_pause_mode) || cur_ticks < prev_cur_ticks) {
			_realtime_tick += cur_ticks - last_cur_ticks;
			last_cur_ticks = cur_ticks;
			next_tick = cur_ticks + MILLISECONDS_PER_TICK;

			bool old_ctrl_pressed = _ctrl_pressed;

			_ctrl_pressed  = !!(mod & KMOD_CTRL);
			_shift_pressed = !!(mod & KMOD_SHIFT);

			/* determine which directional keys are down */
			_dirkeys =
#if SDL_VERSION_ATLEAST(1, 3, 0)
				(keys[SDL_SCANCODE_LEFT]  ? 1 : 0) |
				(keys[SDL_SCANCODE_UP]    ? 2 : 0) |
				(keys[SDL_SCANCODE_RIGHT] ? 4 : 0) |
				(keys[SDL_SCANCODE_DOWN]  ? 8 : 0);
#else
				(keys[SDLK_LEFT]  ? 1 : 0) |
				(keys[SDLK_UP]    ? 2 : 0) |
				(keys[SDLK_RIGHT] ? 4 : 0) |
				(keys[SDLK_DOWN]  ? 8 : 0);
#endif
			if (old_ctrl_pressed != _ctrl_pressed) HandleCtrlChanged();

			/* The gameloop is the part that can run asynchronously. The rest
			 * except sleeping can't. */
			if (_draw_mutex != NULL) _draw_mutex->EndCritical();

			GameLoop();

			if (_draw_mutex != NULL) _draw_mutex->BeginCritical();

			UpdateWindows();
			_local_palette = _cur_palette;
		} else {
			/* Release the thread while sleeping */
			if (_draw_mutex != NULL) _draw_mutex->EndCritical();
			CSleep(1);
			if (_draw_mutex != NULL) _draw_mutex->BeginCritical();

			NetworkDrawChatMessage();
			DrawMouseCursor();
		}

		/* End of the critical part. */
		if (_draw_mutex != NULL && !HasModalProgress()) {
			_draw_mutex->SendSignal();
		} else {
			/* Oh, we didn't have threads, then just draw unthreaded */
			CheckPaletteAnim();
			DrawSurfaceToScreen();
		}
	}

	if (_draw_mutex != NULL) {
		_draw_continue = false;
		/* Sending signal if there is no thread blocked
		 * is very valid and results in noop */
		_draw_mutex->SendSignal();
		_draw_mutex->EndCritical();
		_draw_thread->Join();

		delete _draw_mutex;
		delete _draw_thread;

		_draw_mutex = NULL;
		_draw_thread = NULL;
	}
}

bool VideoDriver_SDL::ChangeResolution(int w, int h)
{
	if (_draw_mutex != NULL) _draw_mutex->BeginCritical(true);
	bool ret = CreateMainSurface(w, h);
	if (_draw_mutex != NULL) _draw_mutex->EndCritical(true);
	return ret;
}

bool VideoDriver_SDL::ToggleFullscreen(bool fullscreen)
{
	if (_draw_mutex != NULL) _draw_mutex->BeginCritical(true);
	_fullscreen = fullscreen;
	GetVideoModes(); // get the list of available video modes
	bool ret = _num_resolutions != 0 && CreateMainSurface(_cur_resolution.width, _cur_resolution.height);

	if (!ret) {
		/* switching resolution failed, put back full_screen to original status */
		_fullscreen ^= true;
	}

	if (_draw_mutex != NULL) _draw_mutex->EndCritical(true);
	return ret;
}

bool VideoDriver_SDL::AfterBlitterChange()
{
	return CreateMainSurface(_screen.width, _screen.height);
}

void VideoDriver_SDL::AcquireBlitterLock()
{
	if (_draw_mutex != NULL) _draw_mutex->BeginCritical(true);
}

void VideoDriver_SDL::ReleaseBlitterLock()
{
	if (_draw_mutex != NULL) _draw_mutex->EndCritical(true);
}

#endif /* WITH_SDL */
