/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
 */

/** @file debug_type.h Types related to debugging. */

#ifndef DEBUG_TYPE_H
#define DEBUG_TYPE_H

/** Debug message severity levels. */
enum class Severity : uint8_t {
	Fatal, ///< Fatal, user should know about this.
	Error, ///< Error, but we are recovering.
	Warning, ///< Warning, wrong but okay if you don't know.
	Notice, ///< Notice.
	Info, ///< Info, information you might care about.
	Debug1, ///< Debug #1 - High level debug messages.
	Debug2, ///< Debug #2 - Low level debug messages.
	Trace1, ///< Trace information #1.
	Trace2, ///< Trace information #2.
	Trace3, ///< Trace information #3.
};

/** Debug facilities. */
enum class Facility : uint8_t {
	Driver, ///< Driver message facility.
	Grf, ///< Grf message facility.
	Map, ///< Map message facility.
	Misc, ///< Misc message facility.
	Net, ///< Net message facility.
	Sprite, ///< Sprite message facility.
	Oldloader, ///< Oldloader message facility.
	Yapf, ///< Yapf message facility.
	Fontcache, ///< Fontcache message facility.
	Script, ///< Script message facility.
	Sl, ///< Saveload message facility.
	Gamelog, ///< Gamelog message facility.
	Desync, ///< Desync message facility.
	Console, ///< Console message facility.
	Random, ///< Random message facility.
	End, ///< End marker.
};

#endif /* DEBUG_TYPE_H */
