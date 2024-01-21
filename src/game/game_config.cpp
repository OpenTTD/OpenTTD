/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file game_config.cpp Implementation of GameConfig. */

#include "../stdafx.h"
#include "../settings_type.h"
#include "game.h"
#include "game_config.h"
#include "game_info.h"

#include "../safeguards.h"

/* static */ GameConfig *GameConfig::GetConfig(ScriptSettingSource source)
{
	GameConfig **config;
	if (source == SSS_FORCE_NEWGAME || (source == SSS_DEFAULT && _game_mode == GM_MENU)) {
		config = &_settings_newgame.game_config;
	} else {
		config = &_settings_game.game_config;
	}
	if (*config == nullptr) *config = new GameConfig();
	return *config;
}

class GameInfo *GameConfig::GetInfo() const
{
	return static_cast<class GameInfo *>(ScriptConfig::GetInfo());
}

ScriptInfo *GameConfig::FindInfo(const std::string &name, int version, bool force_exact_match)
{
	return static_cast<ScriptInfo *>(Game::FindInfo(name, version, force_exact_match));
}

bool GameConfig::ResetInfo(bool force_exact_match)
{
	this->info = (ScriptInfo *)Game::FindInfo(this->name, force_exact_match ? this->version : -1, force_exact_match);
	return this->info != nullptr;
}
