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
		data = MallocT<T*>(max_items + 1);
	}

	~CBinaryHeapT()
	{
		Clear();
		free(data);
		data = NULL;
	}

protected:
	/** Heapify (move gap) down */
	FORCEINLINE uint HeapifyDown(uint gap, T *item)
	{
		assert(gap != 0);

		uint child = gap * 2; // first child is at [parent * 2]

		/* while children are valid */
		while (child <= items) {
			/* choose the smaller child */
			if (child < items && *data[child + 1] < *data[child])
				child++;
			/* is it smaller than our parent? */
			if (!(*data[child] < *item)) {
				/* the smaller child is still bigger or same as parent => we are done */
				break;
			}
			/* if smaller child is smaller than parent, it will become new parent */
			data[gap] = data[child];
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
			if (!(*item <*data[parent])) {
				/* we don't need to continue upstairs */
				break;
			}
			data[gap] = data[parent];
			gap = parent;
		}
		return gap;
	}

public:
	/** Return the number of items stored in the priority queue.
	 *  @return number of items in the queue */
	FORCEINLINE uint Size() const { return items; }

	/** Test if the priority queue is empty.
	 *  @return true if empty */
	FORCEINLINE bool IsEmpty() const { return items == 0; }

	/** Test if the priority queue is full.
	 *  @return true if full. */
	FORCEINLINE bool IsFull() const { return items >= capacity; }

	/** Find the smallest item in the priority queue.
	 *  Return the smallest item, or throw assert if empty. */
	FORCEINLINE T *Begin()
	{
		assert(!IsEmpty());
		return data[1];
	}

	FORCEINLINE T *End()
	{
		return data[1 + items];
	}

	/** Insert new item into the priority queue, maintaining heap order.
	 *  @return false if the queue is full. */
	FORCEINLINE void Push(T *new_item)
	{
		if (IsFull()) {
			capacity *= 2;
			data = ReallocT<T*>(data, capacity + 1);
		}

		/* make place for new item */
		uint gap = HeapifyUp(++items, new_item);
		data[gap] = new_item;
		CheckConsistency();
	}

	/** Remove and return the smallest item from the priority queue. */
	FORCEINLINE T *Shift()
	{
		assert(!IsEmpty());

		T *first = Begin();

		items--;
		/* at index 1 we have a gap now */
		T *last = End();
		uint gap = HeapifyDown(1, last);
		/* move last item to the proper place */
		if (!IsEmpty()) data[gap] = last;

		CheckConsistency();
		return first;
	}

	/** Remove item specified by index */
	FORCEINLINE void RemoveByIdx(uint index)
	{
		if (index < items) {
			assert(index != 0);
			items--;
			/* at position index we have a gap now */

			T *last = End();
			/* Fix binary tree up and downwards */
			uint gap = HeapifyUp(index, last);
			gap = HeapifyDown(gap, last);
			/* move last item to the proper place */
			if (!IsEmpty()) data[gap] = last;
		} else {
			assert(index == items);
			items--;
		}
		CheckConsistency();
	}

	/** return index of the item that matches (using &item1 == &item2) the given item. */
	FORCEINLINE uint FindLinear(const T& item) const
	{
		if (IsEmpty()) return 0;
		for (T **ppI = data + 1, **ppLast = ppI + items; ppI <= ppLast; ppI++) {
			if (*ppI == &item) {
				return ppI - data;
			}
		}
		return 0;
	}

	/** Make the priority queue empty.
	 * All remaining items will remain untouched. */
	FORCEINLINE void Clear() { items = 0; }

	/** verifies the heap consistency (added during first YAPF debug phase) */
	FORCEINLINE void CheckConsistency()
	{
		/* enable it if you suspect binary heap doesn't work well */
#if 0
		for (uint child = 2; child <= items; child++) {
			uint parent = child / 2;
			assert(!(*data[child] < *data[parent]));
		}
#endif
	}
};

#endif /* BINARYHEAP_HPP */
