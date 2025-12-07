/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
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
static bool CheckAPIVersion(const std::string &api_version)
{
	return std::ranges::find(AIInfo::ApiVersions, api_version) != std::end(AIInfo::ApiVersions);
}

template <> SQInteger PushClassName<AIInfo, ScriptType::AI>(HSQUIRRELVM vm) { sq_pushstring(vm, "AIInfo"); return 1; }

/* static */ void AIInfo::RegisterAPI(Squirrel &engine)
{
	/* Create the AIInfo class, and add the RegisterAI function */
	DefSQClass<AIInfo, ScriptType::AI> SQAIInfo("AIInfo");
	SQAIInfo.PreRegister(engine);
	SQAIInfo.AddConstructor<void (AIInfo::*)()>(engine, "x");
	SQAIInfo.DefSQAdvancedMethod(engine, &AIInfo::AddSetting, "AddSetting");
	SQAIInfo.DefSQAdvancedMethod(engine, &AIInfo::AddLabels, "AddLabels");
	SQAIInfo.DefSQConst(engine, ScriptConfigFlags{}.base(), "CONFIG_NONE");
	SQAIInfo.DefSQConst(engine, ScriptConfigFlags{}.base(), "CONFIG_RANDOM"); // Deprecated, mapped to NONE.
	SQAIInfo.DefSQConst(engine, ScriptConfigFlags{ScriptConfigFlag::Boolean}.base(), "CONFIG_BOOLEAN");
	SQAIInfo.DefSQConst(engine, ScriptConfigFlags{ScriptConfigFlag::InGame}.base(), "CONFIG_INGAME");
	SQAIInfo.DefSQConst(engine, ScriptConfigFlags{ScriptConfigFlag::Developer}.base(), "CONFIG_DEVELOPER");

	/* Pre 1.2 had an AI prefix */
	SQAIInfo.DefSQConst(engine, ScriptConfigFlags{}.base(), "AICONFIG_NONE");
	SQAIInfo.DefSQConst(engine, ScriptConfigFlags{}.base(), "AICONFIG_RANDOM"); // Deprecated, mapped to NONE.
	SQAIInfo.DefSQConst(engine, ScriptConfigFlags{ScriptConfigFlag::Boolean}.base(), "AICONFIG_BOOLEAN");
	SQAIInfo.DefSQConst(engine, ScriptConfigFlags{ScriptConfigFlag::InGame}.base(), "AICONFIG_INGAME");

	SQAIInfo.PostRegister(engine);
	engine.AddMethod("RegisterAI", &AIInfo::Constructor, "tx");
	engine.AddMethod("RegisterDummyAI", &AIInfo::DummyConstructor, "tx");
}

/* static */ SQInteger AIInfo::Constructor(HSQUIRRELVM vm)
{
	/* Get the AIInfo */
	SQUserPointer instance = nullptr;
	if (SQ_FAILED(sq_getinstanceup(vm, 2, &instance, nullptr)) || instance == nullptr) return sq_throwerror(vm, "Pass an instance of a child class of AIInfo to RegisterAI");
	AIInfo *info = (AIInfo *)instance;

	SQInteger res = ScriptInfo::Constructor(vm, *info);
	if (res != 0) return res;

	if (info->engine->MethodExists(info->SQ_instance, "MinVersionToLoad")) {
		if (!info->engine->CallIntegerMethod(info->SQ_instance, "MinVersionToLoad", &info->min_loadable_version, MAX_GET_OPS)) return SQ_ERROR;
		if (info->min_loadable_version < 0) return SQ_ERROR;
	} else {
		info->min_loadable_version = info->GetVersion();
	}
	/* When there is an UseAsRandomAI function, call it. */
	if (info->engine->MethodExists(info->SQ_instance, "UseAsRandomAI")) {
		if (!info->engine->CallBoolMethod(info->SQ_instance, "UseAsRandomAI", &info->use_as_random, MAX_GET_OPS)) return SQ_ERROR;
	} else {
		info->use_as_random = true;
	}
	/* Try to get the API version the AI is written for. */
	if (info->engine->MethodExists(info->SQ_instance, "GetAPIVersion")) {
		if (!info->engine->CallStringMethod(info->SQ_instance, "GetAPIVersion", &info->api_version, MAX_GET_OPS)) return SQ_ERROR;
		if (!CheckAPIVersion(info->api_version)) {
			Debug(script, 1, "Loading info.nut from ({}.{}): GetAPIVersion returned invalid version", info->GetName(), info->GetVersion());
			return SQ_ERROR;
		}
	} else {
		info->api_version = "0.7";
	}

	/* Remove the link to the real instance, else it might get deleted by RegisterAI() */
	sq_setinstanceup(vm, 2, nullptr);
	/* Register the AI to the base system */
	info->GetScanner()->RegisterScript(std::unique_ptr<AIInfo>{info});
	return 0;
}

/* static */ SQInteger AIInfo::DummyConstructor(HSQUIRRELVM vm)
{
	/* Get the AIInfo */
	SQUserPointer instance;
	sq_getinstanceup(vm, 2, &instance, nullptr);
	AIInfo *info = (AIInfo *)instance;
	info->api_version = *std::rbegin(AIInfo::ApiVersions);

	SQInteger res = ScriptInfo::Constructor(vm, *info);
	if (res != 0) return res;

	/* Remove the link to the real instance, else it might get deleted by RegisterAI() */
	sq_setinstanceup(vm, 2, nullptr);
	/* Register the AI to the base system */
	static_cast<AIScannerInfo *>(info->GetScanner())->SetDummyAI(std::unique_ptr<AIInfo>(info));
	return 0;
}

AIInfo::AIInfo() :
	min_loadable_version(0),
	use_as_random(false)
{
}

bool AIInfo::CanLoadFromVersion(int version) const
{
	if (version == -1) return true;
	return version >= this->min_loadable_version && version <= this->GetVersion();
}


/* static */ void AILibrary::RegisterAPI(Squirrel &engine)
{
	/* Create the AILibrary class, and add the RegisterLibrary function */
	engine.AddClassBegin("AILibrary");
	engine.AddClassEnd();
	engine.AddMethod("RegisterLibrary", &AILibrary::Constructor, "tx");
}

/* static */ SQInteger AILibrary::Constructor(HSQUIRRELVM vm)
{
	/* Create a new library */
	auto library = std::make_unique<AILibrary>();

	SQInteger res = ScriptInfo::Constructor(vm, *library);
	if (res != 0) {
		return res;
	}

	/* Cache the category */
	if (!library->CheckMethod("GetCategory") || !library->engine->CallStringMethod(library->SQ_instance, "GetCategory", &library->category, MAX_GET_OPS)) {
		return SQ_ERROR;
	}

	/* Register the Library to the base system */
	ScriptScanner *scanner = library->GetScanner();
	scanner->RegisterScript(std::move(library));

	return 0;
}
