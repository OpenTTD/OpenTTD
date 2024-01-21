/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file genworld.cpp Functions to generate a map. */

#include "stdafx.h"
#include "landscape.h"
#include "company_func.h"
#include "genworld.h"
#include "gfxinit.h"
#include "window_func.h"
#include "network/network.h"
#include "heightmap.h"
#include "viewport_func.h"
#include "timer/timer_game_calendar.h"
#include "timer/timer_game_tick.h"
#include "engine_func.h"
#include "water.h"
#include "video/video_driver.h"
#include "tilehighlight_func.h"
#include "saveload/saveload.h"
#include "void_map.h"
#include "town.h"
#include "newgrf.h"
#include "core/random_func.h"
#include "core/backup_type.h"
#include "progress.h"
#include "error.h"
#include "game/game.h"
#include "game/game_instance.h"
#include "string_func.h"
#include "thread.h"
#include "tgp.h"
#include "pathfinder/water_regions.h"

#include "safeguards.h"


void GenerateClearTile();
void GenerateIndustries();
void GenerateObjects();
void GenerateTrees();

void StartupEconomy();
void StartupCompanies();
void StartupDisasters();

void InitializeGame(uint size_x, uint size_y, bool reset_date, bool reset_settings);

/**
 * Please only use this variable in genworld.h and genworld.cpp and
 *  nowhere else. For speed improvements we need it to be global, but
 *  in no way the meaning of it is to use it anywhere else besides
 *  in the genworld.h and genworld.cpp!
 */
GenWorldInfo _gw;

/** Whether we are generating the map or not. */
bool _generating_world;

class AbortGenerateWorldSignal { };

/**
 * Generation is done; show windows again and delete the progress window.
 */
static void CleanupGeneration()
{
	_generating_world = false;

	SetMouseCursorBusy(false);
	SetModalProgress(false);
	_gw.proc     = nullptr;
	_gw.abortp   = nullptr;

	CloseWindowByClass(WC_MODAL_PROGRESS);
	ShowFirstError();
	MarkWholeScreenDirty();
}

/**
 * The internal, real, generate function.
 */
static void _GenerateWorld()
{
	/* Make sure everything is done via OWNER_NONE. */
	Backup<CompanyID> _cur_company(_current_company, OWNER_NONE, FILE_LINE);

	try {
		_generating_world = true;
		if (_network_dedicated) Debug(net, 3, "Generating map, please wait...");
		/* Set the Random() seed to generation_seed so we produce the same map with the same seed */
		_random.SetSeed(_settings_game.game_creation.generation_seed);
		SetGeneratingWorldProgress(GWP_MAP_INIT, 2);
		SetObjectToPlace(SPR_CURSOR_ZZZ, PAL_NONE, HT_NONE, WC_MAIN_WINDOW, 0);
		ScriptObject::InitializeRandomizers();

		BasePersistentStorageArray::SwitchMode(PSM_ENTER_GAMELOOP);

		IncreaseGeneratingWorldProgress(GWP_MAP_INIT);
		/* Must start economy early because of the costs. */
		StartupEconomy();

		/* Don't generate landscape items when in the scenario editor. */
		if (_gw.mode == GWM_EMPTY) {
			SetGeneratingWorldProgress(GWP_OBJECT, 1);

			/* Make sure the tiles at the north border are void tiles if needed. */
			if (_settings_game.construction.freeform_edges) {
				for (uint x = 0; x < Map::SizeX(); x++) MakeVoid(TileXY(x, 0));
				for (uint y = 0; y < Map::SizeY(); y++) MakeVoid(TileXY(0, y));
			}

			/* Make the map the height of the setting */
			if (_game_mode != GM_MENU) FlatEmptyWorld(_settings_game.game_creation.se_flat_world_height);

			ConvertGroundTilesIntoWaterTiles();
			IncreaseGeneratingWorldProgress(GWP_OBJECT);

			_settings_game.game_creation.snow_line_height = DEF_SNOWLINE_HEIGHT;
		} else {
			GenerateLandscape(_gw.mode);
			GenerateClearTile();

			/* Only generate towns, tree and industries in newgame mode. */
			if (_game_mode != GM_EDITOR) {
				if (!GenerateTowns(_settings_game.economy.town_layout)) {
					HandleGeneratingWorldAbortion();
					return;
				}
				GenerateIndustries();
				GenerateObjects();
				GenerateTrees();
			}
		}

		/* These are probably pointless when inside the scenario editor. */
		SetGeneratingWorldProgress(GWP_GAME_INIT, 3);
		StartupCompanies();
		IncreaseGeneratingWorldProgress(GWP_GAME_INIT);
		StartupEngines();
		IncreaseGeneratingWorldProgress(GWP_GAME_INIT);
		StartupDisasters();
		_generating_world = false;

		/* No need to run the tile loop in the scenario editor. */
		if (_gw.mode != GWM_EMPTY) {
			uint i;

			SetGeneratingWorldProgress(GWP_RUNTILELOOP, 0x500);
			for (i = 0; i < 0x500; i++) {
				RunTileLoop();
				TimerGameTick::counter++;
				IncreaseGeneratingWorldProgress(GWP_RUNTILELOOP);
			}

			if (_game_mode != GM_EDITOR) {
				Game::StartNew();

				if (Game::GetInstance() != nullptr) {
					SetGeneratingWorldProgress(GWP_RUNSCRIPT, 2500);
					_generating_world = true;
					for (i = 0; i < 2500; i++) {
						Game::GameLoop();
						IncreaseGeneratingWorldProgress(GWP_RUNSCRIPT);
						if (Game::GetInstance()->IsSleeping()) break;
					}
					_generating_world = false;
				}
			}
		}

		InitializeWaterRegions();

		BasePersistentStorageArray::SwitchMode(PSM_LEAVE_GAMELOOP);

		ResetObjectToPlace();
		_cur_company.Trash();
		_current_company = _local_company = _gw.lc;
		/* Show all vital windows again, because we have hidden them. */
		if (_game_mode != GM_MENU) ShowVitalWindows();

		SetGeneratingWorldProgress(GWP_GAME_START, 1);
		/* Call any callback */
		if (_gw.proc != nullptr) _gw.proc();
		IncreaseGeneratingWorldProgress(GWP_GAME_START);

		CleanupGeneration();

		ShowNewGRFError();

		if (_network_dedicated) Debug(net, 3, "Map generated, starting game");
		Debug(desync, 1, "new_map: {:08x}", _settings_game.game_creation.generation_seed);

		if (_debug_desync_level > 0) {
			std::string name = fmt::format("dmp_cmds_{:08x}_{:08x}.sav", _settings_game.game_creation.generation_seed, TimerGameCalendar::date);
			SaveOrLoad(name, SLO_SAVE, DFT_GAME_FILE, AUTOSAVE_DIR, false);
		}
	} catch (AbortGenerateWorldSignal&) {
		CleanupGeneration();

		BasePersistentStorageArray::SwitchMode(PSM_LEAVE_GAMELOOP, true);
		if (_cur_company.IsValid()) _cur_company.Restore();

		if (_network_dedicated) {
			/* Exit the game to prevent a return to main menu.  */
			Debug(net, 0, "Generating map failed; closing server");
			_exit_game = true;
		} else {
			SwitchToMode(_switch_mode);
		}
	}
}

