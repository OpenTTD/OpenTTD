/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_date.hpp Everything to query and manipulate date related information. */

#ifndef SCRIPT_DATE_HPP
#define SCRIPT_DATE_HPP

#include "script_object.hpp"
#include "../../timer/timer_game_economy.h"

/**
 * Class that handles all date related (calculation) functions.
 * @api ai game
 *
 * @note Months and days of month are 1-based; the first month of the
 *       year is 1 and the first day of the month is also 1.
 * @note Years are zero based; they start with the year 0.
 * @note Dates can be used to determine the number of days between
 *       two different moments in time because they count the number
 *       of days since the year 0.
 *
 * \anchor ScriptCalendarTime
 * \b Calendar-Time
 *
 * Calendar time measures the technological progression in the game.
 * \li The calendar date is shown in the status bar.
 * \li The calendar date affects engine model introduction and expiration.
 * \li Progression of calendar time can be slowed or even halted via game settings.
 *
 * Calendar time uses the Gregorian calendar with 365 or 366 days per year.
 *
 * \anchor ScriptEconomyTime
 * \b Economy-Time
 *
 * Economy time measures the in-game time progression, while the game is not paused.
 * \li Cargo production and consumption follows economy time.
 * \li Recurring income and expenses follow economy time.
 * \li Production and income statistics and balances are created per economy month/quarter/year.
 *
 * Depending on game settings economy time is represented differently:
 * \li Calendar-based timekeeping: Economy- and calendar-time use the identical Gregorian calendar.
 * \li Wallclock-based timekeeping: Economy- and calendar-time are separate.
 *     Economy-time will use a 360 day calendar (12 months with 30 days each), which runs at a constant speed of one economy-month per realtime-minute.
 *     Calendar-time will use a Gregorian calendar, which can be slowed to stopped via game settings.
 */
class ScriptDate : public ScriptObject {
public:
	/**
	 * Date data type is an integer value. Use ScriptDate::GetDate to
	 * compose valid date values for a known year, month and day.
	 */
	enum Date {
		DATE_INVALID = ::EconomyTime::INVALID_DATE.base(), ///< A value representing an invalid date.
	};

	/**
	 * Validates if a date value represent a valid date.
	 * @param date The date to validate.
	 * @return True if the date is valid, otherwise false
	 */
	static bool IsValidDate(Date date);

	/**
	 * Get the current date.
	 * This is the number of days since epoch under the assumption that
	 *  there is a leap year every 4 years, except when dividable by
	 *  100 but not by 400.
	 * @return The current date.
	 */
	static Date GetCurrentDate();

	/**
	 * Get the year of the given date.
	 * @param date The date to get the year of.
	 * @return The year.
	 */
	static SQInteger GetYear(Date date);

	/**
	 * Get the month of the given date.
	 * @param date The date to get the month of.
	 * @return The month.
	 */
	static SQInteger GetMonth(Date date);

	/**
	 * Get the day (of the month) of the given date.
	 * @param date The date to get the day of.
	 * @return The day.
	 */
	static SQInteger GetDayOfMonth(Date date);

	/**
	 * Get the date given a year, month and day of month.
	 * @param year The year of the to-be determined date.
	 * @param month The month of the to-be determined date.
	 * @param day_of_month The day of month of the to-be determined date.
	 * @return The date.
	 */
	static Date GetDate(SQInteger year, SQInteger month, SQInteger day_of_month);

	/**
	 * Get the time of the host system.
	 * @return The amount of seconds passed since 1 Jan 1970.
	 * @api -ai
	 * @note This uses the clock of the host system, which can skew or be set back. Use with caution.
	 */
	static SQInteger GetSystemTime();
};

#endif /* SCRIPT_DATE_HPP */
