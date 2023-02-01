/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file date.cpp Handling of dates in our native format and transforming them to something human readable. */

#include "stdafx.h"
#include "network/network.h"
#include "network/network_func.h"
#include "currency.h"
#include "window_func.h"
#include "settings_type.h"
#include "date_func.h"
#include "vehicle_base.h"
#include "rail_gui.h"
#include "linkgraph/linkgraph.h"
#include "saveload/saveload.h"
#include "newgrf_profiling.h"
#include "widgets/statusbar_widget.h"

#include "safeguards.h"

/*
 * Date/time handling is separated into two systems:
 *  - Calendar time for technology and world appearance
 *    o Vehicle introduction and obsolescence
 *    o Available airports, stations, objects, etc.
 *    o NewGRF variables which can be used to determine visual styles or behavior based on the time of year
 *    o Snow line movement
 *  - Economy time
 *
 * Calendar time is kept in a common Gregorian calendar, with 365 days in a year, leap years, variable
 * length months, and so on.
 *
 * Economy time is kept in (approximate) real time. There are logical days with a duration of 1.998 seconds,
 * logical months with a duration of 1 minute, and logical years with a duration of slightly more than 12 minutes.
 * Internally a calendar with 12 months of 30 days is observed. There are no leap periods.
 * In the user interface, if calendar progress speed is not 100%, all elements that run on economy time are presented in real time units:
 *  - Seconds (multiples of 2), for things which were originally daily:
 *  - Minutes, for things which were originally monthly:
 *    o Industry production per minute
 *    o Passengers last minute
 *    o Industry construction/close down time
 *    o Vehicle service intervals and time since last service
 *  - Periods (12-minute production periods), for things which were originally yearly:
 *    o Company finance window
 *
 * Players can choose to display units in real-time units (seconds/minutes/hours) or in the traditional game date units (days/months/years).
 * If calendar progress speed is anything but 100%, they are forced to use real-time units.
 *
 * Some dates use economy dates in either mode, while others choose the variable depending on the display mode, to account for differences in month and year length.
 * This is why it's not possible to change display modes in a running game, only when starting a new one.
 */

Year      _cur_year;   ///< Current calendar year, starting at 0
Month     _cur_month;  ///< Current calendar month (0..11)
Date      _date;       ///< Current calendar date in days (day counter)
DateFract _date_fract; ///< Fractional part of the calendar day.

Year      _cur_economy_year;   ///< Number of economy years elapsed since game start
Month     _cur_economy_month;  ///< Current economy month (0..11)
Date      _economy_date;       ///< Number of economy days elapsed since game start
DateFract _economy_date_fract; ///< Fractional part of the economy day

uint64 _tick_counter;  ///< Ever incrementing (and sometimes wrapping) tick counter for setting off various events

/**
 * Set the calendar date.
 * @param date  New date
 * @param fract The number of ticks that have passed on this date.
 */
void SetDate(Date date, DateFract fract)
{
	YearMonthDay ymd;

	_date = date;
	_date_fract = fract;
	ConvertDateToYMD(date, &ymd);
	_cur_year = ymd.year;
	_cur_month = ymd.month;
}

/**
 * Set the economy date.
 * @param date  New date
 * @param fract The number of ticks that have passed on this date.
 */
void SetEconomyDate(Date date, DateFract fract)
{
	_economy_date = date;
	_economy_date_fract = fract;

	/* If we're using real-time units, economy months have 30 days and an economy year has 360 days.
	 * Otherwise, we need to keep economy months and years in sync with the calendar. */
	if (_settings_game.economy.use_realtime_units) {
		_cur_economy_year = _economy_date / DAYS_IN_ECONOMY_YEAR;
		_cur_economy_month = _economy_date / DAYS_IN_ECONOMY_MONTH % MONTHS_IN_ECONOMY_YEAR;
	} else {
		YearMonthDay ymd;
		ConvertDateToYMD(date, &ymd);
		_cur_economy_year = ymd.year;
		_cur_economy_month = ymd.month;
	}
}

