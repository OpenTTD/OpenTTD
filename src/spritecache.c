/* $Id$ */

#include "stdafx.h"
#include "openttd.h"
#include "debug.h"
#include "functions.h"
#include "macros.h"
#include "spritecache.h"
#include "table/sprites.h"
#include "fileio.h"

#define SPRITE_CACHE_SIZE 1024*1024


static void* _sprite_ptr[MAX_SPRITES];
static uint32 _sprite_file_pos[MAX_SPRITES];
static int16 _sprite_lru_new[MAX_SPRITES];

typedef struct MemBlock {
	uint32 size;
	byte data[VARARRAY_SIZE];
} MemBlock;

static uint _sprite_lru_counter;
static MemBlock *_spritecache_ptr;
static int _compact_cache_counter;

static void CompactSpriteCache(void);

static bool ReadSpriteHeaderSkipData(void)
{
	uint16 num = FioReadWord();
	byte type;

	if (num == 0) return false;

	type = FioReadByte();
	if (type == 0xFF) {
		FioSkipBytes(num);
		/* Some NewGRF files have "empty" pseudo-sprites which are 1
		 * byte long. Catch these so the sprites won't be displayed. */
		return num != 1;
	}

	FioSkipBytes(7);
	num -= 8;
	if (num == 0) return true;

	if (type & 2) {
		FioSkipBytes(num);
	} else {
		while (num > 0) {
			int8 i = FioReadByte();
			if (i >= 0) {
				num -= i;
				FioSkipBytes(i);
			} else {
				i = -(i >> 3);
				num -= i;
				FioReadByte();
			}
		}
	}

	return true;
}

/* Check if the given Sprite ID exists */
bool SpriteExists(SpriteID id)
{
	/* Special case for Sprite ID zero -- its position is also 0... */
	return _sprite_file_pos[id] != 0 || id == 0;
}

static void* AllocSprite(size_t);

static void* ReadSprite(SpriteID id)
{
	uint num;
	byte type;

	DEBUG(sprite, 9, "Load sprite %d", id);

	if (!SpriteExists(id)) {
		error(
			"Tried to load non-existing sprite #%d.\n"
			"Probable cause: Wrong/missing NewGRFs",
			id
		);
	}

	FioSeekToFile(_sprite_file_pos[id]);

	num  = FioReadWord();
	type = FioReadByte();
	if (type == 0xFF) {
		byte* dest = AllocSprite(num);

		_sprite_ptr[id] = dest;
		FioReadBlock(dest, num);

		return dest;
	} else {
		uint height = FioReadByte();
		uint width  = FioReadWord();
		Sprite* sprite;
		byte* dest;

		num = (type & 0x02) ? width * height : num - 8;
		sprite = AllocSprite(sizeof(*sprite) + num);
		_sprite_ptr[id] = sprite;
		sprite->info   = type;
		sprite->height = (id != 142) ? height : 10; // Compensate for a TTD bug
		sprite->width  = width;
		sprite->x_offs = FioReadWord();
		sprite->y_offs = FioReadWord();

		dest = sprite->data;
		while (num > 0) {
			int8 i = FioReadByte();

			if (i >= 0) {
				num -= i;
				for (; i > 0; --i) *dest++ = FioReadByte();
			} else {
				const byte* rel = dest - (((i & 7) << 8) | FioReadByte());

				i = -(i >> 3);
				num -= i;

				for (; i > 0; --i) *dest++ = *rel++;
			}
		}

		return sprite;
	}
}


bool LoadNextSprite(int load_index, byte file_index)
{
	uint32 file_pos = FioGetPos() | (file_index << 24);

	if (!ReadSpriteHeaderSkipData()) return false;

	if (load_index >= MAX_SPRITES) {
		error("Tried to load too many sprites (#%d; max %d)", load_index, MAX_SPRITES);
	}

	_sprite_file_pos[load_index] = file_pos;

	_sprite_ptr[load_index] = NULL;

	_sprite_lru_new[load_index] = 0;

	return true;
}


void DupSprite(SpriteID old, SpriteID new)
{
	_sprite_file_pos[new] = _sprite_file_pos[old];
	_sprite_ptr[new] = NULL;
}


void SkipSprites(uint count)
{
	for (; count > 0; --count) {
		if (!ReadSpriteHeaderSkipData()) return;
	}
}


#define S_FREE_MASK 1

static inline MemBlock* NextBlock(MemBlock* block)
{
	return (MemBlock*)((byte*)block + (block->size & ~S_FREE_MASK));
}

static uint32 GetSpriteCacheUsage(void)
{
	uint32 tot_size = 0;
	MemBlock* s;

	for (s = _spritecache_ptr; s->size != 0; s = NextBlock(s))
		if (!(s->size & S_FREE_MASK)) tot_size += s->size;

	return tot_size;
}


