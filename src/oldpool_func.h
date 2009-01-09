/* $Id$ */

/** @file oldpool_func.h Functions related to the old pool. */

#ifndef OLDPOOL_FUNC_H
#define OLDPOOL_FUNC_H

#include "oldpool.h"

/**
 * Allocate a pool item; possibly allocate a new block in the pool.
 * @param first the first pool item to start searching
 * @pre first <= Tpool->GetSize()
 * @pre CanAllocateItem()
 * @return the allocated pool item
 */
template<typename T, typename Tid, OldMemoryPool<T> *Tpool> T *PoolItem<T, Tid, Tpool>::AllocateSafeRaw(uint &first)
{
	uint last_minus_one = Tpool->GetSize() - 1;

	for (T *t = Tpool->Get(first); t != NULL; t = ((uint)t->index < last_minus_one) ? Tpool->Get(t->index + 1U) : NULL) {
		if (!t->IsValid()) {
			first = t->index;
			Tid index = t->index;

			memset(t, 0, Tpool->item_size);
			t->index = index;
			return t;
		}
	}

	/* Check if we can add a block to the pool */
	if (Tpool->AddBlockToPool()) return AllocateRaw(first);

	/* One should *ALWAYS* be sure to have enough space before making vehicles! */
	NOT_REACHED();
}

/**
 * Check whether we can allocate an item in this pool. This to prevent the
 * need to actually construct the object and then destructing it again,
 * which could be *very* costly.
 * @param count the number of items to create
 * @return true if and only if at least count items can be allocated.
 */
template<typename T, typename Tid, OldMemoryPool<T> *Tpool> bool PoolItem<T, Tid, Tpool>::CanAllocateItem(uint count)
{
	uint last_minus_one = Tpool->GetSize() - 1;
	uint orig_count = count;

	for (T *t = Tpool->Get(Tpool->first_free_index); count > 0 && t != NULL; t = ((uint)t->index < last_minus_one) ? Tpool->Get(t->index + 1U) : NULL) {
		if (!t->IsValid()) count--;
	}

	if (count == 0) return true;

	/* Check if we can add a block to the pool */
	if (Tpool->AddBlockToPool()) return CanAllocateItem(orig_count);

	return false;
}

#endif /* OLDPOOL_FUNC_H */
