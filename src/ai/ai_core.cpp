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
#include "../framerate_type.h"
#include "ai_scanner.hpp"
#include "ai_instance.hpp"
#include "ai_config.hpp"
#include "ai_info.hpp"
#include "ai.hpp"

#include "../safeguards.h"

/* static */ uint AI::frame_counter = 0;
/* static */ AIScannerInfo *AI::scanner_info = NULL;
/* static */ AIScannerLibrary *AI::scanner_library = NULL;

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

	AIConfig *config = AIConfig::GetConfig(company, AIConfig::SSS_FORCE_GAME);
	AIInfo *info = config->GetInfo();
	if (info == NULL || (rerandomise_ai && config->IsRandom())) {
		info = AI::scanner_info->SelectRandomAI();
		assert(info != NULL);
		/* Load default data and store the name in the settings */
		config->Change(info->GetName(), -1, false, true);
	}
	config->AnchorUnchangeableSettings();

	Backup<CompanyByte> cur_company(_current_company, company, FILE_LINE);
	Company *c = Company::Get(company);

	c->ai_info = info;
	assert(c->ai_instance == NULL);
	c->ai_instance = new AIInstance();
	c->ai_instance->Initialize(info);

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
			PerformanceMeasurer framerate((PerformanceElement)(PFE_AI0 + c->index));
			cur_company.Change(c->index);
			c->ai_instance->GameLoop();
		} else {
			PerformanceMeasurer::SetInactive((PerformanceElement)(PFE_AI0 + c->index));
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
	PerformanceMeasurer::SetInactive((PerformanceElement)(PFE_AI0 + company));

	Backup<CompanyByte> cur_company(_current_company, company, FILE_LINE);
	Company *c = Company::Get(company);

	delete c->ai_instance;
	c->ai_instance = NULL;
	c->ai_info = NULL;

	cur_company.Restore();

	InvalidateWindowData(WC_AI_DEBUG, 0, -1);
	DeleteWindowById(WC_AI_SETTINGS, company);
}

/* static */ void AI::Pause(CompanyID company)
{
	/* The reason why dedicated servers are forbidden to execute this
	 * command is not because it is unsafe, but because there is no way
	 * for the server owner to unpause the script again. */
	if (_network_dedicated) return;

	Backup<CompanyByte> cur_company(_current_company, company, FILE_LINE);
	Company::Get(company)->ai_instance->Pause();

	cur_company.Restore();
}

/* static */ void AI::Unpause(CompanyID company)
{
	Backup<CompanyByte> cur_company(_current_company, company, FILE_LINE);
	Company::Get(company)->ai_instance->Unpause();

	cur_company.Restore();
}

/* static */ bool AI::IsPaused(CompanyID company)
{
	Backup<CompanyByte> cur_company(_current_company, company, FILE_LINE);
	bool paused = Company::Get(company)->ai_instance->IsPaused();

	cur_company.Restore();

	return paused;
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
	if (AI::scanner_info != NULL) AI::Uninitialize(true);

	AI::frame_counter = 0;
	if (AI::scanner_info == NULL) {
		TarScanner::DoScan(TarScanner::AI);
		AI::scanner_info = new AIScannerInfo();
		AI::scanner_info->Initialize();
		AI::scanner_library = new AIScannerLibrary();
		AI::scanner_library->Initialize();
	}
}

