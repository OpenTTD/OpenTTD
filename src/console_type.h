/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file console_type.h Globally used console related types. */

#ifndef CONSOLE_TYPE_H
#define CONSOLE_TYPE_H

#include "gfx_type.h"

/** Modes of the in-game console. */
enum IConsoleModes {
	ICONSOLE_FULL,   ///< In-game console is closed.
	ICONSOLE_OPENED, ///< In-game console is opened, upper 1/3 of the screen.
	ICONSOLE_CLOSED  ///< In-game console is opened, whole screen.
};

/* Colours of the console messages. */
static const TextColour CC_DEFAULT = TC_SILVER;      ///< Default colour of the console.
static const TextColour CC_ERROR   = TC_RED;         ///< Colour for error lines.
static const TextColour CC_WARNING = TC_LIGHT_BLUE;  ///< Colour for warning lines.
static const TextColour CC_INFO    = TC_YELLOW;      ///< Colour for information lines.
static const TextColour CC_DEBUG   = TC_LIGHT_BROWN; ///< Colour for debug output.
static const TextColour CC_COMMAND = TC_GOLD;        ///< Colour for the console's commands.
static const TextColour CC_WHITE   = TC_WHITE;       ///< White console lines for various things such as the welcome.

#endif /* CONSOLE_TYPE_H */
