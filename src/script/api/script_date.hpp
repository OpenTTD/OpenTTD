/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_date.hpp Everything to query and manipulate date related information. */

#ifndef SCRIPT_DATE_HPP
#define SCRIPT_DATE_HPP

#include "script_object.hpp"
#include "../../timer/timer_game_economy.h"
#include "../squirrel_helper.hpp"

#ifndef DOXYGEN_API
template <size_t N>
struct StringLiteral {
	constexpr StringLiteral(const char (&str)[N])
	{
		std::copy_n(str, N, this->value);
	}

	char value[N];
};

template <class Timer, class Time, StringLiteral Tag>
class ScriptDateBase : public ScriptObject {
public:
	using YearMonthDay = Timer::YearMonthDay;
	using Year = Timer::Year;

	enum Date {
		DATE_INVALID = Time::INVALID_DATE.base(),
	};

	ScriptDateBase(Date date = DATE_INVALID) : _date(date) {}

	bool IsValid()
	{
		return this->_date > 0;
	}

	static ScriptDateBase *GetCurrentDate()
	{
		return new ScriptDateBase(static_cast<Date>(Timer::date.base()));
	}

	SQInteger GetYear()
	{
		if (!this->IsValid()) return DATE_INVALID;

		YearMonthDay ymd = Timer::ConvertDateToYMD(static_cast<Timer::Date>(this->_date));
		return ymd.year.base();
	}

	SQInteger GetMonth()
	{
		if (!this->IsValid()) return DATE_INVALID;

		YearMonthDay ymd = Timer::ConvertDateToYMD(static_cast<Timer::Date>(this->_date));
		return ymd.month + 1;
	}

	SQInteger GetDayOfMonth()
	{
		if (!this->IsValid()) return DATE_INVALID;

		YearMonthDay ymd = Timer::ConvertDateToYMD(static_cast<Timer::Date>(this->_date));
		return ymd.day;
	}

	static ScriptDateBase *GetDate(SQInteger year, SQInteger month, SQInteger day_of_month)
	{
		if (month < 1 || month > 12) return new ScriptDateBase();
		if (day_of_month < 1 || day_of_month > 31) return new ScriptDateBase();

		Year timer_year{ClampTo<int32_t>(year)};
		if (timer_year < Time::MIN_YEAR || timer_year > Time::MAX_YEAR) return new ScriptDateBase();

		return new ScriptDateBase(static_cast<Date>(Timer::ConvertYMDToDate(timer_year, month - 1, day_of_month).base()));
	}

	static SQInteger GetSystemTime()
	{
		time_t t;
		time(&t);
		return t;
	}

	Date date() { return this->_date; }

	SQInteger _tostring(HSQUIRRELVM vm)
	{
		sq_pushstring(vm, fmt::format("{}", this->_date));
		return 1;
	}

	SQInteger _add(HSQUIRRELVM vm)
	{
		SQInteger days = 0;
		switch (sq_gettype(vm, 2)) {
			case OT_INTEGER:
				sq_getinteger(vm, 2, &days);
				break;
			case OT_INSTANCE: {
				ScriptDateBase *other = static_cast<ScriptDateBase *>(Squirrel::GetRealInstance(vm, 2, Tag.value));
				if (other == nullptr) return SQ_ERROR;
				days = other->_date;
				break;
			}
			default: return SQ_ERROR;
		}
		ScriptDateBase *res = new ScriptDateBase(static_cast<Date>(this->_date + days));
		res->AddRef();
		Squirrel::CreateClassInstanceVM(vm, Tag.value, res, nullptr, SQConvert::DefSQDestructorCallback<ScriptDateBase>, true);
		return 1;
	}

	SQInteger _sub(HSQUIRRELVM vm)
	{
		SQInteger days = 0;
		switch (sq_gettype(vm, 2)) {
			case OT_INTEGER:
				sq_getinteger(vm, 2, &days);
				break;
			case OT_INSTANCE: {
					ScriptDateBase *other = static_cast<ScriptDateBase *>(Squirrel::GetRealInstance(vm, 2, Tag.value));
					if (other == nullptr) return SQ_ERROR;
					days = other->_date;
					break;
			}
			default: return SQ_ERROR;
		}
		ScriptDateBase *res = new ScriptDateBase(static_cast<Date>(this->_date - days));
		res->AddRef();
		Squirrel::CreateClassInstanceVM(vm, Tag.value, res, nullptr, SQConvert::DefSQDestructorCallback<ScriptDateBase>, true);
		return 1;
	}

	SQInteger _cmp(HSQUIRRELVM vm)
	{
		ScriptDateBase *other = static_cast<ScriptDateBase *>(Squirrel::GetRealInstance(vm, 2, Tag.value));
		if (other == nullptr) return SQ_ERROR;
		sq_pushinteger(vm, this->_date - other->_date);
		return 1;
	}

protected:
	Date _date = DATE_INVALID;
};

