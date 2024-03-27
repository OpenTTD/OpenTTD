/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_townlist.hpp List all the towns. */

#ifndef SCRIPT_TOWNLIST_HPP
#define SCRIPT_TOWNLIST_HPP

#include "script_list.hpp"

/**
 * Creates a list of towns that are currently on the map.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptTownList : public ScriptList {
public:
#ifdef DOXYGEN_API
	ScriptTownList();

	/**
	 * Apply a filter when building the list.
	 * @param filter_function The function which will be doing the filtering.
	 * @param ... The params to give to the filters (minus the first param,
	 *  which is always the index-value).
	 * @note You can write your own filters and use them. Just remember that
	 *  the first parameter should be the index-value, and it should return
	 *  a bool.
	 * @note Example:
	 * @code
	 *  local bribeable_towns = ScriptTownList(ScriptTown.IsActionAvailable, ScriptTown.TOWN_ACTION_BRIBE);
	 *
	 *  function MinPopulation(town_id, pop)
	 *  {
	 *    return ScriptTown.GetPopulation(town_id) >= pop;
	 *  }
	 *  local proper_towns = ScriptTownList(MinPopulation, 1000);
	 * @endcode
	 */
	ScriptTownList(function filter_function, ...);
#else
	ScriptTownList(HSQUIRRELVM vm);
#endif /* DOXYGEN_API */
};

/**
 * Creates a list of all TownEffects known in the game.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptTownEffectList : public ScriptList {
public:
	ScriptTownEffectList();
};

#endif /* SCRIPT_TOWNLIST_HPP */
