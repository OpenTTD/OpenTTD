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
	/** The type to store our dates in. */
	using Date = StrongType::Typedef<int32_t, struct DateTag, StrongType::Compare, StrongType::Integer>;

	/** The fraction of a date we're in, i.e. the number of ticks since the last date changeover. */
	using DateFract = uint16_t;

	/** Type for the year, note: 0 based, i.e. starts at the year 0. */
	using Year = StrongType::Typedef<int32_t, struct YearTag, StrongType::Compare, StrongType::Integer>;
	/** Type for the month, note: 0 based, i.e. 0 = January, 11 = December. */
	using Month = uint8_t;
	/** Type for the day of the month, note: 1 based, first day of a month is 1. */
	using Day = uint8_t;

	/**
	 * Data structure to convert between Date and triplet (year, month, and day).
	 * @see TimerGameCalendar::ConvertDateToYMD(), TimerGameCalendar::ConvertYMDToDate()
	 */
	struct YearMonthDay {
		Year  year;   ///< Year (0...)
		Month month;  ///< Month (0..11)
		Day   day;    ///< Day (1..31)
	};

	enum Trigger {
		DAY,
		WEEK,
		MONTH,
		QUARTER,
		YEAR,
	};
	enum Priority {
		NONE, ///< These timers can be executed in any order; there is no Random() in them, so order is not relevant.

		/* All other may have a Random() call in them, so order is important.
		 * For safety, you can only setup a single timer on a single priority. */
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

	static bool IsLeapYear(Year yr);
	static YearMonthDay ConvertDateToYMD(Date date);
	static Date ConvertYMDToDate(Year year, Month month, Day day);
	static void SetDate(Date date, DateFract fract);

	/**
	 * Calculate the year of a given date.
	 * @param date The date to consider.
	 * @return the year.
	 */
	static constexpr Year DateToYear(Date date)
	{
		/* Hardcode the number of days in a year because we can't access CalendarTime from here. */
		return date.base() / 366;
	}

	/**
	 * Calculate the date of the first day of a given year.
	 * @param year the year to get the first day of.
	 * @return the date.
	 */
	static constexpr Date DateAtStartOfYear(Year year)
	{
		int32_t year_as_int = year.base();
		uint number_of_leap_years = (year == 0) ? 0 : ((year_as_int - 1) / 4 - (year_as_int - 1) / 100 + (year_as_int - 1) / 400 + 1);

		/* Hardcode the number of days in a year because we can't access CalendarTime from here. */
		return (365 * year_as_int) + number_of_leap_years;
	}

	static Year year; ///< Current year, starting at 0.
	static Month month; ///< Current month (0..11).
	static Date date; ///< Current date in days (day counter).
	static DateFract date_fract; ///< Fractional part of the day.
};

/**
 * Storage class for Calendar time constants.
 */
class CalendarTime {
public:
	static constexpr int DAYS_IN_YEAR = 365; ///< days per year
	static constexpr int DAYS_IN_LEAP_YEAR = 366; ///< sometimes, you need one day more...
	static constexpr int MONTHS_IN_YEAR = 12; ///< months per year

	static constexpr int SECONDS_PER_DAY = 2;   ///< approximate seconds per day, not for precise calculations

	/*
	 * ORIGINAL_BASE_YEAR, ORIGINAL_MAX_YEAR and DAYS_TILL_ORIGINAL_BASE_YEAR are
	 * primarily used for loading newgrf and savegame data and returning some
	 * newgrf (callback) functions that were in the original (TTD) inherited
	 * format, where 'TimerGameCalendar::date == 0' meant that it was 1920-01-01.
	 */

	/** The minimum starting year/base year of the original TTD */
	static constexpr TimerGameCalendar::Year ORIGINAL_BASE_YEAR = 1920;
	/** The original ending year */
	static constexpr TimerGameCalendar::Year ORIGINAL_END_YEAR = 2051;
	/** The maximum year of the original TTD */
	static constexpr TimerGameCalendar::Year ORIGINAL_MAX_YEAR = 2090;

	/** The absolute minimum & maximum years in OTTD */
	static constexpr TimerGameCalendar::Year MIN_YEAR = 0;

	/** The default starting year */
	static constexpr TimerGameCalendar::Year DEF_START_YEAR = 1950;
	/** The default scoring end year */
	static constexpr TimerGameCalendar::Year DEF_END_YEAR = ORIGINAL_END_YEAR - 1;

	/**
	 * MAX_YEAR, nicely rounded value of the number of years that can
	 * be encoded in a single 32 bits date, about 2^31 / 366 years.
	 */
	static constexpr TimerGameCalendar::Year MAX_YEAR = 5000000;

	/** The date of the first day of the original base year. */
	static constexpr TimerGameCalendar::Date DAYS_TILL_ORIGINAL_BASE_YEAR = TimerGameCalendar::DateAtStartOfYear(ORIGINAL_BASE_YEAR);

	/** The absolute minimum date. */
	static constexpr TimerGameCalendar::Date MIN_DATE = 0;

	/** The date of the last day of the max year. */
	static constexpr TimerGameCalendar::Date MAX_DATE = TimerGameCalendar::DateAtStartOfYear(CalendarTime::MAX_YEAR + 1) - 1;

	static constexpr TimerGameCalendar::Year INVALID_YEAR = -1; ///< Representation of an invalid year
	static constexpr TimerGameCalendar::Date INVALID_DATE = -1; ///< Representation of an invalid date
};

#endif /* TIMER_GAME_CALENDAR_H */
