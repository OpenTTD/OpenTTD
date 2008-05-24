/* $Id$ */

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
