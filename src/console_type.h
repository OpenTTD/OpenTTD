/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file console_type.h Globally used console related types. */

#ifndef CONSOLE_TYPE_H
#define CONSOLE_TYPE_H

#include "gfx_type.h"

/** Modes of the in-game console. */
enum IConsoleModes : uint8_t {
	ICONSOLE_FULL,   ///< In-game console is opened, whole screen.
	ICONSOLE_OPENED, ///< In-game console is opened, upper 1/3 of the screen.
	ICONSOLE_CLOSED, ///< In-game console is closed.
};

/* Colours of the console messages. */
static const TextColour CC_DEFAULT = TextColour::Silver; ///< Default colour of the console.
static const TextColour CC_ERROR = TextColour::Red; ///< Colour for error lines.
static const TextColour CC_WARNING = TextColour::LightBlue; ///< Colour for warning lines.
static const TextColour CC_HELP = TextColour::LightBlue; ///< Colour for help lines.
static const TextColour CC_INFO = TextColour::Yellow; ///< Colour for information lines.
static const TextColour CC_DEBUG = TextColour::LightBrown; ///< Colour for debug output.
static const TextColour CC_COMMAND = TextColour::Gold; ///< Colour for the console's commands.
static const TextColour CC_WHITE = TextColour::White; ///< White console lines for various things such as the welcome.

#endif /* CONSOLE_TYPE_H */
