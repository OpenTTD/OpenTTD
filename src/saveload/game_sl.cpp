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
#include "compat/game_sl_compat.h"

#include "../string_func.h"
#include "../game/game.hpp"
#include "../game/game_config.hpp"
#include "../network/network.h"
#include "../game/game_instance.hpp"
#include "../game/game_text.hpp"

#include "../safeguards.h"

static std::string _game_saveload_name;
static int         _game_saveload_version;
static std::string _game_saveload_settings;
static bool        _game_saveload_is_random;

static const SaveLoad _game_script_desc[] = {
	   SLEG_SSTR("name",      _game_saveload_name,         SLE_STR),
	   SLEG_SSTR("settings",  _game_saveload_settings,     SLE_STR),
	    SLEG_VAR("version",   _game_saveload_version,   SLE_UINT32),
	    SLEG_VAR("is_random", _game_saveload_is_random,   SLE_BOOL),
};

static void SaveReal_GSDT(int *)
{
	GameConfig *config = GameConfig::GetConfig();

	if (config->HasScript()) {
		_game_saveload_name = config->GetName();
		_game_saveload_version = config->GetVersion();
	} else {
		/* No GameScript is configured for this so store an empty string as name. */
		_game_saveload_name.clear();
		_game_saveload_version = -1;
	}

	_game_saveload_is_random = config->IsRandom();
	_game_saveload_settings = config->SettingsToString();

	SlObject(nullptr, _game_script_desc);
	Game::Save();
}

struct GSDTChunkHandler : ChunkHandler {
	GSDTChunkHandler() : ChunkHandler('GSDT', CH_TABLE) {}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_game_script_desc, _game_script_sl_compat);

		/* Free all current data */
		GameConfig::GetConfig(GameConfig::SSS_FORCE_GAME)->Change(std::nullopt);

		if (SlIterateArray() == -1) return;

		_game_saveload_version = -1;
		SlObject(nullptr, slt);

		if (_game_mode == GM_MENU || (_networking && !_network_server)) {
			GameInstance::LoadEmpty();
			if (SlIterateArray() != -1) SlErrorCorrupt("Too many GameScript configs");
			return;
		}

		GameConfig *config = GameConfig::GetConfig(GameConfig::SSS_FORCE_GAME);
		if (!_game_saveload_name.empty()) {
			config->Change(_game_saveload_name, _game_saveload_version, false, _game_saveload_is_random);
			if (!config->HasScript()) {
				/* No version of the GameScript available that can load the data. Try to load the
				 * latest version of the GameScript instead. */
				config->Change(_game_saveload_name, -1, false, _game_saveload_is_random);
				if (!config->HasScript()) {
					if (_game_saveload_name.compare("%_dummy") != 0) {
						Debug(script, 0, "The savegame has an GameScript by the name '{}', version {} which is no longer available.", _game_saveload_name, _game_saveload_version);
						Debug(script, 0, "This game will continue to run without GameScript.");
					} else {
						Debug(script, 0, "The savegame had no GameScript available at the time of saving.");
						Debug(script, 0, "This game will continue to run without GameScript.");
					}
				} else {
					Debug(script, 0, "The savegame has an GameScript by the name '{}', version {} which is no longer available.", _game_saveload_name, _game_saveload_version);
					Debug(script, 0, "The latest version of that GameScript has been loaded instead, but it'll not get the savegame data as it's incompatible.");
				}
				/* Make sure the GameScript doesn't get the saveload data, as it was not the
				 *  writer of the saveload data in the first place */
				_game_saveload_version = -1;
			}
		}

		config->StringToSettings(_game_saveload_settings);

		/* Load the GameScript saved data */
		config->SetToLoadData(GameInstance::Load(_game_saveload_version));

		if (SlIterateArray() != -1) SlErrorCorrupt("Too many GameScript configs");
	}

	void Save() const override
	{
		SlTableHeader(_game_script_desc);
		SlSetArrayIndex(0);
		SlAutolength((AutolengthProc *)SaveReal_GSDT, nullptr);
	}
};

extern GameStrings *_current_data;

static std::string _game_saveload_string;
static uint32_t _game_saveload_strings;

class SlGameLanguageString : public DefaultSaveLoadHandler<SlGameLanguageString, LanguageStrings> {
public:
	inline static const SaveLoad description[] = {
		SLEG_SSTR("string", _game_saveload_string, SLE_STR | SLF_ALLOW_CONTROL),
	};
	inline const static SaveLoadCompatTable compat_description = _game_language_string_sl_compat;

	void Save(LanguageStrings *ls) const override
	{
		SlSetStructListLength(ls->lines.size());

		for (const auto &string : ls->lines) {
			_game_saveload_string = string;
			SlObject(nullptr, this->GetDescription());
		}
	}

	void Load(LanguageStrings *ls) const override
	{
		uint32_t length = IsSavegameVersionBefore(SLV_SAVELOAD_LIST_LENGTH) ? _game_saveload_strings : (uint32_t)SlGetStructListLength(UINT32_MAX);

		for (uint32_t i = 0; i < length; i++) {
			SlObject(nullptr, this->GetLoadDescription());
			ls->lines.emplace_back(_game_saveload_string);
		}
	}
};

static const SaveLoad _game_language_desc[] = {
	SLE_SSTR(LanguageStrings, language, SLE_STR),
	SLEG_CONDVAR("count", _game_saveload_strings, SLE_UINT32, SL_MIN_VERSION, SLV_SAVELOAD_LIST_LENGTH),
	SLEG_STRUCTLIST("strings", SlGameLanguageString),
};

struct GSTRChunkHandler : ChunkHandler {
	GSTRChunkHandler() : ChunkHandler('GSTR', CH_TABLE) {}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_game_language_desc, _game_language_sl_compat);

		delete _current_data;
		_current_data = new GameStrings();

		while (SlIterateArray() != -1) {
			LanguageStrings ls;
			SlObject(&ls, slt);
			_current_data->raw_strings.push_back(std::move(ls));
		}

		/* If there were no strings in the savegame, set GameStrings to nullptr */
		if (_current_data->raw_strings.empty()) {
			delete _current_data;
			_current_data = nullptr;
			return;
		}

		_current_data->Compile();
		ReconsiderGameScriptLanguage();
	}

	void Save() const override
	{
		SlTableHeader(_game_language_desc);

		if (_current_data == nullptr) return;

		for (uint i = 0; i < _current_data->raw_strings.size(); i++) {
			SlSetArrayIndex(i);
			SlObject(&_current_data->raw_strings[i], _game_language_desc);
		}
	}
};

static const GSTRChunkHandler GSTR;
static const GSDTChunkHandler GSDT;
static const ChunkHandlerRef game_chunk_handlers[] = {
	GSTR,
	GSDT,
};

extern const ChunkHandlerTable _game_chunk_handlers(game_chunk_handlers);
