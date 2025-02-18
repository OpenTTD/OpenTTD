/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_date.cpp Implementation of ScriptDate. */

#include "../../stdafx.h"
#include "script_date.hpp"
#include "../../timer/timer_game_economy.h"
#include "../../timer/timer_game_calendar.h"
#include "../squirrel_helper.hpp"
#include "template/template_date.hpp.sq"

#include <time.h>

#include "../../safeguards.h"

ScriptDate::ScriptDate(Date date, DateType type) : date(date), type(type) {};

bool ScriptDate::IsValid(DateType type)
{
	return this->type == type && this->date >= 0;
}

/* static */ ScriptDate *ScriptDate::GetCurrentDate(bool calendar)
{
	if (calendar) return new ScriptDate(static_cast<Date>(TimerGameCalendar::date.base()), DT_CALENDAR);
	return new ScriptDate(static_cast<Date>(TimerGameEconomy::date.base()), DT_ECONOMY);
}

SQInteger ScriptDate::GetYear()
{
	if (!this->IsValid(this->type)) return DATE_INVALID;

	switch (this->type) {
		case DT_ECONOMY: {
			::TimerGameEconomy::YearMonthDay ymd = ::TimerGameEconomy::ConvertDateToYMD(::TimerGameEconomy::Date{ this->date });
			return ymd.year.base();
		}

		case DT_CALENDAR: {
			::TimerGameCalendar::YearMonthDay ymd = ::TimerGameCalendar::ConvertDateToYMD(::TimerGameCalendar::Date{ this->date });
			return ymd.year.base();
		}

		default:
			return DATE_INVALID;
	}
}

SQInteger ScriptDate::GetMonth()
{
	if (!this->IsValid(this->type)) return DATE_INVALID;

	switch (this->type) {
		case DT_ECONOMY: {
			::TimerGameEconomy::YearMonthDay ymd = ::TimerGameEconomy::ConvertDateToYMD(::TimerGameEconomy::Date{ this->date });
			return ymd.month + 1;
		}

		case DT_CALENDAR: {
			::TimerGameCalendar::YearMonthDay ymd = ::TimerGameCalendar::ConvertDateToYMD(::TimerGameCalendar::Date{ this->date });
			return ymd.month + 1;
		}

		default:
			return DATE_INVALID;
	}
}

SQInteger ScriptDate::GetDayOfMonth()
{
	if (!this->IsValid(this->type)) return DATE_INVALID;

	switch (this->type) {
		case DT_ECONOMY: {
			::TimerGameEconomy::YearMonthDay ymd = ::TimerGameEconomy::ConvertDateToYMD(::TimerGameEconomy::Date{ this->date });
			return ymd.day;
		}

		case DT_CALENDAR: {
			::TimerGameCalendar::YearMonthDay ymd = ::TimerGameCalendar::ConvertDateToYMD(::TimerGameCalendar::Date{ this->date });
			return ymd.day;
		}

		default:
			return DATE_INVALID;
	}
}

/* static */ ScriptDate *ScriptDate::GetDate(SQInteger year, SQInteger month, SQInteger day_of_month, bool calendar)
{
	DateType type = calendar ? DT_CALENDAR : DT_ECONOMY;
	if (month < 1 || month > 12) return new ScriptDate(DATE_INVALID, type);
	if (day_of_month < 1 || day_of_month > 31) return new ScriptDate(DATE_INVALID, type);

	switch (type) {
		case DT_ECONOMY: {
			::TimerGameEconomy::Year timer_year{ClampTo<int32_t>(year)};
			if (timer_year < EconomyTime::MIN_YEAR || timer_year > EconomyTime::MAX_YEAR) return new ScriptDate(DATE_INVALID, type);

			return new ScriptDate(static_cast<Date>(::TimerGameEconomy::ConvertYMDToDate(timer_year, month - 1, day_of_month).base()), type);
		}

		case DT_CALENDAR: {
			::TimerGameCalendar::Year timer_year{ClampTo<int32_t>(year)};
			if (timer_year < CalendarTime::MIN_YEAR || timer_year > CalendarTime::MAX_YEAR) return new ScriptDate(DATE_INVALID, type);

			return new ScriptDate(static_cast<Date>(::TimerGameCalendar::ConvertYMDToDate(timer_year, month - 1, day_of_month).base()), type);
		}

		default: NOT_REACHED();
	}
}

/* static */ ScriptDate *ScriptDate::GetSystemTime()
{
	time_t t;
	time(&t);
	return new ScriptDate(static_cast<Date>(t), DT_SYSTEM);
}

ScriptDate::Date ScriptDate::GetValue()
{
	return this->date;
}

SQInteger ScriptDate::_tostring(HSQUIRRELVM vm)
{
	sq_pushstring(vm, fmt::format("{} {}", this->date, "ECS"[this->type]));
	return 1;
}

/* Safer version of SQConvert::Param<ScriptDate *>::Get(HSQUIRRELVM vm, SQInteger idx). */
static ScriptDate *GetParam(HSQUIRRELVM vm, SQInteger idx)
{
	SQUserPointer instance;
	if (SQ_FAILED(sq_getinstanceup(vm, idx, &instance, nullptr))) return nullptr;
	return static_cast<ScriptDate *>(instance);
}

SQInteger ScriptDate::_add(HSQUIRRELVM vm)
{
	ScriptDate *other = GetParam(vm, 2);
	if (other == nullptr || other->type != this->type) return SQ_ERROR;
	ScriptDate *res = new ScriptDate(static_cast<Date>(this->date + other->date), this->type);
	return SQConvert::Return<ScriptDate *>::Set(vm, res);
}

SQInteger ScriptDate::_sub(HSQUIRRELVM vm)
{
	ScriptDate *other = GetParam(vm, 2);
	if (other == nullptr || other->type != this->type) return SQ_ERROR;
	ScriptDate *res = new ScriptDate(static_cast<Date>(this->date - other->date), this->type);
	return SQConvert::Return<ScriptDate *>::Set(vm, res);
}

SQInteger ScriptDate::_cmp(HSQUIRRELVM vm)
{
	ScriptDate *other = GetParam(vm, 2);
	if (other == nullptr || other->type != this->type) return SQ_ERROR;
	sq_pushinteger(vm, this->date - other->date);
	return 1;
}
