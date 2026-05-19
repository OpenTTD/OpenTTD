/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file genworld.h Functions related to world/map generation. */

#ifndef GENWORLD_H
#define GENWORLD_H

#include "company_type.h"
#include "landscape_type.h"
#include <thread>

/** Constants related to world generation */
enum LandscapeGenerator : uint8_t {
	/* Order of these enums has to be the same as in lang/english.txt
	 * Otherwise you will get inconsistent behaviour. */
	LG_ORIGINAL     = 0,  ///< The original landscape generator
	LG_TERRAGENESIS = 1,  ///< TerraGenesis Perlin landscape generator
};

static const uint32_t GENERATE_NEW_SEED = UINT32_MAX; ///< Create a new random seed

/** Modes for GenerateWorld */
enum GenWorldMode : uint8_t {
	GWM_NEWGAME   = 0, ///< Generate a map for a new game
	GWM_EMPTY     = 1, ///< Generate an empty map (sea-level)
	GWM_RANDOM    = 2, ///< Generate a random map for SE
	GWM_HEIGHTMAP = 3, ///< Generate a newgame from a heightmap
};

/** Smoothness presets. */
enum TgenSmoothness : uint8_t {
	TGEN_SMOOTHNESS_BEGIN,      ///< First smoothness value.
	TGEN_SMOOTHNESS_VERY_SMOOTH = TGEN_SMOOTHNESS_BEGIN, ///< Smoothness preset 'very smooth'.
	TGEN_SMOOTHNESS_SMOOTH,     ///< Smoothness preset 'smooth'.
	TGEN_SMOOTHNESS_ROUGH,      ///< Smoothness preset 'rough'.
	TGEN_SMOOTHNESS_VERY_ROUGH, ///< Smoothness preset 'very rough'.

	TGEN_SMOOTHNESS_END,        ///< Used to iterate.
};

static const uint CUSTOM_SEA_LEVEL_NUMBER_DIFFICULTY = 4; ///< Value for custom sea level in difficulty settings.
static const uint CUSTOM_SEA_LEVEL_MIN_PERCENTAGE = 1;    ///< Minimum percentage a user can specify for custom sea level.
static const uint CUSTOM_SEA_LEVEL_MAX_PERCENTAGE = 90;   ///< Maximum percentage a user can specify for custom sea level.

static constexpr uint MAP_HEIGHT_LIMIT_ORIGINAL = 15; ///< Original map height limit.

static const uint MAP_HEIGHT_LIMIT_AUTO_MINIMUM = 30; ///< When map height limit is auto, make this the lowest possible map height limit.
static const uint MAP_HEIGHT_LIMIT_AUTO_CEILING_ROOM = 15; ///< When map height limit is auto, the map height limit will be the highest peak plus this value.

typedef void GWDoneProc();  ///< Procedure called when the genworld process finishes
typedef void GWAbortProc(); ///< Called when genworld is aborted

/** Current stage of world generation process */
enum class GenWorldProgress : uint8_t {
	Init, ///< Initialize/allocate the map, start economy.
	Landscape, ///< Create the landscape.
	Rivers, ///< Create the rivers.
	RoughAndRocks, ///< Make rough and rocky areas.
	Towns, ///< Generate towns.
	LandIndustries, ///< Generate industries.
	WaterIndustries, ///< Generate industries.
	Objects, ///< Generate objects (radio tower, light houses).
	Trees, ///< Generate trees.
	GameInit, ///< Initialize the game.
	RunTileLoop, ///< Runs the tile loop 1280 times to make snow etc.
	GameScript, ///< Runs the game script at most 2500 times, or when ever the script sleeps.
	GameStart, ///< Really prepare to start the game.
	End, ///< End marker.
};
DECLARE_ENUM_AS_SEQUENTIAL(GenWorldProgress)

/* genworld.cpp */
void GenerateWorldSetCallback(GWDoneProc *proc);
void GenerateWorldSetAbortCallback(GWAbortProc *proc);
void GenerateWorld(GenWorldMode mode, uint size_x, uint size_y, bool reset_settings = true);
void AbortGeneratingWorld();
bool IsGeneratingWorldAborted();
void HandleGeneratingWorldAbortion();
void LoadTownData();

/* genworld_gui.cpp */
void SetNewLandscapeType(LandscapeType landscape);
void SetGeneratingWorldProgress(GenWorldProgress cls, uint total);
void IncreaseGeneratingWorldProgress(GenWorldProgress cls);
void PrepareGenerateWorldProgress();
void ShowGenerateWorldProgress();
void StartNewGameWithoutGUI(uint32_t seed);
void ShowCreateScenario();
void StartScenarioEditor();

extern bool _generating_world;

#endif /* GENWORLD_H */
