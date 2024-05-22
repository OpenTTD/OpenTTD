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

/**
 * Calendar time is used for technology and time-of-year changes, including:
 * - Vehicle, airport, station, object introduction and obsolescence
 * - Vehicle and engine age
 * - NewGRF variables for visual styles or behavior based on year or time of year (e.g. variable snow line)
 * - Inflation, since it is tied to original game years. One interpretation of inflation is that it compensates for faster and higher capacity vehicles,
 *   another is that it compensates for more established companies. Each of these point to a different choice of calendar versus economy time, but we have to pick one
 *   so we follow a previous decision to tie inflation to original TTD game years.
 */

#include "../stdafx.h"
#include "../openttd.h"
#include "timer.h"
#include "timer_game_calendar.h"
#include "../vehicle_base.h"

#include "../safeguards.h"

TimerGameCalendar::Year TimerGameCalendar::year = {};
TimerGameCalendar::Month TimerGameCalendar::month = {};
TimerGameCalendar::Date TimerGameCalendar::date = {};
TimerGameCalendar::DateFract TimerGameCalendar::date_fract = {};
uint16_t TimerGameCalendar::sub_date_fract = {};

/**
 * Converts a Date to a Year, Month & Day.
 * @param date the date to convert from
 * @returns YearMonthDay representation of the Date.
 */
/* static */ TimerGameCalendar::YearMonthDay TimerGameCalendar::ConvertDateToYMD(TimerGameCalendar::Date date)
{
	/* This wrapper function only exists because economy time sometimes does things differently, when using wallclock units. */
	return CalendarConvertDateToYMD(date);
}

/**
 * Converts a tuple of Year, Month and Day to a Date.
 * @param year  is a number between 0..MAX_YEAR
 * @param month is a number between 0..11
 * @param day   is a number between 1..31
 * @returns The equivalent date.
 */
/* static */ TimerGameCalendar::Date TimerGameCalendar::ConvertYMDToDate(TimerGameCalendar::Year year, TimerGameCalendar::Month month, TimerGameCalendar::Day day)
{
	/* This wrapper function only exists because economy time sometimes does things differently, when using wallclock units. */
	return CalendarConvertYMDToDate(year, month, day);
}

/**
 * Set the date.
 * @param date  New date
 * @param fract The number of ticks that have passed on this date.
 */
/* static */ void TimerGameCalendar::SetDate(TimerGameCalendar::Date date, TimerGameCalendar::DateFract fract)
{
	assert(fract < Ticks::DAY_TICKS);

	TimerGameCalendar::date = date;
	TimerGameCalendar::date_fract = fract;
	TimerGameCalendar::YearMonthDay ymd = TimerGameCalendar::ConvertDateToYMD(date);
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
bool TimerManager<TimerGameCalendar>::Elapsed([[maybe_unused]] TimerGameCalendar::TElapsed delta)
{
	assert(delta == 1);

	if (_game_mode == GM_MENU) return false;

	/* If calendar day progress is frozen, don't try to advance time. */
	if (_settings_game.economy.minutes_per_calendar_year == CalendarTime::FROZEN_MINUTES_PER_YEAR) return false;

	/* If we are using a non-default calendar progression speed, we need to check the sub_date_fract before updating date_fract. */
	if (_settings_game.economy.minutes_per_calendar_year != CalendarTime::DEF_MINUTES_PER_YEAR) {
		TimerGameCalendar::sub_date_fract += Ticks::DAY_TICKS;

		/* Check if we are ready to increment date_fract */
		const uint16_t threshold = (_settings_game.economy.minutes_per_calendar_year * Ticks::DAY_TICKS) / CalendarTime::DEF_MINUTES_PER_YEAR;
		if (TimerGameCalendar::sub_date_fract < threshold) return false;

		TimerGameCalendar::sub_date_fract = std::min<uint16_t>(TimerGameCalendar::sub_date_fract - threshold, Ticks::DAY_TICKS - 1);
	}

	TimerGameCalendar::date_fract++;

	/* Check if we entered a new day. */
	if (TimerGameCalendar::date_fract < Ticks::DAY_TICKS) return true;
	TimerGameCalendar::date_fract = 0;
	TimerGameCalendar::sub_date_fract = 0;

	/* Increase day counter. */
	TimerGameCalendar::date++;

	TimerGameCalendar::YearMonthDay ymd = TimerGameCalendar::ConvertDateToYMD(TimerGameCalendar::date);

	/* Check if we entered a new month. */
	bool new_month = ymd.month != TimerGameCalendar::month;

	/* Check if we entered a new year. */
	bool new_year = ymd.year != TimerGameCalendar::year;

	/* Update internal variables before calling the daily/monthly/yearly loops. */
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

	/* If we reached the maximum year, decrement dates by a year. */
	if (TimerGameCalendar::year == CalendarTime::MAX_YEAR + 1) {
		int days_this_year;

		TimerGameCalendar::year--;
		days_this_year = TimerGameCalendar::IsLeapYear(TimerGameCalendar::year) ? CalendarTime::DAYS_IN_LEAP_YEAR : CalendarTime::DAYS_IN_YEAR;
		TimerGameCalendar::date -= days_this_year;
	}

	return true;
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
