/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file game_sl.cpp Handles the saveload part of the GameScripts */

#include "../stdafx.h"
#include "../debug.h"
#include "saveload.h"
#include "../string_func.h"

#include "../game/game.hpp"
#include "../game/game_config.hpp"
#include "../network/network.h"
#include "../game/game_instance.hpp"
#include "../game/game_text.hpp"

#include "../safeguards.h"

static char _game_saveload_name[64];
static int  _game_saveload_version;
static char _game_saveload_settings[1024];
static bool _game_saveload_is_random;

static const SaveLoad _game_script[] = {
	    SLEG_STR(_game_saveload_name,        SLE_STRB),
	    SLEG_STR(_game_saveload_settings,    SLE_STRB),
	    SLEG_VAR(_game_saveload_version,   SLE_UINT32),
	    SLEG_VAR(_game_saveload_is_random,   SLE_BOOL),
	     SLE_END()
};

static void SaveReal_GSDT(int *index_ptr)
{
	GameConfig *config = GameConfig::GetConfig();

	if (config->HasScript()) {
		strecpy(_game_saveload_name, config->GetName(), lastof(_game_saveload_name));
		_game_saveload_version = config->GetVersion();
	} else {
		/* No GameScript is configured for this so store an empty string as name. */
		_game_saveload_name[0] = '\0';
		_game_saveload_version = -1;
	}

	_game_saveload_is_random = config->IsRandom();
	_game_saveload_settings[0] = '\0';
	config->SettingsToString(_game_saveload_settings, lastof(_game_saveload_settings));

	SlObject(NULL, _game_script);
	Game::Save();
}

static void Load_GSDT()
{
	/* Free all current data */
	GameConfig::GetConfig(GameConfig::SSS_FORCE_GAME)->Change(NULL);

	if ((CompanyID)SlIterateArray() == (CompanyID)-1) return;

	_game_saveload_version = -1;
	SlObject(NULL, _game_script);

	if (_networking && !_network_server) {
		GameInstance::LoadEmpty();
		if ((CompanyID)SlIterateArray() != (CompanyID)-1) SlErrorCorrupt("Too many GameScript configs");
		return;
	}

	GameConfig *config = GameConfig::GetConfig(GameConfig::SSS_FORCE_GAME);
	if (StrEmpty(_game_saveload_name)) {
	} else {
		config->Change(_game_saveload_name, _game_saveload_version, false, _game_saveload_is_random);
		if (!config->HasScript()) {
			/* No version of the GameScript available that can load the data. Try to load the
			 * latest version of the GameScript instead. */
			config->Change(_game_saveload_name, -1, false, _game_saveload_is_random);
			if (!config->HasScript()) {
				if (strcmp(_game_saveload_name, "%_dummy") != 0) {
					DEBUG(script, 0, "The savegame has an GameScript by the name '%s', version %d which is no longer available.", _game_saveload_name, _game_saveload_version);
					DEBUG(script, 0, "This game will continue to run without GameScript.");
				} else {
					DEBUG(script, 0, "The savegame had no GameScript available at the time of saving.");
					DEBUG(script, 0, "This game will continue to run without GameScript.");
				}
			} else {
				DEBUG(script, 0, "The savegame has an GameScript by the name '%s', version %d which is no longer available.", _game_saveload_name, _game_saveload_version);
				DEBUG(script, 0, "The latest version of that GameScript has been loaded instead, but it'll not get the savegame data as it's incompatible.");
			}
			/* Make sure the GameScript doesn't get the saveload data, as he was not the
			 *  writer of the saveload data in the first place */
			_game_saveload_version = -1;
		}
	}

	config->StringToSettings(_game_saveload_settings);

	/* Start the GameScript directly if it was active in the savegame */
	Game::StartNew();
	Game::Load(_game_saveload_version);

	if ((CompanyID)SlIterateArray() != (CompanyID)-1) SlErrorCorrupt("Too many GameScript configs");
}

static void Save_GSDT()
{
	SlSetArrayIndex(0);
	SlAutolength((AutolengthProc *)SaveReal_GSDT, NULL);
}

extern GameStrings *_current_data;

static const char *_game_saveload_string;
static uint _game_saveload_strings;

static const SaveLoad _game_language_header[] = {
	SLEG_STR(_game_saveload_string, SLE_STR),
	SLEG_VAR(_game_saveload_strings, SLE_UINT32),
	 SLE_END()
};

static const SaveLoad _game_language_string[] = {
	SLEG_STR(_game_saveload_string, SLE_STR | SLF_ALLOW_CONTROL),
	 SLE_END()
};

static void SaveReal_GSTR(LanguageStrings *ls)
{
	_game_saveload_string  = ls->language;
	_game_saveload_strings = ls->lines.size();

	SlObject(NULL, _game_language_header);
	for (uint i = 0; i < _game_saveload_strings; i++) {
		_game_saveload_string = ls->lines[i];
		SlObject(NULL, _game_language_string);
	}
}

static void Load_GSTR()
{
	delete _current_data;
	_current_data = new GameStrings();

	while (SlIterateArray() != -1) {
		_game_saveload_string = NULL;
		SlObject(NULL, _game_language_header);

		LanguageStrings *ls = new LanguageStrings(_game_saveload_string != NULL ? _game_saveload_string : "");
		for (uint i = 0; i < _game_saveload_strings; i++) {
			SlObject(NULL, _game_language_string);
			ls->lines.push_back(stredup(_game_saveload_string != NULL ? _game_saveload_string : ""));
		}

		_current_data->raw_strings.push_back(ls);
	}

	/* If there were no strings in the savegame, set GameStrings to NULL */
	if (_current_data->raw_strings.size() == 0) {
		delete _current_data;
		_current_data = NULL;
		return;
	}

	_current_data->Compile();
	ReconsiderGameScriptLanguage();
}

static void Save_GSTR()
{
	if (_current_data == NULL) return;

	for (uint i = 0; i < _current_data->raw_strings.size(); i++) {
		SlSetArrayIndex(i);
		SlAutolength((AutolengthProc *)SaveReal_GSTR, _current_data->raw_strings[i]);
	}
}

extern const ChunkHandler _game_chunk_handlers[] = {
	{ 'GSTR', Save_GSTR, Load_GSTR, NULL, NULL, CH_ARRAY },
	{ 'GSDT', Save_GSDT, Load_GSDT, NULL, NULL, CH_ARRAY | CH_LAST},
};
