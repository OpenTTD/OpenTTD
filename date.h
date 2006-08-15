/* $Id$ */

/**
 * 1 day is 74 ticks; _date_fract used to be uint16 and incremented by 885. On
 *                    an overflow the new day begun and 65535 / 885 = 74.
 * 1 tick is approximately 30 ms.
 * 1 day is thus about 2 seconds (74 * 30 = 2220) on a machine that can run OpenTTD normally
 */
#define DAY_TICKS 74

#define BASE_YEAR 1920
#define MIN_YEAR 1920
#define MAX_YEAR 2090

/* Year and Date are defined elsewhere */
typedef uint8  Month;
typedef uint8  Day;
typedef uint16 DateFract;

typedef struct YearMonthDay {
	Year  year;
	Month month;
	Day   day;
} YearMonthDay;

extern Year      _cur_year;
extern Month     _cur_month;
extern Date      _date;
extern DateFract _date_fract;


void SetDate(Date date);
void ConvertDayToYMD(YearMonthDay *ymd, Date date);
uint ConvertYMDToDay(Year year, Month month, Day day);
Date ConvertIntDate(uint date);
