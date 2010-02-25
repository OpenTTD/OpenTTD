/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file binaryheap.hpp Binary heap implementation. */

#ifndef  BINARYHEAP_HPP
#define  BINARYHEAP_HPP

/* Enable it if you suspect binary heap doesn't work well */
#define BINARYHEAP_CHECK 0

#if BINARYHEAP_CHECK
	#define CHECK_CONSISTY() this->CheckConsistency()
#else
	#define CHECK_CONSISTY() ;
#endif

/**
 * Binary Heap as C++ template.
 *
 * For information about Binary Heap algotithm,
 *   see: http://www.policyalmanac.org/games/binaryHeaps.htm
 *
 * Implementation specific notes:
 *
 * 1) It allocates space for item pointers (array). Items are allocated elsewhere.
 *
 * 2) T*[0] is never used. Total array size is max_items + 1, because we
 *    use indices 1..max_items instead of zero based C indexing.
 *
 * 3) Item of the binary heap should support these public members:
 *    - 'lower-than' operator '<' - used for comparing items before moving
 *
 */

template <class T>
class CBinaryHeapT {
private:
	uint items;    ///< Number of items in the heap
	uint capacity; ///< Maximum number of items the heap can hold
	T **data;      ///< The heap item pointers

public:
	explicit CBinaryHeapT(uint max_items)
		: items(0)
		, capacity(max_items)
	{
		this->data = MallocT<T *>(max_items + 1);
	}

	~CBinaryHeapT()
	{
		this->Clear();
		free(this->data);
		this->data = NULL;
	}

protected:
	/** Heapify (move gap) down */
	FORCEINLINE uint HeapifyDown(uint gap, T *item)
	{
		assert(gap != 0);

		uint child = gap * 2; // first child is at [parent * 2]

		/* while children are valid */
		while (child <= this->items) {
			/* choose the smaller child */
			if (child < this->items && *this->data[child + 1] < *this->data[child])
				child++;
			/* is it smaller than our parent? */
			if (!(*this->data[child] < *item)) {
				/* the smaller child is still bigger or same as parent => we are done */
				break;
			}
			/* if smaller child is smaller than parent, it will become new parent */
			this->data[gap] = this->data[child];
			gap = child;
			/* where do we have our new children? */
			child = gap * 2;
		}
		return gap;
	}

	/** Heapify (move gap) up */
	FORCEINLINE uint HeapifyUp(uint gap, T *item)
	{
		assert(gap != 0);

		uint parent;

		while (gap > 1) {
			/* compare [gap] with its parent */
			parent = gap / 2;
			if (!(*item < *this->data[parent])) {
				/* we don't need to continue upstairs */
				break;
			}
			this->data[gap] = this->data[parent];
			gap = parent;
		}
		return gap;
	}

#if BINARYHEAP_CHECK
	/** verifies the heap consistency (added during first YAPF debug phase) */
	FORCEINLINE void CheckConsistency()
	{
		for (uint child = 2; child <= this->items; child++) {
			uint parent = child / 2;
			assert(!(*this->data[child] < *this->data[parent]));
		}
	}
#endif

public:
	/** Return the number of items stored in the priority queue.
	 *  @return number of items in the queue */
	FORCEINLINE uint Size() const { return this->items; }

	/** Test if the priority queue is empty.
	 *  @return true if empty */
	FORCEINLINE bool IsEmpty() const { return this->items == 0; }

	/** Test if the priority queue is full.
	 *  @return true if full. */
	FORCEINLINE bool IsFull() const { return this->items >= this->capacity; }

	/** Find the smallest item in the priority queue.
	 *  Return the smallest item, or throw assert if empty. */
	FORCEINLINE T *Begin()
	{
		assert(!this->IsEmpty());
		return this->data[1];
	}

	FORCEINLINE T *End()
	{
		return this->data[1 + this->items];
	}

	/** Insert new item into the priority queue, maintaining heap order.
	 *  @return false if the queue is full. */
	FORCEINLINE void Push(T *new_item)
	{
		if (this->IsFull()) {
			this->capacity *= 2;
			this->data = ReallocT<T*>(this->data, this->capacity + 1);
		}

		/* make place for new item */
		uint gap = this->HeapifyUp(++items, new_item);
		this->data[gap] = new_item;
		CHECK_CONSISTY();
	}

	/** Remove and return the smallest item from the priority queue. */
	FORCEINLINE T *Shift()
	{
		assert(!this->IsEmpty());

		T *first = this->Begin();

		this->items--;
		/* at index 1 we have a gap now */
		T *last = this->End();
		uint gap = this->HeapifyDown(1, last);
		/* move last item to the proper place */
		if (!this->IsEmpty()) this->data[gap] = last;

		CHECK_CONSISTY();
		return first;
	}

	/** Remove item specified by index */
	FORCEINLINE void RemoveByIdx(uint index)
	{
		if (index < this->items) {
			assert(index != 0);
			this->items--;
			/* at position index we have a gap now */

			T *last = this->End();
			/* Fix binary tree up and downwards */
			uint gap = this->HeapifyUp(index, last);
			gap = this->HeapifyDown(gap, last);
			/* move last item to the proper place */
			if (!this->IsEmpty()) this->data[gap] = last;
		} else {
			assert(index == this->items);
			this->items--;
		}
		CHECK_CONSISTY();
	}

	/** return index of the item that matches (using &item1 == &item2) the given item. */
	FORCEINLINE uint FindLinear(const T &item) const
	{
		if (this->IsEmpty()) return 0;
		for (T **ppI = this->data + 1, **ppLast = ppI + this->items; ppI <= ppLast; ppI++) {
			if (*ppI == &item) {
				return ppI - this->data;
			}
		}
		return 0;
	}

	/** Make the priority queue empty.
	 * All remaining items will remain untouched. */
	FORCEINLINE void Clear() { this->items = 0; }
};

#endif /* BINARYHEAP_HPP */
