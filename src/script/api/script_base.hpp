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
 */
class ScriptBase : public ScriptObject {
public:
	/**
	 * Get a random value.
	 * @return A random value between 0 and MAX(uint32_t).
	 */
	static SQInteger Rand();

	/**
	 * Get a random value.
	 * @param unused_param This parameter is not used, but is needed to work with lists.
	 * @return A random value between 0 and MAX(uint32_t).
	 */
	static SQInteger RandItem(SQInteger unused_param);

	/**
	 * Get a random value in a range.
	 * @param max The first number this function will never return (the maximum it returns is max - 1).
	 *            The value will be clamped to 0 .. MAX(uint32_t).
	 * @return A random value between 0 .. max - 1.
	 */
	static SQInteger RandRange(SQInteger max);

	/**
	 * Get a random value in a range.
	 * @param unused_param This parameter is not used, but is needed to work with lists.
	 * @param max The first number this function will never return (the maximum it returns is max - 1).
	 *            The value will be clamped to 0 .. MAX(uint32_t).
	 * @return A random value between 0 .. max - 1.
	 */
	static SQInteger RandRangeItem(SQInteger unused_param, SQInteger max);

	/**
	 * Returns approximately 'out' times true when called 'max' times.
	 *   After all, it is a random function.
	 * @param out How many times it should return true.
	 *            The value will be clamped to 0 .. MAX(uint32_t).
	 * @param max Out of this many times.
	 *            The value will be clamped to 0 .. MAX(uint32_t).
	 * @pre \a out is at most equal to \a max.
	 * @return True if the chance worked out.
	 */
	static bool Chance(SQInteger out, SQInteger max);

	/**
	 * Returns approximately 'out' times true when called 'max' times.
	 *   After all, it is a random function.
	 * @param unused_param This parameter is not used, but is needed to work with lists.
	 * @param out How many times it should return true.
	 *            The value will be clamped to 0 .. MAX(uint32_t).
	 * @param max Out of this many times.
	 *            The value will be clamped to 0 .. MAX(uint32_t).
	 * @pre \a out is at most equal to \a max.
	 * @return True if the chance worked out.
	 */
	static bool ChanceItem(SQInteger unused_param, SQInteger out, SQInteger max);
};

#endif /* SCRIPT_BASE_HPP */
