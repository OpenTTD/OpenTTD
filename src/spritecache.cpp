/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file spritecache.cpp Caching of sprites. */

#include "stdafx.h"
#include "fileio_func.h"
#include "spriteloader/grf.hpp"
#include "gfx_func.h"
#ifdef WITH_PNG
#include "spriteloader/png.hpp"
#endif /* WITH_PNG */
#include "blitter/factory.hpp"
#include "core/math_func.hpp"

#include "table/sprites.h"

/* Default of 4MB spritecache */
uint _sprite_cache_size = 4;

typedef SimpleTinyEnumT<SpriteType, byte> SpriteTypeByte;

struct SpriteCache {
	void *ptr;
	size_t file_pos;
	uint32 id;
	uint16 file_slot;
	int16 lru;
	SpriteTypeByte type; ///< In some cases a single sprite is misused by two NewGRFs. Once as real sprite and once as recolour sprite. If the recolour sprite gets into the cache it might be drawn as real sprite which causes enormous trouble.
	bool warned;         ///< True iff the user has been warned about incorrect use of this sprite
};


static uint _spritecache_items = 0;
static SpriteCache *_spritecache = NULL;


static inline SpriteCache *GetSpriteCache(uint index)
{
	return &_spritecache[index];
}

static inline bool IsMapgenSpriteID(SpriteID sprite)
{
	return IsInsideMM(sprite, 4845, 4882);
}

static SpriteCache *AllocateSpriteCache(uint index)
{
	if (index >= _spritecache_items) {
		/* Add another 1024 items to the 'pool' */
		uint items = Align(index + 1, 1024);

		DEBUG(sprite, 4, "Increasing sprite cache to %u items (" PRINTF_SIZE " bytes)", items, items * sizeof(*_spritecache));

		_spritecache = ReallocT(_spritecache, items);

		/* Reset the new items and update the count */
		memset(_spritecache + _spritecache_items, 0, (items - _spritecache_items) * sizeof(*_spritecache));
		_spritecache_items = items;
	}

	return GetSpriteCache(index);
}


struct MemBlock {
	size_t size;
	byte data[];
};

static uint _sprite_lru_counter;
static MemBlock *_spritecache_ptr;
static int _compact_cache_counter;

static void CompactSpriteCache();

/**
 * Skip the given amount of sprite graphics data.
 * @param type the type of sprite (compressed etc)
 * @param num the amount of sprites to skip
 * @return true if the data could be correctly skipped.
 */
bool SkipSpriteData(byte type, uint16 num)
{
	if (type & 2) {
		FioSkipBytes(num);
	} else {
		while (num > 0) {
			int8 i = FioReadByte();
			if (i >= 0) {
				int size = (i == 0) ? 0x80 : i;
				if (size > num) return false;
				num -= size;
				FioSkipBytes(size);
			} else {
				i = -(i >> 3);
				num -= i;
				FioReadByte();
			}
		}
	}
	return true;
}

/**
 * Read the sprite header data and then skip the real payload.
 * @return type of sprite; ST_INVALID if the sprite is a pseudo- or unusable sprite
 */
static SpriteType ReadSpriteHeaderSkipData()
{
	uint16 num = FioReadWord();

	if (num == 0) return ST_INVALID;

	byte type = FioReadByte();
	if (type == 0xFF) {
		FioSkipBytes(num);
		/* Some NewGRF files have "empty" pseudo-sprites which are 1
		 * byte long. Catch these so the sprites won't be displayed. */
		return (num == 1) ? ST_INVALID : ST_RECOLOUR;
	}

	FioSkipBytes(7);
	return SkipSpriteData(type, num - 8) ? ST_NORMAL : ST_INVALID;
}

/* Check if the given Sprite ID exists */
bool SpriteExists(SpriteID id)
{
	/* Special case for Sprite ID zero -- its position is also 0... */
	if (id == 0) return true;
	if (id >= _spritecache_items) return false;
	return !(GetSpriteCache(id)->file_pos == 0 && GetSpriteCache(id)->file_slot == 0);
}

/**
 * Get the sprite type of a given sprite.
 * @param sprite The sprite to look at.
 * @return the type of sprite.
 */
SpriteType GetSpriteType(SpriteID sprite)
{
	if (!SpriteExists(sprite)) return ST_INVALID;
	return GetSpriteCache(sprite)->type;
}

