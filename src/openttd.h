/* $Id$ */

/** @file openttd.h Some generic types. */

#ifndef OPENTTD_H
#define OPENTTD_H

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
extern int8 _pause_game;

#endif /* OPENTTD_H */
