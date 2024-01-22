/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file timer_game_calendar.h Definition of the game-calendar-timer */

#ifndef TIMER_GAME_CALENDAR_H
#define TIMER_GAME_CALENDAR_H

#include "../stdafx.h"
#include "../core/strong_typedef_type.hpp"
#include "timer_game_common.h"

/**
 * Timer that is increased every 27ms, and counts towards ticks / days / months / years.
 *
 * The amount of days in a month depends on the month and year (leap-years).
 * There are always 74 ticks in a day (and with 27ms, this makes 1 day 1.998 seconds).
 *
 * Calendar time is used for technology and time-of-year changes, including:
 * - Vehicle, airport, station, object introduction and obsolescence
 * - NewGRF variables for visual styles or behavior based on year or time of year (e.g. variable snow line)
 * - Inflation, since it is tied to original game years. One interpretation of inflation is that it compensates for faster and higher capacity vehicles,
 *   another is that it compensates for more established companies. Each of these point to a different choice of calendar versus economy time, but we have to pick one
 *   so we follow a previous decision to tie inflation to original TTD game years.
 */
class TimerGameCalendar : public TimerGame<struct Calendar> {
public:
	static Year year; ///< Current year, starting at 0.
	static Month month; ///< Current month (0..11).
	static Date date; ///< Current date in days (day counter).
	static DateFract date_fract; ///< Fractional part of the day.

	static void SetDate(Date date, DateFract fract);
};

/**
 * Storage class for Calendar time constants.
 */
class CalendarTime : public TimerGameConst<struct Calendar> {};

#endif /* TIMER_GAME_CALENDAR_H */
