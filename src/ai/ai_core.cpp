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
/* static */ AIScannerInfo *AI::scanner_info = nullptr;
/* static */ AIScannerLibrary *AI::scanner_library = nullptr;

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
	if (info == nullptr || (rerandomise_ai && config->IsRandom())) {
		info = AI::scanner_info->SelectRandomAI();
		assert(info != nullptr);
		/* Load default data and store the name in the settings */
		config->Change(info->GetName(), -1, false, true);
	}
	config->AnchorUnchangeableSettings();

	Backup<CompanyID> cur_company(_current_company, company, FILE_LINE);
	Company *c = Company::Get(company);

	c->ai_info = info;
	assert(c->ai_instance == nullptr);
	c->ai_instance = new AIInstance();
	c->ai_instance->Initialize(info);
	c->ai_instance->LoadOnStack(config->GetToLoadData());
	config->SetToLoadData(nullptr);

	cur_company.Restore();

	InvalidateWindowClassesData(WC_SCRIPT_DEBUG, -1);
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

	Backup<CompanyID> cur_company(_current_company, FILE_LINE);
	for (const Company *c : Company::Iterate()) {
		if (c->is_ai) {
			PerformanceMeasurer framerate((PerformanceElement)(PFE_AI0 + c->index));
			cur_company.Change(c->index);
			c->ai_instance->GameLoop();
			/* Occasionally collect garbage; every 255 ticks do one company.
			 * Effectively collecting garbage once every two months per AI. */
			if ((AI::frame_counter & 255) == 0 && (CompanyID)GB(AI::frame_counter, 8, 4) == c->index) {
				c->ai_instance->CollectGarbage();
			}
		} else {
			PerformanceMeasurer::SetInactive((PerformanceElement)(PFE_AI0 + c->index));
		}
	}
	cur_company.Restore();
}

/* static */ uint AI::GetTick()
{
	return AI::frame_counter;
}

/* static */ void AI::Stop(CompanyID company)
{
	if (_networking && !_network_server) return;
	PerformanceMeasurer::SetInactive((PerformanceElement)(PFE_AI0 + company));

	Backup<CompanyID> cur_company(_current_company, company, FILE_LINE);
	Company *c = Company::Get(company);

	delete c->ai_instance;
	c->ai_instance = nullptr;
	c->ai_info = nullptr;

	cur_company.Restore();

	InvalidateWindowClassesData(WC_SCRIPT_DEBUG, -1);
	CloseWindowById(WC_SCRIPT_SETTINGS, company);
}

/* static */ void AI::Pause(CompanyID company)
{
	/* The reason why dedicated servers are forbidden to execute this
	 * command is not because it is unsafe, but because there is no way
	 * for the server owner to unpause the script again. */
	if (_network_dedicated) return;

	Backup<CompanyID> cur_company(_current_company, company, FILE_LINE);
	Company::Get(company)->ai_instance->Pause();

	cur_company.Restore();
}

/* static */ void AI::Unpause(CompanyID company)
{
	Backup<CompanyID> cur_company(_current_company, company, FILE_LINE);
	Company::Get(company)->ai_instance->Unpause();

	cur_company.Restore();
}

/* static */ bool AI::IsPaused(CompanyID company)
{
	Backup<CompanyID> cur_company(_current_company, company, FILE_LINE);
	bool paused = Company::Get(company)->ai_instance->IsPaused();

	cur_company.Restore();

	return paused;
}

/* static */ void AI::KillAll()
{
	/* It might happen there are no companies .. than we have nothing to loop */
	if (Company::GetPoolSize() == 0) return;

	for (const Company *c : Company::Iterate()) {
		if (c->is_ai) AI::Stop(c->index);
	}
}

/* static */ void AI::Initialize()
{
	if (AI::scanner_info != nullptr) AI::Uninitialize(true);

	AI::frame_counter = 0;
	if (AI::scanner_info == nullptr) {
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
		AI::scanner_info = nullptr;
		AI::scanner_library = nullptr;

		for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
			if (_settings_game.ai_config[c] != nullptr) {
				delete _settings_game.ai_config[c];
				_settings_game.ai_config[c] = nullptr;
			}
			if (_settings_newgame.ai_config[c] != nullptr) {
				delete _settings_newgame.ai_config[c];
				_settings_newgame.ai_config[c] = nullptr;
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
		if (_settings_game.ai_config[c] != nullptr && _settings_game.ai_config[c]->HasScript()) {
			if (!_settings_game.ai_config[c]->ResetInfo(true)) {
				Debug(script, 0, "After a reload, the AI by the name '{}' was no longer found, and removed from the list.", _settings_game.ai_config[c]->GetName());
				_settings_game.ai_config[c]->Change(std::nullopt);
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
		if (_settings_newgame.ai_config[c] != nullptr && _settings_newgame.ai_config[c]->HasScript()) {
			if (!_settings_newgame.ai_config[c]->ResetInfo(false)) {
				Debug(script, 0, "After a reload, the AI by the name '{}' was no longer found, and removed from the list.", _settings_newgame.ai_config[c]->GetName());
				_settings_newgame.ai_config[c]->Change(std::nullopt);
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
	Backup<CompanyID> cur_company(_current_company, company, FILE_LINE);
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
		assert(c != nullptr && c->ai_instance != nullptr);

		Backup<CompanyID> cur_company(_current_company, company, FILE_LINE);
		c->ai_instance->Save();
		cur_company.Restore();
	} else {
		AIInstance::SaveEmpty();
	}
}

/* static */ void AI::GetConsoleList(std::back_insert_iterator<std::string> &output_iterator, bool newest_only)
{
	AI::scanner_info->GetConsoleList(output_iterator, newest_only);
}

/* static */ void AI::GetConsoleLibraryList(std::back_insert_iterator<std::string> &output_iterator)
{
	 AI::scanner_library->GetConsoleList(output_iterator, true);
}

/* static */ const ScriptInfoList *AI::GetInfoList()
{
	return AI::scanner_info->GetInfoList();
}

/* static */ const ScriptInfoList *AI::GetUniqueInfoList()
{
	return AI::scanner_info->GetUniqueInfoList();
}

/* static */ AIInfo *AI::FindInfo(const std::string &name, int version, bool force_exact_match)
{
	return AI::scanner_info->FindInfo(name, version, force_exact_match);
}

/* static */ AILibrary *AI::FindLibrary(const std::string &library, int version)
{
	return AI::scanner_library->FindLibrary(library, version);
}

/* static */ void AI::Rescan()
{
	TarScanner::DoScan(TarScanner::AI);

	AI::scanner_info->RescanDir();
	AI::scanner_library->RescanDir();
	ResetConfig();

	InvalidateWindowData(WC_SCRIPT_LIST, 0, 1);
	SetWindowClassesDirty(WC_SCRIPT_DEBUG);
	InvalidateWindowClassesData(WC_SCRIPT_SETTINGS);
}

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

/* static */ AIScannerInfo *AI::GetScannerInfo()
{
	return AI::scanner_info;
}

/* static */ AIScannerLibrary *AI::GetScannerLibrary()
{
	return AI::scanner_library;
}

