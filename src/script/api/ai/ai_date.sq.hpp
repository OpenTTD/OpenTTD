/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

#include "../script_date.hpp"
#include "../template/template_date.sq.hpp"


template <> SQInteger PushClassName<ScriptEconomyDate, ScriptType::AI>(HSQUIRRELVM vm) { sq_pushstring(vm, "AIEconomyDate"); return 1; }
template <> SQInteger PushClassName<ScriptCalendarDate, ScriptType::AI>(HSQUIRRELVM vm) { sq_pushstring(vm, "AICalendarDate"); return 1; }

void SQAIDate_Register(Squirrel &engine)
{
	DefSQClass<ScriptEconomyDate, ScriptType::AI> SQAIEconomyDate("AIEconomyDate");
	SQAIEconomyDate.PreRegister(engine, "AIObject");
	SQAIEconomyDate.AddSQAdvancedConstructor(engine);

	SQAIEconomyDate.DefSQConst(engine, ScriptEconomyDate::DATE_INVALID, "DATE_INVALID");

	SQAIEconomyDate.DefSQStaticMethod(engine, &ScriptEconomyDate::GetCurrentDate, "GetCurrentDate", ".");
	SQAIEconomyDate.DefSQStaticMethod(engine, &ScriptEconomyDate::GetDate,        "GetDate",        ".iii");

	SQAIEconomyDate.DefSQMethod(engine, &ScriptEconomyDate::IsValid,        "IsValid",       ".");
	SQAIEconomyDate.DefSQMethod(engine, &ScriptEconomyDate::GetYear,        "GetYear",       ".");
	SQAIEconomyDate.DefSQMethod(engine, &ScriptEconomyDate::GetMonth,       "GetMonth",      ".");
	SQAIEconomyDate.DefSQMethod(engine, &ScriptEconomyDate::GetDayOfMonth,  "GetDayOfMonth", ".");
	SQAIEconomyDate.DefSQMethod(engine, &ScriptEconomyDate::date,           "date",          ".");

	SQAIEconomyDate.DefSQAdvancedMethod(engine, &ScriptEconomyDate::_tostring, "_tostring");
	SQAIEconomyDate.DefSQAdvancedMethod(engine, &ScriptEconomyDate::_add,      "_add");
	SQAIEconomyDate.DefSQAdvancedMethod(engine, &ScriptEconomyDate::_sub,      "_sub");
	SQAIEconomyDate.DefSQAdvancedMethod(engine, &ScriptEconomyDate::_cmp,      "_cmp");

	SQAIEconomyDate.PostRegister(engine);


	DefSQClass<ScriptCalendarDate, ScriptType::AI> SQAICalendarDate("AICalendarDate");
	SQAICalendarDate.PreRegister(engine, "AIObject");
	SQAICalendarDate.AddSQAdvancedConstructor(engine);

	SQAICalendarDate.DefSQConst(engine, ScriptCalendarDate::DATE_INVALID, "DATE_INVALID");

	SQAICalendarDate.DefSQStaticMethod(engine, &ScriptCalendarDate::GetCurrentDate, "GetCurrentDate", ".");
	SQAICalendarDate.DefSQStaticMethod(engine, &ScriptCalendarDate::GetDate,        "GetDate",        ".iii");

	SQAICalendarDate.DefSQMethod(engine, &ScriptCalendarDate::IsValid,        "IsValid",       ".");
	SQAICalendarDate.DefSQMethod(engine, &ScriptCalendarDate::GetYear,        "GetYear",       ".");
	SQAICalendarDate.DefSQMethod(engine, &ScriptCalendarDate::GetMonth,       "GetMonth",      ".");
	SQAICalendarDate.DefSQMethod(engine, &ScriptCalendarDate::GetDayOfMonth,  "GetDayOfMonth", ".");
	SQAICalendarDate.DefSQMethod(engine, &ScriptCalendarDate::date,           "date",          ".");

	SQAICalendarDate.DefSQAdvancedMethod(engine, &ScriptCalendarDate::_tostring, "_tostring");
	SQAICalendarDate.DefSQAdvancedMethod(engine, &ScriptCalendarDate::_add,      "_add");
	SQAICalendarDate.DefSQAdvancedMethod(engine, &ScriptCalendarDate::_sub,      "_sub");
	SQAICalendarDate.DefSQAdvancedMethod(engine, &ScriptCalendarDate::_cmp,      "_cmp");

	SQAICalendarDate.PostRegister(engine);

}
