/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_game.cpp Implementation of ScriptGame. */

#include "../../stdafx.h"
#include "script_game.hpp"
#include "../../command_type.h"
#include "../../settings_type.h"
#include "../../network/network.h"

#include "../../safeguards.h"

/* static */ bool ScriptGame::Pause()
{
	return ScriptObject::DoCommand(0, PM_PAUSED_GAME_SCRIPT, 1, CMD_PAUSE);
}

/* static */ bool ScriptGame::Unpause()
{
	return ScriptObject::DoCommand(0, PM_PAUSED_GAME_SCRIPT, 0, CMD_PAUSE);
}

/* static */ bool ScriptGame::IsPaused()
{
	return !!_pause_mode;
}

/* static */ ScriptGame::LandscapeType ScriptGame::GetLandscape()
{
	return (ScriptGame::LandscapeType)_settings_game.game_creation.landscape;
}

/* static */ bool ScriptGame::IsMultiplayer()
{
	return _network_server;
}
