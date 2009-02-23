/* $Id$ */

/** @file ai_core.cpp Implementation of AI. */

#include "../stdafx.h"
#include "../core/bitmath_func.hpp"
#include "../company_base.h"
#include "../company_func.h"
#include "../debug.h"
#include "../network/network.h"
#include "../settings_type.h"
#include "../window_func.h"
#include "../command_func.h"
#include "ai.hpp"
#include "ai_scanner.hpp"
#include "ai_instance.hpp"
#include "ai_config.hpp"

/* static */ uint AI::frame_counter = 0;
/* static */ AIScanner *AI::ai_scanner = NULL;

/* static */ bool AI::CanStartNew()
{
	/* Only allow new AIs on the server and only when that is allowed in multiplayer */
	return !_networking || (_network_server && _settings_game.ai.ai_in_multiplayer);
}

/* static */ void AI::StartNew(CompanyID company)
{
	assert(IsValidCompanyID(company));

	/* Clients shouldn't start AIs */
	if (_networking && !_network_server) return;

	AIInfo *info = AIConfig::GetConfig(company)->GetInfo();
	if (info == NULL) {
		info = AI::ai_scanner->SelectRandomAI();
		assert(info != NULL);
		/* Load default data and store the name in the settings */
		AIConfig::GetConfig(company)->ChangeAI(info->GetName());
	}

	_current_company = company;
	Company *c = GetCompany(company);

	c->ai_info = info;
	assert(c->ai_instance == NULL);
	c->ai_instance = new AIInstance(info);

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

	const Company *c;
	FOR_ALL_COMPANIES(c) {
		if (!IsHumanCompany(c->index)) {
			_current_company = c->index;
			c->ai_instance->GameLoop();
		}
	}

	/* Occasionally collect garbage; every 255 ticks do one company.
	 * Effectively collecting garbage once every two months per AI. */
	if ((AI::frame_counter & 255) == 0) {
		CompanyID cid = (CompanyID)GB(AI::frame_counter, 8, 4);
		if (IsValidCompanyID(cid) && !IsHumanCompany(cid)) GetCompany(cid)->ai_instance->CollectGarbage();
	}

	_current_company = OWNER_NONE;
}

/* static */ uint AI::GetTick()
{
	return AI::frame_counter;
}

/* static */ void AI::Stop(CompanyID company)
{
	if (_networking && !_network_server) return;

	CompanyID old_company = _current_company;
	_current_company = company;
	Company *c = GetCompany(company);

	delete c->ai_instance;
	c->ai_instance = NULL;

	_current_company = old_company;

	InvalidateWindowData(WC_AI_DEBUG, 0, -1);
}

/* static */ void AI::KillAll()
{
	/* It might happen there are no companies .. than we have nothing to loop */
	if (GetCompanyPoolSize() == 0) return;

	const Company *c;
	FOR_ALL_COMPANIES(c) {
		if (!IsHumanCompany(c->index)) AI::Stop(c->index);
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
	if (!IsValidCompanyID(company) || IsHumanCompany(company)) {
		event->Release();
		return;
	}

	/* Queue the event */
	CompanyID old_company = _current_company;
	_current_company = company;
	AIEventController::InsertEvent(event);
	_current_company = old_company;

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

void CcAI(bool success, TileIndex tile, uint32 p1, uint32 p2)
{
	AIObject::SetLastCommandRes(success);

	if (!success) {
		AIObject::SetLastError(AIError::StringToError(_error_message));
	} else {
		AIObject::IncreaseDoCommandCosts(AIObject::GetLastCost());
	}

	GetCompany(_current_company)->ai_instance->Continue();
}

/* static */ void AI::Save(CompanyID company)
{
	if (!_networking || _network_server) {
		assert(IsValidCompanyID(company));
		assert(GetCompany(company)->ai_instance != NULL);

		CompanyID old_company = _current_company;
		_current_company = company;
		GetCompany(company)->ai_instance->Save();
		_current_company = old_company;
	} else {
		AIInstance::SaveEmpty();
	}
}

/* static */ void AI::Load(CompanyID company, int version)
{
	if (!_networking || _network_server) {
		assert(IsValidCompanyID(company));
		assert(GetCompany(company)->ai_instance != NULL);

		CompanyID old_company = _current_company;
		_current_company = company;
		GetCompany(company)->ai_instance->Load(version);
		_current_company = old_company;
	} else {
		/* Read, but ignore, the load data */
		AIInstance::LoadEmpty();
	}
}

/* static */ int AI::GetStartNextTime()
{
	/* Find the first company which doesn't exist yet */
	for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
		if (!IsValidCompanyID(c)) return AIConfig::GetConfig(c)->GetSetting("start_date");
	}

	/* Currently no AI can be started, check again in a year. */
	return DAYS_IN_YEAR;
}

/* static */ char *AI::GetConsoleList(char *p, const char *last)
{
	return AI::ai_scanner->GetAIConsoleList(p, last);
}

/* static */ const AIInfoList *AI::GetInfoList()
{
	return AI::ai_scanner->GetAIInfoList();
}

/* static */ const AIInfoList *AI::GetUniqueInfoList()
{
	return AI::ai_scanner->GetUniqueAIInfoList();
}

/* static */ AIInfo *AI::FindInfo(const char *name, int version)
{
	return AI::ai_scanner->FindInfo(name, version);
}

/* static */ bool AI::ImportLibrary(const char *library, const char *class_name, int version, HSQUIRRELVM vm)
{
	return AI::ai_scanner->ImportLibrary(library, class_name, version, vm, GetCompany(_current_company)->ai_instance->GetController());
}

/* static */ void AI::Rescan()
{
	AI::ai_scanner->RescanAIDir();
	ResetConfig();
}
