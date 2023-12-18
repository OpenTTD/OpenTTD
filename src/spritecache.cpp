/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file spritecache.cpp Caching of sprites. */

#include "stdafx.h"
#include "random_access_file_type.h"
#include "spriteloader/grf.hpp"
#include "gfx_func.h"
#include "error.h"
#include "error_func.h"
#include "zoom_func.h"
#include "settings_type.h"
#include "blitter/factory.hpp"
#include "core/math_func.hpp"
#include "core/mem_func.hpp"
#include "video/video_driver.hpp"
#include "spritecache.h"
#include "spritecache_internal.h"

#include "table/sprites.h"
#include "table/strings.h"
#include "table/palette_convert.h"

#include "safeguards.h"

/* Default of 4MB spritecache */
uint _sprite_cache_size = 4;


static uint _spritecache_items = 0;
static SpriteCache *_spritecache = nullptr;
static std::vector<std::unique_ptr<SpriteFile>> _sprite_files;

static inline SpriteCache *GetSpriteCache(uint index)
{
	return &_spritecache[index];
}

SpriteCache *AllocateSpriteCache(uint index)
{
	if (index >= _spritecache_items) {
		/* Add another 1024 items to the 'pool' */
		uint items = Align(index + 1, 1024);

		Debug(sprite, 4, "Increasing sprite cache to {} items ({} bytes)", items, items * sizeof(*_spritecache));

		_spritecache = ReallocT(_spritecache, items);

		/* Reset the new items and update the count */
		memset(_spritecache + _spritecache_items, 0, (items - _spritecache_items) * sizeof(*_spritecache));
		_spritecache_items = items;
	}

	return GetSpriteCache(index);
}

/**
 * Get the cached SpriteFile given the name of the file.
 * @param filename The name of the file at the disk.
 * @return The SpriteFile or \c null.
 */
static SpriteFile *GetCachedSpriteFileByName(const std::string &filename)
{
	for (auto &f : _sprite_files) {
		if (f->GetFilename() == filename) {
			return f.get();
		}
	}
	return nullptr;
}

/**
 * Open/get the SpriteFile that is cached for use in the sprite cache.
 * @param filename      Name of the file at the disk.
 * @param subdir        The sub directory to search this file in.
 * @param palette_remap Whether a palette remap needs to be performed for this file.
 * @return The reference to the SpriteCache.
 */
