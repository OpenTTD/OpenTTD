/* $Id$ */

/** @file variables.h Messing file that will cease to exist some time in the future. */

#ifndef VARIABLES_H
#define VARIABLES_H

#ifndef VARDEF
#define VARDEF extern
#endif

/* Amount of game ticks */
VARDEF uint16 _tick_counter;

/* Skip aging of cargo? */
VARDEF byte _age_cargo_skip_counter;

/* Also save scrollpos_x, scrollpos_y and zoom */
VARDEF uint16 _disaster_delay;

/* Determines what station to operate on in the
 *  tick handler. */
VARDEF uint16 _station_tick_ctr;

/* Iterator through all towns in OnTick_Town */
VARDEF uint32 _cur_town_ctr;
/* Frequency iterator at the same place */
VARDEF uint32 _cur_town_iter;

VARDEF uint _cur_player_tick_index;
VARDEF uint _next_competitor_start;

/* Determines how often to run the tree loop */
VARDEF byte _trees_tick_ctr;

/* Keep track of current game position */
VARDEF int _saved_scrollpos_x;
VARDEF int _saved_scrollpos_y;

/* NOSAVE: Used in palette animations only, not really important. */
VARDEF int _palette_animation_counter;


VARDEF uint32 _frame_counter;
VARDEF uint32 _realtime_tick;

VARDEF bool _is_old_ai_player; // current player is an oldAI player? (enables a lot of cheats..)

VARDEF bool _do_autosave;
VARDEF int _autosave_ctr;

VARDEF byte _display_opt;
VARDEF int _caret_timer;

VARDEF bool _rightclick_emulate;

/* IN/OUT parameters to commands */
VARDEF bool _generating_world;

/* Used when switching from the intro menu. */
VARDEF byte _switch_mode;

VARDEF char _savegame_format[8];

VARDEF char *_config_file;
VARDEF char *_highscore_file;
VARDEF char *_log_file;

/* landscape.cpp */
extern const byte _tileh_to_sprite[32];

/* misc */
VARDEF char _screenshot_name[128];
VARDEF byte _vehicle_design_names;

/* Forking stuff */
VARDEF bool _dedicated_forks;

#endif /* VARIABLES_H */
