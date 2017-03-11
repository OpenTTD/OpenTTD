/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_info.cpp Implementation of AIInfo and AILibrary */

#include "../stdafx.h"

#include "../script/squirrel_class.hpp"
#include "ai_info.hpp"
#include "ai_scanner.hpp"
#include "../debug.h"
#include "../string_func.h"
#include "../rev.h"

#include "../safeguards.h"

/**
 * Check if the API version provided by the AI is supported.
 * @param api_version The API version as provided by the AI.
 */
static bool CheckAPIVersion(const char *api_version)
{
	return strcmp(api_version, "0.7") == 0 || strcmp(api_version, "1.0") == 0 || strcmp(api_version, "1.1") == 0 ||
			strcmp(api_version, "1.2") == 0 || strcmp(api_version, "1.3") == 0 || strcmp(api_version, "1.4") == 0 ||
			strcmp(api_version, "1.5") == 0 || strcmp(api_version, "1.6") == 0 || strcmp(api_version, "1.7") == 0 ||
			strcmp(api_version, "1.8") == 0;
}

#if defined(WIN32)
#undef GetClassName
#endif /* WIN32 */
template <> const char *GetClassName<AIInfo, ST_AI>() { return "AIInfo"; }

/* static */ void AIInfo::RegisterAPI(Squirrel *engine)
{
	/* Create the AIInfo class, and add the RegisterAI function */
	DefSQClass<AIInfo, ST_AI> SQAIInfo("AIInfo");
	SQAIInfo.PreRegister(engine);
	SQAIInfo.AddConstructor<void (AIInfo::*)(), 1>(engine, "x");
	SQAIInfo.DefSQAdvancedMethod(engine, &AIInfo::AddSetting, "AddSetting");
	SQAIInfo.DefSQAdvancedMethod(engine, &AIInfo::AddLabels, "AddLabels");
	SQAIInfo.DefSQConst(engine, SCRIPTCONFIG_NONE, "CONFIG_NONE");
	SQAIInfo.DefSQConst(engine, SCRIPTCONFIG_RANDOM, "CONFIG_RANDOM");
	SQAIInfo.DefSQConst(engine, SCRIPTCONFIG_BOOLEAN, "CONFIG_BOOLEAN");
	SQAIInfo.DefSQConst(engine, SCRIPTCONFIG_INGAME, "CONFIG_INGAME");
	SQAIInfo.DefSQConst(engine, SCRIPTCONFIG_DEVELOPER, "CONFIG_DEVELOPER");

	/* Pre 1.2 had an AI prefix */
	SQAIInfo.DefSQConst(engine, SCRIPTCONFIG_NONE, "AICONFIG_NONE");
	SQAIInfo.DefSQConst(engine, SCRIPTCONFIG_RANDOM, "AICONFIG_RANDOM");
	SQAIInfo.DefSQConst(engine, SCRIPTCONFIG_BOOLEAN, "AICONFIG_BOOLEAN");
	SQAIInfo.DefSQConst(engine, SCRIPTCONFIG_INGAME, "AICONFIG_INGAME");

	SQAIInfo.PostRegister(engine);
	engine->AddMethod("RegisterAI", &AIInfo::Constructor, 2, "tx");
	engine->AddMethod("RegisterDummyAI", &AIInfo::DummyConstructor, 2, "tx");
}

