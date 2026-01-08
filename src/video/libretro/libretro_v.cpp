/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file libretro_v.cpp Libretro video driver implementation. */

#ifdef WITH_LIBRETRO

#include "../../stdafx.h"
#include "libretro_v.h"
#include "libretro_core.h"
#include "libretro.h"

#include "../../debug.h"
#include "../../gfx_func.h"
#include "../../gfx_type.h"
#include "../../blitter/factory.hpp"
#include "../../core/math_func.hpp"
#include "../../framerate_type.h"
#include "../../window_func.h"
#include "../../window_gui.h"
#include "../../progress.h"
#include "../../thread.h"
#include "../../map_func.h"
#include "../../hotkeys.h"

#include "../../safeguards.h"

/* Global driver instance for libretro callbacks */
static VideoDriver_Libretro *_libretro_video_driver = nullptr;

/* Static instance getters */
VideoDriver_Libretro *VideoDriver_Libretro::GetInstance()
{
	return _libretro_video_driver;
}

std::optional<std::string_view> VideoDriver_Libretro::Start(const StringList &param)
{
	Debug(driver, 1, "[libretro_v] Start: Initializing libretro video driver");

	_libretro_video_driver = this;

	/* Get resolution from libretro core */
	unsigned w, h, p;
	LibretroCore::GetVideoInfo(&w, &h, &p);

	this->screen_width = w > 0 ? w : 1280;
	this->screen_height = h > 0 ? h : 720;

	Debug(driver, 1, "[libretro_v] Start: Resolution {}x{}", this->screen_width, this->screen_height);

	/* Set up OpenTTD screen */
	_cur_resolution.width = this->screen_width;
	_cur_resolution.height = this->screen_height;

	/* Initialize _resolutions list for Options window (required for resolution dropdown) */
	extern std::vector<Dimension> _resolutions;
	_resolutions.clear();
	auto add_res = [&](int rw, int rh) {
		if (rw <= 0 || rh <= 0) return;
		for (const auto &d : _resolutions) {
			if ((int)d.width == rw && (int)d.height == rh) return;
		}
		_resolutions.emplace_back(rw, rh);
	};

	add_res(this->screen_width, this->screen_height);
	add_res(640, 480);
	add_res(800, 600);
	add_res(1024, 768);
	add_res(1280, 720);
	add_res(1920, 1080);

	Debug(driver, 1, "[libretro_v] Start: Initialized _resolutions with {} entries", _resolutions.size());

	/* Allocate screen buffer */
	this->AllocateBackingStore(this->screen_width, this->screen_height);

	/* Initialize dirty blocks buffer - this is critical for SetDirty to work */
	ScreenSizeChanged();

	/* Mark the whole screen as dirty initially */
	this->MakeDirty(0, 0, this->screen_width, this->screen_height);

	/* We handle the game loop ourselves */
	this->is_game_threaded = false;

	Debug(driver, 1, "[libretro_v] Start: Video driver initialized successfully");

	return std::nullopt;
}

void VideoDriver_Libretro::Stop()
{
	Debug(driver, 1, "[libretro_v] Stop: Shutting down libretro video driver");

	this->FreeBackingStore();
	_libretro_video_driver = nullptr;

	Debug(driver, 1, "[libretro_v] Stop: Video driver shutdown complete");
}

void VideoDriver_Libretro::MakeDirty(int left, int top, int width, int height)
{
	/* Expand dirty rect to cover the new dirty area */
	if (this->dirty_rect.left == 0 && this->dirty_rect.right == 0) {
		this->dirty_rect.left = left;
		this->dirty_rect.top = top;
		this->dirty_rect.right = left + width;
		this->dirty_rect.bottom = top + height;
	} else {
		this->dirty_rect.left = std::min(this->dirty_rect.left, left);
		this->dirty_rect.top = std::min(this->dirty_rect.top, top);
		this->dirty_rect.right = std::max(this->dirty_rect.right, left + width);
		this->dirty_rect.bottom = std::max(this->dirty_rect.bottom, top + height);
	}
}

void VideoDriver_Libretro::MainLoop()
{
	/* In libretro, we don't run our own loop - retro_run calls us */
	Debug(driver, 1, "[libretro_v] MainLoop: Called but we use external loop");
}

bool VideoDriver_Libretro::ChangeResolution(int w, int h)
{
	Debug(driver, 1, "[libretro_v] ChangeResolution: {}x{}", w, h);
	if (!LibretroCore::SetVideoGeometry((unsigned)w, (unsigned)h)) return false;

	this->screen_width = w;
	this->screen_height = h;

	_cur_resolution.width = w;
	_cur_resolution.height = h;

	this->AllocateBackingStore(w, h);
	this->MakeDirty(0, 0, w, h);
	GameSizeChanged();

	return true;
}

