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

#include "../stdafx.h"
#include "../openttd.h"
#include "timer.h"
#include "timer_game_calendar.h"
#include "../vehicle_base.h"
#include "../linkgraph/linkgraph.h"

#include "../safeguards.h"

TimerGameCalendar::Year TimerGameCalendar::year = {};
TimerGameCalendar::Month TimerGameCalendar::month = {};
TimerGameCalendar::Date TimerGameCalendar::date = {};
TimerGameCalendar::DateFract TimerGameCalendar::date_fract = {};

#define M(a, b) ((a << 5) | b)
static const uint16_t _month_date_from_year_day[] = {
	M(0, 1), M(0, 2), M(0, 3), M(0, 4), M(0, 5), M(0, 6), M(0, 7), M(0, 8), M(0, 9), M(0, 10), M(0, 11), M(0, 12), M(0, 13), M(0, 14), M(0, 15), M(0, 16), M(0, 17), M(0, 18), M(0, 19), M(0, 20), M(0, 21), M(0, 22), M(0, 23), M(0, 24), M(0, 25), M(0, 26), M(0, 27), M(0, 28), M(0, 29), M(0, 30), M(0, 31),
	M(1, 1), M(1, 2), M(1, 3), M(1, 4), M(1, 5), M(1, 6), M(1, 7), M(1, 8), M(1, 9), M(1, 10), M(1, 11), M(1, 12), M(1, 13), M(1, 14), M(1, 15), M(1, 16), M(1, 17), M(1, 18), M(1, 19), M(1, 20), M(1, 21), M(1, 22), M(1, 23), M(1, 24), M(1, 25), M(1, 26), M(1, 27), M(1, 28), M(1, 29),
	M(2, 1), M(2, 2), M(2, 3), M(2, 4), M(2, 5), M(2, 6), M(2, 7), M(2, 8), M(2, 9), M(2, 10), M(2, 11), M(2, 12), M(2, 13), M(2, 14), M(2, 15), M(2, 16), M(2, 17), M(2, 18), M(2, 19), M(2, 20), M(2, 21), M(2, 22), M(2, 23), M(2, 24), M(2, 25), M(2, 26), M(2, 27), M(2, 28), M(2, 29), M(2, 30), M(2, 31),
	M(3, 1), M(3, 2), M(3, 3), M(3, 4), M(3, 5), M(3, 6), M(3, 7), M(3, 8), M(3, 9), M(3, 10), M(3, 11), M(3, 12), M(3, 13), M(3, 14), M(3, 15), M(3, 16), M(3, 17), M(3, 18), M(3, 19), M(3, 20), M(3, 21), M(3, 22), M(3, 23), M(3, 24), M(3, 25), M(3, 26), M(3, 27), M(3, 28), M(3, 29), M(3, 30),
	M(4, 1), M(4, 2), M(4, 3), M(4, 4), M(4, 5), M(4, 6), M(4, 7), M(4, 8), M(4, 9), M(4, 10), M(4, 11), M(4, 12), M(4, 13), M(4, 14), M(4, 15), M(4, 16), M(4, 17), M(4, 18), M(4, 19), M(4, 20), M(4, 21), M(4, 22), M(4, 23), M(4, 24), M(4, 25), M(4, 26), M(4, 27), M(4, 28), M(4, 29), M(4, 30), M(4, 31),
	M(5, 1), M(5, 2), M(5, 3), M(5, 4), M(5, 5), M(5, 6), M(5, 7), M(5, 8), M(5, 9), M(5, 10), M(5, 11), M(5, 12), M(5, 13), M(5, 14), M(5, 15), M(5, 16), M(5, 17), M(5, 18), M(5, 19), M(5, 20), M(5, 21), M(5, 22), M(5, 23), M(5, 24), M(5, 25), M(5, 26), M(5, 27), M(5, 28), M(5, 29), M(5, 30),
	M(6, 1), M(6, 2), M(6, 3), M(6, 4), M(6, 5), M(6, 6), M(6, 7), M(6, 8), M(6, 9), M(6, 10), M(6, 11), M(6, 12), M(6, 13), M(6, 14), M(6, 15), M(6, 16), M(6, 17), M(6, 18), M(6, 19), M(6, 20), M(6, 21), M(6, 22), M(6, 23), M(6, 24), M(6, 25), M(6, 26), M(6, 27), M(6, 28), M(6, 29), M(6, 30), M(6, 31),
	M(7, 1), M(7, 2), M(7, 3), M(7, 4), M(7, 5), M(7, 6), M(7, 7), M(7, 8), M(7, 9), M(7, 10), M(7, 11), M(7, 12), M(7, 13), M(7, 14), M(7, 15), M(7, 16), M(7, 17), M(7, 18), M(7, 19), M(7, 20), M(7, 21), M(7, 22), M(7, 23), M(7, 24), M(7, 25), M(7, 26), M(7, 27), M(7, 28), M(7, 29), M(7, 30), M(7, 31),
	M(8, 1), M(8, 2), M(8, 3), M(8, 4), M(8, 5), M(8, 6), M(8, 7), M(8, 8), M(8, 9), M(8, 10), M(8, 11), M(8, 12), M(8, 13), M(8, 14), M(8, 15), M(8, 16), M(8, 17), M(8, 18), M(8, 19), M(8, 20), M(8, 21), M(8, 22), M(8, 23), M(8, 24), M(8, 25), M(8, 26), M(8, 27), M(8, 28), M(8, 29), M(8, 30),
	M(9, 1), M(9, 2), M(9, 3), M(9, 4), M(9, 5), M(9, 6), M(9, 7), M(9, 8), M(9, 9), M(9, 10), M(9, 11), M(9, 12), M(9, 13), M(9, 14), M(9, 15), M(9, 16), M(9, 17), M(9, 18), M(9, 19), M(9, 20), M(9, 21), M(9, 22), M(9, 23), M(9, 24), M(9, 25), M(9, 26), M(9, 27), M(9, 28), M(9, 29), M(9, 30), M(9, 31),
	M(10, 1), M(10, 2), M(10, 3), M(10, 4), M(10, 5), M(10, 6), M(10, 7), M(10, 8), M(10, 9), M(10, 10), M(10, 11), M(10, 12), M(10, 13), M(10, 14), M(10, 15), M(10, 16), M(10, 17), M(10, 18), M(10, 19), M(10, 20), M(10, 21), M(10, 22), M(10, 23), M(10, 24), M(10, 25), M(10, 26), M(10, 27), M(10, 28), M(10, 29), M(10, 30),
	M(11, 1), M(11, 2), M(11, 3), M(11, 4), M(11, 5), M(11, 6), M(11, 7), M(11, 8), M(11, 9), M(11, 10), M(11, 11), M(11, 12), M(11, 13), M(11, 14), M(11, 15), M(11, 16), M(11, 17), M(11, 18), M(11, 19), M(11, 20), M(11, 21), M(11, 22), M(11, 23), M(11, 24), M(11, 25), M(11, 26), M(11, 27), M(11, 28), M(11, 29), M(11, 30), M(11, 31),
};
#undef M

