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
#include "town_cmd.h"
#include "signs_cmd.h"
#include "3rdparty/nlohmann/json.hpp"
#include "strings_func.h"
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
#include "video/video_driver.hpp"
#include "tilehighlight_func.h"
#include "saveload/saveload.h"
#include "void_map.h"
#include "town.h"
#include "newgrf.h"
#include "newgrf_house.h"
#include "core/random_func.hpp"
#include "core/backup_type.hpp"
#include "progress.h"
#include "error.h"
#include "game/game.hpp"
#include "game/game_instance.hpp"
#include "string_func.h"
#include "thread.h"
#include "tgp.h"

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
	Backup<CompanyID> _cur_company(_current_company, OWNER_NONE);

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
		if (!CheckTownRoadTypes()) {
			HandleGeneratingWorldAbortion();
			return;
		}

		bool landscape_generated = false;

		/* Don't generate landscape items when in the scenario editor. */
		if (_gw.mode != GWM_EMPTY) {
			landscape_generated = GenerateLandscape(_gw.mode);
		}

		if (!landscape_generated) {
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
			std::string name = fmt::format("dmp_cmds_{:08x}_{:08x}.sav", _settings_game.game_creation.generation_seed, TimerGameEconomy::date);
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

	if (_settings_game.game_creation.generation_seed == GENERATE_NEW_SEED) _settings_game.game_creation.generation_seed = InteractiveRandom();

	/* Load the right landscape stuff, and the NewGRFs! */
	GfxLoadSprites();
	InitializeBuildingCounts();
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

/** Town data imported from JSON files and used to place towns. */
struct ExternalTownData {
	TownID town_id; ///< The TownID of the town in OpenTTD. Not imported, but set during the founding proceess and stored here for convenience.
	std::string name; ///< The name of the town.
	uint population; ///< The target population of the town when created in OpenTTD. If input is blank, defaults to 0.
	bool is_city; ///< Should it be created as a city in OpenTTD? If input is blank, defaults to false.
	float x_proportion; ///< The X coordinate of the town, as a proportion 0..1 of the maximum X coordinate.
	float y_proportion; ///< The Y coordinate of the town, as a proportion 0..1 of the maximum Y coordinate.
};

/**
 * Helper for CircularTileSearch to found a town on or near a given tile.
 * @param tile The tile to try founding the town upon.
 * @param user_data The ExternalTownData to attempt to found.
 * @return True if the town was founded successfully.
 */
static bool TryFoundTownNearby(TileIndex tile, void *user_data)
{
	ExternalTownData &town = *static_cast<ExternalTownData *>(user_data);
	std::tuple<CommandCost, Money, TownID> result = Command<CMD_FOUND_TOWN>::Do(DC_EXEC, tile, TSZ_SMALL, town.is_city, _settings_game.economy.town_layout, false, 0, town.name);

	TownID id = std::get<TownID>(result);

	/* Check if the command failed. */
	if (id == INVALID_TOWN) return false;

	/* The command succeeded, send the ID back through user_data. */
	town.town_id = id;
	return true;
}

/**
 * Load town data from _file_to_saveload, place towns at the appropriate locations, and expand them to their target populations.
 */
void LoadTownData()
{
	/* Load the JSON file as a string initially. We'll parse it soon. */
	size_t filesize;
	FILE *f = FioFOpenFile(_file_to_saveload.name, "rb", HEIGHTMAP_DIR, &filesize);

	if (f == nullptr) {
		ShowErrorMessage(STR_TOWN_DATA_ERROR_LOAD_FAILED, STR_TOWN_DATA_ERROR_JSON_FORMATTED_INCORRECTLY, WL_ERROR);
		return;
	}

	std::string text(filesize, '\0');
	size_t len = fread(text.data(), filesize, 1, f);
	FioFCloseFile(f);
	if (len != 1) {
		ShowErrorMessage(STR_TOWN_DATA_ERROR_LOAD_FAILED, STR_TOWN_DATA_ERROR_JSON_FORMATTED_INCORRECTLY, WL_ERROR);
		return;
	}

	/* Now parse the JSON. */
	nlohmann::json town_data;
	try {
		town_data = nlohmann::json::parse(text);
	} catch (nlohmann::json::exception &) {
		ShowErrorMessage(STR_TOWN_DATA_ERROR_LOAD_FAILED, STR_TOWN_DATA_ERROR_JSON_FORMATTED_INCORRECTLY, WL_ERROR);
		return;
	}

	/* Check for JSON formatting errors with the array of towns. */
	if (!town_data.is_array()) {
		ShowErrorMessage(STR_TOWN_DATA_ERROR_LOAD_FAILED, STR_TOWN_DATA_ERROR_JSON_FORMATTED_INCORRECTLY, WL_ERROR);
		return;
	}

	std::vector<std::pair<Town *, uint> > towns;
	uint failed_towns = 0;

	/* Iterate through towns and attempt to found them. */
	for (auto &feature : town_data) {
		ExternalTownData town;

		/* Ensure JSON is formatted properly. */
		if (!feature.is_object()) {
			ShowErrorMessage(STR_TOWN_DATA_ERROR_LOAD_FAILED, STR_TOWN_DATA_ERROR_JSON_FORMATTED_INCORRECTLY, WL_ERROR);
			return;
		}

		/* Check to ensure all fields exist and are of the correct type.
		 * If the town name is formatted wrong, all we can do is give a general warning. */
		if (!feature.contains("name") || !feature.at("name").is_string()) {
			ShowErrorMessage(STR_TOWN_DATA_ERROR_LOAD_FAILED, STR_TOWN_DATA_ERROR_JSON_FORMATTED_INCORRECTLY, WL_ERROR);
			return;
		}

		/* If other fields are formatted wrong, we can actually inform the player which town is the problem. */
		if (!feature.contains("population") || !feature.at("population").is_number() ||
				!feature.contains("city") || !feature.at("city").is_boolean() ||
				!feature.contains("x") || !feature.at("x").is_number() ||
				!feature.contains("y") || !feature.at("y").is_number()) {
			feature.at("name").get_to(town.name);
			SetDParamStr(0, town.name);
			ShowErrorMessage(STR_TOWN_DATA_ERROR_LOAD_FAILED, STR_TOWN_DATA_ERROR_TOWN_FORMATTED_INCORRECTLY, WL_ERROR);
			return;
		}

		/* Set town properties. */
		feature.at("name").get_to(town.name);
		feature.at("population").get_to(town.population);
		feature.at("city").get_to(town.is_city);

		/* Set town coordinates. */
		feature.at("x").get_to(town.x_proportion);
		feature.at("y").get_to(town.y_proportion);

		/* Check for improper coordinates and warn the player. */
		if (town.x_proportion <= 0.0f || town.y_proportion <= 0.0f || town.x_proportion >= 1.0f || town.y_proportion >= 1.0f) {
			SetDParamStr(0, town.name);
			ShowErrorMessage(STR_TOWN_DATA_ERROR_LOAD_FAILED, STR_TOWN_DATA_ERROR_BAD_COORDINATE, WL_ERROR);
			return;
		}

		/* Find the target tile for the town. */
		TileIndex tile;
		switch (_settings_game.game_creation.heightmap_rotation) {
			case HM_CLOCKWISE:
				/* Tile coordinates align with what we expect. */
				tile = TileXY(town.x_proportion * Map::MaxX(), town.y_proportion * Map::MaxY());
				break;
			case HM_COUNTER_CLOCKWISE:
				/* Tile coordinates are rotated and must be adjusted. */
				tile = TileXY((1 - town.y_proportion * Map::MaxX()), town.x_proportion * Map::MaxY());
				break;
			default: NOT_REACHED();
		}

		/* Try founding on the target tile, and if that doesn't work, find the nearest suitable tile up to 16 tiles away.
		 * The target might be on water, blocked somehow, or on a steep slope that can't be terraformed by the founding command. */
		TileIndex search_tile = tile;
		bool success = CircularTileSearch(&search_tile, 16, 0, 0, TryFoundTownNearby, &town);

		/* If we still fail to found the town, we'll create a sign at the intended location and tell the player how many towns we failed to create in an error message.
		 * This allows the player to diagnose a heightmap misalignment, if towns end up in the sea, or place towns manually, if in rough terrain. */
		if (!success) {
			Command<CMD_PLACE_SIGN>::Post(tile, town.name);
			failed_towns++;
			continue;
		}

		towns.emplace_back(std::make_pair(Town::Get(town.town_id), town.population));
	}

	/* If we couldn't found a town (or multiple), display a message to the player with the number of failed towns. */
	if (failed_towns > 0) {
		SetDParam(0, failed_towns);
		ShowErrorMessage(STR_TOWN_DATA_ERROR_FAILED_TO_FOUND_TOWN, INVALID_STRING_ID, WL_WARNING);
	}

	/* Now that we've created the towns, let's grow them to their target populations. */
	for (const auto &item : towns) {
		Town *t = item.first;
		uint population = item.second;

		/* Grid towns can grow almost forever, but the town growth algorithm gets less and less efficient as it wanders roads randomly,
		 * so we set an arbitrary limit. With a flat map and a 3x3 grid layout this results in about 4900 houses, or 2800 houses with "Better roads." */
		int try_limit = 1000;

		/* If a town repeatedly fails to grow, continuing to try only wastes time. */
		int fail_limit = 10;

		/* Grow by a constant number of houses each time, instead of growth based on current town size.
		 * We want our try limit to apply in a predictable way, no matter the road layout and other geography. */
		const int HOUSES_TO_GROW = 10;

		do {
			uint before = t->cache.num_houses;
			Command<CMD_EXPAND_TOWN>::Post(t->index, HOUSES_TO_GROW);
			if (t->cache.num_houses <= before) fail_limit--;
		} while (fail_limit > 0 && try_limit-- > 0 && t->cache.population < population);
	}
}
