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
#include "../../settings_type.h"
#include "../../core/geometry_type.hpp"
#include "cocoa_v.h"
#include "cocoa_wnd.h"
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

extern bool _tab_is_down;

#ifdef _DEBUG
static uint32 _tEvent;
#endif


static uint32 GetTick()
{
	struct timeval tim;

	gettimeofday(&tim, NULL);
	return tim.tv_usec / 1000 + tim.tv_sec * 1000;
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

	[ NSApp sendEvent:event ];

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

			NSUInteger cur_mods = [ NSEvent modifierFlags ];

#if defined(_DEBUG)
			if (cur_mods & NSShiftKeyMask)
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

				_ctrl_pressed = !!(cur_mods & ( _settings_client.gui.right_mouse_btn_emulation != RMBE_CONTROL ? NSControlKeyMask : NSCommandKeyMask));
				_shift_pressed = !!(cur_mods & NSShiftKeyMask);

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
