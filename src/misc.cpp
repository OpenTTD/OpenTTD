/* $Id$ */

/** @file misc.cpp Misc functions that shouldn't be here. */

#include "stdafx.h"
#include "openttd.h"
#include "landscape.h"
#include "news_func.h"
#include "variables.h"
#include "ai/ai.hpp"
#include "newgrf_house.h"
#include "cargotype.h"
#include "group.h"
#include "economy_func.h"
#include "functions.h"
#include "map_func.h"
#include "date_func.h"
#include "vehicle_func.h"
#include "texteff.hpp"
#include "gfx_func.h"
#include "gamelog.h"
#include "animated_tile_func.h"
#include "settings_type.h"
#include "tilehighlight_func.h"
#include "network/network_func.h"
#include "window_func.h"

#include "table/sprites.h"

extern TileIndex _cur_tileloop_tile;
extern void MakeNewgameSettingsLive();

void InitializeVehicles();
void InitializeWaypoints();
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
void InitializeTrees();
void InitializeSigns();
void InitializeStations();
void InitializeCargoPackets();
void InitializeCompanies();
void InitializeCheats();
void InitializeNPF();
void InitializeOldNames();

void InitializeGame(uint size_x, uint size_y, bool reset_date)
{
	/* Make sure there isn't any window that can influence anything
	 * related to the new game we're about to start/load. */
	UnInitWindowSystem();

	AllocateMap(size_x, size_y);

	SetObjectToPlace(SPR_CURSOR_ZZZ, PAL_NONE, VHM_NONE, WC_MAIN_WINDOW, 0);

	_pause_game = 0;
	_fast_forward = 0;
	_tick_counter = 0;
	_realtime_tick = 0;
	_date_fract = 0;
	_cur_tileloop_tile = 0;
	_thd.redsq = INVALID_TILE;
	MakeNewgameSettingsLive();

	if (reset_date) {
		SetDate(ConvertYMDToDate(_settings_game.game_creation.starting_year, 0, 1));
		InitializeOldNames();
	}

	InitializeEngineRenews();
	InitializeVehicles();
	InitializeWaypoints();
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
	InitializeTrees();
	InitializeSigns();
	InitializeStations();
	InitializeCargoPackets();
	InitializeIndustries();
	InitializeBuildingCounts();

	InitializeTrains();
	InitializeNPF();

	InitializeCompanies();
	AI::Initialize();
	InitializeCheats();

	InitTextEffects();
#ifdef ENABLE_NETWORK
	NetworkInitChatMessage();
#endif /* ENABLE_NETWORK */
	InitializeAnimatedTiles();

	InitializeLandscapeVariables(false);

	ResetObjectToPlace();

	GamelogReset();
	GamelogStartAction(GLAT_START);
	GamelogRevision();
	GamelogMode();
	GamelogGRFAddList(_grfconfig);
	GamelogStopAction();
}


/* Calculate constants that depend on the landscape type. */
void InitializeLandscapeVariables(bool only_constants)
{
	if (only_constants) return;

	for (CargoID i = 0; i < NUM_CARGO; i++) {
		_cargo_payment_rates[i] = GetCargo(i)->initial_payment;
		_cargo_payment_rates_frac[i] = 0;
	}
}