SpriteFile &OpenCachedSpriteFile(const std::string &filename, Subdirectory subdir, bool palette_remap)
{
	SpriteFile *file = GetCachedSpriteFileByName(filename);
	if (file == nullptr) {
		file = _sprite_files.insert(std::end(_sprite_files), std::make_unique<SpriteFile>(filename, subdir, palette_remap))->get();
	} else {
		file->SeekToBegin();
	}
	return *file;
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

/**
 * Skip the given amount of sprite graphics data.
 * @param type the type of sprite (compressed etc)
 * @param num the amount of sprites to skip
 * @return true if the data could be correctly skipped.
 */
bool SkipSpriteData(SpriteFile &file, byte type, uint16_t num)
{
	if (type & 2) {
		file.SkipBytes(num);
	} else {
		while (num > 0) {
			int8_t i = file.ReadByte();
			if (i >= 0) {
				int size = (i == 0) ? 0x80 : i;
				if (size > num) return false;
				num -= size;
				file.SkipBytes(size);
			} else {
				i = -(i >> 3);
				num -= i;
				file.ReadByte();
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
	return !(GetSpriteCache(id)->file_pos == 0 && GetSpriteCache(id)->file == nullptr);
}

/**
 * Get the sprite type of a given sprite.
 * @param sprite The sprite to look at.
 * @return the type of sprite.
 */
SpriteType GetSpriteType(SpriteID sprite)
{
	if (!SpriteExists(sprite)) return SpriteType::Invalid;
	return GetSpriteCache(sprite)->type;
}

/**
 * Get the SpriteFile of a given sprite.
 * @param sprite The sprite to look at.
 * @return The SpriteFile.
 */
SpriteFile *GetOriginFile(SpriteID sprite)
{
	if (!SpriteExists(sprite)) return nullptr;
	return GetSpriteCache(sprite)->file;
}

/**
 * Get the GRF-local sprite id of a given sprite.
 * @param sprite The sprite to look at.
 * @return The GRF-local sprite id.
 */
uint32_t GetSpriteLocalID(SpriteID sprite)
{
	if (!SpriteExists(sprite)) return 0;
	return GetSpriteCache(sprite)->id;
}

/**
 * Count the sprites which originate from a specific file in a range of SpriteIDs.
 * @param file The loaded SpriteFile.
 * @param begin First sprite in range.
 * @param end First sprite not in range.
 * @return Number of sprites.
 */
uint GetSpriteCountForFile(const std::string &filename, SpriteID begin, SpriteID end)
{
	SpriteFile *file = GetCachedSpriteFileByName(filename);
	if (file == nullptr) return 0;

	uint count = 0;
	for (SpriteID i = begin; i != end; i++) {
		if (SpriteExists(i)) {
			SpriteCache *sc = GetSpriteCache(i);
			if (sc->file == file) {
				count++;
				Debug(sprite, 4, "Sprite: {}", i);
			}
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

static bool ResizeSpriteIn(SpriteLoader::SpriteCollection &sprite, ZoomLevel src, ZoomLevel tgt)
{
	uint8_t scaled_1 = ScaleByZoom(1, (ZoomLevel)(src - tgt));

	/* Check for possible memory overflow. */
	if (sprite[src].width * scaled_1 > UINT16_MAX || sprite[src].height * scaled_1 > UINT16_MAX) return false;

	sprite[tgt].width  = sprite[src].width  * scaled_1;
	sprite[tgt].height = sprite[src].height * scaled_1;
	sprite[tgt].x_offs = sprite[src].x_offs * scaled_1;
	sprite[tgt].y_offs = sprite[src].y_offs * scaled_1;
	sprite[tgt].colours = sprite[src].colours;

	sprite[tgt].AllocateData(tgt, static_cast<size_t>(sprite[tgt].width) * sprite[tgt].height);

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

static void ResizeSpriteOut(SpriteLoader::SpriteCollection &sprite, ZoomLevel zoom)
{
	/* Algorithm based on 32bpp_Optimized::ResizeSprite() */
	sprite[zoom].width  = UnScaleByZoom(sprite[ZOOM_LVL_NORMAL].width,  zoom);
	sprite[zoom].height = UnScaleByZoom(sprite[ZOOM_LVL_NORMAL].height, zoom);
	sprite[zoom].x_offs = UnScaleByZoom(sprite[ZOOM_LVL_NORMAL].x_offs, zoom);
	sprite[zoom].y_offs = UnScaleByZoom(sprite[ZOOM_LVL_NORMAL].y_offs, zoom);
	sprite[zoom].colours = sprite[ZOOM_LVL_NORMAL].colours;

	sprite[zoom].AllocateData(zoom, static_cast<size_t>(sprite[zoom].height) * sprite[zoom].width);

	SpriteLoader::CommonPixel *dst = sprite[zoom].data;
	const SpriteLoader::CommonPixel *src = sprite[zoom - 1].data;
	[[maybe_unused]] const SpriteLoader::CommonPixel *src_end = src + sprite[zoom - 1].height * sprite[zoom - 1].width;

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
	size_t sprite_size = static_cast<size_t>(sprite->width) * sprite->height;
	SpriteLoader::CommonPixel *src_data = MallocT<SpriteLoader::CommonPixel>(sprite_size);
	MemCpyT(src_data, sprite->data, sprite_size);
	sprite->AllocateData(zoom, static_cast<size_t>(width) * height);

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

static bool PadSprites(SpriteLoader::SpriteCollection &sprite, uint8_t sprite_avail, SpriteEncoder *encoder)
{
	/* Get minimum top left corner coordinates. */
	int min_xoffs = INT32_MAX;
	int min_yoffs = INT32_MAX;
	for (ZoomLevel zoom = ZOOM_LVL_BEGIN; zoom != ZOOM_LVL_END; zoom++) {
		if (HasBit(sprite_avail, zoom)) {
			min_xoffs = std::min(min_xoffs, ScaleByZoom(sprite[zoom].x_offs, zoom));
			min_yoffs = std::min(min_yoffs, ScaleByZoom(sprite[zoom].y_offs, zoom));
		}
	}

	/* Get maximum dimensions taking necessary padding at the top left into account. */
	int max_width  = INT32_MIN;
	int max_height = INT32_MIN;
	for (ZoomLevel zoom = ZOOM_LVL_BEGIN; zoom != ZOOM_LVL_END; zoom++) {
		if (HasBit(sprite_avail, zoom)) {
			max_width  = std::max(max_width, ScaleByZoom(sprite[zoom].width + sprite[zoom].x_offs - UnScaleByZoom(min_xoffs, zoom), zoom));
			max_height = std::max(max_height, ScaleByZoom(sprite[zoom].height + sprite[zoom].y_offs - UnScaleByZoom(min_yoffs, zoom), zoom));
		}
	}

	/* Align height and width if required to match the needs of the sprite encoder. */
	uint align = encoder->GetSpriteAlignment();
	if (align != 0) {
		max_width  = Align(max_width,  align);
		max_height = Align(max_height, align);
	}

	/* Pad sprites where needed. */
	for (ZoomLevel zoom = ZOOM_LVL_BEGIN; zoom != ZOOM_LVL_END; zoom++) {
		if (HasBit(sprite_avail, zoom)) {
			/* Scaling the sprite dimensions in the blitter is done with rounding up,
			 * so a negative padding here is not an error. */
			int pad_left   = std::max(0, sprite[zoom].x_offs - UnScaleByZoom(min_xoffs, zoom));
			int pad_top    = std::max(0, sprite[zoom].y_offs - UnScaleByZoom(min_yoffs, zoom));
			int pad_right  = std::max(0, UnScaleByZoom(max_width, zoom) - sprite[zoom].width - pad_left);
			int pad_bottom = std::max(0, UnScaleByZoom(max_height, zoom) - sprite[zoom].height - pad_top);

			if (pad_left > 0 || pad_right > 0 || pad_top > 0 || pad_bottom > 0) {
				if (!PadSingleSprite(&sprite[zoom], zoom, pad_left, pad_top, pad_right, pad_bottom)) return false;
			}
		}
	}

	return true;
}

static bool ResizeSprites(SpriteLoader::SpriteCollection &sprite, uint8_t sprite_avail, SpriteEncoder *encoder)
{
	/* Create a fully zoomed image if it does not exist */
	ZoomLevel first_avail = static_cast<ZoomLevel>(FIND_FIRST_BIT(sprite_avail));
	if (first_avail != ZOOM_LVL_NORMAL) {
		if (!ResizeSpriteIn(sprite, first_avail, ZOOM_LVL_NORMAL)) return false;
		SetBit(sprite_avail, ZOOM_LVL_NORMAL);
	}

	/* Pad sprites to make sizes match. */
	if (!PadSprites(sprite, sprite_avail, encoder)) return false;

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

	/* Upscale to desired sprite_min_zoom if provided sprite only had zoomed in versions. */
	if (first_avail < _settings_client.gui.sprite_zoom_min) {
		if (_settings_client.gui.sprite_zoom_min >= ZOOM_LVL_OUT_4X) ResizeSpriteIn(sprite, ZOOM_LVL_OUT_4X, ZOOM_LVL_OUT_2X);
		if (_settings_client.gui.sprite_zoom_min >= ZOOM_LVL_OUT_2X) ResizeSpriteIn(sprite, ZOOM_LVL_OUT_2X, ZOOM_LVL_NORMAL);
	}

	return  true;
}

/**
 * Load a recolour sprite into memory.
 * @param file GRF we're reading from.
 * @param num Size of the sprite in the GRF.
 * @return Sprite data.
 */
static void *ReadRecolourSprite(SpriteFile &file, uint num)
{
	/* "Normal" recolour sprites are ALWAYS 257 bytes. Then there is a small
	 * number of recolour sprites that are 17 bytes that only exist in DOS
	 * GRFs which are the same as 257 byte recolour sprites, but with the last
	 * 240 bytes zeroed.  */
	static const uint RECOLOUR_SPRITE_SIZE = 257;
	byte *dest = (byte *)AllocSprite(std::max(RECOLOUR_SPRITE_SIZE, num));

	if (file.NeedsPaletteRemap()) {
		byte *dest_tmp = new byte[std::max(RECOLOUR_SPRITE_SIZE, num)];

		/* Only a few recolour sprites are less than 257 bytes */
		if (num < RECOLOUR_SPRITE_SIZE) memset(dest_tmp, 0, RECOLOUR_SPRITE_SIZE);
		file.ReadBlock(dest_tmp, num);

		/* The data of index 0 is never used; "literal 00" according to the (New)GRF specs. */
		for (uint i = 1; i < RECOLOUR_SPRITE_SIZE; i++) {
			dest[i] = _palmap_w2d[dest_tmp[_palmap_d2w[i - 1] + 1]];
		}
		delete[] dest_tmp;
	} else {
		file.ReadBlock(dest, num);
	}

	return dest;
}

/**
 * Read a sprite from disk.
 * @param sc          Location of sprite.
 * @param id          Sprite number.
 * @param sprite_type Type of sprite.
 * @param allocator   Allocator function to use.
 * @param encoder     Sprite encoder to use.
 * @return Read sprite data.
 */
static void *ReadSprite(const SpriteCache *sc, SpriteID id, SpriteType sprite_type, AllocatorProc *allocator, SpriteEncoder *encoder)
{
	/* Use current blitter if no other sprite encoder is given. */
	if (encoder == nullptr) encoder = BlitterFactory::GetCurrentBlitter();

	SpriteFile &file = *sc->file;
	size_t file_pos = sc->file_pos;

	assert(sprite_type != SpriteType::Recolour);
	assert(IsMapgenSpriteID(id) == (sprite_type == SpriteType::MapGen));
	assert(sc->type == sprite_type);

	Debug(sprite, 9, "Load sprite {}", id);

	SpriteLoader::SpriteCollection sprite;
	uint8_t sprite_avail = 0;
	sprite[ZOOM_LVL_NORMAL].type = sprite_type;

	SpriteLoaderGrf sprite_loader(file.GetContainerVersion());
	if (sprite_type != SpriteType::MapGen && encoder->Is32BppSupported()) {
		/* Try for 32bpp sprites first. */
		sprite_avail = sprite_loader.LoadSprite(sprite, file, file_pos, sprite_type, true, sc->control_flags);
	}
	if (sprite_avail == 0) {
		sprite_avail = sprite_loader.LoadSprite(sprite, file, file_pos, sprite_type, false, sc->control_flags);
	}

	if (sprite_avail == 0) {
		if (sprite_type == SpriteType::MapGen) return nullptr;
		if (id == SPR_IMG_QUERY) UserError("Okay... something went horribly wrong. I couldn't load the fallback sprite. What should I do?");
		return (void*)GetRawSprite(SPR_IMG_QUERY, SpriteType::Normal, allocator, encoder);
	}

	if (sprite_type == SpriteType::MapGen) {
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

	if (!ResizeSprites(sprite, sprite_avail, encoder)) {
		if (id == SPR_IMG_QUERY) UserError("Okay... something went horribly wrong. I couldn't resize the fallback sprite. What should I do?");
		return (void*)GetRawSprite(SPR_IMG_QUERY, SpriteType::Normal, allocator, encoder);
	}

	if (sprite[ZOOM_LVL_NORMAL].type == SpriteType::Font && _font_zoom != ZOOM_LVL_NORMAL) {
		/* Make ZOOM_LVL_NORMAL be ZOOM_LVL_GUI */
		sprite[ZOOM_LVL_NORMAL].width  = sprite[_font_zoom].width;
		sprite[ZOOM_LVL_NORMAL].height = sprite[_font_zoom].height;
		sprite[ZOOM_LVL_NORMAL].x_offs = sprite[_font_zoom].x_offs;
		sprite[ZOOM_LVL_NORMAL].y_offs = sprite[_font_zoom].y_offs;
		sprite[ZOOM_LVL_NORMAL].data   = sprite[_font_zoom].data;
		sprite[ZOOM_LVL_NORMAL].colours = sprite[_font_zoom].colours;
	}

	return encoder->Encode(sprite, allocator);
}

struct GrfSpriteOffset {
	size_t file_pos;
	byte control_flags;
};

/** Map from sprite numbers to position in the GRF file. */
static std::map<uint32_t, GrfSpriteOffset> _grf_sprite_offsets;

/**
 * Get the file offset for a specific sprite in the sprite section of a GRF.
 * @param id ID of the sprite to look up.
 * @return Position of the sprite in the sprite section or SIZE_MAX if no such sprite is present.
 */
size_t GetGRFSpriteOffset(uint32_t id)
{
	return _grf_sprite_offsets.find(id) != _grf_sprite_offsets.end() ? _grf_sprite_offsets[id].file_pos : SIZE_MAX;
}

/**
 * Parse the sprite section of GRFs.
 * @param container_version Container version of the GRF we're currently processing.
 */
void ReadGRFSpriteOffsets(SpriteFile &file)
{
	_grf_sprite_offsets.clear();

	if (file.GetContainerVersion() >= 2) {
		/* Seek to sprite section of the GRF. */
		size_t data_offset = file.ReadDword();
		size_t old_pos = file.GetPos();
		file.SeekTo(data_offset, SEEK_CUR);

		GrfSpriteOffset offset = { 0, 0 };

		/* Loop over all sprite section entries and store the file
		 * offset for each newly encountered ID. */
		uint32_t id, prev_id = 0;
		while ((id = file.ReadDword()) != 0) {
			if (id != prev_id) {
				_grf_sprite_offsets[prev_id] = offset;
				offset.file_pos = file.GetPos() - 4;
				offset.control_flags = 0;
			}
			prev_id = id;
			uint length = file.ReadDword();
			if (length > 0) {
				byte colour = file.ReadByte() & SCC_MASK;
				length--;
				if (length > 0) {
					byte zoom = file.ReadByte();
					length--;
					if (colour != 0 && zoom == 0) { // ZOOM_LVL_OUT_4X (normal zoom)
						SetBit(offset.control_flags, (colour != SCC_PAL) ? SCCF_ALLOW_ZOOM_MIN_1X_32BPP : SCCF_ALLOW_ZOOM_MIN_1X_PAL);
						SetBit(offset.control_flags, (colour != SCC_PAL) ? SCCF_ALLOW_ZOOM_MIN_2X_32BPP : SCCF_ALLOW_ZOOM_MIN_2X_PAL);
					}
					if (colour != 0 && zoom == 2) { // ZOOM_LVL_OUT_2X (2x zoomed in)
						SetBit(offset.control_flags, (colour != SCC_PAL) ? SCCF_ALLOW_ZOOM_MIN_2X_32BPP : SCCF_ALLOW_ZOOM_MIN_2X_PAL);
					}
				}
			}
			file.SkipBytes(length);
		}
		if (prev_id != 0) _grf_sprite_offsets[prev_id] = offset;

		/* Continue processing the data section. */
		file.SeekTo(old_pos, SEEK_SET);
	}
}


/**
 * Load a real or recolour sprite.
 * @param load_index Global sprite index.
 * @param file GRF to load from.
 * @param file_sprite_id Sprite number in the GRF.
 * @param container_version Container version of the GRF.
 * @return True if a valid sprite was loaded, false on any error.
 */
bool LoadNextSprite(int load_index, SpriteFile &file, uint file_sprite_id)
{
	size_t file_pos = file.GetPos();

	/* Read sprite header. */
	uint32_t num = file.GetContainerVersion() >= 2 ? file.ReadDword() : file.ReadWord();
	if (num == 0) return false;
	byte grf_type = file.ReadByte();

	SpriteType type;
	void *data = nullptr;
	byte control_flags = 0;
	if (grf_type == 0xFF) {
		/* Some NewGRF files have "empty" pseudo-sprites which are 1
		 * byte long. Catch these so the sprites won't be displayed. */
		if (num == 1) {
			file.ReadByte();
			return false;
		}
		type = SpriteType::Recolour;
		data = ReadRecolourSprite(file, num);
	} else if (file.GetContainerVersion() >= 2 && grf_type == 0xFD) {
		if (num != 4) {
			/* Invalid sprite section include, ignore. */
			file.SkipBytes(num);
			return false;
		}
		/* It is not an error if no sprite with the provided ID is found in the sprite section. */
		auto iter = _grf_sprite_offsets.find(file.ReadDword());
		if (iter != _grf_sprite_offsets.end()) {
			file_pos = iter->second.file_pos;
			control_flags = iter->second.control_flags;
		} else {
			file_pos = SIZE_MAX;
		}
		type = SpriteType::Normal;
	} else {
		file.SkipBytes(7);
		type = SkipSpriteData(file, grf_type, num - 8) ? SpriteType::Normal : SpriteType::Invalid;
		/* Inline sprites are not supported for container version >= 2. */
		if (file.GetContainerVersion() >= 2) return false;
	}

	if (type == SpriteType::Invalid) return false;

	if (load_index >= MAX_SPRITES) {
		UserError("Tried to load too many sprites (#{}; max {})", load_index, MAX_SPRITES);
	}

	bool is_mapgen = IsMapgenSpriteID(load_index);

	if (is_mapgen) {
		if (type != SpriteType::Normal) UserError("Uhm, would you be so kind not to load a NewGRF that changes the type of the map generator sprites?");
		type = SpriteType::MapGen;
	}

	SpriteCache *sc = AllocateSpriteCache(load_index);
	sc->file = &file;
	sc->file_pos = file_pos;
	sc->ptr = data;
	sc->lru = 0;
	sc->id = file_sprite_id;
	sc->type = type;
	sc->warned = false;
	sc->control_flags = control_flags;

	return true;
}


void DupSprite(SpriteID old_spr, SpriteID new_spr)
{
	SpriteCache *scnew = AllocateSpriteCache(new_spr); // may reallocate: so put it first
	SpriteCache *scold = GetSpriteCache(old_spr);

	scnew->file = scold->file;
	scnew->file_pos = scold->file_pos;
	scnew->ptr = nullptr;
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
static_assert(sizeof(MemBlock) == sizeof(size_t));
/* make sure it's a power of two */
static_assert((sizeof(size_t) & (sizeof(size_t) - 1)) == 0);

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

		Debug(sprite, 5, "Fixing lru {}, inuse={}", _sprite_lru_counter, GetSpriteCacheUsage());

		for (i = 0; i != _spritecache_items; i++) {
			SpriteCache *sc = GetSpriteCache(i);
			if (sc->ptr != nullptr) {
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

	Debug(sprite, 3, "Compacting sprite cache, inuse={}", GetSpriteCacheUsage());

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
	GetSpriteCache(item)->ptr = nullptr;

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

	Debug(sprite, 3, "DeleteEntryFromSpriteCache, inuse={}", GetSpriteCacheUsage());

	cur_lru = 0xffff;
	for (SpriteID i = 0; i != _spritecache_items; i++) {
		SpriteCache *sc = GetSpriteCache(i);
		if (sc->type != SpriteType::Recolour && sc->ptr != nullptr && sc->lru < cur_lru) {
			cur_lru = sc->lru;
			best = i;
		}
	}

	/* Display an error message and die, in case we found no sprite at all.
	 * This shouldn't really happen, unless all sprites are locked. */
	if (best == UINT_MAX) FatalError("Out of sprite memory");

	DeleteEntryFromSpriteCache(best);
}

void *AllocSprite(size_t mem_req)
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
 * Sprite allocator simply using malloc.
 */
void *SimpleSpriteAlloc(size_t size)
{
	return MallocT<byte>(size);
}

/**
 * Handles the case when a sprite of different type is requested than is present in the SpriteCache.
 * For SpriteType::Font sprites, it is normal. In other cases, default sprite is loaded instead.
 * @param sprite ID of loaded sprite
 * @param requested requested sprite type
 * @param sc the currently known sprite cache for the requested sprite
 * @return fallback sprite
 * @note this function will do UserError() in the case the fallback sprite isn't available
 */
static void *HandleInvalidSpriteRequest(SpriteID sprite, SpriteType requested, SpriteCache *sc, AllocatorProc *allocator)
{
	static const char * const sprite_types[] = {
		"normal",        // SpriteType::Normal
		"map generator", // SpriteType::MapGen
		"character",     // SpriteType::Font
		"recolour",      // SpriteType::Recolour
	};

	SpriteType available = sc->type;
	if (requested == SpriteType::Font && available == SpriteType::Normal) {
		if (sc->ptr == nullptr) sc->type = SpriteType::Font;
		return GetRawSprite(sprite, sc->type, allocator);
	}

	byte warning_level = sc->warned ? 6 : 0;
	sc->warned = true;
	Debug(sprite, warning_level, "Tried to load {} sprite #{} as a {} sprite. Probable cause: NewGRF interference", sprite_types[static_cast<byte>(available)], sprite, sprite_types[static_cast<byte>(requested)]);

	switch (requested) {
		case SpriteType::Normal:
			if (sprite == SPR_IMG_QUERY) UserError("Uhm, would you be so kind not to load a NewGRF that makes the 'query' sprite a non-normal sprite?");
			FALLTHROUGH;
		case SpriteType::Font:
			return GetRawSprite(SPR_IMG_QUERY, SpriteType::Normal, allocator);
		case SpriteType::Recolour:
			if (sprite == PALETTE_TO_DARK_BLUE) UserError("Uhm, would you be so kind not to load a NewGRF that makes the 'PALETTE_TO_DARK_BLUE' sprite a non-remap sprite?");
			return GetRawSprite(PALETTE_TO_DARK_BLUE, SpriteType::Recolour, allocator);
		case SpriteType::MapGen:
			/* this shouldn't happen, overriding of SpriteType::MapGen sprites is checked in LoadNextSprite()
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
 * @param allocator Allocator function to use. Set to nullptr to use the usual sprite cache.
 * @param encoder Sprite encoder to use. Set to nullptr to use the currently active blitter.
 * @return Sprite raw data
 */
void *GetRawSprite(SpriteID sprite, SpriteType type, AllocatorProc *allocator, SpriteEncoder *encoder)
{
	assert(type != SpriteType::MapGen || IsMapgenSpriteID(sprite));
	assert(type < SpriteType::Invalid);

	if (!SpriteExists(sprite)) {
		Debug(sprite, 1, "Tried to load non-existing sprite #{}. Probable cause: Wrong/missing NewGRFs", sprite);

		/* SPR_IMG_QUERY is a BIG FAT RED ? */
		sprite = SPR_IMG_QUERY;
	}

	SpriteCache *sc = GetSpriteCache(sprite);

	if (sc->type != type) return HandleInvalidSpriteRequest(sprite, type, sc, allocator);

	if (allocator == nullptr && encoder == nullptr) {
		/* Load sprite into/from spritecache */

		/* Update LRU */
		sc->lru = ++_sprite_lru_counter;

		/* Load the sprite, if it is not loaded, yet */
		if (sc->ptr == nullptr) sc->ptr = ReadSprite(sc, sprite, type, AllocSprite, nullptr);

		return sc->ptr;
	} else {
		/* Do not use the spritecache, but a different allocator. */
		return ReadSprite(sc, sprite, type, allocator, encoder);
	}
}


static void GfxInitSpriteCache()
{
	/* initialize sprite cache heap */
	int bpp = BlitterFactory::GetCurrentBlitter()->GetScreenDepth();
	uint target_size = (bpp > 0 ? _sprite_cache_size * bpp / 8 : 1) * 1024 * 1024;

	/* Remember 'target_size' from the previous allocation attempt, so we do not try to reach the target_size multiple times in case of failure. */
	static uint last_alloc_attempt = 0;

	if (_spritecache_ptr == nullptr || (_allocated_sprite_cache_size != target_size && target_size != last_alloc_attempt)) {
		delete[] reinterpret_cast<byte *>(_spritecache_ptr);

		last_alloc_attempt = target_size;
		_allocated_sprite_cache_size = target_size;

		do {
			/* Try to allocate 50% more to make sure we do not allocate almost all available. */
			_spritecache_ptr = reinterpret_cast<MemBlock *>(new(std::nothrow) byte[_allocated_sprite_cache_size + _allocated_sprite_cache_size / 2]);

			if (_spritecache_ptr != nullptr) {
				/* Allocation succeeded, but we wanted less. */
				delete[] reinterpret_cast<byte *>(_spritecache_ptr);
				_spritecache_ptr = reinterpret_cast<MemBlock *>(new byte[_allocated_sprite_cache_size]);
			} else if (_allocated_sprite_cache_size < 2 * 1024 * 1024) {
				UserError("Cannot allocate spritecache");
			} else {
				/* Try again to allocate half. */
				_allocated_sprite_cache_size >>= 1;
			}
		} while (_spritecache_ptr == nullptr);

		if (_allocated_sprite_cache_size != target_size) {
			Debug(misc, 0, "Not enough memory to allocate {} MiB of spritecache. Spritecache was reduced to {} MiB.", target_size / 1024 / 1024, _allocated_sprite_cache_size / 1024 / 1024);

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
	_spritecache = nullptr;

	_compact_cache_counter = 0;
	_sprite_files.clear();
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
		if (sc->type != SpriteType::Recolour && sc->ptr != nullptr) DeleteEntryFromSpriteCache(i);
	}

	VideoDriver::GetInstance()->ClearSystemSprites();
}

/**
 * Remove all encoded font sprites from the sprite cache without
 * discarding sprite location information.
 */
void GfxClearFontSpriteCache()
{
	/* Clear sprite ptr for all cached font items */
	for (uint i = 0; i != _spritecache_items; i++) {
		SpriteCache *sc = GetSpriteCache(i);
		if (sc->type == SpriteType::Font && sc->ptr != nullptr) DeleteEntryFromSpriteCache(i);
	}
}

/* static */ ReusableBuffer<SpriteLoader::CommonPixel> SpriteLoader::Sprite::buffer[ZOOM_LVL_END];
