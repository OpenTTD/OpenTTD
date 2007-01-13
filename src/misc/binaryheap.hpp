/* $Id$ */

#ifndef  BINARYHEAP_HPP
#define  BINARYHEAP_HPP

//void* operator new (size_t size, void* p) {return p;}
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
//void operator delete (void* p, void* p2) {}
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
 * 2) ItemPtr [0] is never used. Total array size is max_items + 1, because we
 *    use indices 1..max_items instead of zero based C indexing.
 *
 * 3) Item of the binary heap should support these public members:
 *    - 'lower-then' operator '<' - used for comparing items before moving
 *
 */

template <class Titem_>
class CBinaryHeapT {
public:
	typedef Titem_ *ItemPtr;
private:
	int                     m_size;     ///< Number of items in the heap
	int                     m_max_size; ///< Maximum number of items the heap can hold
	ItemPtr*                m_items;    ///< The heap item pointers

public:
	explicit CBinaryHeapT(int max_items = 102400)
		: m_size(0)
		, m_max_size(max_items)
	{
		m_items = new ItemPtr[max_items + 1];
	}

	~CBinaryHeapT()
	{
		Clear();
		delete [] m_items;
		m_items = NULL;
	}

public:
	/** Return the number of items stored in the priority queue.
	 *  @return number of items in the queue */
	FORCEINLINE int Size() const {return m_size;};

	/** Test if the priority queue is empty.
	 *  @return true if empty */
	FORCEINLINE bool IsEmpty() const {return (m_size == 0);};

	/** Test if the priority queue is full.
	 *  @return true if full. */
	FORCEINLINE bool IsFull() const {return (m_size >= m_max_size);};

	/** Find the smallest item in the priority queue.
	 *  Return the smallest item, or throw assert if empty. */
	FORCEINLINE Titem_& GetHead() {assert(!IsEmpty()); return *m_items[1];}

	/** Insert new item into the priority queue, maintaining heap order.
	 *  @return false if the queue is full. */
	bool Push(Titem_& new_item);

	/** Remove and return the smallest item from the priority queue. */
	FORCEINLINE Titem_& PopHead() {Titem_& ret = GetHead(); RemoveHead(); return ret;};

	/** Remove the smallest item from the priority queue. */
	void RemoveHead();

	/** Remove item specified by index */
	void RemoveByIdx(int idx);

	/** return index of the item that matches (using &item1 == &item2) the given item. */
	int FindLinear(const Titem_& item) const;

	/** Make the priority queue empty.
	 * All remaining items will remain untouched. */
	void Clear() {m_size = 0;};

	/** verifies the heap consistency (added during first YAPF debug phase) */
	void CheckConsistency();
};


template <class Titem_>
FORCEINLINE bool CBinaryHeapT<Titem_>::Push(Titem_& new_item)
{
	if (IsFull()) return false;

	// make place for new item
	int gap = ++m_size;
	// Heapify up
	for (int parent = gap / 2; (parent > 0) && (new_item < *m_items[parent]); gap = parent, parent /= 2)
		m_items[gap] = m_items[parent];
	m_items[gap] = &new_item;
	CheckConsistency();
	return true;
}

template <class Titem_>
FORCEINLINE void CBinaryHeapT<Titem_>::RemoveHead()
{
	assert(!IsEmpty());

	// at index 1 we have a gap now
	int gap = 1;

	// Heapify down:
	//   last item becomes a candidate for the head. Call it new_item.
	Titem_& new_item = *m_items[m_size--];

	// now we must maintain relation between parent and its children:
	//   parent <= any child
	// from head down to the tail
	int child  = 2; // first child is at [parent * 2]

	// while children are valid
	while (child <= m_size) {
		// choose the smaller child
		if (child < m_size && *m_items[child + 1] < *m_items[child])
			child++;
		// is it smaller than our parent?
		if (!(*m_items[child] < new_item)) {
			// the smaller child is still bigger or same as parent => we are done
			break;
		}
		// if smaller child is smaller than parent, it will become new parent
		m_items[gap] = m_items[child];
		gap = child;
		// where do we have our new children?
		child = gap * 2;
	}
	// move last item to the proper place
	if (m_size > 0) m_items[gap] = &new_item;
	CheckConsistency();
}

template <class Titem_>
inline void CBinaryHeapT<Titem_>::RemoveByIdx(int idx)
{
	// at position idx we have a gap now
	int gap = idx;
	Titem_& last = *m_items[m_size];
	if (idx < m_size) {
		assert(idx >= 1);
		m_size--;
		// and the candidate item for fixing this gap is our last item 'last'
		// Move gap / last item up:
		while (gap > 1)
		{
			// compare [gap] with its parent
			int parent = gap / 2;
			if (last < *m_items[parent]) {
				m_items[gap] = m_items[parent];
				gap = parent;
			} else {
				// we don't need to continue upstairs
				break;
			}
		}

		// Heapify (move gap) down:
		while (true) {
			// where we do have our children?
			int child  = gap * 2; // first child is at [parent * 2]
			if (child > m_size) break;
			// choose the smaller child
			if (child < m_size && *m_items[child + 1] < *m_items[child])
				child++;
			// is it smaller than our parent?
			if (!(*m_items[child] < last)) {
				// the smaller child is still bigger or same as parent => we are done
				break;
			}
			// if smaller child is smaller than parent, it will become new parent
			m_items[gap] = m_items[child];
			gap = child;
		}
		// move parent to the proper place
		if (m_size > 0) m_items[gap] = &last;
	}
	else {
		assert(idx == m_size);
		m_size--;
	}
	CheckConsistency();
}

template <class Titem_>
inline int CBinaryHeapT<Titem_>::FindLinear(const Titem_& item) const
{
	if (IsEmpty()) return 0;
	for (ItemPtr *ppI = m_items + 1, *ppLast = ppI + m_size; ppI <= ppLast; ppI++) {
		if (*ppI == &item) {
			return ppI - m_items;
		}
	}
	return 0;
}

template <class Titem_>
FORCEINLINE void CBinaryHeapT<Titem_>::CheckConsistency()
{
	// enable it if you suspect binary heap doesn't work well
#if 0
	for (int child = 2; child <= m_size; child++) {
		int parent = child / 2;
		assert(!(m_items[child] < m_items[parent]));
	}
#endif
}


#endif /* BINARYHEAP_HPP */
