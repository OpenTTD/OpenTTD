/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_controller.cpp Implementation of AIControler. */

#include "../../stdafx.h"
#include "../../string_func.h"
#include "../../script/squirrel.hpp"
#include "../../rev.h"

#include "script_controller.hpp"
#include "../script_fatalerror.hpp"
#include "../script_info.hpp"
#include "../script_instance.hpp"
#include "script_log.hpp"

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

	throw Script_Suspend(ticks, NULL);
}

/* static */ void ScriptController::Print(bool error_msg, const char *message)
{
	ScriptLog::Log(error_msg ? ScriptLog::LOG_SQ_ERROR : ScriptLog::LOG_SQ_INFO, message);
}

ScriptController::ScriptController(CompanyID company) :
	ticks(0),
	loaded_library_count(0)
{
	ScriptObject::SetCompany(company);
}

ScriptController::~ScriptController()
{
	for (LoadedLibraryList::iterator iter = this->loaded_library.begin(); iter != this->loaded_library.end(); iter++) {
		free((*iter).second);
		free((*iter).first);
	}

	this->loaded_library.clear();
}

/* static */ uint ScriptController::GetTick()
{
	return ScriptObject::GetActiveInstance()->GetController()->ticks;
}

/* static */ int ScriptController::GetOpsTillSuspend()
{
	return ScriptObject::GetActiveInstance()->GetOpsTillSuspend();
}

/* static */ int ScriptController::GetSetting(const char *name)
{
	return ScriptObject::GetActiveInstance()->GetSetting(name);
}

/* static */ uint ScriptController::GetVersion()
{
	return _openttd_newgrf_version;
}

/* static */ HSQOBJECT ScriptController::Import(const char *library, const char *class_name, int version)
{
	ScriptController *controller = ScriptObject::GetActiveInstance()->GetController();
	Squirrel *engine = ScriptObject::GetActiveInstance()->engine;
	HSQUIRRELVM vm = engine->GetVM();

	/* Internally we store libraries as 'library.version' */
	char library_name[1024];
	snprintf(library_name, sizeof(library_name), "%s.%d", library, version);
	strtolower(library_name);

	ScriptInfo *lib = ScriptObject::GetActiveInstance()->FindLibrary(library, version);
	if (lib == NULL) {
		char error[1024];
		snprintf(error, sizeof(error), "couldn't find library '%s' with version %d", library, version);
		throw sq_throwerror(vm, OTTD2SQ(error));
	}

	/* Get the current table/class we belong to */
	HSQOBJECT parent;
	sq_getstackobj(vm, 1, &parent);

	char fake_class[1024];

	LoadedLibraryList::iterator iter = controller->loaded_library.find(library_name);
	if (iter != controller->loaded_library.end()) {
		ttd_strlcpy(fake_class, (*iter).second, sizeof(fake_class));
	} else {
		int next_number = ++controller->loaded_library_count;

		/* Create a new fake internal name */
		snprintf(fake_class, sizeof(fake_class), "_internalNA%d", next_number);

		/* Load the library in a 'fake' namespace, so we can link it to the name the user requested */
		sq_pushroottable(vm);
		sq_pushstring(vm, OTTD2SQ(fake_class), -1);
		sq_newclass(vm, SQFalse);
		/* Load the library */
		if (!engine->LoadScript(vm, lib->GetMainScript(), false)) {
			char error[1024];
			snprintf(error, sizeof(error), "there was a compile error when importing '%s' version %d", library, version);
			throw sq_throwerror(vm, OTTD2SQ(error));
		}
		/* Create the fake class */
		sq_newslot(vm, -3, SQFalse);
		sq_pop(vm, 1);

		controller->loaded_library[strdup(library_name)] = strdup(fake_class);
	}

	/* Find the real class inside the fake class (like 'sets.Vector') */
	sq_pushroottable(vm);
	sq_pushstring(vm, OTTD2SQ(fake_class), -1);
	if (SQ_FAILED(sq_get(vm, -2))) {
		throw sq_throwerror(vm, _SC("internal error assigning library class"));
	}
	sq_pushstring(vm, OTTD2SQ(lib->GetInstanceName()), -1);
	if (SQ_FAILED(sq_get(vm, -2))) {
		char error[1024];
		snprintf(error, sizeof(error), "unable to find class '%s' in the library '%s' version %d", lib->GetInstanceName(), library, version);
		throw sq_throwerror(vm, OTTD2SQ(error));
	}
	HSQOBJECT obj;
	sq_getstackobj(vm, -1, &obj);
	sq_pop(vm, 3);

	if (StrEmpty(class_name)) return obj;

	/* Now link the name the user wanted to our 'fake' class */
	sq_pushobject(vm, parent);
	sq_pushstring(vm, OTTD2SQ(class_name), -1);
	sq_pushobject(vm, obj);
	sq_newclass(vm, SQTrue);
	sq_newslot(vm, -3, SQFalse);
	sq_pop(vm, 1);

	return obj;
}
