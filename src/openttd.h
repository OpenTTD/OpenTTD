/* $Id$ */

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
	SM_EDITOR,
	SM_LOAD,
	SM_MENU,
	SM_SAVE,
	SM_GENRANDLAND,
	SM_LOAD_SCENARIO,
	SM_START_SCENARIO,
	SM_START_HEIGHTMAP,
	SM_LOAD_HEIGHTMAP,
};

/* Display Options */
enum {
	DO_SHOW_TOWN_NAMES    = 0,
	DO_SHOW_STATION_NAMES = 1,
	DO_SHOW_SIGNS         = 2,
	DO_FULL_ANIMATION     = 3,
	DO_FULL_DETAIL        = 5,
	DO_WAYPOINTS          = 6,
};

extern GameMode _game_mode;
extern SwitchMode _switch_mode;
extern bool _exit_game;

/** Modes of pausing we've got */
enum PauseMode {
	PM_UNPAUSED        = 0,      ///< A normal unpaused game
	PM_PAUSED_NORMAL   = 1 << 0, ///< A game normally paused
	PM_PAUSED_SAVELOAD = 1 << 1, ///< A game paused for saving/loading
	PM_PAUSED_JOIN     = 1 << 2, ///< A game paused for 'pause on join'
	PM_PAUSED_ERROR    = 1 << 3, ///< A game paused because a (critical) error
};
DECLARE_ENUM_AS_BIT_SET(PauseMode);
typedef SimpleTinyEnumT<PauseMode, byte> PauseModeByte;

/** The current pause mode */
extern PauseModeByte _pause_mode;

#endif /* OPENTTD_H */