/* static */ SQInteger AIInfo::Constructor(HSQUIRRELVM vm)
{
	/* Get the AIInfo */
	SQUserPointer instance = NULL;
	if (SQ_FAILED(sq_getinstanceup(vm, 2, &instance, 0)) || instance == NULL) return sq_throwerror(vm, "Pass an instance of a child class of AIInfo to RegisterAI");
	AIInfo *info = (AIInfo *)instance;

	SQInteger res = ScriptInfo::Constructor(vm, info);
	if (res != 0) return res;

	ScriptConfigItem config = _start_date_config;
	config.name = stredup(config.name);
	config.description = stredup(config.description);
	info->config_list.push_front(config);

	if (info->engine->MethodExists(*info->SQ_instance, "MinVersionToLoad")) {
		if (!info->engine->CallIntegerMethod(*info->SQ_instance, "MinVersionToLoad", &info->min_loadable_version, MAX_GET_OPS)) return SQ_ERROR;
	} else {
		info->min_loadable_version = info->GetVersion();
	}
	/* When there is an UseAsRandomAI function, call it. */
	if (info->engine->MethodExists(*info->SQ_instance, "UseAsRandomAI")) {
		if (!info->engine->CallBoolMethod(*info->SQ_instance, "UseAsRandomAI", &info->use_as_random, MAX_GET_OPS)) return SQ_ERROR;
	} else {
		info->use_as_random = true;
	}
	/* Try to get the API version the AI is written for. */
	if (info->engine->MethodExists(*info->SQ_instance, "GetAPIVersion")) {
		if (!info->engine->CallStringMethodStrdup(*info->SQ_instance, "GetAPIVersion", &info->api_version, MAX_GET_OPS)) return SQ_ERROR;
		if (!CheckAPIVersion(info->api_version)) {
			DEBUG(script, 1, "Loading info.nut from (%s.%d): GetAPIVersion returned invalid version", info->GetName(), info->GetVersion());
			return SQ_ERROR;
		}
	} else {
		info->api_version = stredup("0.7");
	}

	/* Remove the link to the real instance, else it might get deleted by RegisterAI() */
	sq_setinstanceup(vm, 2, NULL);
	/* Register the AI to the base system */
	info->GetScanner()->RegisterScript(info);
	return 0;
}

/* static */ SQInteger AIInfo::DummyConstructor(HSQUIRRELVM vm)
{
	/* Get the AIInfo */
	SQUserPointer instance;
	sq_getinstanceup(vm, 2, &instance, 0);
	AIInfo *info = (AIInfo *)instance;
	info->api_version = NULL;

	SQInteger res = ScriptInfo::Constructor(vm, info);
	if (res != 0) return res;

	char buf[8];
	seprintf(buf, lastof(buf), "%d.%d", GB(_openttd_newgrf_version, 28, 4), GB(_openttd_newgrf_version, 24, 4));
	info->api_version = stredup(buf);

	/* Remove the link to the real instance, else it might get deleted by RegisterAI() */
	sq_setinstanceup(vm, 2, NULL);
	/* Register the AI to the base system */
	static_cast<AIScannerInfo *>(info->GetScanner())->SetDummyAI(info);
	return 0;
}

AIInfo::AIInfo() :
	min_loadable_version(0),
	use_as_random(false),
	api_version(NULL)
{
}

AIInfo::~AIInfo()
{
	free(this->api_version);
}

bool AIInfo::CanLoadFromVersion(int version) const
{
	if (version == -1) return true;
	return version >= this->min_loadable_version && version <= this->GetVersion();
}


AILibrary::~AILibrary()
{
	free(this->category);
}

/* static */ void AILibrary::RegisterAPI(Squirrel *engine)
{
	/* Create the AILibrary class, and add the RegisterLibrary function */
	engine->AddClassBegin("AILibrary");
	engine->AddClassEnd();
	engine->AddMethod("RegisterLibrary", &AILibrary::Constructor, 2, "tx");
}

/* static */ SQInteger AILibrary::Constructor(HSQUIRRELVM vm)
{
	/* Create a new library */
	AILibrary *library = new AILibrary();

	SQInteger res = ScriptInfo::Constructor(vm, library);
	if (res != 0) {
		delete library;
		return res;
	}

	/* Cache the category */
	if (!library->CheckMethod("GetCategory") || !library->engine->CallStringMethodStrdup(*library->SQ_instance, "GetCategory", &library->category, MAX_GET_OPS)) {
		delete library;
		return SQ_ERROR;
	}

	/* Register the Library to the base system */
	library->GetScanner()->RegisterScript(library);

	return 0;
}
