/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file date_type.h Types related to the dates in OpenTTD. */

#ifndef DATE_TYPE_H
#define DATE_TYPE_H

#include "timer/timer_game_calendar.h"

typedef int32_t  Ticks;     ///< The type to store ticks in


/**
 * 1 day is 74 ticks; TimerGameCalendar::date_fract used to be uint16_t and incremented by 885. On
 *                    an overflow the new day begun and 65535 / 885 = 74.
 * 1 tick is approximately 27 ms.
 * 1 day is thus about 2 seconds (74 * 27 = 1998) on a machine that can run OpenTTD normally
 */
static const int DAY_TICKS         =  74; ///< ticks per day
static const int DAYS_IN_YEAR      = 365; ///< days per year
static const int DAYS_IN_LEAP_YEAR = 366; ///< sometimes, you need one day more...
static const int MONTHS_IN_YEAR    =  12; ///< months per year

static const int SECONDS_PER_DAY   = 2;   ///< approximate seconds per day, not for precise calculations

static const int STATION_RATING_TICKS     = 185; ///< cycle duration for updating station rating
static const int STATION_ACCEPTANCE_TICKS = 250; ///< cycle duration for updating station acceptance
static const int STATION_LINKGRAPH_TICKS  = 504; ///< cycle duration for cleaning dead links
static const int CARGO_AGING_TICKS        = 185; ///< cycle duration for aging cargo
static const int INDUSTRY_PRODUCE_TICKS   = 256; ///< cycle duration for industry production
static const int TOWN_GROWTH_TICKS        = 70;  ///< cycle duration for towns trying to grow. (this originates from the size of the town array in TTD
static const int INDUSTRY_CUT_TREE_TICKS  = INDUSTRY_PRODUCE_TICKS * 2; ///< cycle duration for lumber mill's extra action


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

/**
 * Calculate the date of the first day of a given year.
 * @param year the year to get the first day of.
 * @return the date.
 */
static constexpr TimerGameCalendar::Date DateAtStartOfYear(TimerGameCalendar::Year year)
{
	uint number_of_leap_years = (year == 0) ? 0 : ((year - 1) / 4 - (year - 1) / 100 + (year - 1) / 400 + 1);

	return (DAYS_IN_YEAR * year) + number_of_leap_years;
}

/**
 * The date of the first day of the original base year.
 */
static constexpr TimerGameCalendar::Date DAYS_TILL_ORIGINAL_BASE_YEAR = DateAtStartOfYear(ORIGINAL_BASE_YEAR);

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

/** The date of the last day of the max year. */
static constexpr TimerGameCalendar::Date MAX_DATE = DateAtStartOfYear(MAX_YEAR + 1) - 1;

static constexpr TimerGameCalendar::Year INVALID_YEAR = -1; ///< Representation of an invalid year
static constexpr TimerGameCalendar::Date INVALID_DATE = -1; ///< Representation of an invalid date
static constexpr Ticks INVALID_TICKS = -1; ///< Representation of an invalid number of ticks

#endif /* DATE_TYPE_H */
