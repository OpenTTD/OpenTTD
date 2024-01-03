/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file timer_game_tick.h Definition of the tick-based game-timer */

#ifndef TIMER_GAME_TICK_H
#define TIMER_GAME_TICK_H

#include "../gfx_type.h"

#include <chrono>

/**
 * Timer that represents the game-ticks. It will pause when the game is paused.
 *
 * @note Callbacks are executed in the game-thread.
 */
class TimerGameTick {
public:
	using Ticks = int32_t; ///< The type to store ticks in
	using TickCounter = uint64_t; ///< The type that the tick counter is stored in

	using TPeriod = uint;
	using TElapsed = uint;
	struct TStorage {
		uint elapsed;
	};

	static TickCounter counter; ///< Monotonic counter, in ticks, since start of game.
};

/**
 * Storage class for Ticks constants.
 */
class Ticks {
public:
	static constexpr TimerGameTick::Ticks INVALID_TICKS = -1; ///< Representation of an invalid number of ticks.

	/**
	 * 1 day is 74 ticks; TimerGameCalendar::date_fract used to be uint16_t and incremented by 885. On an overflow the new day begun and 65535 / 885 = 74.
	 * 1 tick is approximately 27 ms.
	 * 1 day is thus about 2 seconds (74 * 27 = 1998) on a machine that can run OpenTTD normally
	 */
	static constexpr TimerGameTick::Ticks DAY_TICKS = 74; ///< ticks per day
	static constexpr TimerGameTick::Ticks TICKS_PER_SECOND = 1000 / MILLISECONDS_PER_TICK; ///< Estimation of how many ticks fit in a single second.

	static constexpr TimerGameTick::Ticks STATION_RATING_TICKS = 185; ///< Cycle duration for updating station rating.
	static constexpr TimerGameTick::Ticks STATION_ACCEPTANCE_TICKS = 250; ///< Cycle duration for updating station acceptance.
	static constexpr TimerGameTick::Ticks STATION_LINKGRAPH_TICKS = 504; ///< Cycle duration for cleaning dead links.
	static constexpr TimerGameTick::Ticks CARGO_AGING_TICKS = 185; ///< Cycle duration for aging cargo.
	static constexpr TimerGameTick::Ticks INDUSTRY_PRODUCE_TICKS = 256; ///< Cycle duration for industry production.
	static constexpr TimerGameTick::Ticks TOWN_GROWTH_TICKS = 70;  ///< Cycle duration for towns trying to grow (this originates from the size of the town array in TTD).
	static constexpr TimerGameTick::Ticks INDUSTRY_CUT_TREE_TICKS = INDUSTRY_PRODUCE_TICKS * 2; ///< Cycle duration for lumber mill's extra action.
};

#endif /* TIMER_GAME_TICK_H */
