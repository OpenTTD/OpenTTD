/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_sl.cpp Handles the saveload part of the AIs */

#include "../stdafx.h"
#include "../company_base.h"
#include "../debug.h"
#include "saveload.h"
#include "../string_func.h"

#include "../ai/ai.hpp"
#include "../ai/ai_config.hpp"
#include "../network/network.h"
#include "../ai/ai_instance.hpp"

#include "../safeguards.h"

static char _ai_saveload_name[64];
static int  _ai_saveload_version;
static char _ai_saveload_settings[1024];
static bool _ai_saveload_is_random;

static const SaveLoad _ai_company[] = {
	    SLEG_STR(_ai_saveload_name,        SLE_STRB),
	    SLEG_STR(_ai_saveload_settings,    SLE_STRB),
	SLEG_CONDVAR(_ai_saveload_version,   SLE_UINT32, SLV_108, SL_MAX_VERSION),
	SLEG_CONDVAR(_ai_saveload_is_random,   SLE_BOOL, SLV_136, SL_MAX_VERSION),
	     SLE_END()
};

static void SaveReal_AIPL(int *index_ptr)
{
	CompanyID index = (CompanyID)*index_ptr;
	AIConfig *config = AIConfig::GetConfig(index);

	if (config->HasScript()) {
		strecpy(_ai_saveload_name, config->GetName(), lastof(_ai_saveload_name));
		_ai_saveload_version = config->GetVersion();
	} else {
		/* No AI is configured for this so store an empty string as name. */
		_ai_saveload_name[0] = '\0';
		_ai_saveload_version = -1;
	}

	_ai_saveload_is_random = config->IsRandom();
	_ai_saveload_settings[0] = '\0';
	config->SettingsToString(_ai_saveload_settings, lastof(_ai_saveload_settings));

	SlObject(nullptr, _ai_company);
	/* If the AI was active, store his data too */
	if (Company::IsValidAiID(index)) AI::Save(index);
}

static void Load_AIPL()
{
	/* Free all current data */
	for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
		AIConfig::GetConfig(c, AIConfig::SSS_FORCE_GAME)->Change(nullptr);
	}

	CompanyID index;
	while ((index = (CompanyID)SlIterateArray()) != (CompanyID)-1) {
		if (index >= MAX_COMPANIES) SlErrorCorrupt("Too many AI configs");

		_ai_saveload_is_random = 0;
		_ai_saveload_version = -1;
		SlObject(nullptr, _ai_company);

		if (_networking && !_network_server) {
			if (Company::IsValidAiID(index)) AIInstance::LoadEmpty();
			continue;
		}

		AIConfig *config = AIConfig::GetConfig(index, AIConfig::SSS_FORCE_GAME);
		if (StrEmpty(_ai_saveload_name)) {
			/* A random AI. */
			config->Change(nullptr, -1, false, true);
		} else {
			config->Change(_ai_saveload_name, _ai_saveload_version, false, _ai_saveload_is_random);
			if (!config->HasScript()) {
				/* No version of the AI available that can load the data. Try to load the
				 * latest version of the AI instead. */
				config->Change(_ai_saveload_name, -1, false, _ai_saveload_is_random);
				if (!config->HasScript()) {
					if (strcmp(_ai_saveload_name, "%_dummy") != 0) {
						DEBUG(script, 0, "The savegame has an AI by the name '%s', version %d which is no longer available.", _ai_saveload_name, _ai_saveload_version);
						DEBUG(script, 0, "A random other AI will be loaded in its place.");
					} else {
						DEBUG(script, 0, "The savegame had no AIs available at the time of saving.");
						DEBUG(script, 0, "A random available AI will be loaded now.");
					}
				} else {
					DEBUG(script, 0, "The savegame has an AI by the name '%s', version %d which is no longer available.", _ai_saveload_name, _ai_saveload_version);
					DEBUG(script, 0, "The latest version of that AI has been loaded instead, but it'll not get the savegame data as it's incompatible.");
				}
				/* Make sure the AI doesn't get the saveload data, as he was not the
				 *  writer of the saveload data in the first place */
				_ai_saveload_version = -1;
			}
		}

		config->StringToSettings(_ai_saveload_settings);

		/* Start the AI directly if it was active in the savegame */
		if (Company::IsValidAiID(index)) {
			AI::StartNew(index, false);
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
	{ 'AIPL', Save_AIPL, Load_AIPL, nullptr, nullptr, CH_ARRAY | CH_LAST},
};
