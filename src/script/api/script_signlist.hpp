/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_signlist.hpp List all the signs of your company. */

#ifndef SCRIPT_SIGNLIST_HPP
#define SCRIPT_SIGNLIST_HPP

#include "script_list.hpp"

/**
 * Create a list of signs your company has created.
 * @api ai game
 * @ingroup ScriptList
 */
class ScriptSignList : public ScriptList {
public:
#ifdef DOXYGEN_API
	ScriptSignList();

	/**
	 * Apply a filter when building the list.
	 * @param filter_function The function which will be doing the filtering.
	 * @param params The params to give to the filters (minus the first param,
	 *  which is always the index-value).
	 * @note You can write your own filters and use them. Just remember that
	 *  the first parameter should be the index-value, and it should return
	 *  a bool.
	 * @note Example:
	 *  function Contains(sign_id, str)
	 *  {
	 *    local name = ScriptSign.GetName(sign_id);
	 *    return name != null && name.find(str) != null;
	 *  }
	 *  ScriptSignList(Contains, "something");
	 */
	ScriptSignList(void *filter_function, int params, ...);
#else
	ScriptSignList(HSQUIRRELVM);
#endif /* DOXYGEN_API */
};

#endif /* SCRIPT_SIGNLIST_HPP */
