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

static char _ai_saveload_name[64];
static int  _ai_saveload_version;
static char _ai_saveload_settings[1024];
static bool _ai_saveload_is_random;

static const SaveLoad _ai_company[] = {
	    SLEG_STR(_ai_saveload_name,        SLE_STRB),
	    SLEG_STR(_ai_saveload_settings,    SLE_STRB),
	SLEG_CONDVAR(_ai_saveload_version,   SLE_UINT32, 108, SL_MAX_VERSION),
	SLEG_CONDVAR(_ai_saveload_is_random,   SLE_BOOL, 136, SL_MAX_VERSION),
	     SLE_END()
};

#ifdef ENABLE_AI
#include "../ai/ai.hpp"
#include "../ai/ai_config.hpp"
#include "../network/network.h"
#include "../ai/ai_instance.hpp"

static void SaveReal_AIPL(int *index_ptr)
{
	CompanyID index = (CompanyID)*index_ptr;
	AIConfig *config = AIConfig::GetConfig(index);

	if (config->HasScript()) {
		ttd_strlcpy(_ai_saveload_name, config->GetName(), lengthof(_ai_saveload_name));
		_ai_saveload_version = config->GetVersion();
	} else {
		/* No AI is configured for this so store an empty string as name. */
		_ai_saveload_name[0] = '\0';
		_ai_saveload_version = -1;
	}

	_ai_saveload_is_random = config->IsRandom();
	_ai_saveload_settings[0] = '\0';
	config->SettingsToString(_ai_saveload_settings, lengthof(_ai_saveload_settings));

	SlObject(NULL, _ai_company);
	/* If the AI was active, store his data too */
	if (Company::IsValidAiID(index)) AI::Save(index);
}

static void Load_AIPL()
{
	/* Free all current data */
	for (CompanyID c = COMPANY_FIRST; c < MAX_COMPANIES; c++) {
		AIConfig::GetConfig(c, AIConfig::SSS_FORCE_GAME)->Change(NULL);
	}

	CompanyID index;
	while ((index = (CompanyID)SlIterateArray()) != (CompanyID)-1) {
		if (index >= MAX_COMPANIES) SlErrorCorrupt("Too many AI configs");

		_ai_saveload_version = -1;
		SlObject(NULL, _ai_company);

		if (_networking && !_network_server) {
			if (Company::IsValidAiID(index)) AIInstance::LoadEmpty();
			continue;
		}

		AIConfig *config = AIConfig::GetConfig(index, AIConfig::SSS_FORCE_GAME);
		if (StrEmpty(_ai_saveload_name)) {
			/* A random AI. */
			config->Change(NULL, -1, false, true);
		} else {
			config->Change(_ai_saveload_name, _ai_saveload_version, false, _ai_saveload_is_random);
			if (!config->HasScript()) {
				/* No version of the AI available that can load the data. Try to load the
				 * latest version of the AI instead. */
				config->Change(_ai_saveload_name, -1, false, _ai_saveload_is_random);
				if (!config->HasScript()) {
					if (strcmp(_ai_saveload_name, "%_dummy") != 0) {
						DEBUG(ai, 0, "The savegame has an AI by the name '%s', version %d which is no longer available.", _ai_saveload_name, _ai_saveload_version);
						DEBUG(ai, 0, "A random other AI will be loaded in its place.");
					} else {
						DEBUG(ai, 0, "The savegame had no AIs available at the time of saving.");
						DEBUG(ai, 0, "A random available AI will be loaded now.");
					}
				} else {
					DEBUG(ai, 0, "The savegame has an AI by the name '%s', version %d which is no longer available.", _ai_saveload_name, _ai_saveload_version);
					DEBUG(ai, 0, "The latest version of that AI has been loaded instead, but it'll not get the savegame data as it's incompatible.");
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
	{ 'AIPL', Save_AIPL, Load_AIPL, NULL, NULL, CH_ARRAY | CH_LAST},
};
#else

/** The type of the data that follows in the savegame. */
enum SQSaveLoadType {
	SQSL_INT             = 0x00, ///< The following data is an integer.
	SQSL_STRING          = 0x01, ///< The following data is an string.
	SQSL_ARRAY           = 0x02, ///< The following data is an array.
	SQSL_TABLE           = 0x03, ///< The following data is an table.
	SQSL_BOOL            = 0x04, ///< The following data is a boolean.
	SQSL_NULL            = 0x05, ///< A null variable.
	SQSL_ARRAY_TABLE_END = 0xFF, ///< Marks the end of an array or table, no data follows.
};

static byte _ai_sl_byte;

static const SaveLoad _ai_byte[] = {
	SLEG_VAR(_ai_sl_byte, SLE_UINT8),
	SLE_END()
};

static bool LoadObjects()
{
	SlObject(NULL, _ai_byte);
	switch (_ai_sl_byte) {
		case SQSL_INT: {
			int value;
			SlArray(&value, 1, SLE_INT32);
			return true;
		}

		case SQSL_STRING: {
			SlObject(NULL, _ai_byte);
			static char buf[256];
			SlArray(buf, _ai_sl_byte, SLE_CHAR);
			return true;
		}

		case SQSL_ARRAY:
			while (LoadObjects()) { }
			return true;

		case SQSL_TABLE:
			while (LoadObjects()) { LoadObjects(); }
			return true;

		case SQSL_BOOL:
			SlObject(NULL, _ai_byte);
			return true;

		case SQSL_NULL:
			return true;

		case SQSL_ARRAY_TABLE_END:
			return false;

		default: SlErrorCorrupt("Invalid AI data type");
	}
}

static void Load_AIPL()
{
	CompanyID index;
	while ((index = (CompanyID)SlIterateArray()) != (CompanyID)-1) {
		SlObject(NULL, _ai_company);

		if (!Company::IsValidAiID(index)) continue;
		SlObject(NULL, _ai_byte);
		/* Check if there was anything saved at all. */
		if (_ai_sl_byte == 0) continue;
		LoadObjects();
	}
}

extern const ChunkHandler _ai_chunk_handlers[] = {
	{ 'AIPL', NULL, Load_AIPL, NULL, NULL, CH_ARRAY | CH_LAST},
};
#endif /* ENABLE_AI */
