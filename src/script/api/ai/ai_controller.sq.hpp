/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

#include "../script_controller.hpp"

template <> SQInteger PushClassName<ScriptController, ScriptType::AI>(HSQUIRRELVM vm) { sq_pushstring(vm, "AIController"); return 1; }

void SQAIController_Register(Squirrel &engine)
{
	DefSQClass<ScriptController, ScriptType::AI> SQAIController("AIController");
	SQAIController.PreRegister(engine);

	SQAIController.DefSQStaticMethod(engine, &ScriptController::GetTick,           "GetTick",           ".");
	SQAIController.DefSQStaticMethod(engine, &ScriptController::GetOpsTillSuspend, "GetOpsTillSuspend", ".");
	SQAIController.DefSQStaticMethod(engine, &ScriptController::SetCommandDelay,   "SetCommandDelay",   ".i");
	SQAIController.DefSQStaticMethod(engine, &ScriptController::Sleep,             "Sleep",             ".i");
	SQAIController.DefSQStaticMethod(engine, &ScriptController::Break,             "Break",             ".s");
	SQAIController.DefSQStaticMethod(engine, &ScriptController::GetSetting,        "GetSetting",        ".s");
	SQAIController.DefSQStaticMethod(engine, &ScriptController::GetVersion,        "GetVersion",        ".");
	SQAIController.DefSQStaticMethod(engine, &ScriptController::Print,             "Print",             ".bs");

	SQAIController.PostRegister(engine);

	/* Register the import statement to the global scope */
	SQAIController.DefSQStaticMethod(engine, &ScriptController::Import,            "import",            ".ssi");
}
