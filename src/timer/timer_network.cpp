/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file timer_network.cpp
 * This file implements the timer logic for the network-timer.
 */

#include "stdafx.h"
#include "timer.h"
#include "timer_network.h"
#include "../network/network_internal.h"

#include "safeguards.h"

template<>
void IntervalTimer<TimerNetwork>::Elapsed(TimerNetwork::TElapsed delta)
{
	this->callback(delta);
}

template<>
void TimeoutTimer<TimerNetwork>::Elapsed(TimerNetwork::TElapsed delta)
{
	if (this->fired) return;

	this->callback(1);
	this->fired = true;
}

template<>
void TimerManager<TimerNetwork>::Elapsed(TimerNetwork::TElapsed delta)
{
	assert(delta == 1);

	_frame_counter++;

	/* Make a temporary copy of the timers, as a timer's callback might add/remove other timers. */
	auto timers = TimerManager<TimerNetwork>::GetTimers();

	for (auto timer : timers) {
		timer->Elapsed(1);
	}
}
