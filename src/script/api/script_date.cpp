/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_date.cpp Implementation of ScriptDate. */

#include "../../stdafx.h"
#include "script_date.hpp"
#include "../../timer/timer_game_calendar.h"

#include <time.h>

#include "../../safeguards.h"

/* static */ bool ScriptDate::IsValidDate(Date date)
{
	return date >= 0;
}

/* static */ ScriptDate::Date ScriptDate::GetCurrentDate()
{
	return (ScriptDate::Date)TimerGameCalendar::date.base();
}

/* static */ SQInteger ScriptDate::GetYear(ScriptDate::Date date)
{
	if (date < 0) return DATE_INVALID;

	::TimerGameCalendar::YearMonthDay ymd = ::TimerGameCalendar::ConvertDateToYMD(date);
	return ymd.year.base();
}

/* static */ SQInteger ScriptDate::GetMonth(ScriptDate::Date date)
{
	if (date < 0) return DATE_INVALID;

	::TimerGameCalendar::YearMonthDay ymd = ::TimerGameCalendar::ConvertDateToYMD(date);
	return ymd.month + 1;
}

/* static */ SQInteger ScriptDate::GetDayOfMonth(ScriptDate::Date date)
{
	if (date < 0) return DATE_INVALID;

	::TimerGameCalendar::YearMonthDay ymd = ::TimerGameCalendar::ConvertDateToYMD(date);
	return ymd.day;
}

/* static */ ScriptDate::Date ScriptDate::GetDate(SQInteger year, SQInteger month, SQInteger day_of_month)
{
	if (month < 1 || month > 12) return DATE_INVALID;
	if (day_of_month < 1 || day_of_month > 31) return DATE_INVALID;
	if (year < 0 || year > CalendarTime::MAX_YEAR) return DATE_INVALID;

	return (ScriptDate::Date)::TimerGameCalendar::ConvertYMDToDate(year, month - 1, day_of_month).base();
}

/* static */ SQInteger ScriptDate::GetSystemTime()
{
	time_t t;
	time(&t);
	return t;
}
