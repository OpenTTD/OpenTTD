/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file game_instance.cpp Implementation of GameInstance. */

#include "../stdafx.h"
#include "../error.h"

#include "../script/squirrel_class.hpp"

#include "../script/script_storage.hpp"
#include "../script/script_cmd.h"
#include "../script/script_gui.h"
#include "game_config.hpp"
#include "game_info.hpp"
#include "game_instance.hpp"
#include "game_text.hpp"
#include "game.hpp"

/* Convert all Game related classes to Squirrel data. */
#include "../script/api/game/game_includes.hpp"

#include "../safeguards.h"


GameInstance::GameInstance() :
	ScriptInstance("GS")
{}

void GameInstance::Initialize(GameInfo *info)
{
	this->versionAPI = info->GetAPIVersion();

	/* Register the GameController */
	SQGSController_Register(this->engine);

	ScriptInstance::Initialize(info->GetMainScript(), info->GetInstanceName(), OWNER_DEITY);
}

void GameInstance::RegisterAPI()
{
	ScriptInstance::RegisterAPI();

	/* Register all classes */
	SQGS_RegisterAll(this->engine);

	RegisterGameTranslation(this->engine);

	if (!this->LoadCompatibilityScripts(this->versionAPI, GAME_DIR)) this->Died();
}

int GameInstance::GetSetting(const std::string &name)
{
	return GameConfig::GetConfig()->GetSetting(name);
}

ScriptInfo *GameInstance::FindLibrary(const std::string &library, int version)
{
	return (ScriptInfo *)Game::FindLibrary(library, version);
}

void GameInstance::Died()
{
	ScriptInstance::Died();

	ShowScriptDebugWindow(OWNER_DEITY);

	const GameInfo *info = Game::GetInfo();
	if (info != nullptr) {
		ShowErrorMessage(STR_ERROR_AI_PLEASE_REPORT_CRASH, INVALID_STRING_ID, WL_WARNING);

		if (!info->GetURL().empty()) {
			ScriptLog::Info("Please report the error to the following URL:");
			ScriptLog::Info(info->GetURL());
		}
	}
}

/**
 * DoCommand callback function for all commands executed by Game Scripts.
 * @param cmd cmd as given to DoCommandPInternal.
 * @param result The result of the command.
 * @param data Command data as given to Command<>::Post.
 * @param result_data Additional returned data from the command.
 */
void CcGame(Commands cmd, const CommandCost &result, const CommandDataBuffer &data, CommandDataBuffer result_data)
{
	if (Game::GetGameInstance()->DoCommandCallback(result, data, std::move(result_data), cmd)) {
		Game::GetGameInstance()->Continue();
	}
}

CommandCallbackData *GameInstance::GetDoCommandCallback()
{
	return &CcGame;
}