/**
 * Set here the function, if any, that you want to be called when landscape
 * generation is done.
 * @param proc callback procedure
 */
void GenerateWorldSetCallback(GWDoneProc *proc)
{
	_gw.proc = proc;
}

/**
 * Set here the function, if any, that you want to be called when landscape
 * generation is aborted.
 * @param proc callback procedure
 */
void GenerateWorldSetAbortCallback(GWAbortProc *proc)
{
	_gw.abortp = proc;
}

/**
 * Initializes the abortion process
 */
void AbortGeneratingWorld()
{
	_gw.abort = true;
}

/**
 * Is the generation being aborted?
 * @return the 'aborted' status
 */
bool IsGeneratingWorldAborted()
{
	return _gw.abort || _exit_game;
}

/**
 * Really handle the abortion, i.e. clean up some of the mess
 */
void HandleGeneratingWorldAbortion()
{
	/* Clean up - in SE create an empty map, otherwise, go to intro menu */
	_switch_mode = (_game_mode == GM_EDITOR) ? SM_EDITOR : SM_MENU;

	if (_gw.abortp != nullptr) _gw.abortp();

	throw AbortGenerateWorldSignal();
}

/**
 * Generate a world.
 * @param mode The mode of world generation (see GenWorldMode).
 * @param size_x The X-size of the map.
 * @param size_y The Y-size of the map.
 * @param reset_settings Whether to reset the game configuration (used for restart)
 */
void GenerateWorld(GenWorldMode mode, uint size_x, uint size_y, bool reset_settings)
{
	if (HasModalProgress()) return;
	_gw.mode   = mode;
	_gw.size_x = size_x;
	_gw.size_y = size_y;
	SetModalProgress(true);
	_gw.abort  = false;
	_gw.abortp = nullptr;
	_gw.lc     = _local_company;

	/* This disables some commands and stuff */
	SetLocalCompany(COMPANY_SPECTATOR);

	InitializeGame(_gw.size_x, _gw.size_y, true, reset_settings);
	PrepareGenerateWorldProgress();

	if (_settings_game.construction.map_height_limit == 0) {
		uint estimated_height = 0;

		if (_gw.mode == GWM_EMPTY && _game_mode != GM_MENU) {
			estimated_height = _settings_game.game_creation.se_flat_world_height;
		} else if (_gw.mode == GWM_HEIGHTMAP) {
			estimated_height = _settings_game.game_creation.heightmap_height;
		} else if (_settings_game.game_creation.land_generator == LG_TERRAGENESIS) {
			estimated_height = GetEstimationTGPMapHeight();
		} else {
			estimated_height = 0;
		}

		_settings_game.construction.map_height_limit = std::max(MAP_HEIGHT_LIMIT_AUTO_MINIMUM, std::min(MAX_MAP_HEIGHT_LIMIT, estimated_height + MAP_HEIGHT_LIMIT_AUTO_CEILING_ROOM));
	}

	if (_settings_game.game_creation.generation_seed == GENERATE_NEW_SEED) _settings_game.game_creation.generation_seed = _settings_newgame.game_creation.generation_seed = InteractiveRandom();

	/* Load the right landscape stuff, and the NewGRFs! */
	GfxLoadSprites();
	LoadStringWidthTable();

	/* Re-init the windowing system */
	ResetWindowSystem();

	/* Create toolbars */
	SetupColoursAndInitialWindow();
	SetObjectToPlace(SPR_CURSOR_ZZZ, PAL_NONE, HT_NONE, WC_MAIN_WINDOW, 0);

	UnshowCriticalError();
	CloseAllNonVitalWindows();
	HideVitalWindows();

	ShowGenerateWorldProgress();

	/* Centre the view on the map */
	ScrollMainWindowToTile(TileXY(Map::SizeX() / 2, Map::SizeY() / 2), true);

	_GenerateWorld();
}
