/* $Id$ */

/** @file oldpool.h */

#ifndef OLDPOOL_H
#define OLDPOOL_H

/* The function that is called after a new block is added
     start_item is the first item of the new made block */
typedef void OldMemoryPoolNewBlock(uint start_item);
/* The function that is called before a block is cleaned up */
typedef void OldMemoryPoolCleanBlock(uint start_item, uint end_item);

/**
 * Stuff for dynamic vehicles. Use the wrappers to access the OldMemoryPool
 *  please try to avoid manual calls!
 */
struct OldMemoryPoolBase {
	void CleanPool();
	bool AddBlockToPool();
	bool AddBlockIfNeeded(uint index);

protected:
	OldMemoryPoolBase(const char *name, uint max_blocks, uint block_size_bits, uint item_size,
				OldMemoryPoolNewBlock *new_block_proc, OldMemoryPoolCleanBlock *clean_block_proc) :
		name(name), max_blocks(max_blocks), block_size_bits(block_size_bits), item_size(item_size),
		new_block_proc(new_block_proc), clean_block_proc(clean_block_proc), current_blocks(0),
		total_items(0), blocks(NULL) {}

	const char* name;     ///< Name of the pool (just for debugging)

	const uint max_blocks;      ///< The max amount of blocks this pool can have
	const uint block_size_bits; ///< The size of each block in bits
	const uint item_size;       ///< How many bytes one block is

	/// Pointer to a function that is called after a new block is added
	OldMemoryPoolNewBlock *new_block_proc;
	/// Pointer to a function that is called to clean a block
	OldMemoryPoolCleanBlock *clean_block_proc;

	uint current_blocks;        ///< How many blocks we have in our pool
	uint total_items;           ///< How many items we now have in this pool

public:
	uint first_free_index;      ///< The index of the first free pool item in this pool
	byte **blocks;              ///< An array of blocks (one block hold all the items)

	/**
	 * Get the size of this pool, i.e. the total number of items you
	 * can put into it at the current moment; the pool might still
	 * be able to increase the size of the pool.
	 * @return the size of the pool
	 */
	inline uint GetSize() const
	{
		return this->total_items;
	}

	/**
	 * Can this pool allocate more blocks, i.e. is the maximum amount
	 * of allocated blocks not yet reached?
	 * @return the if and only if the amount of allocable blocks is
	 *         less than the amount of allocated blocks.
	 */
	inline bool CanAllocateMoreBlocks() const
	{
		return this->current_blocks < this->max_blocks;
	}

	/**
	 * Get the maximum number of allocable blocks.
	 * @return the numebr of blocks
	 */
	inline uint GetBlockCount() const
	{
		return this->current_blocks;
	}

	/**
	 * Get the name of this pool.
	 * @return the name
	 */
	inline const char *GetName() const
	{
		return this->name;
	}
};

template <typename T>
struct OldMemoryPool : public OldMemoryPoolBase {
	OldMemoryPool(const char *name, uint max_blocks, uint block_size_bits, uint item_size,
				OldMemoryPoolNewBlock *new_block_proc, OldMemoryPoolCleanBlock *clean_block_proc) :
		OldMemoryPoolBase(name, max_blocks, block_size_bits, item_size, new_block_proc, clean_block_proc) {}

	/**
	 * Get the pool entry at the given index.
	 * @param index the index into the pool
	 * @pre index < this->GetSize()
	 * @return the pool entry.
	 */
	inline T *Get(uint index) const
	{
		assert(index < this->GetSize());
		return (T*)(this->blocks[index >> this->block_size_bits] +
				(index & ((1 << this->block_size_bits) - 1)) * sizeof(T));
	}
};

/**
 * Those are the wrappers:
 *   CleanPool cleans the pool up, but you can use AddBlockToPool directly again
 *     (no need to call CreatePool!)
 *   AddBlockToPool adds 1 more block to the pool. Returns false if there is no
 *     more room
 */
static inline void CleanPool(OldMemoryPoolBase *array) { array->CleanPool(); }
static inline bool AddBlockToPool(OldMemoryPoolBase *array) { return array->AddBlockToPool(); }

/**
 * Adds blocks to the pool if needed (and possible) till index fits inside the pool
 *
 * @return Returns false if adding failed
 */
static inline bool AddBlockIfNeeded(OldMemoryPoolBase *array, uint index) { return array->AddBlockIfNeeded(index); }


/**
 * Generic function to initialize a new block in a pool.
 * @param start_item the first item that needs to be initialized
 */
template <typename T, OldMemoryPool<T> *Tpool>
static void PoolNewBlock(uint start_item)
{
	for (T *t = Tpool->Get(start_item); t != NULL; t = (t->index + 1U < Tpool->GetSize()) ? Tpool->Get(t->index + 1U) : NULL) {
		t = new (t) T();
		t->index = start_item++;
	}
}

/**
 * Generic function to free a new block in a pool.
 * This function uses QuickFree that is intended to only free memory that would be lost if the pool is freed.
 * @param start_item the first item that needs to be cleaned
 * @param end_item   the last item that needs to be cleaned
 */
template <typename T, OldMemoryPool<T> *Tpool>
static void PoolCleanBlock(uint start_item, uint end_item)
{
	for (uint i = start_item; i <= end_item; i++) {
		T *t = Tpool->Get(i);
		if (t->IsValid()) {
			t->QuickFree();
		}
	}
}


