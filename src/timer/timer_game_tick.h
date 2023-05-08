/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file timer_game_tick.h Definition of the tick-based game-timer */

#ifndef TIMER_GAME_TICK_H
#define TIMER_GAME_TICK_H

#include "gfx_type.h"

#include <chrono>

/** Estimation of how many ticks fit in a single second. */
static const uint TICKS_PER_SECOND = 1000 / MILLISECONDS_PER_TICK;

/**
 * Timer that represents the game-ticks. It will pause when the game is paused.
 *
 * @note Callbacks are executed in the game-thread.
 */
class TimerGameTick {
public:
	using TPeriod = uint;
	using TElapsed = uint;
	struct TStorage {
		uint elapsed;
	};

	static uint64_t counter; ///< Monotonic counter, in ticks, since start of game.
};

#endif /* TIMER_GAME_TICK_H */
