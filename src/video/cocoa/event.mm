/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/******************************************************************************
 *                             Cocoa video driver                             *
 * Known things left to do:                                                   *
 *  Nothing at the moment.                                                    *
 ******************************************************************************/

#ifdef WITH_COCOA

#include "../../stdafx.h"

#define Rect  OTTDRect
#define Point OTTDPoint
#import <Cocoa/Cocoa.h>
#undef Rect
#undef Point

#include "../../openttd.h"
#include "../../debug.h"
#include "../../os/macosx/splash.h"
#include "../../settings_type.h"
#include "../../core/geometry_type.hpp"
#include "cocoa_v.h"
#include "cocoa_wnd.h"
#include "cocoa_keys.h"
#include "../../blitter/factory.hpp"
#include "../../gfx_func.h"
#include "../../network/network.h"
#include "../../core/random_func.hpp"
#include "../../core/math_func.hpp"
#include "../../texteff.hpp"
#include "../../window_func.h"
#include "../../thread.h"

#import <sys/time.h> /* gettimeofday */

/**
 * Important notice regarding all modifications!!!!!!!
 * There are certain limitations because the file is objective C++.
 * gdb has limitations.
 * C++ and objective C code can't be joined in all cases (classes stuff).
 * Read http://developer.apple.com/releasenotes/Cocoa/Objective-C++.html for more information.
 */


static unsigned int _current_mods;
static bool _tab_is_down;
#ifdef _DEBUG
static uint32 _tEvent;
#endif


static uint32 GetTick()
{
	struct timeval tim;

	gettimeofday(&tim, NULL);
	return tim.tv_usec / 1000 + tim.tv_sec * 1000;
}

struct VkMapping {
	unsigned short vk_from;
	byte map_to;
};

#define AS(x, z) {x, z}

