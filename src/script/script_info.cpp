/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_info.cpp Implementation of ScriptInfo. */

#include "../stdafx.h"

#include "squirrel_helper.hpp"

#include "script_info.hpp"
#include "script_scanner.hpp"

/** Number of operations to get the author and similar information. */
static const int MAX_GET_OPS            =   1000;
/** Number of operations to create an instance of a script. */
static const int MAX_CREATEINSTANCE_OPS = 100000;

ScriptInfo::~ScriptInfo()
{
	free(this->author);
	free(this->name);
	free(this->short_name);
	free(this->description);
	free(this->date);
	free(this->instance_name);
	free(this->url);
	free(this->main_script);
	free(this->tar_file);
	free(this->SQ_instance);
}

bool ScriptInfo::CheckMethod(const char *name) const
{
	if (!this->engine->MethodExists(*this->SQ_instance, name)) {
		char error[1024];
		snprintf(error, sizeof(error), "your info.nut/library.nut doesn't have the method '%s'", name);
		this->engine->ThrowError(error);
		return false;
	}
	return true;
}

/* static */ SQInteger ScriptInfo::Constructor(HSQUIRRELVM vm, ScriptInfo *info)
{
	/* Set some basic info from the parent */
	info->SQ_instance = MallocT<SQObject>(1);
	Squirrel::GetInstance(vm, info->SQ_instance, 2);
	/* Make sure the instance stays alive over time */
	sq_addref(vm, info->SQ_instance);

	info->scanner = (ScriptScanner *)Squirrel::GetGlobalPointer(vm);
	info->engine = info->scanner->GetEngine();

	/* Ensure the mandatory functions exist */
	static const char * const required_functions[] = {
		"GetAuthor",
		"GetName",
		"GetShortName",
		"GetDescription",
		"GetVersion",
		"GetDate",
		"CreateInstance",
	};
	for (size_t i = 0; i < lengthof(required_functions); i++) {
		if (!info->CheckMethod(required_functions[i])) return SQ_ERROR;
	}

	/* Get location information of the scanner */
	info->main_script = strdup(info->scanner->GetMainScript());
	const char *tar_name = info->scanner->GetTarFile();
	if (tar_name != NULL) info->tar_file = strdup(tar_name);

	/* Cache the data the info file gives us. */
	if (!info->engine->CallStringMethodStrdup(*info->SQ_instance, "GetAuthor", &info->author, MAX_GET_OPS)) return SQ_ERROR;
	if (!info->engine->CallStringMethodStrdup(*info->SQ_instance, "GetName", &info->name, MAX_GET_OPS)) return SQ_ERROR;
	if (!info->engine->CallStringMethodStrdup(*info->SQ_instance, "GetShortName", &info->short_name, MAX_GET_OPS)) return SQ_ERROR;
	if (!info->engine->CallStringMethodStrdup(*info->SQ_instance, "GetDescription", &info->description, MAX_GET_OPS)) return SQ_ERROR;
	if (!info->engine->CallStringMethodStrdup(*info->SQ_instance, "GetDate", &info->date, MAX_GET_OPS)) return SQ_ERROR;
	if (!info->engine->CallIntegerMethod(*info->SQ_instance, "GetVersion", &info->version, MAX_GET_OPS)) return SQ_ERROR;
	if (!info->engine->CallStringMethodStrdup(*info->SQ_instance, "CreateInstance", &info->instance_name, MAX_CREATEINSTANCE_OPS)) return SQ_ERROR;

	/* The GetURL function is optional. */
	if (info->engine->MethodExists(*info->SQ_instance, "GetURL")) {
		if (!info->engine->CallStringMethodStrdup(*info->SQ_instance, "GetURL", &info->url, MAX_GET_OPS)) return SQ_ERROR;
	}

	return 0;
}
