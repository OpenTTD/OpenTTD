/* $Id$ */

/** @file oldpool.cpp */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "oldpool.h"
#include "helpers.hpp"

/**
 * Clean a pool in a safe way (does free all blocks)
 */
void OldMemoryPoolBase::CleanPool()
{
	uint i;

	DEBUG(misc, 4, "[Pool] (%s) cleaning pool..", this->name);

	/* Free all blocks */
	for (i = 0; i < this->current_blocks; i++) {
		if (this->clean_block_proc != NULL) {
			this->clean_block_proc(i * (1 << this->block_size_bits), (i + 1) * (1 << this->block_size_bits) - 1);
		}
		free(this->blocks[i]);
	}

	/* Free the block itself */
	free(this->blocks);

	/* Clear up some critical data */
	this->total_items = 0;
	this->current_blocks = 0;
	this->blocks = NULL;
}

/**
 * This function tries to increase the size of array by adding
 *  1 block too it
 *
 * @return Returns false if the pool could not be increased
 */
bool OldMemoryPoolBase::AddBlockToPool()
{
	/* Is the pool at his max? */
	if (this->max_blocks == this->current_blocks) return false;

	this->total_items = (this->current_blocks + 1) * (1 << this->block_size_bits);

	DEBUG(misc, 4, "[Pool] (%s) increasing size of pool to %d items (%d bytes)", this->name, this->total_items, this->total_items * this->item_size);

	/* Increase the poolsize */
	this->blocks = ReallocT(this->blocks, this->current_blocks + 1);
	if (this->blocks == NULL) error("Pool: (%s) could not allocate memory for blocks", this->name);

	/* Allocate memory to the new block item */
	this->blocks[this->current_blocks] = MallocT<byte>(this->item_size * (1 << this->block_size_bits));
	if (this->blocks[this->current_blocks] == NULL)
		error("Pool: (%s) could not allocate memory for blocks", this->name);

	/* Clean the content of the new block */
	memset(this->blocks[this->current_blocks], 0, this->item_size * (1 << this->block_size_bits));

	/* Call a custom function if defined (e.g. to fill indexes) */
	if (this->new_block_proc != NULL) this->new_block_proc(this->current_blocks * (1 << this->block_size_bits));

	/* We have a new block */
	this->current_blocks++;

	return true;
}

/**
 * Adds blocks to the pool if needed (and possible) till index fits inside the pool
 *
 * @return Returns false if adding failed
 */
bool OldMemoryPoolBase::AddBlockIfNeeded(uint index)
{
	while (index >= this->total_items) {
		if (!this->AddBlockToPool()) return false;
	}

	return true;
}