static const VkMapping _vk_mapping[] = {
	AS(QZ_BACKQUOTE,  WKC_BACKQUOTE), // key left of '1'
	AS(QZ_BACKQUOTE2, WKC_BACKQUOTE), // some keyboards have it on another scancode

	/* Pageup stuff + up/down */
	AS(QZ_PAGEUP,   WKC_PAGEUP),
	AS(QZ_PAGEDOWN, WKC_PAGEDOWN),

	AS(QZ_UP,    WKC_UP),
	AS(QZ_DOWN,  WKC_DOWN),
	AS(QZ_LEFT,  WKC_LEFT),
	AS(QZ_RIGHT, WKC_RIGHT),

	AS(QZ_HOME, WKC_HOME),
	AS(QZ_END,  WKC_END),

	AS(QZ_INSERT, WKC_INSERT),
	AS(QZ_DELETE, WKC_DELETE),

	/* Letters. QZ_[a-z] is not in numerical order so we can't use AM(...) */
	AS(QZ_a, 'A'),
	AS(QZ_b, 'B'),
	AS(QZ_c, 'C'),
	AS(QZ_d, 'D'),
	AS(QZ_e, 'E'),
	AS(QZ_f, 'F'),
	AS(QZ_g, 'G'),
	AS(QZ_h, 'H'),
	AS(QZ_i, 'I'),
	AS(QZ_j, 'J'),
	AS(QZ_k, 'K'),
	AS(QZ_l, 'L'),
	AS(QZ_m, 'M'),
	AS(QZ_n, 'N'),
	AS(QZ_o, 'O'),
	AS(QZ_p, 'P'),
	AS(QZ_q, 'Q'),
	AS(QZ_r, 'R'),
	AS(QZ_s, 'S'),
	AS(QZ_t, 'T'),
	AS(QZ_u, 'U'),
	AS(QZ_v, 'V'),
	AS(QZ_w, 'W'),
	AS(QZ_x, 'X'),
	AS(QZ_y, 'Y'),
	AS(QZ_z, 'Z'),
	/* Same thing for digits */
	AS(QZ_0, '0'),
	AS(QZ_1, '1'),
	AS(QZ_2, '2'),
	AS(QZ_3, '3'),
	AS(QZ_4, '4'),
	AS(QZ_5, '5'),
	AS(QZ_6, '6'),
	AS(QZ_7, '7'),
	AS(QZ_8, '8'),
	AS(QZ_9, '9'),

	AS(QZ_ESCAPE,    WKC_ESC),
	AS(QZ_PAUSE,     WKC_PAUSE),
	AS(QZ_BACKSPACE, WKC_BACKSPACE),

	AS(QZ_SPACE,  WKC_SPACE),
	AS(QZ_RETURN, WKC_RETURN),
	AS(QZ_TAB,    WKC_TAB),

	/* Function keys */
	AS(QZ_F1,  WKC_F1),
	AS(QZ_F2,  WKC_F2),
	AS(QZ_F3,  WKC_F3),
	AS(QZ_F4,  WKC_F4),
	AS(QZ_F5,  WKC_F5),
	AS(QZ_F6,  WKC_F6),
	AS(QZ_F7,  WKC_F7),
	AS(QZ_F8,  WKC_F8),
	AS(QZ_F9,  WKC_F9),
	AS(QZ_F10, WKC_F10),
	AS(QZ_F11, WKC_F11),
	AS(QZ_F12, WKC_F12),

	/* Numeric part */
	AS(QZ_KP0,         '0'),
	AS(QZ_KP1,         '1'),
	AS(QZ_KP2,         '2'),
	AS(QZ_KP3,         '3'),
	AS(QZ_KP4,         '4'),
	AS(QZ_KP5,         '5'),
	AS(QZ_KP6,         '6'),
	AS(QZ_KP7,         '7'),
	AS(QZ_KP8,         '8'),
	AS(QZ_KP9,         '9'),
	AS(QZ_KP_DIVIDE,   WKC_NUM_DIV),
	AS(QZ_KP_MULTIPLY, WKC_NUM_MUL),
	AS(QZ_KP_MINUS,    WKC_NUM_MINUS),
	AS(QZ_KP_PLUS,     WKC_NUM_PLUS),
	AS(QZ_KP_ENTER,    WKC_NUM_ENTER),
	AS(QZ_KP_PERIOD,   WKC_NUM_DECIMAL),

	/* Other non-letter keys */
	AS(QZ_SLASH,        WKC_SLASH),
	AS(QZ_SEMICOLON,    WKC_SEMICOLON),
	AS(QZ_EQUALS,       WKC_EQUALS),
	AS(QZ_LEFTBRACKET,  WKC_L_BRACKET),
	AS(QZ_BACKSLASH,    WKC_BACKSLASH),
	AS(QZ_RIGHTBRACKET, WKC_R_BRACKET),

	AS(QZ_QUOTE,   WKC_SINGLEQUOTE),
	AS(QZ_COMMA,   WKC_COMMA),
	AS(QZ_MINUS,   WKC_MINUS),
	AS(QZ_PERIOD,  WKC_PERIOD)
};


static uint32 QZ_MapKey(unsigned short sym)
{
	uint32 key = 0;

	for (const VkMapping *map = _vk_mapping; map != endof(_vk_mapping); ++map) {
		if (sym == map->vk_from) {
			key = map->map_to;
			break;
		}
	}

	if (_current_mods & NSShiftKeyMask)     key |= WKC_SHIFT;
	if (_current_mods & NSControlKeyMask)   key |= (_settings_client.gui.right_mouse_btn_emulation != RMBE_CONTROL ? WKC_CTRL : WKC_META);
	if (_current_mods & NSAlternateKeyMask) key |= WKC_ALT;
	if (_current_mods & NSCommandKeyMask)   key |= (_settings_client.gui.right_mouse_btn_emulation != RMBE_CONTROL ? WKC_META : WKC_CTRL);

	return key;
}

