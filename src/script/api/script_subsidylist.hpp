/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_subsidylist.hpp List all the subsidies. */

#ifndef SCRIPT_SUBSIDYLIST_HPP
#define SCRIPT_SUBSIDYLIST_HPP

#include "script_list.hpp"

/**
 * Creates a list of all current subsidies.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptSubsidyList : public ScriptList {
public:
#ifdef DOXYGEN_API
	ScriptSubsidyList();

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
	 *  function IsType(subsidy_id, type)
	 *  {
	 *    return ScriptSubsidy.GetSourceType(subsidy_id) == type;
	 *  }
	 *  local town_subs = ScriptSubsidyList(IsType, ScriptSubsidy.SPT_TOWN);
	 * @endcode
	 */
	ScriptSubsidyList(function filter_function, ...);
#else
	ScriptSubsidyList(HSQUIRRELVM vm);
#endif /* DOXYGEN_API */
};

#endif /* SCRIPT_SUBSIDYLIST_HPP */
