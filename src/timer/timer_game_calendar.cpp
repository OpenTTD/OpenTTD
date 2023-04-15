/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file timer_game_calendar.cpp
 * This file implements the timer logic for the game-calendar-timer.
 */

#include "stdafx.h"
#include "date_func.h"
#include "openttd.h"
#include "timer.h"
#include "timer_game_calendar.h"
#include "vehicle_base.h"
#include "linkgraph/linkgraph.h"

#include "safeguards.h"

template<>
void IntervalTimer<TimerGameCalendar>::Elapsed(TimerGameCalendar::TElapsed trigger)
{
	if (trigger == this->period.trigger) {
		this->callback(1);
	}
}

template<>
void TimeoutTimer<TimerGameCalendar>::Elapsed(TimerGameCalendar::TElapsed trigger)
{
	if (this->fired) return;

	if (trigger == this->period.trigger) {
		this->callback();
		this->fired = true;
	}
}

template<>
void TimerManager<TimerGameCalendar>::Elapsed(TimerGameCalendar::TElapsed delta)
{
	assert(delta == 1);

	_tick_counter++;

	if (_game_mode == GM_MENU) return;

	_date_fract++;
	if (_date_fract < DAY_TICKS) return;
	_date_fract = 0;

	/* increase day counter */
	_date++;

	YearMonthDay ymd;
	ConvertDateToYMD(_date, &ymd);

	/* check if we entered a new month? */
	bool new_month = ymd.month != _cur_month;

	/* check if we entered a new year? */
	bool new_year = ymd.year != _cur_year;

	/* update internal variables before calling the daily/monthly/yearly loops */
	_cur_month = ymd.month;
	_cur_year  = ymd.year;

	/* Make a temporary copy of the timers, as a timer's callback might add/remove other timers. */
	auto timers = TimerManager<TimerGameCalendar>::GetTimers();

	for (auto timer : timers) {
		timer->Elapsed(TimerGameCalendar::DAY);
	}

	if (new_month) {
		for (auto timer : timers) {
			timer->Elapsed(TimerGameCalendar::MONTH);
		}
	}

	if (new_year) {
		for (auto timer : timers) {
			timer->Elapsed(TimerGameCalendar::YEAR);
		}
	}

	/* check if we reached the maximum year, decrement dates by a year */
	if (_cur_year == MAX_YEAR + 1) {
		int days_this_year;

		_cur_year--;
		days_this_year = IsLeapYear(_cur_year) ? DAYS_IN_LEAP_YEAR : DAYS_IN_YEAR;
		_date -= days_this_year;
		for (Vehicle *v : Vehicle::Iterate()) v->ShiftDates(-days_this_year);
		for (LinkGraph *lg : LinkGraph::Iterate()) lg->ShiftDates(-days_this_year);
	}
}

#ifdef WITH_ASSERT
template<>
void TimerManager<TimerGameCalendar>::Validate(TimerGameCalendar::TPeriod period)
{
	if (period.priority == TimerGameCalendar::Priority::NONE) return;

	/* Validate we didn't make a developer error and scheduled more than one
	 * entry on the same priority/trigger. There can only be one timer on
	 * a specific trigger/priority, to ensure we are deterministic. */
	for (const auto &timer : TimerManager<TimerGameCalendar>::GetTimers()) {
		if (timer->period.trigger != period.trigger) continue;

		assert(timer->period.priority != period.priority);
	}
}
#endif /* WITH_ASSERT */
