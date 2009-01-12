/* $Id$ */

/** @file ai_sl.cpp Handles the saveload part of the AIs */

#include "../stdafx.h"
#include "../openttd.h"
#include "../company_base.h"
#include "../company_func.h"
#include "../debug.h"
#include "saveload.h"
#include "../settings_type.h"
#include "../string_func.h"
#include "../ai/ai.hpp"
#include "../ai/ai_config.hpp"

static char _ai_saveload_ainame[64];
static char _ai_company_convert_array[1024];

static const SaveLoad _ai_company[] = {
	SLEG_STR(_ai_saveload_ainame,       SLE_STRB),
	SLEG_STR(_ai_company_convert_array, SLE_STRB),
	SLE_END()
};

static void SaveReal_AIPL(int *index_ptr)
{
	CompanyID index = (CompanyID)*index_ptr;
	AIConfig *config = AIConfig::GetConfig(index);

	ttd_strlcpy(_ai_saveload_ainame, config->GetName(), lengthof(_ai_saveload_ainame));

	_ai_company_convert_array[0] = '\0';
	config->SettingsToString(_ai_company_convert_array, lengthof(_ai_company_convert_array));

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
		AIConfig *config = AIConfig::GetConfig(index);
		SlObject(NULL, _ai_company);

		if (_ai_saveload_ainame[0] == '\0' || AI::GetCompanyInfo(_ai_saveload_ainame) == NULL) {
			if (strcmp(_ai_saveload_ainame, "%_dummy") != 0) {
				DEBUG(ai, 0, "The savegame has an AI by the name '%s' which is no longer available.", _ai_saveload_ainame);
				DEBUG(ai, 0, "A random other AI will be loaded in its place.");
			} else {
				DEBUG(ai, 0, "The savegame had no AIs available at the time of saving.");
				DEBUG(ai, 0, "A random available AI will be loaded now.");
			}
			config->ChangeAI(NULL);
		} else {
			config->ChangeAI(_ai_saveload_ainame);
		}

		config->StringToSettings(_ai_company_convert_array);

		/* Start the AI directly if it was active in the savegame */
		if (IsValidCompanyID(index) && !IsHumanCompany(index)) {
			AI::StartNew(index);
			AI::Load(index);
		}
	}
}

static void Save_AIPL()
{
	for (int i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
		if (!AIConfig::GetConfig((CompanyID)i)->HasAI()) continue;

		SlSetArrayIndex(i);
		SlAutolength((AutolengthProc *)SaveReal_AIPL, &i);
	}
}

extern const ChunkHandler _ai_chunk_handlers[] = {
	{ 'AIPL', Save_AIPL, Load_AIPL, CH_ARRAY | CH_LAST},
};
