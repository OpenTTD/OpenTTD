/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file script_priorityqueue.hpp A queue that keeps a list of items sorted by a priority. */
/** @defgroup ScriptPriorityQueue Classes that create a priority queue of items. */

#ifndef SCRIPT_PRIORITYQUEUE_HPP
#define SCRIPT_PRIORITYQUEUE_HPP

#include "script_object.hpp"
#include <utility>

/**
 * Class that creates a queue which keeps its items ordered by an item priority.
 * @api ai game
 */
class ScriptPriorityQueue : public ScriptObject {
public:
	typedef std::pair<SQInteger, HSQOBJECT> PriorityItem;
private:
	struct PriorityComparator {
		bool operator()(const PriorityItem &lhs, const PriorityItem &rhs) const noexcept
		{
			return lhs.first > rhs.first;
		}
	};

	PriorityComparator        comp;
	std::vector<PriorityItem> queue;  ///< The priority list

public:
	~ScriptPriorityQueue();

#ifdef DOXYGEN_API
	/**
	 * Add a single item to the queue.
	 * @param item The item to add. Can be any Squirrel type. Should be unique, otherwise it is ignored.
	 * @param priority The priority to assign the item.
	 * @return True if the item was inserted, false if it was already in the queue.
	 */
	bool Insert(void *item, SQInteger priority);

	/**
	 * Remove and return the item with the lowest priority.
	 * @return The item with the lowest priority, removed from the queue. Returns null on an empty queue.
	 * @pre !IsEmpty()
	 */
	void *Pop();

	/**
	 * Get the item with the lowest priority, keeping it in the queue.
	 * @return The item with the lowest priority. Returns null on an empty queue.
	 * @pre !IsEmpty()
	 */
	void *Peek();

	/**
	 * Check if an items is already included in the queue.
	 * @param item The item to check whether it's already in this queue.
	 * @return true if the items is already in the queue.
	 * @note Performance is O(n), use only when absolutely required.
	 */
	bool Exists(void *item);

	/**
	 * Clear the queue, making Count() returning 0 and IsEmpty() returning true.
	 */
	void Clear();
#else
	SQInteger Insert(HSQUIRRELVM vm);
	SQInteger Pop(HSQUIRRELVM vm);
	SQInteger Peek(HSQUIRRELVM vm);
	SQInteger Exists(HSQUIRRELVM vm);
	SQInteger Clear(HSQUIRRELVM vm);
#endif /* DOXYGEN_API */

	/**
	 * Check if the queue is empty.
	 * @return true if the queue is empty.
	 */
	bool IsEmpty();

	/**
	 * Returns the amount of items in the queue.
	 * @return amount of items in the queue.
	 */
	SQInteger Count();
};

#endif /* SCRIPT_PRIORITYQUEUE_HPP */
