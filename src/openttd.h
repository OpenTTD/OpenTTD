/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file openttd.h Some generic types. */

#ifndef OPENTTD_H
#define OPENTTD_H

#include <atomic>
#include <chrono>
#include "core/enum_type.hpp"

/** Mode which defines the state of the game. */
enum class GameMode : uint8_t {
	Menu, ///< In the main menu.
	Normal, ///< Playing a game.
	Editor, ///< In the scenario editor.
	Bootstrap, ///< In the content bootstrap process.
};

/** Mode which defines what mode we're switching to. */
enum class SwitchMode : uint8_t {
	None, ///< Not switching game mode.
	NewGame, ///< New Game --> 'Random game'.
	RestartGame, ///< Restart --> 'Random game' with current settings.
	ReloadGame, ///< Reload the savegame / scenario / heightmap you started the game with.
	Editor, ///< Switch to scenario editor.
	LoadGame, ///< Load game, Play Scenario.
	Menu, ///< Switch to game intro menu.
	SaveGame, ///< Save game.
	SaveHeightmap, ///< Save heightmap.
	GenerateRandomLand, ///< Generate random land within scenario editor.
	LoadScenario, ///< Load scenario from scenario editor.
	StartHeightmap, ///< Load a heightmap and start a new game from it.
	LoadHeightmap, ///< Load heightmap from scenario editor.
	RestartHeightmap, ///< Load a heightmap and start a new game from it with current settings.
	JoinGame, ///< Join a network game.
};

/** Display Options */
enum class DisplayOption : uint8_t {
	ShowTownNames = 0, ///< Display town names.
	ShowStationNames = 1, ///< Display station names.
	ShowSigns = 2, ///< Display signs.
	FullAnimation = 3, ///< Perform palette animation.
	FullDetail = 5, ///< Also draw details of track and roads.
	ShowWaypointNames = 6, ///< Display waypoint names.
	ShowCompetitorSigns = 7, ///< Display signs, station names and waypoint names of opponent companies. Buoys and oilrig-stations are always shown, even if this option is turned off.
};

/** Bitset of \c DisplayOption elements. */
using DisplayOptions = EnumBitSet<DisplayOption, uint8_t>;

struct GameSessionStats {
	std::chrono::steady_clock::time_point start_time; ///< Time when the current game was started.
	std::string savegame_id; ///< Unique ID of the savegame.
	std::optional<size_t> savegame_size; ///< Size of the last saved savegame in bytes, or std::nullopt if not saved yet.
};

extern GameMode _game_mode;
extern SwitchMode _switch_mode;
extern GameSessionStats _game_session_stats;
extern std::atomic<bool> _exit_game;
extern bool _save_config;

/** Modes of pausing we've got */
enum class PauseMode : uint8_t {
	Normal             = 0, ///< A game normally paused
	SaveLoad           = 1, ///< A game paused for saving/loading
	Join               = 2, ///< A game paused for 'pause_on_join'
	Error              = 3, ///< A game paused because a (critical) error
	ActiveClients      = 4, ///< A game paused for 'min_active_clients'
	GameScript         = 5, ///< A game paused by a game script
	LinkGraph          = 6, ///< A game paused due to the link graph schedule lagging
	CommandDuringPause = 7, ///< A game paused, and a command executed during the pause; resets on autosave
};

/** Bitset of \c PauseMode elements. */
using PauseModes = EnumBitSet<PauseMode, uint8_t>;

/** The current pause mode */
extern PauseModes _pause_mode;

void AskExitGame();
void AskExitToGameMenu();

int openttd_main(std::span<std::string_view> arguments);
void StateGameLoop();
void HandleExitGameRequest();

void SwitchToMode(SwitchMode new_mode);

bool RequestNewGRFScan(struct NewGRFScanCallback *callback = nullptr);
void GenerateSavegameId();

void OpenBrowser(const std::string &url);
void ChangeAutosaveFrequency(bool reset);

#endif /* OPENTTD_H */
