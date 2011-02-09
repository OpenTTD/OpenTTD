/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file pool_type.hpp Defintion of Pool, structure used to access PoolItems, and PoolItem, base structure for Vehicle, Town, and other indexed items. */

#ifndef POOL_TYPE_HPP
#define POOL_TYPE_HPP

/**
 * Base class for all pools.
 * @tparam Titem        Type of the class/struct that is going to be pooled
 * @tparam Tindex       Type of the index for this pool
 * @tparam Tgrowth_step Size of growths; if the pool is full increase the size by this amount
 * @tparam Tmax_size    Maximum size of the pool
 * @tparam Tcache       Whether to perform 'alloc' caching, i.e. don't actually free/malloc just reuse the memory
 * @tparam Tzero        Whether to zero the memory
 * @warning when Tcache is enabled *all* instances of this pool's item must be of the same size.
 */
template <class Titem, typename Tindex, size_t Tgrowth_step, size_t Tmax_size, bool Tcache = false, bool Tzero = true>
struct Pool {
	static const size_t MAX_SIZE = Tmax_size; ///< Make template parameter accessible from outside

	const char * const name; ///< Name of this pool

	size_t size;         ///< Current allocated size
	size_t first_free;   ///< No item with index lower than this is free (doesn't say anything about this one!)
	size_t first_unused; ///< This and all higher indexes are free (doesn't say anything about first_unused-1 !)
	size_t items;        ///< Number of used indexes (non-NULL)
#ifdef OTTD_ASSERT
	size_t checked;      ///< Number of items we checked for
#endif /* OTTD_ASSERT */
	bool cleaning;       ///< True if cleaning pool (deleting all items)

	Titem **data;        ///< Pointer to array of pointers to Titem

	Pool(const char *name);
	void CleanPool();

	/**
	 * Returs Titem with given index
	 * @param index of item to get
	 * @return pointer to Titem
	 * @pre index < this->first_unused
	 */
	FORCEINLINE Titem *Get(size_t index)
	{
		assert(index < this->first_unused);
		return this->data[index];
	}

	/**
	 * Tests whether given index can be used to get valid (non-NULL) Titem
	 * @param index index to examine
	 * @return true if PoolItem::Get(index) will return non-NULL pointer
	 */
	FORCEINLINE bool IsValidID(size_t index)
	{
		return index < this->first_unused && this->Get(index) != NULL;
	}

	/**
	 * Tests whether we can allocate 'n' items
	 * @param n number of items we want to allocate
	 * @return true if 'n' items can be allocated
	 */
	FORCEINLINE bool CanAllocate(size_t n = 1)
	{
		bool ret = this->items <= Tmax_size - n;
#ifdef OTTD_ASSERT
		this->checked = ret ? n : 0;
#endif /* OTTD_ASSERT */
		return ret;
	}

	/**
	 * Base class for all PoolItems
	 * @tparam Tpool The pool this item is going to be part of
	 */
	template <struct Pool<Titem, Tindex, Tgrowth_step, Tmax_size, Tcache, Tzero> *Tpool>
	struct PoolItem {
		Tindex index; ///< Index of this pool item

		/**
		 * Allocates space for new Titem
		 * @param size size of Titem
		 * @return pointer to allocated memory
		 * @note can never fail (return NULL), use CanAllocate() to check first!
		 */
		FORCEINLINE void *operator new(size_t size)
		{
			return Tpool->GetNew(size);
		}

		/**
		 * Marks Titem as free. Its memory is released
		 * @param p memory to free
		 * @note the item has to be allocated in the pool!
		 */
		FORCEINLINE void operator delete(void *p)
		{
			Titem *pn = (Titem *)p;
			assert(pn == Tpool->Get(pn->index));
			Tpool->FreeItem(pn->index);
		}

		/**
		 * Allocates space for new Titem with given index
		 * @param size size of Titem
		 * @param index index of item
		 * @return pointer to allocated memory
		 * @note can never fail (return NULL), use CanAllocate() to check first!
		 * @pre index has to be unused! Else it will crash
		 */
		FORCEINLINE void *operator new(size_t size, size_t index)
		{
			return Tpool->GetNew(size, index);
		}

