/* $Id$ */

#ifndef POOL_H
#define POOL_H

typedef struct MemoryPool MemoryPool;

/* The function that is called after a new block is added
     start_item is the first item of the new made block */
typedef void MemoryPoolNewBlock(uint start_item);

/**
 * Stuff for dynamic vehicles. Use the wrappers to access the MemoryPool
 *  please try to avoid manual calls!
 */
struct MemoryPool {
	const char name[10];        //! Name of the pool (just for debugging)

	const uint max_blocks;      //! The max amount of blocks this pool can have
	const uint block_size_bits; //! The size of each block in bits
	const uint item_size;       //! How many bytes one block is

	MemoryPoolNewBlock *new_block_proc;
	//!< Pointer to a function that is called after a new block is added

	uint current_blocks;        //! How many blocks we have in our pool
	uint total_items;           //! How many items we now have in this pool

	byte **blocks;              //! An array of blocks (one block hold all the items)
};

/**
 * Those are the wrappers:
 *   CleanPool cleans the pool up, but you can use AddBlockToPool directly again
 *     (no need to call CreatePool!)
 *   AddBlockToPool adds 1 more block to the pool. Returns false if there is no
 *     more room
 */
void CleanPool(MemoryPool *array);
bool AddBlockToPool(MemoryPool *array);

/**
 * Adds blocks to the pool if needed (and possible) till index fits inside the pool
 *
 * @return Returns false if adding failed
 */
bool AddBlockIfNeeded(MemoryPool *array, uint index);

static inline byte *GetItemFromPool(MemoryPool *pool, uint index)
{
	assert(index < pool->total_items);
	return (pool->blocks[index >> pool->block_size_bits] + (index & ((1 << pool->block_size_bits) - 1)) * pool->item_size);
}

#endif /* POOL_H */
