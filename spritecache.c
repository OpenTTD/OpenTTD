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

#define WANT_NEW_LRU


static void* _sprite_ptr[MAX_SPRITES];
static uint32 _sprite_file_pos[MAX_SPRITES];

#if defined(WANT_NEW_LRU)
static int16 _sprite_lru_new[MAX_SPRITES];
#else
static uint16 _sprite_lru[MAX_SPRITES];
static uint16 _sprite_lru_cur[MAX_SPRITES];
#endif

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

	DEBUG(spritecache, 9) ("load sprite %d", id);

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

#if defined(WANT_NEW_LRU)
	_sprite_lru_new[load_index] = 0;
#else
	_sprite_lru[load_index] = 0xFFFF;
	_sprite_lru_cur[load_index] = 0;
#endif

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
#if defined(WANT_NEW_LRU)
	if (_sprite_lru_counter > 16384) {
		DEBUG(spritecache, 2) ("fixing lru %d, inuse=%d", _sprite_lru_counter, GetSpriteCacheUsage());

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
#else
	for (i = 0; i != MAX_SPRITES; i++)
		if (_sprite_ptr[i] != NULL && _sprite_lru[i] != 65535)
			_sprite_lru[i]++;
	// Reset the lru counter.
	_sprite_lru_counter = 0;
#endif

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

	DEBUG(spritecache, 2) (
		"compacting sprite cache, inuse=%d", GetSpriteCacheUsage()
	);

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

	DEBUG(spritecache, 2) ("DeleteEntryFromSpriteCache, inuse=%d", GetSpriteCacheUsage());

#if defined(WANT_NEW_LRU)
	cur_lru = 0xffff;
	for (i = 0; i != MAX_SPRITES; i++) {
		if (_sprite_ptr[i] != NULL && _sprite_lru_new[i] < cur_lru) {
			cur_lru = _sprite_lru_new[i];
			best = i;
		}
	}
#else
	{
	uint16 cur_lru = 0, cur_lru_cur = 0xffff;
	for (i = 0; i != MAX_SPRITES; i++) {
		if (_sprite_ptr[i] == NULL || _sprite_lru[i] < cur_lru) continue;

		// Found a sprite with a higher LRU value, then remember it.
		if (_sprite_lru[i] != cur_lru) {
			cur_lru = _sprite_lru[i];
			best = i;

		// Else if both sprites were very recently referenced, compare by the cur value instead.
		} else if (cur_lru == 0 && _sprite_lru_cur[i] <= cur_lru_cur) {
			cur_lru_cur = _sprite_lru_cur[i];
			cur_lru = _sprite_lru[i];
			best = i;
		}
	}
	}
#endif

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

#if defined(NEW_ROTATION)
#define X15(x) else if (s >= x && s < (x+15)) { s = _rotate_tile_sprite[s - x] + x; }
#define X19(x) else if (s >= x && s < (x+19)) { s = _rotate_tile_sprite[s - x] + x; }
#define MAP(from,to,map) else if (s >= from && s <= to) { s = map[s - from] + from; }


static uint RotateSprite(uint s)
{
	static const byte _rotate_tile_sprite[19] = { 0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 17, 18, 16, 15 };
	static const byte _coast_map[9] = {0, 4, 3, 1, 2, 6, 8, 5, 7};
	static const byte _fence_map[6] = {1, 0, 5, 4, 3, 2};

	if (0);
	X19(752)
	X15(990-1)
	X19(3924)
	X19(3943)
	X19(3962)
	X19(3981)
	X19(4000)
	X19(4023)
	X19(4042)
	MAP(4061, 4069, _coast_map)
	X19(4126)
	X19(4145)
	X19(4164)
	X19(4183)
	X19(4202)
	X19(4221)
	X19(4240)
	X19(4259)
	X19(4259)
	X19(4278)
	MAP(4090, 4095, _fence_map)
	MAP(4096, 4101, _fence_map)
	MAP(4102, 4107, _fence_map)
	MAP(4108, 4113, _fence_map)
	MAP(4114, 4119, _fence_map)
	MAP(4120, 4125, _fence_map)
	return s;
}
#endif

const void *GetRawSprite(SpriteID sprite)
{
	void* p;

	assert(sprite < MAX_SPRITES);

#if defined(NEW_ROTATION)
	sprite = RotateSprite(sprite);
#endif

	// Update LRU
#if defined(WANT_NEW_LRU)
	_sprite_lru_new[sprite] = ++_sprite_lru_counter;
#else
	_sprite_lru_cur[sprite] = ++_sprite_lru_counter;
	_sprite_lru[sprite] = 0;
#endif

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
