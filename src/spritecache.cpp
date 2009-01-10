/* $Id$ */

/** @file spritecache.cpp Caching of sprites. */

#include "stdafx.h"
#include "openttd.h"
#include "variables.h"
#include "debug.h"
#include "spritecache.h"
#include "fileio_func.h"
#include "spriteloader/grf.hpp"
#include "core/alloc_func.hpp"
#include "core/math_func.hpp"
#include "gfx_func.h"
#ifdef WITH_PNG
#include "spriteloader/png.hpp"
#endif /* WITH_PNG */
#include "blitter/factory.hpp"

#include "table/sprites.h"

/* Default of 4MB spritecache */
uint _sprite_cache_size = 4;


struct SpriteCache {
	void *ptr;
	size_t file_pos;
	uint32 id;
	uint16 file_slot;
	int16 lru;
	SpriteType type; ///< In some cases a single sprite is misused by two NewGRFs. Once as real sprite and once as recolour sprite. If the recolour sprite gets into the cache it might be drawn as real sprite which causes enormous trouble.
};


static uint _spritecache_items = 0;
static SpriteCache *_spritecache = NULL;


static inline SpriteCache *GetSpriteCache(uint index)
{
	return &_spritecache[index];
}


static SpriteCache *AllocateSpriteCache(uint index)
{
	if (index >= _spritecache_items) {
		/* Add another 1024 items to the 'pool' */
		uint items = Align(index + 1, 1024);

		DEBUG(sprite, 4, "Increasing sprite cache to %d items (%d bytes)", items, items * sizeof(*_spritecache));

		_spritecache = ReallocT(_spritecache, items);

		/* Reset the new items and update the count */
		memset(_spritecache + _spritecache_items, 0, (items - _spritecache_items) * sizeof(*_spritecache));
		_spritecache_items = items;
	}

	return GetSpriteCache(index);
}


struct MemBlock {
	size_t size;
	byte data[VARARRAY_SIZE];
};

static uint _sprite_lru_counter;
static MemBlock *_spritecache_ptr;
static int _compact_cache_counter;

static void CompactSpriteCache();

/**
 * Skip the given amount of sprite graphics data.
 * @param type the type of sprite (compressed etc)
 * @param num the amount of sprites to skip
 */
void SkipSpriteData(byte type, uint16 num)
{
	if (type & 2) {
		FioSkipBytes(num);
	} else {
		while (num > 0) {
			int8 i = FioReadByte();
			if (i >= 0) {
				int size = (i == 0) ? 0x80 : i;
				num -= size;
				FioSkipBytes(size);
			} else {
				i = -(i >> 3);
				num -= i;
				FioReadByte();
			}
		}
	}
}

/**
 * Read the sprite header data and then skip the real payload.
 * @return true if the sprite is a pseudo sprite.
 */
static bool ReadSpriteHeaderSkipData()
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
	SkipSpriteData(type, num - 8);

	return true;
}

/* Check if the given Sprite ID exists */
bool SpriteExists(SpriteID id)
{
	/* Special case for Sprite ID zero -- its position is also 0... */
	if (id == 0) return true;
	if (id >= _spritecache_items) return false;
	return !(GetSpriteCache(id)->file_pos == 0 && GetSpriteCache(id)->file_slot == 0);
}

void *AllocSprite(size_t);

