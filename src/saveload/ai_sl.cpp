/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_sl.cpp Handles the saveload part of the AIs */

#include "../stdafx.h"
#include "../debug.h"

#include "saveload.h"
#include "compat/ai_sl_compat.h"

#include "../company_base.h"
#include "../string_func.h"

#include "../ai/ai.hpp"
#include "../ai/ai_config.hpp"
#include "../network/network.h"
#include "../ai/ai_instance.hpp"

#include "../safeguards.h"

static std::string _ai_saveload_name;
static int         _ai_saveload_version;
static std::string _ai_saveload_settings;
static bool        _ai_saveload_is_random;

static const SaveLoad _ai_company_desc[] = {
	   SLEG_SSTR("name",      _ai_saveload_name,         SLE_STR),
	   SLEG_SSTR("settings",  _ai_saveload_settings,     SLE_STR),
	SLEG_CONDVAR("version",   _ai_saveload_version,   SLE_UINT32, SLV_108, SL_MAX_VERSION),
	SLEG_CONDVAR("is_random", _ai_saveload_is_random,   SLE_BOOL, SLV_136, SL_MAX_VERSION),
};

static void SaveReal_AIPL(int *index_ptr)
{
	CompanyID index = (CompanyID)*index_ptr;
	AIConfig *config = AIConfig::GetConfig(index);

	if (config->HasScript()) {
		_ai_saveload_name = config->GetName();
		_ai_saveload_version = config->GetVersion();
	} else {
		/* No AI is configured for this so store an empty string as name. */
		_ai_saveload_name.clear();
		_ai_saveload_version = -1;
	}

	_ai_saveload_is_random = config->IsRandom();
	_ai_saveload_settings = config->SettingsToString();

	SlObject(nullptr, _ai_company_desc);
	/* If the AI was active, store its data too */
	if (Company::IsValidAiID(index)) AI::Save(index);
}

struct AIPLChunkHandler : ChunkHandler {
	AIPLChunkHandler() : ChunkHandler('AIPL', CH_TABLE) {}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_ai_company_desc, _ai_company_sl_compat);

		/* Free all current data */
		for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
			AIConfig::GetConfig(c, AIConfig::SSS_FORCE_GAME)->Change(std::nullopt);
		}

		CompanyID index;
		while ((index = (CompanyID)SlIterateArray()) != (CompanyID)-1) {
			if (index >= MAX_COMPANIES) SlErrorCorrupt("Too many AI configs");

			_ai_saveload_is_random = false;
			_ai_saveload_version = -1;
			SlObject(nullptr, slt);

			if (_game_mode == GM_MENU || (_networking && !_network_server)) {
				if (Company::IsValidAiID(index)) AIInstance::LoadEmpty();
				continue;
			}

			AIConfig *config = AIConfig::GetConfig(index, AIConfig::SSS_FORCE_GAME);
			if (_ai_saveload_name.empty()) {
				/* A random AI. */
				config->Change(std::nullopt, -1, false, true);
			} else {
				config->Change(_ai_saveload_name, _ai_saveload_version, false, _ai_saveload_is_random, false);
				if (!config->HasScript()) {
					/* No version of the AI available that can load the data. Try to load the
					 * latest version of the AI instead. */
					config->Change(_ai_saveload_name, -1, false, _ai_saveload_is_random, false);
					if (!config->HasScript()) {
						if (_ai_saveload_name.compare("%_dummy") != 0) {
							Debug(script, 0, "The savegame has an AI by the name '{}', version {} which is no longer available.", _ai_saveload_name, _ai_saveload_version);
							Debug(script, 0, "A random other AI will be loaded in its place.");
						} else {
							Debug(script, 0, "The savegame had no AIs available at the time of saving.");
							Debug(script, 0, "A random available AI will be loaded now.");
						}
					} else {
						Debug(script, 0, "The savegame has an AI by the name '{}', version {} which is no longer available.", _ai_saveload_name, _ai_saveload_version);
						Debug(script, 0, "The latest version of that AI has been loaded instead, but it'll not get the savegame data as it's incompatible.");
					}
					/* Make sure the AI doesn't get the saveload data, as it was not the
					 *  writer of the saveload data in the first place */
					_ai_saveload_version = -1;
				}
			}

			config->StringToSettings(_ai_saveload_settings);

			/* Load the AI saved data */
			if (Company::IsValidAiID(index)) config->SetToLoadData(AIInstance::Load(_ai_saveload_version));
		}
	}

	void Save() const override
	{
		SlTableHeader(_ai_company_desc);

		for (int i = COMPANY_FIRST; i < MAX_COMPANIES; i++) {
			SlSetArrayIndex(i);
			SlAutolength((AutolengthProc *)SaveReal_AIPL, &i);
		}
	}
};

static const AIPLChunkHandler AIPL;
static const ChunkHandlerRef ai_chunk_handlers[] = {
	AIPL,
};

extern const ChunkHandlerTable _ai_chunk_handlers(ai_chunk_handlers);