/**
 * Generalization for all pool items that are saved in the savegame.
 * It specifies all the mechanics to access the pool easily.
 */
template <typename T, typename Tid, OldMemoryPool<T> *Tpool>
struct PoolItem {
	/**
	 * The pool-wide index of this object.
	 */
	Tid index;

	/**
	 * We like to have the correct class destructed.
	 */
	virtual ~PoolItem()
	{
		if (this->index < Tpool->first_free_index) Tpool->first_free_index = this->index;
	}

	/**
	 * Called on each object when the pool is being destroyed, so one
	 * can free allocated memory without the need for freeing for
	 * example orders.
	 */
	virtual void QuickFree()
	{
	}

	/**
	 * An overriden version of new that allocates memory on the pool.
	 * @param size the size of the variable (unused)
	 * @return the memory that is 'allocated'
	 */
	void *operator new(size_t size)
	{
		return AllocateRaw();
	}

	/**
	 * 'Free' the memory allocated by the overriden new.
	 * @param p the memory to 'free'
	 */
	void operator delete(void *p)
	{
	}

	/**
	 * An overriden version of new, so you can directly allocate a new object with
	 * the correct index when one is loading the savegame.
	 * @param size  the size of the variable (unused)
	 * @param index the index of the object
	 * @return the memory that is 'allocated'
	 */
	void *operator new(size_t size, int index)
	{
		if (!Tpool->AddBlockIfNeeded(index)) error("%s: failed loading savegame: too many %s", Tpool->GetName(), Tpool->GetName());

		return Tpool->Get(index);
	}

	/**
	 * 'Free' the memory allocated by the overriden new.
	 * @param p     the memory to 'free'
	 * @param index the original parameter given to create the memory
	 */
	void operator delete(void *p, int index)
	{
	}

	/**
	 * An overriden version of new, so you can use the vehicle instance
	 * instead of a newly allocated piece of memory.
	 * @param size the size of the variable (unused)
	 * @param pn   the already existing object to use as 'storage' backend
	 * @return the memory that is 'allocated'
	 */
	void *operator new(size_t size, T *pn)
	{
		return pn;
	}

	/**
	 * 'Free' the memory allocated by the overriden new.
	 * @param p  the memory to 'free'
	 * @param pn the pointer that was given to 'new' on creation.
	 */
	void operator delete(void *p, T *pn)
	{
	}

	/**
	 * Is this a valid object or not?
	 * @return true if and only if it is valid
	 */
	virtual bool IsValid() const
	{
		return false;
	}

private:
	/**
	 * Allocate a pool item; possibly allocate a new block in the pool.
	 * @return the allocated pool item (or NULL when the pool is full).
	 */
	static T *AllocateRaw()
	{
		for (T *t = Tpool->Get(Tpool->first_free_index); t != NULL; t = (t->index + 1U < Tpool->GetSize()) ? Tpool->Get(t->index + 1U) : NULL) {
			if (!t->IsValid()) {
				Tpool->first_free_index = t->index;
				Tid index = t->index;

				memset(t, 0, sizeof(T));
				t->index = index;
				return t;
			}
		}

		/* Check if we can add a block to the pool */
		if (Tpool->AddBlockToPool()) return AllocateRaw();

		return NULL;
	}
};


#define OLD_POOL_ENUM(name, type, block_size_bits, max_blocks) \
	enum { \
		name##_POOL_BLOCK_SIZE_BITS = block_size_bits, \
		name##_POOL_MAX_BLOCKS      = max_blocks \
	};


#define OLD_POOL_ACCESSORS(name, type) \
	static inline type* Get##name(uint index) { return _##name##_pool.Get(index);  } \
	static inline uint Get##name##PoolSize()  { return _##name##_pool.GetSize(); }


#define DECLARE_OLD_POOL(name, type, block_size_bits, max_blocks) \
	OLD_POOL_ENUM(name, type, block_size_bits, max_blocks) \
	extern OldMemoryPool<type> _##name##_pool; \
	OLD_POOL_ACCESSORS(name, type)


#define DEFINE_OLD_POOL(name, type, new_block_proc, clean_block_proc) \
	OldMemoryPool<type> _##name##_pool( \
		#name, name##_POOL_MAX_BLOCKS, name##_POOL_BLOCK_SIZE_BITS, sizeof(type), \
		new_block_proc, clean_block_proc);

#define DEFINE_OLD_POOL_GENERIC(name, type) \
	OldMemoryPool<type> _##name##_pool( \
		#name, name##_POOL_MAX_BLOCKS, name##_POOL_BLOCK_SIZE_BITS, sizeof(type), \
		PoolNewBlock<type, &_##name##_pool>, PoolCleanBlock<type, &_##name##_pool>);


#define STATIC_OLD_POOL(name, type, block_size_bits, max_blocks, new_block_proc, clean_block_proc) \
	OLD_POOL_ENUM(name, type, block_size_bits, max_blocks) \
	static DEFINE_OLD_POOL(name, type, new_block_proc, clean_block_proc) \
	OLD_POOL_ACCESSORS(name, type)

#endif /* OLDPOOL_H */
