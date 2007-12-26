/* $Id$ */

/** @file date_type.h Types related to the dates in OpenTTD. */

#ifndef DATE_TYPE_H
#define DATE_TYPE_H

/**
 * 1 day is 74 ticks; _date_fract used to be uint16 and incremented by 885. On
 *                    an overflow the new day begun and 65535 / 885 = 74.
 * 1 tick is approximately 30 ms.
 * 1 day is thus about 2 seconds (74 * 30 = 2220) on a machine that can run OpenTTD normally
 */
#define DAY_TICKS 74

/*
 * ORIGINAL_BASE_YEAR, ORIGINAL_MAX_YEAR and DAYS_TILL_ORIGINAL_BASE_YEAR are
 * primarily used for loading newgrf and savegame data and returning some
 * newgrf (callback) functions that were in the original (TTD) inherited
 * format, where '_date == 0' meant that it was 1920-01-01.
 */

/** The minimum starting year/base year of the original TTD */
#define ORIGINAL_BASE_YEAR 1920
/** The maximum year of the original TTD */
#define ORIGINAL_MAX_YEAR 2090

/**
 * The offset in days from the '_date == 0' till
 * 'ConvertYMDToDate(ORIGINAL_BASE_YEAR, 0, 1)'
 */
#define DAYS_TILL_ORIGINAL_BASE_YEAR (365 * ORIGINAL_BASE_YEAR + ORIGINAL_BASE_YEAR / 4 - ORIGINAL_BASE_YEAR / 100 + ORIGINAL_BASE_YEAR / 400)

/* The absolute minimum & maximum years in OTTD */
#define MIN_YEAR 0
/* MAX_YEAR, nicely rounded value of the number of years that can
 * be encoded in a single 32 bits date, about 2^31 / 366 years. */
#define MAX_YEAR 5000000

typedef int32  Date;
typedef uint16 DateFract;

typedef int32  Year;
typedef uint8  Month;
typedef uint8  Day;

struct YearMonthDay {
	Year  year;
	Month month;
	Day   day;
};

static const Year INVALID_YEAR = -1;
static const Date INVALID_DATE = -1;

#endif /* DATE_TYPE_H */
