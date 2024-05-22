/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file timer_game_economy.h Definition of the game-economy-timer */

#ifndef TIMER_GAME_ECONOMY_H
#define TIMER_GAME_ECONOMY_H

#include "../core/strong_typedef_type.hpp"
#include "timer_game_common.h"

/**
 * Timer that is increased every 27ms, and counts towards economy time units, expressed in days / months / years.
 *
 * For now, this is kept in sync with the calendar date, so the amount of days in a month depends on the month and year (leap-years).
 * There are always 74 ticks in a day (and with 27ms, this makes 1 day 1.998 seconds).
 *
 * Economy time is used for the regular pace of the game, including:
 * - Industry and house production/consumption
 * - Industry production changes, closure, and spawning
 * - Town growth
 * - Company age and periodical finance stats
 * - Vehicle age and profit statistics, both individual and group
 * - Vehicle aging, depreciation, reliability, and renewal
 * - Payment intervals for running and maintenance costs, loan interest, etc.
 * - Cargo payment "time" calculation
 * - Local authority and station ratings change intervals
 */
class TimerGameEconomy : public TimerGame<struct Economy> {
public:
	static Year year; ///< Current year, starting at 0.
	static Month month; ///< Current month (0..11).
	static Date date; ///< Current date in days (day counter).
	static DateFract date_fract; ///< Fractional part of the day.

	static YearMonthDay ConvertDateToYMD(Date date);
	static Date ConvertYMDToDate(Year year, Month month, Day day);
	static void SetDate(Date date, DateFract fract);
	static bool UsingWallclockUnits(bool newgame = false);
};

/**
 * Storage class for Economy time constants.
 */
class EconomyTime : public TimerGameConst<struct Economy> {
public:
	static constexpr int DAYS_IN_ECONOMY_YEAR = 360; ///< Days in an economy year, when in wallclock timekeeping mode.
	static constexpr int DAYS_IN_ECONOMY_MONTH = 30; ///< Days in an economy month, when in wallclock timekeeping mode.
};

#endif /* TIMER_GAME_ECONOMY_H */
