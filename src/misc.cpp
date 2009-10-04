/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file misc.cpp Misc functions that shouldn't be here. */

#include "stdafx.h"
#include "openttd.h"
#include "landscape.h"
#include "news_func.h"
#include "variables.h"
#include "ai/ai.hpp"
#include "newgrf_house.h"
#include "group.h"
#include "economy_func.h"
#include "date_func.h"
#include "texteff.hpp"
#include "gfx_func.h"
#include "gamelog.h"
#include "animated_tile_func.h"
#include "tilehighlight_func.h"
#include "network/network_func.h"
#include "window_func.h"

#include "table/sprites.h"

extern TileIndex _cur_tileloop_tile;
extern void MakeNewgameSettingsLive();

void InitializeSound();
void InitializeVehicles();
void InitializeDepots();
void InitializeEngineRenews();
void InitializeOrders();
void InitializeClearLand();
void InitializeRailGui();
void InitializeRoadGui();
void InitializeAirportGui();
void InitializeDockGui();
void InitializeIndustries();
void InitializeTowns();
void InitializeSubsidies();
void InitializeTrees();
void InitializeSigns();
void InitializeStations();
void InitializeRoadStops();
void InitializeCargoPackets();
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

	SetObjectToPlace(SPR_CURSOR_ZZZ, PAL_NONE, HT_NONE, WC_MAIN_WINDOW, 0);

	_pause_mode = PM_UNPAUSED;
	_fast_forward = 0;
	_tick_counter = 0;
	_realtime_tick = 0;
	_date_fract = 0;
	_cur_tileloop_tile = 0;
	_thd.redsq = INVALID_TILE;
	if (reset_settings) MakeNewgameSettingsLive();

	InitializeSound();

	if (reset_date) {
		SetDate(ConvertYMDToDate(_settings_game.game_creation.starting_year, 0, 1));
		InitializeOldNames();
	}

	InitializeEngineRenews();
	InitializeVehicles();
	InitializeDepots();
	InitializeOrders();
	InitializeGroup();

	InitNewsItemStructs();
	InitializeLandscape();
	InitializeClearLand();
	InitializeRailGui();
	InitializeRoadGui();
	InitializeAirportGui();
	InitializeDockGui();
	InitializeTowns();
	InitializeSubsidies();
	InitializeTrees();
	InitializeSigns();
	InitializeStations();
	InitializeRoadStops();
	InitializeCargoPackets();
	InitializeIndustries();
	InitializeBuildingCounts();

	InitializeNPF();

	InitializeCompanies();
	AI::Initialize();
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
