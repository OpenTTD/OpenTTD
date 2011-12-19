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
#include "../company_base.h"
#include "../company_func.h"
#include "../network/network.h"
#include "game.hpp"
#include "game_instance.hpp"

/* static */ uint Game::frame_counter = 0;
/* static */ GameInstance *Game::instance = NULL;

/* static */ void Game::GameLoop()
{
	if (_networking && !_network_server) return;

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
	if (Game::instance != NULL) Game::Uninitialize();

	Game::frame_counter = 0;
	if (Game::instance == NULL) {
		/* Clients shouldn't start GameScripts */
		if (_networking && !_network_server) return;

		Backup<CompanyByte> cur_company(_current_company, FILE_LINE);
		cur_company.Change(OWNER_DEITY);

		Game::instance = new GameInstance();
		Game::instance->Initialize();

		cur_company.Restore();
	}
}

/* static */ void Game::Uninitialize()
{
	delete Game::instance;
	Game::instance = NULL;
}
