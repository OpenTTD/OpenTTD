/* $Id$ */

#ifndef OLDPOOL_H
#define OLDPOOL_H

typedef struct OldMemoryPool OldMemoryPool;

/* The function that is called after a new block is added
     start_item is the first item of the new made block */
typedef void OldMemoryPoolNewBlock(uint start_item);
/* The function that is called before a block is cleaned up */
typedef void OldMemoryPoolCleanBlock(uint start_item, uint end_item);

/**
 * Stuff for dynamic vehicles. Use the wrappers to access the OldMemoryPool
 *  please try to avoid manual calls!
 */
struct OldMemoryPool {
	const char* const name;     ///< Name of the pool (just for debugging)

	const uint max_blocks;      ///< The max amount of blocks this pool can have
	const uint block_size_bits; ///< The size of each block in bits
	const uint item_size;       ///< How many bytes one block is

	/// Pointer to a function that is called after a new block is added
	OldMemoryPoolNewBlock *new_block_proc;
	/// Pointer to a function that is called to clean a block
	OldMemoryPoolCleanBlock *clean_block_proc;

	uint current_blocks;        ///< How many blocks we have in our pool
	uint total_items;           ///< How many items we now have in this pool

	byte **blocks;              ///< An array of blocks (one block hold all the items)
};

/**
 * Those are the wrappers:
 *   CleanPool cleans the pool up, but you can use AddBlockToPool directly again
 *     (no need to call CreatePool!)
 *   AddBlockToPool adds 1 more block to the pool. Returns false if there is no
 *     more room
 */
void CleanPool(OldMemoryPool *array);
bool AddBlockToPool(OldMemoryPool *array);

/**
 * Adds blocks to the pool if needed (and possible) till index fits inside the pool
 *
 * @return Returns false if adding failed
 */
bool AddBlockIfNeeded(OldMemoryPool *array, uint index);


#define OLD_POOL_ENUM(name, type, block_size_bits, max_blocks) \
	enum { \
		name##_POOL_BLOCK_SIZE_BITS = block_size_bits, \
		name##_POOL_MAX_BLOCKS      = max_blocks \
	};


#define OLD_POOL_ACCESSORS(name, type) \
	static inline type* Get##name(uint index) \
	{ \
		assert(index < _##name##_pool.total_items); \
		return (type*)( \
			_##name##_pool.blocks[index >> name##_POOL_BLOCK_SIZE_BITS] + \
			(index & ((1 << name##_POOL_BLOCK_SIZE_BITS) - 1)) * sizeof(type) \
		); \
	} \
\
	static inline uint Get##name##PoolSize(void) \
	{ \
		return _##name##_pool.total_items; \
	}


#define DECLARE_OLD_POOL(name, type, block_size_bits, max_blocks) \
	OLD_POOL_ENUM(name, type, block_size_bits, max_blocks) \
	extern OldMemoryPool _##name##_pool; \
	OLD_POOL_ACCESSORS(name, type)


#define DEFINE_OLD_POOL(name, type, new_block_proc, clean_block_proc) \
	OldMemoryPool _##name##_pool = { \
		#name, name##_POOL_MAX_BLOCKS, name##_POOL_BLOCK_SIZE_BITS, sizeof(type), \
		new_block_proc, clean_block_proc, \
		0, 0, NULL \
	};


#define STATIC_OLD_POOL(name, type, block_size_bits, max_blocks, new_block_proc, clean_block_proc) \
	OLD_POOL_ENUM(name, type, block_size_bits, max_blocks) \
	static DEFINE_OLD_POOL(name, type, new_block_proc, clean_block_proc) \
	OLD_POOL_ACCESSORS(name, type)

#endif /* OLDPOOL_H */
