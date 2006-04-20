/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "pool.h"
#include "sprite.h"

enum {
	SPRITEGROUP_POOL_BLOCK_SIZE_BITS = 4, /* (1 << 4) == 16 items */
	SPRITEGROUP_POOL_MAX_BLOCKS      = 8000,
};

static uint _spritegroup_count = 0;
static MemoryPool _spritegroup_pool;


static void SpriteGroupPoolCleanBlock(uint start_item, uint end_item)
{
	uint i;

	for (i = start_item; i <= end_item; i++) {
		SpriteGroup *group = (SpriteGroup*)GetItemFromPool(&_spritegroup_pool, i);

		/* Free dynamically allocated memory */
		switch (group->type) {
			case SGT_REAL:
				free(group->g.real.loaded);
				free(group->g.real.loading);
				break;

			case SGT_DETERMINISTIC:
				free(group->g.determ.ranges);
				break;

			case SGT_RANDOMIZED:
				free(group->g.random.groups);
				break;

			default:
				break;
		}
	}
}


/* Initialize the SpriteGroup pool */
static MemoryPool _spritegroup_pool = { "SpriteGr", SPRITEGROUP_POOL_MAX_BLOCKS, SPRITEGROUP_POOL_BLOCK_SIZE_BITS, sizeof(SpriteGroup), NULL, &SpriteGroupPoolCleanBlock, 0, 0, NULL };


/* Allocate a new SpriteGroup */
SpriteGroup *AllocateSpriteGroup(void)
{
	/* This is totally different to the other pool allocators, as we never remove an item from the pool. */
	if (_spritegroup_count == _spritegroup_pool.total_items) {
		if (!AddBlockToPool(&_spritegroup_pool)) return NULL;
	}

	return (SpriteGroup*)GetItemFromPool(&_spritegroup_pool, _spritegroup_count++);
}


void InitializeSpriteGroupPool(void)
{
	CleanPool(&_spritegroup_pool);
	AddBlockToPool(&_spritegroup_pool);

	_spritegroup_count = 0;
}
