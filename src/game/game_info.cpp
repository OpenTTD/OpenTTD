/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
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
static bool CheckAPIVersion(const char *api_version)
{
	return strcmp(api_version, "1.2") == 0 || strcmp(api_version, "1.3") == 0 || strcmp(api_version, "1.4") == 0 ||
			strcmp(api_version, "1.5") == 0 || strcmp(api_version, "1.6") == 0 || strcmp(api_version, "1.7") == 0 ||
			strcmp(api_version, "1.8") == 0;
}

#if defined(WIN32)
#undef GetClassName
#endif /* WIN32 */
template <> const char *GetClassName<GameInfo, ST_GS>() { return "GSInfo"; }

/* static */ void GameInfo::RegisterAPI(Squirrel *engine)
{
	/* Create the GSInfo class, and add the RegisterGS function */
	DefSQClass<GameInfo, ST_GS> SQGSInfo("GSInfo");
	SQGSInfo.PreRegister(engine);
	SQGSInfo.AddConstructor<void (GameInfo::*)(), 1>(engine, "x");
	SQGSInfo.DefSQAdvancedMethod(engine, &GameInfo::AddSetting, "AddSetting");
	SQGSInfo.DefSQAdvancedMethod(engine, &GameInfo::AddLabels, "AddLabels");
	SQGSInfo.DefSQConst(engine, SCRIPTCONFIG_NONE, "CONFIG_NONE");
	SQGSInfo.DefSQConst(engine, SCRIPTCONFIG_RANDOM, "CONFIG_RANDOM");
	SQGSInfo.DefSQConst(engine, SCRIPTCONFIG_BOOLEAN, "CONFIG_BOOLEAN");
	SQGSInfo.DefSQConst(engine, SCRIPTCONFIG_INGAME, "CONFIG_INGAME");
	SQGSInfo.DefSQConst(engine, SCRIPTCONFIG_DEVELOPER, "CONFIG_DEVELOPER");

	SQGSInfo.PostRegister(engine);
	engine->AddMethod("RegisterGS", &GameInfo::Constructor, 2, "tx");
}

/* static */ SQInteger GameInfo::Constructor(HSQUIRRELVM vm)
{
	/* Get the GameInfo */
	SQUserPointer instance = NULL;
	if (SQ_FAILED(sq_getinstanceup(vm, 2, &instance, 0)) || instance == NULL) return sq_throwerror(vm, "Pass an instance of a child class of GameInfo to RegisterGame");
	GameInfo *info = (GameInfo *)instance;

	SQInteger res = ScriptInfo::Constructor(vm, info);
	if (res != 0) return res;

	if (info->engine->MethodExists(*info->SQ_instance, "MinVersionToLoad")) {
		if (!info->engine->CallIntegerMethod(*info->SQ_instance, "MinVersionToLoad", &info->min_loadable_version, MAX_GET_OPS)) return SQ_ERROR;
	} else {
		info->min_loadable_version = info->GetVersion();
	}
	/* When there is an IsSelectable function, call it. */
	if (info->engine->MethodExists(*info->SQ_instance, "IsDeveloperOnly")) {
		if (!info->engine->CallBoolMethod(*info->SQ_instance, "IsDeveloperOnly", &info->is_developer_only, MAX_GET_OPS)) return SQ_ERROR;
	} else {
		info->is_developer_only = false;
	}
	/* Try to get the API version the AI is written for. */
	if (!info->CheckMethod("GetAPIVersion")) return SQ_ERROR;
	if (!info->engine->CallStringMethodStrdup(*info->SQ_instance, "GetAPIVersion", &info->api_version, MAX_GET_OPS)) return SQ_ERROR;
	if (!CheckAPIVersion(info->api_version)) {
		DEBUG(script, 1, "Loading info.nut from (%s.%d): GetAPIVersion returned invalid version", info->GetName(), info->GetVersion());
		return SQ_ERROR;
	}

	/* Remove the link to the real instance, else it might get deleted by RegisterGame() */
	sq_setinstanceup(vm, 2, NULL);
	/* Register the Game to the base system */
	info->GetScanner()->RegisterScript(info);
	return 0;
}

GameInfo::GameInfo() :
	min_loadable_version(0),
	is_developer_only(false),
	api_version(NULL)
{
}

GameInfo::~GameInfo()
{
	free(this->api_version);
}

bool GameInfo::CanLoadFromVersion(int version) const
{
	if (version == -1) return true;
	return version >= this->min_loadable_version && version <= this->GetVersion();
}


GameLibrary::~GameLibrary()
{
	free(this->category);
}

/* static */ void GameLibrary::RegisterAPI(Squirrel *engine)
{
	/* Create the GameLibrary class, and add the RegisterLibrary function */
	engine->AddClassBegin("GSLibrary");
	engine->AddClassEnd();
	engine->AddMethod("RegisterLibrary", &GameLibrary::Constructor, 2, "tx");
}

/* static */ SQInteger GameLibrary::Constructor(HSQUIRRELVM vm)
{
	/* Create a new library */
	GameLibrary *library = new GameLibrary();

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
