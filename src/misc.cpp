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
#include "script/script_gui.h"
#include "newgrf.h"
#include "newgrf_house.h"
#include "economy_func.h"
#include "timer/timer_game_calendar.h"
#include "timer/timer_game_economy.h"
#include "timer/timer_game_tick.h"
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
#include "station_kdtree.h"
#include "town_kdtree.h"
#include "viewport_kdtree.h"
#include "newgrf_profiling.h"
#include "3rdparty/monocypher/monocypher.h"

#include "safeguards.h"

std::string _savegame_id; ///< Unique ID of the current savegame.

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
void InitializeTownGui();
void InitializeIndustries();
void InitializeObjects();
void InitializeTrees();
void InitializeCompanies();
void InitializeCheats();
void InitializeNPF();
void InitializeOldNames();

/**
 * Generate an unique ID.
 *
 * It isn't as much of an unique ID but more a hashed digest of a random
 * string and a time. It is very likely to be unique, but it does not follow
 * any UUID standard.
 */
std::string GenerateUid(std::string_view subject)
{
	std::array<uint8_t, 32> random_bytes;
	RandomBytesWithFallback(random_bytes);

	auto current_time = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	std::string coding_string = fmt::format("{}{}", current_time, subject);

	std::array<uint8_t, 16> digest;
	crypto_blake2b_ctx ctx;
	crypto_blake2b_init(&ctx, digest.size());
	crypto_blake2b_update(&ctx, random_bytes.data(), random_bytes.size());
	crypto_blake2b_update(&ctx, reinterpret_cast<const uint8_t *>(coding_string.data()), coding_string.size());
	crypto_blake2b_final(&ctx, digest.data());

	return FormatArrayAsHex(digest);
}

/**
 * Generate an unique savegame ID.
 */
void GenerateSavegameId()
{
	_savegame_id = GenerateUid("OpenTTD Savegame ID");
}

void InitializeGame(uint size_x, uint size_y, bool reset_date, bool reset_settings)
{
	/* Make sure there isn't any window that can influence anything
	 * related to the new game we're about to start/load. */
	UnInitWindowSystem();

	Map::Allocate(size_x, size_y);

	_pause_mode = PM_UNPAUSED;
	_game_speed = 100;
	TimerGameTick::counter = 0;
	_cur_tileloop_tile = 1;
	_thd.redsq = INVALID_TILE;
	if (reset_settings) MakeNewgameSettingsLive();

	_newgrf_profilers.clear();

	if (reset_date) {
		TimerGameCalendar::Date new_date = TimerGameCalendar::ConvertYMDToDate(_settings_game.game_creation.starting_year, 0, 1);
		TimerGameCalendar::SetDate(new_date, 0);
		/* Keep the economy date synced with the calendar date. */
		TimerGameEconomy::SetDate(new_date.base(), 0);
		InitializeOldNames();
	}

	LinkGraphSchedule::Clear();
	PoolBase::Clean(PT_NORMAL);

	RebuildStationKdtree();
	RebuildTownKdtree();
	RebuildViewportKdtree();

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
	InitializeTownGui();
	InitializeScriptGui();
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
	NetworkInitChatMessage();
	InitializeAnimatedTiles();

	InitializeEconomy();

	ResetObjectToPlace();

	_gamelog.Reset();
	_gamelog.StartAction(GLAT_START);
	_gamelog.Revision();
	_gamelog.Mode();
	_gamelog.GRFAddList(_grfconfig);
	_gamelog.StopAction();
}
