/* $Id$ */

#include "../stdafx.h"

#ifdef WITH_SDL

#include "../openttd.h"
#include "../debug.h"
#include "../functions.h"
#include "../gfx.h"
#include "../macros.h"
#include "../sdl.h"
#include "../window.h"
#include "../network.h"
#include "../variables.h"
#include "sdl_v.h"
#include <SDL.h>

static SDL_Surface *_sdl_screen;
static bool _all_modes;

#define MAX_DIRTY_RECTS 100
static SDL_Rect _dirty_rects[MAX_DIRTY_RECTS];
static int _num_dirty_rects;

static void SdlVideoMakeDirty(int left, int top, int width, int height)
{
	if (_num_dirty_rects < MAX_DIRTY_RECTS) {
		_dirty_rects[_num_dirty_rects].x = left;
		_dirty_rects[_num_dirty_rects].y = top;
		_dirty_rects[_num_dirty_rects].w = width;
		_dirty_rects[_num_dirty_rects].h = height;
	}
	_num_dirty_rects++;
}

static void UpdatePalette(uint start, uint count)
{
	SDL_Color pal[256];
	uint i;

	for (i = 0; i != count; i++) {
		pal[i].r = _cur_palette[start + i].r;
		pal[i].g = _cur_palette[start + i].g;
		pal[i].b = _cur_palette[start + i].b;
		pal[i].unused = 0;
	}

	SDL_CALL SDL_SetColors(_sdl_screen, pal, start, count);
}

static void InitPalette(void)
{
	UpdatePalette(0, 256);
}

static void CheckPaletteAnim(void)
{
	if (_pal_last_dirty != -1) {
		UpdatePalette(_pal_first_dirty, _pal_last_dirty - _pal_first_dirty + 1);
		_pal_last_dirty = -1;
	}
}

static void DrawSurfaceToScreen(void)
{
	int n = _num_dirty_rects;
	if (n != 0) {
		_num_dirty_rects = 0;
		if (n > MAX_DIRTY_RECTS)
			SDL_CALL SDL_UpdateRect(_sdl_screen, 0, 0, 0, 0);
		else
			SDL_CALL SDL_UpdateRects(_sdl_screen, n, _dirty_rects);
	}
}

