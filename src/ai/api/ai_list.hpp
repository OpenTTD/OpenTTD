/* $Id$ */

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
