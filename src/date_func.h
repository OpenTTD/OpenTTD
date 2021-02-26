/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file date_func.h Functions related to dates. */

#ifndef DATE_FUNC_H
#define DATE_FUNC_H

#include "date_type.h"

extern Year      _cur_year;
extern Month     _cur_month;
extern Date      _date;
extern DateFract _date_fract;
extern uint16    _tick_counter;
extern GameYear  _game_year;
extern bool      _year_is_looping;

bool YearIsLooping();
void SetDate(Date date, DateFract fract);
void ConvertDateToYMD(Date date, YearMonthDay *ymd);
Date ConvertYMDToDate(Year year, Month month, Day day);

/**
 * Checks whether the given year is a leap year or not.
 * @param yr The year to check.
 * @return True if \c yr is a leap year, otherwise false.
 */
static inline bool IsLeapYear(Year yr)
{
	return yr % 4 == 0 && (yr % 100 != 0 || yr % 400 == 0);
}

#endif /* DATE_FUNC_H */