static void *ReadSprite(SpriteCache *sc, SpriteID id, SpriteType sprite_type)
{
	uint8 file_slot = sc->file_slot;
	size_t file_pos = sc->file_pos;

	DEBUG(sprite, 9, "Load sprite %d", id);

	if (!SpriteExists(id)) {
		DEBUG(sprite, 1, "Tried to load non-existing sprite #%d. Probable cause: Wrong/missing NewGRFs", id);

		/* SPR_IMG_QUERY is a BIG FAT RED ? */
		id = SPR_IMG_QUERY;
		file_slot = GetSpriteCache(SPR_IMG_QUERY)->file_slot;
		file_pos  = GetSpriteCache(SPR_IMG_QUERY)->file_pos;
	}

	if (sprite_type == ST_NORMAL && BlitterFactoryBase::GetCurrentBlitter()->GetScreenDepth() == 32) {
#ifdef WITH_PNG
		/* Try loading 32bpp graphics in case we are 32bpp output */
		SpriteLoaderPNG sprite_loader;
		SpriteLoader::Sprite sprite;

		if (sprite_loader.LoadSprite(&sprite, file_slot, sc->id, sprite_type)) {
			sc->ptr = BlitterFactoryBase::GetCurrentBlitter()->Encode(&sprite, &AllocSprite);
			free(sprite.data);

			sc->type = sprite_type;

			return sc->ptr;
		}
		/* If the PNG couldn't be loaded, fall back to 8bpp grfs */
#else
		static bool show_once = true;
		if (show_once) {
			DEBUG(misc, 0, "You are running a 32bpp blitter, but this build is without libpng support; falling back to 8bpp graphics");
			show_once = false;
		}
#endif /* WITH_PNG */
	}

	FioSeekToFile(file_slot, file_pos);

	/* Read the size and type */
	int num  = FioReadWord();
	byte type = FioReadByte();
	/* Type 0xFF indicates either a colormap or some other non-sprite info */
	if (type == 0xFF) {
		if (sprite_type != ST_RECOLOUR) {
			static byte warning_level = 0;
			DEBUG(sprite, warning_level, "Tried to load recolour sprite #%d as a real sprite. Probable cause: NewGRF interference", id);
			warning_level = 6;
			if (id == SPR_IMG_QUERY) usererror("Uhm, would you be so kind not to load a NewGRF that makes the 'query' sprite a recolour-sprite?");
			return (void*)GetRawSprite(SPR_IMG_QUERY, ST_NORMAL);
		}

		/* "Normal" recolour sprites are ALWAYS 257 bytes. Then there is a small
		 * number of recolour sprites that are 17 bytes that only exist in DOS
		 * GRFs which are the same as 257 byte recolour sprites, but with the last
		 * 240 bytes zeroed.  */
		static const int RECOLOUR_SPRITE_SIZE = 257;
		byte *dest = (byte *)AllocSprite(max(RECOLOUR_SPRITE_SIZE, num));

		sc->ptr = dest;
		sc->type = sprite_type;

		if (_palette_remap_grf[sc->file_slot]) {
			byte *dest_tmp = AllocaM(byte, max(RECOLOUR_SPRITE_SIZE, num));

			/* Only a few recolour sprites are less than 257 bytes */
			if (num < RECOLOUR_SPRITE_SIZE) memset(dest_tmp, 0, RECOLOUR_SPRITE_SIZE);
			FioReadBlock(dest_tmp, num);

			/* The data of index 0 is never used; "literal 00" according to the (New)GRF specs. */
			for (int i = 1; i < RECOLOUR_SPRITE_SIZE; i++) {
				dest[i] = _palette_remap[dest_tmp[_palette_reverse_remap[i - 1] + 1]];
			}
		} else {
			FioReadBlock(dest, num);
		}

		return sc->ptr;
	}
	/* Ugly hack to work around the problem that the old landscape
	 *  generator assumes that those sprites are stored uncompressed in
	 *  the memory, and they are only read directly by the code, never
	 *  send to the blitter. So do not send it to the blitter (which will
	 *  result in a data array in the format the blitter likes most), but
	 *  read the data directly from disk and store that as sprite.
	 * Ugly: yes. Other solution: no. Blame the original author or
	 *  something ;) The image should really have been a data-stream
	 *  (so type = 0xFF basicly). */
	assert((id >= 4845 && id <= 4881) == (sprite_type == ST_MAPGEN));
	if (sprite_type == ST_MAPGEN) {
		uint height = FioReadByte();
		uint width  = FioReadWord();
		Sprite *sprite;
		byte *dest;

		num = width * height;
		sprite = (Sprite *)AllocSprite(sizeof(*sprite) + num);
		sc->ptr = sprite;
		sprite->height = height;
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
				const byte *rel = dest - (((i & 7) << 8) | FioReadByte());
				i = -(i >> 3);
				num -= i;
				for (; i > 0; --i) *dest++ = *rel++;
			}
		}

		sc->type = sprite_type;

		return sc->ptr;
	}

	if (sprite_type == ST_RECOLOUR) {
		static byte warning_level = 0;
		DEBUG(sprite, warning_level, "Tried to load real sprite #%d as a recolour sprite. Probable cause: NewGRF interference", id);
		warning_level = 6;
		return (void*)GetRawSprite(id, ST_NORMAL);
	}

	SpriteLoaderGrf sprite_loader;
	SpriteLoader::Sprite sprite;

	sc->type = sprite_type;

	if (!sprite_loader.LoadSprite(&sprite, file_slot, file_pos, sprite_type)) {
		if (id == SPR_IMG_QUERY) usererror("Okay... something went horribly wrong. I couldn't load the fallback sprite. What should I do?");
		return (void*)GetRawSprite(SPR_IMG_QUERY, ST_NORMAL);
	}
	sc->ptr = BlitterFactoryBase::GetCurrentBlitter()->Encode(&sprite, &AllocSprite);
	free(sprite.data);

	return sc->ptr;
}


