/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file game_info.cpp Implementation of GameInfo */

#include "../stdafx.h"

#include "../script/squirrel_class.hpp"
#include "game_info.hpp"
#include "game_scanner.hpp"
#include "../debug.h"

#include "../safeguards.h"

/**
 * Check if the API version provided by the Game is supported.
 * @param api_version The API version as provided by the Game.
 */
static bool CheckAPIVersion(const std::string &api_version)
{
	return std::ranges::find(GameInfo::ApiVersions, api_version) != std::end(GameInfo::ApiVersions);
}

template <> SQInteger PushClassName<GameInfo, ScriptType::GS>(HSQUIRRELVM vm) { sq_pushstring(vm, "GSInfo"); return 1; }

/* static */ void GameInfo::RegisterAPI(Squirrel &engine)
{
	/* Create the GSInfo class, and add the RegisterGS function */
	DefSQClass<GameInfo, ScriptType::GS> SQGSInfo("GSInfo");
	SQGSInfo.PreRegister(engine);
	SQGSInfo.AddConstructor<void (GameInfo::*)()>(engine, "x");
	SQGSInfo.DefSQAdvancedMethod(engine, &GameInfo::AddSetting, "AddSetting");
	SQGSInfo.DefSQAdvancedMethod(engine, &GameInfo::AddLabels, "AddLabels");
	SQGSInfo.DefSQConst(engine, ScriptConfigFlags{}.base(), "CONFIG_NONE");
	SQGSInfo.DefSQConst(engine, ScriptConfigFlags{}.base(), "CONFIG_RANDOM"); // Deprecated, mapped to NONE.
	SQGSInfo.DefSQConst(engine, ScriptConfigFlags{ScriptConfigFlag::Boolean}.base(), "CONFIG_BOOLEAN");
	SQGSInfo.DefSQConst(engine, ScriptConfigFlags{ScriptConfigFlag::InGame}.base(), "CONFIG_INGAME");
	SQGSInfo.DefSQConst(engine, ScriptConfigFlags{ScriptConfigFlag::Developer}.base(), "CONFIG_DEVELOPER");

	SQGSInfo.PostRegister(engine);
	engine.AddMethod("RegisterGS", &GameInfo::Constructor, "tx");
}

/* static */ SQInteger GameInfo::Constructor(HSQUIRRELVM vm)
{
	/* Get the GameInfo */
	SQUserPointer instance = nullptr;
	if (SQ_FAILED(sq_getinstanceup(vm, 2, &instance, nullptr)) || instance == nullptr) return sq_throwerror(vm, "Pass an instance of a child class of GameInfo to RegisterGame");
	GameInfo *info = (GameInfo *)instance;

	SQInteger res = ScriptInfo::Constructor(vm, *info);
	if (res != 0) return res;

	if (info->engine->MethodExists(info->SQ_instance, "MinVersionToLoad")) {
		if (!info->engine->CallIntegerMethod(info->SQ_instance, "MinVersionToLoad", &info->min_loadable_version, MAX_GET_OPS)) return SQ_ERROR;
		if (info->min_loadable_version < 0) return SQ_ERROR;
	} else {
		info->min_loadable_version = info->GetVersion();
	}
	/* When there is an IsSelectable function, call it. */
	if (info->engine->MethodExists(info->SQ_instance, "IsDeveloperOnly")) {
		if (!info->engine->CallBoolMethod(info->SQ_instance, "IsDeveloperOnly", &info->is_developer_only, MAX_GET_OPS)) return SQ_ERROR;
	} else {
		info->is_developer_only = false;
	}
	/* Try to get the API version the AI is written for. */
	if (!info->CheckMethod("GetAPIVersion")) return SQ_ERROR;
	if (!info->engine->CallStringMethod(info->SQ_instance, "GetAPIVersion", &info->api_version, MAX_GET_OPS)) return SQ_ERROR;
	if (!CheckAPIVersion(info->api_version)) {
		Debug(script, 1, "Loading info.nut from ({}.{}): GetAPIVersion returned invalid version", info->GetName(), info->GetVersion());
		return SQ_ERROR;
	}

	/* Remove the link to the real instance, else it might get deleted by RegisterGame() */
	sq_setinstanceup(vm, 2, nullptr);
	/* Register the Game to the base system */
	info->GetScanner()->RegisterScript(std::unique_ptr<GameInfo>{info});
	return 0;
}

GameInfo::GameInfo() :
	min_loadable_version(0),
	is_developer_only(false)
{
}

bool GameInfo::CanLoadFromVersion(int version) const
{
	if (version == -1) return true;
	return version >= this->min_loadable_version && version <= this->GetVersion();
}


/* static */ void GameLibrary::RegisterAPI(Squirrel &engine)
{
	/* Create the GameLibrary class, and add the RegisterLibrary function */
	engine.AddClassBegin("GSLibrary");
	engine.AddClassEnd();
	engine.AddMethod("RegisterLibrary", &GameLibrary::Constructor, "tx");
}

/* static */ SQInteger GameLibrary::Constructor(HSQUIRRELVM vm)
{
	/* Create a new library */
	auto library = std::make_unique<GameLibrary>();

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