/* static */ void AI::Uninitialize(bool keepConfig)
{
	AI::KillAll();

	if (keepConfig) {
		/* Run a rescan, which indexes all AIInfos again, and check if we can
		 *  still load all the AIS, while keeping the configs in place */
		Rescan();
	} else {
		delete AI::scanner_info;
		delete AI::scanner_library;
		AI::scanner_info = NULL;
		AI::scanner_library = NULL;

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
	/* Check for both newgame as current game if we can reload the AIInfo inside
	 *  the AIConfig. If not, remove the AI from the list (which will assign
	 *  a random new AI on reload). */
	for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
		if (_settings_game.ai_config[c] != NULL && _settings_game.ai_config[c]->HasScript()) {
			if (!_settings_game.ai_config[c]->ResetInfo(true)) {
				DEBUG(script, 0, "After a reload, the AI by the name '%s' was no longer found, and removed from the list.", _settings_game.ai_config[c]->GetName());
				_settings_game.ai_config[c]->Change(NULL);
				if (Company::IsValidAiID(c)) {
					/* The code belonging to an already running AI was deleted. We can only do
					 * one thing here to keep everything sane and that is kill the AI. After
					 * killing the offending AI we start a random other one in it's place, just
					 * like what would happen if the AI was missing during loading. */
					AI::Stop(c);
					AI::StartNew(c, false);
				}
			} else if (Company::IsValidAiID(c)) {
				/* Update the reference in the Company struct. */
				Company::Get(c)->ai_info = _settings_game.ai_config[c]->GetInfo();
			}
		}
		if (_settings_newgame.ai_config[c] != NULL && _settings_newgame.ai_config[c]->HasScript()) {
			if (!_settings_newgame.ai_config[c]->ResetInfo(false)) {
				DEBUG(script, 0, "After a reload, the AI by the name '%s' was no longer found, and removed from the list.", _settings_newgame.ai_config[c]->GetName());
				_settings_newgame.ai_config[c]->Change(NULL);
			}
		}
	}
}

/* static */ void AI::NewEvent(CompanyID company, ScriptEvent *event)
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
	Company::Get(_current_company)->ai_instance->InsertEvent(event);
	cur_company.Restore();

	event->Release();
}

/* static */ void AI::BroadcastNewEvent(ScriptEvent *event, CompanyID skip_company)
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
		if (!Company::IsValidID(c)) return AIConfig::GetConfig(c, AIConfig::SSS_FORCE_GAME)->GetSetting("start_date");
	}

	/* Currently no AI can be started, check again in a year. */
	return DAYS_IN_YEAR;
}

/* static */ char *AI::GetConsoleList(char *p, const char *last, bool newest_only)
{
	return AI::scanner_info->GetConsoleList(p, last, newest_only);
}

/* static */ char *AI::GetConsoleLibraryList(char *p, const char *last)
{
	 return AI::scanner_library->GetConsoleList(p, last, true);
}

/* static */ const ScriptInfoList *AI::GetInfoList()
{
	return AI::scanner_info->GetInfoList();
}

/* static */ const ScriptInfoList *AI::GetUniqueInfoList()
{
	return AI::scanner_info->GetUniqueInfoList();
}

/* static */ AIInfo *AI::FindInfo(const char *name, int version, bool force_exact_match)
{
	return AI::scanner_info->FindInfo(name, version, force_exact_match);
}

/* static */ AILibrary *AI::FindLibrary(const char *library, int version)
{
	return AI::scanner_library->FindLibrary(library, version);
}

/* static */ void AI::Rescan()
{
	TarScanner::DoScan(TarScanner::AI);

	AI::scanner_info->RescanDir();
	AI::scanner_library->RescanDir();
	ResetConfig();

	InvalidateWindowData(WC_AI_LIST, 0, 1);
	SetWindowClassesDirty(WC_AI_DEBUG);
	InvalidateWindowClassesData(WC_AI_SETTINGS);
}

#if defined(ENABLE_NETWORK)

/**
 * Check whether we have an AI (library) with the exact characteristics as ci.
 * @param ci the characteristics to search on (shortname and md5sum)
 * @param md5sum whether to check the MD5 checksum
 * @return true iff we have an AI (library) matching.
 */
/* static */ bool AI::HasAI(const ContentInfo *ci, bool md5sum)
{
	return AI::scanner_info->HasScript(ci, md5sum);
}

/* static */ bool AI::HasAILibrary(const ContentInfo *ci, bool md5sum)
{
	return AI::scanner_library->HasScript(ci, md5sum);
}

#endif /* defined(ENABLE_NETWORK) */

/* static */ AIScannerInfo *AI::GetScannerInfo()
{
	return AI::scanner_info;
}

/* static */ AIScannerLibrary *AI::GetScannerLibrary()
{
	return AI::scanner_library;
}

