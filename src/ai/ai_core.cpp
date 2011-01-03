/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_core.cpp Implementation of AI. */

#include "../stdafx.h"
#include "../core/backup_type.hpp"
#include "../core/bitmath_func.hpp"
#include "../company_base.h"
#include "../company_func.h"
#include "../network/network.h"
#include "../window_func.h"
#include "../command_func.h"
#include "ai_scanner.hpp"
#include "ai_instance.hpp"
#include "ai_config.hpp"
#include "api/ai_error.hpp"

/* static */ uint AI::frame_counter = 0;
/* static */ AIScanner *AI::ai_scanner = NULL;

/* static */ bool AI::CanStartNew()
{
	/* Only allow new AIs on the server and only when that is allowed in multiplayer */
	return !_networking || (_network_server && _settings_game.ai.ai_in_multiplayer);
}

/* static */ void AI::StartNew(CompanyID company, bool rerandomise_ai)
{
	assert(Company::IsValidID(company));

	/* Clients shouldn't start AIs */
	if (_networking && !_network_server) return;

	AIConfig *config = AIConfig::GetConfig(company);
	AIInfo *info = config->GetInfo();
	if (info == NULL || (rerandomise_ai && config->IsRandomAI())) {
		info = AI::ai_scanner->SelectRandomAI();
		assert(info != NULL);
		/* Load default data and store the name in the settings */
		config->ChangeAI(info->GetName(), -1, false, true);
	}

	Backup<CompanyByte> cur_company(_current_company, company, FILE_LINE);
	Company *c = Company::Get(company);

	c->ai_info = info;
	assert(c->ai_instance == NULL);
	c->ai_instance = new AIInstance(info);

	cur_company.Restore();

	InvalidateWindowData(WC_AI_DEBUG, 0, -1);
	return;
}

/* static */ void AI::GameLoop()
{
	/* If we are in networking, only servers run this function, and that only if it is allowed */
	if (_networking && (!_network_server || !_settings_game.ai.ai_in_multiplayer)) return;

	/* The speed with which AIs go, is limited by the 'competitor_speed' */
	AI::frame_counter++;
	assert(_settings_game.difficulty.competitor_speed <= 4);
	if ((AI::frame_counter & ((1 << (4 - _settings_game.difficulty.competitor_speed)) - 1)) != 0) return;

	Backup<CompanyByte> cur_company(_current_company, FILE_LINE);
	const Company *c;
	FOR_ALL_COMPANIES(c) {
		if (c->is_ai) {
			cur_company.Change(c->index);
			c->ai_instance->GameLoop();
		}
	}
	cur_company.Restore();

	/* Occasionally collect garbage; every 255 ticks do one company.
	 * Effectively collecting garbage once every two months per AI. */
	if ((AI::frame_counter & 255) == 0) {
		CompanyID cid = (CompanyID)GB(AI::frame_counter, 8, 4);
		if (Company::IsValidAiID(cid)) Company::Get(cid)->ai_instance->CollectGarbage();
	}
}

/* static */ uint AI::GetTick()
{
	return AI::frame_counter;
}

/* static */ void AI::Stop(CompanyID company)
{
	if (_networking && !_network_server) return;

	Backup<CompanyByte> cur_company(_current_company, company, FILE_LINE);
	Company *c = Company::Get(company);

	delete c->ai_instance;
	c->ai_instance = NULL;

	cur_company.Restore();

	InvalidateWindowData(WC_AI_DEBUG, 0, -1);
	DeleteWindowById(WC_AI_SETTINGS, company);
}

/* static */ void AI::Suspend(CompanyID company)
{
	if (_networking && !_network_server) return;

	Backup<CompanyByte> cur_company(_current_company, company, FILE_LINE);
	Company::Get(company)->ai_instance->Suspend();

	cur_company.Restore();
}

/* static */ void AI::KillAll()
{
	/* It might happen there are no companies .. than we have nothing to loop */
	if (Company::GetPoolSize() == 0) return;

	const Company *c;
	FOR_ALL_COMPANIES(c) {
		if (c->is_ai) AI::Stop(c->index);
	}
}

/* static */ void AI::Initialize()
{
	if (AI::ai_scanner != NULL) AI::Uninitialize(true);

	AI::frame_counter = 0;
	if (AI::ai_scanner == NULL) AI::ai_scanner = new AIScanner();
}

/* static */ void AI::Uninitialize(bool keepConfig)
{
	AI::KillAll();

	if (keepConfig) {
		/* Run a rescan, which indexes all AIInfos again, and check if we can
		 *  still load all the AIS, while keeping the configs in place */
		Rescan();
	} else {
		delete AI::ai_scanner;
		AI::ai_scanner = NULL;

		for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
			if (_settings_game.ai_config[c] != NULL) {
				delete _settings_game.ai_config[c];
				_settings_game.ai_config[c] = NULL;
			}
			if (_settings_newgame.ai_config[c] != NULL) {
				delete _settings_newgame.ai_config[c];
				_settings_newgame.ai_config[c] = NULL;
			}
		}
	}
}

