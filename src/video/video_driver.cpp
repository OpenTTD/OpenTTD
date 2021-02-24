/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file video_driver.cpp Common code between video driver implementations. */

#include "../stdafx.h"
#include "../debug.h"
#include "../core/random_func.hpp"
#include "../network/network.h"
#include "../gfx_func.h"
#include "../progress.h"
#include "../thread.h"
#include "../window_func.h"
#include "video_driver.hpp"

bool _video_hw_accel; ///< Whether to consider hardware accelerated video drivers.

void VideoDriver::Tick()
{
	auto cur_ticks = std::chrono::steady_clock::now();

	if (cur_ticks >= this->next_game_tick) {
		this->next_game_tick += this->GetGameInterval();
		/* Avoid next_game_tick getting behind more and more if it cannot keep up. */
		if (this->next_game_tick < cur_ticks - ALLOWED_DRIFT * this->GetGameInterval()) this->next_game_tick = cur_ticks;

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
	if (this->HasGUI() && cur_ticks >= this->next_draw_tick && (_switch_mode == SM_NONE || _game_mode == GM_BOOTSTRAP || HasModalProgress())) {
		this->next_draw_tick += this->GetDrawInterval();
		/* Avoid next_draw_tick getting behind more and more if it cannot keep up. */
		if (this->next_draw_tick < cur_ticks - ALLOWED_DRIFT * this->GetDrawInterval()) this->next_draw_tick = cur_ticks;

		/* Keep the interactive randomizer a bit more random by requesting
		 * new values when-ever we can. */
		InteractiveRandom();

		while (this->PollEvent()) {}
		this->InputLoop();

		/* Check if the fast-forward button is still pressed. */
		if (fast_forward_key_pressed && !_networking && _game_mode != GM_MENU) {
			ChangeGameSpeed(true);
			this->fast_forward_via_key = true;
		} else if (this->fast_forward_via_key) {
			ChangeGameSpeed(false);
			this->fast_forward_via_key = false;
		}

		::InputLoop();
		UpdateWindows();

		this->CheckPaletteAnim();
		this->Paint();
	}
}

void VideoDriver::SleepTillNextTick()
{
	/* See how much time there is till we have to process the next event, and try to hit that as close as possible. */
	auto next_tick = std::min(this->next_draw_tick, this->next_game_tick);
	auto now = std::chrono::steady_clock::now();

	if (next_tick > now) {
		this->UnlockVideoBuffer();
		std::this_thread::sleep_for(next_tick - now);
		this->LockVideoBuffer();
	}
}
