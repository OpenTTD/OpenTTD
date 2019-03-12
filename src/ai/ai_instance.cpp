/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_instance.cpp Implementation of AIInstance. */

#include "../stdafx.h"
#include "../debug.h"
#include "../error.h"

#include "../script/squirrel_class.hpp"

#include "ai_config.hpp"
#include "ai_gui.hpp"
#include "ai.hpp"

#include "../script/script_storage.hpp"
#include "ai_info.hpp"
#include "ai_instance.hpp"

/* Manually include the Text glue. */
#include "../script/api/template/template_text.hpp.sq"

/* Convert all AI related classes to Squirrel data. */
#include "../script/api/ai/ai_includes.hpp"

#include "../company_base.h"
#include "../company_func.h"

#include "../safeguards.h"

AIInstance::AIInstance() :
	ScriptInstance("AI")
{}

void AIInstance::Initialize(AIInfo *info)
{
	this->versionAPI = info->GetAPIVersion();

	/* Register the AIController (including the "import" command) */
	SQAIController_Register(this->engine);

	ScriptInstance::Initialize(info->GetMainScript(), info->GetInstanceName(), _current_company);
}

void AIInstance::RegisterAPI()
{
	ScriptInstance::RegisterAPI();

	/* Register all classes */
	SQAI_RegisterAll(this->engine);

	if (!this->LoadCompatibilityScripts(this->versionAPI, AI_DIR)) this->Died();
}

void AIInstance::Died()
{
	ScriptInstance::Died();

	ShowAIDebugWindow(_current_company);

	const AIInfo *info = AIConfig::GetConfig(_current_company, AIConfig::SSS_FORCE_GAME)->GetInfo();
	if (info != nullptr) {
		ShowErrorMessage(STR_ERROR_AI_PLEASE_REPORT_CRASH, INVALID_STRING_ID, WL_WARNING);

		if (info->GetURL() != nullptr) {
			ScriptLog::Info("Please report the error to the following URL:");
			ScriptLog::Info(info->GetURL());
		}
	}
}

void AIInstance::LoadDummyScript()
{
	ScriptAllocatorScope alloc_scope(this->engine);
	extern void Script_CreateDummy(HSQUIRRELVM vm, StringID string, const char *type);
	Script_CreateDummy(this->engine->GetVM(), STR_ERROR_AI_NO_AI_FOUND, "AI");
}

int AIInstance::GetSetting(const char *name)
{
	return AIConfig::GetConfig(_current_company)->GetSetting(name);
}

ScriptInfo *AIInstance::FindLibrary(const char *library, int version)
{
	return (ScriptInfo *)AI::FindLibrary(library, version);
}

/**
 * DoCommand callback function for all commands executed by AIs.
 * @param result The result of the command.
 * @param tile The tile on which the command was executed.
 * @param p1 p1 as given to DoCommandPInternal.
 * @param p2 p2 as given to DoCommandPInternal.
 * @param cmd cmd as given to DoCommandPInternal.
 */
void CcAI(const CommandCost &result, TileIndex tile, uint32 p1, uint32 p2, uint32 cmd)
{
	/*
	 * The company might not exist anymore. Check for this.
	 * The command checks are not useful since this callback
	 * is also called when the command fails, which is does
	 * when the company does not exist anymore.
	 */
	const Company *c = Company::GetIfValid(_current_company);
	if (c == nullptr || c->ai_instance == nullptr) return;

	if (c->ai_instance->DoCommandCallback(result, tile, p1, p2, cmd)) {
		c->ai_instance->Continue();
	}
}

CommandCallback *AIInstance::GetDoCommandCallback()
{
	return &CcAI;
}