void IncreaseSpriteLRU(void)
{
	int i;

	// Increase all LRU values
	if (_sprite_lru_counter > 16384) {
		DEBUG(sprite, 3, "Fixing lru %d, inuse=%d", _sprite_lru_counter, GetSpriteCacheUsage());

		for (i = 0; i != MAX_SPRITES; i++)
			if (_sprite_ptr[i] != NULL) {
				if (_sprite_lru_new[i] >= 0) {
					_sprite_lru_new[i] = -1;
				} else if (_sprite_lru_new[i] != -32768) {
					_sprite_lru_new[i]--;
				}
			}
		_sprite_lru_counter = 0;
	}

	// Compact sprite cache every now and then.
	if (++_compact_cache_counter >= 740) {
		CompactSpriteCache();
		_compact_cache_counter = 0;
	}
}

// Called when holes in the sprite cache should be removed.
// That is accomplished by moving the cached data.
static void CompactSpriteCache(void)
{
	MemBlock *s;

	DEBUG(sprite, 3, "Compacting sprite cache, inuse=%d", GetSpriteCacheUsage());

	for (s = _spritecache_ptr; s->size != 0;) {
		if (s->size & S_FREE_MASK) {
			MemBlock* next = NextBlock(s);
			MemBlock temp;
			void** i;

			// Since free blocks are automatically coalesced, this should hold true.
			assert(!(next->size & S_FREE_MASK));

			// If the next block is the sentinel block, we can safely return
			if (next->size == 0)
				break;

			// Locate the sprite belonging to the next pointer.
			for (i = _sprite_ptr; *i != next->data; ++i) {
				assert(i != endof(_sprite_ptr));
			}

			*i = s->data; // Adjust sprite array entry
			// Swap this and the next block
			temp = *s;
			memmove(s, next, next->size);
			s = NextBlock(s);
			*s = temp;

			// Coalesce free blocks
			while (NextBlock(s)->size & S_FREE_MASK) {
				s->size += NextBlock(s)->size & ~S_FREE_MASK;
			}
		} else {
			s = NextBlock(s);
		}
	}
}

static void DeleteEntryFromSpriteCache(void)
{
	int i;
	int best = -1;
	MemBlock* s;
	int cur_lru;

	DEBUG(sprite, 3, "DeleteEntryFromSpriteCache, inuse=%d", GetSpriteCacheUsage());

	cur_lru = 0xffff;
	for (i = 0; i != MAX_SPRITES; i++) {
		if (_sprite_ptr[i] != NULL && _sprite_lru_new[i] < cur_lru) {
			cur_lru = _sprite_lru_new[i];
			best = i;
		}
	}

	// Display an error message and die, in case we found no sprite at all.
	// This shouldn't really happen, unless all sprites are locked.
	if (best == -1)
		error("Out of sprite memory");

	// Mark the block as free (the block must be in use)
	s = (MemBlock*)_sprite_ptr[best] - 1;
	assert(!(s->size & S_FREE_MASK));
	s->size |= S_FREE_MASK;
	_sprite_ptr[best] = NULL;

	// And coalesce adjacent free blocks
	for (s = _spritecache_ptr; s->size != 0; s = NextBlock(s)) {
		if (s->size & S_FREE_MASK) {
			while (NextBlock(s)->size & S_FREE_MASK) {
				s->size += NextBlock(s)->size & ~S_FREE_MASK;
			}
		}
	}
}

static void* AllocSprite(size_t mem_req)
{
	mem_req += sizeof(MemBlock);

	/* Align this to an uint32 boundary. This also makes sure that the 2 least
	 * bits are not used, so we could use those for other things. */
	mem_req = ALIGN(mem_req, sizeof(uint32));

	for (;;) {
		MemBlock* s;

		for (s = _spritecache_ptr; s->size != 0; s = NextBlock(s)) {
			if (s->size & S_FREE_MASK) {
				size_t cur_size = s->size & ~S_FREE_MASK;

				/* Is the block exactly the size we need or
				 * big enough for an additional free block? */
				if (cur_size == mem_req ||
						cur_size >= mem_req + sizeof(MemBlock)) {
					// Set size and in use
					s->size = mem_req;

					// Do we need to inject a free block too?
					if (cur_size != mem_req) {
						NextBlock(s)->size = (cur_size - mem_req) | S_FREE_MASK;
					}

					return s->data;
				}
			}
		}

		// Reached sentinel, but no block found yet. Delete some old entry.
		DeleteEntryFromSpriteCache();
	}
}


const void *GetRawSprite(SpriteID sprite)
{
	void* p;

	assert(sprite < MAX_SPRITES);

	// Update LRU
	_sprite_lru_new[sprite] = ++_sprite_lru_counter;

	p = _sprite_ptr[sprite];
	// Load the sprite, if it is not loaded, yet
	if (p == NULL) p = ReadSprite(sprite);
	return p;
}


void GfxInitSpriteMem(void)
{
	// initialize sprite cache heap
	if (_spritecache_ptr == NULL) _spritecache_ptr = malloc(SPRITE_CACHE_SIZE);

	// A big free block
	_spritecache_ptr->size = (SPRITE_CACHE_SIZE - sizeof(MemBlock)) | S_FREE_MASK;
	// Sentinel block (identified by size == 0)
	NextBlock(_spritecache_ptr)->size = 0;

	memset(_sprite_ptr, 0, sizeof(_sprite_ptr));

	_compact_cache_counter = 0;
}
