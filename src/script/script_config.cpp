/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_config.cpp Implementation of ScriptConfig. */

#include "../stdafx.h"
#include "../settings_type.h"
#include "../core/random_func.hpp"
#include "script_info.hpp"
#include "api/script_object.hpp"
#include "../textfile_gui.h"
#include "../string_func.h"

#include "../safeguards.h"

void ScriptConfig::Change(std::optional<const std::string> name, int version, bool force_exact_match, bool is_random)
{
	if (name.has_value()) {
		this->name = std::move(name.value());
		this->info = this->FindInfo(this->name, version, force_exact_match);
	} else {
		this->info = nullptr;
	}
	this->version = (info == nullptr) ? -1 : info->GetVersion();
	this->is_random = is_random;
	this->config_list.reset();
	this->to_load_data.reset();

	this->ClearConfigList();

	if (_game_mode == GM_NORMAL && this->info != nullptr) {
		/* If we're in an existing game and the Script is changed, set all settings
		 *  for the Script that have the random flag to a random value. */
		for (const auto &item : *this->info->GetConfigList()) {
			if (item.flags & SCRIPTCONFIG_RANDOM) {
				this->SetSetting(item.name, ScriptObject::GetRandomizer(OWNER_NONE).Next(item.max_value + 1 - item.min_value) + item.min_value);
			}
		}

		this->AddRandomDeviation();
	}
}

ScriptConfig::ScriptConfig(const ScriptConfig *config)
{
	this->name = config->name;
	this->info = config->info;
	this->version = config->version;
	this->is_random = config->is_random;
	this->to_load_data.reset();

	for (const auto &item : config->settings) {
		this->settings[item.first] = item.second;
	}

	/* Virtual functions get called statically in constructors, so make it explicit to remove any confusion. */
	this->ScriptConfig::AddRandomDeviation();
}

ScriptConfig::~ScriptConfig()
{
	this->ResetSettings();
	this->to_load_data.reset();
}

ScriptInfo *ScriptConfig::GetInfo() const
{
	return this->info;
}

const ScriptConfigItemList *ScriptConfig::GetConfigList()
{
	if (this->info != nullptr) return this->info->GetConfigList();
	if (this->config_list == nullptr) {
		this->config_list = std::make_unique<ScriptConfigItemList>();
	}
	return this->config_list.get();
}

void ScriptConfig::ClearConfigList()
{
	this->settings.clear();
}

void ScriptConfig::AnchorUnchangeableSettings()
{
	for (const auto &item : *this->GetConfigList()) {
		if ((item.flags & SCRIPTCONFIG_INGAME) == 0) {
			this->SetSetting(item.name, this->GetSetting(item.name));
		}
	}
}

int ScriptConfig::GetSetting(const std::string &name) const
{
	const auto it = this->settings.find(name);
	if (it == this->settings.end()) return this->info->GetSettingDefaultValue(name);
	return (*it).second;
}

void ScriptConfig::SetSetting(const std::string &name, int value)
{
	/* You can only set Script specific settings if an Script is selected. */
	if (this->info == nullptr) return;

	const ScriptConfigItem *config_item = this->info->GetConfigItem(name);
	if (config_item == nullptr) return;

	value = Clamp(value, config_item->min_value, config_item->max_value);

	this->settings[name] = value;
}

void ScriptConfig::ResetSettings()
{
	this->settings.clear();
}

void ScriptConfig::ResetEditableSettings(bool yet_to_start)
{
	if (this->info == nullptr) return ResetSettings();

	for (SettingValueList::iterator it = this->settings.begin(); it != this->settings.end();) {
		const ScriptConfigItem *config_item = this->info->GetConfigItem(it->first);
		assert(config_item != nullptr);

		bool editable = yet_to_start || (config_item->flags & SCRIPTCONFIG_INGAME) != 0;
		bool visible = _settings_client.gui.ai_developer_tools || (config_item->flags & SCRIPTCONFIG_DEVELOPER) == 0;

		if (editable && visible) {
			it = this->settings.erase(it);
		} else {
			it++;
		}
	}
}

void ScriptConfig::AddRandomDeviation()
{
	for (const auto &item : *this->GetConfigList()) {
		if (item.random_deviation != 0) {
			this->SetSetting(item.name, ScriptObject::GetRandomizer(OWNER_NONE).Next(item.random_deviation * 2 + 1) - item.random_deviation + this->GetSetting(item.name));
		}
	}
}

bool ScriptConfig::HasScript() const
{
	return this->info != nullptr;
}

bool ScriptConfig::IsRandom() const
{
	return this->is_random;
}

const std::string &ScriptConfig::GetName() const
{
	return this->name;
}

int ScriptConfig::GetVersion() const
{
	return this->version;
}

void ScriptConfig::StringToSettings(const std::string &value)
{
	char *value_copy = stredup(value.c_str());
	char *s = value_copy;

	while (s != nullptr) {
		/* Analyze the string ('name=value,name=value\0') */
		char *item_name = s;
		s = strchr(s, '=');
		if (s == nullptr) break;
		if (*s == '\0') break;
		*s = '\0';
		s++;

		char *item_value = s;
		s = strchr(s, ',');
		if (s != nullptr) {
			*s = '\0';
			s++;
		}

		this->SetSetting(item_name, atoi(item_value));
	}
	free(value_copy);
}

std::string ScriptConfig::SettingsToString() const
{
	if (this->settings.empty()) return {};

	std::string result;
	for (const auto &item : this->settings) {
		fmt::format_to(std::back_inserter(result), "{}={},", item.first, item.second);
	}

	/* Remove the last ','. */
	result.resize(result.size() - 1);
	return result;
}

std::optional<std::string> ScriptConfig::GetTextfile(TextfileType type, CompanyID slot) const
{
	if (slot == INVALID_COMPANY || this->GetInfo() == nullptr) return std::nullopt;

	return ::GetTextfile(type, (slot == OWNER_DEITY) ? GAME_DIR : AI_DIR, this->GetInfo()->GetMainScript());
}

void ScriptConfig::SetToLoadData(ScriptInstance::ScriptData *data)
{
	this->to_load_data.reset(data);
}

ScriptInstance::ScriptData *ScriptConfig::GetToLoadData()
{
	return this->to_load_data.get();
}

