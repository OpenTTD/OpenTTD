/* $Id$ */

/** @file ai_date.cpp Implementation of AIDate. */

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