static bool QZ_KeyEvent(unsigned short keycode, unsigned short unicode, BOOL down)
{
	bool interpret_keys = true;

	switch (keycode) {
		case QZ_UP:    SB(_dirkeys, 1, 1, down); break;
		case QZ_DOWN:  SB(_dirkeys, 3, 1, down); break;
		case QZ_LEFT:  SB(_dirkeys, 0, 1, down); break;
		case QZ_RIGHT: SB(_dirkeys, 2, 1, down); break;

		case QZ_TAB: _tab_is_down = down; break;

		case QZ_RETURN:
		case QZ_f:
			if (down && (_current_mods & NSCommandKeyMask)) {
				VideoDriver::GetInstance()->ToggleFullscreen(!_fullscreen);
			}
			break;

		case QZ_v:
			if (down && EditBoxInGlobalFocus() && (_current_mods & (NSCommandKeyMask | NSControlKeyMask))) {
				HandleKeypress(WKC_CTRL | 'V', unicode);
			}
			break;
		case QZ_u:
			if (down && EditBoxInGlobalFocus() && (_current_mods & (NSCommandKeyMask | NSControlKeyMask))) {
				HandleKeypress(WKC_CTRL | 'U', unicode);
			}
			break;
	}

	if (down) {
		uint32 pressed_key = QZ_MapKey(keycode);

		static bool console = false;

		/* The second backquote may have a character, which we don't want to interpret. */
		if (pressed_key == WKC_BACKQUOTE && (console || unicode == 0)) {
			if (!console) {
				/* Backquote is a dead key, require a double press for hotkey behaviour (i.e. console). */
				console = true;
				return true;
			} else {
				/* Second backquote, don't interpret as text input. */
				interpret_keys = false;
			}
		}
		console = false;

		/* Don't handle normal characters if an edit box has the focus. */
		if (!EditBoxInGlobalFocus() || IsInsideMM(pressed_key & ~WKC_SPECIAL_KEYS, WKC_F1, WKC_PAUSE + 1)) {
			HandleKeypress(pressed_key, unicode);
		}
		DEBUG(driver, 2, "cocoa_v: QZ_KeyEvent: %x (%x), down, mapping: %x", keycode, unicode, pressed_key);
	} else {
		DEBUG(driver, 2, "cocoa_v: QZ_KeyEvent: %x (%x), up", keycode, unicode);
	}

	return interpret_keys;
}

static void QZ_DoUnsidedModifiers(unsigned int newMods)
{
	const int mapping[] = { QZ_CAPSLOCK, QZ_LSHIFT, QZ_LCTRL, QZ_LALT, QZ_LMETA };

	if (_current_mods == newMods) return;

	/* Iterate through the bits, testing each against the current modifiers */
	for (unsigned int i = 0, bit = NSAlphaShiftKeyMask; bit <= NSCommandKeyMask; bit <<= 1, ++i) {
		unsigned int currentMask, newMask;

		currentMask = _current_mods & bit;
		newMask     = newMods & bit;

		if (currentMask && currentMask != newMask) { // modifier up event
			/* If this was Caps Lock, we need some additional voodoo to make SDL happy (is this needed in ottd?) */
			if (bit == NSAlphaShiftKeyMask) QZ_KeyEvent(mapping[i], 0, YES);
			QZ_KeyEvent(mapping[i], 0, NO);
		} else if (newMask && currentMask != newMask) { // modifier down event
			QZ_KeyEvent(mapping[i], 0, YES);
			/* If this was Caps Lock, we need some additional voodoo to make SDL happy (is this needed in ottd?) */
			if (bit == NSAlphaShiftKeyMask) QZ_KeyEvent(mapping[i], 0, NO);
		}
	}

	_current_mods = newMods;
}




bool VideoDriver_Cocoa::PollEvent()
{
#ifdef _DEBUG
	uint32 et0 = GetTick();
#endif
	NSEvent *event = [ NSApp nextEventMatchingMask:NSAnyEventMask
				untilDate:[ NSDate distantPast ]
				inMode:NSDefaultRunLoopMode dequeue:YES ];
#ifdef _DEBUG
	_tEvent += GetTick() - et0;
#endif

	if (event == nil) return false;
	if (!this->active) {
		[ NSApp sendEvent:event ];
		return true;
	}

	QZ_DoUnsidedModifiers( [ event modifierFlags ] );

	NSString *chars;
	switch ([ event type ]) {
		case NSKeyDown: {
			/* Quit, hide and minimize */
			switch ([ event keyCode ]) {
				case QZ_q:
				case QZ_h:
				case QZ_m:
					if ([ event modifierFlags ] & NSCommandKeyMask) {
						[ NSApp sendEvent:event ];
					}
					break;
			}

			chars = [ event characters ];
			unsigned short unicode = [ chars length ] > 0 ? [ chars characterAtIndex:0 ] : 0;
			if (EditBoxInGlobalFocus()) {
				if (QZ_KeyEvent([ event keyCode ], unicode, YES)) {
					[ this->cocoaview interpretKeyEvents:[ NSArray arrayWithObject:event ] ];
				}
			} else {
				QZ_KeyEvent([ event keyCode ], unicode, YES);
				for (uint i = 1; i < [ chars length ]; i++) {
					QZ_KeyEvent(0, [ chars characterAtIndex:i ], YES);
				}
			}
			break;
		}

		case NSKeyUp:
			/* Quit, hide and minimize */
			switch ([ event keyCode ]) {
				case QZ_q:
				case QZ_h:
				case QZ_m:
					if ([ event modifierFlags ] & NSCommandKeyMask) {
						[ NSApp sendEvent:event ];
					}
					break;
			}

			chars = [ event characters ];
			QZ_KeyEvent([ event keyCode ], [ chars length ] ? [ chars characterAtIndex:0 ] : 0, NO);
			break;

		default:
			[ NSApp sendEvent:event ];
	}

	return true;
}


