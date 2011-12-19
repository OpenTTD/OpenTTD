/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file game_instance.cpp Implementation of GameInstance. */

#include "../stdafx.h"
#include "../debug.h"
#include "../saveload/saveload.h"

#include "../script/squirrel_class.hpp"

#include "../script/script_storage.hpp"
#include "game_config.hpp"
#include "game_info.hpp"
#include "game_instance.hpp"
#include "game.hpp"

/* Convert all Game related classes to Squirrel data.
 * Note: this line is a marker in squirrel_export.sh. Do not change! */
#include "../script/api/game/game_accounting.hpp.sq"
#include "../script/api/game/game_controller.hpp.sq"
#include "../script/api/game/game_error.hpp.sq"
#include "../script/api/game/game_event.hpp.sq"
#include "../script/api/game/game_execmode.hpp.sq"
#include "../script/api/game/game_list.hpp.sq"
#include "../script/api/game/game_log.hpp.sq"
#include "../script/api/game/game_testmode.hpp.sq"


GameInstance::GameInstance() :
	ScriptInstance("GS")
{}

void GameInstance::Initialize(GameInfo *info)
{
	/* Register the GameController */
	SQGSController_Register(this->engine);

	ScriptInstance::Initialize(info->GetMainScript(), info->GetInstanceName());
}

void GameInstance::RegisterAPI()
{
	ScriptInstance::RegisterAPI();

/* Register all classes */
	SQGSList_Register(this->engine);
	SQGSAccounting_Register(this->engine);
	SQGSError_Register(this->engine);
	SQGSEvent_Register(this->engine);
	SQGSEventController_Register(this->engine);
	SQGSExecMode_Register(this->engine);
	SQGSLog_Register(this->engine);
	SQGSTestMode_Register(this->engine);

}

int GameInstance::GetSetting(const char *name)
{
	return GameConfig::GetConfig()->GetSetting(name);
}

ScriptInfo *GameInstance::FindLibrary(const char *library, int version)
{
	/* 'import' is not supported with GameScripts */
	return NULL;
}

/**
 * DoCommand callback function for all commands executed by Game Scripts.
 * @param result The result of the command.
 * @param tile The tile on which the command was executed.
 * @param p1 p1 as given to DoCommandPInternal.
 * @param p2 p2 as given to DoCommandPInternal.
 */
void CcGame(const CommandCost &result, TileIndex tile, uint32 p1, uint32 p2)
{
	Game::GetGameInstance()->DoCommandCallback(result, tile, p1, p2);
	Game::GetGameInstance()->Continue();
}

CommandCallback *GameInstance::GetDoCommandCallback()
{
	return &CcGame;
}
