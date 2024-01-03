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
#include "../error_func.h"
#include "../gfx_func.h"
#include "../blitter/factory.hpp"
#include "../core/random_func.hpp"
#include "../core/math_func.hpp"
#include "../framerate_type.h"
#include "../progress.h"
#include "../thread.h"
#include "../window_func.h"
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
static Palette _local_palette; ///< Current palette to use for drawing.

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

void VideoDriver_Allegro::Paint()
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
		pal[i].r = _local_palette.palette[i].r / 4;
		pal[i].g = _local_palette.palette[i].g / 4;
		pal[i].b = _local_palette.palette[i].b / 4;
		pal[i].filler = 0;
	}

	set_palette_range(pal, start, end - 1, 1);
}

static void InitPalette()
{
	UpdatePalette(0, 256);
}

void VideoDriver_Allegro::CheckPaletteAnim()
{
	if (!CopyPalette(_local_palette)) return;

	Blitter *blitter = BlitterFactory::GetCurrentBlitter();

	switch (blitter->UsePaletteAnimation()) {
		case Blitter::PALETTE_ANIMATION_VIDEO_BACKEND:
			UpdatePalette(_local_palette.first_dirty, _local_palette.count_dirty);
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

	_resolutions.clear();

	GFX_MODE_LIST *mode_list = get_gfx_mode_list(gfx_driver->id);
	if (mode_list == nullptr) {
		_resolutions.assign(std::begin(default_resolutions), std::end(default_resolutions));
		return;
	}

	GFX_MODE *modes = mode_list->mode;

	for (int i = 0; modes[i].bpp != 0; i++) {
		uint w = modes[i].width;
		uint h = modes[i].height;
		if (w < 640 || h < 480) continue;
		if (std::find(_resolutions.begin(), _resolutions.end(), Dimension(w, h)) != _resolutions.end()) continue;
		_resolutions.emplace_back(w, h);
	}

	SortResolutions();

	destroy_gfx_mode_list(mode_list);
}

static void GetAvailableVideoMode(uint *w, uint *h)
{
	/* No video modes, so just try it and see where it ends */
	if (_resolutions.empty()) return;

	/* is the wanted mode among the available modes? */
	if (std::find(_resolutions.begin(), _resolutions.end(), Dimension(*w, *h)) != _resolutions.end()) return;

	/* use the closest possible resolution */
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

static bool CreateMainSurface(uint w, uint h)
{
	int bpp = BlitterFactory::GetCurrentBlitter()->GetScreenDepth();
	if (bpp == 0) UserError("Can't use a blitter that blits 0 bpp for normal visuals");
	set_color_depth(bpp);

	GetAvailableVideoMode(&w, &h);
	if (set_gfx_mode(_fullscreen ? GFX_AUTODETECT_FULLSCREEN : GFX_AUTODETECT_WINDOWED, w, h, 0, 0) != 0) {
		Debug(driver, 0, "Allegro: Couldn't allocate a window to draw on '{}'", allegro_error);
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
	memset(_screen.dst_ptr, 0, static_cast<size_t>(_screen.height) * _screen.pitch);

	/* Set the mouse at the place where we expect it */
	poll_mouse();
	_cursor.pos.x = mouse_x;
	_cursor.pos.y = mouse_y;

	BlitterFactory::GetCurrentBlitter()->PostResize();

	InitPalette();

	std::string caption = VideoDriver::GetCaption();
	set_window_title(caption.c_str());

	enable_hardware_cursor();
	select_mouse_cursor(MOUSE_CURSOR_ARROW);
	show_mouse(_allegro_screen);

	GameSizeChanged();

	return true;
}

bool VideoDriver_Allegro::ClaimMousePointer()
{
	select_mouse_cursor(MOUSE_CURSOR_NONE);
	show_mouse(nullptr);
	disable_hardware_cursor();
	return true;
}

std::vector<int> VideoDriver_Allegro::GetListOfMonitorRefreshRates()
{
	std::vector<int> rates = {};

	int refresh_rate = get_refresh_rate();
	if (refresh_rate != 0) rates.push_back(refresh_rate);

	return rates;
}

struct AllegroVkMapping {
	uint16_t vk_from;
	byte vk_count;
	byte map_to;
};

#define AS(x, z) {x, 0, z}
#define AM(x, y, z, w) {x, y - x, z}

static const AllegroVkMapping _vk_mapping[] = {
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

static uint32_t ConvertAllegroKeyIntoMy(char32_t *character)
{
	int scancode;
	int unicode = ureadkey(&scancode);

	const AllegroVkMapping *map;
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
	Debug(driver, 0, "Scancode character pressed {}", scancode);
	Debug(driver, 0, "Unicode character pressed {}", unicode);
#endif

	*character = unicode;
	return key;
}

static const uint LEFT_BUTTON  = 0;
static const uint RIGHT_BUTTON = 1;

bool VideoDriver_Allegro::PollEvent()
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
	if (_cursor.UpdateCursorPosition(mouse_x, mouse_y)) {
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
		char32_t character;
		uint keycode = ConvertAllegroKeyIntoMy(&character);
		HandleKeypress(keycode, character);
	}

	return false;
}

/**
 * There are multiple modules that might be using Allegro and
 * Allegro can only be initiated once.
 */
int _allegro_instance_count = 0;

const char *VideoDriver_Allegro::Start(const StringList &param)
{
	if (_allegro_instance_count == 0 && install_allegro(SYSTEM_AUTODETECT, &errno, nullptr)) {
		Debug(driver, 0, "allegro: install_allegro failed '{}'", allegro_error);
		return "Failed to set up Allegro";
	}
	_allegro_instance_count++;

	this->UpdateAutoResolution();

	install_timer();
	install_mouse();
	install_keyboard();

#if defined _DEBUG
/* Allegro replaces SEGV/ABRT signals meaning that the debugger will never
 * be triggered, so rereplace the signals and make the debugger useful. */
	signal(SIGABRT, nullptr);
	signal(SIGSEGV, nullptr);
#endif

	GetVideoModes();
	if (!CreateMainSurface(_cur_resolution.width, _cur_resolution.height)) {
		return "Failed to set up Allegro video";
	}
	MarkWholeScreenDirty();
	set_close_button_callback(HandleExitGameRequest);

	this->is_game_threaded = !GetDriverParamBool(param, "no_threads") && !GetDriverParamBool(param, "no_thread");

	return nullptr;
}

void VideoDriver_Allegro::Stop()
{
	if (--_allegro_instance_count == 0) allegro_exit();
}

void VideoDriver_Allegro::InputLoop()
{
	bool old_ctrl_pressed = _ctrl_pressed;

	_ctrl_pressed  = !!(key_shifts & KB_CTRL_FLAG);
	_shift_pressed = !!(key_shifts & KB_SHIFT_FLAG);

	/* Speedup when pressing tab, except when using ALT+TAB
	 * to switch to another application. */
	this->fast_forward_key_pressed = key[KEY_TAB] && (key_shifts & KB_ALT_FLAG) == 0;

	/* Determine which directional keys are down. */
	_dirkeys =
		(key[KEY_LEFT]  ? 1 : 0) |
		(key[KEY_UP]    ? 2 : 0) |
		(key[KEY_RIGHT] ? 4 : 0) |
		(key[KEY_DOWN]  ? 8 : 0);

	if (old_ctrl_pressed != _ctrl_pressed) HandleCtrlChanged();
}

void VideoDriver_Allegro::MainLoop()
{
	this->StartGameThread();

	for (;;) {
		if (_exit_game) break;

		this->Tick();
		this->SleepTillNextTick();
	}

	this->StopGameThread();
}

bool VideoDriver_Allegro::ChangeResolution(int w, int h)
{
	return CreateMainSurface(w, h);
}

bool VideoDriver_Allegro::ToggleFullscreen(bool fullscreen)
{
	_fullscreen = fullscreen;
	GetVideoModes(); // get the list of available video modes
	if (_resolutions.empty() || !this->ChangeResolution(_cur_resolution.width, _cur_resolution.height)) {
		/* switching resolution failed, put back full_screen to original status */
		_fullscreen ^= true;
		return false;
	}
	return true;
}

bool VideoDriver_Allegro::AfterBlitterChange()
{
	return CreateMainSurface(_screen.width, _screen.height);
}

#endif /* WITH_ALLEGRO */
