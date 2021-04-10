/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file video_driver.cpp Common code between video driver implementations. */

#include "../stdafx.h"
#include "../core/random_func.hpp"
#include "../network/network.h"
#include "../blitter/factory.hpp"
#include "../debug.h"
#include "../fontcache.h"
#include "../gfx_func.h"
#include "../gfxinit.h"
#include "../progress.h"
#include "../thread.h"
#include "../window_func.h"
#include "video_driver.hpp"

bool _video_hw_accel; ///< Whether to consider hardware accelerated video drivers.
bool _video_vsync; ///< Whether we should use vsync (only if _video_hw_accel is enabled).

void VideoDriver::GameLoop()
{
	this->next_game_tick += this->GetGameInterval();

	/* Avoid next_game_tick getting behind more and more if it cannot keep up. */
	auto now = std::chrono::steady_clock::now();
	if (this->next_game_tick < now - ALLOWED_DRIFT * this->GetGameInterval()) this->next_game_tick = now;

	{
		std::lock_guard<std::mutex> lock(this->game_state_mutex);

		::GameLoop();
	}
}

void VideoDriver::GameThread()
{
	while (!_exit_game) {
		this->GameLoop();

		auto now = std::chrono::steady_clock::now();
		if (this->next_game_tick > now) {
			std::this_thread::sleep_for(this->next_game_tick - now);
		} else {
			/* Ensure we yield this thread if drawings wants to take a lock on
			 * the game state. This is mainly because most OSes have an
			 * optimization that if you unlock/lock a mutex in the same thread
			 * quickly, it will never context switch even if there is another
			 * thread waiting to take the lock on the same mutex. */
			std::lock_guard<std::mutex> lock(this->game_thread_wait_mutex);
		}
	}
}

/**
 * Pause the game-loop for a bit, releasing the game-state lock. This allows,
 * if the draw-tick requested this, the drawing to happen.
 */
void VideoDriver::GameLoopPause()
{
	/* If we are not called from the game-thread, ignore this request. */
	if (std::this_thread::get_id() != this->game_thread.get_id()) return;

	this->game_state_mutex.unlock();

	{
		/* See GameThread() for more details on this lock. */
		std::lock_guard<std::mutex> lock(this->game_thread_wait_mutex);
	}

	this->game_state_mutex.lock();
}

/* static */ void VideoDriver::GameThreadThunk(VideoDriver *drv)
{
	drv->GameThread();
}

void VideoDriver::StartGameThread()
{
	if (this->is_game_threaded) {
		this->is_game_threaded = StartNewThread(&this->game_thread, "ottd:game", &VideoDriver::GameThreadThunk, this);
	}

	DEBUG(driver, 1, "using %sthread for game-loop", this->is_game_threaded ? "" : "no ");
}

void VideoDriver::StopGameThread()
{
	if (!this->is_game_threaded) return;

	this->game_thread.join();
}

void VideoDriver::RealChangeBlitter(const char *repl_blitter)
{
	const char *cur_blitter = BlitterFactory::GetCurrentBlitter()->GetName();

	DEBUG(driver, 1, "Switching blitter from '%s' to '%s'... ", cur_blitter, repl_blitter);
	Blitter *new_blitter = BlitterFactory::SelectBlitter(repl_blitter);
	if (new_blitter == nullptr) NOT_REACHED();
	DEBUG(driver, 1, "Successfully switched to %s.", repl_blitter);

	if (!this->AfterBlitterChange()) {
		/* Failed to switch blitter, let's hope we can return to the old one. */
		if (BlitterFactory::SelectBlitter(cur_blitter) == nullptr || !this->AfterBlitterChange()) usererror("Failed to reinitialize video driver. Specify a fixed blitter in the config");
	}

	/* Clear caches that might have sprites for another blitter. */
	this->ClearSystemSprites();
	ClearFontCache();
	GfxClearSpriteCache();
	ReInitAllWindows();
}

void VideoDriver::Tick()
{
	if (!this->is_game_threaded && std::chrono::steady_clock::now() >= this->next_game_tick) {
		this->GameLoop();

		/* For things like dedicated server, don't run a separate draw-tick. */
		if (!this->HasGUI()) {
			::InputLoop();
			::UpdateWindows();
			this->next_draw_tick = this->next_game_tick;
		}
	}

	auto now = std::chrono::steady_clock::now();
	if (this->HasGUI() && now >= this->next_draw_tick) {
		this->next_draw_tick += this->GetDrawInterval();
		/* Avoid next_draw_tick getting behind more and more if it cannot keep up. */
		if (this->next_draw_tick < now - ALLOWED_DRIFT * this->GetDrawInterval()) this->next_draw_tick = now;

		/* Keep the interactive randomizer a bit more random by requesting
		 * new values when-ever we can. */
		InteractiveRandom();

		this->InputLoop();

		/* Check if the fast-forward button is still pressed. */
		if (fast_forward_key_pressed && !_networking && _game_mode != GM_MENU) {
			ChangeGameSpeed(true);
			this->fast_forward_via_key = true;
		} else if (this->fast_forward_via_key) {
			ChangeGameSpeed(false);
			this->fast_forward_via_key = false;
		}

		{
			/* Tell the game-thread to stop so we can have a go. */
			std::lock_guard<std::mutex> lock_wait(this->game_thread_wait_mutex);
			std::lock_guard<std::mutex> lock_state(this->game_state_mutex);

			this->LockVideoBuffer();

			if (this->change_blitter != nullptr) {
				this->RealChangeBlitter(this->change_blitter);
				this->change_blitter = nullptr;
			}

			while (this->PollEvent()) {}
			::InputLoop();

			/* Prevent drawing when switching mode, as windows can be removed when they should still appear. */
			if (_game_mode == GM_BOOTSTRAP || _switch_mode == SM_NONE || HasModalProgress()) {
				::UpdateWindows();
			}

			this->PopulateSystemSprites();
		}

		this->CheckPaletteAnim();
		this->Paint();

		this->UnlockVideoBuffer();
	}
}

void VideoDriver::SleepTillNextTick()
{
	auto next_tick = this->next_draw_tick;
	auto now = std::chrono::steady_clock::now();

	if (!this->is_game_threaded) {
		next_tick = min(next_tick, this->next_game_tick);
	}

	if (next_tick > now) {
		std::this_thread::sleep_for(next_tick - now);
	}
}
