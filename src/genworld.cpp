/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
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
#include "saveload/saveload_func.h"
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
#include "newgrf_railtype.h"
#include "newgrf_roadtype.h"
#include "string_func.h"
#include "thread.h"
#include "tgp.h"

#include "table/strings.h"

#include "safeguards.h"


void GenerateClearTile();
void GenerateIndustries();
void GenerateObjects();
void GenerateTrees();

void StartupEconomy();
void StartupCompanies();
void StartupDisasters();

void InitializeGame(uint size_x, uint size_y, bool reset_date, bool reset_settings);

/** Properties of current genworld process */
struct GenWorldInfo {
	static inline bool abort;            ///< Whether to abort the thread ASAP
	static inline GenWorldMode mode;     ///< What mode are we making a world in
	static inline CompanyID lc;          ///< The local_company before generating
	static inline uint size_x;           ///< X-size of the map
	static inline uint size_y;           ///< Y-size of the map
	static inline GWDoneProc *proc;      ///< Proc that is called when done (can be nullptr)
	static inline GWAbortProc *abortp;   ///< Proc that is called when aborting (can be nullptr)
};

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
	GenWorldInfo::proc     = nullptr;
	GenWorldInfo::abortp   = nullptr;

	CloseWindowByClass(WindowClass::ModalProgress);
	ShowFirstError();
	MarkWholeScreenDirty();
}

/**
 * The internal, real, generate function.
 */
