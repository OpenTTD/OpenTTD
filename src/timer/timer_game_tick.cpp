/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file timer_game_tick.cpp
 * This file implements the timer logic for the tick-based game-timer.
 */

#include "../stdafx.h"
#include "timer.h"
#include "timer_game_tick.h"

#include "../safeguards.h"

TimerGameTick::TickCounter TimerGameTick::counter = 0;

template<>
void IntervalTimer<TimerGameTick>::Elapsed(TimerGameTick::TElapsed delta)
{
	if (this->period.value == 0) return;

	this->storage.elapsed += delta;

	uint count = 0;
	while (this->storage.elapsed >= this->period.value) {
		this->storage.elapsed -= this->period.value;
		count++;
	}

	if (count > 0) {
		this->callback(count);
	}
}

template<>
void TimeoutTimer<TimerGameTick>::Elapsed(TimerGameTick::TElapsed delta)
{
	if (this->fired) return;
	if (this->period.value == 0) return;

	this->storage.elapsed += delta;

	if (this->storage.elapsed >= this->period.value) {
		this->callback();
		this->fired = true;
	}
}

template<>
bool TimerManager<TimerGameTick>::Elapsed(TimerGameTick::TElapsed delta)
{
	TimerGameTick::counter++;

	for (auto timer : TimerManager<TimerGameTick>::GetTimers()) {
		timer->Elapsed(delta);
	}

	return true;
}

#ifdef WITH_ASSERT
template<>
void TimerManager<TimerGameTick>::Validate(TimerGameTick::TPeriod period)
{
	if (period.priority == TimerGameTick::Priority::NONE) return;

	/* Validate we didn't make a developer error and scheduled more than one
	 * entry on the same priority. There can only be one timer on
	 * a specific priority, to ensure we are deterministic, and to avoid
	 * container sort order invariant issues with timer period saveload. */
	for (const auto &timer : TimerManager<TimerGameTick>::GetTimers()) {
		assert(timer->period.priority != period.priority);
	}
}
#endif /* WITH_ASSERT */