#define M(a, b) ((a << 5) | b)
static const uint16 _month_date_from_year_day[] = {
	M( 0, 1), M( 0, 2), M( 0, 3), M( 0, 4), M( 0, 5), M( 0, 6), M( 0, 7), M( 0, 8), M( 0, 9), M( 0, 10), M( 0, 11), M( 0, 12), M( 0, 13), M( 0, 14), M( 0, 15), M( 0, 16), M( 0, 17), M( 0, 18), M( 0, 19), M( 0, 20), M( 0, 21), M( 0, 22), M( 0, 23), M( 0, 24), M( 0, 25), M( 0, 26), M( 0, 27), M( 0, 28), M( 0, 29), M( 0, 30), M( 0, 31),
	M( 1, 1), M( 1, 2), M( 1, 3), M( 1, 4), M( 1, 5), M( 1, 6), M( 1, 7), M( 1, 8), M( 1, 9), M( 1, 10), M( 1, 11), M( 1, 12), M( 1, 13), M( 1, 14), M( 1, 15), M( 1, 16), M( 1, 17), M( 1, 18), M( 1, 19), M( 1, 20), M( 1, 21), M( 1, 22), M( 1, 23), M( 1, 24), M( 1, 25), M( 1, 26), M( 1, 27), M( 1, 28), M( 1, 29),
	M( 2, 1), M( 2, 2), M( 2, 3), M( 2, 4), M( 2, 5), M( 2, 6), M( 2, 7), M( 2, 8), M( 2, 9), M( 2, 10), M( 2, 11), M( 2, 12), M( 2, 13), M( 2, 14), M( 2, 15), M( 2, 16), M( 2, 17), M( 2, 18), M( 2, 19), M( 2, 20), M( 2, 21), M( 2, 22), M( 2, 23), M( 2, 24), M( 2, 25), M( 2, 26), M( 2, 27), M( 2, 28), M( 2, 29), M( 2, 30), M( 2, 31),
	M( 3, 1), M( 3, 2), M( 3, 3), M( 3, 4), M( 3, 5), M( 3, 6), M( 3, 7), M( 3, 8), M( 3, 9), M( 3, 10), M( 3, 11), M( 3, 12), M( 3, 13), M( 3, 14), M( 3, 15), M( 3, 16), M( 3, 17), M( 3, 18), M( 3, 19), M( 3, 20), M( 3, 21), M( 3, 22), M( 3, 23), M( 3, 24), M( 3, 25), M( 3, 26), M( 3, 27), M( 3, 28), M( 3, 29), M( 3, 30),
	M( 4, 1), M( 4, 2), M( 4, 3), M( 4, 4), M( 4, 5), M( 4, 6), M( 4, 7), M( 4, 8), M( 4, 9), M( 4, 10), M( 4, 11), M( 4, 12), M( 4, 13), M( 4, 14), M( 4, 15), M( 4, 16), M( 4, 17), M( 4, 18), M( 4, 19), M( 4, 20), M( 4, 21), M( 4, 22), M( 4, 23), M( 4, 24), M( 4, 25), M( 4, 26), M( 4, 27), M( 4, 28), M( 4, 29), M( 4, 30), M( 4, 31),
	M( 5, 1), M( 5, 2), M( 5, 3), M( 5, 4), M( 5, 5), M( 5, 6), M( 5, 7), M( 5, 8), M( 5, 9), M( 5, 10), M( 5, 11), M( 5, 12), M( 5, 13), M( 5, 14), M( 5, 15), M( 5, 16), M( 5, 17), M( 5, 18), M( 5, 19), M( 5, 20), M( 5, 21), M( 5, 22), M( 5, 23), M( 5, 24), M( 5, 25), M( 5, 26), M( 5, 27), M( 5, 28), M( 5, 29), M( 5, 30),
	M( 6, 1), M( 6, 2), M( 6, 3), M( 6, 4), M( 6, 5), M( 6, 6), M( 6, 7), M( 6, 8), M( 6, 9), M( 6, 10), M( 6, 11), M( 6, 12), M( 6, 13), M( 6, 14), M( 6, 15), M( 6, 16), M( 6, 17), M( 6, 18), M( 6, 19), M( 6, 20), M( 6, 21), M( 6, 22), M( 6, 23), M( 6, 24), M( 6, 25), M( 6, 26), M( 6, 27), M( 6, 28), M( 6, 29), M( 6, 30), M( 6, 31),
	M( 7, 1), M( 7, 2), M( 7, 3), M( 7, 4), M( 7, 5), M( 7, 6), M( 7, 7), M( 7, 8), M( 7, 9), M( 7, 10), M( 7, 11), M( 7, 12), M( 7, 13), M( 7, 14), M( 7, 15), M( 7, 16), M( 7, 17), M( 7, 18), M( 7, 19), M( 7, 20), M( 7, 21), M( 7, 22), M( 7, 23), M( 7, 24), M( 7, 25), M( 7, 26), M( 7, 27), M( 7, 28), M( 7, 29), M( 7, 30), M( 7, 31),
	M( 8, 1), M( 8, 2), M( 8, 3), M( 8, 4), M( 8, 5), M( 8, 6), M( 8, 7), M( 8, 8), M( 8, 9), M( 8, 10), M( 8, 11), M( 8, 12), M( 8, 13), M( 8, 14), M( 8, 15), M( 8, 16), M( 8, 17), M( 8, 18), M( 8, 19), M( 8, 20), M( 8, 21), M( 8, 22), M( 8, 23), M( 8, 24), M( 8, 25), M( 8, 26), M( 8, 27), M( 8, 28), M( 8, 29), M( 8, 30),
	M( 9, 1), M( 9, 2), M( 9, 3), M( 9, 4), M( 9, 5), M( 9, 6), M( 9, 7), M( 9, 8), M( 9, 9), M( 9, 10), M( 9, 11), M( 9, 12), M( 9, 13), M( 9, 14), M( 9, 15), M( 9, 16), M( 9, 17), M( 9, 18), M( 9, 19), M( 9, 20), M( 9, 21), M( 9, 22), M( 9, 23), M( 9, 24), M( 9, 25), M( 9, 26), M( 9, 27), M( 9, 28), M( 9, 29), M( 9, 30), M( 9, 31),
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
static const uint16 _accum_days_for_month[] = {
	ACCUM_JAN, ACCUM_FEB, ACCUM_MAR, ACCUM_APR,
	ACCUM_MAY, ACCUM_JUN, ACCUM_JUL, ACCUM_AUG,
	ACCUM_SEP, ACCUM_OCT, ACCUM_NOV, ACCUM_DEC,
};

/**
 * Converts a Date to a (Gregorian calendar) Year, Month & Day.
 * @param date the date to convert from
 * @param ymd  the year, month and day to write to
 */
void ConvertDateToYMD(Date date, YearMonthDay *ymd)
{
	/* Year determination in multiple steps to account for leap
	 * years. First do the large steps, then the smaller ones.
	 */

	/* There are 97 leap years in 400 years */
	Year yr = 400 * (date / (DAYS_IN_YEAR * 400 + 97));
	int rem = date % (DAYS_IN_YEAR * 400 + 97);
	uint16 x;

	if (rem >= DAYS_IN_YEAR * 100 + 25) {
		/* There are 25 leap years in the first 100 years after
		 * every 400th year, as every 400th year is a leap year */
		yr  += 100;
		rem -= DAYS_IN_YEAR * 100 + 25;

		/* There are 24 leap years in the next couple of 100 years */
		yr += 100 * (rem / (DAYS_IN_YEAR * 100 + 24));
		rem = (rem % (DAYS_IN_YEAR * 100 + 24));
	}

	if (!IsLeapYear(yr) && rem >= DAYS_IN_YEAR * 4) {
		/* The first 4 year of the century are not always a leap year */
		yr  += 4;
		rem -= DAYS_IN_YEAR * 4;
	}

	/* There is 1 leap year every 4 years */
	yr += 4 * (rem / (DAYS_IN_YEAR * 4 + 1));
	rem = rem % (DAYS_IN_YEAR * 4 + 1);

	/* The last (max 3) years to account for; the first one
	 * can be, but is not necessarily a leap year */
	while (rem >= (IsLeapYear(yr) ? DAYS_IN_LEAP_YEAR : DAYS_IN_YEAR)) {
		rem -= IsLeapYear(yr) ? DAYS_IN_LEAP_YEAR : DAYS_IN_YEAR;
		yr++;
	}

	/* Skip the 29th of February in non-leap years */
	if (!IsLeapYear(yr) && rem >= ACCUM_MAR - 1) rem++;

	ymd->year = yr;

	x = _month_date_from_year_day[rem];
	ymd->month = x >> 5;
	ymd->day = x & 0x1F;
}

/**
 * Converts a tuple of (Gregorian calendar) Year, Month and Day to a Date.
 * @param year  is a number between 0..MAX_YEAR
 * @param month is a number between 0..11
 * @param day   is a number between 1..31
 */
Date ConvertYMDToDate(Year year, Month month, Day day)
{
	/* Day-offset in a leap year */
	int days = _accum_days_for_month[month] + day - 1;

	/* Account for the missing of the 29th of February in non-leap years */
	if (!IsLeapYear(year) && days >= ACCUM_MAR) days--;

	return DAYS_TILL(year) + days;
}

/** Functions used by the IncreaseDate function */

extern void EnginesCalendarDailyLoop();
extern void DisasterEconomyDailyLoop();
extern void IndustryEconomyDailyLoop();

extern void CompaniesCalendarMonthlyLoop();
extern void CompaniesEconomyMonthlyLoop();
extern void EnginesCalendarMonthlyLoop();
extern void TownsEconomyMonthlyLoop();
extern void IndustryEconomyMonthlyLoop();
extern void StationEconomyMonthlyLoop();
extern void SubsidyEconomyMonthlyLoop();

extern void CompaniesEconomyYearlyLoop();
extern void VehiclesEconomyYearlyLoop();
extern void TownsEconomyYearlyLoop();

extern void ShowEndGameChart();


/** Available settings for autosave intervals. */
static const Month _autosave_months[] = {
	 0, ///< never
	 1, ///< every month/minute
	 3, ///< every 3 months/minutes
	 6, ///< every 6 months/minutes
	12, ///< every 12 months/minutes
};

/**
 * Runs various procedures that have to be done when calendar year turns
 */
static void OnNewCalendarYear()
{
	InvalidateWindowClassesData(WC_BUILD_STATION);
	InvalidateWindowClassesData(WC_BUS_STATION);
	InvalidateWindowClassesData(WC_TRUCK_STATION);

	if (_cur_year == _settings_client.gui.semaphore_build_before) ResetSignalVariant();

	/* check if we reached end of the game (end of ending year); 0 = never */
	if (_cur_year == _settings_game.game_creation.ending_year + 1 && _settings_game.game_creation.ending_year != 0) {
		ShowEndGameChart();
	}

	if (_settings_client.gui.auto_euro) CheckSwitchToEuro();

	/* If we reached the maximum year, decrement dates by a year */
	if (_cur_year == MAX_YEAR + 1) {
		_cur_year--;
		const int days_this_year = IsLeapYear(_cur_year) ? DAYS_IN_LEAP_YEAR : DAYS_IN_YEAR;
		_date -= days_this_year;
	}
}

/**
 * Runs various procedures that have to be done when economy year turns
 */
static void OnNewEconomyYear()
{
	CompaniesEconomyYearlyLoop();
	VehiclesEconomyYearlyLoop();
	TownsEconomyYearlyLoop();

	if (_network_server) NetworkServerYearlyLoop();

	/* If the maximum economy year has been reached, decrement everything by several years */
	if (_cur_economy_year == MAX_YEAR + 1) {
		/* Shift back 700 years to avoid doing this too often, and to have a number divisible by 7 (week length) */
		const int shift_back_years = 700;
		const int shift_back_days = DAYS_IN_ECONOMY_YEAR * shift_back_years;

		_economy_date -= shift_back_days;
		_cur_economy_year -= shift_back_years;

		for (Vehicle* v : Vehicle::Iterate()) v->ShiftDates(-shift_back_days);
		for (LinkGraph* lg : LinkGraph::Iterate()) lg->ShiftDates(-shift_back_days);
	}
}

/**
 * Runs various procedures that have to be done when calendar month turns
 */
static void OnNewCalendarMonth()
{
	SetWindowClassesDirty(WC_CHEATS);
	CompaniesCalendarMonthlyLoop();
	EnginesCalendarMonthlyLoop();
}

/**
 * Runs various procedures that have to be done when economy month turns
 */
static void OnNewEconomyMonth()
{
	if (_settings_client.gui.autosave != 0 && (_cur_economy_month % _autosave_months[_settings_client.gui.autosave]) == 0) {
		_do_autosave = true;
		SetWindowDirty(WC_STATUS_BAR, 0);
	}

	CompaniesEconomyMonthlyLoop();
	TownsEconomyMonthlyLoop();
	IndustryEconomyMonthlyLoop();
	SubsidyEconomyMonthlyLoop();
	StationEconomyMonthlyLoop();

	if (_network_server) NetworkServerEconomyMonthlyLoop();
}

/**
 * Runs various procedures that have to be done every calendar day
 */
static void OnNewCalendarDay()
{
	if (!_newgrf_profilers.empty() && _newgrf_profile_end_date <= _date) {
		NewGRFProfiler::FinishAll();
	}


	SetWindowWidgetDirty(WC_STATUS_BAR, 0, WID_S_LEFT);
	EnginesCalendarDailyLoop();

	/* Refresh after possible snowline change */
	SetWindowClassesDirty(WC_TOWN_VIEW);
}

/**
 * Runs various procedures that have to be done every economy day
 */
static void OnNewEconomyDay()
{
	if (_network_server) NetworkServerDailyLoop();

	DisasterEconomyDailyLoop();
	IndustryEconomyDailyLoop();
}

static void IncreaseCalendarDate()
{
	/* If calendar day progress is frozen, don't try to advance the date (particularly since this would divide by 0). */
	if (_settings_game.economy.calendar_progress_speed == FROZEN_CALENDAR_PROGRESS_SPEED) return;

	/* Scale calendar day progression by the chosen speed percentage,
	   then bail out if it's not time to increase the calendar date yet. */
	if (++_date_fract < (DAY_TICKS * 100) / _settings_game.economy.calendar_progress_speed) return;

	/* Reset fract counter and increase day counter */
	_date_fract = 0;
	_date++;

	YearMonthDay ymd;
	ConvertDateToYMD(_date, &ymd);

	/* check if we entered a new month? */
	bool new_month = ymd.month != _cur_month;

	/* check if we entered a new year? */
	bool new_year = ymd.year != _cur_year;

	/* update internal variables before calling the daily/monthly/yearly loops */
	_cur_month = ymd.month;
	_cur_year  = ymd.year;

	/* yes, call various daily loops */
	OnNewCalendarDay();

	/* yes, call various monthly loops */
	if (new_month) OnNewCalendarMonth();

	/* yes, call various yearly loops */
	if (new_year) OnNewCalendarYear();
}

static void IncreaseEconomyDate()
{
	_economy_date_fract++;
	if (_economy_date_fract < DAY_TICKS) return;
	_economy_date_fract = 0;

	_economy_date++;

	bool new_month;
	bool new_year;

	/* If we're using real-time units, economy months have 30 days and an economy year has 360 days.
	 * Otherwise, we need to keep economy months and years in sync with the calendar. */
	if (_settings_game.economy.use_realtime_units) {
		new_month = _economy_date % DAYS_IN_ECONOMY_MONTH == 0;
		new_year = _economy_date % DAYS_IN_ECONOMY_YEAR == 0;
	} else {
		YearMonthDay ymd;
		ConvertDateToYMD(_economy_date, &ymd);

		new_month = ymd.month != _cur_economy_month;
		new_year = ymd.year != _cur_economy_year;
	}

	if (new_year) {
		_cur_economy_month = 0;
		_cur_economy_year++;
	} else if (new_month) {
		_cur_economy_month++;
	}

	OnNewEconomyDay();

	if (new_month) OnNewEconomyMonth();

	if (new_year) OnNewEconomyYear();
}

/**
 * Increases the tick counter and both types of dates (economic and calendar).
 */
void IncreaseDate()
{
	_tick_counter++;

	/* Time does not advance in the main menu. */
	if (_game_mode == GM_MENU) return;

	IncreaseCalendarDate();
	IncreaseEconomyDate();
}
