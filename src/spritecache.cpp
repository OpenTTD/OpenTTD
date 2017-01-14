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
#include "error.h"
#include "zoom_func.h"
#include "settings_type.h"
#include "blitter/factory.hpp"
#include "core/math_func.hpp"
#include "core/mem_func.hpp"

#include "table/sprites.h"
#include "table/strings.h"
#include "table/palette_convert.h"

#include "safeguards.h"

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
	byte container_ver;  ///< Container version of the GRF the sprite is from.
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
static uint _allocated_sprite_cache_size = 0;
static int _compact_cache_counter;

static void CompactSpriteCache();
static void *AllocSprite(size_t mem_req);

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

/* Check if the given Sprite ID exists */
bool SpriteExists(SpriteID id)
{
	if (id >= _spritecache_items) return false;

	/* Special case for Sprite ID zero -- its position is also 0... */
	if (id == 0) return true;
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
 * Count the sprites which originate from a specific file slot in a range of SpriteIDs.
 * @param file_slot FIOS file slot.
 * @param begin First sprite in range.
 * @param end First sprite not in range.
 * @return Number of sprites.
 */
uint GetSpriteCountForSlot(uint file_slot, SpriteID begin, SpriteID end)
{
	uint count = 0;
	for (SpriteID i = begin; i != end; i++) {
		if (SpriteExists(i)) {
			SpriteCache *sc = GetSpriteCache(i);
			if (sc->file_slot == file_slot) count++;
		}
	}
	return count;
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

static bool ResizeSpriteIn(SpriteLoader::Sprite *sprite, ZoomLevel src, ZoomLevel tgt)
{
	uint8 scaled_1 = ScaleByZoom(1, (ZoomLevel)(src - tgt));

	/* Check for possible memory overflow. */
	if (sprite[src].width * scaled_1 > UINT16_MAX || sprite[src].height * scaled_1 > UINT16_MAX) return false;

	sprite[tgt].width  = sprite[src].width  * scaled_1;
	sprite[tgt].height = sprite[src].height * scaled_1;
	sprite[tgt].x_offs = sprite[src].x_offs * scaled_1;
	sprite[tgt].y_offs = sprite[src].y_offs * scaled_1;

	sprite[tgt].AllocateData(tgt, sprite[tgt].width * sprite[tgt].height);

	SpriteLoader::CommonPixel *dst = sprite[tgt].data;
	for (int y = 0; y < sprite[tgt].height; y++) {
		const SpriteLoader::CommonPixel *src_ln = &sprite[src].data[y / scaled_1 * sprite[src].width];
		for (int x = 0; x < sprite[tgt].width; x++) {
			*dst = src_ln[x / scaled_1];
			dst++;
		}
	}

	return true;
}

static void ResizeSpriteOut(SpriteLoader::Sprite *sprite, ZoomLevel zoom)
{
	/* Algorithm based on 32bpp_Optimized::ResizeSprite() */
	sprite[zoom].width  = UnScaleByZoom(sprite[ZOOM_LVL_NORMAL].width,  zoom);
	sprite[zoom].height = UnScaleByZoom(sprite[ZOOM_LVL_NORMAL].height, zoom);
	sprite[zoom].x_offs = UnScaleByZoom(sprite[ZOOM_LVL_NORMAL].x_offs, zoom);
	sprite[zoom].y_offs = UnScaleByZoom(sprite[ZOOM_LVL_NORMAL].y_offs, zoom);

	sprite[zoom].AllocateData(zoom, sprite[zoom].height * sprite[zoom].width);

	SpriteLoader::CommonPixel *dst = sprite[zoom].data;
	const SpriteLoader::CommonPixel *src = sprite[zoom - 1].data;
	const SpriteLoader::CommonPixel *src_end = src + sprite[zoom - 1].height * sprite[zoom - 1].width;

	for (uint y = 0; y < sprite[zoom].height; y++) {
		const SpriteLoader::CommonPixel *src_ln = src + sprite[zoom - 1].width;
		assert(src_ln <= src_end);
		for (uint x = 0; x < sprite[zoom].width; x++) {
			assert(src < src_ln);
			if (src + 1 != src_ln && (src + 1)->a != 0) {
				*dst = *(src + 1);
			} else {
				*dst = *src;
			}
			dst++;
			src += 2;
		}
		src = src_ln + sprite[zoom - 1].width;
	}
}

static bool PadSingleSprite(SpriteLoader::Sprite *sprite, ZoomLevel zoom, uint pad_left, uint pad_top, uint pad_right, uint pad_bottom)
{
	uint width  = sprite->width + pad_left + pad_right;
	uint height = sprite->height + pad_top + pad_bottom;

	if (width > UINT16_MAX || height > UINT16_MAX) return false;

	/* Copy source data and reallocate sprite memory. */
	SpriteLoader::CommonPixel *src_data = MallocT<SpriteLoader::CommonPixel>(sprite->width * sprite->height);
	MemCpyT(src_data, sprite->data, sprite->width * sprite->height);
	sprite->AllocateData(zoom, width * height);

	/* Copy with padding to destination. */
	SpriteLoader::CommonPixel *src = src_data;
	SpriteLoader::CommonPixel *data = sprite->data;
	for (uint y = 0; y < height; y++) {
		if (y < pad_top || pad_bottom + y >= height) {
			/* Top/bottom padding. */
			MemSetT(data, 0, width);
			data += width;
		} else {
			if (pad_left > 0) {
				/* Pad left. */
				MemSetT(data, 0, pad_left);
				data += pad_left;
			}

			/* Copy pixels. */
			MemCpyT(data, src, sprite->width);
			src += sprite->width;
			data += sprite->width;

			if (pad_right > 0) {
				/* Pad right. */
				MemSetT(data, 0, pad_right);
				data += pad_right;
			}
		}
	}
	free(src_data);

	/* Update sprite size. */
	sprite->width   = width;
	sprite->height  = height;
	sprite->x_offs -= pad_left;
	sprite->y_offs -= pad_top;

	return true;
}

static bool PadSprites(SpriteLoader::Sprite *sprite, uint8 sprite_avail)
{
	/* Get minimum top left corner coordinates. */
	int min_xoffs = INT32_MAX;
	int min_yoffs = INT32_MAX;
	for (ZoomLevel zoom = ZOOM_LVL_BEGIN; zoom != ZOOM_LVL_END; zoom++) {
		if (HasBit(sprite_avail, zoom)) {
			min_xoffs = min(min_xoffs, ScaleByZoom(sprite[zoom].x_offs, zoom));
			min_yoffs = min(min_yoffs, ScaleByZoom(sprite[zoom].y_offs, zoom));
		}
	}

	/* Get maximum dimensions taking necessary padding at the top left into account. */
	int max_width  = INT32_MIN;
	int max_height = INT32_MIN;
	for (ZoomLevel zoom = ZOOM_LVL_BEGIN; zoom != ZOOM_LVL_END; zoom++) {
		if (HasBit(sprite_avail, zoom)) {
			max_width  = max(max_width, ScaleByZoom(sprite[zoom].width + sprite[zoom].x_offs - UnScaleByZoom(min_xoffs, zoom), zoom));
			max_height = max(max_height, ScaleByZoom(sprite[zoom].height + sprite[zoom].y_offs - UnScaleByZoom(min_yoffs, zoom), zoom));
		}
	}

	/* Pad sprites where needed. */
	for (ZoomLevel zoom = ZOOM_LVL_BEGIN; zoom != ZOOM_LVL_END; zoom++) {
		if (HasBit(sprite_avail, zoom)) {
			/* Scaling the sprite dimensions in the blitter is done with rounding up,
			 * so a negative padding here is not an error. */
			int pad_left   = max(0, sprite[zoom].x_offs - UnScaleByZoom(min_xoffs, zoom));
			int pad_top    = max(0, sprite[zoom].y_offs - UnScaleByZoom(min_yoffs, zoom));
			int pad_right  = max(0, UnScaleByZoom(max_width, zoom) - sprite[zoom].width - pad_left);
			int pad_bottom = max(0, UnScaleByZoom(max_height, zoom) - sprite[zoom].height - pad_top);

			if (pad_left > 0 || pad_right > 0 || pad_top > 0 || pad_bottom > 0) {
				if (!PadSingleSprite(&sprite[zoom], zoom, pad_left, pad_top, pad_right, pad_bottom)) return false;
			}
		}
	}

	return true;
}

static bool ResizeSprites(SpriteLoader::Sprite *sprite, uint8 sprite_avail, uint32 file_slot, uint32 file_pos)
{
	/* Create a fully zoomed image if it does not exist */
	ZoomLevel first_avail = static_cast<ZoomLevel>(FIND_FIRST_BIT(sprite_avail));
	if (first_avail != ZOOM_LVL_NORMAL) {
		if (!ResizeSpriteIn(sprite, first_avail, ZOOM_LVL_NORMAL)) return false;
		SetBit(sprite_avail, ZOOM_LVL_NORMAL);
	}

	/* Pad sprites to make sizes match. */
	if (!PadSprites(sprite, sprite_avail)) return false;

	/* Create other missing zoom levels */
	for (ZoomLevel zoom = ZOOM_LVL_OUT_2X; zoom != ZOOM_LVL_END; zoom++) {
		if (HasBit(sprite_avail, zoom)) {
			/* Check that size and offsets match the fully zoomed image. */
			assert(sprite[zoom].width  == UnScaleByZoom(sprite[ZOOM_LVL_NORMAL].width,  zoom));
			assert(sprite[zoom].height == UnScaleByZoom(sprite[ZOOM_LVL_NORMAL].height, zoom));
			assert(sprite[zoom].x_offs == UnScaleByZoom(sprite[ZOOM_LVL_NORMAL].x_offs, zoom));
			assert(sprite[zoom].y_offs == UnScaleByZoom(sprite[ZOOM_LVL_NORMAL].y_offs, zoom));
		}

		/* Zoom level is not available, or unusable, so create it */
		if (!HasBit(sprite_avail, zoom)) ResizeSpriteOut(sprite, zoom);
	}

	return  true;
}

/**
 * Load a recolour sprite into memory.
 * @param file_slot GRF we're reading from.
 * @param num Size of the sprite in the GRF.
 * @return Sprite data.
 */
static void *ReadRecolourSprite(uint16 file_slot, uint num)
{
	/* "Normal" recolour sprites are ALWAYS 257 bytes. Then there is a small
	 * number of recolour sprites that are 17 bytes that only exist in DOS
	 * GRFs which are the same as 257 byte recolour sprites, but with the last
	 * 240 bytes zeroed.  */
	static const uint RECOLOUR_SPRITE_SIZE = 257;
	byte *dest = (byte *)AllocSprite(max(RECOLOUR_SPRITE_SIZE, num));

	if (_palette_remap_grf[file_slot]) {
		byte *dest_tmp = AllocaM(byte, max(RECOLOUR_SPRITE_SIZE, num));

		/* Only a few recolour sprites are less than 257 bytes */
		if (num < RECOLOUR_SPRITE_SIZE) memset(dest_tmp, 0, RECOLOUR_SPRITE_SIZE);
		FioReadBlock(dest_tmp, num);

		/* The data of index 0 is never used; "literal 00" according to the (New)GRF specs. */
		for (uint i = 1; i < RECOLOUR_SPRITE_SIZE; i++) {
			dest[i] = _palmap_w2d[dest_tmp[_palmap_d2w[i - 1] + 1]];
		}
	} else {
		FioReadBlock(dest, num);
	}

	return dest;
}

/**
 * Read a sprite from disk.
 * @param sc          Location of sprite.
 * @param id          Sprite number.
 * @param sprite_type Type of sprite.
 * @param allocator   Allocator function to use.
 * @return Read sprite data.
 */
static void *ReadSprite(const SpriteCache *sc, SpriteID id, SpriteType sprite_type, AllocatorProc *allocator)
{
	uint8 file_slot = sc->file_slot;
	size_t file_pos = sc->file_pos;

	assert(sprite_type != ST_RECOLOUR);
	assert(IsMapgenSpriteID(id) == (sprite_type == ST_MAPGEN));
	assert(sc->type == sprite_type);

	DEBUG(sprite, 9, "Load sprite %d", id);

	SpriteLoader::Sprite sprite[ZOOM_LVL_COUNT];
	uint8 sprite_avail = 0;
	sprite[ZOOM_LVL_NORMAL].type = sprite_type;

	SpriteLoaderGrf sprite_loader(sc->container_ver);
	if (sprite_type != ST_MAPGEN && BlitterFactory::GetCurrentBlitter()->GetScreenDepth() == 32) {
		/* Try for 32bpp sprites first. */
		sprite_avail = sprite_loader.LoadSprite(sprite, file_slot, file_pos, sprite_type, true);
	}
	if (sprite_avail == 0) {
		sprite_avail = sprite_loader.LoadSprite(sprite, file_slot, file_pos, sprite_type, false);
	}

	if (sprite_avail == 0) {
		if (sprite_type == ST_MAPGEN) return NULL;
		if (id == SPR_IMG_QUERY) usererror("Okay... something went horribly wrong. I couldn't load the fallback sprite. What should I do?");
		return (void*)GetRawSprite(SPR_IMG_QUERY, ST_NORMAL, allocator);
	}

	if (sprite_type == ST_MAPGEN) {
		/* Ugly hack to work around the problem that the old landscape
		 *  generator assumes that those sprites are stored uncompressed in
		 *  the memory, and they are only read directly by the code, never
		 *  send to the blitter. So do not send it to the blitter (which will
		 *  result in a data array in the format the blitter likes most), but
		 *  extract the data directly and store that as sprite.
		 * Ugly: yes. Other solution: no. Blame the original author or
		 *  something ;) The image should really have been a data-stream
		 *  (so type = 0xFF basically). */
		uint num = sprite[ZOOM_LVL_NORMAL].width * sprite[ZOOM_LVL_NORMAL].height;

		Sprite *s = (Sprite *)allocator(sizeof(*s) + num);
		s->width  = sprite[ZOOM_LVL_NORMAL].width;
		s->height = sprite[ZOOM_LVL_NORMAL].height;
		s->x_offs = sprite[ZOOM_LVL_NORMAL].x_offs;
		s->y_offs = sprite[ZOOM_LVL_NORMAL].y_offs;

		SpriteLoader::CommonPixel *src = sprite[ZOOM_LVL_NORMAL].data;
		byte *dest = s->data;
		while (num-- > 0) {
			*dest++ = src->m;
			src++;
		}

		return s;
	}

	if (!ResizeSprites(sprite, sprite_avail, file_slot, sc->id)) {
		if (id == SPR_IMG_QUERY) usererror("Okay... something went horribly wrong. I couldn't resize the fallback sprite. What should I do?");
		return (void*)GetRawSprite(SPR_IMG_QUERY, ST_NORMAL, allocator);
	}

	if (sprite->type == ST_FONT && ZOOM_LVL_GUI != ZOOM_LVL_NORMAL) {
		/* Make ZOOM_LVL_GUI be ZOOM_LVL_NORMAL */
		sprite[ZOOM_LVL_NORMAL].width  = sprite[ZOOM_LVL_GUI].width;
		sprite[ZOOM_LVL_NORMAL].height = sprite[ZOOM_LVL_GUI].height;
		sprite[ZOOM_LVL_NORMAL].x_offs = sprite[ZOOM_LVL_GUI].x_offs;
		sprite[ZOOM_LVL_NORMAL].y_offs = sprite[ZOOM_LVL_GUI].y_offs;
		sprite[ZOOM_LVL_NORMAL].data   = sprite[ZOOM_LVL_GUI].data;
	}

	return BlitterFactory::GetCurrentBlitter()->Encode(sprite, allocator);
}


/** Map from sprite numbers to position in the GRF file. */
static std::map<uint32, size_t> _grf_sprite_offsets;

/**
 * Get the file offset for a specific sprite in the sprite section of a GRF.
 * @param id ID of the sprite to look up.
 * @return Position of the sprite in the sprite section or SIZE_MAX if no such sprite is present.
 */
size_t GetGRFSpriteOffset(uint32 id)
{
	return _grf_sprite_offsets.find(id) != _grf_sprite_offsets.end() ? _grf_sprite_offsets[id] : SIZE_MAX;
}

/**
 * Parse the sprite section of GRFs.
 * @param container_version Container version of the GRF we're currently processing.
 */
void ReadGRFSpriteOffsets(byte container_version)
{
	_grf_sprite_offsets.clear();

	if (container_version >= 2) {
		/* Seek to sprite section of the GRF. */
		size_t data_offset = FioReadDword();
		size_t old_pos = FioGetPos();
		FioSeekTo(data_offset, SEEK_CUR);

		/* Loop over all sprite section entries and store the file
		 * offset for each newly encountered ID. */
		uint32 id, prev_id = 0;
		while ((id = FioReadDword()) != 0) {
			if (id != prev_id) _grf_sprite_offsets[id] = FioGetPos() - 4;
			prev_id = id;
			FioSkipBytes(FioReadDword());
		}

		/* Continue processing the data section. */
		FioSeekTo(old_pos, SEEK_SET);
	}
}


/**
 * Load a real or recolour sprite.
 * @param load_index Global sprite index.
 * @param file_slot GRF to load from.
 * @param file_sprite_id Sprite number in the GRF.
 * @param container_version Container version of the GRF.
 * @return True if a valid sprite was loaded, false on any error.
 */
bool LoadNextSprite(int load_index, byte file_slot, uint file_sprite_id, byte container_version)
{
	size_t file_pos = FioGetPos();

	/* Read sprite header. */
	uint32 num = container_version >= 2 ? FioReadDword() : FioReadWord();
	if (num == 0) return false;
	byte grf_type = FioReadByte();

	SpriteType type;
	void *data = NULL;
	if (grf_type == 0xFF) {
		/* Some NewGRF files have "empty" pseudo-sprites which are 1
		 * byte long. Catch these so the sprites won't be displayed. */
		if (num == 1) {
			FioReadByte();
			return false;
		}
		type = ST_RECOLOUR;
		data = ReadRecolourSprite(file_slot, num);
	} else if (container_version >= 2 && grf_type == 0xFD) {
		if (num != 4) {
			/* Invalid sprite section include, ignore. */
			FioSkipBytes(num);
			return false;
		}
		/* It is not an error if no sprite with the provided ID is found in the sprite section. */
		file_pos = GetGRFSpriteOffset(FioReadDword());
		type = ST_NORMAL;
	} else {
		FioSkipBytes(7);
		type = SkipSpriteData(grf_type, num - 8) ? ST_NORMAL : ST_INVALID;
		/* Inline sprites are not supported for container version >= 2. */
		if (container_version >= 2) return false;
	}

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
	sc->ptr = data;
	sc->lru = 0;
	sc->id = file_sprite_id;
	sc->type = type;
	sc->warned = false;
	sc->container_ver = container_version;

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
	scnew->container_ver = scold->container_ver;
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

/**
 * Delete a single entry from the sprite cache.
 * @param item Entry to delete.
 */
static void DeleteEntryFromSpriteCache(uint item)
{
	/* Mark the block as free (the block must be in use) */
	MemBlock *s = (MemBlock*)GetSpriteCache(item)->ptr - 1;
	assert(!(s->size & S_FREE_MASK));
	s->size |= S_FREE_MASK;
	GetSpriteCache(item)->ptr = NULL;

	/* And coalesce adjacent free blocks */
	for (s = _spritecache_ptr; s->size != 0; s = NextBlock(s)) {
		if (s->size & S_FREE_MASK) {
			while (NextBlock(s)->size & S_FREE_MASK) {
				s->size += NextBlock(s)->size & ~S_FREE_MASK;
			}
		}
	}
}

static void DeleteEntryFromSpriteCache()
{
	uint best = UINT_MAX;
	int cur_lru;

	DEBUG(sprite, 3, "DeleteEntryFromSpriteCache, inuse=" PRINTF_SIZE, GetSpriteCacheUsage());

	cur_lru = 0xffff;
	for (SpriteID i = 0; i != _spritecache_items; i++) {
		SpriteCache *sc = GetSpriteCache(i);
		if (sc->type != ST_RECOLOUR && sc->ptr != NULL && sc->lru < cur_lru) {
			cur_lru = sc->lru;
			best = i;
		}
	}

	/* Display an error message and die, in case we found no sprite at all.
	 * This shouldn't really happen, unless all sprites are locked. */
	if (best == UINT_MAX) error("Out of sprite memory");

	DeleteEntryFromSpriteCache(best);
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
static void *HandleInvalidSpriteRequest(SpriteID sprite, SpriteType requested, SpriteCache *sc, AllocatorProc *allocator)
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
		return GetRawSprite(sprite, sc->type, allocator);
	}

	byte warning_level = sc->warned ? 6 : 0;
	sc->warned = true;
	DEBUG(sprite, warning_level, "Tried to load %s sprite #%d as a %s sprite. Probable cause: NewGRF interference", sprite_types[available], sprite, sprite_types[requested]);

	switch (requested) {
		case ST_NORMAL:
			if (sprite == SPR_IMG_QUERY) usererror("Uhm, would you be so kind not to load a NewGRF that makes the 'query' sprite a non-normal sprite?");
			/* FALL THROUGH */
		case ST_FONT:
			return GetRawSprite(SPR_IMG_QUERY, ST_NORMAL, allocator);
		case ST_RECOLOUR:
			if (sprite == PALETTE_TO_DARK_BLUE) usererror("Uhm, would you be so kind not to load a NewGRF that makes the 'PALETTE_TO_DARK_BLUE' sprite a non-remap sprite?");
			return GetRawSprite(PALETTE_TO_DARK_BLUE, ST_RECOLOUR, allocator);
		case ST_MAPGEN:
			/* this shouldn't happen, overriding of ST_MAPGEN sprites is checked in LoadNextSprite()
			 * (the only case the check fails is when these sprites weren't even loaded...) */
		default:
			NOT_REACHED();
	}
}

/**
 * Reads a sprite (from disk or sprite cache).
 * If the sprite is not available or of wrong type, a fallback sprite is returned.
 * @param sprite Sprite to read.
 * @param type Expected sprite type.
 * @param allocator Allocator function to use. Set to NULL to use the usual sprite cache.
 * @return Sprite raw data
 */
void *GetRawSprite(SpriteID sprite, SpriteType type, AllocatorProc *allocator)
{
	assert(type != ST_MAPGEN || IsMapgenSpriteID(sprite));
	assert(type < ST_INVALID);

	if (!SpriteExists(sprite)) {
		DEBUG(sprite, 1, "Tried to load non-existing sprite #%d. Probable cause: Wrong/missing NewGRFs", sprite);

		/* SPR_IMG_QUERY is a BIG FAT RED ? */
		sprite = SPR_IMG_QUERY;
	}

	SpriteCache *sc = GetSpriteCache(sprite);

	if (sc->type != type) return HandleInvalidSpriteRequest(sprite, type, sc, allocator);

	if (allocator == NULL) {
		/* Load sprite into/from spritecache */

		/* Update LRU */
		sc->lru = ++_sprite_lru_counter;

		/* Load the sprite, if it is not loaded, yet */
		if (sc->ptr == NULL) sc->ptr = ReadSprite(sc, sprite, type, AllocSprite);

		return sc->ptr;
	} else {
		/* Do not use the spritecache, but a different allocator. */
		return ReadSprite(sc, sprite, type, allocator);
	}
}


static void GfxInitSpriteCache()
{
	/* initialize sprite cache heap */
	int bpp = BlitterFactory::GetCurrentBlitter()->GetScreenDepth();
	uint target_size = (bpp > 0 ? _sprite_cache_size * bpp / 8 : 1) * 1024 * 1024;

	/* Remember 'target_size' from the previous allocation attempt, so we do not try to reach the target_size multiple times in case of failure. */
	static uint last_alloc_attempt = 0;

	if (_spritecache_ptr == NULL || (_allocated_sprite_cache_size != target_size && target_size != last_alloc_attempt)) {
		delete[] reinterpret_cast<byte *>(_spritecache_ptr);

		last_alloc_attempt = target_size;
		_allocated_sprite_cache_size = target_size;

		do {
			try {
				/* Try to allocate 50% more to make sure we do not allocate almost all available. */
				_spritecache_ptr = reinterpret_cast<MemBlock *>(new byte[_allocated_sprite_cache_size + _allocated_sprite_cache_size / 2]);
			} catch (std::bad_alloc &) {
				_spritecache_ptr = NULL;
			}

			if (_spritecache_ptr != NULL) {
				/* Allocation succeeded, but we wanted less. */
				delete[] reinterpret_cast<byte *>(_spritecache_ptr);
				_spritecache_ptr = reinterpret_cast<MemBlock *>(new byte[_allocated_sprite_cache_size]);
			} else if (_allocated_sprite_cache_size < 2 * 1024 * 1024) {
				usererror("Cannot allocate spritecache");
			} else {
				/* Try again to allocate half. */
				_allocated_sprite_cache_size >>= 1;
			}
		} while (_spritecache_ptr == NULL);

		if (_allocated_sprite_cache_size != target_size) {
			DEBUG(misc, 0, "Not enough memory to allocate %d MiB of spritecache. Spritecache was reduced to %d MiB.", target_size / 1024 / 1024, _allocated_sprite_cache_size / 1024 / 1024);

			ErrorMessageData msg(STR_CONFIG_ERROR_OUT_OF_MEMORY, STR_CONFIG_ERROR_SPRITECACHE_TOO_BIG);
			msg.SetDParam(0, target_size);
			msg.SetDParam(1, _allocated_sprite_cache_size);
			ScheduleErrorMessage(msg);
		}
	}

	/* A big free block */
	_spritecache_ptr->size = (_allocated_sprite_cache_size - sizeof(MemBlock)) | S_FREE_MASK;
	/* Sentinel block (identified by size == 0) */
	NextBlock(_spritecache_ptr)->size = 0;
}

void GfxInitSpriteMem()
{
	GfxInitSpriteCache();

	/* Reset the spritecache 'pool' */
	free(_spritecache);
	_spritecache_items = 0;
	_spritecache = NULL;

	_compact_cache_counter = 0;
}

/**
 * Remove all encoded sprites from the sprite cache without
 * discarding sprite location information.
 */
void GfxClearSpriteCache()
{
	/* Clear sprite ptr for all cached items */
	for (uint i = 0; i != _spritecache_items; i++) {
		SpriteCache *sc = GetSpriteCache(i);
		if (sc->type != ST_RECOLOUR && sc->ptr != NULL) DeleteEntryFromSpriteCache(i);
	}
}

/* static */ ReusableBuffer<SpriteLoader::CommonPixel> SpriteLoader::Sprite::buffer[ZOOM_LVL_COUNT];
