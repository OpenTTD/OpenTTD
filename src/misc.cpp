/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file misc.cpp Misc functions that shouldn't be here. */

#include "stdafx.h"
#include "landscape.h"
#include "news_func.h"
#include "ai/ai.hpp"
#include "ai/ai_gui.hpp"
#include "newgrf.h"
#include "newgrf_house.h"
#include "economy_func.h"
#include "date_func.h"
#include "texteff.hpp"
#include "gfx_func.h"
#include "gamelog.h"
#include "animated_tile_func.h"
#include "tilehighlight_func.h"
#include "network/network_func.h"
#include "window_func.h"
#include "core/pool_type.hpp"
#include "game/game.hpp"
#include "linkgraph/linkgraphschedule.h"

#include "safeguards.h"


extern TileIndex _cur_tileloop_tile;
extern void MakeNewgameSettingsLive();

void InitializeSound();
void InitializeMusic();
void InitializeVehicles();
void InitializeRailGui();
void InitializeRoadGui();
void InitializeAirportGui();
void InitializeDockGui();
void InitializeGraphGui();
void InitializeObjectGui();
void InitializeIndustries();
void InitializeObjects();
void InitializeTrees();
void InitializeCompanies();
void InitializeCheats();
void InitializeNPF();
void InitializeOldNames();

void InitializeGame(uint size_x, uint size_y, bool reset_date, bool reset_settings)
{
	/* Make sure there isn't any window that can influence anything
	 * related to the new game we're about to start/load. */
	UnInitWindowSystem();

	AllocateMap(size_x, size_y);

	_pause_mode = PM_UNPAUSED;
	_fast_forward = 0;
	_tick_counter = 0;
	_cur_tileloop_tile = 1;
	_thd.redsq = INVALID_TILE;
	if (reset_settings) MakeNewgameSettingsLive();

	if (reset_date) {
		SetDate(ConvertYMDToDate(_settings_game.game_creation.starting_year, 0, 1), 0);
		InitializeOldNames();
	}

	LinkGraphSchedule::Clear();
	PoolBase::Clean(PT_NORMAL);

	ResetPersistentNewGRFData();

	InitializeSound();
	InitializeMusic();

	InitializeVehicles();

	InitNewsItemStructs();
	InitializeLandscape();
	InitializeRailGui();
	InitializeRoadGui();
	InitializeAirportGui();
	InitializeDockGui();
	InitializeGraphGui();
	InitializeObjectGui();
	InitializeAIGui();
	InitializeTrees();
	InitializeIndustries();
	InitializeObjects();
	InitializeBuildingCounts();

	InitializeNPF();

	InitializeCompanies();
	AI::Initialize();
	Game::Initialize();
	InitializeCheats();

	InitTextEffects();
#ifdef ENABLE_NETWORK
	NetworkInitChatMessage();
#endif /* ENABLE_NETWORK */
	InitializeAnimatedTiles();

	InitializeEconomy();

	ResetObjectToPlace();

	GamelogReset();
	GamelogStartAction(GLAT_START);
	GamelogRevision();
	GamelogMode();
	GamelogGRFAddList(_grfconfig);
	GamelogStopAction();
}
