/* $Id$ */

/** @file pool_type.hpp Defintion of Pool, structure used to access PoolItems, and PoolItem, base structure for Vehicle, Town, and other indexed items. */

#ifndef POOL_TYPE_HPP
#define POOL_TYPE_HPP

template <class Titem, typename Tindex, size_t Tgrowth_step, size_t Tmax_size>
struct Pool {
	static const size_t MAX_SIZE = Tmax_size; ///< Make template parameter accessible from outside

	const char * const name; ///< Name of this pool

	size_t size;         ///< Current allocated size
	size_t first_free;   ///< No item with index lower than this is free (doesn't say anything about this one!)
	size_t first_unused; ///< This and all higher indexes are free (doesn't say anything about first_unused-1 !)
	size_t items;        ///< Number of used indexes (non-NULL)

	bool cleaning;       ///< True if cleaning pool (deleting all items)

	Titem **data;        ///< Pointer to array of pointers to Titem

	/** Constructor */
	Pool(const char *name);
	/** Destroys all items in the pool and resets all member variables */
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
		return this->items <= Tmax_size - n;
	}

	/** Base class for all PoolItems */
	template <struct Pool<Titem, Tindex, Tgrowth_step, Tmax_size> *Tpool>
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
		 * Deletes item with given index.
		 * @param p Titem memory to release
		 * @param index index of item
		 * @note never use this! Only for internal use
		 *       (called automatically when constructor of Titem uses throw())
		 */
		FORCEINLINE void operator delete(void *p, size_t index)
		{
			assert(p == Tpool->Get(index));
			Tpool->FreeItem(index);
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

		/**
		 * Deletes item that was allocated by the function above
		 * @param p Titem memory to release
		 * @param ptr parameter given to operator new
		 * @note never use this! Only for internal use
		 *       (called automatically when constructor of Titem uses throw())
		 */
		FORCEINLINE void operator delete(void *p, void *ptr)
		{
			assert(p == ptr);
			for (size_t i = 0; i < Tpool->first_unused; i++) {
				assert(ptr != Tpool->data[i]);
			}
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
	};

private:
	static const size_t NO_FREE_ITEM = MAX_UVALUE(size_t); ///< Contant to indicate we can't allocate any more items

	/**
	 * Makes given index valid
	 * @param size size of item
	 * @param index index of item
	 * @pre index < this->size
	 * @pre this->Get(index) == NULL
	 */
	void *AllocateItem(size_t size, size_t index);

	/**
	 * Resizes the pool so 'index' can be addressed
	 * @param index index we will allocate later
	 * @pre index >= this->size
	 * @pre index < Tmax_size
	 */
	void ResizeFor(size_t index);

	/**
	 * Searches for first free index
	 * @return first free index, NO_FREE_ITEM on failure
	 */
	size_t FindFirstFree();

	/**
	 * Allocates new item
	 * @param size size of item
	 * @return pointer to allocated item
	 * @note error() on failure! (no free item)
	 */
	void *GetNew(size_t size);

	/**
	 * Allocates new item with given index
	 * @param size size of item
	 * @param index index of item
	 * @return pointer to allocated item
	 * @note usererror() on failure! (index out of range or already used)
	 */
	void *GetNew(size_t size, size_t index);

	/**
	 * Deallocates memory used by this index and marks item as free
	 * @param index item to deallocate
	 * @pre unit is allocated (non-NULL)
	 * @note 'delete NULL' doesn't cause call of this function, so it is safe
	 */
	void FreeItem(size_t index);
};

#define FOR_ALL_ITEMS_FROM(type, iter, var, start) \
	for (size_t iter = start; var = NULL, iter < type::GetPoolSize(); iter++) \
		if ((var = type::Get(iter)) != NULL)

#define FOR_ALL_ITEMS(type, iter, var) FOR_ALL_ITEMS_FROM(type, iter, var, 0)

#endif /* POOL_TYPE_HPP */
