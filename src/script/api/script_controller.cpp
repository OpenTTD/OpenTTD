/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_controller.cpp Implementation of ScriptControler. */

#include "../../stdafx.h"
#include "../../string_func.h"
#include "../../script/squirrel.hpp"
#include "../../rev.h"

#include "script_controller.hpp"
#include "script_error.hpp"
#include "../script_fatalerror.hpp"
#include "../script_info.hpp"
#include "../script_instance.hpp"
#include "script_log.hpp"
#include "../script_gui.h"
#include "../../settings_type.h"
#include "../../network/network.h"
#include "../../misc_cmd.h"

#include "../../safeguards.h"

/* static */ void ScriptController::SetCommandDelay(int ticks)
{
	if (ticks <= 0) return;
	ScriptObject::SetDoCommandDelay(ticks);
}

/* static */ void ScriptController::Sleep(int ticks)
{
	if (!ScriptObject::CanSuspend()) {
		throw Script_FatalError("You are not allowed to call Sleep in your constructor, Save(), Load(), and any valuator.");
	}

	if (ticks <= 0) {
		ScriptLog::Warning("Sleep() value should be > 0. Assuming value 1.");
		ticks = 1;
	}

	throw Script_Suspend(ticks, nullptr);
}

/* static */ void ScriptController::Break(const std::string &message)
{
	if (_network_dedicated || !_settings_client.gui.ai_developer_tools) return;

	ScriptObject::GetActiveInstance()->Pause();

	ScriptLog::Log(ScriptLogTypes::LOG_SQ_ERROR, fmt::format("Break: {}", message));

	/* Inform script developer that their script has been paused and
	 * needs manual action to continue. */
	ShowScriptDebugWindow(ScriptObject::GetRootCompany());

	if ((_pause_mode & PM_PAUSED_NORMAL) == PM_UNPAUSED) {
		ScriptObject::Command<CMD_PAUSE>::Do(PM_PAUSED_NORMAL, true);
	}
}

/* static */ void ScriptController::Print(bool error_msg, const std::string &message)
{
	ScriptLog::Log(error_msg ? ScriptLogTypes::LOG_SQ_ERROR : ScriptLogTypes::LOG_SQ_INFO, message);
}

ScriptController::ScriptController(CompanyID company) :
	ticks(0),
	loaded_library_count(0)
{
	ScriptObject::SetCompany(company);
}

/* static */ uint ScriptController::GetTick()
{
	return ScriptObject::GetActiveInstance()->GetController()->ticks;
}

/* static */ int ScriptController::GetOpsTillSuspend()
{
	return ScriptObject::GetActiveInstance()->GetOpsTillSuspend();
}

/* static */ int ScriptController::GetSetting(const std::string &name)
{
	return ScriptObject::GetActiveInstance()->GetSetting(name);
}

/* static */ uint ScriptController::GetVersion()
{
	return _openttd_newgrf_version;
}

/* static */ HSQOBJECT ScriptController::Import(const std::string &library, const std::string &class_name, int version)
{
	ScriptController *controller = ScriptObject::GetActiveInstance()->GetController();
	Squirrel *engine = ScriptObject::GetActiveInstance()->engine;
	HSQUIRRELVM vm = engine->GetVM();

	ScriptInfo *lib = ScriptObject::GetActiveInstance()->FindLibrary(library, version);
	if (lib == nullptr) {
		throw sq_throwerror(vm, fmt::format("couldn't find library '{}' with version {}", library, version));
	}

	/* Internally we store libraries as 'library.version' */
	std::string library_name = fmt::format("{}.{}", library, version);

	/* Get the current table/class we belong to */
	HSQOBJECT parent;
	sq_getstackobj(vm, 1, &parent);

	std::string fake_class;

	LoadedLibraryList::iterator it = controller->loaded_library.find(library_name);
	if (it != controller->loaded_library.end()) {
		fake_class = (*it).second;
	} else {
		int next_number = ++controller->loaded_library_count;

		/* Create a new fake internal name */
		fake_class = fmt::format("_internalNA{}", next_number);

		/* Load the library in a 'fake' namespace, so we can link it to the name the user requested */
		sq_pushroottable(vm);
		sq_pushstring(vm, fake_class, -1);
		sq_newclass(vm, SQFalse);
		/* Load the library */
		if (!engine->LoadScript(vm, lib->GetMainScript(), false)) {
			throw sq_throwerror(vm, fmt::format("there was a compile error when importing '{}' version {}", library, version));
		}
		/* Create the fake class */
		sq_newslot(vm, -3, SQFalse);
		sq_pop(vm, 1);

		controller->loaded_library[library_name] = fake_class;
	}

	/* Find the real class inside the fake class (like 'sets.Vector') */
	sq_pushroottable(vm);
	sq_pushstring(vm, fake_class, -1);
	if (SQ_FAILED(sq_get(vm, -2))) {
		throw sq_throwerror(vm, "internal error assigning library class");
	}
	sq_pushstring(vm, lib->GetInstanceName(), -1);
	if (SQ_FAILED(sq_get(vm, -2))) {
		throw sq_throwerror(vm, fmt::format("unable to find class '{}' in the library '{}' version {}", lib->GetInstanceName(), library, version));
	}
	HSQOBJECT obj;
	sq_getstackobj(vm, -1, &obj);
	sq_pop(vm, 3);

	if (class_name.empty()) return obj;

	/* Now link the name the user wanted to our 'fake' class */
	sq_pushobject(vm, parent);
	sq_pushstring(vm, class_name, -1);
	sq_pushobject(vm, obj);
	sq_newclass(vm, SQTrue);
	sq_newslot(vm, -3, SQFalse);
	sq_pop(vm, 1);

	return obj;
}
