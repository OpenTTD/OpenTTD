/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "oldpool.h"
#include "helpers.hpp"

/**
 * Clean a pool in a safe way (does free all blocks)
 */
void CleanPool(OldMemoryPool *pool)
{
	uint i;

	DEBUG(misc, 4, "[Pool] (%s) cleaning pool..", pool->name);

	/* Free all blocks */
	for (i = 0; i < pool->current_blocks; i++) {
		if (pool->clean_block_proc != NULL) {
			pool->clean_block_proc(i * (1 << pool->block_size_bits), (i + 1) * (1 << pool->block_size_bits) - 1);
		}
		free(pool->blocks[i]);
	}

	/* Free the block itself */
	free(pool->blocks);

	/* Clear up some critical data */
	pool->total_items = 0;
	pool->current_blocks = 0;
	pool->blocks = NULL;
}

/**
 * This function tries to increase the size of array by adding
 *  1 block too it
 *
 * @return Returns false if the pool could not be increased
 */
bool AddBlockToPool(OldMemoryPool *pool)
{
	/* Is the pool at his max? */
	if (pool->max_blocks == pool->current_blocks)
		return false;

	pool->total_items = (pool->current_blocks + 1) * (1 << pool->block_size_bits);

	DEBUG(misc, 4, "[Pool] (%s) increasing size of pool to %d items (%d bytes)", pool->name, pool->total_items, pool->total_items * pool->item_size);

	/* Increase the poolsize */
	ReallocT(&pool->blocks, pool->current_blocks + 1);
	if (pool->blocks == NULL) error("Pool: (%s) could not allocate memory for blocks", pool->name);

	/* Allocate memory to the new block item */
	MallocT(&pool->blocks[pool->current_blocks], pool->item_size * (1 << pool->block_size_bits));
	if (pool->blocks[pool->current_blocks] == NULL)
		error("Pool: (%s) could not allocate memory for blocks", pool->name);

	/* Clean the content of the new block */
	memset(pool->blocks[pool->current_blocks], 0, pool->item_size * (1 << pool->block_size_bits));

	/* Call a custom function if defined (e.g. to fill indexes) */
	if (pool->new_block_proc != NULL)
		pool->new_block_proc(pool->current_blocks * (1 << pool->block_size_bits));

	/* We have a new block */
	pool->current_blocks++;

	return true;
}

/**
 * Adds blocks to the pool if needed (and possible) till index fits inside the pool
 *
 * @return Returns false if adding failed
 */
bool AddBlockIfNeeded(OldMemoryPool *pool, uint index)
{
	while (index >= pool->total_items) {
		if (!AddBlockToPool(pool))
			return false;
	}

	return true;
}
