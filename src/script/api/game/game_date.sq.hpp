/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

#include "../script_date.hpp"
#include "../template/template_date.sq.hpp"

template <> SQInteger PushClassName<ScriptEconomyDate, ScriptType::GS>(HSQUIRRELVM vm) { sq_pushstring(vm, "GSEconomyDate"); return 1; }
template <> SQInteger PushClassName<ScriptCalendarDate, ScriptType::GS>(HSQUIRRELVM vm) { sq_pushstring(vm, "GSCalendarDate"); return 1; }

void SQGSDate_Register(Squirrel &engine)
{
	DefSQClass<ScriptEconomyDate, ScriptType::GS> SQGSEconomyDate("GSEconomyDate");
	SQGSEconomyDate.PreRegister(engine, "GSObject");
	SQGSEconomyDate.AddSQAdvancedConstructor(engine);

	SQGSEconomyDate.DefSQConst(engine, ScriptEconomyDate::DATE_INVALID, "DATE_INVALID");

	SQGSEconomyDate.DefSQStaticMethod(engine, &ScriptEconomyDate::GetCurrentDate, "GetCurrentDate", ".");
	SQGSEconomyDate.DefSQStaticMethod(engine, &ScriptEconomyDate::GetDate,        "GetDate",        ".iii");
	SQGSEconomyDate.DefSQStaticMethod(engine, &ScriptEconomyDate::GetSystemTime,  "GetSystemTime",  ".");

	SQGSEconomyDate.DefSQMethod(engine, &ScriptEconomyDate::IsValid,        "IsValid",       ".");
	SQGSEconomyDate.DefSQMethod(engine, &ScriptEconomyDate::GetYear,        "GetYear",       ".");
	SQGSEconomyDate.DefSQMethod(engine, &ScriptEconomyDate::GetMonth,       "GetMonth",      ".");
	SQGSEconomyDate.DefSQMethod(engine, &ScriptEconomyDate::GetDayOfMonth,  "GetDayOfMonth", ".");
	SQGSEconomyDate.DefSQMethod(engine, &ScriptEconomyDate::date,           "date",          ".");

	SQGSEconomyDate.DefSQAdvancedMethod(engine, &ScriptEconomyDate::_tostring, "_tostring");
	SQGSEconomyDate.DefSQAdvancedMethod(engine, &ScriptEconomyDate::_add,      "_add");
	SQGSEconomyDate.DefSQAdvancedMethod(engine, &ScriptEconomyDate::_sub,      "_sub");
	SQGSEconomyDate.DefSQAdvancedMethod(engine, &ScriptEconomyDate::_cmp,      "_cmp");

	SQGSEconomyDate.PostRegister(engine);


	DefSQClass<ScriptCalendarDate, ScriptType::GS> SQGSCalendarDate("GSCalendarDate");
	SQGSCalendarDate.PreRegister(engine, "GSObject");
	SQGSCalendarDate.AddSQAdvancedConstructor(engine);

	SQGSCalendarDate.DefSQConst(engine, ScriptCalendarDate::DATE_INVALID, "DATE_INVALID");

	SQGSCalendarDate.DefSQStaticMethod(engine, &ScriptCalendarDate::GetCurrentDate, "GetCurrentDate", ".");
	SQGSCalendarDate.DefSQStaticMethod(engine, &ScriptCalendarDate::GetDate,        "GetDate",        ".iii");
	SQGSCalendarDate.DefSQStaticMethod(engine, &ScriptCalendarDate::GetSystemTime,  "GetSystemTime",  ".");

	SQGSCalendarDate.DefSQMethod(engine, &ScriptCalendarDate::IsValid,        "IsValid",       ".");
	SQGSCalendarDate.DefSQMethod(engine, &ScriptCalendarDate::GetYear,        "GetYear",       ".");
	SQGSCalendarDate.DefSQMethod(engine, &ScriptCalendarDate::GetMonth,       "GetMonth",      ".");
	SQGSCalendarDate.DefSQMethod(engine, &ScriptCalendarDate::GetDayOfMonth,  "GetDayOfMonth", ".");
	SQGSCalendarDate.DefSQMethod(engine, &ScriptCalendarDate::date,           "date",          ".");

	SQGSCalendarDate.DefSQAdvancedMethod(engine, &ScriptCalendarDate::_tostring, "_tostring");
	SQGSCalendarDate.DefSQAdvancedMethod(engine, &ScriptCalendarDate::_add,      "_add");
	SQGSCalendarDate.DefSQAdvancedMethod(engine, &ScriptCalendarDate::_sub,      "_sub");
	SQGSCalendarDate.DefSQAdvancedMethod(engine, &ScriptCalendarDate::_cmp,      "_cmp");

	SQGSCalendarDate.PostRegister(engine);
}
