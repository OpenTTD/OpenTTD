/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file timer_game_economy.cpp
 * This file implements the timer logic for the game-economy-timer.
 */

#include "../stdafx.h"
#include "../openttd.h"
#include "timer.h"
#include "timer_game_economy.h"
#include "timer_game_tick.h"
#include "../vehicle_base.h"
#include "../linkgraph/linkgraph.h"

#include "../safeguards.h"

TimerGameEconomy::Year TimerGameEconomy::year = {};
TimerGameEconomy::Month TimerGameEconomy::month = {};
TimerGameEconomy::Date TimerGameEconomy::date = {};
TimerGameEconomy::DateFract TimerGameEconomy::date_fract = {};

/**
 * Set the date.
 * @param date The new date
 * @param fract The number of ticks that have passed on this date.
 */
/* static */ void TimerGameEconomy::SetDate(TimerGameEconomy::Date date, TimerGameEconomy::DateFract fract)
{
	assert(fract < Ticks::DAY_TICKS);

	TimerGameEconomy::date = date;
	TimerGameEconomy::date_fract = fract;
	TimerGameEconomy::YearMonthDay ymd = TimerGameEconomy::ConvertDateToYMD(date);
	TimerGameEconomy::year = ymd.year;
	TimerGameEconomy::month = ymd.month;
}

template<>
void IntervalTimer<TimerGameEconomy>::Elapsed(TimerGameEconomy::TElapsed trigger)
{
	if (trigger == this->period.trigger) {
		this->callback(1);
	}
}

template<>
void TimeoutTimer<TimerGameEconomy>::Elapsed(TimerGameEconomy::TElapsed trigger)
{
	if (this->fired) return;

	if (trigger == this->period.trigger) {
		this->callback();
		this->fired = true;
	}
}

template<>
void TimerManager<TimerGameEconomy>::Elapsed([[maybe_unused]] TimerGameEconomy::TElapsed delta)
{
	assert(delta == 1);

	if (_game_mode == GM_MENU) return;

	TimerGameEconomy::date_fract++;
	if (TimerGameEconomy::date_fract < Ticks::DAY_TICKS) return;
	TimerGameEconomy::date_fract = 0;

	/* increase day counter */
	TimerGameEconomy::date++;

	TimerGameEconomy::YearMonthDay ymd = TimerGameEconomy::ConvertDateToYMD(TimerGameEconomy::date);

	/* check if we entered a new month? */
	bool new_month = ymd.month != TimerGameEconomy::month;

	/* check if we entered a new year? */
	bool new_year = ymd.year != TimerGameEconomy::year;

	/* update internal variables before calling the daily/monthly/yearly loops */
	TimerGameEconomy::month = ymd.month;
	TimerGameEconomy::year = ymd.year;

	/* Make a temporary copy of the timers, as a timer's callback might add/remove other timers. */
	auto timers = TimerManager<TimerGameEconomy>::GetTimers();

	for (auto timer : timers) {
		timer->Elapsed(TimerGameEconomy::DAY);
	}

	if ((TimerGameEconomy::date.base() % 7) == 3) {
		for (auto timer : timers) {
			timer->Elapsed(TimerGameEconomy::WEEK);
		}
	}

	if (new_month) {
		for (auto timer : timers) {
			timer->Elapsed(TimerGameEconomy::MONTH);
		}

		if ((TimerGameEconomy::month % 3) == 0) {
			for (auto timer : timers) {
				timer->Elapsed(TimerGameEconomy::QUARTER);
			}
		}
	}

	if (new_year) {
		for (auto timer : timers) {
			timer->Elapsed(TimerGameEconomy::YEAR);
		}
	}

	/* check if we reached the maximum year, decrement dates by a year */
	if (TimerGameEconomy::year == EconomyTime::MAX_YEAR + 1) {
		int days_this_year;

		TimerGameEconomy::year--;
		days_this_year = TimerGameEconomy::IsLeapYear(TimerGameEconomy::year) ? EconomyTime::DAYS_IN_LEAP_YEAR : EconomyTime::DAYS_IN_YEAR;
		TimerGameEconomy::date -= days_this_year;
		for (Vehicle *v : Vehicle::Iterate()) v->ShiftDates(-days_this_year);
		for (LinkGraph *lg : LinkGraph::Iterate()) lg->ShiftDates(-days_this_year);
	}
}

#ifdef WITH_ASSERT
template<>
void TimerManager<TimerGameEconomy>::Validate(TimerGameEconomy::TPeriod period)
{
	if (period.priority == TimerGameEconomy::Priority::NONE) return;

	/* Validate we didn't make a developer error and scheduled more than one
	 * entry on the same priority/trigger. There can only be one timer on
	 * a specific trigger/priority, to ensure we are deterministic. */
	for (const auto &timer : TimerManager<TimerGameEconomy>::GetTimers()) {
		if (timer->period.trigger != period.trigger) continue;

		assert(timer->period.priority != period.priority);
	}
}
#endif /* WITH_ASSERT */