		/**
		 * Allocates space for new Titem at given memory address
		 * @param size size of Titem
		 * @param ptr where are we allocating the item?
		 * @return pointer to allocated memory (== ptr)
		 * @note use of this is strongly discouraged
		 * @pre the memory must not be allocated in the Pool!
		 */
		FORCEINLINE void *operator new(size_t size, void *ptr)
		{
			for (size_t i = 0; i < Tpool->first_unused; i++) {
				/* Don't allow creating new objects over existing.
				 * Even if we called the destructor and reused this memory,
				 * we don't know whether 'size' and size of currently allocated
				 * memory are the same (because of possible inheritance).
				 * Use { size_t index = item->index; delete item; new (index) item; }
				 * instead to make sure destructor is called and no memory leaks. */
				assert(ptr != Tpool->data[i]);
			}
			return ptr;
		}


		/** Helper functions so we can use PoolItem::Function() instead of _poolitem_pool.Function() */

		/**
		 * Tests whether we can allocate 'n' items
		 * @param n number of items we want to allocate
		 * @return true if 'n' items can be allocated
		 */
		static FORCEINLINE bool CanAllocateItem(size_t n = 1)
		{
			return Tpool->CanAllocate(n);
		}

		/**
		 * Returns current state of pool cleaning - yes or no
		 * @return true iff we are cleaning the pool now
		 */
		static FORCEINLINE bool CleaningPool()
		{
			return Tpool->cleaning;
		}

		/**
		 * Tests whether given index can be used to get valid (non-NULL) Titem
		 * @param index index to examine
		 * @return true if PoolItem::Get(index) will return non-NULL pointer
		 */
		static FORCEINLINE bool IsValidID(size_t index)
		{
			return Tpool->IsValidID(index);
		}

		/**
		 * Returs Titem with given index
		 * @param index of item to get
		 * @return pointer to Titem
		 * @pre index < this->first_unused
		 */
		static FORCEINLINE Titem *Get(size_t index)
		{
			return Tpool->Get(index);
		}

		/**
		 * Returs Titem with given index
		 * @param index of item to get
		 * @return pointer to Titem
		 * @note returns NULL for invalid index
		 */
		static FORCEINLINE Titem *GetIfValid(size_t index)
		{
			return index < Tpool->first_unused ? Tpool->Get(index) : NULL;
		}

		/**
		 * Returns first unused index. Useful when iterating over
		 * all pool items.
		 * @return first unused index
		 */
		static FORCEINLINE size_t GetPoolSize()
		{
			return Tpool->first_unused;
		}

		/**
		 * Returns number of valid items in the pool
		 * @return number of valid items in the pool
		 */
		static FORCEINLINE size_t GetNumItems()
		{
			return Tpool->items;
		}

		/**
		 * Dummy function called after destructor of each member.
		 * If you want to use it, override it in PoolItem's subclass.
		 * @param index index of deleted item
		 * @note when this function is called, PoolItem::Get(index) == NULL.
		 * @note it's called only when !CleaningPool()
		 */
		static FORCEINLINE void PostDestructor(size_t index) { }
	};

private:
	static const size_t NO_FREE_ITEM = MAX_UVALUE(size_t); ///< Contant to indicate we can't allocate any more items

	/**
	 * Helper struct to cache 'freed' PoolItems so we
	 * do not need to allocate them again.
	 */
	struct AllocCache {
		/** The next in our 'cache' */
		AllocCache *next;
	};

	/** Cache of freed pointers */
	AllocCache *alloc_cache;

	void *AllocateItem(size_t size, size_t index);
	void ResizeFor(size_t index);
	size_t FindFirstFree();

	void *GetNew(size_t size);
	void *GetNew(size_t size, size_t index);

	void FreeItem(size_t index);
};

#define FOR_ALL_ITEMS_FROM(type, iter, var, start) \
	for (size_t iter = start; var = NULL, iter < type::GetPoolSize(); iter++) \
		if ((var = type::Get(iter)) != NULL)

#define FOR_ALL_ITEMS(type, iter, var) FOR_ALL_ITEMS_FROM(type, iter, var, 0)

#endif /* POOL_TYPE_HPP */
