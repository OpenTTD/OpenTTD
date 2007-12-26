/* $Id$ */

/** @file date_func.h Functions related to dates. */

#ifndef DATE_FUNC_H
#define DATE_FUNC_H

#include "date_type.h"

extern Year      _cur_year;
extern Month     _cur_month;
extern Date      _date;
extern DateFract _date_fract;

void SetDate(Date date);
void ConvertDateToYMD(Date date, YearMonthDay *ymd);
Date ConvertYMDToDate(Year year, Month month, Day day);

#endif /* DATE_FUNC_H */