bool VideoDriver_Libretro::ToggleFullscreen(bool fullscreen)
{
	/* Libretro handles fullscreen */
	return false;
}

std::vector<int> VideoDriver_Libretro::GetListOfMonitorRefreshRates()
{
	/* Libretro cores run at a fixed frame rate, typically 60 Hz.
	 * Return an empty vector since we don't have monitor refresh rates. */
	return {};
}

bool VideoDriver_Libretro::AllocateBackingStore(int w, int h)
{
	Debug(driver, 1, "[libretro_v] AllocateBackingStore: {}x{}", w, h);

	/* Free any existing buffer */
	this->FreeBackingStore();

	this->screen_width = w;
	this->screen_height = h;

	/* Allocate the blitter's screen buffer */
	_screen.width = w;
	_screen.height = h;
	_screen.pitch = w;
	_screen.dst_ptr = new uint8_t[static_cast<size_t>(w) * h * 4]();

	/* Set up the RGBA buffer for libretro */
	this->video_buffer_size = static_cast<size_t>(w) * h * sizeof(uint32_t);
	this->video_buffer = new uint32_t[static_cast<size_t>(w) * h]();

	Debug(driver, 1, "[libretro_v] AllocateBackingStore: Allocated {} bytes", this->video_buffer_size);

	Blitter *blitter = BlitterFactory::GetCurrentBlitter();
	if (blitter != nullptr) {
		Debug(driver, 1, "[libretro_v] AllocateBackingStore: Calling blitter->PostResize (blitter={})", blitter->GetName());
		blitter->PostResize();
		Debug(driver, 1, "[libretro_v] AllocateBackingStore: blitter->PostResize returned");
	}

	return true;
}

void VideoDriver_Libretro::FreeBackingStore()
{
	if (_screen.dst_ptr) {
		delete[] static_cast<uint8_t*>(_screen.dst_ptr);
		_screen.dst_ptr = nullptr;
	}

	if (this->video_buffer) {
		delete[] this->video_buffer;
		this->video_buffer = nullptr;
	}

	this->video_buffer_size = 0;
}

void VideoDriver_Libretro::Paint()
{
	PerformanceMeasurer framerate(PFE_VIDEO);

	/* Check if we have anything dirty */
	if (this->dirty_rect.left >= this->dirty_rect.right) return;

	Blitter *blitter = BlitterFactory::GetCurrentBlitter();
	if (!blitter || !_screen.dst_ptr || !this->video_buffer) return;

	/* Copy the blitter's output to our RGBA buffer */
	/* The blitter writes to _screen.dst_ptr, we need to convert to RGBA */
	uint8_t *src = static_cast<uint8_t*>(_screen.dst_ptr);
	uint32_t *dst = this->video_buffer;

	/* For 32bpp blitters, we can do a direct copy with byte reordering */
	if (blitter->GetScreenDepth() == 32) {
		size_t pixels = static_cast<size_t>(this->screen_width) * this->screen_height;
		for (size_t i = 0; i < pixels; i++) {
			/* OpenTTD uses BGRA, libretro expects XRGB8888 */
			uint8_t b = src[i * 4 + 0];
			uint8_t g = src[i * 4 + 1];
			uint8_t r = src[i * 4 + 2];
			dst[i] = (r << 16) | (g << 8) | b;
		}
	} else if (blitter->GetScreenDepth() == 8) {
		/* 8bpp needs palette conversion */
		size_t pixels = static_cast<size_t>(this->screen_width) * this->screen_height;
		for (size_t i = 0; i < pixels; i++) {
			uint8_t idx = src[i];
			/* Use the current palette */
			uint32_t color =
				(_cur_palette.palette[idx].r << 16) |
				(_cur_palette.palette[idx].g << 8) |
				_cur_palette.palette[idx].b;
			dst[i] = color;
		}
	}

	/* Clear dirty rect */
	this->dirty_rect = {};
}

void VideoDriver_Libretro::CheckPaletteAnim()
{
	if (_cur_palette.count_dirty != 0) {
		Blitter *blitter = BlitterFactory::GetCurrentBlitter();

		switch (blitter->UsePaletteAnimation()) {
			case Blitter::PaletteAnimation::VideoBackend:
				/* We handle palette in Paint() */
				break;
			case Blitter::PaletteAnimation::Blitter:
				blitter->PaletteAnimate(_cur_palette);
				break;
			case Blitter::PaletteAnimation::None:
				break;
		}

		_cur_palette.count_dirty = 0;
	}
}