static void _GenerateWorld()
{
	/* Make sure everything is done via OWNER_NONE. */
	Backup<CompanyID> cur_company(_current_company, OWNER_NONE);

	try {
		_generating_world = true;
		if (_network_dedicated) Debug(net, 3, "Generating map, please wait...");
		/* Set the Random() seed to generation_seed so we produce the same map with the same seed */
		_random.SetSeed(_settings_game.game_creation.generation_seed);
		SetGeneratingWorldProgress(GenWorldProgress::Init, 2);
		SetObjectToPlace(SPR_CURSOR_ZZZ, PAL_NONE, HT_NONE, WindowClass::MainWindow, 0);
		ScriptObject::InitializeRandomizers();

		BasePersistentStorageArray::SwitchMode(PSM_ENTER_GAMELOOP);

		IncreaseGeneratingWorldProgress(GenWorldProgress::Init);
		/* Must start economy early because of the costs. */
		StartupEconomy();
		if (!CheckTownRoadTypes()) {
			HandleGeneratingWorldAbortion();
			return;
		}

		bool landscape_generated = false;

		/* Don't generate landscape items when in the scenario editor. */
		if (GenWorldInfo::mode != GWM_EMPTY) {
			landscape_generated = GenerateLandscape(GenWorldInfo::mode);
		}

		if (!landscape_generated) {
			SetGeneratingWorldProgress(GenWorldProgress::Objects, 1);

			/* Make sure the tiles at the north border are void tiles if needed. */
			if (_settings_game.construction.freeform_edges) {
				for (uint x = 0; x < Map::SizeX(); x++) MakeVoid(TileXY(x, 0));
				for (uint y = 0; y < Map::SizeY(); y++) MakeVoid(TileXY(0, y));
			}

			/* Make the map the height of the setting */
			if (_game_mode != GameMode::Menu) FlatEmptyWorld(_settings_game.game_creation.se_flat_world_height);

			ConvertGroundTilesIntoWaterTiles();
			Map::CountLandTiles();
			IncreaseGeneratingWorldProgress(GenWorldProgress::Objects);

			_settings_game.game_creation.snow_line_height = DEF_SNOWLINE_HEIGHT;
		} else {
			GenerateClearTile();
			Map::CountLandTiles();

			/* Only generate towns, tree and industries in newgame mode. */
			if (_game_mode != GameMode::Editor) {
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
		SetGeneratingWorldProgress(GenWorldProgress::GameInit, 3);
		StartupCompanies();
		IncreaseGeneratingWorldProgress(GenWorldProgress::GameInit);
		StartupEngines();
		IncreaseGeneratingWorldProgress(GenWorldProgress::GameInit);
		StartupDisasters();
		_generating_world = false;

		Game::StartNew();

		/* No need to run the tile loop in the scenario editor. */
		if (GenWorldInfo::mode != GWM_EMPTY) {
			uint i;

			SetGeneratingWorldProgress(GenWorldProgress::RunTileLoop, 0x500);
			for (i = 0; i < 0x500; i++) {
				RunTileLoop();
				TimerGameTick::counter++;
				IncreaseGeneratingWorldProgress(GenWorldProgress::RunTileLoop);
			}

			if (_game_mode != GameMode::Editor) {
				if (Game::GetInstance() != nullptr) {
					SetGeneratingWorldProgress(GenWorldProgress::GameScript, 2500);
					_generating_world = true;
					for (i = 0; i < 2500; i++) {
						Game::GameLoop();
						IncreaseGeneratingWorldProgress(GenWorldProgress::GameScript);
						if (Game::GetInstance()->IsSleeping()) break;
					}
					_generating_world = false;
				}
			}
		}

		BasePersistentStorageArray::SwitchMode(PSM_LEAVE_GAMELOOP);

		ResetObjectToPlace();
		cur_company.Trash();
		_current_company = _local_company = GenWorldInfo::lc;
		/* Show all vital windows again, because we have hidden them. */
		if (_game_mode != GameMode::Menu) ShowVitalWindows();

		SetGeneratingWorldProgress(GenWorldProgress::GameStart, 1);
		/* Call any callback */
		if (GenWorldInfo::proc != nullptr) GenWorldInfo::proc();
		IncreaseGeneratingWorldProgress(GenWorldProgress::GameStart);

		CleanupGeneration();

		ShowNewGRFError();

		if (_network_dedicated) Debug(net, 3, "Map generated, starting game");
		Debug(desync, 1, "new_map: {:08x}", _settings_game.game_creation.generation_seed);

		if (_debug_desync_level > 0) {
			std::string name = fmt::format("dmp_cmds_{:08x}_{:08x}.sav", _settings_game.game_creation.generation_seed, TimerGameEconomy::date);
			SaveOrLoad(name, SaveLoadOperation::Save, DetailedFileType::GameFile, Subdirectory::Autosave, false);
		}
	} catch (AbortGenerateWorldSignal&) {
		CleanupGeneration();

		BasePersistentStorageArray::SwitchMode(PSM_LEAVE_GAMELOOP, true);
		if (cur_company.IsValid()) cur_company.Restore();

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
	GenWorldInfo::proc = proc;
}

/**
 * Set here the function, if any, that you want to be called when landscape
 * generation is aborted.
 * @param proc callback procedure
 */
void GenerateWorldSetAbortCallback(GWAbortProc *proc)
{
	GenWorldInfo::abortp = proc;
}

/**
 * Initializes the abortion process
 */
void AbortGeneratingWorld()
{
	GenWorldInfo::abort = true;
}

/**
 * Is the generation being aborted?
 * @return the 'aborted' status
 */
bool IsGeneratingWorldAborted()
{
	return GenWorldInfo::abort || _exit_game;
}

/**
 * Really handle the abortion, i.e. clean up some of the mess
 */
void HandleGeneratingWorldAbortion()
{
	/* Clean up - in SE create an empty map, otherwise, go to intro menu */
	_switch_mode = (_game_mode == GameMode::Editor) ? SwitchMode::Editor : SwitchMode::Menu;

	if (GenWorldInfo::abortp != nullptr) GenWorldInfo::abortp();

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
	GenWorldInfo::mode   = mode;
	GenWorldInfo::size_x = size_x;
	GenWorldInfo::size_y = size_y;
	SetModalProgress(true);
	GenWorldInfo::abort  = false;
	GenWorldInfo::abortp = nullptr;
	GenWorldInfo::lc     = _local_company;

	/* This disables some commands and stuff */
	SetLocalCompany(COMPANY_SPECTATOR);

	InitializeGame(GenWorldInfo::size_x, GenWorldInfo::size_y, true, reset_settings);
	PrepareGenerateWorldProgress();

	if (_settings_game.construction.map_height_limit == 0) {
		uint estimated_height = 0;

		if (GenWorldInfo::mode == GWM_EMPTY && _game_mode != GameMode::Menu) {
			estimated_height = _settings_game.game_creation.se_flat_world_height;
		} else if (GenWorldInfo::mode == GWM_HEIGHTMAP) {
			estimated_height = _settings_game.game_creation.heightmap_height;
		} else if (_settings_game.game_creation.land_generator == LG_TERRAGENESIS) {
			estimated_height = GetEstimationTGPMapHeight();
		} else {
			estimated_height = 0;
		}

		_settings_game.construction.map_height_limit = std::max<uint8_t>(MAP_HEIGHT_LIMIT_AUTO_MINIMUM, std::min<uint8_t>(MAX_MAP_HEIGHT_LIMIT, estimated_height + MAP_HEIGHT_LIMIT_AUTO_CEILING_ROOM));
	}

	if (_settings_game.game_creation.generation_seed == GENERATE_NEW_SEED) _settings_game.game_creation.generation_seed = InteractiveRandom();

	/* Load the right landscape stuff, and the NewGRFs! */
	GfxLoadSprites();
	SetCurrentRailTypeLabelList();
	SetCurrentRoadTypeLabelList();
	InitializeBuildingCounts();
	LoadStringWidthTable();

	/* Re-init the windowing system */
	ResetWindowSystem();

	/* Create toolbars */
	SetupColoursAndInitialWindow();
	SetObjectToPlace(SPR_CURSOR_ZZZ, PAL_NONE, HT_NONE, WindowClass::MainWindow, 0);

	UnshowCriticalError();
	CloseAllNonVitalWindows();
	HideVitalWindows();

	ShowGenerateWorldProgress();

	/* Centre the view on the map */
	ScrollMainWindowToTile(TileXY(Map::SizeX() / 2, Map::SizeY() / 2), true);

	_GenerateWorld();
}

/** Parsed town data. */
struct ParsedTown {
	std::string name; ///< The name of the town.
	uint population; ///< The target population of the town when created in OpenTTD. If input is blank, defaults to 0.
	bool is_city; ///< Should it be created as a city in OpenTTD? If input is blank, defaults to false.
	TileIndex target_tile; ///< The target tile of the town.
};

/**
 * Translation coordates from a proportions to TileIndex.
 * @param x The X proportion between 0..1.
 * @param y The Y proportion between 0..1.
 * @return The translated TileIndex, or INVALID_TILE if out of bounds.
 */
static TileIndex TranslateCoordinates(float x, float y)
{
	if (x <= 0.0f || y <= 0.0f || x >= 1.0f || y >= 1.0f) return INVALID_TILE;

	/* Determine the target tile.. */
	switch (_settings_game.game_creation.heightmap_rotation) {
		case HM_CLOCKWISE:
			/* Tile coordinates align with what we expect. */
			return TileXY(x * Map::MaxX(), y * Map::MaxY());

		case HM_COUNTER_CLOCKWISE:
			/* Tile coordinates are rotated and must be adjusted. */
			return TileXY((1 - y) * Map::MaxX(), x * Map::MaxY());

		default:
			NOT_REACHED();
	}
}

/**
 * Parse town data from json text.
 * @param text The json formatted text.
 * @return List of towns to create.
 */
static std::vector<ParsedTown> ParseTownData(std::string_view text)
{
	/* Now parse the JSON. */
	nlohmann::json town_data;
	try {
		town_data = nlohmann::json::parse(text);
	} catch (nlohmann::json::exception &) {
		ShowErrorMessage(GetEncodedString(STR_TOWN_DATA_ERROR_LOAD_FAILED), GetEncodedString(STR_TOWN_DATA_ERROR_JSON_FORMATTED_INCORRECTLY), WarningLevel::Error);
		return {};
	}

	/* Check for JSON formatting errors with the array of towns. */
	if (!town_data.is_array()) {
		ShowErrorMessage(GetEncodedString(STR_TOWN_DATA_ERROR_LOAD_FAILED), GetEncodedString(STR_TOWN_DATA_ERROR_JSON_FORMATTED_INCORRECTLY), WarningLevel::Error);
		return {};
	}

	std::vector<ParsedTown> towns;

	/* Iterate through towns and attempt to found them. */
	for (auto &feature : town_data) {
		ParsedTown &town = towns.emplace_back();

		/* Ensure JSON is formatted properly. */
		if (!feature.is_object()) {
			ShowErrorMessage(GetEncodedString(STR_TOWN_DATA_ERROR_LOAD_FAILED), GetEncodedString(STR_TOWN_DATA_ERROR_JSON_FORMATTED_INCORRECTLY), WarningLevel::Error);
			return {};
		}

		/* Check to ensure all fields exist and are of the correct type.
		 * If the town name is formatted wrong, all we can do is give a general warning. */
		if (!feature.contains("name") || !feature.at("name").is_string()) {
			ShowErrorMessage(GetEncodedString(STR_TOWN_DATA_ERROR_LOAD_FAILED), GetEncodedString(STR_TOWN_DATA_ERROR_JSON_FORMATTED_INCORRECTLY), WarningLevel::Error);
			return {};
		}

		feature.at("name").get_to(town.name);

		/* If other fields are formatted wrong, we can actually inform the player which town is the problem. */
		if (!feature.contains("population") || !feature.at("population").is_number() ||
				!feature.contains("city") || !feature.at("city").is_boolean() ||
				!feature.contains("x") || !feature.at("x").is_number() ||
				!feature.contains("y") || !feature.at("y").is_number()) {
			ShowErrorMessage(GetEncodedString(STR_TOWN_DATA_ERROR_LOAD_FAILED), GetEncodedString(STR_TOWN_DATA_ERROR_TOWN_FORMATTED_INCORRECTLY, town.name), WarningLevel::Error);
			return {};
		}

		/* Set town properties. */
		feature.at("population").get_to(town.population);
		feature.at("city").get_to(town.is_city);

		/* Find the target tile for the town. */
		town.target_tile = TranslateCoordinates(feature.at("x").get<float>(), feature.at("y").get<float>());
		if (town.target_tile == INVALID_TILE) {
			ShowErrorMessage(GetEncodedString(STR_TOWN_DATA_ERROR_LOAD_FAILED), GetEncodedString(STR_TOWN_DATA_ERROR_BAD_COORDINATE, town.name), WarningLevel::Error);
			return {};
		}
	}

	return towns;
}

/**
 * Load town data from _file_to_saveload, place towns at the appropriate locations, and expand them to their target populations.
 */
void LoadTownData()
{
	/* Load the JSON file as a string initially. We'll parse it soon. */
	size_t filesize;
	auto f = FioFOpenFile(_file_to_saveload.name, "rb", Subdirectory::Heightmap, &filesize);

	if (!f.has_value()) {
		ShowErrorMessage(GetEncodedString(STR_TOWN_DATA_ERROR_LOAD_FAILED),
			GetEncodedString(STR_TOWN_DATA_ERROR_JSON_FORMATTED_INCORRECTLY), WarningLevel::Error);
		return;
	}

	std::string text(filesize, '\0');
	size_t len = fread(text.data(), filesize, 1, *f);
	f.reset();
	if (len != 1) {
		ShowErrorMessage(GetEncodedString(STR_TOWN_DATA_ERROR_LOAD_FAILED),
			GetEncodedString(STR_TOWN_DATA_ERROR_JSON_FORMATTED_INCORRECTLY), WarningLevel::Error);
		return;
	}

	std::vector<ParsedTown> town_data = ParseTownData(text);
	if (town_data.empty()) return;

	std::vector<std::pair<const Town *, uint> > towns;
	uint failed_towns = 0;

	AutoRestoreBackup old_generating_world(_generating_world, true);
	bool road_pending = UpdateNearestTownForRoadTiles(true);

	for (auto &town : town_data) {
		const Town *t = TryGenerateNamedTownAroundTile(town.target_tile, TownSize::Small, town.is_city, _settings_game.economy.town_layout, town.name);

		/* If we still fail to found the town, we'll create a sign at the intended location and tell the player how many towns we failed to create in an error message.
		 * This allows the player to diagnose a heightmap misalignment, if towns end up in the sea, or place towns manually, if in rough terrain. */
		if (t == nullptr) {
			Command<Commands::PlaceSign>::Post(town.target_tile, town.name);
			failed_towns++;
			continue;
		}

		towns.emplace_back(t, town.population);
	}

	/* If we couldn't found a town (or multiple), display a message to the player with the number of failed towns. */
	if (failed_towns > 0) {
		ShowErrorMessage(GetEncodedString(STR_TOWN_DATA_ERROR_FAILED_TO_FOUND_TOWN, failed_towns), {}, WarningLevel::Warning);
	}

	/* Now that we've created the towns, let's grow them to their target populations. */
	for (const auto &[t, population] : towns) {
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
			Command<Commands::ExpandTown>::Post(t->index, HOUSES_TO_GROW, {TownExpandMode::Buildings, TownExpandMode::Roads});
			if (t->cache.num_houses <= before) fail_limit--;
		} while (fail_limit > 0 && try_limit-- > 0 && t->cache.population < population);
	}

	if (road_pending) UpdateNearestTownForRoadTiles(false);
}
