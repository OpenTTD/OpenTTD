/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_date_calendar.cpp Implementation of ScriptDateCalendar. */

#include "../../stdafx.h"
#include "script_date_calendar.hpp"
#include "../../timer/timer_game_calendar.h"

#include <time.h>

#include "../../safeguards.h"

/* static */ bool ScriptDateCalendar::IsValidDate(Date date)
{
	return date >= 0;
}

/* static */ ScriptDateCalendar::Date ScriptDateCalendar::GetCurrentDate()
{
	return (ScriptDateCalendar::Date)TimerGameCalendar::date.base();
}

/* static */ SQInteger ScriptDateCalendar::GetYear(ScriptDateCalendar::Date date)
{
	if (date < 0) return DATE_INVALID;

	::TimerGameCalendar::YearMonthDay ymd;
	::TimerGameCalendar::ConvertDateToYMD(date, &ymd);
	return ymd.year.base();
}

/* static */ SQInteger ScriptDateCalendar::GetMonth(ScriptDateCalendar::Date date)
{
	if (date < 0) return DATE_INVALID;

	::TimerGameCalendar::YearMonthDay ymd;
	::TimerGameCalendar::ConvertDateToYMD(date, &ymd);
	return ymd.month + 1;
}

/* static */ SQInteger ScriptDateCalendar::GetDayOfMonth(ScriptDateCalendar::Date date)
{
	if (date < 0) return DATE_INVALID;

	::TimerGameCalendar::YearMonthDay ymd;
	::TimerGameCalendar::ConvertDateToYMD(date, &ymd);
	return ymd.day;
}

/* static */ ScriptDateCalendar::Date ScriptDateCalendar::GetDate(SQInteger year, SQInteger month, SQInteger day_of_month)
{
	if (month < 1 || month > 12) return DATE_INVALID;
	if (day_of_month < 1 || day_of_month > 31) return DATE_INVALID;
	if (year < 0 || year > CalendarTime::MAX_YEAR) return DATE_INVALID;

	return (ScriptDateCalendar::Date)::TimerGameCalendar::ConvertYMDToDate(year, month - 1, day_of_month).base();
}