bool VideoDriver_Libretro::PollEvent()
{
	/* Libretro input is already polled by the frontend. Inject it into OpenTTD once per tick. */
	this->ProcessLibretroInput();
	return false;
}

void VideoDriver_Libretro::InputLoop()
{
	/* Handle fast forward key etc */
	VideoDriver::InputLoop();
}

/* Convert libretro keycode to OpenTTD keycode */
static uint ConvertRetroKeyToOTTD(unsigned retro_key, uint16_t modifiers)
{
	uint key = 0;

	/* Map special keys */
	switch (retro_key) {
		case RETROK_ESCAPE:    key = WKC_ESC; break;
		case RETROK_BACKSPACE: key = WKC_BACKSPACE; break;
		case RETROK_INSERT:    key = WKC_INSERT; break;
		case RETROK_DELETE:    key = WKC_DELETE; break;
		case RETROK_PAGEUP:    key = WKC_PAGEUP; break;
		case RETROK_PAGEDOWN:  key = WKC_PAGEDOWN; break;
		case RETROK_END:       key = WKC_END; break;
		case RETROK_HOME:      key = WKC_HOME; break;
		case RETROK_LEFT:      key = WKC_LEFT; break;
		case RETROK_UP:        key = WKC_UP; break;
		case RETROK_RIGHT:     key = WKC_RIGHT; break;
		case RETROK_DOWN:      key = WKC_DOWN; break;
		case RETROK_RETURN:    key = WKC_RETURN; break;
		case RETROK_KP_ENTER:  key = WKC_NUM_ENTER; break;
		case RETROK_TAB:       key = WKC_TAB; break;
		case RETROK_SPACE:     key = WKC_SPACE; break;
		case RETROK_F1:        key = WKC_F1; break;
		case RETROK_F2:        key = WKC_F2; break;
		case RETROK_F3:        key = WKC_F3; break;
		case RETROK_F4:        key = WKC_F4; break;
		case RETROK_F5:        key = WKC_F5; break;
		case RETROK_F6:        key = WKC_F6; break;
		case RETROK_F7:        key = WKC_F7; break;
		case RETROK_F8:        key = WKC_F8; break;
		case RETROK_F9:        key = WKC_F9; break;
		case RETROK_F10:       key = WKC_F10; break;
		case RETROK_F11:       key = WKC_F11; break;
		case RETROK_F12:       key = WKC_F12; break;
		case RETROK_PAUSE:     key = WKC_PAUSE; break;
		case RETROK_BACKQUOTE: key = WKC_BACKQUOTE; break;
		case RETROK_KP_DIVIDE:   key = WKC_NUM_DIV; break;
		case RETROK_KP_MULTIPLY: key = WKC_NUM_MUL; break;
		case RETROK_KP_MINUS:    key = WKC_NUM_MINUS; break;
		case RETROK_KP_PLUS:     key = WKC_NUM_PLUS; break;
		case RETROK_KP_PERIOD:   key = WKC_NUM_DECIMAL; break;
		case RETROK_SLASH:       key = WKC_SLASH; break;
		case RETROK_SEMICOLON:   key = WKC_SEMICOLON; break;
		case RETROK_EQUALS:      key = WKC_EQUALS; break;
		case RETROK_LEFTBRACKET: key = WKC_L_BRACKET; break;
		case RETROK_BACKSLASH:   key = WKC_BACKSLASH; break;
		case RETROK_RIGHTBRACKET:key = WKC_R_BRACKET; break;
		case RETROK_QUOTE:       key = WKC_SINGLEQUOTE; break;
		case RETROK_COMMA:       key = WKC_COMMA; break;
		case RETROK_PERIOD:      key = WKC_PERIOD; break;
		case RETROK_MINUS:       key = WKC_MINUS; break;
		default:
			/* Letters and numbers map directly */
			if (retro_key >= RETROK_a && retro_key <= RETROK_z) {
				key = 'A' + (retro_key - RETROK_a);
			} else if (retro_key >= RETROK_0 && retro_key <= RETROK_9) {
				key = '0' + (retro_key - RETROK_0);
			} else if (retro_key >= RETROK_KP0 && retro_key <= RETROK_KP9) {
				key = '0' + (retro_key - RETROK_KP0);
			}
			break;
	}

	/* Add modifiers */
	if (modifiers & RETROKMOD_SHIFT) key |= WKC_SHIFT;
	if (modifiers & RETROKMOD_CTRL)  key |= WKC_CTRL;
	if (modifiers & RETROKMOD_ALT)   key |= WKC_ALT;
	if (modifiers & RETROKMOD_META)  key |= WKC_META;

	return key;
}

