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

TimerGameCalendar::Year TimerGameCalendar::year = {};
TimerGameCalendar::Month TimerGameCalendar::month = {};
TimerGameCalendar::Date TimerGameCalendar::date = {};
TimerGameCalendar::DateFract TimerGameCalendar::date_fract = {};

/**
 * Set the date.
 * @param date  New date
 * @param fract The number of ticks that have passed on this date.
 */
/* static */ void TimerGameCalendar::SetDate(TimerGameCalendar::Date date, TimerGameCalendar::DateFract fract)
{
	assert(fract < DAY_TICKS);

	YearMonthDay ymd;

	TimerGameCalendar::date = date;
	TimerGameCalendar::date_fract = fract;
	ConvertDateToYMD(date, &ymd);
	TimerGameCalendar::year = ymd.year;
	TimerGameCalendar::month = ymd.month;
}

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

	if (_game_mode == GM_MENU) return;

	TimerGameCalendar::date_fract++;
	if (TimerGameCalendar::date_fract < DAY_TICKS) return;
	TimerGameCalendar::date_fract = 0;

	/* increase day counter */
	TimerGameCalendar::date++;

	YearMonthDay ymd;
	ConvertDateToYMD(TimerGameCalendar::date, &ymd);

	/* check if we entered a new month? */
	bool new_month = ymd.month != TimerGameCalendar::month;

	/* check if we entered a new year? */
	bool new_year = ymd.year != TimerGameCalendar::year;

	/* update internal variables before calling the daily/monthly/yearly loops */
	TimerGameCalendar::month = ymd.month;
	TimerGameCalendar::year = ymd.year;

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
	if (TimerGameCalendar::year == MAX_YEAR + 1) {
		int days_this_year;

		TimerGameCalendar::year--;
		days_this_year = IsLeapYear(TimerGameCalendar::year) ? DAYS_IN_LEAP_YEAR : DAYS_IN_YEAR;
		TimerGameCalendar::date -= days_this_year;
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
