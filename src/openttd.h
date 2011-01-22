/* $Id$ */

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

enum GameMode {
	GM_MENU,
	GM_NORMAL,
	GM_EDITOR,
};

enum SwitchMode {
	SM_NONE,
	SM_NEWGAME,
	SM_RESTARTGAME,
	SM_EDITOR,
	SM_LOAD,
	SM_MENU,
	SM_SAVE,
	SM_GENRANDLAND,
	SM_LOAD_SCENARIO,
	SM_START_HEIGHTMAP,
	SM_LOAD_HEIGHTMAP,
};

/* Display Options */
enum DisplayOptions {
	DO_SHOW_TOWN_NAMES     = 0,
	DO_SHOW_STATION_NAMES  = 1,
	DO_SHOW_SIGNS          = 2,
	DO_FULL_ANIMATION      = 3,
	DO_FULL_DETAIL         = 5,
	DO_SHOW_WAYPOINT_NAMES = 6,
};

extern GameMode _game_mode;
extern SwitchMode _switch_mode;
extern bool _exit_game;

/** Modes of pausing we've got */
enum PauseMode {
	PM_UNPAUSED              = 0,      ///< A normal unpaused game
	PM_PAUSED_NORMAL         = 1 << 0, ///< A game normally paused
	PM_PAUSED_SAVELOAD       = 1 << 1, ///< A game paused for saving/loading
	PM_PAUSED_JOIN           = 1 << 2, ///< A game paused for 'pause_on_join'
	PM_PAUSED_ERROR          = 1 << 3, ///< A game paused because a (critical) error
	PM_PAUSED_ACTIVE_CLIENTS = 1 << 4, ///< A game paused for 'min_active_clients'

	/* Pause mode bits when paused for network reasons */
	PMB_PAUSED_NETWORK = PM_PAUSED_ACTIVE_CLIENTS | PM_PAUSED_JOIN,
};
DECLARE_ENUM_AS_BIT_SET(PauseMode)
typedef SimpleTinyEnumT<PauseMode, byte> PauseModeByte;

/** The current pause mode */
extern PauseModeByte _pause_mode;

#endif /* OPENTTD_H */