void VideoDriver_Libretro::ProcessLibretroInput()
{
	/* Get mouse state from libretro */
	int mx, my, wheel;
	bool ml, mr, mm;
	LibretroCore::GetMouseState(&mx, &my, &ml, &mr, &mm, &wheel);

	/* Update cursor position.
	 * When OpenTTD enables "fixed" cursor mode (e.g. RMB scrolling), the cursor position
	 * must remain fixed, but the relative delta still needs to be updated for viewport scrolling.
	 */
	static int last_x = 0;
	static int last_y = 0;
	static bool last_valid = false;

	int clamped_x = Clamp(mx, 0, this->screen_width - 1);
	int clamped_y = Clamp(my, 0, this->screen_height - 1);

	if (!last_valid) {
		last_x = clamped_x;
		last_y = clamped_y;
		last_valid = true;
	}

	int dx = clamped_x - last_x;
	int dy = clamped_y - last_y;
	last_x = clamped_x;
	last_y = clamped_y;

	if (_cursor.fix_at) {
		if (dx != 0 || dy != 0) _cursor.UpdateCursorPositionRelative(dx, dy);
	} else {
		_cursor.UpdateCursorPosition(clamped_x, clamped_y);
	}
	_cursor.in_window = true;

	/* Handle mouse wheel */
	if (wheel != 0) {
		_cursor.wheel += wheel;
	}

	/* Handle mouse button events - must call HandleMouseEvents() immediately on state change
	 * like Win32/SDL drivers do, otherwise quick clicks within a single frame are missed. */
	static bool prev_ml = false, prev_mr = false;

	if (ml != prev_ml) {
		if (ml) {
			/* Left button pressed */
			_left_button_down = true;
			_left_button_clicked = false;
		} else {
			/* Left button released */
			_left_button_down = false;
			_left_button_clicked = false;
		}
		prev_ml = ml;
		HandleMouseEvents();
	}

	if (mr != prev_mr) {
		if (mr) {
			/* Right button pressed */
			_right_button_down = true;
			_right_button_clicked = true;
		} else {
			/* Right button released */
			_right_button_down = false;
		}
		prev_mr = mr;
		HandleMouseEvents();
	}

	/* Process keyboard events from libretro */
	bool key_down;
	unsigned keycode;
	uint32_t character;
	uint16_t modifiers;

	while (LibretroCore::GetNextKeyEvent(&key_down, &keycode, &character, &modifiers)) {
		if (key_down) {
			/* Handle arrow keys for _dirkeys */
			switch (keycode) {
				case RETROK_UP:    SB(_dirkeys, 1, 1, 1); break;
				case RETROK_DOWN:  SB(_dirkeys, 3, 1, 1); break;
				case RETROK_LEFT:  SB(_dirkeys, 0, 1, 1); break;
				case RETROK_RIGHT: SB(_dirkeys, 2, 1, 1); break;
				default: break;
			}

			/* Convert and send keypress to OpenTTD */
			uint ottd_key = ConvertRetroKeyToOTTD(keycode, modifiers);
			char32_t unicode_char = (character != 0) ? character : 0;
			HandleKeypress(ottd_key, unicode_char);
		} else {
			/* Key released */
			switch (keycode) {
				case RETROK_UP:    SB(_dirkeys, 1, 1, 0); break;
				case RETROK_DOWN:  SB(_dirkeys, 3, 1, 0); break;
				case RETROK_LEFT:  SB(_dirkeys, 0, 1, 0); break;
				case RETROK_RIGHT: SB(_dirkeys, 2, 1, 0); break;
				default: break;
			}
		}
	}

	/* Mouse events are handled by ::InputLoop() inside VideoDriver::Tick(). */
}

void VideoDriver_Libretro::RunFrame()
{
	this->Tick();
}

uint32_t *VideoDriver_Libretro::GetVideoBuffer()
{
	return this->video_buffer;
}

void VideoDriver_Libretro::GetVideoSize(unsigned *w, unsigned *h, unsigned *pitch)
{
	if (w) *w = this->screen_width;
	if (h) *h = this->screen_height;
	if (pitch) *pitch = this->screen_width * sizeof(uint32_t);
}

#endif /* WITH_LIBRETRO */
