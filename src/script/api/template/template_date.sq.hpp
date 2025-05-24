/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

#include "../script_date.hpp"

namespace SQConvert {
	/* Allow ScriptEconomyDate to be used as Squirrel parameter */
	template <> struct Param<ScriptEconomyDate *> { static inline ScriptEconomyDate *Get(HSQUIRRELVM vm, int index) { return  static_cast<ScriptEconomyDate *>(sq_gettype(vm, index) == OT_NULL ? nullptr : Squirrel::GetRealInstance(vm, index, "EconomyDate")); } };
	template <> struct Param<ScriptEconomyDate &> { static inline ScriptEconomyDate &Get(HSQUIRRELVM vm, int index) { return *static_cast<ScriptEconomyDate *>(Squirrel::GetRealInstance(vm, index, "EconomyDate")); } };
	template <> struct Param<const ScriptEconomyDate *> { static inline const ScriptEconomyDate *Get(HSQUIRRELVM vm, int index) { return  static_cast<ScriptEconomyDate *>(sq_gettype(vm, index) == OT_NULL ? nullptr : Squirrel::GetRealInstance(vm, index, "EconomyDate")); } };
	template <> struct Param<const ScriptEconomyDate &> { static inline const ScriptEconomyDate &Get(HSQUIRRELVM vm, int index) { return *static_cast<ScriptEconomyDate *>(Squirrel::GetRealInstance(vm, index, "EconomyDate")); } };
	template <> struct Return<ScriptEconomyDate *> { static inline int Set(HSQUIRRELVM vm, ScriptEconomyDate *res) { if (res == nullptr) { sq_pushnull(vm); return 1; } res->AddRef(); Squirrel::CreateClassInstanceVM(vm, "EconomyDate", res, nullptr, DefSQDestructorCallback<ScriptEconomyDate>, true); return 1; } };

	/* Allow ScriptCalendarDate to be used as Squirrel parameter */
	template <> struct Param<ScriptCalendarDate *> { static inline ScriptCalendarDate *Get(HSQUIRRELVM vm, int index) { return  static_cast<ScriptCalendarDate *>(sq_gettype(vm, index) == OT_NULL ? nullptr : Squirrel::GetRealInstance(vm, index, "CalendarDate")); } };
	template <> struct Param<ScriptCalendarDate &> { static inline ScriptCalendarDate &Get(HSQUIRRELVM vm, int index) { return *static_cast<ScriptCalendarDate *>(Squirrel::GetRealInstance(vm, index, "CalendarDate")); } };
	template <> struct Param<const ScriptCalendarDate *> { static inline const ScriptCalendarDate *Get(HSQUIRRELVM vm, int index) { return  static_cast<ScriptCalendarDate *>(sq_gettype(vm, index) == OT_NULL ? nullptr : Squirrel::GetRealInstance(vm, index, "CalendarDate")); } };
	template <> struct Param<const ScriptCalendarDate &> { static inline const ScriptCalendarDate &Get(HSQUIRRELVM vm, int index) { return *static_cast<ScriptCalendarDate *>(Squirrel::GetRealInstance(vm, index, "CalendarDate")); } };
	template <> struct Return<ScriptCalendarDate *> { static inline int Set(HSQUIRRELVM vm, ScriptCalendarDate *res) { if (res == nullptr) { sq_pushnull(vm); return 1; } res->AddRef(); Squirrel::CreateClassInstanceVM(vm, "CalendarDate", res, nullptr, DefSQDestructorCallback<ScriptCalendarDate>, true); return 1; } };
} // namespace SQConvert