/* static */ void AI::ResetConfig()
{
	/* Check for both newgame as current game if we can reload the AIInfo insde
	 *  the AIConfig. If not, remove the AI from the list (which will assign
	 *  a random new AI on reload). */
	for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
		if (_settings_game.ai_config[c] != NULL && _settings_game.ai_config[c]->HasAI()) {
			if (!_settings_game.ai_config[c]->ResetInfo()) {
				DEBUG(ai, 0, "After a reload, the AI by the name '%s' was no longer found, and removed from the list.", _settings_game.ai_config[c]->GetName());
				_settings_game.ai_config[c]->ChangeAI(NULL);
			}
		}
		if (_settings_newgame.ai_config[c] != NULL && _settings_newgame.ai_config[c]->HasAI()) {
			if (!_settings_newgame.ai_config[c]->ResetInfo()) {
				DEBUG(ai, 0, "After a reload, the AI by the name '%s' was no longer found, and removed from the list.", _settings_newgame.ai_config[c]->GetName());
				_settings_newgame.ai_config[c]->ChangeAI(NULL);
			}
		}
	}
}

/* static */ void AI::NewEvent(CompanyID company, AIEvent *event)
{
	/* AddRef() and Release() need to be called at least once, so do it here */
	event->AddRef();

	/* Clients should ignore events */
	if (_networking && !_network_server) {
		event->Release();
		return;
	}

	/* Only AIs can have an event-queue */
	if (!Company::IsValidAiID(company)) {
		event->Release();
		return;
	}

	/* Queue the event */
	Backup<CompanyByte> cur_company(_current_company, company, FILE_LINE);
	AIEventController::InsertEvent(event);
	cur_company.Restore();

	event->Release();
}

/* static */ void AI::BroadcastNewEvent(AIEvent *event, CompanyID skip_company)
{
	/* AddRef() and Release() need to be called at least once, so do it here */
	event->AddRef();

	/* Clients should ignore events */
	if (_networking && !_network_server) {
		event->Release();
		return;
	}

	/* Try to send the event to all AIs */
	for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
		if (c != skip_company) AI::NewEvent(c, event);
	}

	event->Release();
}

/**
 * DoCommand callback function for all commands executed by AIs.
 * @param result The result of the command.
 * @param tile The tile on which the command was executed.
 * @param p1 p1 as given to DoCommandPInternal.
 * @param p2 p2 as given to DoCommandPInternal.
 */
void CcAI(const CommandCost &result, TileIndex tile, uint32 p1, uint32 p2)
{
	AIObject::SetLastCommandRes(result.Succeeded());

	if (result.Failed()) {
		AIObject::SetLastError(AIError::StringToError(result.GetErrorMessage()));
	} else {
		AIObject::IncreaseDoCommandCosts(result.GetCost());
		AIObject::SetLastCost(result.GetCost());
	}

	Company::Get(_current_company)->ai_instance->Continue();
}

/* static */ void AI::Save(CompanyID company)
{
	if (!_networking || _network_server) {
		Company *c = Company::GetIfValid(company);
		assert(c != NULL && c->ai_instance != NULL);

		Backup<CompanyByte> cur_company(_current_company, company, FILE_LINE);
		c->ai_instance->Save();
		cur_company.Restore();
	} else {
		AIInstance::SaveEmpty();
	}
}

/* static */ void AI::Load(CompanyID company, int version)
{
	if (!_networking || _network_server) {
		Company *c = Company::GetIfValid(company);
		assert(c != NULL && c->ai_instance != NULL);

		Backup<CompanyByte> cur_company(_current_company, company, FILE_LINE);
		c->ai_instance->Load(version);
		cur_company.Restore();
	} else {
		/* Read, but ignore, the load data */
		AIInstance::LoadEmpty();
	}
}

/* static */ int AI::GetStartNextTime()
{
	/* Find the first company which doesn't exist yet */
	for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
		if (!Company::IsValidID(c)) return AIConfig::GetConfig(c)->GetSetting("start_date");
	}

	/* Currently no AI can be started, check again in a year. */
	return DAYS_IN_YEAR;
}

/* static */ char *AI::GetConsoleList(char *p, const char *last)
{
	return AI::ai_scanner->GetAIConsoleList(p, last);
}

/* static */ char *AI::GetConsoleLibraryList(char *p, const char *last)
{
	 return AI::ai_scanner->GetAIConsoleLibraryList(p, last);
}

/* static */ const AIInfoList *AI::GetInfoList()
{
	return AI::ai_scanner->GetAIInfoList();
}

/* static */ const AIInfoList *AI::GetUniqueInfoList()
{
	return AI::ai_scanner->GetUniqueAIInfoList();
}

/* static */ AIInfo *AI::FindInfo(const char *name, int version, bool force_exact_match)
{
	return AI::ai_scanner->FindInfo(name, version, force_exact_match);
}

/* static */ bool AI::ImportLibrary(const char *library, const char *class_name, int version, HSQUIRRELVM vm)
{
	return AI::ai_scanner->ImportLibrary(library, class_name, version, vm, Company::Get(_current_company)->ai_instance->GetController());
}

/* static */ void AI::Rescan()
{
	AI::ai_scanner->RescanAIDir();
	ResetConfig();
}