/**
 * Get the (FIOS) file slot of a given sprite.
 * @param sprite The sprite to look at.
 * @return the FIOS file slot
 */
uint GetOriginFileSlot(SpriteID sprite)
{
	if (!SpriteExists(sprite)) return 0;
	return GetSpriteCache(sprite)->file_slot;
}

/**
 * Get a reasonable (upper bound) estimate of the maximum
 * SpriteID used in OpenTTD; there will be no sprites with
 * a higher SpriteID, although there might be up to roughly
 * a thousand unused SpriteIDs below this number.
 * @note It's actually the number of spritecache items.
 * @return maximum SpriteID
 */
uint GetMaxSpriteID()
{
	return _spritecache_items;
}

static void *AllocSprite(size_t);

static void *ReadSprite(SpriteCache *sc, SpriteID id, SpriteType sprite_type)
{
	uint8 file_slot = sc->file_slot;
	size_t file_pos = sc->file_pos;

	assert(IsMapgenSpriteID(id) == (sprite_type == ST_MAPGEN));
	assert(sc->type == sprite_type);

	DEBUG(sprite, 9, "Load sprite %d", id);

	if (sprite_type == ST_NORMAL && BlitterFactoryBase::GetCurrentBlitter()->GetScreenDepth() == 32) {
#ifdef WITH_PNG
		/* Try loading 32bpp graphics in case we are 32bpp output */
		SpriteLoaderPNG sprite_loader;
		SpriteLoader::Sprite sprite;

		if (sprite_loader.LoadSprite(&sprite, file_slot, sc->id, sprite_type)) {
			sc->ptr = BlitterFactoryBase::GetCurrentBlitter()->Encode(&sprite, &AllocSprite);

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

	/* Type 0xFF indicates either a colourmap or some other non-sprite info */
	assert((type == 0xFF) == (sprite_type == ST_RECOLOUR));
	if (type == 0xFF) {
		/* "Normal" recolour sprites are ALWAYS 257 bytes. Then there is a small
		 * number of recolour sprites that are 17 bytes that only exist in DOS
		 * GRFs which are the same as 257 byte recolour sprites, but with the last
		 * 240 bytes zeroed.  */
		static const int RECOLOUR_SPRITE_SIZE = 257;
		byte *dest = (byte *)AllocSprite(max(RECOLOUR_SPRITE_SIZE, num));

		sc->ptr = dest;

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

	assert(sprite_type == ST_NORMAL || sprite_type == ST_FONT);

	SpriteLoaderGrf sprite_loader;
	SpriteLoader::Sprite sprite;

	if (!sprite_loader.LoadSprite(&sprite, file_slot, file_pos, sprite_type)) {
		if (id == SPR_IMG_QUERY) usererror("Okay... something went horribly wrong. I couldn't load the fallback sprite. What should I do?");
		return (void*)GetRawSprite(SPR_IMG_QUERY, ST_NORMAL);
	}
	sc->ptr = BlitterFactoryBase::GetCurrentBlitter()->Encode(&sprite, &AllocSprite);

	return sc->ptr;
}


bool LoadNextSprite(int load_index, byte file_slot, uint file_sprite_id)
{
	size_t file_pos = FioGetPos();

	SpriteType type = ReadSpriteHeaderSkipData();

	if (type == ST_INVALID) return false;

	if (load_index >= MAX_SPRITES) {
		usererror("Tried to load too many sprites (#%d; max %d)", load_index, MAX_SPRITES);
	}

	bool is_mapgen = IsMapgenSpriteID(load_index);

	if (is_mapgen) {
		if (type != ST_NORMAL) usererror("Uhm, would you be so kind not to load a NewGRF that changes the type of the map generator sprites?");
		type = ST_MAPGEN;
	}

	SpriteCache *sc = AllocateSpriteCache(load_index);
	sc->file_slot = file_slot;
	sc->file_pos = file_pos;
	sc->ptr = NULL;
	sc->lru = 0;
	sc->id = file_sprite_id;
	sc->type = type;
	sc->warned = false;

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
	scnew->warned = false;
}

/**
 * S_FREE_MASK is used to mask-out lower bits of MemBlock::size
 * If they are non-zero, the block is free.
 * S_FREE_MASK has to ensure MemBlock is correctly aligned -
 * it means 8B (S_FREE_MASK == 7) on 64bit systems!
 */
static const size_t S_FREE_MASK = sizeof(size_t) - 1;

/* to make sure nobody adds things to MemBlock without checking S_FREE_MASK first */
assert_compile(sizeof(MemBlock) == sizeof(size_t));
/* make sure it's a power of two */
assert_compile((sizeof(size_t) & (sizeof(size_t) - 1)) == 0);

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

		DEBUG(sprite, 3, "Fixing lru %u, inuse=" PRINTF_SIZE, _sprite_lru_counter, GetSpriteCacheUsage());

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

/**
 * Called when holes in the sprite cache should be removed.
 * That is accomplished by moving the cached data.
 */
static void CompactSpriteCache()
{
	MemBlock *s;

	DEBUG(sprite, 3, "Compacting sprite cache, inuse=" PRINTF_SIZE, GetSpriteCacheUsage());

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

	DEBUG(sprite, 3, "DeleteEntryFromSpriteCache, inuse=" PRINTF_SIZE, GetSpriteCacheUsage());

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

static void *AllocSprite(size_t mem_req)
{
	mem_req += sizeof(MemBlock);

	/* Align this to correct boundary. This also makes sure at least one
	 * bit is not used, so we can use it for other things. */
	mem_req = Align(mem_req, S_FREE_MASK + 1);

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

/**
 * Handles the case when a sprite of different type is requested than is present in the SpriteCache.
 * For ST_FONT sprites, it is normal. In other cases, default sprite is loaded instead.
 * @param sprite ID of loaded sprite
 * @param requested requested sprite type
 * @param sc the currently known sprite cache for the requested sprite
 * @return fallback sprite
 * @note this function will do usererror() in the case the fallback sprite isn't available
 */
static void *HandleInvalidSpriteRequest(SpriteID sprite, SpriteType requested, SpriteCache *sc)
{
	static const char * const sprite_types[] = {
		"normal",        // ST_NORMAL
		"map generator", // ST_MAPGEN
		"character",     // ST_FONT
		"recolour",      // ST_RECOLOUR
	};

	SpriteType available = sc->type;
	if (requested == ST_FONT && available == ST_NORMAL) {
		if (sc->ptr == NULL) sc->type = ST_FONT;
		return GetRawSprite(sprite, sc->type);
	}

	byte warning_level = sc->warned ? 6 : 0;
	sc->warned = true;
	DEBUG(sprite, warning_level, "Tried to load %s sprite #%d as a %s sprite. Probable cause: NewGRF interference", sprite_types[available], sprite, sprite_types[requested]);

	switch (requested) {
		case ST_NORMAL:
			if (sprite == SPR_IMG_QUERY) usererror("Uhm, would you be so kind not to load a NewGRF that makes the 'query' sprite a non-normal sprite?");
			/* FALL THROUGH */
		case ST_FONT:
			return GetRawSprite(SPR_IMG_QUERY, ST_NORMAL);
		case ST_RECOLOUR:
			if (sprite == PALETTE_TO_DARK_BLUE) usererror("Uhm, would you be so kind not to load a NewGRF that makes the 'PALETTE_TO_DARK_BLUE' sprite a non-remap sprite?");
			return GetRawSprite(PALETTE_TO_DARK_BLUE, ST_RECOLOUR);
		case ST_MAPGEN:
			/* this shouldn't happen, overriding of ST_MAPGEN sprites is checked in LoadNextSprite()
			 * (the only case the check fails is when these sprites weren't even loaded...) */
		default:
			NOT_REACHED();
	}
}

void *GetRawSprite(SpriteID sprite, SpriteType type)
{
	assert(IsMapgenSpriteID(sprite) == (type == ST_MAPGEN));
	assert(type < ST_INVALID);

	if (!SpriteExists(sprite)) {
		DEBUG(sprite, 1, "Tried to load non-existing sprite #%d. Probable cause: Wrong/missing NewGRFs", sprite);

		/* SPR_IMG_QUERY is a BIG FAT RED ? */
		sprite = SPR_IMG_QUERY;
	}

	SpriteCache *sc = GetSpriteCache(sprite);

	if (sc->type != type) return HandleInvalidSpriteRequest(sprite, type, sc);

	/* Update LRU */
	sc->lru = ++_sprite_lru_counter;

	void *p = sc->ptr;

	/* Load the sprite, if it is not loaded, yet */
	if (p == NULL) p = ReadSprite(sc, sprite, type);

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

/* static */ ReusableBuffer<SpriteLoader::CommonPixel> SpriteLoader::Sprite::buffer;