enum DaysTillMonth {
	ACCUM_JAN = 0,
	ACCUM_FEB = ACCUM_JAN + 31,
	ACCUM_MAR = ACCUM_FEB + 29,
	ACCUM_APR = ACCUM_MAR + 31,
	ACCUM_MAY = ACCUM_APR + 30,
	ACCUM_JUN = ACCUM_MAY + 31,
	ACCUM_JUL = ACCUM_JUN + 30,
	ACCUM_AUG = ACCUM_JUL + 31,
	ACCUM_SEP = ACCUM_AUG + 31,
	ACCUM_OCT = ACCUM_SEP + 30,
	ACCUM_NOV = ACCUM_OCT + 31,
	ACCUM_DEC = ACCUM_NOV + 30,
};

/** Number of days to pass from the first day in the year before reaching the first of a month. */
static const uint16_t _accum_days_for_month[] = {
	ACCUM_JAN, ACCUM_FEB, ACCUM_MAR, ACCUM_APR,
	ACCUM_MAY, ACCUM_JUN, ACCUM_JUL, ACCUM_AUG,
	ACCUM_SEP, ACCUM_OCT, ACCUM_NOV, ACCUM_DEC,
};

/**
 * Converts a Date to a Year, Month & Day.
 * @param date the date to convert from
 * @returns YearMonthDay representation of the Date.
 */