static const uint16 default_resolutions[][2] = {
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

static void GetVideoModes(void)
{
	int i;
	SDL_Rect **modes;

	modes = SDL_CALL SDL_ListModes(NULL, SDL_SWSURFACE + (_fullscreen ? SDL_FULLSCREEN : 0));

	if (modes == NULL)
		error("sdl: no modes available");

	_all_modes = (modes == (void*)-1);

	if (_all_modes) {
		// all modes available, put some default ones here
		memcpy(_resolutions, default_resolutions, sizeof(default_resolutions));
		_num_resolutions = lengthof(default_resolutions);
	} else {
		int n = 0;
		for (i = 0; modes[i]; i++) {
			int w = modes[i]->w;
			int h = modes[i]->h;
			if (IS_INT_INSIDE(w, 640, MAX_SCREEN_WIDTH + 1) &&
					IS_INT_INSIDE(h, 480, MAX_SCREEN_HEIGHT + 1)) {
				int j;
				for (j = 0; j < n; j++) {
					if (_resolutions[j][0] == w && _resolutions[j][1] == h) break;
				}

				if (j == n) {
					_resolutions[j][0] = w;
					_resolutions[j][1] = h;
					if (++n == lengthof(_resolutions)) break;
				}
			}
		}
		_num_resolutions = n;
		SortResolutions(_num_resolutions);
	}
}

static void GetAvailableVideoMode(int *w, int *h)
{
	int i;
	int best;
	uint delta;

	// all modes available?
	if (_all_modes) return;

	// is the wanted mode among the available modes?
	for (i = 0; i != _num_resolutions; i++) {
		if (*w == _resolutions[i][0] && *h == _resolutions[i][1]) return;
	}

	// use the closest possible resolution
	best = 0;
	delta = abs((_resolutions[0][0] - *w) * (_resolutions[0][1] - *h));
	for (i = 1; i != _num_resolutions; ++i) {
		uint newdelta = abs((_resolutions[i][0] - *w) * (_resolutions[i][1] - *h));
		if (newdelta < delta) {
			best = i;
			delta = newdelta;
		}
	}
	*w = _resolutions[best][0];
	*h = _resolutions[best][1];
}

#ifndef ICON_DIR
#define ICON_DIR "media"
#endif

#ifdef WIN32
/* Let's redefine the LoadBMP macro with because we are dynamically
 * loading SDL and need to 'SDL_CALL' all functions */
#undef SDL_LoadBMP
#define SDL_LoadBMP(file)	SDL_LoadBMP_RW(SDL_CALL SDL_RWFromFile(file, "rb"), 1)
#endif

static bool CreateMainSurface(int w, int h)
{
	extern const char _openttd_revision[];
	SDL_Surface *newscreen, *icon;
	char caption[50];

	GetAvailableVideoMode(&w, &h);

	DEBUG(driver, 1, "SDL: using mode %dx%d", w, h);

	/* Give the application an icon */
	icon = SDL_CALL SDL_LoadBMP(ICON_DIR PATHSEP "openttd.32.bmp");
	if (icon != NULL) {
		/* Get the colourkey, which will be magenta */
		uint32 rgbmap = SDL_CALL SDL_MapRGB(icon->format, 255, 0, 255);

		SDL_CALL SDL_SetColorKey(icon, SDL_SRCCOLORKEY, rgbmap);
		SDL_CALL SDL_WM_SetIcon(icon, NULL);
		SDL_CALL SDL_FreeSurface(icon);
	}

	// DO NOT CHANGE TO HWSURFACE, IT DOES NOT WORK
	newscreen = SDL_CALL SDL_SetVideoMode(w, h, 8, SDL_SWSURFACE | SDL_HWPALETTE | (_fullscreen ? SDL_FULLSCREEN : SDL_RESIZABLE));
	if (newscreen == NULL)
		return false;

	_screen.width = newscreen->w;
	_screen.height = newscreen->h;
	_screen.pitch = newscreen->pitch;

	_sdl_screen = newscreen;
	InitPalette();

	snprintf(caption, sizeof(caption), "OpenTTD %s", _openttd_revision);
	SDL_CALL SDL_WM_SetCaption(caption, caption);
	SDL_CALL SDL_ShowCursor(0);

	GameSizeChanged();

	return true;
}

typedef struct VkMapping {
	uint16 vk_from;
	byte vk_count;
	byte map_to;
} VkMapping;

#define AS(x, z) {x, 0, z}
#define AM(x, y, z, w) {x, y - x, z}

static const VkMapping _vk_mapping[] = {
	// Pageup stuff + up/down
	AM(SDLK_PAGEUP, SDLK_PAGEDOWN, WKC_PAGEUP, WKC_PAGEDOWN),
	AS(SDLK_UP,     WKC_UP),
	AS(SDLK_DOWN,   WKC_DOWN),
	AS(SDLK_LEFT,   WKC_LEFT),
	AS(SDLK_RIGHT,  WKC_RIGHT),

	AS(SDLK_HOME,   WKC_HOME),
	AS(SDLK_END,    WKC_END),

	AS(SDLK_INSERT, WKC_INSERT),
	AS(SDLK_DELETE, WKC_DELETE),

	// Map letters & digits
	AM(SDLK_a, SDLK_z, 'A', 'Z'),
	AM(SDLK_0, SDLK_9, '0', '9'),

	AS(SDLK_ESCAPE,    WKC_ESC),
	AS(SDLK_PAUSE,     WKC_PAUSE),
	AS(SDLK_BACKSPACE, WKC_BACKSPACE),

	AS(SDLK_SPACE,     WKC_SPACE),
	AS(SDLK_RETURN,    WKC_RETURN),
	AS(SDLK_TAB,       WKC_TAB),

	// Function keys
	AM(SDLK_F1, SDLK_F12, WKC_F1, WKC_F12),

	// Numeric part.
	// What is the virtual keycode for numeric enter??
	AM(SDLK_KP0, SDLK_KP9, WKC_NUM_0, WKC_NUM_9),
	AS(SDLK_KP_DIVIDE,   WKC_NUM_DIV),
	AS(SDLK_KP_MULTIPLY, WKC_NUM_MUL),
	AS(SDLK_KP_MINUS,    WKC_NUM_MINUS),
	AS(SDLK_KP_PLUS,     WKC_NUM_PLUS),
	AS(SDLK_KP_ENTER,    WKC_NUM_ENTER),
	AS(SDLK_KP_PERIOD,   WKC_NUM_DECIMAL)
};

static uint32 ConvertSdlKeyIntoMy(SDL_keysym *sym)
{
	const VkMapping *map;
	uint key = 0;

	for (map = _vk_mapping; map != endof(_vk_mapping); ++map) {
		if ((uint)(sym->sym - map->vk_from) <= map->vk_count) {
			key = sym->sym - map->vk_from + map->map_to;
			break;
		}
	}

	// check scancode for BACKQUOTE key, because we want the key left of "1", not anything else (on non-US keyboards)
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

	// META are the command keys on mac
	if (sym->mod & KMOD_META)  key |= WKC_META;
	if (sym->mod & KMOD_SHIFT) key |= WKC_SHIFT;
	if (sym->mod & KMOD_CTRL)  key |= WKC_CTRL;
	if (sym->mod & KMOD_ALT)   key |= WKC_ALT;
	// these two lines really help porting hotkey combos. Uncomment to use -- Bjarni
#if 0
	DEBUG(driver, 0, "Scancode character pressed %u", sym->scancode);
	DEBUG(driver, 0, "Unicode character pressed %u", sym->unicode);
#endif
	return (key << 16) + sym->unicode;
}

static int PollEvent(void)
{
	SDL_Event ev;

	if (!SDL_CALL SDL_PollEvent(&ev)) return -2;

	switch (ev.type) {
		case SDL_MOUSEMOTION:
			if (_cursor.fix_at) {
				int dx = ev.motion.x - _cursor.pos.x;
				int dy = ev.motion.y - _cursor.pos.y;
				if (dx != 0 || dy != 0) {
					_cursor.delta.x += dx;
					_cursor.delta.y += dy;
					SDL_CALL SDL_WarpMouse(_cursor.pos.x, _cursor.pos.y);
				}
			} else {
				_cursor.delta.x = ev.motion.x - _cursor.pos.x;
				_cursor.delta.y = ev.motion.y - _cursor.pos.y;
				_cursor.pos.x = ev.motion.x;
				_cursor.pos.y = ev.motion.y;
				_cursor.dirty = true;
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

		case SDL_QUIT: HandleExitGameRequest(); break;

		case SDL_KEYDOWN: /* Toggle full-screen on ALT + ENTER/F */
			if ((ev.key.keysym.mod & (KMOD_ALT | KMOD_META)) &&
					(ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_f)) {
				ToggleFullScreen(!_fullscreen);
			} else {
				HandleKeypress(ConvertSdlKeyIntoMy(&ev.key.keysym));
			}
			break;

		case SDL_VIDEORESIZE: {
			int w = clamp(ev.resize.w, 64, MAX_SCREEN_WIDTH);
			int h = clamp(ev.resize.h, 64, MAX_SCREEN_HEIGHT);
			ChangeResInGame(w, h);
			break;
		}
	}
	return -1;
}

static const char *SdlVideoStart(const char * const *parm)
{
	char buf[30];

	const char *s = SdlOpen(SDL_INIT_VIDEO);
	if (s != NULL) return s;

	SDL_CALL SDL_VideoDriverName(buf, 30);
	DEBUG(driver, 1, "SDL: using driver '%s'", buf);

	GetVideoModes();
	CreateMainSurface(_cur_resolution[0], _cur_resolution[1]);
	MarkWholeScreenDirty();

	SDL_CALL SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	SDL_CALL SDL_EnableUNICODE(1);
	return NULL;
}

static void SdlVideoStop(void)
{
	SdlClose(SDL_INIT_VIDEO);
}

static void SdlVideoMainLoop(void)
{
	uint32 next_tick = SDL_CALL SDL_GetTicks() + 30;
	uint32 cur_ticks;
	uint32 pal_tick = 0;
	uint32 mod;
	int numkeys;
	Uint8 *keys;

	for (;;) {
		InteractiveRandom(); // randomness

		while (PollEvent() == -1) {}
		if (_exit_game) return;

		mod = SDL_CALL SDL_GetModState();
		keys = SDL_CALL SDL_GetKeyState(&numkeys);
#if defined(_DEBUG)
		if (_shift_pressed)
#else
		/* Speedup when pressing tab, except when using ALT+TAB
		 * to switch to another application */
		if (keys[SDLK_TAB] && (mod & KMOD_ALT) == 0)
#endif
		{
			if (!_networking && _game_mode != GM_MENU) _fast_forward |= 2;
		} else if (_fast_forward & 2) {
			_fast_forward = 0;
		}

		cur_ticks = SDL_CALL SDL_GetTicks();
		if ((_fast_forward && !_pause) || cur_ticks > next_tick)
			next_tick = cur_ticks;

		if (cur_ticks == next_tick) {
			next_tick += 30;

			_ctrl_pressed  = !!(mod & KMOD_CTRL);
			_shift_pressed = !!(mod & KMOD_SHIFT);
#ifdef _DEBUG
			_dbg_screen_rect = !!(mod & KMOD_CAPS);
#endif

			// determine which directional keys are down
			_dirkeys =
				(keys[SDLK_LEFT]  ? 1 : 0) |
				(keys[SDLK_UP]    ? 2 : 0) |
				(keys[SDLK_RIGHT] ? 4 : 0) |
				(keys[SDLK_DOWN]  ? 8 : 0);
			GameLoop();

			_screen.dst_ptr = _sdl_screen->pixels;
			UpdateWindows();
			if (++pal_tick > 4) {
				CheckPaletteAnim();
				pal_tick = 1;
			}
			DrawSurfaceToScreen();
		} else {
			SDL_CALL SDL_Delay(1);
			_screen.dst_ptr = _sdl_screen->pixels;
			DrawTextMessage();
			DrawMouseCursor();
			DrawSurfaceToScreen();
		}
	}
}

static bool SdlVideoChangeRes(int w, int h)
{
	return CreateMainSurface(w, h);
}

static void SdlVideoFullScreen(bool full_screen)
{
	_fullscreen = full_screen;
	GetVideoModes(); // get the list of available video modes
	if (_num_resolutions == 0 || !_video_driver->change_resolution(_cur_resolution[0], _cur_resolution[1])) {
		// switching resolution failed, put back full_screen to original status
		_fullscreen ^= true;
	}
}

const HalVideoDriver _sdl_video_driver = {
	SdlVideoStart,
	SdlVideoStop,
	SdlVideoMakeDirty,
	SdlVideoMainLoop,
	SdlVideoChangeRes,
	SdlVideoFullScreen,
};

#endif
