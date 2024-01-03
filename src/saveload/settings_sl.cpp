/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file settings_sl.cpp Handles the saveload part of the settings. */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/settings_sl_compat.h"

#include "../settings_type.h"
#include "../settings_table.h"
#include "../network/network.h"
#include "../fios.h"

#include "../safeguards.h"

/**
 * Prepare for reading and old diff_custom by zero-ing the memory.
 */
void PrepareOldDiffCustom()
{
	memset(_old_diff_custom, 0, sizeof(_old_diff_custom));
}

/**
 * Reading of the old diff_custom array and transforming it to the new format.
 * @param savegame is it read from the config or savegame. In the latter case
 *                 we are sure there is an array; in the former case we have
 *                 to check that.
 */
void HandleOldDiffCustom(bool savegame)
{
	/* Savegames before v4 didn't have "town_council_tolerance" in savegame yet. */
	bool has_no_town_council_tolerance = savegame && IsSavegameVersionBefore(SLV_4);
	uint options_to_load = GAME_DIFFICULTY_NUM - (has_no_town_council_tolerance ? 1 : 0);

	if (!savegame) {
		/* If we did read to old_diff_custom, then at least one value must be non 0. */
		bool old_diff_custom_used = false;
		for (uint i = 0; i < options_to_load && !old_diff_custom_used; i++) {
			old_diff_custom_used = (_old_diff_custom[i] != 0);
		}

		if (!old_diff_custom_used) return;
	}

	/* Iterate over all the old difficulty settings, and convert the list-value to the new setting. */
	uint i = 0;
	for (const auto &name : _old_diff_settings) {
		if (has_no_town_council_tolerance && name == "town_council_tolerance") continue;

		std::string fullname = "difficulty." + name;
		const SettingDesc *sd = GetSettingFromName(fullname);

		/* Some settings are no longer in use; skip reading those. */
		if (sd == nullptr) {
			i++;
			continue;
		}

		int32_t value = (int32_t)((name == "max_loan" ? 1000 : 1) * _old_diff_custom[i++]);
		sd->AsIntSetting()->MakeValueValidAndWrite(savegame ? &_settings_game : &_settings_newgame, value);
	}
}

/**
 * Get the SaveLoad description for the SettingTable.
 * @param settings SettingDesc struct containing all information.
 * @param is_loading True iff the SaveLoad table is for loading.
 * @return Vector with SaveLoad entries for the SettingTable.
 */
static std::vector<SaveLoad> GetSettingsDesc(const SettingTable &settings, bool is_loading)
{
	std::vector<SaveLoad> saveloads;
	for (auto &desc : settings) {
		const SettingDesc *sd = GetSettingDesc(desc);
		if (sd->flags & SF_NOT_IN_SAVE) continue;

		if (is_loading && (sd->flags & SF_NO_NETWORK_SYNC) && _networking && !_network_server) {
			if (IsSavegameVersionBefore(SLV_TABLE_CHUNKS)) {
				/* We don't want to read this setting, so we do need to skip over it. */
				saveloads.push_back({sd->GetName(), sd->save.cmd, GetVarFileType(sd->save.conv) | SLE_VAR_NULL, sd->save.length, sd->save.version_from, sd->save.version_to, 0, nullptr, 0, nullptr});
			}
			continue;
		}

		saveloads.push_back(sd->save);
	}

	return saveloads;
}

/**
 * Save and load handler for settings
 * @param settings SettingDesc struct containing all information
 * @param object can be either nullptr in which case we load global variables or
 * a pointer to a struct which is getting saved
 */
static void LoadSettings(const SettingTable &settings, void *object, const SaveLoadCompatTable &slct)
{
	const std::vector<SaveLoad> slt = SlCompatTableHeader(GetSettingsDesc(settings, true), slct);

	if (!IsSavegameVersionBefore(SLV_RIFF_TO_ARRAY) && SlIterateArray() == -1) return;
	SlObject(object, slt);
	if (!IsSavegameVersionBefore(SLV_RIFF_TO_ARRAY) && SlIterateArray() != -1) SlErrorCorrupt("Too many settings entries");

	/* Ensure all IntSettings are valid (min/max could have changed between versions etc). */
	for (auto &desc : settings) {
		const SettingDesc *sd = GetSettingDesc(desc);
		if (sd->flags & SF_NOT_IN_SAVE) continue;
		if ((sd->flags & SF_NO_NETWORK_SYNC) && _networking && !_network_server) continue;
		if (!SlIsObjectCurrentlyValid(sd->save.version_from, sd->save.version_to)) continue;

		if (sd->IsIntSetting()) {
			const IntSettingDesc *int_setting = sd->AsIntSetting();
			int_setting->MakeValueValidAndWrite(object, int_setting->Read(object));
		}
	}
}

/**
 * Save and load handler for settings
 * @param settings SettingDesc struct containing all information
 * @param object can be either nullptr in which case we load global variables or
 * a pointer to a struct which is getting saved
 */
static void SaveSettings(const SettingTable &settings, void *object)
{
	const std::vector<SaveLoad> slt = GetSettingsDesc(settings, false);

	SlTableHeader(slt);

	SlSetArrayIndex(0);
	SlObject(object, slt);
}

struct OPTSChunkHandler : ChunkHandler {
	OPTSChunkHandler() : ChunkHandler('OPTS', CH_READONLY) {}

	void Load() const override
	{
		/* Copy over default setting since some might not get loaded in
		 * a networking environment. This ensures for example that the local
		 * autosave-frequency stays when joining a network-server */
		PrepareOldDiffCustom();
		LoadSettings(_old_gameopt_settings, &_settings_game, _gameopt_sl_compat);
		HandleOldDiffCustom(true);
	}
};

struct PATSChunkHandler : ChunkHandler {
	PATSChunkHandler() : ChunkHandler('PATS', CH_TABLE) {}

	/**
	 * Create a single table with all settings that should be stored/loaded
	 * in the savegame.
	 */
	SettingTable GetSettingTable() const
	{
		static const SettingTable saveload_settings_tables[] = {
			_difficulty_settings,
			_economy_settings,
			_game_settings,
			_linkgraph_settings,
			_locale_settings,
			_pathfinding_settings,
			_script_settings,
			_world_settings,
		};
		static std::vector<SettingVariant> settings_table;

		if (settings_table.empty()) {
			for (auto &saveload_settings_table : saveload_settings_tables) {
				for (auto &saveload_setting : saveload_settings_table) {
					settings_table.push_back(saveload_setting);
				}
			}
		}

		return settings_table;
	}

	void Load() const override
	{
		/* Copy over default setting since some might not get loaded in
		 * a networking environment. This ensures for example that the local
		 * currency setting stays when joining a network-server */
		LoadSettings(this->GetSettingTable(), &_settings_game, _settings_sl_compat);
	}

	void LoadCheck(size_t) const override
	{
		LoadSettings(this->GetSettingTable(), &_load_check_data.settings, _settings_sl_compat);
	}

	void Save() const override
	{
		SaveSettings(this->GetSettingTable(), &_settings_game);
	}
};

static const OPTSChunkHandler OPTS;
static const PATSChunkHandler PATS;
static const ChunkHandlerRef setting_chunk_handlers[] = {
	OPTS,
	PATS,
};

extern const ChunkHandlerTable _setting_chunk_handlers(setting_chunk_handlers);
