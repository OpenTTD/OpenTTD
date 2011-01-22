/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_date.cpp Implementation of AIDate. */

#include "../../stdafx.h"
#include "ai_date.hpp"
#include "../../date_func.h"

/* static */ int32 AIDate::GetCurrentDate()
{
	return ::_date;
}

/* static */ int32 AIDate::GetYear(int32 date)
{
	if (date < 0) return -1;

	::YearMonthDay ymd;
	::ConvertDateToYMD(date, &ymd);
	return ymd.year;
}

/* static */ int32 AIDate::GetMonth(int32 date)
{
	if (date < 0) return -1;

	::YearMonthDay ymd;
	::ConvertDateToYMD(date, &ymd);
	return ymd.month + 1;
}

/* static */ int32 AIDate::GetDayOfMonth(int32 date)
{
	if (date < 0) return -1;

	::YearMonthDay ymd;
	::ConvertDateToYMD(date, &ymd);
	return ymd.day;
}

/* static */ int32 AIDate::GetDate(int32 year, int32 month, int32 day_of_month)
{
	if (month < 1 || month > 12) return -1;
	if (day_of_month < 1 || day_of_month > 31) return -1;
	if (year < 0 || year > MAX_YEAR) return -1;

	return ::ConvertYMDToDate(year, month - 1, day_of_month);
}
