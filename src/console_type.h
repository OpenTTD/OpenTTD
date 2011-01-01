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

/** Modes of the in-game console. */
enum IConsoleModes {
	ICONSOLE_FULL,   ///< In-game console is closed.
	ICONSOLE_OPENED, ///< In-game console is opened, upper 1/3 of the screen.
	ICONSOLE_CLOSED  ///< In-game console is opened, whole screen.
};

/** Colours of the console messages. */
enum ConsoleColour {
	CC_DEFAULT =  1,
	CC_ERROR   =  3,
	CC_WARNING = 13,
	CC_INFO    =  8,
	CC_DEBUG   =  5,
	CC_COMMAND =  2,
	CC_WHITE   = 12,
};

static inline bool IsValidConsoleColour(uint c)
{
	return c == CC_DEFAULT || c == CC_ERROR || c == CC_WARNING || c == CC_INFO ||
			c == CC_DEBUG || c == CC_COMMAND || c == CC_WHITE;
}

#endif /* CONSOLE_TYPE_H */
