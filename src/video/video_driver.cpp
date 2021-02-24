/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file video_driver.cpp Common code between video driver implementations. */

#include "../stdafx.h"
#include "../debug.h"
#include "../gfx_func.h"
#include "../progress.h"
#include "../thread.h"
#include "../window_func.h"
#include "video_driver.hpp"

bool VideoDriver::Tick()
{
	auto cur_ticks = std::chrono::steady_clock::now();

	/* If more than a millisecond has passed, increase the _realtime_tick. */
	if (cur_ticks - this->last_realtime_tick > std::chrono::milliseconds(1)) {
		auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(cur_ticks - this->last_realtime_tick);
		_realtime_tick += delta.count();
		this->last_realtime_tick += delta;
	}

	if (cur_ticks >= this->next_game_tick || (_fast_forward && !_pause_mode)) {
		if (_fast_forward && !_pause_mode) {
			this->next_game_tick = cur_ticks + this->GetGameInterval();
		} else {
			this->next_game_tick += this->GetGameInterval();
			/* Avoid next_game_tick getting behind more and more if it cannot keep up. */
			if (this->next_game_tick < cur_ticks - ALLOWED_DRIFT * this->GetGameInterval()) this->next_game_tick = cur_ticks;
		}

		/* The game loop is the part that can run asynchronously.
		 * The rest except sleeping can't. */
		this->UnlockVideoBuffer();
		::GameLoop();
		this->LockVideoBuffer();

		/* For things like dedicated server, don't run a separate draw-tick. */
		if (!this->HasGUI()) {
			::InputLoop();
			UpdateWindows();
			this->next_draw_tick = this->next_game_tick;
		}
	}

	/* Prevent drawing when switching mode, as windows can be removed when they should still appear. */
	if (this->HasGUI() && cur_ticks >= this->next_draw_tick && (_switch_mode == SM_NONE || HasModalProgress())) {
		this->next_draw_tick += this->GetDrawInterval();
		/* Avoid next_draw_tick getting behind more and more if it cannot keep up. */
		if (this->next_draw_tick < cur_ticks - ALLOWED_DRIFT * this->GetDrawInterval()) this->next_draw_tick = cur_ticks;

		while (this->PollEvent()) {}
		this->InputLoop();
		::InputLoop();
		UpdateWindows();
		this->CheckPaletteAnim();

		return true;
	}

	return false;
}

void VideoDriver::SleepTillNextTick()
{
	/* If we are not in fast-forward, create some time between calls to ease up CPU usage. */
	if (!_fast_forward || _pause_mode) {
		/* See how much time there is till we have to process the next event, and try to hit that as close as possible. */
		auto next_tick = std::min(this->next_draw_tick, this->next_game_tick);
		auto now = std::chrono::steady_clock::now();

		if (next_tick > now) {
			this->UnlockVideoBuffer();
			std::this_thread::sleep_for(next_tick - now);
			this->LockVideoBuffer();
		}
	}
}