bool LoadNextSprite(int load_index, byte file_slot, uint file_sprite_id)
{
	SpriteCache *sc;
	size_t file_pos = FioGetPos();

	if (!ReadSpriteHeaderSkipData()) return false;

	if (load_index >= MAX_SPRITES) {
		usererror("Tried to load too many sprites (#%d; max %d)", load_index, MAX_SPRITES);
	}

	sc = AllocateSpriteCache(load_index);
	sc->file_slot = file_slot;
	sc->file_pos = file_pos;
	sc->ptr = NULL;
	sc->lru = 0;
	sc->id = file_sprite_id;
	sc->type = ST_NORMAL;

	return true;
}


void DupSprite(SpriteID old_spr, SpriteID new_spr)
{
	SpriteCache *scnew = AllocateSpriteCache(new_spr); // may reallocate: so put it first
	SpriteCache *scold = GetSpriteCache(old_spr);

	scnew->file_slot = scold->file_slot;
	scnew->file_pos = scold->file_pos;
	scnew->ptr = NULL;
	scnew->id = scold->id;
	scnew->type = scold->type;
}


#define S_FREE_MASK 1

static inline MemBlock *NextBlock(MemBlock *block)
{
	return (MemBlock*)((byte*)block + (block->size & ~S_FREE_MASK));
}

static size_t GetSpriteCacheUsage()
{
	size_t tot_size = 0;
	MemBlock *s;

	for (s = _spritecache_ptr; s->size != 0; s = NextBlock(s)) {
		if (!(s->size & S_FREE_MASK)) tot_size += s->size;
	}

	return tot_size;
}


void IncreaseSpriteLRU()
{
	/* Increase all LRU values */
	if (_sprite_lru_counter > 16384) {
		SpriteID i;

		DEBUG(sprite, 3, "Fixing lru %d, inuse=%d", _sprite_lru_counter, GetSpriteCacheUsage());

		for (i = 0; i != _spritecache_items; i++) {
			SpriteCache *sc = GetSpriteCache(i);
			if (sc->ptr != NULL) {
				if (sc->lru >= 0) {
					sc->lru = -1;
				} else if (sc->lru != -32768) {
					sc->lru--;
				}
			}
		}
		_sprite_lru_counter = 0;
	}

	/* Compact sprite cache every now and then. */
	if (++_compact_cache_counter >= 740) {
		CompactSpriteCache();
		_compact_cache_counter = 0;
	}
}

/** Called when holes in the sprite cache should be removed.
 * That is accomplished by moving the cached data. */
static void CompactSpriteCache()
{
	MemBlock *s;

	DEBUG(sprite, 3, "Compacting sprite cache, inuse=%d", GetSpriteCacheUsage());

	for (s = _spritecache_ptr; s->size != 0;) {
		if (s->size & S_FREE_MASK) {
			MemBlock *next = NextBlock(s);
			MemBlock temp;
			SpriteID i;

			/* Since free blocks are automatically coalesced, this should hold true. */
			assert(!(next->size & S_FREE_MASK));

			/* If the next block is the sentinel block, we can safely return */
			if (next->size == 0) break;

			/* Locate the sprite belonging to the next pointer. */
			for (i = 0; GetSpriteCache(i)->ptr != next->data; i++) {
				assert(i != _spritecache_items);
			}

			GetSpriteCache(i)->ptr = s->data; // Adjust sprite array entry
			/* Swap this and the next block */
			temp = *s;
			memmove(s, next, next->size);
			s = NextBlock(s);
			*s = temp;

			/* Coalesce free blocks */
			while (NextBlock(s)->size & S_FREE_MASK) {
				s->size += NextBlock(s)->size & ~S_FREE_MASK;
			}
		} else {
			s = NextBlock(s);
		}
	}
}

