/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file game_config.cpp Implementation of GameConfig. */

#include "../stdafx.h"
#include "../settings_type.h"
#include "game.hpp"
#include "game_config.hpp"
#include "game_info.hpp"

#include "../safeguards.h"

/* static */ GameConfig *GameConfig::GetConfig(ScriptSettingSource source)
{
	if (_game_mode == GM_MENU) source = SSS_FORCE_NEWGAME;

	auto &config = (source == SSS_FORCE_NEWGAME) ? _settings_newgame.script_config.game : _settings_game.script_config.game;
	if (config == nullptr) config = std::make_unique<GameConfig>();

	return config.get();
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
