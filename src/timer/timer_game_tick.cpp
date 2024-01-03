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
	if (this->period == 0) return;

	this->storage.elapsed += delta;

	uint count = 0;
	while (this->storage.elapsed >= this->period) {
		this->storage.elapsed -= this->period;
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
	if (this->period == 0) return;

	this->storage.elapsed += delta;

	if (this->storage.elapsed >= this->period) {
		this->callback();
		this->fired = true;
	}
}

template<>
void TimerManager<TimerGameTick>::Elapsed(TimerGameTick::TElapsed delta)
{
	TimerGameTick::counter++;

	for (auto timer : TimerManager<TimerGameTick>::GetTimers()) {
		timer->Elapsed(delta);
	}
}

#ifdef WITH_ASSERT
template<>
void TimerManager<TimerGameTick>::Validate(TimerGameTick::TPeriod)
{
}
#endif /* WITH_ASSERT */
