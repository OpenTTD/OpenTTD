/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file squirrel_std.h defines the Squirrel Standard Function class */

#ifndef SQUIRREL_STD_HPP
#define SQUIRREL_STD_HPP

#include "squirrel.h"

#if defined(__APPLE__)
/* Which idiotic system makes 'require' a macro? :s Oh well.... */
#undef require
#endif /* __APPLE__ */

/**
 * By default we want to give a set of standard commands to a SQ script.
 * Most of them are easy wrappers around internal functions. Of course we
 *  could just as easy include things like the stdmath of SQ, but of those
 *  functions we are sure they work on all our supported targets.
 */
class SquirrelStd {
public:

	/**
	 * Get the lowest of two integers.
	 */
	static SQInteger min(HSQUIRRELVM vm);

	/**
	 * Get the highest of two integers.
	 */
	static SQInteger max(HSQUIRRELVM vm);

	/**
	 * Load another file on runtime.
	 * @note This is always loaded on the root-level, no matter where you call this.
	 * @note The filename is always relative from the script it is called from. Absolute calls are NOT allowed!
	 */
	static SQInteger require(HSQUIRRELVM vm);

	/**
	 * Enable/disable stack trace showing for handled exceptions.
	 */
	static SQInteger notifyallexceptions(HSQUIRRELVM vm);
};

/**
 * Register all standard functions we want to give to a script.
 */
void squirrel_register_std(Squirrel *engine);

/**
 * Register all standard functions that are available on first startup.
 * @note this set is very limited, and is only meant to load other scripts and things like that.
 */
void squirrel_register_global_std(Squirrel *engine);

#endif /* SQUIRREL_STD_HPP */
