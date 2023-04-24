/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file timer_game_calendar.h Definition of the game-calendar-timer */

#ifndef TIMER_GAME_CALENDAR_H
#define TIMER_GAME_CALENDAR_H

#include "../date_type.h"

/**
 * Timer that is increased every 27ms, and counts towards ticks / days / months / years.
 *
 * The amount of days in a month depends on the month and year (leap-years).
 * There are always 74 ticks in a day (and with 27ms, this makes 1 day 1.998 seconds).
 *
 * IntervalTimer and TimeoutTimer based on this Timer are a bit unusual, as their count is always one.
 * You create those timers based on a transition: a new day, a new month or a new year.
 *
 * Additionally, you need to set a priority. To ensure deterministic behaviour, events are executed
 * in priority. It is important that if you assign NONE, you do not use Random() in your callback.
 * Other than that, make sure you only set one callback per priority.
 *
 * For example:
 *   IntervalTimer<TimerGameCalendar>({TimerGameCalendar::DAY, TimerGameCalendar::Priority::NONE}, [](uint count){});
 *
 * @note Callbacks are executed in the game-thread.
 */
class TimerGameCalendar {
public:
	enum Trigger {
		DAY,
		MONTH,
		YEAR,
	};
	enum Priority {
		NONE, ///< These timers can be executed in any order; there is no Random() in them, so order is not relevant.

		/* All other may have a Random() call in them, so order is important.
		 * For safety, you can only setup a single timer on a single priority. */
		AUTOSAVE,
		COMPANY,
		DISASTER,
		ENGINE,
		INDUSTRY,
		STATION,
		SUBSIDY,
		TOWN,
		VEHICLE,
	};

	struct TPeriod {
		Trigger trigger;
		Priority priority;

		TPeriod(Trigger trigger, Priority priority) : trigger(trigger), priority(priority) {}

		bool operator < (const TPeriod &other) const
		{
			if (this->trigger != other.trigger) return this->trigger < other.trigger;
			return this->priority < other.priority;
		}

		bool operator == (const TPeriod &other) const
		{
			return this->trigger == other.trigger && this->priority == other.priority;
		}
	};

	using TElapsed = uint;
	struct TStorage {
	};

	static void SetDate(Date date, DateFract fract);

	static Year year; ///< Current year, starting at 0.
	static Month month; ///< Current month (0..11).
	static Date date; ///< Current date in days (day counter).
	static DateFract date_fract; ///< Fractional part of the day.
};

#endif /* TIMER_GAME_CALENDAR_H */
