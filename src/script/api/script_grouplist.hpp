/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_grouplist.hpp List all the groups (you own). */

#ifndef SCRIPT_GROUPLIST_HPP
#define SCRIPT_GROUPLIST_HPP

#include "script_list.hpp"

/**
 * Creates a list of groups of which you are the owner.
 * @note Neither ScriptGroup::GROUP_ALL nor ScriptGroup::GROUP_DEFAULT is in this list.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptGroupList : public ScriptList {
public:
#ifdef DOXYGEN_API
	/**
	 * @game @pre ScriptCompanyMode::IsValid().
	 */
	ScriptGroupList();

	/**
	 * Apply a filter when building the list.
	 * @param filter_function The function which will be doing the filtering.
	 * @param ... The params to give to the filters (minus the first param,
	 *  which is always the index-value).
	 * @game @pre ScriptCompanyMode::IsValid().
	 * @note You can write your own filters and use them. Just remember that
	 *  the first parameter should be the index-value, and it should return
	 *  a bool.
	 * @note Example:
	 * @code
	 *  function IsType(group_id, type)
	 *  {
	 *    return ScriptGroup.GetVehicleType(group_id) == type;
	 *  }
	 *  local rv_groups = ScriptGroupList(IsType, ScriptVehicle.VT_ROAD);
	 * @endcode
	 */
	ScriptGroupList(function filter_function, ...);
#else
	/**
	 * The constructor wrapper from Squirrel.
	 */
	ScriptGroupList(HSQUIRRELVM vm);
#endif /* DOXYGEN_API */
};

#endif /* SCRIPT_GROUPLIST_HPP */
