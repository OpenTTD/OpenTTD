/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file allegro_v.cpp Implementation of the Allegro video driver.
 * @note Implementing threaded pushing of data to the display is
 *       not faster (it's a few percent slower) in contrast to the
 *       results gained with threading it for SDL.
 */

#ifdef WITH_ALLEGRO

#include "../stdafx.h"
#include "../openttd.h"
#include "../gfx_func.h"
#include "../rev.h"
#include "../blitter/factory.hpp"
#include "../network/network.h"
#include "../core/random_func.hpp"
#include "../core/math_func.hpp"
#include "../framerate_type.h"
#include "allegro_v.h"
#include <allegro.h>

#include "../safeguards.h"

#ifdef _DEBUG
/* Allegro replaces SEGV/ABRT signals meaning that the debugger will never
 * be triggered, so rereplace the signals and make the debugger useful. */
#include <signal.h>
#endif

static FVideoDriver_Allegro iFVideoDriver_Allegro;

static BITMAP *_allegro_screen;

#define MAX_DIRTY_RECTS 100
static PointDimension _dirty_rects[MAX_DIRTY_RECTS];
static int _num_dirty_rects;

void VideoDriver_Allegro::MakeDirty(int left, int top, int width, int height)
{
	if (_num_dirty_rects < MAX_DIRTY_RECTS) {
		_dirty_rects[_num_dirty_rects].x = left;
		_dirty_rects[_num_dirty_rects].y = top;
		_dirty_rects[_num_dirty_rects].width = width;
		_dirty_rects[_num_dirty_rects].height = height;
	}
	_num_dirty_rects++;
}

static void DrawSurfaceToScreen()
{
	PerformanceMeasurer framerate(PFE_VIDEO);

	int n = _num_dirty_rects;
	if (n == 0) return;

	_num_dirty_rects = 0;
	if (n > MAX_DIRTY_RECTS) {
		blit(_allegro_screen, screen, 0, 0, 0, 0, _allegro_screen->w, _allegro_screen->h);
		return;
	}

	for (int i = 0; i < n; i++) {
		blit(_allegro_screen, screen, _dirty_rects[i].x, _dirty_rects[i].y, _dirty_rects[i].x, _dirty_rects[i].y, _dirty_rects[i].width, _dirty_rects[i].height);
	}
}


static void UpdatePalette(uint start, uint count)
{
	static PALETTE pal;

	uint end = start + count;
	for (uint i = start; i != end; i++) {
		pal[i].r = _cur_palette.palette[i].r / 4;
		pal[i].g = _cur_palette.palette[i].g / 4;
		pal[i].b = _cur_palette.palette[i].b / 4;
		pal[i].filler = 0;
	}

	set_palette_range(pal, start, end - 1, 1);
}

static void InitPalette()
{
	UpdatePalette(0, 256);
}

static void CheckPaletteAnim()
{
	if (_cur_palette.count_dirty != 0) {
		Blitter *blitter = BlitterFactory::GetCurrentBlitter();

		switch (blitter->UsePaletteAnimation()) {
			case Blitter::PALETTE_ANIMATION_VIDEO_BACKEND:
				UpdatePalette(_cur_palette.first_dirty, _cur_palette.count_dirty);
				break;

			case Blitter::PALETTE_ANIMATION_BLITTER:
				blitter->PaletteAnimate(_cur_palette);
				break;

			case Blitter::PALETTE_ANIMATION_NONE:
				break;

			default:
				NOT_REACHED();
		}
		_cur_palette.count_dirty = 0;
	}
}

static const Dimension default_resolutions[] = {
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
	/* Need to set a gfx_mode as there is NO other way to autodetect for
	 * cards ourselves... and we need a card to get the modes. */
	set_gfx_mode(_fullscreen ? GFX_AUTODETECT_FULLSCREEN : GFX_AUTODETECT_WINDOWED, 640, 480, 0, 0);

	GFX_MODE_LIST *mode_list = get_gfx_mode_list(gfx_driver->id);
	if (mode_list == NULL) {
		memcpy(_resolutions, default_resolutions, sizeof(default_resolutions));
		_num_resolutions = lengthof(default_resolutions);
		return;
	}

	GFX_MODE *modes = mode_list->mode;

	int n = 0;
	for (int i = 0; modes[i].bpp != 0; i++) {
		uint w = modes[i].width;
		uint h = modes[i].height;
		if (w >= 640 && h >= 480) {
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
	}
	_num_resolutions = n;
	SortResolutions(_num_resolutions);

	destroy_gfx_mode_list(mode_list);
}

static void GetAvailableVideoMode(uint *w, uint *h)
{
	/* No video modes, so just try it and see where it ends */
	if (_num_resolutions == 0) return;

	/* is the wanted mode among the available modes? */
	for (int i = 0; i != _num_resolutions; i++) {
		if (*w == _resolutions[i].width && *h == _resolutions[i].height) return;
	}

	/* use the closest possible resolution */
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

static bool CreateMainSurface(uint w, uint h)
{
	int bpp = BlitterFactory::GetCurrentBlitter()->GetScreenDepth();
	if (bpp == 0) usererror("Can't use a blitter that blits 0 bpp for normal visuals");
	set_color_depth(bpp);

	GetAvailableVideoMode(&w, &h);
	if (set_gfx_mode(_fullscreen ? GFX_AUTODETECT_FULLSCREEN : GFX_AUTODETECT_WINDOWED, w, h, 0, 0) != 0) {
		DEBUG(driver, 0, "Allegro: Couldn't allocate a window to draw on '%s'", allegro_error);
		return false;
	}

	/* The size of the screen might be bigger than the part we can actually draw on!
	 * So calculate the size based on the top, bottom, left and right */
	_allegro_screen = create_bitmap_ex(bpp, screen->cr - screen->cl, screen->cb - screen->ct);
	_screen.width = _allegro_screen->w;
	_screen.height = _allegro_screen->h;
	_screen.pitch = ((byte*)screen->line[1] - (byte*)screen->line[0]) / (bpp / 8);
	_screen.dst_ptr = _allegro_screen->line[0];

	/* Initialise the screen so we don't blit garbage to the screen */
	memset(_screen.dst_ptr, 0, _screen.height * _screen.pitch);

	/* Set the mouse at the place where we expect it */
	poll_mouse();
	_cursor.pos.x = mouse_x;
	_cursor.pos.y = mouse_y;

	BlitterFactory::GetCurrentBlitter()->PostResize();

	InitPalette();

	char caption[32];
	seprintf(caption, lastof(caption), "OpenTTD %s", _openttd_revision);
	set_window_title(caption);

	enable_hardware_cursor();
	select_mouse_cursor(MOUSE_CURSOR_ARROW);
	show_mouse(_allegro_screen);

	GameSizeChanged();

	return true;
}

bool VideoDriver_Allegro::ClaimMousePointer()
{
	select_mouse_cursor(MOUSE_CURSOR_NONE);
	show_mouse(NULL);
	disable_hardware_cursor();
	return true;
}

struct VkMapping {
	uint16 vk_from;
	byte vk_count;
	byte map_to;
};

#define AS(x, z) {x, 0, z}
#define AM(x, y, z, w) {x, y - x, z}

static const VkMapping _vk_mapping[] = {
	/* Pageup stuff + up/down */
	AM(KEY_PGUP, KEY_PGDN, WKC_PAGEUP, WKC_PAGEDOWN),
	AS(KEY_UP,     WKC_UP),
	AS(KEY_DOWN,   WKC_DOWN),
	AS(KEY_LEFT,   WKC_LEFT),
	AS(KEY_RIGHT,  WKC_RIGHT),

	AS(KEY_HOME,   WKC_HOME),
	AS(KEY_END,    WKC_END),

	AS(KEY_INSERT, WKC_INSERT),
	AS(KEY_DEL,    WKC_DELETE),

	/* Map letters & digits */
	AM(KEY_A, KEY_Z, 'A', 'Z'),
	AM(KEY_0, KEY_9, '0', '9'),

	AS(KEY_ESC,       WKC_ESC),
	AS(KEY_PAUSE,     WKC_PAUSE),
	AS(KEY_BACKSPACE, WKC_BACKSPACE),

	AS(KEY_SPACE,     WKC_SPACE),
	AS(KEY_ENTER,     WKC_RETURN),
	AS(KEY_TAB,       WKC_TAB),

	/* Function keys */
	AM(KEY_F1, KEY_F12, WKC_F1, WKC_F12),

	/* Numeric part. */
	AM(KEY_0_PAD, KEY_9_PAD, '0', '9'),
	AS(KEY_SLASH_PAD,   WKC_NUM_DIV),
	AS(KEY_ASTERISK,    WKC_NUM_MUL),
	AS(KEY_MINUS_PAD,   WKC_NUM_MINUS),
	AS(KEY_PLUS_PAD,    WKC_NUM_PLUS),
	AS(KEY_ENTER_PAD,   WKC_NUM_ENTER),
	AS(KEY_DEL_PAD,     WKC_DELETE),

	/* Other non-letter keys */
	AS(KEY_SLASH,        WKC_SLASH),
	AS(KEY_SEMICOLON,    WKC_SEMICOLON),
	AS(KEY_EQUALS,       WKC_EQUALS),
	AS(KEY_OPENBRACE,    WKC_L_BRACKET),
	AS(KEY_BACKSLASH,    WKC_BACKSLASH),
	AS(KEY_CLOSEBRACE,   WKC_R_BRACKET),

	AS(KEY_QUOTE,   WKC_SINGLEQUOTE),
	AS(KEY_COMMA,   WKC_COMMA),
	AS(KEY_MINUS,   WKC_MINUS),
	AS(KEY_STOP,    WKC_PERIOD),
	AS(KEY_TILDE,   WKC_BACKQUOTE),
};

static uint32 ConvertAllegroKeyIntoMy(WChar *character)
{
	int scancode;
	int unicode = ureadkey(&scancode);

	const VkMapping *map;
	uint key = 0;

	for (map = _vk_mapping; map != endof(_vk_mapping); ++map) {
		if ((uint)(scancode - map->vk_from) <= map->vk_count) {
			key = scancode - map->vk_from + map->map_to;
			break;
		}
	}

	if (key_shifts & KB_SHIFT_FLAG) key |= WKC_SHIFT;
	if (key_shifts & KB_CTRL_FLAG)  key |= WKC_CTRL;
	if (key_shifts & KB_ALT_FLAG)   key |= WKC_ALT;
#if 0
	DEBUG(driver, 0, "Scancode character pressed %u", scancode);
	DEBUG(driver, 0, "Unicode character pressed %u", unicode);
#endif

	*character = unicode;
	return key;
}

static const uint LEFT_BUTTON  = 0;
static const uint RIGHT_BUTTON = 1;

static void PollEvent()
{
	poll_mouse();

	bool mouse_action = false;

	/* Mouse buttons */
	static int prev_button_state;
	if (prev_button_state != mouse_b) {
		uint diff = prev_button_state ^ mouse_b;
		while (diff != 0) {
			uint button = FindFirstBit(diff);
			ClrBit(diff, button);
			if (HasBit(mouse_b, button)) {
				/* Pressed mouse button */
				if (_rightclick_emulate && (key_shifts & KB_CTRL_FLAG)) {
					button = RIGHT_BUTTON;
					ClrBit(diff, RIGHT_BUTTON);
				}
				switch (button) {
					case LEFT_BUTTON:
						_left_button_down = true;
						break;

					case RIGHT_BUTTON:
						_right_button_down = true;
						_right_button_clicked = true;
						break;

					default:
						/* ignore rest */
						break;
				}
			} else {
				/* Released mouse button */
				if (_rightclick_emulate) {
					_right_button_down = false;
					_left_button_down = false;
					_left_button_clicked = false;
				} else if (button == LEFT_BUTTON) {
					_left_button_down = false;
					_left_button_clicked = false;
				} else if (button == RIGHT_BUTTON) {
					_right_button_down = false;
				}
			}
		}
		prev_button_state = mouse_b;
		mouse_action = true;
	}

	/* Mouse movement */
	if (_cursor.UpdateCursorPosition(mouse_x, mouse_y, false)) {
		position_mouse(_cursor.pos.x, _cursor.pos.y);
	}
	if (_cursor.delta.x != 0 || _cursor.delta.y) mouse_action = true;

	static int prev_mouse_z = 0;
	if (prev_mouse_z != mouse_z) {
		_cursor.wheel = (prev_mouse_z - mouse_z) < 0 ? -1 : 1;
		prev_mouse_z = mouse_z;
		mouse_action = true;
	}

	if (mouse_action) HandleMouseEvents();

	poll_keyboard();
	if ((key_shifts & KB_ALT_FLAG) && (key[KEY_ENTER] || key[KEY_F])) {
		ToggleFullScreen(!_fullscreen);
	} else if (keypressed()) {
		WChar character;
		uint keycode = ConvertAllegroKeyIntoMy(&character);
		HandleKeypress(keycode, character);
	}
}

/**
 * There are multiple modules that might be using Allegro and
 * Allegro can only be initiated once.
 */
int _allegro_instance_count = 0;

const char *VideoDriver_Allegro::Start(const char * const *parm)
{
	if (_allegro_instance_count == 0 && install_allegro(SYSTEM_AUTODETECT, &errno, NULL)) {
		DEBUG(driver, 0, "allegro: install_allegro failed '%s'", allegro_error);
		return "Failed to set up Allegro";
	}
	_allegro_instance_count++;

	install_timer();
	install_mouse();
	install_keyboard();

#if defined _DEBUG
/* Allegro replaces SEGV/ABRT signals meaning that the debugger will never
 * be triggered, so rereplace the signals and make the debugger useful. */
	signal(SIGABRT, NULL);
	signal(SIGSEGV, NULL);
#endif

#if defined(DOS)
	/* Force DOS builds to ALWAYS use full screen as
	 * it can't do windowed. */
	_fullscreen = true;
#endif

	GetVideoModes();
	if (!CreateMainSurface(_cur_resolution.width, _cur_resolution.height)) {
		return "Failed to set up Allegro video";
	}
	MarkWholeScreenDirty();
	set_close_button_callback(HandleExitGameRequest);

	return NULL;
}

void VideoDriver_Allegro::Stop()
{
	if (--_allegro_instance_count == 0) allegro_exit();
}

#if defined(UNIX) || defined(__OS2__) || defined(DOS)
# include <sys/time.h> /* gettimeofday */

static uint32 GetTime()
{
	struct timeval tim;

	gettimeofday(&tim, NULL);
	return tim.tv_usec / 1000 + tim.tv_sec * 1000;
}
#else
static uint32 GetTime()
{
	return GetTickCount();
}
#endif


void VideoDriver_Allegro::MainLoop()
{
	uint32 cur_ticks = GetTime();
	uint32 last_cur_ticks = cur_ticks;
	uint32 next_tick = cur_ticks + MILLISECONDS_PER_TICK;

	CheckPaletteAnim();

	for (;;) {
		uint32 prev_cur_ticks = cur_ticks; // to check for wrapping
		InteractiveRandom(); // randomness

		PollEvent();
		if (_exit_game) return;

#if defined(_DEBUG)
		if (_shift_pressed)
#else
		/* Speedup when pressing tab, except when using ALT+TAB
		 * to switch to another application */
		if (key[KEY_TAB] && (key_shifts & KB_ALT_FLAG) == 0)
#endif
		{
			if (!_networking && _game_mode != GM_MENU) _fast_forward |= 2;
		} else if (_fast_forward & 2) {
			_fast_forward = 0;
		}

		cur_ticks = GetTime();
		if (cur_ticks >= next_tick || (_fast_forward && !_pause_mode) || cur_ticks < prev_cur_ticks) {
			_realtime_tick += cur_ticks - last_cur_ticks;
			last_cur_ticks = cur_ticks;
			next_tick = cur_ticks + MILLISECONDS_PER_TICK;

			bool old_ctrl_pressed = _ctrl_pressed;

			_ctrl_pressed  = !!(key_shifts & KB_CTRL_FLAG);
			_shift_pressed = !!(key_shifts & KB_SHIFT_FLAG);

			/* determine which directional keys are down */
			_dirkeys =
				(key[KEY_LEFT]  ? 1 : 0) |
				(key[KEY_UP]    ? 2 : 0) |
				(key[KEY_RIGHT] ? 4 : 0) |
				(key[KEY_DOWN]  ? 8 : 0);

			if (old_ctrl_pressed != _ctrl_pressed) HandleCtrlChanged();

			GameLoop();

			UpdateWindows();
			CheckPaletteAnim();
			DrawSurfaceToScreen();
		} else {
			CSleep(1);
			NetworkDrawChatMessage();
			DrawMouseCursor();
			DrawSurfaceToScreen();
		}
	}
}

bool VideoDriver_Allegro::ChangeResolution(int w, int h)
{
	return CreateMainSurface(w, h);
}

bool VideoDriver_Allegro::ToggleFullscreen(bool fullscreen)
{
#ifdef DOS
	return false;
#else
	_fullscreen = fullscreen;
	GetVideoModes(); // get the list of available video modes
	if (_num_resolutions == 0 || !this->ChangeResolution(_cur_resolution.width, _cur_resolution.height)) {
		/* switching resolution failed, put back full_screen to original status */
		_fullscreen ^= true;
		return false;
	}
	return true;
#endif
}

bool VideoDriver_Allegro::AfterBlitterChange()
{
	return CreateMainSurface(_screen.width, _screen.height);
}

#endif /* WITH_ALLEGRO */
