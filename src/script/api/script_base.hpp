/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_base.hpp Everything to query basic things. */

#ifndef SCRIPT_BASE_HPP
#define SCRIPT_BASE_HPP

#include "script_object.hpp"

/**
 * Class that handles some basic functions.
 * @api ai game
 *
 * @note The random functions are not called Random and RandomRange, because
 *        RANDOM_DEBUG does some tricky stuff, which messes with those names.
 * @note In MP we cannot use Random because that will cause desyncs (scripts are
 *        ran on the server only, not on all clients). This means that
 *        we use InteractiveRandom in MP. Rand() takes care of this for you.
 */
class ScriptBase : public ScriptObject {
public:
	/**
	 * Get a random value.
	 * @return A random value between 0 and MAX(uint32).
	 */
	static uint32 Rand();

	/**
	 * Get a random value.
	 * @param unused_param This parameter is not used, but is needed to work with lists.
	 * @return A random value between 0 and MAX(uint32).
	 */
	static uint32 RandItem(int unused_param);

	/**
	 * Get a random value in a range.
	 * @param max The first number this function will never return (the maximum it returns is max - 1).
	 * @return A random value between 0 .. max - 1.
	 */
	static uint RandRange(uint max);

	/**
	 * Get a random value in a range.
	 * @param unused_param This parameter is not used, but is needed to work with lists.
	 * @param max The first number this function will never return (the maximum it returns is max - 1).
	 * @return A random value between 0 .. max - 1.
	 */
	static uint RandRangeItem(int unused_param, uint max);

	/**
	 * Returns approximately 'out' times true when called 'max' times.
	 *   After all, it is a random function.
	 * @param out How many times it should return true.
	 * @param max Out of this many times.
	 * @pre \a out is at most equal to \a max.
	 * @return True if the chance worked out.
	 */
	static bool Chance(uint out, uint max);

	/**
	 * Returns approximately 'out' times true when called 'max' times.
	 *   After all, it is a random function.
	 * @param unused_param This parameter is not used, but is needed to work with lists.
	 * @param out How many times it should return true.
	 * @param max Out of this many times.
	 * @pre \a out is at most equal to \a max.
	 * @return True if the chance worked out.
	 */
	static bool ChanceItem(int unused_param, uint out, uint max);
};

#endif /* SCRIPT_BASE_HPP */