using ScriptEconomyDate = ScriptDateBase<TimerGameEconomy, EconomyTime, "EconomyDate">;
using ScriptCalendarDate = ScriptDateBase<TimerGameCalendar, CalendarTime, "CalendarDate">;
#else
/**
 * Class that handles all date related (calculation) functions.
 * @api ai game
 *
 * @note Months and days of month are 1-based; the first month of the
 *       year is 1 and the first day of the month is also 1.
 * @note Years are zero based; they start with the year 0.
 * @note Dates can be used to determine the number of days between
 *       two different moments in time because they count the number
 *       of days since the year 0.
 *
 * \anchor ScriptCalendarTime
 * \b Calendar-Time
 *
 * Calendar time measures the technological progression in the game.
 * \li The calendar date is shown in the status bar.
 * \li The calendar date affects engine model introduction and expiration.
 * \li Progression of calendar time can be slowed or even halted via game settings.
 *
 * Calendar time uses the Gregorian calendar with 365 or 366 days per year.
 *
 * \anchor ScriptEconomyTime
 * \b Economy-Time
 *
 * Economy time measures the in-game time progression, while the game is not paused.
 * \li Cargo production and consumption follows economy time.
 * \li Recurring income and expenses follow economy time.
 * \li Production and income statistics and balances are created per economy month/quarter/year.
 *
 * Depending on game settings economy time is represented differently:
 * \li Calendar-based timekeeping: Economy- and calendar-time use the identical Gregorian calendar.
 * \li Wallclock-based timekeeping: Economy- and calendar-time are separate.
 *     Economy-time will use a 360 day calendar (12 months with 30 days each), which runs at a constant speed of one economy-month per realtime-minute.
 *     Calendar-time will use a Gregorian calendar, which can be slowed to stopped via game settings.
 */
class ScriptEconomyDate : public ScriptObject {
public:
	/**
	* Date data type is an integer value. Use ScriptEconomyDate::GetDate to
	* compose valid date values for a known year, month and day.
	*/
	enum Date {
		DATE_INVALID, ///< A value representing an invalid date.
	};

	ScriptEconomyDate(Date date = DATE_INVALID);

	/**
	* Validates if a date value represent a valid date.
	* @return True if the date is valid, otherwise false
	*/
	bool IsValid();

	/**
	* Get the current date.
	* This is the number of days since epoch under the assumption that
	*  there is a leap year every 4 years, except when dividable by
	*  100 but not by 400.
	* @return The current date.
	*/
	static ScriptEconomyDate GetCurrentDate();

	/**
	* Get the year of the given date.
	* @return The year.
	*/
	SQInteger GetYear();

	/**
	* Get the month of the given date.
	* @return The month.
	*/
	SQInteger GetMonth();

	/**
	* Get the day (of the month) of the given date.
	* @return The day.
	*/
	SQInteger GetDayOfMonth();

	/**
	* Get the date given a year, month and day of month.
	* @param year The year of the to-be determined date.
	* @param month The month of the to-be determined date.
	* @param day_of_month The day of month of the to-be determined date.
	* @return The date.
	*/
	static ScriptEconomyDate GetDate(SQInteger year, SQInteger month, SQInteger day_of_month);

	/**
	* Get the time of the host system.
	* @return The amount of seconds passed since 1 Jan 1970.
	* @api -ai
	* @note This uses the clock of the host system, which can skew or be set back. Use with caution.
	*/
	static SQInteger GetSystemTime();

	/**
	* Get the date.
	* @return The date.
	*/
	Date date();
};



class ScriptCalendarDate : public ScriptObject {
public:
	/**
	* Date data type is an integer value. Use ScriptCalendarDate::GetDate to
	* compose valid date values for a known year, month and day.
	*/
	enum Date {
		DATE_INVALID, ///< A value representing an invalid date.
	};

	ScriptCalendarDate(Date date = DATE_INVALID);

	/**
	* Validates if a date value represent a valid date.
	* @return True if the date is valid, otherwise false
	*/
	bool IsValid();

	/**
	* Get the current date.
	* This is the number of days since epoch under the assumption that
	*  there is a leap year every 4 years, except when dividable by
	*  100 but not by 400.
	* @return The current date.
	*/
	static ScriptCalendarDate GetCurrentDate();

	/**
	* Get the year of the given date.
	* @return The year.
	*/
	SQInteger GetYear();

	/**
	* Get the month of the given date.
	* @return The month.
	*/
	SQInteger GetMonth();

	/**
	* Get the day (of the month) of the given date.
	* @return The day.
	*/
	SQInteger GetDayOfMonth();

	/**
	* Get the date given a year, month and day of month.
	* @param year The year of the to-be determined date.
	* @param month The month of the to-be determined date.
	* @param day_of_month The day of month of the to-be determined date.
	* @return The date.
	*/
	static ScriptCalendarDate GetDate(SQInteger year, SQInteger month, SQInteger day_of_month);

	/**
	* Get the time of the host system.
	* @return The amount of seconds passed since 1 Jan 1970.
	* @api -ai
	* @note This uses the clock of the host system, which can skew or be set back. Use with caution.
	*/
	static SQInteger GetSystemTime();

	/**
	* Get the date.
	* @return The date.
	*/
	Date date();
};
#endif /* DOXYGEN_API */

#endif /* SCRIPT_DATE_HPP */
