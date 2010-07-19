/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file variables.h Messing file that will cease to exist some time in the future. */

#ifndef VARIABLES_H
#define VARIABLES_H

#ifndef VARDEF
#define VARDEF extern
#endif

/* Amount of game ticks */
VARDEF uint16 _tick_counter;

VARDEF byte _display_opt;

/* IN/OUT parameters to commands */
VARDEF bool _generating_world;

VARDEF char *_config_file;
VARDEF char *_highscore_file;
VARDEF char *_log_file;

/* landscape.cpp */
extern const byte _tileh_to_sprite[32];

#endif /* VARIABLES_H */