/* static */ TimerGameCalendar::YearMonthDay TimerGameCalendar::ConvertDateToYMD(TimerGameCalendar::Date date)
{
	/* Year determination in multiple steps to account for leap
	 * years. First do the large steps, then the smaller ones.
	 */

	/* There are 97 leap years in 400 years */
	TimerGameCalendar::Year yr = 400 * (date.base() / (CalendarTime::DAYS_IN_YEAR * 400 + 97));
	int rem = date.base() % (CalendarTime::DAYS_IN_YEAR * 400 + 97);

	if (rem >= CalendarTime::DAYS_IN_YEAR * 100 + 25) {
		/* There are 25 leap years in the first 100 years after
		 * every 400th year, as every 400th year is a leap year */
		yr += 100;
		rem -= CalendarTime::DAYS_IN_YEAR * 100 + 25;

		/* There are 24 leap years in the next couple of 100 years */
		yr += 100 * (rem / (CalendarTime::DAYS_IN_YEAR * 100 + 24));
		rem = (rem % (CalendarTime::DAYS_IN_YEAR * 100 + 24));
	}

	if (!TimerGameCalendar::IsLeapYear(yr) && rem >= CalendarTime::DAYS_IN_YEAR * 4) {
		/* The first 4 year of the century are not always a leap year */
		yr += 4;
		rem -= CalendarTime::DAYS_IN_YEAR * 4;
	}

	/* There is 1 leap year every 4 years */
	yr += 4 * (rem / (CalendarTime::DAYS_IN_YEAR * 4 + 1));
	rem = rem % (CalendarTime::DAYS_IN_YEAR * 4 + 1);

	/* The last (max 3) years to account for; the first one
	 * can be, but is not necessarily a leap year */
	while (rem >= (TimerGameCalendar::IsLeapYear(yr) ? CalendarTime::DAYS_IN_LEAP_YEAR : CalendarTime::DAYS_IN_YEAR)) {
		rem -= TimerGameCalendar::IsLeapYear(yr) ? CalendarTime::DAYS_IN_LEAP_YEAR : CalendarTime::DAYS_IN_YEAR;
		yr++;
	}

	/* Skip the 29th of February in non-leap years */
	if (!TimerGameCalendar::IsLeapYear(yr) && rem >= ACCUM_MAR - 1) rem++;

	uint16_t x = _month_date_from_year_day[rem];

	YearMonthDay ymd;
	ymd.year = yr;
	ymd.month = x >> 5;
	ymd.day = x & 0x1F;
	return ymd;
}

/**
 * Converts a tuple of Year, Month and Day to a Date.
 * @param year  is a number between 0..MAX_YEAR
 * @param month is a number between 0..11
 * @param day   is a number between 1..31
 */
/* static */ TimerGameCalendar::Date TimerGameCalendar::ConvertYMDToDate(TimerGameCalendar::Year year, TimerGameCalendar::Month month, TimerGameCalendar::Day day)
{
	/* Day-offset in a leap year */
	int days = _accum_days_for_month[month] + day - 1;

	/* Account for the missing of the 29th of February in non-leap years */
	if (!TimerGameCalendar::IsLeapYear(year) && days >= ACCUM_MAR) days--;

	return TimerGameCalendar::DateAtStartOfYear(year) + days;
}

/**
 * Checks whether the given year is a leap year or not.
 * @param yr The year to check.
 * @return True if \c yr is a leap year, otherwise false.
 */
/* static */ bool TimerGameCalendar::IsLeapYear(TimerGameCalendar::Year yr)
{
	return yr.base() % 4 == 0 && (yr.base() % 100 != 0 || yr.base() % 400 == 0);
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
void TimerManager<TimerGameCalendar>::Elapsed([[maybe_unused]] TimerGameCalendar::TElapsed delta)
{
	assert(delta == 1);

	if (_game_mode == GM_MENU) return;

	TimerGameCalendar::date_fract++;
	if (TimerGameCalendar::date_fract < Ticks::DAY_TICKS) return;
	TimerGameCalendar::date_fract = 0;

	/* increase day counter */
	TimerGameCalendar::date++;

	TimerGameCalendar::YearMonthDay ymd = TimerGameCalendar::ConvertDateToYMD(TimerGameCalendar::date);

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

	if ((TimerGameCalendar::date.base() % 7) == 3) {
		for (auto timer : timers) {
			timer->Elapsed(TimerGameCalendar::WEEK);
		}
	}

	if (new_month) {
		for (auto timer : timers) {
			timer->Elapsed(TimerGameCalendar::MONTH);
		}

		if ((TimerGameCalendar::month % 3) == 0) {
			for (auto timer : timers) {
				timer->Elapsed(TimerGameCalendar::QUARTER);
			}
		}
	}

	if (new_year) {
		for (auto timer : timers) {
			timer->Elapsed(TimerGameCalendar::YEAR);
		}
	}

	/* check if we reached the maximum year, decrement dates by a year */
	if (TimerGameCalendar::year == CalendarTime::MAX_YEAR + 1) {
		int days_this_year;

		TimerGameCalendar::year--;
		days_this_year = TimerGameCalendar::IsLeapYear(TimerGameCalendar::year) ? CalendarTime::DAYS_IN_LEAP_YEAR : CalendarTime::DAYS_IN_YEAR;
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
