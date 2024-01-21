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

/* static */ AIConfig *AIConfig::GetConfig(CompanyID company, ScriptSettingSource source)
{
	assert(company < MAX_COMPANIES);

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

ScriptInfo *AIConfig::FindInfo(const std::string &name, int version, bool force_exact_match)
{
	return static_cast<ScriptInfo *>(AI::FindInfo(name, version, force_exact_match));
}

bool AIConfig::ResetInfo(bool force_exact_match)
{
	this->info = (ScriptInfo *)AI::FindInfo(this->name, force_exact_match ? this->version : -1, force_exact_match);
	return this->info != nullptr;
}
