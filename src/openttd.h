/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file openttd.h Some generic types. */

#ifndef OPENTTD_H
#define OPENTTD_H

#include "core/enum_type.hpp"

/** Mode which defines the state of the game. */
enum GameMode {
	GM_MENU,
	GM_NORMAL,
	GM_EDITOR,
	GM_BOOTSTRAP
};

/** Mode which defines what mode we're switching to. */
enum SwitchMode {
	SM_NONE,
	SM_NEWGAME,         ///< New Game --> 'Random game'.
	SM_RESTARTGAME,     ///< Restart --> 'Random game' with current settings.
	SM_EDITOR,          ///< Switch to scenario editor.
	SM_LOAD_GAME,       ///< Load game, Play Scenario.
	SM_MENU,            ///< Switch to game intro menu.
	SM_SAVE_GAME,       ///< Save game.
	SM_SAVE_HEIGHTMAP,  ///< Save heightmap.
	SM_GENRANDLAND,     ///< Generate random land within scenario editor.
	SM_LOAD_SCENARIO,   ///< Load scenario from scenario editor.
	SM_START_HEIGHTMAP, ///< Load a heightmap and start a new game from it.
	SM_LOAD_HEIGHTMAP,  ///< Load heightmap from scenario editor.
};

/** Display Options */
enum DisplayOptions {
	DO_SHOW_TOWN_NAMES     = 0, ///< Display town names.
	DO_SHOW_STATION_NAMES  = 1, ///< Display station names.
	DO_SHOW_SIGNS          = 2, ///< Display signs.
	DO_FULL_ANIMATION      = 3, ///< Perform palette animation.
	DO_FULL_DETAIL         = 5, ///< Also draw details of track and roads.
	DO_SHOW_WAYPOINT_NAMES = 6, ///< Display waypoint names.
	DO_SHOW_COMPETITOR_SIGNS = 7, ///< Display signs, station names and waypoint names of opponent companies. Buoys and oilrig-stations are always shown, even if this option is turned off.
};

extern GameMode _game_mode;
extern SwitchMode _switch_mode;
extern bool _exit_game;

/** Modes of pausing we've got */
enum PauseMode : byte {
	PM_UNPAUSED              = 0,      ///< A normal unpaused game
	PM_PAUSED_NORMAL         = 1 << 0, ///< A game normally paused
	PM_PAUSED_SAVELOAD       = 1 << 1, ///< A game paused for saving/loading
	PM_PAUSED_JOIN           = 1 << 2, ///< A game paused for 'pause_on_join'
	PM_PAUSED_ERROR          = 1 << 3, ///< A game paused because a (critical) error
	PM_PAUSED_ACTIVE_CLIENTS = 1 << 4, ///< A game paused for 'min_active_clients'
	PM_PAUSED_GAME_SCRIPT    = 1 << 5, ///< A game paused by a game script

	/** Pause mode bits when paused for network reasons. */
	PMB_PAUSED_NETWORK = PM_PAUSED_ACTIVE_CLIENTS | PM_PAUSED_JOIN,
};
DECLARE_ENUM_AS_BIT_SET(PauseMode)

/** The current pause mode */
extern PauseMode _pause_mode;

void AskExitGame();
void AskExitToGameMenu();

int openttd_main(int argc, char *argv[]);
void HandleExitGameRequest();

void SwitchToMode(SwitchMode new_mode);

#endif /* OPENTTD_H */
