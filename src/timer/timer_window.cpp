/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file timer_window.cpp
 * This file implements the timer logic for the Window system.
 */

#include "../stdafx.h"
#include "timer.h"
#include "timer_window.h"

#include "../safeguards.h"

template<>
void IntervalTimer<TimerWindow>::Elapsed(TimerWindow::TElapsed delta)
{
	if (this->period == std::chrono::milliseconds::zero()) return;

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
void TimeoutTimer<TimerWindow>::Elapsed(TimerWindow::TElapsed delta)
{
	if (this->fired) return;
	if (this->period == std::chrono::milliseconds::zero()) return;

	this->storage.elapsed += delta;

	if (this->storage.elapsed >= this->period) {
		this->callback();
		this->fired = true;
	}
}

template<>
void TimerManager<TimerWindow>::Elapsed(TimerWindow::TElapsed delta)
{
	/* Make a temporary copy of the timers, as a timer's callback might add/remove other timers. */
	auto timers = TimerManager<TimerWindow>::GetTimers();

	for (auto timer : timers) {
		timer->Elapsed(delta);
	}
}

#ifdef WITH_ASSERT
template<>
void TimerManager<TimerWindow>::Validate(TimerWindow::TPeriod)
{
}
#endif /* WITH_ASSERT */
