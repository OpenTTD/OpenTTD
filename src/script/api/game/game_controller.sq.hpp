/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

#include "../script_controller.hpp"

template <> SQInteger PushClassName<ScriptController, ScriptType::GS>(HSQUIRRELVM vm) { sq_pushstring(vm, "GSController"); return 1; }

void SQGSController_Register(Squirrel &engine)
{
	DefSQClass<ScriptController, ScriptType::GS> SQGSController("GSController");
	SQGSController.PreRegister(engine);

	SQGSController.DefSQStaticMethod(engine, &ScriptController::GetTick,           "GetTick",           ".");
	SQGSController.DefSQStaticMethod(engine, &ScriptController::GetOpsTillSuspend, "GetOpsTillSuspend", ".");
	SQGSController.DefSQStaticMethod(engine, &ScriptController::SetCommandDelay,   "SetCommandDelay",   ".i");
	SQGSController.DefSQStaticMethod(engine, &ScriptController::Sleep,             "Sleep",             ".i");
	SQGSController.DefSQStaticMethod(engine, &ScriptController::Break,             "Break",             ".s");
	SQGSController.DefSQStaticMethod(engine, &ScriptController::GetSetting,        "GetSetting",        ".s");
	SQGSController.DefSQStaticMethod(engine, &ScriptController::GetVersion,        "GetVersion",        ".");
	SQGSController.DefSQStaticMethod(engine, &ScriptController::Print,             "Print",             ".bs");

	SQGSController.PostRegister(engine);

	/* Register the import statement to the global scope */
	SQGSController.DefSQStaticMethod(engine, &ScriptController::Import,            "import",            ".ssi");
}
