/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_config.cpp Implementation of AIConfig. */

#include "../stdafx.h"
#include "../settings_type.h"
#include "../string_func.h"
#include "ai.hpp"
#include "ai_config.hpp"
#include "ai_info.hpp"

#include "../safeguards.h"

/** Configuration for AI start date, every AI has this setting. */
ScriptConfigItem _start_date_config = {
	"start_date",
	"", // STR_AI_SETTINGS_START_DELAY
	AI::START_NEXT_MIN,
	AI::START_NEXT_MAX,
	AI::START_NEXT_MEDIUM,
	AI::START_NEXT_EASY,
	AI::START_NEXT_MEDIUM,
	AI::START_NEXT_HARD,
	AI::START_NEXT_DEVIATION,
	30,
	SCRIPTCONFIG_NONE,
	nullptr,
	false
};

AIConfig::AIConfig(const AIConfig *config) : ScriptConfig(config)
{
	/* Override start_date as per AIConfig::AddRandomDeviation().
	 * This is necessary because the ScriptConfig constructor will instead call
	 * ScriptConfig::AddRandomDeviation(). */
	int start_date = config->GetSetting("start_date");
	this->SetSetting("start_date", start_date != 0 ? std::max(1, this->GetSetting("start_date")) : 0);
}

/* static */ AIConfig *AIConfig::GetConfig(CompanyID company, ScriptSettingSource source)
{
	AIConfig **config;
	if (source == SSS_FORCE_NEWGAME || (source == SSS_DEFAULT && _game_mode == GM_MENU)) {
		config = &_settings_newgame.ai_config[company];
	} else {
		config = &_settings_game.ai_config[company];
	}
	if (*config == nullptr) *config = new AIConfig();
	return *config;
}

class AIInfo *AIConfig::GetInfo() const
{
	return static_cast<class AIInfo *>(ScriptConfig::GetInfo());
}

ScriptInfo *AIConfig::FindInfo(const char *name, int version, bool force_exact_match)
{
	return static_cast<ScriptInfo *>(AI::FindInfo(name, version, force_exact_match));
}

bool AIConfig::ResetInfo(bool force_exact_match)
{
	this->info = (ScriptInfo *)AI::FindInfo(this->name, force_exact_match ? this->version : -1, force_exact_match);
	return this->info != nullptr;
}

void AIConfig::PushExtraConfigList()
{
	this->config_list->push_back(_start_date_config);
}

void AIConfig::ClearConfigList()
{
	/* The special casing for start_date is here to ensure that the
	 *  start_date setting won't change even if you chose another Script. */
	int start_date = this->GetSetting("start_date");

	ScriptConfig::ClearConfigList();

	this->SetSetting("start_date", start_date);
}

int AIConfig::GetSetting(const char *name) const
{
	if (this->info == nullptr) {
		SettingValueList::const_iterator it = this->settings.find(name);
		if (it == this->settings.end()) {
			assert(strcmp("start_date", name) == 0);
			switch (GetGameSettings().script.settings_profile) {
				case SP_EASY:   return AI::START_NEXT_EASY;
				case SP_MEDIUM: return AI::START_NEXT_MEDIUM;
				case SP_HARD:   return AI::START_NEXT_HARD;
				case SP_CUSTOM: return AI::START_NEXT_MEDIUM;
				default: NOT_REACHED();
			}
		}

		return (*it).second;
	}

	return ScriptConfig::GetSetting(name);
}

void AIConfig::SetSetting(const char *name, int value)
{
	if (this->info == nullptr) {
		if (strcmp("start_date", name) != 0) return;
		value = Clamp(value, AI::START_NEXT_MIN, AI::START_NEXT_MAX);

		SettingValueList::iterator it = this->settings.find(name);
		if (it != this->settings.end()) {
			(*it).second = value;
		} else {
			this->settings[stredup(name)] = value;
		}

		return;
	}

	ScriptConfig::SetSetting(name, value);
}

void AIConfig::AddRandomDeviation()
{
	int start_date = this->GetSetting("start_date");

	ScriptConfig::AddRandomDeviation();

	/* start_date = 0 is a special case, where random deviation does not occur.
	 * If start_date was not already 0, then a minimum value of 1 must apply. */
	this->SetSetting("start_date", start_date != 0 ? std::max(1, this->GetSetting("start_date")) : 0);
}
