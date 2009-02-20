/* $Id$ */

/** @file ai_sl.cpp Handles the saveload part of the AIs */

#include "../stdafx.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../debug.h"
#include "saveload.h"
#include "../string_func.h"
#include "../ai/ai.hpp"
#include "../ai/ai_config.hpp"
#include "../network/network.h"
#include "../ai/ai_instance.hpp"

static char _ai_saveload_name[64];
static int  _ai_saveload_version;
static char _ai_saveload_settings[1024];

static const SaveLoad _ai_company[] = {
	SLEG_STR(_ai_saveload_name,        SLE_STRB),
	SLEG_STR(_ai_saveload_settings,    SLE_STRB),
	SLEG_CONDVAR(_ai_saveload_version, SLE_UINT32, 108, SL_MAX_VERSION),
	SLE_END()
};

static void SaveReal_AIPL(int *index_ptr)
{
	CompanyID index = (CompanyID)*index_ptr;
	AIConfig *config = AIConfig::GetConfig(index);

	if (config->HasAI()) {
		ttd_strlcpy(_ai_saveload_name, config->GetName(), lengthof(_ai_saveload_name));
		_ai_saveload_version = config->GetVersion();
	} else {
		/* No AI is configured for this so store an empty string as name. */
		_ai_saveload_name[0] = '\0';
		_ai_saveload_version = -1;
	}

	_ai_saveload_settings[0] = '\0';
	config->SettingsToString(_ai_saveload_settings, lengthof(_ai_saveload_settings));

	SlObject(NULL, _ai_company);
	/* If the AI was active, store his data too */
	if (IsValidCompanyID(index) && !IsHumanCompany(index)) AI::Save(index);
}

static void Load_AIPL()
{
	/* Free all current data */
	for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
		AIConfig::GetConfig(c)->ChangeAI(NULL);
	}

	CompanyID index;
	while ((index = (CompanyID)SlIterateArray()) != (CompanyID)-1) {
		_ai_saveload_version = -1;
		SlObject(NULL, _ai_company);

		if (_networking && !_network_server) {
			if (IsValidCompanyID(index) && !IsHumanCompany(index)) AIInstance::LoadEmpty();
			continue;
		}

		AIConfig *config = AIConfig::GetConfig(index);
		if (StrEmpty(_ai_saveload_name)) {
			/* A random AI. */
			config->ChangeAI(NULL);
		} else {
			config->ChangeAI(_ai_saveload_name, _ai_saveload_version);
			if (!config->HasAI()) {
				if (strcmp(_ai_saveload_name, "%_dummy") != 0) {
					DEBUG(ai, 0, "The savegame has an AI by the name '%s', version %d which is no longer available.", _ai_saveload_name, _ai_saveload_version);
					DEBUG(ai, 0, "A random other AI will be loaded in its place.");
				} else {
					DEBUG(ai, 0, "The savegame had no AIs available at the time of saving.");
					DEBUG(ai, 0, "A random available AI will be loaded now.");
				}
				/* Make sure the AI doesn't get the saveload data, as he was not the
				 *  writer of the saveload data in the first place */
				_ai_saveload_version = -1;
			}
		}

		config->StringToSettings(_ai_saveload_settings);

		/* Start the AI directly if it was active in the savegame */
		if (IsValidCompanyID(index) && !IsHumanCompany(index)) {
			AI::StartNew(index);
			AI::Load(index, _ai_saveload_version);
		}
	}
}

static void Save_AIPL()
{
	for (int i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
		SlSetArrayIndex(i);
		SlAutolength((AutolengthProc *)SaveReal_AIPL, &i);
	}
}

extern const ChunkHandler _ai_chunk_handlers[] = {
	{ 'AIPL', Save_AIPL, Load_AIPL, CH_ARRAY | CH_LAST},
};
