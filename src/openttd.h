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
enum GameMode : uint8_t {
	GM_MENU,
	GM_NORMAL,
	GM_EDITOR,
	GM_BOOTSTRAP
};

/** Mode which defines what mode we're switching to. */
enum SwitchMode : uint8_t {
	SM_NONE,
	SM_NEWGAME,           ///< New Game --> 'Random game'.
	SM_RESTARTGAME,       ///< Restart --> 'Random game' with current settings.
	SM_RELOADGAME,        ///< Reload the savegame / scenario / heightmap you started the game with.
	SM_EDITOR,            ///< Switch to scenario editor.
	SM_LOAD_GAME,         ///< Load game, Play Scenario.
	SM_MENU,              ///< Switch to game intro menu.
	SM_SAVE_GAME,         ///< Save game.
	SM_SAVE_HEIGHTMAP,    ///< Save heightmap.
	SM_GENRANDLAND,       ///< Generate random land within scenario editor.
	SM_LOAD_SCENARIO,     ///< Load scenario from scenario editor.
	SM_START_HEIGHTMAP,   ///< Load a heightmap and start a new game from it.
	SM_LOAD_HEIGHTMAP,    ///< Load heightmap from scenario editor.
	SM_RESTART_HEIGHTMAP, ///< Load a heightmap and start a new game from it with current settings.
	SM_JOIN_GAME,         ///< Join a network game.
};

/** Display Options */
enum DisplayOptions : uint8_t {
	DO_SHOW_TOWN_NAMES     = 0, ///< Display town names.
	DO_SHOW_STATION_NAMES  = 1, ///< Display station names.
	DO_SHOW_SIGNS          = 2, ///< Display signs.
	DO_FULL_ANIMATION      = 3, ///< Perform palette animation.
	DO_FULL_DETAIL         = 5, ///< Also draw details of track and roads.
	DO_SHOW_WAYPOINT_NAMES = 6, ///< Display waypoint names.
	DO_SHOW_COMPETITOR_SIGNS = 7, ///< Display signs, station names and waypoint names of opponent companies. Buoys and oilrig-stations are always shown, even if this option is turned off.
};

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
