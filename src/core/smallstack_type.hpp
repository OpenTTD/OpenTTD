/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file smallstack_type.hpp Minimal stack that uses a pool to avoid pointers and doesn't allocate any heap memory if there is only one valid item. */

#ifndef SMALLSTACK_TYPE_HPP
#define SMALLSTACK_TYPE_HPP

#include "pool_type.hpp"
#include "pool_func.hpp"

/**
 * Base class for SmallStack. We cannot add this into SmallStack itself as
 * certain compilers don't like it.
 */
template <typename Tindex, typename Titem>
class SmallStackItem {
protected:
	Tindex next; ///< Pool index of next item.
	Titem value; ///< Value of current item.

	/**
	 * Create a new item.
	 * @param value Value of the item.
	 * @param next Next item in the stack.
	 */
	inline SmallStackItem(const Titem &value, Tindex next) :
		next(next), value(value) {}
};

/**
 * Minimal stack that uses a pool to avoid pointers. It has some peculiar
 * properties that make it useful for passing around lists of IDs but not much
 * else:
 * 1. It always includes an invalid item as bottom.
 * 2. It doesn't have a deep copy operation but uses smart pointers instead.
 *    Every copy is thus implicitly shared.
 * 3. Its items are immutable.
 * 4. Due to 2. and 3. memory management can be done by "branch counting".
 *    Whenever you copy a smallstack, the first item on the heap increases its
 *    branch_count, signifying that there are multiple items "in front" of it.
 *    When deleting a stack items are deleted up to the point where
 *    branch_count > 0.
 * 5. You can choose your own index type, so that you can align it with your
 *    value type. E.G. value types of 16 bits length like to be combined with
 *    index types of the same length.
 * @tparam Titem Value type to be used.
 * @tparam Tindex Index type to use for the pool.
 * @tparam Tinvalid Invalid item to keep at the bottom of each stack.
 * @tparam Tgrowth_step Growth step for pool.
 * @tparam Tmax_size Maximum size for pool.
 */
template <typename Titem, typename Tindex, Titem Tinvalid, size_t Tgrowth_step, size_t Tmax_size>
class SmallStack : public SmallStackItem<Tindex, Titem> {
protected:
	class PooledSmallStack;

	/**
	 * Create a branch in the pool if necessary.
	 */
	void Branch()
	{
		if (PooledSmallStack::IsValidID(this->next)) {
			PooledSmallStack::Get(this->next)->CreateBranch();
		}
	}

public:
	typedef SmallStackItem<Tindex, Titem> Item;
	typedef Pool<PooledSmallStack, Tindex, Tgrowth_step, Tmax_size, PT_NORMAL, true, false> SmallStackPool;

	/**
	 * Constructor for a stack with one or two items in it.
	 * @param value Initial item. If not missing or Tinvalid there will be Tinvalid below it.
	 */
	inline SmallStack(const Titem &value = Tinvalid) : Item(value, Tmax_size) {}

	/**
	 * Remove the head of stack and all other items members that are unique to it.
	 */
	inline ~SmallStack()
	{
		if (PooledSmallStack::IsValidID(this->next)) {
			PooledSmallStack *item = PooledSmallStack::Get(this->next);
			if (item->NumBranches() == 0) {
				delete item;
			} else {
				item->DeleteBranch();
			}
		}
	}

	/**
	 * Shallow copy the stack, marking the first item as branched.
	 * @param other Stack to copy from
	 */
	inline SmallStack(const SmallStack &other) : Item(other) { this->Branch(); }

	/**
	 * Shallow copy the stack, marking the first item as branched.
	 * @param other Stack to copy from
	 * @return This smallstack.
	 */
	inline SmallStack &operator=(const SmallStack &other)
	{
		if (this == &other) return *this;
		this->~SmallStack();
		this->next = other.next;
		this->value = other.value;
		this->Branch();
		return *this;
	}

	/**
	 * Push a new item onto the stack.
	 * @param item Item to be pushed.
	 */
	inline void Push(const Titem &item)
	{
		if (this->value != Tinvalid) {
			assert(PooledSmallStack::CanAllocateItem());
			PooledSmallStack *next = new PooledSmallStack(this->value, this->next);
			this->next = next->index;
		}
		this->value = item;
	}

	/**
	 * Pop an item from the stack.
	 * @return Current top of stack.
	 */
	inline Titem Pop()
	{
		Titem ret = this->value;
		if (!PooledSmallStack::IsValidID(this->next)) {
			this->value = Tinvalid;
		} else {
			PooledSmallStack *next = PooledSmallStack::Get(this->next);
			static_cast<Item &>(*this) = *next;
			if (next->NumBranches() == 0) {
				delete next;
			} else {
				next->DeleteBranch();
				this->Branch();
			}
		}
		return ret;
	}

	/**
	 * Check if the stack is empty.
	 * @return If the stack is empty.
	 */
	inline bool IsEmpty() const
	{
		return this->value == Tinvalid && !PooledSmallStack::IsValidID(this->next);
	}

	/**
	 * Check if the given item is contained in the stack.
	 * @param item Item to look for.
	 * @return If the item is in the stack.
	 */
	inline bool Contains(const Titem &item) const
	{
		if (item == Tinvalid || item == this->value) return true;
		const SmallStack *in_list = this;
		while (PooledSmallStack::IsValidID(in_list->next)) {
			in_list = static_cast<const SmallStack *>(
					static_cast<const Item *>(PooledSmallStack::Get(in_list->next)));
			if (in_list->value == item) return true;
		}
		return false;
	}

protected:
	static SmallStackPool _pool;

	/**
	 * SmallStack item that can be kept in a pool (by having an index).
	 */
	class PooledSmallStack : public Item, public SmallStackPool::template PoolItem<&SmallStack::_pool> {
	private:
		Tindex branch_count; ///< Number of branches in the tree structure this item is parent of
	public:
		PooledSmallStack(Titem value, Tindex next) : Item(value, next), branch_count(0) {}

		inline void CreateBranch() { ++this->branch_count; }
		inline void DeleteBranch() { --this->branch_count; }
		inline Tindex NumBranches() { return this->branch_count; }
	};
};


#endif
