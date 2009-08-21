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

enum IConsoleModes {
	ICONSOLE_FULL,
	ICONSOLE_OPENED,
	ICONSOLE_CLOSED
};

enum ConsoleColour {
	CC_DEFAULT =  1,
	CC_ERROR   =  3,
	CC_WARNING = 13,
	CC_INFO    =  8,
	CC_DEBUG   =  5,
	CC_COMMAND =  2,
	CC_WHITE   = 12,
};

#endif /* CONSOLE_TYPE_H */
