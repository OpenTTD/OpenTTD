/* $Id$ */

#ifndef OLDPOOL_FUNC_H

#include "oldpool.h"

/**
 * Allocate a pool item; possibly allocate a new block in the pool.
 * @param first the first pool item to start searching
 * @pre first <= Tpool->GetSize()
 * @return the allocated pool item (or NULL when the pool is full).
 */
template<typename T, typename Tid, OldMemoryPool<T> *Tpool> T *PoolItem<T, Tid, Tpool>::AllocateSafeRaw(uint &first)
{
	uint last_minus_one = Tpool->GetSize() - 1;

	for (T *t = Tpool->Get(first); t != NULL; t = (t->index < last_minus_one) ? Tpool->Get(t->index + 1U) : NULL) {
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

	return NULL;
}

#endif /* OLDPOOL_FUNC_H */