void VideoDriver_Cocoa::GameLoop()
{
	uint32 cur_ticks = GetTick();
	uint32 last_cur_ticks = cur_ticks;
	uint32 next_tick = cur_ticks + MILLISECONDS_PER_TICK;

#ifdef _DEBUG
	uint32 et0 = GetTick();
	uint32 st = 0;
#endif

	DisplaySplashImage();
	this->CheckPaletteAnim();
	this->Draw(true);
	CSleep(1);

	for (int i = 0; i < 2; i++) ::GameLoop();

	UpdateWindows();
	this->CheckPaletteAnim();
	this->Draw();
	CSleep(1);

	/* Set the proper OpenTTD palette which got spoilt by the splash
	 * image when using 8bpp blitter */
	GfxInitPalettes();
	this->CheckPaletteAnim();
	this->Draw(true);

	for (;;) {
		@autoreleasepool {

			uint32 prev_cur_ticks = cur_ticks; // to check for wrapping
			InteractiveRandom(); // randomness

			while (this->PollEvent()) {}

			if (_exit_game) {
				/* Restore saved resolution if in fullscreen mode. */
				if (this->IsFullscreen()) _cur_resolution = this->orig_res;
				break;
			}

#if defined(_DEBUG)
			if (_current_mods & NSShiftKeyMask)
#else
			if (_tab_is_down)
#endif
			{
				if (!_networking && _game_mode != GM_MENU) _fast_forward |= 2;
			} else if (_fast_forward & 2) {
				_fast_forward = 0;
			}

			cur_ticks = GetTick();
			if (cur_ticks >= next_tick || (_fast_forward && !_pause_mode) || cur_ticks < prev_cur_ticks) {
				_realtime_tick += cur_ticks - last_cur_ticks;
				last_cur_ticks = cur_ticks;
				next_tick = cur_ticks + MILLISECONDS_PER_TICK;

				bool old_ctrl_pressed = _ctrl_pressed;

				_ctrl_pressed = !!(_current_mods & ( _settings_client.gui.right_mouse_btn_emulation != RMBE_CONTROL ? NSControlKeyMask : NSCommandKeyMask));
				_shift_pressed = !!(_current_mods & NSShiftKeyMask);

				if (old_ctrl_pressed != _ctrl_pressed) HandleCtrlChanged();

				::GameLoop();

				UpdateWindows();
				this->CheckPaletteAnim();
				this->Draw();
			} else {
#ifdef _DEBUG
				uint32 st0 = GetTick();
#endif
				CSleep(1);
#ifdef _DEBUG
				st += GetTick() - st0;
#endif
				NetworkDrawChatMessage();
				DrawMouseCursor();
				this->Draw();
			}
		}
	}

#ifdef _DEBUG
	uint32 et = GetTick();

	DEBUG(driver, 1, "cocoa_v: nextEventMatchingMask took %i ms total", _tEvent);
	DEBUG(driver, 1, "cocoa_v: game loop took %i ms total (%i ms without sleep)", et - et0, et - et0 - st);
	DEBUG(driver, 1, "cocoa_v: (nextEventMatchingMask total)/(game loop total) is %f%%", (double)_tEvent / (double)(et - et0) * 100);
	DEBUG(driver, 1, "cocoa_v: (nextEventMatchingMask total)/(game loop without sleep total) is %f%%", (double)_tEvent / (double)(et - et0 - st) * 100);
#endif
}

#endif /* WITH_COCOA */
