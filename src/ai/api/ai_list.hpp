/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file ai_list.hpp List custom entries. */

#ifndef AI_LIST_HPP
#define AI_LIST_HPP

#include "ai_abstractlist.hpp"

/**
 * Creates an empty list, in which you can add integers.
 * @ingroup AIList
 */
class AIList : public AIAbstractList {
public:
	static const char *GetClassName() { return "AIList"; }

public:
	/**
	 * Add an item to the list.
	 * @param item the item to add.
	 * @param value the value to assign.
	 */
	void AddItem(int32 item, int32 value);

	/**
	 * Change the value of an item in the list.
	 * @param item the item to change
	 * @param value the value to assign.
	 */
	void ChangeItem(int32 item, int32 value);

	/**
	 * Remove the item from the list.
	 * @param item the item to remove.
	 */
	void RemoveItem(int32 item);

#ifndef DOXYGEN_SKIP
	/**
	 * Used for [] set from Squirrel.
	 */
	SQInteger _set(HSQUIRRELVM vm);
#endif /* DOXYGEN_SKIP */
};

#endif /* AI_LIST_HPP */
