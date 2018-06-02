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

#include "smallvec_type.hpp"
#include "../thread/thread.h"

/**
 * A simplified pool which stores values instead of pointers and doesn't
 * redefine operator new/delete. It also never zeroes memory and always reuses
 * it.
 */
template<typename Titem, typename Tindex, Tindex Tgrowth_step, Tindex Tmax_size>
class SimplePool {
public:
	inline SimplePool() : first_unused(0), first_free(0), mutex(ThreadMutex::New()) {}
	inline ~SimplePool() { delete this->mutex; }

	/**
	 * Get the mutex. We don't lock the mutex in the pool methods as the
	 * SmallStack isn't necessarily in a consistent state after each method.
	 * @return Mutex.
	 */
	inline ThreadMutex *GetMutex() { return this->mutex; }

	/**
	 * Get the item at position index.
	 * @return Item at index.
	 */
	inline Titem &Get(Tindex index) { return this->data[index]; }

	/**
	 * Create a new item and return its index.
	 * @return Index of new item.
	 */
	inline Tindex Create()
	{
		Tindex index = this->FindFirstFree();
		if (index < Tmax_size) {
			this->data[index].valid = true;
			this->first_free = index + 1;
			this->first_unused = max(this->first_unused, this->first_free);
		}
		return index;
	}

	/**
	 * Destroy (or rather invalidate) the item at the given index.
	 * @param index Index of item to be destroyed.
	 */
	inline void Destroy(Tindex index)
	{
		this->data[index].valid = false;
		this->first_free = min(this->first_free, index);
	}

private:

	inline Tindex FindFirstFree()
	{
		Tindex index = this->first_free;
		for (; index < this->first_unused; index++) {
			if (!this->data[index].valid) return index;
		}

		if (index >= this->data.Length() && index < Tmax_size) {
			this->data.Resize(index + 1);
		}
		return index;
	}

	struct SimplePoolPoolItem : public Titem {
		bool valid;
	};

	Tindex first_unused;
	Tindex first_free;

	ThreadMutex *mutex;
	SmallVector<SimplePoolPoolItem, Tgrowth_step> data;
};

/**
 * Base class for SmallStack. We cannot add this into SmallStack itself as
 * certain compilers don't like it.
 */
template <typename Titem, typename Tindex>
struct SmallStackItem {
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
 * 6. All accesses to the underlying pool are guarded by a mutex and atomic in
 *    the sense that the mutex stays locked until the pool has reacquired a
 *    consistent state. This means that even though a common data structure is
 *    used the SmallStack is still reentrant.
 * @tparam Titem Value type to be used.
 * @tparam Tindex Index type to use for the pool.
 * @tparam Tinvalid Invalid item to keep at the bottom of each stack.
 * @tparam Tgrowth_step Growth step for pool.
 * @tparam Tmax_size Maximum size for pool.
 */
template <typename Titem, typename Tindex, Titem Tinvalid, Tindex Tgrowth_step, Tindex Tmax_size>
class SmallStack : public SmallStackItem<Titem, Tindex> {
public:

	typedef SmallStackItem<Titem, Tindex> Item;

	/**
	 * SmallStack item that can be kept in a pool.
	 */
	struct PooledSmallStack : public Item {
		Tindex branch_count; ///< Number of branches in the tree structure this item is parent of
	};

	typedef SimplePool<PooledSmallStack, Tindex, Tgrowth_step, Tmax_size> SmallStackPool;

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
		/* Pop() locks the mutex and after each pop the pool is consistent.*/
		while (this->next != Tmax_size) this->Pop();
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
		while (this->next != Tmax_size) this->Pop();
		this->next = other.next;
		this->value = other.value;
		/* Deleting and branching are independent operations, so it's fine to
		 * acquire separate locks for them. */
		this->Branch();
		return *this;
	}

	/**
	 * Pushes a new item onto the stack if there is still space in the
	 * underlying pool. Otherwise the topmost item's value gets overwritten.
	 * @param item Item to be pushed.
	 */
	inline void Push(const Titem &item)
	{
		if (this->value != Tinvalid) {
			ThreadMutexLocker lock(SmallStack::GetPool().GetMutex());
			Tindex new_item = SmallStack::GetPool().Create();
			if (new_item != Tmax_size) {
				PooledSmallStack &pushed = SmallStack::GetPool().Get(new_item);
				pushed.value = this->value;
				pushed.next = this->next;
				pushed.branch_count = 0;
				this->next = new_item;
			}
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
		if (this->next == Tmax_size) {
			this->value = Tinvalid;
		} else {
			ThreadMutexLocker lock(SmallStack::GetPool().GetMutex());
			PooledSmallStack &popped = SmallStack::GetPool().Get(this->next);
			this->value = popped.value;
			if (popped.branch_count == 0) {
				SmallStack::GetPool().Destroy(this->next);
			} else {
				--popped.branch_count;
				/* We can't use Branch() here as we already have the mutex.*/
				if (popped.next != Tmax_size) {
					++(SmallStack::GetPool().Get(popped.next).branch_count);
				}
			}
			/* Accessing popped here is no problem as the pool will only set
			 * the validity flag, not actually delete the item, on Destroy().
			 * It's impossible for another thread to acquire the same item in
			 * the mean time because of the mutex. */
			this->next = popped.next;
		}
		return ret;
	}

	/**
	 * Check if the stack is empty.
	 * @return If the stack is empty.
	 */
	inline bool IsEmpty() const
	{
		return this->value == Tinvalid && this->next == Tmax_size;
	}

	/**
	 * Check if the given item is contained in the stack.
	 * @param item Item to look for.
	 * @return If the item is in the stack.
	 */
	inline bool Contains(const Titem &item) const
	{
		if (item == Tinvalid || item == this->value) return true;
		if (this->next != Tmax_size) {
			ThreadMutexLocker lock(SmallStack::GetPool().GetMutex());
			const SmallStack *in_list = this;
			do {
				in_list = static_cast<const SmallStack *>(
						static_cast<const Item *>(&SmallStack::GetPool().Get(in_list->next)));
				if (in_list->value == item) return true;
			} while (in_list->next != Tmax_size);
		}
		return false;
	}

protected:
	static SmallStackPool &GetPool()
	{
		static SmallStackPool pool;
		return pool;
	}

	/**
	 * Create a branch in the pool if necessary.
	 */
	inline void Branch()
	{
		if (this->next != Tmax_size) {
			ThreadMutexLocker lock(SmallStack::GetPool().GetMutex());
			++(SmallStack::GetPool().Get(this->next).branch_count);
		}
	}
};

#endif
