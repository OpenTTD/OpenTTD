/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file genworld.h Functions related to world/map generation. */

#ifndef GENWORLD_H
#define GENWORLD_H

#include "company_type.h"
#include <thread>

/** Constants related to world generation */
enum LandscapeGenerator {
	/* Order of these enums has to be the same as in lang/english.txt
	 * Otherwise you will get inconsistent behaviour. */
	LG_ORIGINAL     = 0,  ///< The original landscape generator
	LG_TERRAGENESIS = 1,  ///< TerraGenesis Perlin landscape generator
};

static const uint32_t GENERATE_NEW_SEED = UINT32_MAX; ///< Create a new random seed

/** Modes for GenerateWorld */
enum GenWorldMode {
	GWM_NEWGAME   = 0, ///< Generate a map for a new game
	GWM_EMPTY     = 1, ///< Generate an empty map (sea-level)
	GWM_RANDOM    = 2, ///< Generate a random map for SE
	GWM_HEIGHTMAP = 3, ///< Generate a newgame from a heightmap
};

/** Smoothness presets. */
enum TgenSmoothness {
	TGEN_SMOOTHNESS_BEGIN,      ///< First smoothness value.
	TGEN_SMOOTHNESS_VERY_SMOOTH = TGEN_SMOOTHNESS_BEGIN, ///< Smoothness preset 'very smooth'.
	TGEN_SMOOTHNESS_SMOOTH,     ///< Smoothness preset 'smooth'.
	TGEN_SMOOTHNESS_ROUGH,      ///< Smoothness preset 'rough'.
	TGEN_SMOOTHNESS_VERY_ROUGH, ///< Smoothness preset 'very rough'.

	TGEN_SMOOTHNESS_END,        ///< Used to iterate.
};

static const uint CUSTOM_TERRAIN_TYPE_NUMBER_DIFFICULTY = 5; ///< Value for custom terrain type in difficulty settings.

static const uint CUSTOM_SEA_LEVEL_NUMBER_DIFFICULTY = 4; ///< Value for custom sea level in difficulty settings.
static const uint CUSTOM_SEA_LEVEL_MIN_PERCENTAGE = 1;    ///< Minimum percentage a user can specify for custom sea level.
static const uint CUSTOM_SEA_LEVEL_MAX_PERCENTAGE = 90;   ///< Maximum percentage a user can specify for custom sea level.

static const uint MAP_HEIGHT_LIMIT_AUTO_MINIMUM = 30; ///< When map height limit is auto, make this the lowest possible map height limit.
static const uint MAP_HEIGHT_LIMIT_AUTO_CEILING_ROOM = 15; ///< When map height limit is auto, the map height limit will be the higest peak plus this value.

typedef void GWDoneProc();  ///< Procedure called when the genworld process finishes
typedef void GWAbortProc(); ///< Called when genworld is aborted

/** Properties of current genworld process */
struct GenWorldInfo {
	bool abort;            ///< Whether to abort the thread ASAP
	GenWorldMode mode;     ///< What mode are we making a world in
	CompanyID lc;          ///< The local_company before generating
	uint size_x;           ///< X-size of the map
	uint size_y;           ///< Y-size of the map
	GWDoneProc *proc;      ///< Proc that is called when done (can be nullptr)
	GWAbortProc *abortp;   ///< Proc that is called when aborting (can be nullptr)
};

/** Current stage of world generation process */
enum GenWorldProgress {
	GWP_MAP_INIT,    ///< Initialize/allocate the map, start economy
	GWP_LANDSCAPE,   ///< Create the landscape
	GWP_RIVER,       ///< Create the rivers
	GWP_ROUGH_ROCKY, ///< Make rough and rocky areas
	GWP_TOWN,        ///< Generate towns
	GWP_INDUSTRY,    ///< Generate industries
	GWP_OBJECT,      ///< Generate objects (radio tower, light houses)
	GWP_TREE,        ///< Generate trees
	GWP_GAME_INIT,   ///< Initialize the game
	GWP_RUNTILELOOP, ///< Runs the tile loop 1280 times to make snow etc
	GWP_RUNSCRIPT,   ///< Runs the game script at most 2500 times, or when ever the script sleeps
	GWP_GAME_START,  ///< Really prepare to start the game
	GWP_CLASS_COUNT
};

/* genworld.cpp */
void GenerateWorldSetCallback(GWDoneProc *proc);
void GenerateWorldSetAbortCallback(GWAbortProc *proc);
void GenerateWorld(GenWorldMode mode, uint size_x, uint size_y, bool reset_settings = true);
void AbortGeneratingWorld();
bool IsGeneratingWorldAborted();
void HandleGeneratingWorldAbortion();

/* genworld_gui.cpp */
void SetNewLandscapeType(byte landscape);
void SetGeneratingWorldProgress(GenWorldProgress cls, uint total);
void IncreaseGeneratingWorldProgress(GenWorldProgress cls);
void PrepareGenerateWorldProgress();
void ShowGenerateWorldProgress();
void StartNewGameWithoutGUI(uint32_t seed);
void ShowCreateScenario();
void StartScenarioEditor();

extern bool _generating_world;

#endif /* GENWORLD_H */
