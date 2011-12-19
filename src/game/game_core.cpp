/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file game_core.cpp Implementation of Game. */

#include "../stdafx.h"
#include "../command_func.h"
#include "../core/backup_type.hpp"
#include "../core/bitmath_func.hpp"
#include "../company_base.h"
#include "../company_func.h"
#include "../network/network.h"
#include "../window_func.h"
#include "../fileio_func.h"
#include "game.hpp"
#include "game_scanner.hpp"
#include "game_config.hpp"
#include "game_instance.hpp"

/* static */ uint Game::frame_counter = 0;
/* static */ GameInfo *Game::info = NULL;
/* static */ GameInstance *Game::instance = NULL;
/* static */ GameScannerInfo *Game::scanner = NULL;

/* static */ void Game::GameLoop()
{
	if (_networking && !_network_server) return;
	if (Game::instance == NULL) return;

	Game::frame_counter++;

	Backup<CompanyByte> cur_company(_current_company, FILE_LINE);
	cur_company.Change(OWNER_DEITY);
	Game::instance->GameLoop();
	cur_company.Restore();

	/* Occasionally collect garbage */
	if ((Game::frame_counter & 255) == 0) {
		Game::instance->CollectGarbage();
	}
}

/* static */ void Game::Initialize()
{
	if (Game::instance != NULL) Game::Uninitialize(true);

	Game::frame_counter = 0;

	if (Game::scanner == NULL) {
		TarScanner::DoScan(TarScanner::GAME);
		Game::scanner = new GameScannerInfo();
		Game::scanner->Initialize();
	}

	if (Game::instance == NULL) {
		/* Clients shouldn't start GameScripts */
		if (_networking && !_network_server) return;

		GameConfig *config = GameConfig::GetConfig();
		GameInfo *info = config->GetInfo();
		if (info == NULL) return;

		Backup<CompanyByte> cur_company(_current_company, FILE_LINE);
		cur_company.Change(OWNER_DEITY);

		Game::info = info;
		Game::instance = new GameInstance();
		Game::instance->Initialize(info);

		cur_company.Restore();

		InvalidateWindowData(WC_AI_DEBUG, 0, -1);
	}
}

/* static */ void Game::Uninitialize(bool keepConfig)
{
	delete Game::instance;
	Game::instance = NULL;

	if (keepConfig) {
		Rescan();
	} else {
		delete Game::scanner;
		Game::scanner = NULL;

		if (_settings_game.game_config != NULL) {
			delete _settings_game.game_config;
			_settings_game.game_config = NULL;
		}
		if (_settings_newgame.game_config != NULL) {
			delete _settings_newgame.game_config;
			_settings_newgame.game_config = NULL;
		}
	}
}

/* static */ void Game::ResetConfig()
{
	/* Check for both newgame as current game if we can reload the GameInfo insde
	 *  the GameConfig. If not, remove the Game from the list. */
	if (_settings_game.game_config != NULL && _settings_game.game_config->HasScript()) {
		if (!_settings_game.game_config->ResetInfo(true)) {
			DEBUG(script, 0, "After a reload, the GameScript by the name '%s' was no longer found, and removed from the list.", _settings_game.game_config->GetName());
			_settings_game.game_config->Change(NULL);
			if (Game::instance != NULL) {
				delete Game::instance;
				Game::instance = NULL;
			}
		} else if (Game::instance != NULL) {
			Game::info = _settings_game.game_config->GetInfo();
		}
	}
	if (_settings_newgame.game_config != NULL && _settings_newgame.game_config->HasScript()) {
		if (!_settings_newgame.game_config->ResetInfo(false)) {
			DEBUG(script, 0, "After a reload, the GameScript by the name '%s' was no longer found, and removed from the list.", _settings_newgame.game_config->GetName());
			_settings_newgame.game_config->Change(NULL);
		}
	}
}

/* static */ void Game::Rescan()
{
	TarScanner::DoScan(TarScanner::GAME);

	Game::scanner->RescanDir();
	ResetConfig();

	InvalidateWindowData(WC_AI_LIST, 0, 1);
	SetWindowClassesDirty(WC_AI_DEBUG);
	SetWindowDirty(WC_AI_SETTINGS, 0);
}


/* static */ char *Game::GetConsoleList(char *p, const char *last, bool newest_only)
{
	return Game::scanner->GetConsoleList(p, last, newest_only);
}

/* static */ const ScriptInfoList *Game::GetInfoList()
{
	return Game::scanner->GetInfoList();
}

/* static */ const ScriptInfoList *Game::GetUniqueInfoList()
{
	return Game::scanner->GetUniqueInfoList();
}

/* static */ GameInfo *Game::FindInfo(const char *name, int version, bool force_exact_match)
{
	return Game::scanner->FindInfo(name, version, force_exact_match);
}
