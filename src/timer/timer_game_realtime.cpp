/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file timer_game_realtime.cpp
 * This file implements the timer logic for the real time game-timer.
 */

#include "../stdafx.h"
#include "../openttd.h"
#include "timer.h"
#include "timer_game_realtime.h"

#include "../safeguards.h"

template<>
void IntervalTimer<TimerGameRealtime>::Elapsed(TimerGameRealtime::TElapsed delta)
{
	if (this->period.period == std::chrono::milliseconds::zero()) return;
	if (this->period.flag == TimerGameRealtime::PeriodFlags::AUTOSAVE && _pause_mode != PM_UNPAUSED && (_pause_mode & PM_COMMAND_DURING_PAUSE) == 0) return;
	if (this->period.flag == TimerGameRealtime::PeriodFlags::UNPAUSED && _pause_mode != PM_UNPAUSED) return;

	this->storage.elapsed += delta;

	uint count = 0;
	while (this->storage.elapsed >= this->period.period) {
		this->storage.elapsed -= this->period.period;
		count++;
	}

	if (count > 0) {
		this->callback(count);
	}
}

template<>
void TimeoutTimer<TimerGameRealtime>::Elapsed(TimerGameRealtime::TElapsed delta)
{
	if (this->fired) return;
	if (this->period.period == std::chrono::milliseconds::zero()) return;
	if (this->period.flag == TimerGameRealtime::PeriodFlags::AUTOSAVE && _pause_mode != PM_UNPAUSED && (_pause_mode & PM_COMMAND_DURING_PAUSE) == 0) return;
	if (this->period.flag == TimerGameRealtime::PeriodFlags::UNPAUSED && _pause_mode != PM_UNPAUSED) return;

	this->storage.elapsed += delta;

	if (this->storage.elapsed >= this->period.period) {
		this->callback();
		this->fired = true;
	}
}

template<>
void TimerManager<TimerGameRealtime>::Elapsed(TimerGameRealtime::TElapsed delta)
{
	for (auto timer : TimerManager<TimerGameRealtime>::GetTimers()) {
		timer->Elapsed(delta);
	}
}

#ifdef WITH_ASSERT
template<>
void TimerManager<TimerGameRealtime>::Validate(TimerGameRealtime::TPeriod)
{
}
#endif /* WITH_ASSERT */
