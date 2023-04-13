/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file timer_manager.h Definition of the TimerManager */
/** @note don't include this file; include "timer.h". */

#ifndef TIMER_MANAGER_H
#define TIMER_MANAGER_H

#include "stdafx.h"

#include <set>

template <typename TTimerType>
class BaseTimer;

/**
 * The TimerManager manages a single Timer-type.
 *
 * It allows for automatic registration and unregistration of timers like Interval and OneShot.
 *
 * Each Timer-type needs to implement the Elapsed() method, and distribute that to the timers if needed.
 */
template <typename TTimerType>
class TimerManager {
public:
	using TElapsed = typename TTimerType::TElapsed;

	/* Avoid copying this object; it is a singleton object. */
	TimerManager(TimerManager const &) = delete;
	TimerManager &operator=(TimerManager const &) = delete;

	/**
	 * Register a timer.
	 *
	 * @param timer The timer to register.
	 */
	static void RegisterTimer(BaseTimer<TTimerType> &timer) {
		GetTimers().insert(&timer);
	}

	/**
	 * Unregister a timer.
	 *
	 * @param timer The timer to unregister.
	 */
	static void UnregisterTimer(BaseTimer<TTimerType> &timer) {
		GetTimers().erase(&timer);
	}

	/**
	 * Called when time for this timer elapsed.
	 *
	 * The implementation per type is different, but they all share a similar goal:
	 *   Call the Elapsed() method of all active timers.
	 *
	 * @param value The amount of time that has elapsed.
	 */
	static void Elapsed(TElapsed value);

private:
	/**
	 * Sorter for timers.
	 *
	 * It will sort based on the period, smaller first. If the period is the
	 * same, it will sort based on the pointer value.
	 */
	struct base_timer_sorter {
		bool operator() (BaseTimer<TTimerType> *a, BaseTimer<TTimerType> *b) const {
			if (a->period == b->period) return a < b;
			return a->period < b->period;
		}
	};

	/** Singleton list, to store all the active timers. */
	static std::set<BaseTimer<TTimerType> *, base_timer_sorter> &GetTimers() {
		static std::set<BaseTimer<TTimerType> *, base_timer_sorter> timers;
		return timers;
	}
};

#endif /* TIMER_MANAGER_H */