static void DeleteEntryFromSpriteCache()
{
	SpriteID i;
	uint best = UINT_MAX;
	MemBlock *s;
	int cur_lru;

	DEBUG(sprite, 3, "DeleteEntryFromSpriteCache, inuse=%d", GetSpriteCacheUsage());

	cur_lru = 0xffff;
	for (i = 0; i != _spritecache_items; i++) {
		SpriteCache *sc = GetSpriteCache(i);
		if (sc->ptr != NULL && sc->lru < cur_lru) {
			cur_lru = sc->lru;
			best = i;
		}
	}

	/* Display an error message and die, in case we found no sprite at all.
	 * This shouldn't really happen, unless all sprites are locked. */
	if (best == UINT_MAX) error("Out of sprite memory");

	/* Mark the block as free (the block must be in use) */
	s = (MemBlock*)GetSpriteCache(best)->ptr - 1;
	assert(!(s->size & S_FREE_MASK));
	s->size |= S_FREE_MASK;
	GetSpriteCache(best)->ptr = NULL;

	/* And coalesce adjacent free blocks */
	for (s = _spritecache_ptr; s->size != 0; s = NextBlock(s)) {
		if (s->size & S_FREE_MASK) {
			while (NextBlock(s)->size & S_FREE_MASK) {
				s->size += NextBlock(s)->size & ~S_FREE_MASK;
			}
		}
	}
}

void *AllocSprite(size_t mem_req)
{
	mem_req += sizeof(MemBlock);

	/* Align this to an uint32 boundary. This also makes sure that the 2 least
	 * bits are not used, so we could use those for other things. */
	mem_req = Align(mem_req, sizeof(uint32));

	for (;;) {
		MemBlock *s;

		for (s = _spritecache_ptr; s->size != 0; s = NextBlock(s)) {
			if (s->size & S_FREE_MASK) {
				size_t cur_size = s->size & ~S_FREE_MASK;

				/* Is the block exactly the size we need or
				 * big enough for an additional free block? */
				if (cur_size == mem_req ||
						cur_size >= mem_req + sizeof(MemBlock)) {
					/* Set size and in use */
					s->size = mem_req;

					/* Do we need to inject a free block too? */
					if (cur_size != mem_req) {
						NextBlock(s)->size = (cur_size - mem_req) | S_FREE_MASK;
					}

					return s->data;
				}
			}
		}

		/* Reached sentinel, but no block found yet. Delete some old entry. */
		DeleteEntryFromSpriteCache();
	}
}


const void *GetRawSprite(SpriteID sprite, SpriteType type)
{
	SpriteCache *sc;
	void *p;

	assert(sprite < _spritecache_items);

	sc = GetSpriteCache(sprite);

	/* Update LRU */
	sc->lru = ++_sprite_lru_counter;

	p = sc->ptr;

	/* Load the sprite, if it is not loaded, yet */
	if (p == NULL || sc->type != type) p = ReadSprite(sc, sprite, type);

	return p;
}


void GfxInitSpriteMem()
{
	/* initialize sprite cache heap */
	if (_spritecache_ptr == NULL) _spritecache_ptr = (MemBlock*)MallocT<byte>(_sprite_cache_size * 1024 * 1024);

	/* A big free block */
	_spritecache_ptr->size = ((_sprite_cache_size * 1024 * 1024) - sizeof(MemBlock)) | S_FREE_MASK;
	/* Sentinel block (identified by size == 0) */
	NextBlock(_spritecache_ptr)->size = 0;

	/* Reset the spritecache 'pool' */
	free(_spritecache);
	_spritecache_items = 0;
	_spritecache = NULL;

	_compact_cache_counter = 0;
}
